/*
 * Cogl
 *
 * An object oriented GL/GLES Abstraction/Utility Layer
 *
 * Copyright (C) 2009,2010,2011 Intel Corporation.
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
 *
 * Authors:
 *  Neil Roberts   <neil@linux.intel.com>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "cogl.h"
#include "cogl-internal.h"
#include "cogl-util.h"
#include "cogl-texture-private.h"
#include "cogl-sub-texture-private.h"
#include "cogl-context-private.h"
#include "cogl-object.h"
#include "cogl-texture-driver.h"
#include "cogl-texture-rectangle-private.h"

#include <string.h>
#include <math.h>

static void _cogl_sub_texture_free (CoglSubTexture *sub_tex);

COGL_TEXTURE_DEFINE (SubTexture, sub_texture);

static const CoglTextureVtable cogl_sub_texture_vtable;

static void
_cogl_sub_texture_unmap_quad (CoglSubTexture *sub_tex,
                              float *coords)
{
  /* NB: coords[] come in as non-normalized if sub_tex->full_texture
   * is a CoglTextureRectangle otherwhise they are normalized. The
   * coordinates we write out though must always be normalized.
   *
   * NB: sub_tex->sub_x/y/width/height are in non-normalized
   * coordinates.
   */
  if (cogl_is_texture_rectangle (sub_tex->full_texture))
    {
      coords[0] = (coords[0] - sub_tex->sub_x) / sub_tex->sub_width;
      coords[1] = (coords[1] - sub_tex->sub_y) / sub_tex->sub_height;
      coords[2] = (coords[2] - sub_tex->sub_x) / sub_tex->sub_width;
      coords[3] = (coords[3] - sub_tex->sub_y) / sub_tex->sub_height;
    }
  else
    {
      float width = cogl_texture_get_width (sub_tex->full_texture);
      float height = cogl_texture_get_height (sub_tex->full_texture);
      coords[0] = (coords[0] * width - sub_tex->sub_x) / sub_tex->sub_width;
      coords[1] = (coords[1] * height - sub_tex->sub_y) / sub_tex->sub_height;
      coords[2] = (coords[2] * width - sub_tex->sub_x) / sub_tex->sub_width;
      coords[3] = (coords[3] * height - sub_tex->sub_y) / sub_tex->sub_height;
    }
}

static void
_cogl_sub_texture_map_quad (CoglSubTexture *sub_tex,
                            float *coords)
{
  /* NB: coords[] always come in as normalized coordinates but may go
   * out as non-normalized if sub_tex->full_texture is a
   * CoglTextureRectangle.
   *
   * NB: sub_tex->sub_x/y/width/height are in non-normalized
   * coordinates.
   */

  if (cogl_is_texture_rectangle (sub_tex->full_texture))
    {
      coords[0] = coords[0] * sub_tex->sub_width + sub_tex->sub_x;
      coords[1] = coords[1] * sub_tex->sub_height + sub_tex->sub_y;
      coords[2] = coords[2] * sub_tex->sub_width + sub_tex->sub_x;
      coords[3] = coords[3] * sub_tex->sub_height + sub_tex->sub_y;
    }
  else
    {
      float width = cogl_texture_get_width (sub_tex->full_texture);
      float height = cogl_texture_get_height (sub_tex->full_texture);
      coords[0] = (coords[0] * sub_tex->sub_width + sub_tex->sub_x) / width;
      coords[1] = (coords[1] * sub_tex->sub_height + sub_tex->sub_y) / height;
      coords[2] = (coords[2] * sub_tex->sub_width + sub_tex->sub_x) / width;
      coords[3] = (coords[3] * sub_tex->sub_height + sub_tex->sub_y) / height;
    }
}

typedef struct _CoglSubTextureForeachData
{
  CoglSubTexture *sub_tex;
  CoglMetaTextureCallback callback;
  void *user_data;
} CoglSubTextureForeachData;

static void
unmap_coords_cb (CoglTexture *slice_texture,
                 const float *slice_texture_coords,
                 const float *meta_coords,
                 void *user_data)
{
  CoglSubTextureForeachData *data = user_data;
  float unmapped_coords[4];

  memcpy (unmapped_coords, meta_coords, sizeof (unmapped_coords));

  _cogl_sub_texture_unmap_quad (data->sub_tex, unmapped_coords);

  data->callback (slice_texture,
                  slice_texture_coords,
                  unmapped_coords,
                  data->user_data);
}

