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
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef _GST_RNNOISE_H_
#define _GST_RNNOISE_H_

#include <gst/audio/gstaudiofilter.h>
#include "rnnoise.h"

G_BEGIN_DECLS

#define GST_TYPE_RNNOISE   (gst_rnnoise_get_type())
#define GST_RNNOISE(obj)   (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_RNNOISE,GstRnnoise))
#define GST_RNNOISE_CLASS(klass)   (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_RNNOISE,GstRnnoiseClass))
#define GST_IS_RNNOISE(obj)   (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_RNNOISE))
#define GST_IS_RNNOISE_CLASS(obj)   (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_RNNOISE))

typedef struct _GstRnnoise GstRnnoise;
typedef struct _GstRnnoiseClass GstRnnoiseClass;

struct _GstRnnoise
{
  GstAudioFilter base_rnnoise;
  DenoiseState *st;
  GstAdapter *adapter;
  float vad_threshold;
  GMutex mutex;
  char *model_path;
  RNNModel *model;
};

struct _GstRnnoiseClass
{
  GstAudioFilterClass base_rnnoise_class;
};

GType gst_rnnoise_get_type (void);

G_END_DECLS

#endif
