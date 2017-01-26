/* GStreamer
 * David Holroyd <dave@badgers-in-foil.co.uk>, Copyright (C) BBC 2016-2017
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
 * SECTION:element-gstwhp198dec
 *
 * The whp198dec element decodes Audio Description descriptors encoded in an
 * audio waveform per <ulink url="http://downloads.bbc.co.uk/rd/pubs/whp/whp-pdf-files/WHP198.pdf">BBC R&D Whitepaper WHP 198</ulink>.
 *
 * <refsect2>
 * <title>Example launch line</title>
 * |[
 * gst-launch-1.0 filesrc location=test.wav ! wavparse ! deinterleave name=d d.src_1 ! audioconvert ! whp198dec  ! fakesink dump=true
 * ]|
 * Extract WHP198 waveform from a stereo WAV file and dump the decoded descriptors
 * </refsect2>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <math.h>
#include <gst/gst.h>
#include "gstwhp198dec.h"

GST_DEBUG_CATEGORY_STATIC (gst_whp198dec_debug_category);
#define GST_CAT_DEFAULT gst_whp198dec_debug_category

/* prototypes */


static void gst_whp198dec_set_property (GObject * object,
    guint property_id, const GValue * value, GParamSpec * pspec);
static void gst_whp198dec_get_property (GObject * object,
    guint property_id, GValue * value, GParamSpec * pspec);
static void gst_whp198dec_dispose (GObject * object);
static void gst_whp198dec_finalize (GObject * object);

static GstFlowReturn gst_whp198dec_handle_frame (GstWhp198dec *dec,
    GstBuffer * buffer);

enum
{
  PROP_0
};

enum
{
  STATE_UNSYNCHRONISED,
  STATE_FIRST_TRANSITION,
  STATE_SYNCHRONISED
};

enum
{
  AD_STATE_AWAIT_TAG,
  AD_STATE_CONSUME_TAIL
};


/* pad templates */

static GstStaticPadTemplate gst_whp198dec_src_template =
GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("application/x-tr_101_154_ad_descriptor")
    );

static GstStaticPadTemplate gst_whp198dec_sink_template =
GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("audio/x-raw,format=S16LE,rate=48000,"
        "channels=1,layout=interleaved")
    );


/* class initialization */

G_DEFINE_TYPE_WITH_CODE (GstWhp198dec, gst_whp198dec, GST_TYPE_ELEMENT,
    GST_DEBUG_CATEGORY_INIT (gst_whp198dec_debug_category, "whp198dec", 0,
        "debug category for whp198dec element"));

static void
gst_whp198dec_class_init (GstWhp198decClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  /* Setting up pads and setting metadata should be moved to
     base_class_init if you intend to subclass this class. */
  gst_element_class_add_pad_template (GST_ELEMENT_CLASS (klass),
      gst_static_pad_template_get (&gst_whp198dec_src_template));
  gst_element_class_add_pad_template (GST_ELEMENT_CLASS (klass),
      gst_static_pad_template_get (&gst_whp198dec_sink_template));

  gst_element_class_set_static_metadata (GST_ELEMENT_CLASS (klass),
      "WHP198 Audio Description data track decoder",
      "Generic",
      "Decodes an Audio Description data track embedded in an audio channel per BBC R&D White Paper 198",
      "David Holroyd <dave@badgers-in-foil.co.uk>");

  gobject_class->set_property = gst_whp198dec_set_property;
  gobject_class->get_property = gst_whp198dec_get_property;
  gobject_class->dispose = gst_whp198dec_dispose;
  gobject_class->finalize = gst_whp198dec_finalize;
}

static GstFlowReturn
gst_whp198dec_chain (GstPad * pad, GstObject * parent, GstBuffer * buffer)
{
  GstWhp198dec *whp198dec = GST_WHP198DEC (parent);
  return gst_whp198dec_handle_frame (whp198dec, buffer);
}

