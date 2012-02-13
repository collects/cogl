/*
 * Cogl
 *
 * An object oriented GL/GLES Abstraction/Utility Layer
 *
 * Copyright (C) 2007,2008,2009 Intel Corporation.
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

#ifndef __COGL_FRAMEBUFFER_PRIVATE_H
#define __COGL_FRAMEBUFFER_PRIVATE_H

#include "cogl-object-private.h"
#include "cogl-matrix-stack.h"
#include "cogl-clip-state-private.h"
#include "cogl-journal-private.h"
#include "cogl-winsys-private.h"
#include "cogl-attribute-private.h"

#ifdef COGL_HAS_XLIB_SUPPORT
#include <X11/Xlib.h>
#endif

#ifdef COGL_HAS_GLX_SUPPORT
#include <GL/glx.h>
#include <GL/glxext.h>
#endif

typedef enum _CoglFramebufferType {
  COGL_FRAMEBUFFER_TYPE_ONSCREEN,
  COGL_FRAMEBUFFER_TYPE_OFFSCREEN
} CoglFramebufferType;

typedef struct
{
  CoglSwapChain *swap_chain;
  gboolean need_stencil;
  int samples_per_pixel;
  gboolean swap_throttled;
} CoglFramebufferConfig;

/* Flags to pass to _cogl_offscreen_new_to_texture_full */
typedef enum
{
  COGL_OFFSCREEN_DISABLE_DEPTH_AND_STENCIL = 1
} CoglOffscreenFlags;

/* XXX: The order of these indices determines the order they are
 * flushed.
 *
 * Flushing clip state may trash the modelview and projection matrices
 * so we must do it before flushing the matrices.
 */
typedef enum _CoglFramebufferStateIndex
{
  COGL_FRAMEBUFFER_STATE_INDEX_BIND               = 0,
  COGL_FRAMEBUFFER_STATE_INDEX_VIEWPORT           = 1,
  COGL_FRAMEBUFFER_STATE_INDEX_CLIP               = 2,
  COGL_FRAMEBUFFER_STATE_INDEX_DITHER             = 3,
  COGL_FRAMEBUFFER_STATE_INDEX_MODELVIEW          = 4,
  COGL_FRAMEBUFFER_STATE_INDEX_PROJECTION         = 5,
  COGL_FRAMEBUFFER_STATE_INDEX_COLOR_MASK         = 6,
  COGL_FRAMEBUFFER_STATE_INDEX_FRONT_FACE_WINDING = 7,
  COGL_FRAMEBUFFER_STATE_INDEX_MAX                = 8
} CoglFramebufferStateIndex;

typedef enum _CoglFramebufferState
{
  COGL_FRAMEBUFFER_STATE_BIND               = 1<<0,
  COGL_FRAMEBUFFER_STATE_VIEWPORT           = 1<<1,
  COGL_FRAMEBUFFER_STATE_CLIP               = 1<<2,
  COGL_FRAMEBUFFER_STATE_DITHER             = 1<<3,
  COGL_FRAMEBUFFER_STATE_MODELVIEW          = 1<<4,
  COGL_FRAMEBUFFER_STATE_PROJECTION         = 1<<5,
  COGL_FRAMEBUFFER_STATE_COLOR_MASK         = 1<<6,
  COGL_FRAMEBUFFER_STATE_FRONT_FACE_WINDING = 1<<7
} CoglFramebufferState;

#define COGL_FRAMEBUFFER_STATE_ALL ((1<<COGL_FRAMEBUFFER_STATE_INDEX_MAX) - 1)

struct _CoglFramebuffer
{
  CoglObject          _parent;
  CoglContext        *context;
  CoglFramebufferType  type;

  /* The user configuration before allocation... */
  CoglFramebufferConfig config;

  int                 width;
  int                 height;
  /* Format of the pixels in the framebuffer (including the expected
     premult state) */
  CoglPixelFormat     format;
  gboolean            allocated;

  CoglMatrixStack    *modelview_stack;
  CoglMatrixStack    *projection_stack;
  float               viewport_x;
  float               viewport_y;
  float               viewport_width;
  float               viewport_height;

  CoglClipState       clip_state;

  gboolean            dirty_bitmasks;
  int                 red_bits;
  int                 blue_bits;
  int                 green_bits;
  int                 alpha_bits;

  gboolean            dither_enabled;
  CoglColorMask       color_mask;

  int                 samples_per_pixel;

  /* We journal the textured rectangles we want to submit to OpenGL so
   * we have an oppertunity to batch them together into less draw
   * calls. */
  CoglJournal        *journal;

  /* The scene of a given framebuffer may depend on images in other
   * framebuffers... */
  GList              *deps;