static void
_cogl_sub_texture_foreach_sub_texture_in_region (
                                       CoglTexture *tex,
                                       float virtual_tx_1,
                                       float virtual_ty_1,
                                       float virtual_tx_2,
                                       float virtual_ty_2,
                                       CoglMetaTextureCallback callback,
                                       void *user_data)
{
  CoglSubTexture *sub_tex = COGL_SUB_TEXTURE (tex);
  CoglTexture *full_texture = sub_tex->full_texture;
  float mapped_coords[4] =
    { virtual_tx_1, virtual_ty_1, virtual_tx_2, virtual_ty_2};
  float virtual_coords[4] =
    { virtual_tx_1, virtual_ty_1, virtual_tx_2, virtual_ty_2};

  /* map the virtual coordinates to ->full_texture coordinates */
  _cogl_sub_texture_map_quad (sub_tex, mapped_coords);

  /* TODO: Add something like cogl_is_low_level_texture() */
  if (cogl_is_texture_2d (full_texture) ||
      cogl_is_texture_rectangle (full_texture))
    {
      callback (sub_tex->full_texture,
                mapped_coords,
                virtual_coords,
                user_data);
    }
  else
    {
      CoglSubTextureForeachData data;

      data.sub_tex = sub_tex;
      data.callback = callback;
      data.user_data = user_data;

      cogl_meta_texture_foreach_in_region (COGL_META_TEXTURE (full_texture),
                                           mapped_coords[0],
                                           mapped_coords[1],
                                           mapped_coords[2],
                                           mapped_coords[3],
                                           COGL_PIPELINE_WRAP_MODE_REPEAT,
                                           COGL_PIPELINE_WRAP_MODE_REPEAT,
                                           unmap_coords_cb,
                                           &data);
    }
}

static void
_cogl_sub_texture_set_wrap_mode_parameters (CoglTexture *tex,
                                            GLenum wrap_mode_s,
                                            GLenum wrap_mode_t,
                                            GLenum wrap_mode_p)
{
  CoglSubTexture *sub_tex = COGL_SUB_TEXTURE (tex);

  _cogl_texture_set_wrap_mode_parameters (sub_tex->full_texture,
                                          wrap_mode_s,
                                          wrap_mode_t,
                                          wrap_mode_p);
}

static void
_cogl_sub_texture_free (CoglSubTexture *sub_tex)
{
  cogl_object_unref (sub_tex->next_texture);
  cogl_object_unref (sub_tex->full_texture);

  /* Chain up */
  _cogl_texture_free (COGL_TEXTURE (sub_tex));
}

CoglSubTexture *
cogl_sub_texture_new (CoglContext *ctx,
                      CoglTexture *next_texture,
                      int sub_x, int sub_y,
                      int sub_width, int sub_height)
{
  CoglTexture    *full_texture;
  CoglSubTexture *sub_tex;
  CoglTexture    *tex;
  unsigned int    next_width, next_height;

  next_width = cogl_texture_get_width (next_texture);
  next_height = cogl_texture_get_height (next_texture);

  /* The region must specify a non-zero subset of the full texture */
  _COGL_RETURN_VAL_IF_FAIL (sub_x >= 0 && sub_y >= 0, NULL);
  _COGL_RETURN_VAL_IF_FAIL (sub_width > 0 && sub_height > 0, NULL);
  _COGL_RETURN_VAL_IF_FAIL (sub_x + sub_width <= next_width, NULL);
  _COGL_RETURN_VAL_IF_FAIL (sub_y + sub_height <= next_height, NULL);

  sub_tex = g_new (CoglSubTexture, 1);

  tex = COGL_TEXTURE (sub_tex);

  _cogl_texture_init (tex, &cogl_sub_texture_vtable);

  /* If the next texture is also a sub texture we can avoid one level
     of indirection by referencing the full texture of that texture
     instead. */
  if (cogl_is_sub_texture (next_texture))
    {
      CoglSubTexture *other_sub_tex =
        _cogl_sub_texture_pointer_from_handle (next_texture);
      full_texture = other_sub_tex->full_texture;
      sub_x += other_sub_tex->sub_x;
      sub_y += other_sub_tex->sub_y;
    }
  else
    full_texture = next_texture;

  sub_tex->next_texture = cogl_object_ref (next_texture);
  sub_tex->full_texture = cogl_object_ref (full_texture);

  sub_tex->sub_x = sub_x;
  sub_tex->sub_y = sub_y;
  sub_tex->sub_width = sub_width;
  sub_tex->sub_height = sub_height;

  return _cogl_sub_texture_handle_new (sub_tex);
}

CoglTexture *
cogl_sub_texture_get_parent (CoglSubTexture *sub_texture)
{
  return sub_texture->next_texture;
}

static int
_cogl_sub_texture_get_max_waste (CoglTexture *tex)
{
  CoglSubTexture *sub_tex = COGL_SUB_TEXTURE (tex);

  return cogl_texture_get_max_waste (sub_tex->full_texture);
}

static gboolean
_cogl_sub_texture_is_sliced (CoglTexture *tex)
{
  CoglSubTexture *sub_tex = COGL_SUB_TEXTURE (tex);

  return cogl_texture_is_sliced (sub_tex->full_texture);
}

static gboolean
_cogl_sub_texture_can_hardware_repeat (CoglTexture *tex)
{
  CoglSubTexture *sub_tex = COGL_SUB_TEXTURE (tex);

  /* We can hardware repeat if the subtexture actually represents all of the
     of the full texture */
  return (sub_tex->sub_width ==
          cogl_texture_get_width (sub_tex->full_texture) &&
          sub_tex->sub_height ==
          cogl_texture_get_height (sub_tex->full_texture) &&
          _cogl_texture_can_hardware_repeat (sub_tex->full_texture));
}