static void
gst_whp198dec_init (GstWhp198dec * whp198dec)
{
  whp198dec->manchester.last_sample = 0;
  whp198dec->manchester.state = STATE_UNSYNCHRONISED;
  whp198dec->descriptor.accumulator = 0;
  whp198dec->descriptor.state = AD_STATE_AWAIT_TAG;
  whp198dec->descriptor.buffer = NULL;
  whp198dec->descriptor.buffer_write_offset = 0;

  whp198dec->srcpad =
      gst_pad_new_from_static_template (&gst_whp198dec_src_template, "src");
  whp198dec->sinkpad =
      gst_pad_new_from_static_template (&gst_whp198dec_sink_template, "sink");

  gst_pad_set_chain_function (whp198dec->sinkpad,
      GST_DEBUG_FUNCPTR (gst_whp198dec_chain));
  gst_pad_use_fixed_caps (whp198dec->sinkpad);
  gst_pad_use_fixed_caps (whp198dec->srcpad);

  gst_element_add_pad (GST_ELEMENT (whp198dec), whp198dec->srcpad);
  gst_element_add_pad (GST_ELEMENT (whp198dec), whp198dec->sinkpad);
}

void
gst_whp198dec_set_property (GObject * object, guint property_id,
    const GValue * value, GParamSpec * pspec)
{
  GstWhp198dec *whp198dec = GST_WHP198DEC (object);

  GST_DEBUG_OBJECT (whp198dec, "set_property");

  switch (property_id) {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

void
gst_whp198dec_get_property (GObject * object, guint property_id,
    GValue * value, GParamSpec * pspec)
{
  GstWhp198dec *whp198dec = GST_WHP198DEC (object);

  GST_DEBUG_OBJECT (whp198dec, "get_property");

  switch (property_id) {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

void
gst_whp198dec_dispose (GObject * object)
{
  GstWhp198dec *whp198dec = GST_WHP198DEC (object);

  GST_DEBUG_OBJECT (whp198dec, "dispose");

  G_OBJECT_CLASS (gst_whp198dec_parent_class)->dispose (object);
}

void
gst_whp198dec_finalize (GObject * object)
{
  GstWhp198dec *whp198dec = GST_WHP198DEC (object);

  GST_DEBUG_OBJECT (whp198dec, "finalize");

  /* clean up object here */

  G_OBJECT_CLASS (gst_whp198dec_parent_class)->finalize (object);
}


#define AD_TEXT_TAG 0x4454474144

static void
ad_discontinuity(GstWhp198dec *dec)
{
  dec->descriptor.state = AD_STATE_AWAIT_TAG;
  dec->descriptor.accumulator = 0;
  if (dec->descriptor.buffer) {
    gst_buffer_unref(dec->descriptor.buffer);
    dec->descriptor.buffer = NULL;
  }
}


static unsigned short crc_table [0x100] = {
  0x0000, 0x1021, 0x2042, 0x3063, 0x4084, 0x50a5, 0x60c6, 0x70e7, 0x8108,
  0x9129, 0xa14a, 0xb16b, 0xc18c, 0xd1ad, 0xe1ce, 0xf1ef, 0x1231, 0x0210,
  0x3273, 0x2252, 0x52b5, 0x4294, 0x72f7, 0x62d6, 0x9339, 0x8318, 0xb37b,
  0xa35a, 0xd3bd, 0xc39c, 0xf3ff, 0xe3de, 0x2462, 0x3443, 0x0420, 0x1401,
  0x64e6, 0x74c7, 0x44a4, 0x5485, 0xa56a, 0xb54b, 0x8528, 0x9509, 0xe5ee,
  0xf5cf, 0xc5ac, 0xd58d, 0x3653, 0x2672, 0x1611, 0x0630, 0x76d7, 0x66f6,
  0x5695, 0x46b4, 0xb75b, 0xa77a, 0x9719, 0x8738, 0xf7df, 0xe7fe, 0xd79d,
  0xc7bc, 0x48c4, 0x58e5, 0x6886, 0x78a7, 0x0840, 0x1861, 0x2802, 0x3823,
  0xc9cc, 0xd9ed, 0xe98e, 0xf9af, 0x8948, 0x9969, 0xa90a, 0xb92b, 0x5af5,
  0x4ad4, 0x7ab7, 0x6a96, 0x1a71, 0x0a50, 0x3a33, 0x2a12, 0xdbfd, 0xcbdc,
  0xfbbf, 0xeb9e, 0x9b79, 0x8b58, 0xbb3b, 0xab1a, 0x6ca6, 0x7c87, 0x4ce4,
  0x5cc5, 0x2c22, 0x3c03, 0x0c60, 0x1c41, 0xedae, 0xfd8f, 0xcdec, 0xddcd,
  0xad2a, 0xbd0b, 0x8d68, 0x9d49, 0x7e97, 0x6eb6, 0x5ed5, 0x4ef4, 0x3e13,
  0x2e32, 0x1e51, 0x0e70, 0xff9f, 0xefbe, 0xdfdd, 0xcffc, 0xbf1b, 0xaf3a,
  0x9f59, 0x8f78, 0x9188, 0x81a9, 0xb1ca, 0xa1eb, 0xd10c, 0xc12d, 0xf14e,
  0xe16f, 0x1080, 0x00a1, 0x30c2, 0x20e3, 0x5004, 0x4025, 0x7046, 0x6067,
  0x83b9, 0x9398, 0xa3fb, 0xb3da, 0xc33d, 0xd31c, 0xe37f, 0xf35e, 0x02b1,
  0x1290, 0x22f3, 0x32d2, 0x4235, 0x5214, 0x6277, 0x7256, 0xb5ea, 0xa5cb,
  0x95a8, 0x8589, 0xf56e, 0xe54f, 0xd52c, 0xc50d, 0x34e2, 0x24c3, 0x14a0,
  0x0481, 0x7466, 0x6447, 0x5424, 0x4405, 0xa7db, 0xb7fa, 0x8799, 0x97b8,
  0xe75f, 0xf77e, 0xc71d, 0xd73c, 0x26d3, 0x36f2, 0x0691, 0x16b0, 0x6657,
  0x7676, 0x4615, 0x5634, 0xd94c, 0xc96d, 0xf90e, 0xe92f, 0x99c8, 0x89e9,
  0xb98a, 0xa9ab, 0x5844, 0x4865, 0x7806, 0x6827, 0x18c0, 0x08e1, 0x3882,
  0x28a3, 0xcb7d, 0xdb5c, 0xeb3f, 0xfb1e, 0x8bf9, 0x9bd8, 0xabbb, 0xbb9a,
  0x4a75, 0x5a54, 0x6a37, 0x7a16, 0x0af1, 0x1ad0, 0x2ab3, 0x3a92, 0xfd2e,
  0xed0f, 0xdd6c, 0xcd4d, 0xbdaa, 0xad8b, 0x9de8, 0x8dc9, 0x7c26, 0x6c07,
  0x5c64, 0x4c45, 0x3ca2, 0x2c83, 0x1ce0, 0x0cc1, 0xef1f, 0xff3e, 0xcf5d,
  0xdf7c, 0xaf9b, 0xbfba, 0x8fd9, 0x9ff8, 0x6e17, 0x7e36, 0x4e55, 0x5e74,
  0x2e93, 0x3eb2, 0x0ed1, 0x1ef0
};

static guint16 crc_16_ccitt(const guint8 *data, const size_t length)
{ 
   gint crc = 0x1d0f;
   gint temp;

   for (size_t count = 0; count < length; ++count) {
     temp = (*data++ ^ (crc >> 8)) & 0xff;
     crc = crc_table[temp] ^ (crc << 8);
   }

   return (guint16)crc;
}

#define BYTE 8

static void
ad_decoded_bit(GstWhp198dec *dec, const int bit, GstClockTime ts)
{
  dec->descriptor.accumulator <<= 1;
  dec->descriptor.accumulator |= bit;
  switch (dec->descriptor.state) {
    case AD_STATE_AWAIT_TAG:
      if (((dec->descriptor.accumulator >> 16) & 0x00ffffffffff) == AD_TEXT_TAG) {
        int descriptor_length = (dec->descriptor.accumulator >> (7*BYTE)) & 0x0f;
        if (descriptor_length < 8) {
          GST_DEBUG_OBJECT (dec, "invalid descriptor length %d", descriptor_length);
          return;
        }
        int revision_text_tag = (dec->descriptor.accumulator >> BYTE) & 0xff;
        int AD_fade  =  dec->descriptor.accumulator  & 0xff;
        GST_DEBUG_OBJECT (dec, "found descriptor, length=%d, revision=%x, fade=%x", descriptor_length, revision_text_tag, AD_fade);
        int reserved_bytes = 7;
        GstBuffer *buf = gst_buffer_new_and_alloc (1 + descriptor_length + reserved_bytes);
        // Assign a timestamp to the buffer holding the descriptor based on the
        // timestamp of the just-decoded manchester bit.  Might make more sense
        // to use the timestamp of what we now believe to be the initial bit of
        // this descriptor, but it's not clear to me exactly what the intended
        // time of application is for a given descriptor, so this will probably
        // do,
        GST_BUFFER_PTS(buf) = ts;
        GstMapInfo map;
        gst_buffer_map (buf, &map, GST_MAP_WRITE);
        dec->descriptor.buffer_write_offset = 0;
        map.data[dec->descriptor.buffer_write_offset++] = (dec->descriptor.accumulator >> (7*BYTE)) & 0xff;
        map.data[dec->descriptor.buffer_write_offset++] = (dec->descriptor.accumulator >> (6*BYTE)) & 0xff;
        map.data[dec->descriptor.buffer_write_offset++] = (dec->descriptor.accumulator >> (5*BYTE)) & 0xff;
        map.data[dec->descriptor.buffer_write_offset++] = (dec->descriptor.accumulator >> (4*BYTE)) & 0xff;
        map.data[dec->descriptor.buffer_write_offset++] = (dec->descriptor.accumulator >> (3*BYTE)) & 0xff;
        map.data[dec->descriptor.buffer_write_offset++] = (dec->descriptor.accumulator >> (2*BYTE)) & 0xff;
        map.data[dec->descriptor.buffer_write_offset++] = (dec->descriptor.accumulator >> (1*BYTE)) & 0xff;
        map.data[dec->descriptor.buffer_write_offset++] = (dec->descriptor.accumulator            ) & 0xff;
        gst_buffer_unmap(buf, &map);
        dec->descriptor.state = AD_STATE_CONSUME_TAIL;
        int descriptor_bytes_consumed = 6;
        int descriptor_bytes_remaining = descriptor_length - descriptor_bytes_consumed;
        dec->descriptor.remaining_tail_bits = (descriptor_bytes_remaining + reserved_bytes - 1) * 8;
        dec->descriptor.buffer = buf;
      }
      break;
    case AD_STATE_CONSUME_TAIL:
      dec->descriptor.remaining_tail_bits--;
      if (dec->descriptor.remaining_tail_bits % 8 == 0) {
        GstMapInfo map;
        gst_buffer_map (dec->descriptor.buffer, &map, GST_MAP_WRITE);
        map.data[dec->descriptor.buffer_write_offset++] = dec->descriptor.accumulator & 0xff;
        gst_buffer_unmap(dec->descriptor.buffer, &map);
      }
      if (dec->descriptor.remaining_tail_bits == 0) {
        dec->descriptor.state = AD_STATE_AWAIT_TAG;
        GstMapInfo map;
        gst_buffer_map (dec->descriptor.buffer, &map, GST_MAP_WRITE);
        int crc = crc_16_ccitt(map.data, gst_buffer_get_size(dec->descriptor.buffer));
        gst_buffer_unmap(dec->descriptor.buffer, &map);
        if (crc == 0) {
          GstFlowReturn ret = gst_pad_push (dec->srcpad, dec->descriptor.buffer);
        } else {
          GST_DEBUG_OBJECT (dec, "Incorrect descriptor CRC found");
          gst_buffer_unref(dec->descriptor.buffer);
        }
        dec->descriptor.buffer = NULL;
      }
      break;
  }
}

static bool
epsilon_equals(const float a, const float b, const float epsilon)
{
  return fabs(a - b) < epsilon;
}

#define SAMPLE_FREQ 48000  // samples-per-second
#define DATA_RATE   1280.0   // bits-per-second
#define EPSILON_SAMPLES 5
#define THRESHOLD 1000

static gboolean
sign_change(gint a, gint b)
{
  return (a < 0) != (b < 0);
}

static double
current_transition_estimate_error(struct _GstWhp198decManchester *manchester)
{
  return manchester->in_sample_count - manchester->next_expected_transition_sample;
}

enum TransitionType
{
  TRANSITION_BIT,
  TRANSITION_IGNORE,
  TRANSITION_SYNC_LOST
};

static enum TransitionType
mark_transition(struct _GstWhp198decManchester *manchester)
{
  enum TransitionType detect = TRANSITION_IGNORE;

  if (manchester->state == STATE_UNSYNCHRONISED) {
    manchester->state = STATE_FIRST_TRANSITION;
    manchester->duration_estimate = SAMPLE_FREQ/DATA_RATE;
    manchester->next_expected_transition_sample = manchester->in_sample_count + manchester->duration_estimate;
  } else if (manchester->state == STATE_FIRST_TRANSITION) {
    double error = current_transition_estimate_error(manchester);
    if (epsilon_equals(error, -manchester->duration_estimate / 2, EPSILON_SAMPLES)) {
      // this is a transition inbetween bit-centres, rather than a
      // bit-center transition itself.  Ignore it and wait for the bit
      // centre to turn up in about duration_estimate/2 samples
    } else if (epsilon_equals(error, manchester->duration_estimate / 2, EPSILON_SAMPLES)) {
      // we are out of phase (initial transition must have been a half
      // bit),
      manchester->next_expected_transition_sample -= manchester->duration_estimate / 2;
    } else if (epsilon_equals(error, 0, EPSILON_SAMPLES)) {
      // found transition at the expected bit-centre, so we are
      // hopefully in sync,
      manchester->state = STATE_SYNCHRONISED;
      manchester->next_expected_transition_sample += manchester->duration_estimate;
      //GST_DEBUG_OBJECT (dec, "sync found at in_sample_count=%ld", manchester->in_sample_count);
    } else {
      manchester->state = STATE_UNSYNCHRONISED;
      //GST_DEBUG_OBJECT (dec, "failed to acquire sync (error=%f, in_sample_count=%ld, next_expected_transition_sample=%f, duration_estimate=%f)",
      //                  error, dec->manchester.in_sample_count, dec->manchester.next_expected_transition_sample, dec->manchester.duration_estimate);
    }
  } else if (manchester->state == STATE_SYNCHRONISED) {
    double error = current_transition_estimate_error(manchester);
    if (epsilon_equals(error, -manchester->duration_estimate / 2, EPSILON_SAMPLES)) {
      // this is a transition inbetween bit-centres, rather than
      // a bit-center transition itself
    } else if (epsilon_equals(error, 0.0, EPSILON_SAMPLES)) {
      detect = TRANSITION_BIT;
      manchester->next_expected_transition_sample += manchester->duration_estimate;
    } else {
      manchester->state = STATE_UNSYNCHRONISED;
      detect = TRANSITION_SYNC_LOST;
    }
  }

  return detect;
}

static void
process_samples (GstWhp198dec *dec, gint16 *data, gint samples, GstClockTime buffer_ts)
{
  struct _GstWhp198decManchester *manchester = &dec->manchester;
  for (int i=0; i<samples; i++) {
    gint sample = data[i];

    if (sign_change(sample, manchester->last_sample)) {
      switch (mark_transition(manchester)) {
        case TRANSITION_BIT: ;
          int bit = sample < 0 ? 1 : 0;
          ad_decoded_bit(dec, bit, buffer_ts + i * GST_SECOND / SAMPLE_FREQ);
          break;
        case TRANSITION_SYNC_LOST:
          GST_DEBUG_OBJECT (dec, "lost sync");
          ad_discontinuity(dec);
          break;
        case TRANSITION_IGNORE:
          // nothing to do
          break;
      }
    }
    manchester->last_sample = sample;
    manchester->in_sample_count++;
  }
}

static GstFlowReturn
gst_whp198dec_handle_frame (GstWhp198dec *dec, GstBuffer * buffer)
{
  GstMapInfo map;

  if (!buffer) {
    return GST_FLOW_OK;
  }

  gst_buffer_map (buffer, &map, GST_MAP_READ);
  process_samples (dec,
                   (gint16 *)map.data,
                   map.size / sizeof(guint16),
                   GST_BUFFER_PTS(buffer));
  gst_buffer_unmap (buffer, &map);
  return GST_FLOW_OK;
}
