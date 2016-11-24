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
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef _GST_WHP198DEC_H_
#define _GST_WHP198DEC_H_

#include <stdbool.h>

G_BEGIN_DECLS

#define GST_TYPE_WHP198DEC   (gst_whp198dec_get_type())
#define GST_WHP198DEC(obj)   (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_WHP198DEC,GstWhp198dec))
#define GST_WHP198DEC_CLASS(klass)   (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_WHP198DEC,GstWhp198decClass))
#define GST_IS_WHP198DEC(obj)   (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_WHP198DEC))
#define GST_IS_WHP198DEC_CLASS(obj)   (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_WHP198DEC))

typedef struct _GstWhp198dec GstWhp198dec;
typedef struct _GstWhp198decClass GstWhp198decClass;

struct _GstWhp198decManchester {
  gint last_sample;
  int state;
  double duration_estimate;
  gint64 in_sample_count;
  double next_expected_transition_sample;
};

struct _GstWhp198dec
{
  GstElement base_whp198dec;

  GstPad *sinkpad, *srcpad;

  // state of Manchester Encoding decode process,
  struct _GstWhp198decManchester manchester;

  // state of AD Descriptor recogniser,
  struct {
    guint64 accumulator;
    int state;
    int remaining_tail_bits;
    GstBuffer *buffer;
    int buffer_write_offset;
  } descriptor;
};

struct _GstWhp198decClass
{
  GstElementClass base_whp198dec_class;
};

GType gst_whp198dec_get_type (void);

G_END_DECLS

#endif
