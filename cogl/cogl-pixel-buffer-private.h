/*
 * Cogl
 *
 * An object oriented GL/GLES Abstraction/Utility Layer
 *
 * Copyright (C) 2010 Intel Corporation.
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
 *   Damien Lespiau <damien.lespiau@intel.com>
 *   Robert Bragg <robert@linux.intel.com>
 */

#ifndef __COGL_PIXEL_BUFFER_PRIVATE_H__
#define __COGL_PIXEL_BUFFER_PRIVATE_H__

#include "cogl-handle.h"
#include "cogl-buffer-private.h"

#include <glib.h>

G_BEGIN_DECLS

#define COGL_PIXEL_BUFFER(array)     ((CoglPixelBuffer *)(array))

struct _CoglPixelBuffer
{
  CoglBuffer            _parent;

  CoglPixelFormat       format;
  unsigned int          width;
  unsigned int          height;
  unsigned int          stride;
};

G_END_DECLS

#endif /* __COGL_PIXEL_BUFFER_PRIVATE_H__ */
