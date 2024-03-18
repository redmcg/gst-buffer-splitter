/*
 * GStreamer
 * Copyright (C) 2005 Thomas Vander Stichele <thomas@apestaart.org>
 * Copyright (C) 2005 Ronald S. Bultje <rbultje@ronald.bitfreak.net>
 * Copyright (C) 2024 Brendan McGrath <<user@hostname.org>>
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
 * SECTION:element-bufferjoiner
 *
 * FIXME:Describe bufferjoiner here.
 *
 * <refsect2>
 * <title>Example launch line</title>
 * |[
 * gst-launch -v -m fakesrc ! bufferjoiner ! fakesink silent=TRUE
 * ]|
 * </refsect2>
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <stdbool.h>
#include <gst/gst.h>

#include "gstbufferjoiner.h"

GST_DEBUG_CATEGORY_STATIC (gst_bufferjoiner_debug);
#define GST_CAT_DEFAULT gst_bufferjoiner_debug

#define DEFAULT_MAX_BYTES     (8192 * 2)
// #define DEFAULT_MAX_TIME_NS   33333333 
// #define DEFAULT_MAX_TIME_NS   1000000000
// #define DEFAULT_MAX_TIME_NS   12500000
#define DEFAULT_MAX_TIME_NS    21333333
/* Filter signals and args */
enum
{
  /* FILL ME */
  LAST_SIGNAL
};

enum
{
  PROP_0,
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

#define gst_bufferjoiner_parent_class parent_class
G_DEFINE_TYPE (Gstbufferjoiner, gst_bufferjoiner, GST_TYPE_ELEMENT);

GST_ELEMENT_REGISTER_DEFINE (bufferjoiner, "bufferjoiner", GST_RANK_NONE,
    GST_TYPE_BUFFERJOINER);

static void gst_bufferjoiner_set_property (GObject * object,
    guint prop_id, const GValue * value, GParamSpec * pspec);
static void gst_bufferjoiner_get_property (GObject * object,
    guint prop_id, GValue * value, GParamSpec * pspec);

static gboolean gst_bufferjoiner_sink_event (GstPad * pad,
    GstObject * parent, GstEvent * event);
static GstFlowReturn gst_bufferjoiner_chain (GstPad * pad,
    GstObject * parent, GstBuffer * buf);
static GstStateChangeReturn gst_bufferjoiner_change_state (GstElement * element,
              GstStateChange transition);

/* GObject vmethod implementations */

/* initialize the bufferjoiner's class */
static void
gst_bufferjoiner_class_init (GstbufferjoinerClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;

  gobject_class = (GObjectClass *) klass;
  gstelement_class = (GstElementClass *) klass;

  gobject_class->set_property = gst_bufferjoiner_set_property;
  gobject_class->get_property = gst_bufferjoiner_get_property;

  gstelement_class->change_state = GST_DEBUG_FUNCPTR(gst_bufferjoiner_change_state);

  gst_element_class_set_details_simple (gstelement_class,
      "bufferjoiner",
      "FIXME:Generic",
      "FIXME:Generic Template Element", "Brendan McGrath <<user@hostname.org>>");

  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&src_factory));
  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&sink_factory));
}

static gboolean
checkfull(GstDataQueue * queue,
                  guint visible,
                  guint bytes,
                  guint64 time,
                  gpointer checkdata)
{
    // return bytes >= DEFAULT_MAX_BYTES;
    return time >= DEFAULT_MAX_TIME_NS;
}

GstFlowReturn
send_it(Gstbufferjoiner *filter)
{
  GstBuffer *buffer = NULL;
  while(!gst_data_queue_is_empty(filter->queue))
  {
    GstBuffer *next;
    GstDataQueueItem *item;
    if (!gst_data_queue_pop(filter->queue, &item))
      break;

    next = GST_BUFFER(item->object);
    if (buffer)
    {
      gst_buffer_append(buffer, next);
      buffer->duration += GST_BUFFER_DURATION(next);
      buffer->offset_end = GST_BUFFER_OFFSET_END(next);
    }
    else
    {
      buffer = next;
    }
    g_free(item);
  }
  GST_LOG_OBJECT(GST_OBJECT(filter->srcpad), "sending %" GST_PTR_FORMAT, buffer);
  return gst_pad_push(filter->srcpad, buffer);
}

void
task (gpointer user_data)
{
  Gstbufferjoiner * joiner = user_data;
  if (!gst_data_queue_is_empty(joiner->queue))
      send_it(joiner);
  gst_pad_pause_task(joiner->srcpad);
}

/* initialize the new element
 * instantiate pads and add them to element
 * set pad callback functions
 * initialize instance structure
 */
