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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "cogl.h"

#include <string.h>
#include <math.h>
#include <stdlib.h>
#include <glib/gi18n-lib.h>

#include "cogl-debug.h"
#include "cogl-internal.h"
#include "cogl-util.h"
#include "cogl-context-private.h"
#include "cogl-pipeline-private.h"
#include "cogl-pipeline-opengl-private.h"
#include "cogl-winsys-private.h"
#include "cogl-framebuffer-private.h"
#include "cogl-matrix-private.h"
#include "cogl-journal-private.h"
#include "cogl-bitmap-private.h"
#include "cogl-texture-private.h"
#include "cogl-texture-driver.h"
#include "cogl-attribute-private.h"
#include "cogl-framebuffer-private.h"
#include "cogl-renderer-private.h"
#include "cogl-config-private.h"
#include "cogl-private.h"

#ifndef GL_PACK_INVERT_MESA
#define GL_PACK_INVERT_MESA 0x8758
#endif

#ifdef COGL_GL_DEBUG
/* GL error to string conversion */
static const struct {
  GLuint error_code;
  const char *error_string;
} gl_errors[] = {
  { GL_NO_ERROR,          "No error" },
  { GL_INVALID_ENUM,      "Invalid enumeration value" },
  { GL_INVALID_VALUE,     "Invalid value" },
  { GL_INVALID_OPERATION, "Invalid operation" },
#ifdef HAVE_COGL_GL
  { GL_STACK_OVERFLOW,    "Stack overflow" },
  { GL_STACK_UNDERFLOW,   "Stack underflow" },
#endif
  { GL_OUT_OF_MEMORY,     "Out of memory" },

#ifdef GL_INVALID_FRAMEBUFFER_OPERATION_EXT
  { GL_INVALID_FRAMEBUFFER_OPERATION_EXT, "Invalid framebuffer operation" }
#endif
};

static const unsigned int n_gl_errors = G_N_ELEMENTS (gl_errors);

const char *
cogl_gl_error_to_string (GLenum error_code)
{
  int i;

  for (i = 0; i < n_gl_errors; i++)
    {
      if (gl_errors[i].error_code == error_code)
        return gl_errors[i].error_string;
    }

  return "Unknown GL error";
}
#endif /* COGL_GL_DEBUG */

CoglFuncPtr
cogl_get_proc_address (const char* name)
{
  _COGL_GET_CONTEXT (ctx, NULL);

  return _cogl_renderer_get_proc_address (ctx->display->renderer, name);
}

gboolean
_cogl_check_extension (const char *name, const gchar *ext)
{
  char *end;
  int name_len, n;

  if (name == NULL || ext == NULL)
    return FALSE;

  end = (char*)(ext + strlen(ext));

  name_len = strlen(name);

  while (ext < end)
    {
      n = strcspn(ext, " ");

      if ((name_len == n) && (!strncmp(name, ext, n)))
	return TRUE;
      ext += (n + 1);
    }

  return FALSE;
}

/* XXX: This has been deprecated as public API */
gboolean
cogl_check_extension (const char *name, const char *ext)
{
  return _cogl_check_extension (name, ext);
}

/* XXX: it's expected that we'll deprecated this with
 * cogl_framebuffer_clear at some point. */
void
cogl_clear (const CoglColor *color, unsigned long buffers)
{
  cogl_framebuffer_clear (cogl_get_draw_framebuffer (), buffers, color);
}

/* XXX: This API has been deprecated */
void
cogl_set_depth_test_enabled (gboolean setting)
{
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  if (ctx->legacy_depth_test_enabled == setting)
    return;

  ctx->legacy_depth_test_enabled = setting;
  if (ctx->legacy_depth_test_enabled)
    ctx->legacy_state_set++;
  else
    ctx->legacy_state_set--;
}

/* XXX: This API has been deprecated */
gboolean
cogl_get_depth_test_enabled (void)
{
  _COGL_GET_CONTEXT (ctx, FALSE);
  return ctx->legacy_depth_test_enabled;
}

