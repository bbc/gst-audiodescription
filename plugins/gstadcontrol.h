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

#ifndef _GST_ADCONTROL_H_
#define _GST_ADCONTROL_H_

#include <gst/base/gstbasesink.h>
#include <gst/controller/gstinterpolationcontrolsource.h>

G_BEGIN_DECLS

#define GST_TYPE_ADCONTROL   (gst_adcontrol_get_type())
#define GST_ADCONTROL(obj)   (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_ADCONTROL,GstAdcontrol))
#define GST_ADCONTROL_CLASS(klass)   (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_ADCONTROL,GstAdcontrolClass))
#define GST_IS_ADCONTROL(obj)   (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_ADCONTROL))
#define GST_IS_ADCONTROL_CLASS(obj)   (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_ADCONTROL))

typedef struct _GstAdcontrol GstAdcontrol;
typedef struct _GstAdcontrolClass GstAdcontrolClass;

struct _GstAdcontrol
{
  GstBaseSink base_adcontrol;

  GstElement *volume_element;
  GstPad *main_sink;
  GstPad *main_src;
  GstPad *ad_sink;
  GstControlSource *fade_control;
};

struct _GstAdcontrolClass
{
  GstBaseSinkClass base_adcontrol_class;
};

GType gst_adcontrol_get_type (void);

G_END_DECLS

#endif