  /* As part of an optimization for reading-back single pixels from a
   * framebuffer in some simple cases where the geometry is still
   * available in the journal we need to track the bounds of the last
   * region cleared, its color and we need to track when something
   * does in fact draw to that region so it is no longer clear.
   */
  float               clear_color_red;
  float               clear_color_green;
  float               clear_color_blue;
  float               clear_color_alpha;
  int                 clear_clip_x0;
  int                 clear_clip_y0;
  int                 clear_clip_x1;
  int                 clear_clip_y1;
  gboolean            clear_clip_dirty;
};

typedef struct _CoglOffscreen
{
  CoglFramebuffer  _parent;
  GLuint          fbo_handle;
  GSList          *renderbuffers;

  CoglTexture    *texture;
  int             texture_level;
  int             texture_level_width;
  int             texture_level_height;

  /* FIXME: _cogl_offscreen_new_to_texture_full should be made to use
   * fb->config to configure if we want a depth or stencil buffer so
   * we can get rid of these flags */
  CoglOffscreenFlags create_flags;
} CoglOffscreen;

#define COGL_OFFSCREEN(X) ((CoglOffscreen *)(X))

void
_cogl_framebuffer_init (CoglFramebuffer *framebuffer,
                        CoglContext *ctx,
                        CoglFramebufferType type,
                        CoglPixelFormat format,
                        int width,
                        int height);

void _cogl_framebuffer_free (CoglFramebuffer *framebuffer);

const CoglWinsysVtable *
_cogl_framebuffer_get_winsys (CoglFramebuffer *framebuffer);

void
_cogl_framebuffer_clear_without_flush4f (CoglFramebuffer *framebuffer,
                                         unsigned long buffers,
                                         float red,
                                         float green,
                                         float blue,
                                         float alpha);

void
_cogl_framebuffer_dirty (CoglFramebuffer *framebuffer);

CoglClipState *
_cogl_framebuffer_get_clip_state (CoglFramebuffer *framebuffer);

/*
 * _cogl_framebuffer_get_clip_stack:
 * @framebuffer: A #CoglFramebuffer
 *
 * Gets a pointer to the current clip stack. This can be used to later
 * return to the same clip stack state with
 * _cogl_framebuffer_set_clip_stack(). A reference is not taken on the
 * stack so if you want to keep it you should call
 * _cogl_clip_stack_ref().
 *
 * Return value: a pointer to the @framebuffer clip stack.
 */
CoglClipStack *
_cogl_framebuffer_get_clip_stack (CoglFramebuffer *framebuffer);

/*
 * _cogl_framebuffer_set_clip_stack:
 * @framebuffer: A #CoglFramebuffer
 * @stack: a pointer to the replacement clip stack
 *
 * Replaces the @framebuffer clip stack with @stack.
 */
void
_cogl_framebuffer_set_clip_stack (CoglFramebuffer *framebuffer,
                                  CoglClipStack *stack);

CoglMatrixStack *
_cogl_framebuffer_get_modelview_stack (CoglFramebuffer *framebuffer);

CoglMatrixStack *
_cogl_framebuffer_get_projection_stack (CoglFramebuffer *framebuffer);

void
_cogl_framebuffer_add_dependency (CoglFramebuffer *framebuffer,
                                  CoglFramebuffer *dependency);

void
_cogl_framebuffer_remove_all_dependencies (CoglFramebuffer *framebuffer);

void
_cogl_framebuffer_flush_journal (CoglFramebuffer *framebuffer);

void
_cogl_framebuffer_flush_dependency_journals (CoglFramebuffer *framebuffer);

gboolean
_cogl_framebuffer_try_fast_read_pixel (CoglFramebuffer *framebuffer,
                                       int x,
                                       int y,
                                       CoglReadPixelsFlags source,
                                       CoglPixelFormat format,
                                       guint8 *pixel);

void
_cogl_framebuffer_flush_state (CoglFramebuffer *draw_buffer,
                               CoglFramebuffer *read_buffer,
                               CoglFramebufferState state);

CoglFramebuffer *
_cogl_get_read_framebuffer (void);

GSList *
_cogl_create_framebuffer_stack (void);

void
_cogl_free_framebuffer_stack (GSList *stack);

/*
 * _cogl_offscreen_new_to_texture_full:
 * @texture: A #CoglTexture pointer
 * @create_flags: Flags specifying how to create the FBO
 * @level: The mipmap level within the texture to target
 *
 * Creates a new offscreen buffer which will target the given
 * texture. By default the buffer will have a depth and stencil
 * buffer. This can be disabled by passing
 * %COGL_OFFSCREEN_DISABLE_DEPTH_AND_STENCIL in @create_flags.
 *
 * Return value: the new CoglOffscreen object.
 */