void
cogl_set_backface_culling_enabled (gboolean setting)
{
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  if (ctx->legacy_backface_culling_enabled == setting)
    return;

  ctx->legacy_backface_culling_enabled = setting;

  if (ctx->legacy_backface_culling_enabled)
    ctx->legacy_state_set++;
  else
    ctx->legacy_state_set--;
}

gboolean
cogl_get_backface_culling_enabled (void)
{
  _COGL_GET_CONTEXT (ctx, FALSE);

  return ctx->legacy_backface_culling_enabled;
}

void
cogl_set_source_color (const CoglColor *color)
{
  CoglPipeline *pipeline;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  if (cogl_color_get_alpha_byte (color) == 0xff)
    {
      cogl_pipeline_set_color (ctx->opaque_color_pipeline, color);
      pipeline = ctx->opaque_color_pipeline;
    }
  else
    {
      CoglColor premultiplied = *color;
      cogl_color_premultiply (&premultiplied);
      cogl_pipeline_set_color (ctx->blended_color_pipeline, &premultiplied);
      pipeline = ctx->blended_color_pipeline;
    }

  cogl_set_source (pipeline);
}

void
cogl_set_viewport (int x,
                   int y,
                   int width,
                   int height)
{
  CoglFramebuffer *framebuffer;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  framebuffer = cogl_get_draw_framebuffer ();

  cogl_framebuffer_set_viewport (framebuffer,
                                 x,
                                 y,
                                 width,
                                 height);
}

/* XXX: This should be deprecated, and we should expose a way to also
 * specify an x and y viewport offset */
void
cogl_viewport (unsigned int width,
	       unsigned int height)
{
  cogl_set_viewport (0, 0, width, height);
}

CoglFeatureFlags
cogl_get_features (void)
{
  _COGL_GET_CONTEXT (ctx, 0);

  return ctx->feature_flags;
}

gboolean
cogl_features_available (CoglFeatureFlags features)
{
  _COGL_GET_CONTEXT (ctx, 0);

  return (ctx->feature_flags & features) == features;
}

gboolean
cogl_has_feature (CoglContext *ctx, CoglFeatureID feature)
{
  return COGL_FLAGS_GET (ctx->features, feature);
}

gboolean
cogl_has_features (CoglContext *ctx, ...)
{
  va_list args;
  CoglFeatureID feature;

  va_start (args, ctx);
  while ((feature = va_arg (args, CoglFeatureID)))
    if (!cogl_has_feature (ctx, feature))
      return FALSE;
  va_end (args);

  return TRUE;
}

void
cogl_foreach_feature (CoglContext *ctx,
                      CoglFeatureCallback callback,
                      void *user_data)
{
  int i;
  for (i = 0; i < _COGL_N_FEATURE_IDS; i++)
    if (COGL_FLAGS_GET (ctx->features, i))
      callback (i, user_data);
}

/* XXX: This function should either be replaced with one returning
 * integers, or removed/deprecated and make the
 * _cogl_framebuffer_get_viewport* functions public.
 */
void
cogl_get_viewport (float viewport[4])
{
  CoglFramebuffer *framebuffer;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  framebuffer = cogl_get_draw_framebuffer ();
  cogl_framebuffer_get_viewport4fv (framebuffer, viewport);
}

void
cogl_get_bitmasks (int *red,
                   int *green,
                   int *blue,
                   int *alpha)
{
  CoglFramebuffer *framebuffer;

  framebuffer = cogl_get_draw_framebuffer ();

  if (red)
    *red = cogl_framebuffer_get_red_bits (framebuffer);

  if (green)
    *green = cogl_framebuffer_get_green_bits (framebuffer);

  if (blue)
    *blue = cogl_framebuffer_get_blue_bits (framebuffer);

  if (alpha)
    *alpha = cogl_framebuffer_get_alpha_bits (framebuffer);
}

void
cogl_set_fog (const CoglColor *fog_color,
              CoglFogMode      mode,
              float            density,
              float            z_near,
              float            z_far)
{
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  if (ctx->legacy_fog_state.enabled == FALSE)
    ctx->legacy_state_set++;

  ctx->legacy_fog_state.enabled = TRUE;
  ctx->legacy_fog_state.color = *fog_color;
  ctx->legacy_fog_state.mode = mode;
  ctx->legacy_fog_state.density = density;
  ctx->legacy_fog_state.z_near = z_near;
  ctx->legacy_fog_state.z_far = z_far;
}

