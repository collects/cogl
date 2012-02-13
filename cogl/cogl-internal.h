/*
 * Cogl
 *
 * An object oriented GL/GLES Abstraction/Utility Layer
 *
 * Copyright (C) 2007,2008,2009,2010 Intel Corporation.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 *
 */

#ifndef __COGL_INTERNAL_H
#define __COGL_INTERNAL_H

#include "cogl.h"
#include "cogl-matrix-stack.h"
#include "cogl-bitmask.h"

#ifdef COGL_HAS_XLIB_SUPPORT
#include <X11/Xutil.h>
#endif

#ifdef COGL_GL_DEBUG

const char *
cogl_gl_error_to_string (GLenum error_code);

#define GE(ctx, x)                      G_STMT_START {  \
  GLenum __err;                                         \
  (ctx)->x;                                             \
  while ((__err = (ctx)->glGetError ()) != GL_NO_ERROR) \
    {                                                   \
      g_warning ("%s: GL error (%d): %s\n",             \
                 G_STRLOC,                              \
                 __err,                                 \
                 cogl_gl_error_to_string (__err));      \
    }                                   } G_STMT_END

#define GE_RET(ret, ctx, x)             G_STMT_START {  \
  GLenum __err;                                         \
  ret = (ctx)->x;                                       \
  while ((__err = (ctx)->glGetError ()) != GL_NO_ERROR) \
    {                                                   \
      g_warning ("%s: GL error (%d): %s\n",             \
                 G_STRLOC,                              \
                 __err,                                 \
                 cogl_gl_error_to_string (__err));      \
    }                                   } G_STMT_END

#else /* !COGL_GL_DEBUG */

#define GE(ctx, x) ((ctx)->x)
#define GE_RET(ret, ctx, x) (ret = ((ctx)->x))

#endif /* COGL_GL_DEBUG */

#define COGL_ENABLE_ALPHA_TEST        (1<<1)
#define COGL_ENABLE_VERTEX_ARRAY      (1<<2)
#define COGL_ENABLE_COLOR_ARRAY       (1<<3)

int
_cogl_get_format_bpp (CoglPixelFormat format);

void
_cogl_enable (unsigned long flags);

unsigned long
_cogl_get_enable (void);

void
_cogl_transform_point (const CoglMatrix *matrix_mv,
                       const CoglMatrix *matrix_p,
                       const float *viewport,
                       float *x,
                       float *y);

#define COGL_DRIVER_ERROR (_cogl_driver_error_quark ())

typedef enum { /*< prefix=COGL_DRIVER_ERROR >*/
  COGL_DRIVER_ERROR_UNKNOWN_VERSION,
  COGL_DRIVER_ERROR_INVALID_VERSION,
  COGL_DRIVER_ERROR_NO_SUITABLE_DRIVER_FOUND,
  COGL_DRIVER_ERROR_FAILED_TO_LOAD_LIBRARY
} CoglDriverError;

typedef enum
{
  COGL_DRIVER_GL,
  COGL_DRIVER_GLES1,
  COGL_DRIVER_GLES2
} CoglDriver;

typedef enum
{
  COGL_PRIVATE_FEATURE_TEXTURE_2D_FROM_EGL_IMAGE = 1L<<0,
  COGL_PRIVATE_FEATURE_MESA_PACK_INVERT = 1L<<1,
  COGL_PRIVATE_FEATURE_STENCIL_BUFFER = 1L<<2,
  COGL_PRIVATE_FEATURE_OFFSCREEN_BLIT = 1L<<3,
  COGL_PRIVATE_FEATURE_FOUR_CLIP_PLANES = 1L<<4,
  COGL_PRIVATE_FEATURE_PBOS = 1L<<5,
  COGL_PRIVATE_FEATURE_VBOS = 1L<<6
} CoglPrivateFeatureFlags;

/* Sometimes when evaluating pipelines, either during comparisons or
 * if calculating a hash value we need to tweak the evaluation
 * semantics */
typedef enum _CoglPipelineEvalFlags
{
  COGL_PIPELINE_EVAL_FLAG_NONE = 0
} CoglPipelineEvalFlags;

gboolean
_cogl_check_extension (const char *name, const char *ext);

GQuark
_cogl_driver_error_quark (void);

#endif /* __COGL_INTERNAL_H */
