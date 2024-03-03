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
  PROP_DELIMITER,
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
G_DEFINE_TYPE (Gstbuffersplitter, gst_buffersplitter, GST_TYPE_BASE_PARSE);

GST_ELEMENT_REGISTER_DEFINE (buffersplitter, "buffersplitter", GST_RANK_NONE,
    GST_TYPE_BUFFER_SPLITTER);

static gboolean gst_buffersplitter_start (GstBaseParse * parse);

static gboolean gst_buffersplitter_set_sink_caps (GstBaseParse * parse, GstCaps * caps);

static GstFlowReturn gst_buffersplitter_handle_frame (GstBaseParse * parse,
    GstBaseParseFrame * frame, gint * skipsize);

static void gst_buffersplitter_set_property (GObject * object,
    guint prop_id, const GValue * value, GParamSpec * pspec);
static void gst_buffersplitter_get_property (GObject * object,
    guint prop_id, GValue * value, GParamSpec * pspec);

/* GObject vmethod implementations */

/* initialize the buffersplitter's class */
static void
gst_buffersplitter_class_init (GstbuffersplitterClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;
  GstBaseParseClass *parse_class;

  gobject_class = (GObjectClass *) klass;
  gstelement_class = (GstElementClass *) klass;
  parse_class = GST_BASE_PARSE_CLASS (klass);

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

  parse_class->start = GST_DEBUG_FUNCPTR (gst_buffersplitter_start);
  parse_class->set_sink_caps = GST_DEBUG_FUNCPTR (gst_buffersplitter_set_sink_caps);
  parse_class->handle_frame = GST_DEBUG_FUNCPTR (gst_buffersplitter_handle_frame);
}

/* ascii must have an even number of chars */
static void update_bytes(Gstbuffersplitter * splitter)
{
// TODO: add support for lowercase chars
#define CHAR_TO_NIBBLE(x) (x >= '0' && x <= '9' ? x - '0' : x - 'A')
  const gchar *ascii = splitter->delimiter;
  g_free(splitter->delimiter_bytes);
  gsize bytesize = (strlen(ascii)+1)/2;
  guint8 *bytes = malloc(bytesize);
  for (gsize i = 0; i < bytesize; i++)
    bytes[i] = CHAR_TO_NIBBLE(ascii[i*2]) << 4 | CHAR_TO_NIBBLE(ascii[i*2+1]);

  splitter->delimiter_size = bytesize;
  splitter->delimiter_bytes = bytes;
}

/* initialize the new element
 * instantiate pads and add them to element
 * set pad callback functions
 * initialize instance structure
 */
static void
gst_buffersplitter_init (Gstbuffersplitter * splitter)
{
  splitter->delimiter = "000001";

  // TODO: make this data private
  splitter->delimiter_size = 3;
  splitter->delimiter_bytes = g_malloc(splitter->delimiter_size);
  splitter->delimiter_bytes[0] = 0;
  splitter->delimiter_bytes[1] = 0;
  splitter->delimiter_bytes[2] = 1;
}

static void
gst_buffersplitter_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  Gstbuffersplitter *splitter = GST_BUFFER_SPLITTER (object);

  switch (prop_id) {
    case PROP_DELIMITER:
      // TODO: validate an even number of hex-only chars
      splitter->delimiter = g_value_get_string (value);
      update_bytes(splitter);
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
  Gstbuffersplitter *splitter = GST_BUFFER_SPLITTER (object);

  switch (prop_id) {
    case PROP_DELIMITER:
      g_value_set_string (value, splitter->delimiter);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

/* GstBaseParse vmethod implementations */

static gboolean gst_buffersplitter_start (GstBaseParse * parse)
{
  Gstbuffersplitter *splitter = GST_BUFFER_SPLITTER (parse);

  gst_base_parse_set_min_frame_size (parse, splitter->delimiter_size+1);

  return TRUE;
}

static gboolean gst_buffersplitter_set_sink_caps (GstBaseParse * parse, GstCaps * caps)
{
  Gstbuffersplitter *splitter;
  splitter = GST_BUFFER_SPLITTER (parse);

  GST_DEBUG_OBJECT(splitter, "%" GST_PTR_FORMAT, caps);
  GstCaps *downstream_caps = gst_caps_fixate(gst_pad_peer_query_caps(splitter->element.srcpad, caps));
  GST_LOG_OBJECT(splitter, "set srcpad caps: %" GST_PTR_FORMAT, downstream_caps);
  gst_pad_set_caps(splitter->element.srcpad, downstream_caps);
  return TRUE;
}

static const guint8* next_buffer(Gstbuffersplitter * splitter, const guint8 *buff, const guint8 *end)
{
  const guint8 *next = NULL;
  const guint8 *ptr = buff;

  ptr++; // skip first byte

  while (!next && ptr <= end - 5) {
    gsize i;
    for (i = 0; i < splitter->delimiter_size && ptr[i] == splitter->delimiter_bytes[i]; i++);
    if (i == splitter->delimiter_size)
      next = ptr;
    ptr++;
  }

  return next;
}

static GstFlowReturn gst_buffersplitter_handle_frame (GstBaseParse * parse,
    GstBaseParseFrame * frame, gint * skipsize)
{
  Gstbuffersplitter *splitter;
  splitter = GST_BUFFER_SPLITTER (parse);

  if (!gst_pad_has_current_caps(splitter->element.srcpad))
  {
    GstCaps *caps = gst_caps_fixate(gst_pad_peer_query_caps(splitter->element.srcpad, NULL));
    GST_LOG_OBJECT(splitter, "set srcpad caps: %" GST_PTR_FORMAT, caps);
    gst_pad_set_caps(splitter->element.srcpad, caps);
  }

  GstMapInfo map;
  gst_buffer_map(frame->buffer, &map, GST_MAP_READ);

  const guint8 *end = map.data + map.size;
  const guint8 *next;
  next = next_buffer(splitter, map.data, end);

  if (next)
    gst_base_parse_finish_frame(parse, frame, next - map.data);
  else if (GST_BASE_PARSE_DRAINING(parse))
    gst_base_parse_finish_frame(parse, frame, end - map.data);

  return GST_FLOW_OK;
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