void
cogl_disable_fog (void)
{
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  if (ctx->legacy_fog_state.enabled == TRUE)
    ctx->legacy_state_set--;

  ctx->legacy_fog_state.enabled = FALSE;
}

void
cogl_flush (void)
{
  GList *l;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  for (l = ctx->framebuffers; l; l = l->next)
    _cogl_framebuffer_flush_journal (l->data);
}

void
_cogl_read_pixels_with_rowstride (int x,
                                  int y,
                                  int width,
                                  int height,
                                  CoglReadPixelsFlags source,
                                  CoglPixelFormat format,
                                  guint8 *pixels,
                                  int rowstride)
{
  CoglFramebuffer *framebuffer = _cogl_get_read_framebuffer ();
  int              framebuffer_height;
  int              bpp;
  CoglBitmap      *bmp;
  GLenum           gl_intformat;
  GLenum           gl_format;
  GLenum           gl_type;
  CoglPixelFormat  bmp_format;
  gboolean         pack_invert_set;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  _COGL_RETURN_IF_FAIL (source == COGL_READ_PIXELS_COLOR_BUFFER);

  if (width == 1 && height == 1 && !framebuffer->clear_clip_dirty)
    {
      /* If everything drawn so far for this frame is still in the
       * Journal then if all of the rectangles only have a flat
       * opaque color we have a fast-path for reading a single pixel
       * that avoids the relatively high cost of flushing primitives
       * to be drawn on the GPU (considering how simple the geometry
       * is in this case) and then blocking on the long GPU pipelines
       * for the result.
       */
      if (_cogl_framebuffer_try_fast_read_pixel (framebuffer,
                                                 x, y, source, format,
                                                 pixels))
        return;
    }

  /* make sure any batched primitives get emitted to the GL driver
   * before issuing our read pixels...
   *
   * XXX: Note we currently use cogl_flush to ensure *all* journals
   * are flushed here and not _cogl_journal_flush because we don't
   * track the dependencies between framebuffers so we don't know if
   * the current framebuffer depends on the contents of other
   * framebuffers which could also have associated journal entries.
   */
  cogl_flush ();

  _cogl_framebuffer_flush_state (cogl_get_draw_framebuffer (),
                                 framebuffer,
                                 COGL_FRAMEBUFFER_STATE_BIND);

  framebuffer_height = cogl_framebuffer_get_height (framebuffer);

  /* The y co-ordinate should be given in OpenGL's coordinate system
   * so 0 is the bottom row
   *
   * NB: all offscreen rendering is done upside down so no conversion
   * is necissary in this case.
   */
  if (!cogl_is_offscreen (framebuffer))
    y = framebuffer_height - y - height;

  /* Initialise the CoglBitmap */
  bpp = _cogl_get_format_bpp (format);
  bmp_format = format;

  if ((format & COGL_A_BIT))
    {
      /* We match the premultiplied state of the target buffer to the
       * premultiplied state of the framebuffer so that it will get
       * converted to the right format below */

      if ((framebuffer->format & COGL_PREMULT_BIT))
        bmp_format |= COGL_PREMULT_BIT;
      else
        bmp_format &= ~COGL_PREMULT_BIT;
    }

  bmp = _cogl_bitmap_new_from_data (pixels,
                                    bmp_format, width, height, rowstride,
                                    NULL, NULL);

  ctx->texture_driver->pixel_format_to_gl (format,
                                           &gl_intformat,
                                           &gl_format,
                                           &gl_type);

  /* NB: All offscreen rendering is done upside down so there is no need
   * to flip in this case... */
  if ((ctx->private_feature_flags & COGL_PRIVATE_FEATURE_MESA_PACK_INVERT) &&
      !cogl_is_offscreen (framebuffer))
    {
      GE (ctx, glPixelStorei (GL_PACK_INVERT_MESA, TRUE));
      pack_invert_set = TRUE;
    }
  else
    pack_invert_set = FALSE;

  /* Under GLES only GL_RGBA with GL_UNSIGNED_BYTE as well as an
     implementation specific format under
     GL_IMPLEMENTATION_COLOR_READ_FORMAT_OES and
     GL_IMPLEMENTATION_COLOR_READ_TYPE_OES is supported. We could try
     to be more clever and check if the requested type matches that
     but we would need some reliable functions to convert from GL
     types to Cogl types. For now, lets just always read in
     GL_RGBA/GL_UNSIGNED_BYTE and convert if necessary. We also need
     to use this intermediate buffer if the rowstride has padding
     because GLES does not support setting GL_ROW_LENGTH */
  if (ctx->driver != COGL_DRIVER_GL &&
      (gl_format != GL_RGBA || gl_type != GL_UNSIGNED_BYTE ||
       rowstride != 4 * width))
    {
      CoglBitmap *tmp_bmp, *dst_bmp;
      guint8 *tmp_data = g_malloc (width * height * 4);

      tmp_bmp = _cogl_bitmap_new_from_data (tmp_data,
                                            COGL_PIXEL_FORMAT_RGBA_8888 |
                                            (bmp_format & COGL_PREMULT_BIT),
                                            width, height, 4 * width,
                                            (CoglBitmapDestroyNotify) g_free,
                                            NULL);

      ctx->texture_driver->prep_gl_for_pixels_download (4 * width, 4);

      GE( ctx, glReadPixels (x, y, width, height,
                             GL_RGBA, GL_UNSIGNED_BYTE,
                             tmp_data) );

      /* CoglBitmap doesn't currently have a way to convert without
         allocating its own buffer so we have to copy the data
         again */
      if ((dst_bmp = _cogl_bitmap_convert_format_and_premult (tmp_bmp,
                                                              format)))
        {
          _cogl_bitmap_copy_subregion (dst_bmp,
                                       bmp,
                                       0, 0,
                                       0, 0,
                                       width, height);
          cogl_object_unref (dst_bmp);
        }
      else
        {
          /* FIXME: there's no way to report an error here so we'll
             just have to leave the data initialised */
        }

      cogl_object_unref (tmp_bmp);
    }
  else
    {
      ctx->texture_driver->prep_gl_for_pixels_download (rowstride, bpp);

      GE( ctx, glReadPixels (x, y, width, height, gl_format, gl_type, pixels) );

      /* Convert to the premult format specified by the caller
         in-place. This will do nothing if the premult status is already
         correct. */
      _cogl_bitmap_convert_premult_status (bmp, format);
    }

  /* Currently this function owns the pack_invert state and we don't want this
   * to interfere with other Cogl components so all other code can assume that
   * we leave the pack_invert state off. */
  if (pack_invert_set)
    GE (ctx, glPixelStorei (GL_PACK_INVERT_MESA, FALSE));

  /* NB: All offscreen rendering is done upside down so there is no need
   * to flip in this case... */
  if (!cogl_is_offscreen (framebuffer) && !pack_invert_set)
    {
      guint8 *temprow = g_alloca (rowstride * sizeof (guint8));

      /* vertically flip the buffer in-place */
      for (y = 0; y < height / 2; y++)
        {
          if (y != height - y - 1) /* skip center row */
            {
              memcpy (temprow,
                      pixels + y * rowstride, rowstride);
              memcpy (pixels + y * rowstride,
                      pixels + (height - y - 1) * rowstride, rowstride);
              memcpy (pixels + (height - y - 1) * rowstride,
                      temprow,
                      rowstride);
            }
        }
    }

  cogl_object_unref (bmp);
}

