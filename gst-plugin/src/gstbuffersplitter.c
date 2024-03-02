/*
 * GStreamer
 * Copyright (C) 2005 Thomas Vander Stichele <thomas@apestaart.org>
 * Copyright (C) 2005 Ronald S. Bultje <rbultje@ronald.bitfreak.net>
 * Copyright (C) 2024 Brendan McGrath <brendan@redmandi.com>
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Alternatively, the contents of this file may be used under the
 * GNU Lesser General Public License Version 2.1 (the "LGPL"), in
 * which case the following provisions apply instead of the ones
 * mentioned above:
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
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/**
 * SECTION:element-buffersplitter
 *
 * FIXME:Describe buffersplitter here.
 *
 * <refsect2>
 * <title>Example launch line</title>
 * |[
 * gst-launch -v -m fakesrc ! buffersplitter ! fakesink silent=TRUE
 * ]|
 * </refsect2>
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gst/gst.h>

#include "gstbuffersplitter.h"

GST_DEBUG_CATEGORY_STATIC (gst_buffersplitter_debug);
#define GST_CAT_DEFAULT gst_buffersplitter_debug

/* Filter signals and args */
enum
{
  /* FILL ME */
  LAST_SIGNAL
};

enum
{
  PROP_0,
  PROP_DELIMITER
};

/* the capabilities of the inputs and outputs.
 *
 * describe the real formats here.
 */
static GstStaticPadTemplate sink_factory = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("ANY")
    );

static GstStaticPadTemplate src_factory = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("ANY")
    );

#define gst_buffersplitter_parent_class parent_class
G_DEFINE_TYPE (Gstbuffersplitter, gst_buffersplitter, GST_TYPE_ELEMENT);

GST_ELEMENT_REGISTER_DEFINE (buffersplitter, "buffersplitter", GST_RANK_NONE,
    GST_TYPE_BUFFER_SPLITTER);

static void gst_buffersplitter_set_property (GObject * object,
    guint prop_id, const GValue * value, GParamSpec * pspec);
static void gst_buffersplitter_get_property (GObject * object,
    guint prop_id, GValue * value, GParamSpec * pspec);

static gboolean gst_buffersplitter_sink_event (GstPad * pad,
    GstObject * parent, GstEvent * event);
static GstFlowReturn gst_buffersplitter_chain (GstPad * pad,
    GstObject * parent, GstBuffer * buf);

/* GObject vmethod implementations */

/* initialize the buffersplitter's class */
static void
gst_buffersplitter_class_init (GstbuffersplitterClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;

  gobject_class = (GObjectClass *) klass;
  gstelement_class = (GstElementClass *) klass;

  gobject_class->set_property = gst_buffersplitter_set_property;
  gobject_class->get_property = gst_buffersplitter_get_property;

  g_object_class_install_property (gobject_class, PROP_DELIMITER,
      g_param_spec_string ("delimiter", "Delimiter", "The sequence of bytes on which to split",
          FALSE, G_PARAM_READWRITE));

  gst_element_class_set_details_simple (gstelement_class,
      "buffersplitter",
      "FIXME:Generic",
      "FIXME:Generic Template Element", "Brendan McGrath <brendan@redmandi.com>");

  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&src_factory));
  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&sink_factory));
}

/* ascii must have an even number of chars */
static void update_bytes(Gstbuffersplitter * filter)
{
// TODO: add support for lowercase chars
#define CHAR_TO_NIBBLE(x) (x >= '0' && x <= '9' ? x - '0' : x - 'A')
  const gchar *ascii = filter->delimiter;
  g_free(filter->delimiter_bytes);
  gsize bytesize = (strlen(ascii)+1)/2;
  guint8 *bytes = malloc(bytesize);
  for (gsize i = 0; i < bytesize; i++)
    bytes[i] = CHAR_TO_NIBBLE(ascii[i*2]) << 4 | CHAR_TO_NIBBLE(ascii[i*2+1]);

  filter->delimiter_size = bytesize;
  filter->delimiter_bytes = bytes;
}

/* initialize the new element
 * instantiate pads and add them to element
 * set pad callback functions
 * initialize instance structure
 */
static void
gst_buffersplitter_init (Gstbuffersplitter * filter)
{
  filter->sinkpad = gst_pad_new_from_static_template (&sink_factory, "sink");
  gst_pad_set_event_function (filter->sinkpad,
      GST_DEBUG_FUNCPTR (gst_buffersplitter_sink_event));
  gst_pad_set_chain_function (filter->sinkpad,
      GST_DEBUG_FUNCPTR (gst_buffersplitter_chain));
  GST_PAD_SET_PROXY_CAPS (filter->sinkpad);
  gst_element_add_pad (GST_ELEMENT (filter), filter->sinkpad);

  filter->srcpad = gst_pad_new_from_static_template (&src_factory, "src");
  GST_PAD_SET_PROXY_CAPS (filter->srcpad);
  gst_element_add_pad (GST_ELEMENT (filter), filter->srcpad);

  filter->buffer = gst_buffer_new();
  filter->delimiter = "000001";
  filter->delimiter_size = 3;
  filter->delimiter_bytes = g_malloc(filter->delimiter_size);
  filter->delimiter_bytes[0] = 0;
  filter->delimiter_bytes[1] = 0;
  filter->delimiter_bytes[2] = 1;
}

