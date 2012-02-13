/*
 * Cogl
 *
 * An object oriented GL/GLES Abstraction/Utility Layer
 *
 * Copyright (C) 2009 Intel Corporation.
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

#ifndef __COGL_ATLAS_TEXTURE_H
#define __COGL_ATLAS_TEXTURE_H

#include "cogl-handle.h"
#include "cogl-texture-private.h"
#include "cogl-rectangle-map.h"
#include "cogl-atlas.h"

#define COGL_ATLAS_TEXTURE(tex) ((CoglAtlasTexture *) tex)

typedef struct _CoglAtlasTexture CoglAtlasTexture;

struct _CoglAtlasTexture
{
  CoglTexture           _parent;

  /* The format that the texture is in. This isn't necessarily the
     same format as the atlas texture because we can store
     pre-multiplied and non-pre-multiplied textures together */
  CoglPixelFormat       format;

  /* The rectangle that was used to add this texture to the
     atlas. This includes the 1-pixel border */
  CoglRectangleMapEntry rectangle;

  /* The atlas that this texture is in. If the texture is no longer in
     an atlas then this will be NULL. A reference is taken on the
     atlas by the texture (but not vice versa so there is no cycle) */
  CoglAtlas            *atlas;

  /* A CoglSubTexture representing the region for easy rendering */
  CoglHandle            sub_texture;
};

CoglHandle
_cogl_atlas_texture_new_from_bitmap (CoglBitmap      *bmp,
                                     CoglTextureFlags flags,
                                     CoglPixelFormat  internal_format);

CoglHandle
_cogl_atlas_texture_new_with_size (unsigned int     width,
                                   unsigned int     height,
                                   CoglTextureFlags flags,
                                   CoglPixelFormat  internal_format);

void
_cogl_atlas_texture_add_reorganize_callback (GHookFunc callback,
                                             void *user_data);

void
_cogl_atlas_texture_remove_reorganize_callback (GHookFunc callback,
                                                void *user_data);

#endif /* __COGL_ATLAS_TEXTURE_H */