void
cogl_read_pixels (int x,
                  int y,
                  int width,
                  int height,
                  CoglReadPixelsFlags source,
                  CoglPixelFormat format,
                  guint8 *pixels)
{
  _cogl_read_pixels_with_rowstride (x, y, width, height,
                                    source, format, pixels,
                                    /* rowstride */
                                    _cogl_get_format_bpp (format) * width);
}

void
cogl_begin_gl (void)
{
  CoglPipeline *pipeline;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  if (ctx->in_begin_gl_block)
    {
      static gboolean shown = FALSE;
      if (!shown)
        g_warning ("You should not nest cogl_begin_gl/cogl_end_gl blocks");
      shown = TRUE;
      return;
    }
  ctx->in_begin_gl_block = TRUE;

  /* Flush all batched primitives */
  cogl_flush ();

  /* Flush framebuffer state, including clip state, modelview and
   * projection matrix state
   *
   * NB: _cogl_framebuffer_flush_state may disrupt various state (such
   * as the pipeline state) when flushing the clip stack, so should
   * always be done first when preparing to draw. */
  _cogl_framebuffer_flush_state (cogl_get_draw_framebuffer (),
                                 _cogl_get_read_framebuffer (),
                                 COGL_FRAMEBUFFER_STATE_ALL);

  /* Setup the state for the current pipeline */

  /* We considered flushing a specific, minimal pipeline here to try and
   * simplify the GL state, but decided to avoid special cases and second
   * guessing what would be actually helpful.
   *
   * A user should instead call cogl_set_source_color4ub() before
   * cogl_begin_gl() to simplify the state flushed.
   *
   * XXX: note defining n_tex_coord_attribs using
   * cogl_pipeline_get_n_layers is a hack, but the problem is that
   * n_tex_coord_attribs is usually defined when drawing a primitive
   * which isn't happening here.
   *
   * Maybe it would be more useful if this code did flush the
   * opaque_color_pipeline and then call into cogl-pipeline-opengl.c to then
   * restore all state for the material's backend back to default OpenGL
   * values.
   */
  pipeline = cogl_get_source ();
  _cogl_pipeline_flush_gl_state (pipeline,
                                 FALSE,
                                 cogl_pipeline_get_n_layers (pipeline));

  /* Disable any cached vertex arrays */
  _cogl_attribute_disable_cached_arrays ();
}