static void
gst_bufferjoiner_init (Gstbufferjoiner * joiner)
{
  joiner->sinkpad = gst_pad_new_from_static_template (&sink_factory, "sink");
  gst_pad_set_event_function (joiner->sinkpad,
      GST_DEBUG_FUNCPTR (gst_bufferjoiner_sink_event));
  gst_pad_set_chain_function (joiner->sinkpad,
      GST_DEBUG_FUNCPTR (gst_bufferjoiner_chain));
  GST_PAD_SET_PROXY_CAPS (joiner->sinkpad);
  gst_element_add_pad (GST_ELEMENT (joiner), joiner->sinkpad);

  joiner->srcpad = gst_pad_new_from_static_template (&src_factory, "src");
  GST_PAD_SET_PROXY_CAPS (joiner->srcpad);
  gst_element_add_pad (GST_ELEMENT (joiner), joiner->srcpad);

  joiner->queue = gst_data_queue_new(checkfull, NULL, NULL, NULL);
}

static void
gst_bufferjoiner_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  Gstbufferjoiner *filter = GST_BUFFERJOINER (object);

  switch (prop_id) {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_bufferjoiner_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  Gstbufferjoiner *filter = GST_BUFFERJOINER (object);

  switch (prop_id) {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

/* GstElement vmethod implementations */

/* this function handles sink events */
static gboolean
gst_bufferjoiner_sink_event (GstPad * pad, GstObject * parent,
    GstEvent * event)
{
  Gstbufferjoiner *filter;
  gboolean ret;

  filter = GST_BUFFERJOINER (parent);

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
    case GST_EVENT_EOS:
      gst_pad_pause_task(filter->srcpad);
      gst_task_resume (GST_PAD_TASK(filter->srcpad));
      /* FALLTHROUGH */
    case GST_EVENT_FLUSH_START:
    case GST_EVENT_FLUSH_STOP:
    {
      gboolean flushing = GST_EVENT_TYPE (event) != GST_EVENT_FLUSH_STOP;
      gst_data_queue_set_flushing(filter->queue, flushing);
      if (flushing)
        gst_data_queue_flush(filter->queue);
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

void
destroy(void* data)
{
  GstDataQueueItem *item = data;
  gst_clear_buffer((GstBuffer**)&item->object);
  free(item);
}

/* chain function
 * this function does the actual processing
 */
static GstFlowReturn
gst_bufferjoiner_chain (GstPad * pad, GstObject * parent, GstBuffer * buf)
{
  Gstbufferjoiner *filter;
  GstDataQueueSize size;
  GstDataQueueItem *item = g_malloc(sizeof(*item));
  gboolean ret;
  item->object = GST_MINI_OBJECT(buf);
  item->size = gst_buffer_get_size(buf);
  item->duration = GST_BUFFER_DURATION(buf);
  item->visible = TRUE;
  item->destroy = destroy;

  filter = GST_BUFFERJOINER (parent);

  GST_LOG_OBJECT(GST_OBJECT(pad), "got %" GST_PTR_FORMAT, buf);
  ret = gst_data_queue_push(filter->queue, item);
  if (ret)
  {
    gst_data_queue_get_level(filter->queue, &size);
    // if (size.bytes >= DEFAULT_MAX_BYTES)
    if (size.time >= DEFAULT_MAX_TIME_NS)
    {
      gst_pad_pause_task(filter->srcpad);
      gst_task_resume (GST_PAD_TASK(filter->srcpad));
    }

    return GST_FLOW_OK;
  }
  else
  {
    destroy(item);
  }

  return GST_FLOW_FLUSHING;
}

static GstStateChangeReturn gst_bufferjoiner_change_state (GstElement * element,
              GstStateChange transition)
{
  Gstbufferjoiner *joiner = GST_BUFFERJOINER (element);
  switch(transition)
  {
    case GST_STATE_CHANGE_READY_TO_PAUSED:
      gst_data_queue_set_flushing(joiner->queue, FALSE);
      gst_pad_start_task(joiner->srcpad, task, gst_object_ref(joiner), gst_object_unref);
      break;

    case GST_STATE_CHANGE_PAUSED_TO_READY:
      gst_pad_stop_task(joiner->srcpad);
      gst_data_queue_set_flushing(joiner->queue, TRUE);
      break;

    default:
      break;
  }

  return GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);
}

/* entry point to initialize the plug-in
 * initialize the plug-in itself
 * register the element factories and other features
 */
gboolean
bufferjoiner_init (GstPlugin * bufferjoiner)
{
  /* debug category for filtering log messages
   *
   * exchange the string 'Template bufferjoiner' with your description
   */
  GST_DEBUG_CATEGORY_INIT (gst_bufferjoiner_debug, "bufferjoiner",
      0, "Template bufferjoiner");

  return GST_ELEMENT_REGISTER (bufferjoiner, bufferjoiner);
}

