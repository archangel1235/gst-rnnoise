/* GStreamer
 * Copyright (C) 2024 FIXME <fixme@example.com>
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
 * SECTION:element-gstrnnoise
 *
 * The rnnoise element does FIXME stuff.
 *
 * <refsect2>
 * <title>Example launch line</title>
 * |[
 * gst-launch-1.0 -v fakesrc ! rnnoise ! FIXME ! fakesink
 * ]|
 * FIXME Describe what the pipeline does.
 * </refsect2>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#define FRAME_SIZE_SHIFT 2
#define FRAME_SIZE 480
#define M_PI 3.141592654

#include <math.h>
#include <gst/gst.h>
#include <gst/audio/gstaudiofilter.h>
#include <gst/base/gstbasetransform.h> 
#include "gstrnnoise.h"
#include <stdbool.h>
#include <stdint.h>


GST_DEBUG_CATEGORY_STATIC (gst_rnnoise_debug_category);
#define GST_CAT_DEFAULT gst_rnnoise_debug_category

/* prototypes */

static void gst_rnnoise_set_property (GObject * object,
    guint property_id, const GValue * value, GParamSpec * pspec);
static void gst_rnnoise_get_property (GObject * object,
    guint property_id, GValue * value, GParamSpec * pspec);
static void gst_rnnoise_dispose (GObject * object);

static gboolean gst_rnnoise_setup (GstAudioFilter * filter,
    const GstAudioInfo * info);
static GstFlowReturn gst_rnnoise_transform (GstBaseTransform * trans,
    GstBuffer * inbuf, GstBuffer * outbuf);
static GstFlowReturn gst_rnnoise_transform_ip (GstBaseTransform *trans,
     GstBuffer *buffer);

enum
{
  PROP_0
};

/* pad templates */


static GstStaticPadTemplate gst_rnnoise_src_template =
GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    // S16LE, mono and 48000 is the only format supported by rnnoise
    GST_STATIC_CAPS (
    "audio/x-raw, "
    "format = (string) S16LE, "
    "channels = (int) 1, "
    "rate = (int) 48000"
    )
    );
    
static gboolean gst_transform_size(GstBaseTransform * trans,
    GstPadDirection direction,
    GstCaps * caps,
    gsize size,
    GstCaps * othercaps,
    gsize * othersize){
    // Calculate the size of the output data based on the size of the input data
    if (direction == GST_PAD_SRC) {
      // If we're going from source to sink, multiply the size by 2
      *othersize = size * 2;
    } else {
      // If we're going from sink to source, divide the size by 2
      *othersize = size * 2;
    }
    return TRUE;
  }

static GstStaticPadTemplate gst_rnnoise_sink_template =
GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    // S16LE, mono and 48000 is the only format supported by rnnoise
    GST_STATIC_CAPS (
    "audio/x-raw, "
    "format = (string) S16LE, "
    "channels = (int) 1, "
    "rate = (int) 48000"
    )
    );

/* class initialization */

G_DEFINE_TYPE_WITH_CODE (GstRnnoise, gst_rnnoise, GST_TYPE_AUDIO_FILTER,
    GST_DEBUG_CATEGORY_INIT (gst_rnnoise_debug_category, "rnnoise", 0,
        "debug category for rnnoise element"));

static void
gst_rnnoise_class_init (GstRnnoiseClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstBaseTransformClass *base_transform_class =
      GST_BASE_TRANSFORM_CLASS (klass);
  GstAudioFilterClass *audio_filter_class = GST_AUDIO_FILTER_CLASS (klass);

  /* Setting up pads and setting metadata should be moved to
     base_class_init if you intend to subclass this class. */
  gst_element_class_add_static_pad_template (GST_ELEMENT_CLASS (klass),
      &gst_rnnoise_src_template);
  gst_element_class_add_static_pad_template (GST_ELEMENT_CLASS (klass),
      &gst_rnnoise_sink_template);

  gst_element_class_set_static_metadata (GST_ELEMENT_CLASS (klass),
      "RNNoise noise suppression", "Audio Filter", "Noise suppression using RNNoise",
      "sivaram <sivaram@cradlewise.com>");

  gobject_class->set_property = gst_rnnoise_set_property;
  gobject_class->get_property = gst_rnnoise_get_property;
  gobject_class->dispose = gst_rnnoise_dispose;
  audio_filter_class->setup = GST_DEBUG_FUNCPTR (gst_rnnoise_setup);
  base_transform_class->transform_size = GST_DEBUG_FUNCPTR (gst_transform_size);
  base_transform_class->transform = GST_DEBUG_FUNCPTR (gst_rnnoise_transform);
  //base_transform_class->transform_ip = GST_DEBUG_FUNCPTR (gst_rnnoise_transform_ip);

}

static void
gst_rnnoise_init (GstRnnoise * this)
{
  this->st = rnnoise_create(NULL);
  this->adapter = gst_adapter_new();
  g_mutex_init (&this->mutex);
}