static void
gst_buffersplitter_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  Gstbuffersplitter *filter = GST_BUFFER_SPLITTER (object);

  switch (prop_id) {
    case PROP_DELIMITER:
      // TODO: validate an even number of hex-only chars
      filter->delimiter = g_value_get_string (value);
      update_bytes(filter);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_buffersplitter_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  Gstbuffersplitter *filter = GST_BUFFER_SPLITTER (object);

  switch (prop_id) {
    case PROP_DELIMITER:
      g_value_set_string (value, filter->delimiter);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

/* GstElement vmethod implementations */

/* this function handles sink events */
static gboolean
gst_buffersplitter_sink_event (GstPad * pad, GstObject * parent,
    GstEvent * event)
{
  Gstbuffersplitter *filter;
  gboolean ret;

  filter = GST_BUFFER_SPLITTER (parent);

  GST_LOG_OBJECT (filter, "Received %s event: %" GST_PTR_FORMAT,
      GST_EVENT_TYPE_NAME (event), event);

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_CAPS:
    {
      GstCaps *caps;

      gst_event_parse_caps (event, &caps);
      /* do something with the caps */

      /* and forward */
      ret = gst_pad_event_default (pad, parent, event);
      break;
    }
    default:
      ret = gst_pad_event_default (pad, parent, event);
      break;
  }
  return ret;
}

static const guint8* next_buffer(Gstbuffersplitter * filter, const guint8 *buff, const guint8 *end)
{
    const guint8 *next = NULL;
    const guint8 *ptr = buff;
  
    ptr++; // skip first byte

    while (!next && ptr <= end - 5) {
        gsize i;
        for (i = 0; i < filter->delimiter_size && ptr[i] == filter->delimiter_bytes[i]; i++);
        if (i == filter->delimiter_size)
            next = ptr;
        ptr++;
    }

    return next;
}


/* chain function
 * this function does the actual processing
 */
static GstFlowReturn
gst_buffersplitter_chain (GstPad * pad, GstObject * parent, GstBuffer * buf)
{
  Gstbuffersplitter *filter;

  filter = GST_BUFFER_SPLITTER (parent);

  filter->buffer = gst_buffer_append(filter->buffer, buf);
  
  GstFlowReturn ret = GST_FLOW_OK;
  GstMapInfo map;
  if (!gst_buffer_map(filter->buffer, &map, GST_MAP_READ)) {
    GST_ERROR("Couldn't map memory");
    ret = GST_FLOW_ERROR;
    goto out;
  }

  gboolean sent = FALSE;
  const guint8 *end = map.data + map.size;
  const guint8 *next;
  GstBuffer *send, *keep;
  while (ret == GST_FLOW_OK && (next = next_buffer(filter, map.data, end))) {
    send = gst_buffer_new_memdup(map.data, next - map.data);
    ret = gst_pad_push (filter->srcpad, send);
    if (ret == GST_FLOW_OK) {
      sent = TRUE;
      map.data = (guint8*) next;
    }
  }
  if (sent)
      keep = gst_buffer_new_memdup(map.data, end - map.data);
  gst_buffer_unmap(filter->buffer, &map);
  if (sent) {
    gst_buffer_unref(filter->buffer);
    filter->buffer = keep;
  }

out:
  return ret;
}


/* entry point to initialize the plug-in
 * initialize the plug-in itself
 * register the element factories and other features
 */
static gboolean
buffersplitter_init (GstPlugin * buffersplitter)
{
  /* debug category for filtering log messages
   *
   * exchange the string 'Template buffersplitter' with your description
   */
  GST_DEBUG_CATEGORY_INIT (gst_buffersplitter_debug, "buffersplitter",
      0, "Template buffersplitter");

  return GST_ELEMENT_REGISTER (buffersplitter, buffersplitter);
}

/* PACKAGE: this is usually set by meson depending on some _INIT macro
 * in meson.build and then written into and defined in config.h, but we can
 * just set it ourselves here in case someone doesn't use meson to
 * compile this code. GST_PLUGIN_DEFINE needs PACKAGE to be defined.
 */
#ifndef PACKAGE
#define PACKAGE "myfirstbuffersplitter"
#endif

/* gstreamer looks for this structure to register buffersplitters
 *
 * exchange the string 'Template buffersplitter' with your buffersplitter description
 */
GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    buffersplitter,
    "buffersplitter",
    buffersplitter_init,
    PACKAGE_VERSION, GST_LICENSE, GST_PACKAGE_NAME, GST_PACKAGE_ORIGIN)
