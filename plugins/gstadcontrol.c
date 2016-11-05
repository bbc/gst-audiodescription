/* GStreamer
 * Copyright (C) 2016 David Holroyd <dave@badgers-in-foil.co.uk>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Suite 500,
 * Boston, MA 02110-1335, USA.
 */
/**
 * SECTION:element-gstadcontrol
 *
 * An audio transform element which controls the output volume by also
 * consuming "AD_descriptor" metadata as defined in
 * "ETSI Technical Report 101 154".
 *
 * <refsect2>
 * <title>Example launch line</title>
 * |[
 * gst-launch-1.0 filesrc location=test.wav ! wavparse ! deinterleave name=d d.src_1 ! queue max-size-time=100000000 ! audioconvert ! audio/x-raw,format=S16LE,rate=48000,channels=1 ! whp198dec ! ad.  d.src_0 ! queue max-size-time=100000000 ! audioconvert ! audio/x-raw,format=S16LE,rate=48000,channels=1 ! mix. audiotestsrc wave=red-noise volume=0.3 ! audio/x-raw,format=S16LE,rate=48000,channels=1 ! adcontrol name=ad ! mix. audiomixer name=mix ! autoaudiosink
 * ]|
 * Simulate 'main' programme audio using an audiotestsrc, and mix that test
 * audio with an audio description track from the given .wav file, while
 * using WHP198-encoded metadata from the .wav file to reduce the volume
 * of the test audio as required.
 * </refsect2>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/gst.h>
#include <gst/audio/audio.h>
#include <gst/audio/streamvolume.h>
#include "gstadcontrol.h"

GST_DEBUG_CATEGORY_STATIC (gst_adcontrol_debug_category);
#define GST_CAT_DEFAULT gst_adcontrol_debug_category

/* prototypes */


static void gst_adcontrol_set_property (GObject * object,
    guint property_id, const GValue * value, GParamSpec * pspec);
static void gst_adcontrol_get_property (GObject * object,
    guint property_id, GValue * value, GParamSpec * pspec);
static void gst_adcontrol_dispose (GObject * object);
static void gst_adcontrol_finalize (GObject * object);
static GstFlowReturn
gst_adcontrol_chain (GstPad * pad, GstObject * parent, GstBuffer *buf);

enum
{
  PROP_0
};

/* pad templates */

#define FORMAT "{ "GST_AUDIO_NE(F32)","GST_AUDIO_NE(S16)" }"

static GstStaticPadTemplate sink_template = GST_STATIC_PAD_TEMPLATE ("main_sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("audio/x-raw, "
        "format = (string) " FORMAT ", "
        "layout = (string) { interleaved, non-interleaved }, "
        "rate = (int) [ 1, MAX ], " "channels = (int) [ 1, MAX ]"));

static GstStaticPadTemplate src_template = GST_STATIC_PAD_TEMPLATE ("main_src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("audio/x-raw, "
        "format = (string) " FORMAT ", "
        "layout = (string) { interleaved, non-interleaved }, "
        "rate = (int) [ 1, MAX ], " "channels = (int) [ 1, MAX ]"));

static GstStaticPadTemplate gst_adcontrol_sink_template =
GST_STATIC_PAD_TEMPLATE ("ad_sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("application/x-tr_101_154_ad_descriptor")
    );


/* class initialization */

G_DEFINE_TYPE_WITH_CODE (GstAdcontrol, gst_adcontrol, GST_TYPE_BIN,
  GST_DEBUG_CATEGORY_INIT (gst_adcontrol_debug_category, "adcontrol", 0,
  "debug category for adcontrol element"));

static void
gst_adcontrol_class_init (GstAdcontrolClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstBinClass *bin_class = GST_BIN_CLASS (klass);

  gst_element_class_add_pad_template (GST_ELEMENT_CLASS(klass),
      gst_static_pad_template_get (&src_template));
  gst_element_class_add_pad_template (GST_ELEMENT_CLASS(klass),
      gst_static_pad_template_get (&sink_template));

  gst_element_class_add_static_pad_template (GST_ELEMENT_CLASS(klass),
      &gst_adcontrol_sink_template);

  gst_element_class_set_static_metadata (GST_ELEMENT_CLASS(klass),
      "Audio Description Controler", "Generic", "Accepts Audio Description descriptors defining the 'pan' of the description track, and the 'fade' of the main track, and adjusts the audio levels of the given controls to suit",
      "David Holroyd <dave@badgers-in-foil.co.uk>");

  gobject_class->set_property = gst_adcontrol_set_property;
  gobject_class->get_property = gst_adcontrol_get_property;
  gobject_class->dispose = gst_adcontrol_dispose;
  gobject_class->finalize = gst_adcontrol_finalize;

}