CoglHandle
_cogl_offscreen_new_to_texture_full (CoglTexture *texture,
                                     CoglOffscreenFlags create_flags,
                                     unsigned int level);

/*
 * _cogl_push_framebuffers:
 * @draw_buffer: A pointer to the buffer used for drawing
 * @read_buffer: A pointer to the buffer used for reading back pixels
 *
 * Redirects drawing and reading to the specified framebuffers as in
 * cogl_push_framebuffer() except that it allows the draw and read
 * buffer to be different. The buffers are pushed as a pair so that
 * they can later both be restored with a single call to
 * cogl_pop_framebuffer().
 */
void
_cogl_push_framebuffers (CoglFramebuffer *draw_buffer,
                         CoglFramebuffer *read_buffer);

/*
 * _cogl_blit_framebuffer:
 * @src_x: Source x position
 * @src_y: Source y position
 * @dst_x: Destination x position
 * @dst_y: Destination y position
 * @width: Width of region to copy
 * @height: Height of region to copy
 *
 * This blits a region of the color buffer of the current draw buffer
 * to the current read buffer. The draw and read buffers can be set up
 * using _cogl_push_framebuffers(). This function should only be
 * called if the COGL_PRIVATE_FEATURE_OFFSCREEN_BLIT feature is
 * advertised. The two buffers must both be offscreen and have the
 * same format.
 *
 * Note that this function differs a lot from the glBlitFramebuffer
 * function provided by the GL_EXT_framebuffer_blit extension. Notably
 * it doesn't support having different sizes for the source and
 * destination rectangle. This isn't supported by the corresponding
 * GL_ANGLE_framebuffer_blit extension on GLES2.0 and it doesn't seem
 * like a particularly useful feature. If the application wanted to
 * scale the results it may make more sense to draw a primitive
 * instead.
 *
 * We can only really support blitting between two offscreen buffers
 * for this function on GLES2.0. This is because we effectively render
 * upside down to offscreen buffers to maintain Cogl's representation
 * of the texture coordinate system where 0,0 is the top left of the
 * texture. If we were to blit from an offscreen to an onscreen buffer
 * then we would need to mirror the blit along the x-axis but the GLES
 * extension does not support this.
 *
 * The GL function is documented to be affected by the scissor. This
 * function therefore ensure that an empty clip stack is flushed
 * before performing the blit which means the scissor is effectively
 * ignored.
 *
 * The function also doesn't support specifying the buffers to copy
 * and instead only the color buffer is copied. When copying the depth
 * or stencil buffers the extension on GLES2.0 only supports copying
 * the full buffer which would be awkward to document with this
 * API. If we wanted to support that feature it may be better to have
 * a separate function to copy the entire buffer for a given mask.
 */
void
_cogl_blit_framebuffer (unsigned int src_x,
                        unsigned int src_y,
                        unsigned int dst_x,
                        unsigned int dst_y,
                        unsigned int width,
                        unsigned int height);

void
_cogl_framebuffer_push_projection (CoglFramebuffer *framebuffer);

void
_cogl_framebuffer_pop_projection (CoglFramebuffer *framebuffer);

void
_cogl_framebuffer_save_clip_stack (CoglFramebuffer *framebuffer);

void
_cogl_framebuffer_restore_clip_stack (CoglFramebuffer *framebuffer);

void
_cogl_framebuffer_unref (CoglFramebuffer *framebuffer);

void
_cogl_framebuffer_draw_primitive (CoglFramebuffer *framebuffer,
                                  CoglPipeline *pipeline,
                                  CoglPrimitive *primitive,
                                  CoglDrawFlags flags);

/* This can be called directly by the CoglJournal to draw attributes
 * skipping the implicit journal flush, the framebuffer flush and
 * pipeline validation. */
void
_cogl_framebuffer_draw_attributes (CoglFramebuffer *framebuffer,
                                   CoglPipeline *pipeline,
                                   CoglVerticesMode mode,
                                   int first_vertex,
                                   int n_vertices,
                                   CoglAttribute **attributes,
                                   int n_attributes,
                                   CoglDrawFlags flags);

void
_cogl_framebuffer_draw_indexed_attributes (CoglFramebuffer *framebuffer,
                                           CoglPipeline *pipeline,
                                           CoglVerticesMode mode,
                                           int first_vertex,
                                           int n_vertices,
                                           CoglIndices *indices,
                                           CoglAttribute **attributes,
                                           int n_attributes,
                                           CoglDrawFlags flags);

#endif /* __COGL_FRAMEBUFFER_PRIVATE_H */