void
cogl_end_gl (void)
{
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  if (!ctx->in_begin_gl_block)
    {
      static gboolean shown = FALSE;
      if (!shown)
        g_warning ("cogl_end_gl is being called before cogl_begin_gl");
      shown = TRUE;
      return;
    }
  ctx->in_begin_gl_block = FALSE;
}

void
cogl_push_matrix (void)
{
  cogl_framebuffer_push_matrix (cogl_get_draw_framebuffer ());
}

void
cogl_pop_matrix (void)
{
  cogl_framebuffer_pop_matrix (cogl_get_draw_framebuffer ());
}

void
cogl_scale (float x, float y, float z)
{
  cogl_framebuffer_scale (cogl_get_draw_framebuffer (), x, y, z);
}

void
cogl_translate (float x, float y, float z)
{
  cogl_framebuffer_translate (cogl_get_draw_framebuffer (), x, y, z);
}

void
cogl_rotate (float angle, float x, float y, float z)
{
  cogl_framebuffer_rotate (cogl_get_draw_framebuffer (), angle, x, y, z);
}

void
cogl_transform (const CoglMatrix *matrix)
{
  cogl_framebuffer_transform (cogl_get_draw_framebuffer (), matrix);
}

void
cogl_perspective (float fov_y,
		  float aspect,
		  float z_near,
		  float z_far)
{
  cogl_framebuffer_perspective (cogl_get_draw_framebuffer (),
                                fov_y, aspect, z_near, z_far);
}

void
cogl_frustum (float        left,
	      float        right,
	      float        bottom,
	      float        top,
	      float        z_near,
	      float        z_far)
{
  cogl_framebuffer_frustum (cogl_get_draw_framebuffer (),
                            left, right, bottom, top, z_near, z_far);
}

void
cogl_ortho (float left,
	    float right,
	    float bottom,
	    float top,
	    float near,
	    float far)
{
  cogl_framebuffer_orthographic (cogl_get_draw_framebuffer (),
                                 left, top, right, bottom, near, far);
}