static void
gst_adcontrol_init (GstAdcontrol *self)
{
  self->volume_element = gst_element_factory_make ("volume", "ad-main-volume");
  if (!self->volume_element) {
    GST_WARNING_OBJECT (self, "failed to create volume element");
    return;
  }
  GST_BIN_GET_CLASS (self)->add_element (GST_BIN_CAST (self),
      self->volume_element);

  GstPad *volume_sink = gst_element_get_static_pad (self->volume_element, "sink");
  GstPad *ghost_sink = gst_ghost_pad_new_from_template ("main_sink", volume_sink, GST_PAD_PAD_TEMPLATE (volume_sink));
  gst_object_unref (volume_sink);
  gst_element_add_pad (GST_ELEMENT_CAST (self), ghost_sink);  

  GstPad *volume_src = gst_element_get_static_pad (self->volume_element, "src");
  GstPad *ghost_src = gst_ghost_pad_new_from_template ("main_src", volume_src, GST_PAD_PAD_TEMPLATE (volume_src));
  gst_object_unref (volume_src);
  gst_element_add_pad (GST_ELEMENT_CAST (self), ghost_src);  

  self->ad_sink = gst_pad_new_from_static_template(&gst_adcontrol_sink_template, "ad_sink");
  gst_pad_use_fixed_caps (self->ad_sink);
  gst_element_add_pad (GST_ELEMENT (self), self->ad_sink);
  gst_pad_set_chain_function (self->ad_sink, gst_adcontrol_chain);
}

void
gst_adcontrol_set_property (GObject * object, guint property_id,
    const GValue * value, GParamSpec * pspec)
{
  GstAdcontrol *adcontrol = GST_ADCONTROL (object);

  GST_DEBUG_OBJECT (adcontrol, "set_property");

  switch (property_id) {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

void
gst_adcontrol_get_property (GObject * object, guint property_id,
    GValue * value, GParamSpec * pspec)
{
  GstAdcontrol *adcontrol = GST_ADCONTROL (object);

  GST_DEBUG_OBJECT (adcontrol, "get_property");

  switch (property_id) {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

void
gst_adcontrol_dispose (GObject * object)
{
  GstAdcontrol *adcontrol = GST_ADCONTROL (object);

  GST_DEBUG_OBJECT (adcontrol, "dispose");

  /* clean up as possible.  may be called multiple times */

  G_OBJECT_CLASS (gst_adcontrol_parent_class)->dispose (object);
}

void
gst_adcontrol_finalize (GObject * object)
{
  GstAdcontrol *adcontrol = GST_ADCONTROL (object);

  GST_DEBUG_OBJECT (adcontrol, "finalize");

  /* clean up object here */

  G_OBJECT_CLASS (gst_adcontrol_parent_class)->finalize (object);
}

static gdouble
fade_byte_to_volume(const guint8 fade_byte)
{
  const gdouble db_per_step = 0.3;
  return -fade_byte * db_per_step;
}

/* assumes stereo pan, clamping values which only make sense for surround into the stereo range */
static gdouble
pan_byte_to_pan(guint8 pan_byte)
{
  gint pan;
  if (pan_byte >= 0x16 && pan_byte <= 0x7f) {
    pan = 0x15;
  } else if (pan_byte >= 0x80 && pan_byte <= 0xea) {
    pan = 0xeb;
  }
  if (pan > 0x80) {
    pan = 0x100 - pan;
  }
  const gdouble MAX = 0x15;
  return pan / MAX;
}

static GstFlowReturn
gst_adcontrol_chain (GstPad * pad, GstObject * parent, GstBuffer *buf)
{
  GstAdcontrol *self = GST_ADCONTROL (parent);

  GstClockTime ts = GST_BUFFER_PTS(buf);
  GstMapInfo map;
  if (gst_buffer_get_size(buf) < 9) {
    GST_DEBUG_OBJECT (self, "audio descriptor too short");
    return GST_FLOW_ERROR;
  }
  gst_buffer_map (buf, &map, GST_MAP_READ);
  // TODO: extract descriptor-parsing code, validate headers, etc.
  guint8 fade_byte = map.data[7];
  guint8 pan_byte = map.data[8];
  gst_buffer_unmap(buf, &map);

  gst_stream_volume_set_volume(GST_STREAM_VOLUME(self->volume_element), GST_STREAM_VOLUME_FORMAT_DB, fade_byte_to_volume(fade_byte));
  //g_object_set (self->volume_element, "volume", fade_byte_to_volume(fade_byte), NULL);

  GST_DEBUG_OBJECT (self, "set volume %f ts=%" GST_TIME_FORMAT, fade_byte_to_volume(fade_byte), GST_TIME_ARGS(ts));

  return GST_FLOW_OK;
}
