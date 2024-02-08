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
static void rnnoise_model_init (GstRnnoise * this, FILE *fp);

enum
{
  PROP_0,
  PROP_VAD_THRESHOLD,
  PROP_MODEL_PATH
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
      *othersize = size * 4;
    } else {
      // If we're going from sink to source, divide the size by 2
      *othersize = size * 4;
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

  g_object_class_install_property (gobject_class, PROP_VAD_THRESHOLD,
      g_param_spec_float ("vad-thres", "VAD Threshold",
          "Value for VAD threshold", 0, 1, 0,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_MODEL_PATH,
      g_param_spec_string ("model-path", "Model Path",
          "Path to RNNoise model", NULL,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  gobject_class->dispose = gst_rnnoise_dispose;
  audio_filter_class->setup = GST_DEBUG_FUNCPTR (gst_rnnoise_setup);
  base_transform_class->transform_size = GST_DEBUG_FUNCPTR (gst_transform_size);
  base_transform_class->transform = GST_DEBUG_FUNCPTR (gst_rnnoise_transform);
  //base_transform_class->transform_ip = GST_DEBUG_FUNCPTR (gst_rnnoise_transform_ip);

}

static void
gst_rnnoise_init (GstRnnoise * this)
{
  FILE *fp = fopen(this->model_path, "rb");
  if (fp == NULL && this->model_path != NULL) {
    GST_ERROR("Invalid model path");
  }
  rnnoise_model_init(this, fp);
  this->adapter = gst_adapter_new();
  g_mutex_init (&this->mutex);
  // close file pointer
  if (fp != NULL) {
    fclose(fp);
  }
}

static void
rnnoise_model_init (GstRnnoise * this, FILE *fp)
{
  if (fp == NULL){
    GST_WARNING_OBJECT(this, "No model file provided");
  }
  // get mutex
  g_mutex_lock (&this->mutex);
  // clear the model if it exists
  if (fp == NULL && this->st == NULL)
  {
    this->st = rnnoise_create(NULL);
  }
  else if (fp != NULL && this->st == NULL)
  {
    GST_INFO("Initializing RNNoise model first Time %s", this->model_path);
    this->model = rnnoise_model_from_file(fp);
    this->st = rnnoise_create(this->model);
  }
  else if (fp != NULL && this->st != NULL)
  {
    GST_INFO("Initializing RNNoise model %s", this->model_path);
    rnnoise_destroy(this->st);
    if (this->model != NULL) 
      rnnoise_model_free(this->model);
    this->model = rnnoise_model_from_file(fp);
    if (this->model == NULL) {
      GST_WARNING("Model is NULL");
    }
    this->st = rnnoise_create(this->model);
  }

  g_mutex_unlock (&this->mutex);
}

void
gst_rnnoise_set_property (GObject * object, guint property_id,
    const GValue * value, GParamSpec * pspec)
{
  GstRnnoise *this = GST_RNNOISE (object);

  GST_DEBUG_OBJECT (this, "set_property");

  switch (property_id) {
    case PROP_VAD_THRESHOLD:
      this->vad_threshold = g_value_get_float (value);
      break;
    case PROP_MODEL_PATH:
      this->model_path = g_value_dup_string(value);
      GST_WARNING("Model file name is %s", this->model_path);
      FILE *fp = fopen(this->model_path, "rb");
      if (fp == NULL) {
        GST_WARNING_OBJECT(this, "Invalid model path %s", this->model_path);
        GST_WARNING_OBJECT(this, "Using default model");
      }
      rnnoise_model_init(this, fp);
      if (fp != NULL) {
        fclose(fp);
      }
      break;
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
    case PROP_VAD_THRESHOLD:
      g_value_set_float (value, this->vad_threshold);
      break;
    case PROP_MODEL_PATH:
      g_value_set_string(value, this->model_path);
      break;
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

  if (this->model){
    rnnoise_model_free(this->model);
    this->model = NULL;
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
  
  guint CHUNK_SIZE = FRAME_SIZE;

  // get number of chunks available in the adapter this should be floor
  guint num_chunks = gst_adapter_available(this->adapter) / (CHUNK_SIZE * sizeof(int16_t));

  // resize the output buffer to fit the data
  gst_buffer_resize(outbuf, 0, num_chunks * CHUNK_SIZE * sizeof(int16_t));

  // check if the adapter has enough data to process
  if (num_chunks <= 0) {
    return GST_FLOW_OK;
  }

  float vad_prob = 0.0;

  // While there's enough data in the adapter to fill a chunk, process it
  while (gst_adapter_available(this->adapter) >= CHUNK_SIZE * sizeof(int16_t) * num_chunks) {
    // Pull a chunk from the adapter
    GstBuffer *chunk_buffer = gst_adapter_take_buffer(this->adapter, CHUNK_SIZE * sizeof(int16_t) * num_chunks);
    // Map the chunk buffer for efficient access
    GstMapInfo chunk_map_info;
    if (!gst_buffer_map(chunk_buffer, &chunk_map_info, GST_MAP_READWRITE)) {
      GST_ERROR("Failed to map chunk buffer");
      return GST_FLOW_ERROR;
    }
    
    int16_t *temp = (int16_t *) chunk_map_info.data;
    float x[CHUNK_SIZE];


    for (int i = 0; i < num_chunks; i++){

      for (int j = 0; j < CHUNK_SIZE; j++) {
        x[j] = temp[i*CHUNK_SIZE + j];
      }

      vad_prob = rnnoise_process_frame(this->st, x, x);

      if (vad_prob < this->vad_threshold) {
        for (int j = 0; j < CHUNK_SIZE; j++) {
          x[j] = 0;
        }
      }

      for (int j = 0; j < CHUNK_SIZE; j++) {
        temp[i*CHUNK_SIZE + j] = x[j];
      }
    }

    //gst_buffer_fill(outbuf, position, chunk_map_info.data, CHUNK_SIZE*sizeof(int16_t));
    gst_buffer_fill(outbuf, 0, chunk_map_info.data, CHUNK_SIZE * sizeof(int16_t) * num_chunks);
    
    // Unmap the chunk buffer
    gst_buffer_unmap(chunk_buffer, &chunk_map_info);

    // copy chunk_buffer to outbuf with all metadata
    gst_buffer_copy_into(outbuf, chunk_buffer, \
    GST_BUFFER_COPY_TIMESTAMPS|GST_BUFFER_COPY_FLAGS|GST_BUFFER_COPY_META, 0, -1);

    // Free the chunk buffer
    gst_buffer_unref(chunk_buffer);
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