void
cogl_get_modelview_matrix (CoglMatrix *matrix)
{
  cogl_framebuffer_get_modelview_matrix (cogl_get_draw_framebuffer (), matrix);
}

void
cogl_set_modelview_matrix (CoglMatrix *matrix)
{
  cogl_framebuffer_set_modelview_matrix (cogl_get_draw_framebuffer (), matrix);
}

void
cogl_get_projection_matrix (CoglMatrix *matrix)
{
  cogl_framebuffer_get_projection_matrix (cogl_get_draw_framebuffer (), matrix);
}

void
cogl_set_projection_matrix (CoglMatrix *matrix)
{
  cogl_framebuffer_set_projection_matrix (cogl_get_draw_framebuffer (), matrix);
}

CoglClipState *
_cogl_get_clip_state (void)
{
  CoglFramebuffer *framebuffer;

  framebuffer = cogl_get_draw_framebuffer ();
  return _cogl_framebuffer_get_clip_state (framebuffer);
}

GQuark
_cogl_driver_error_quark (void)
{
  return g_quark_from_static_string ("cogl-driver-error-quark");
}

typedef struct _CoglSourceState
{
  CoglPipeline *pipeline;
  int push_count;
  /* If this is TRUE then the pipeline will be copied and the legacy
     state will be applied whenever the pipeline is used. This is
     necessary because some internal Cogl code expects to be able to
     push a temporary pipeline to put GL into a known state. For that
     to work it also needs to prevent applying the legacy state */
  gboolean enable_legacy;
} CoglSourceState;

static void
_push_source_real (CoglPipeline *pipeline, gboolean enable_legacy)
{
  CoglSourceState *top = g_slice_new (CoglSourceState);
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  top->pipeline = cogl_object_ref (pipeline);
  top->enable_legacy = enable_legacy;
  top->push_count = 1;

  ctx->source_stack = g_list_prepend (ctx->source_stack, top);
}

/* FIXME: This should take a context pointer for Cogl 2.0 Technically
 * we could make it so we can retrieve a context reference from the
 * pipeline, but this would not by symmetric with cogl_pop_source. */
void
cogl_push_source (void *material_or_pipeline)
{
  CoglPipeline *pipeline = COGL_PIPELINE (material_or_pipeline);

  _COGL_RETURN_IF_FAIL (cogl_is_pipeline (pipeline));

  _cogl_push_source (pipeline, TRUE);
}

/* This internal version of cogl_push_source is the same except it
   never applies the legacy state. Some parts of Cogl use this
   internally to set a temporary pipeline with a known state */
void
_cogl_push_source (CoglPipeline *pipeline, gboolean enable_legacy)
{
  CoglSourceState *top;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  _COGL_RETURN_IF_FAIL (cogl_is_pipeline (pipeline));

  if (ctx->source_stack)
    {
      top = ctx->source_stack->data;
      if (top->pipeline == pipeline && top->enable_legacy == enable_legacy)
        {
          top->push_count++;
          return;
        }
      else
        _push_source_real (pipeline, enable_legacy);
    }
  else
    _push_source_real (pipeline, enable_legacy);
}

/* FIXME: This needs to take a context pointer for Cogl 2.0 */
void
cogl_pop_source (void)
{
  CoglSourceState *top;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  _COGL_RETURN_IF_FAIL (ctx->source_stack);

  top = ctx->source_stack->data;
  top->push_count--;
  if (top->push_count == 0)
    {
      cogl_object_unref (top->pipeline);
      g_slice_free (CoglSourceState, top);
      ctx->source_stack = g_list_delete_link (ctx->source_stack,
                                              ctx->source_stack);
    }
}

/* FIXME: This needs to take a context pointer for Cogl 2.0 */
void *
cogl_get_source (void)
{
  CoglSourceState *top;

  _COGL_GET_CONTEXT (ctx, NULL);

  _COGL_RETURN_VAL_IF_FAIL (ctx->source_stack, NULL);

  top = ctx->source_stack->data;
  return top->pipeline;
}