static void
_cogl_sub_texture_transform_coords_to_gl (CoglTexture *tex,
                                          float *s,
                                          float *t)
{
  CoglSubTexture *sub_tex = COGL_SUB_TEXTURE (tex);

  /* This won't work if the sub texture is not the size of the full
     texture and the coordinates are outside the range [0,1] */
  *s = ((*s * sub_tex->sub_width + sub_tex->sub_x) /
        cogl_texture_get_width (sub_tex->full_texture));
  *t = ((*t * sub_tex->sub_height + sub_tex->sub_y) /
        cogl_texture_get_height (sub_tex->full_texture));

  _cogl_texture_transform_coords_to_gl (sub_tex->full_texture, s, t);
}

static CoglTransformResult
_cogl_sub_texture_transform_quad_coords_to_gl (CoglTexture *tex,
                                               float *coords)
{
  CoglSubTexture *sub_tex = COGL_SUB_TEXTURE (tex);
  int i;

  /* We can't support repeating with this method. In this case
     cogl-primitives will resort to manual repeating */
  for (i = 0; i < 4; i++)
    if (coords[i] < 0.0f || coords[i] > 1.0f)
      return COGL_TRANSFORM_SOFTWARE_REPEAT;

  _cogl_sub_texture_map_quad (sub_tex, coords);

  return _cogl_texture_transform_quad_coords_to_gl (sub_tex->full_texture,
                                                    coords);
}

static gboolean
_cogl_sub_texture_get_gl_texture (CoglTexture *tex,
                                  GLuint *out_gl_handle,
                                  GLenum *out_gl_target)
{
  CoglSubTexture *sub_tex = COGL_SUB_TEXTURE (tex);

  return cogl_texture_get_gl_texture (sub_tex->full_texture,
                                      out_gl_handle,
                                      out_gl_target);
}

static void
_cogl_sub_texture_set_filters (CoglTexture *tex,
                               GLenum min_filter,
                               GLenum mag_filter)
{
  CoglSubTexture *sub_tex = COGL_SUB_TEXTURE (tex);

  _cogl_texture_set_filters (sub_tex->full_texture, min_filter, mag_filter);
}

static void
_cogl_sub_texture_pre_paint (CoglTexture *tex,
                             CoglTexturePrePaintFlags flags)
{
  CoglSubTexture *sub_tex = COGL_SUB_TEXTURE (tex);

  _cogl_texture_pre_paint (sub_tex->full_texture, flags);
}

static void
_cogl_sub_texture_ensure_non_quad_rendering (CoglTexture *tex)
{
}

static gboolean
_cogl_sub_texture_set_region (CoglTexture    *tex,
                              int             src_x,
                              int             src_y,
                              int             dst_x,
                              int             dst_y,
                              unsigned int    dst_width,
                              unsigned int    dst_height,
                              CoglBitmap     *bmp)
{
  CoglSubTexture  *sub_tex = COGL_SUB_TEXTURE (tex);

  return cogl_texture_set_region_from_bitmap (sub_tex->full_texture,
                                              src_x, src_y,
                                              dst_x + sub_tex->sub_x,
                                              dst_y + sub_tex->sub_y,
                                              dst_width, dst_height,
                                              bmp);
}

static CoglPixelFormat
_cogl_sub_texture_get_format (CoglTexture *tex)
{
  CoglSubTexture *sub_tex = COGL_SUB_TEXTURE (tex);

  return cogl_texture_get_format (sub_tex->full_texture);
}

static GLenum
_cogl_sub_texture_get_gl_format (CoglTexture *tex)
{
  CoglSubTexture *sub_tex = COGL_SUB_TEXTURE (tex);

  return _cogl_texture_get_gl_format (sub_tex->full_texture);
}

static int
_cogl_sub_texture_get_width (CoglTexture *tex)
{
  CoglSubTexture *sub_tex = COGL_SUB_TEXTURE (tex);

  return sub_tex->sub_width;
}

static int
_cogl_sub_texture_get_height (CoglTexture *tex)
{
  CoglSubTexture *sub_tex = COGL_SUB_TEXTURE (tex);

  return sub_tex->sub_height;
}

static const CoglTextureVtable
cogl_sub_texture_vtable =
  {
    _cogl_sub_texture_set_region,
    NULL, /* get_data */
    _cogl_sub_texture_foreach_sub_texture_in_region,
    _cogl_sub_texture_get_max_waste,
    _cogl_sub_texture_is_sliced,
    _cogl_sub_texture_can_hardware_repeat,
    _cogl_sub_texture_transform_coords_to_gl,
    _cogl_sub_texture_transform_quad_coords_to_gl,
    _cogl_sub_texture_get_gl_texture,
    _cogl_sub_texture_set_filters,
    _cogl_sub_texture_pre_paint,
    _cogl_sub_texture_ensure_non_quad_rendering,
    _cogl_sub_texture_set_wrap_mode_parameters,
    _cogl_sub_texture_get_format,
    _cogl_sub_texture_get_gl_format,
    _cogl_sub_texture_get_width,
    _cogl_sub_texture_get_height,
    NULL /* is_foreign */
  };