void
gst_rnnoise_set_property (GObject * object, guint property_id,
    const GValue * value, GParamSpec * pspec)
{
  GstRnnoise *rnnoise = GST_RNNOISE (object);

  GST_DEBUG_OBJECT (rnnoise, "set_property");

  switch (property_id) {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

void
gst_rnnoise_get_property (GObject * object, guint property_id,
    GValue * value, GParamSpec * pspec)
{
  GstRnnoise *this = GST_RNNOISE (object);

  GST_DEBUG_OBJECT (this, "get_property");

  switch (property_id) {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

void
gst_rnnoise_dispose (GObject * object)
{
  GstRnnoise *this = GST_RNNOISE (object);

  GST_WARNING_OBJECT (this, "dispose");

  if (this->st) {
    rnnoise_destroy(this->st);
    this->st = NULL;
    g_mutex_clear (&this->mutex);
  }

  if (this->adapter) {
    g_object_unref(this->adapter);
    this->adapter = NULL;
  }

  G_OBJECT_CLASS (gst_rnnoise_parent_class)->dispose (object);
}


static gboolean
gst_rnnoise_setup (GstAudioFilter * filter, const GstAudioInfo * info)
{
  GstRnnoise *this = GST_RNNOISE (filter);

  GST_DEBUG_OBJECT (this, "setup");

  return TRUE;
}

static GstFlowReturn 
gst_rnnoise_transform_ip(GstBaseTransform *trans, GstBuffer *buffer) {
  GstRnnoise *this = GST_RNNOISE (trans);

  // push the data into the adapter
  gst_adapter_push(this->adapter, gst_buffer_copy(buffer));
  printf("adapter size: %ld\n", gst_adapter_available(this->adapter));
  guint CHUNK_SIZE = 480;

  // While there's enough data in the adapter to fill a chunk, process it
  while (gst_adapter_available(this->adapter) >= CHUNK_SIZE * sizeof(int16_t)) {
    // Pull a chunk from the adapter
    GstBuffer *chunk_buffer = gst_adapter_take_buffer(this->adapter, CHUNK_SIZE * sizeof(int16_t));
    printf("adapter size: %ld\n", gst_adapter_available(this->adapter));
    printf("chunk_buffer size: %ld\n", gst_buffer_get_size(chunk_buffer));
    // Map the chunk buffer for efficient access
    GstMapInfo chunk_map_info;
    if (!gst_buffer_map(chunk_buffer, &chunk_map_info, GST_MAP_READWRITE)) {
      GST_ERROR("Failed to map chunk buffer");
      return GST_FLOW_ERROR;
    }
    
    int16_t *temp = (int16_t *) chunk_map_info.data;
    float x[CHUNK_SIZE];
    for (int i = 0; i < CHUNK_SIZE; i++) {
      x[i] = temp[i];
    }

    rnnoise_process_frame(this->st, x, x);

    for (int i = 0; i < CHUNK_SIZE; i++) {
      temp[i] = x[i];
    }
    
    // Unmap the chunk buffer
    gst_buffer_unmap(chunk_buffer, &chunk_map_info);

    // Free the chunk buffer
    gst_buffer_unref(chunk_buffer);

  }
  return GST_FLOW_OK;
}

static GstFlowReturn
gst_rnnoise_transform (GstBaseTransform * trans, GstBuffer * inbuf, GstBuffer * outbuf)
{
  GstRnnoise *this = GST_RNNOISE (trans);

  // push the data into the adapter
  gst_adapter_push(this->adapter, gst_buffer_copy(inbuf));
  
  guint CHUNK_SIZE = 480;

  // get number of chunks available in the adapter
  guint num_chunks = gst_adapter_available(this->adapter) / (CHUNK_SIZE * sizeof(int16_t));

  // resize the output buffer to fit the data
  gst_buffer_resize(outbuf, 0, num_chunks * CHUNK_SIZE * sizeof(int16_t));

  guint position = 0;

  // While there's enough data in the adapter to fill a chunk, process it
  while (gst_adapter_available(this->adapter) >= CHUNK_SIZE * sizeof(int16_t)) {
    // Pull a chunk from the adapter
    GstBuffer *chunk_buffer = gst_adapter_take_buffer(this->adapter, CHUNK_SIZE * sizeof(int16_t));
    // Map the chunk buffer for efficient access
    GstMapInfo chunk_map_info;
    if (!gst_buffer_map(chunk_buffer, &chunk_map_info, GST_MAP_READWRITE)) {
      GST_ERROR("Failed to map chunk buffer");
      return GST_FLOW_ERROR;
    }
    
    int16_t *temp = (int16_t *) chunk_map_info.data;
    float x[CHUNK_SIZE];
    for (int i = 0; i < CHUNK_SIZE; i++) {
      x[i] = temp[i];
    }

    rnnoise_process_frame(this->st, x, x);

    for (int i = 0; i < CHUNK_SIZE; i++) {
      temp[i] = x[i];
    }

    // push the data to sink
    gst_buffer_fill(outbuf, position, chunk_map_info.data, CHUNK_SIZE*sizeof(int16_t));
    
    // Unmap the chunk buffer
    gst_buffer_unmap(chunk_buffer, &chunk_map_info);

    // Free the chunk buffer
    gst_buffer_unref(chunk_buffer);

    // Increment the position in the output buffer
    position += CHUNK_SIZE * sizeof(int16_t);
  }

  return GST_FLOW_OK;
}

static gboolean
plugin_init (GstPlugin * plugin)
{
  return gst_element_register (plugin, "rnnoise", GST_RANK_NONE,
      GST_TYPE_RNNOISE);
}


#ifndef VERSION
#define VERSION "0.0.1"
#endif
#ifndef PACKAGE
#define PACKAGE "audio-processing"
#endif
#ifndef PACKAGE_NAME
#define PACKAGE_NAME "rnnoise"
#endif
#ifndef GST_PACKAGE_ORIGIN
#define GST_PACKAGE_ORIGIN "Cradlewise, Inc."
#endif

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    rnnoise,
    "Noise suppression using RNNoise",
    plugin_init, VERSION, "LGPL", PACKAGE_NAME, GST_PACKAGE_ORIGIN)