gboolean
_cogl_get_enable_legacy_state (void)
{
  CoglSourceState *top;

  _COGL_GET_CONTEXT (ctx, FALSE);

  _COGL_RETURN_VAL_IF_FAIL (ctx->source_stack, FALSE);

  top = ctx->source_stack->data;
  return top->enable_legacy;
}

void
cogl_set_source (void *material_or_pipeline)
{
  CoglSourceState *top;
  CoglPipeline *pipeline = COGL_PIPELINE (material_or_pipeline);

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  _COGL_RETURN_IF_FAIL (cogl_is_pipeline (pipeline));
  _COGL_RETURN_IF_FAIL (ctx->source_stack);

  top = ctx->source_stack->data;
  if (top->pipeline == pipeline && top->enable_legacy)
    return;

  if (top->push_count == 1)
    {
      /* NB: top->pipeline may be only thing keeping pipeline
       * alive currently so ref pipeline first... */
      cogl_object_ref (pipeline);
      cogl_object_unref (top->pipeline);
      top->pipeline = pipeline;
      top->enable_legacy = TRUE;
    }
  else
    {
      top->push_count--;
      cogl_push_source (pipeline);
    }
}

void
cogl_set_source_texture (CoglTexture *texture)
{
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  _COGL_RETURN_IF_FAIL (texture != NULL);

  cogl_pipeline_set_layer_texture (ctx->texture_pipeline, 0, texture);
  cogl_set_source (ctx->texture_pipeline);
}

void
cogl_set_source_color4ub (guint8 red,
                          guint8 green,
                          guint8 blue,
                          guint8 alpha)
{
  CoglColor c = { 0, };

  cogl_color_init_from_4ub (&c, red, green, blue, alpha);
  cogl_set_source_color (&c);
}

void
cogl_set_source_color4f (float red,
                         float green,
                         float blue,
                         float alpha)
{
  CoglColor c = { 0, };

  cogl_color_init_from_4f (&c, red, green, blue, alpha);
  cogl_set_source_color (&c);
}

/* Scale from OpenGL normalized device coordinates (ranging from -1 to 1)
 * to Cogl window/framebuffer coordinates (ranging from 0 to buffer-size) with
 * (0,0) being top left. */
#define VIEWPORT_TRANSFORM_X(x, vp_origin_x, vp_width) \
    (  ( ((x) + 1.0) * ((vp_width) / 2.0) ) + (vp_origin_x)  )
/* Note: for Y we first flip all coordinates around the X axis while in
 * normalized device coodinates */
#define VIEWPORT_TRANSFORM_Y(y, vp_origin_y, vp_height) \
    (  ( ((-(y)) + 1.0) * ((vp_height) / 2.0) ) + (vp_origin_y)  )

/* Transform a homogeneous vertex position from model space to Cogl
 * window coordinates (with 0,0 being top left) */
void
_cogl_transform_point (const CoglMatrix *matrix_mv,
                       const CoglMatrix *matrix_p,
                       const float *viewport,
                       float *x,
                       float *y)
{
  float z = 0;
  float w = 1;

  /* Apply the modelview matrix transform */
  cogl_matrix_transform_point (matrix_mv, x, y, &z, &w);

  /* Apply the projection matrix transform */
  cogl_matrix_transform_point (matrix_p, x, y, &z, &w);

  /* Perform perspective division */
  *x /= w;
  *y /= w;

  /* Apply viewport transform */
  *x = VIEWPORT_TRANSFORM_X (*x, viewport[0], viewport[2]);
  *y = VIEWPORT_TRANSFORM_Y (*y, viewport[1], viewport[3]);
}

#undef VIEWPORT_TRANSFORM_X
#undef VIEWPORT_TRANSFORM_Y

GQuark
_cogl_error_quark (void)
{
  return g_quark_from_static_string ("cogl-error-quark");
}

void
_cogl_init (void)
{
  static gsize init_status = 0;

  if (g_once_init_enter (&init_status))
    {
      bindtextdomain (GETTEXT_PACKAGE, COGL_LOCALEDIR);
      bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");

      g_type_init ();

      _cogl_config_read ();
      _cogl_debug_check_environment ();
      g_once_init_leave (&init_status, 1);
    }
}
