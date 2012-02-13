/*
 * Cogl
 *
 * An object oriented GL/GLES Abstraction/Utility Layer
 *
 * Copyright (C) 2011 Intel Corporation.
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
 * License along with this library. If not, see
 * <http://www.gnu.org/licenses/>.
 *
 *
 * Authors:
 *   Robert Bragg <robert@linux.intel.com>
 *   Neil Roberts <neil@linux.intel.com>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <X11/Xlib.h>

#include "cogl-winsys-egl-x11-private.h"
#include "cogl-winsys-egl-private.h"
#include "cogl-xlib-renderer-private.h"
#include "cogl-xlib-renderer.h"
#include "cogl-framebuffer-private.h"
#include "cogl-onscreen-private.h"
#include "cogl-display-private.h"
#include "cogl-renderer-private.h"

#include "cogl-texture-pixmap-x11-private.h"
#include "cogl-texture-2d-private.h"

#define COGL_ONSCREEN_X11_EVENT_MASK StructureNotifyMask

static const CoglWinsysEGLVtable _cogl_winsys_egl_vtable;

typedef struct _CoglDisplayXlib
{
  Window dummy_xwin;
} CoglDisplayXlib;

typedef struct _CoglOnscreenXlib
{
  Window xwin;
  gboolean is_foreign_xwin;
} CoglOnscreenXlib;

#ifdef EGL_KHR_image_pixmap
typedef struct _CoglTexturePixmapEGL
{
  EGLImageKHR image;
  CoglTexture *texture;
} CoglTexturePixmapEGL;
#endif

static CoglOnscreen *
find_onscreen_for_xid (CoglContext *context, guint32 xid)
{
  GList *l;

  for (l = context->framebuffers; l; l = l->next)
    {
      CoglFramebuffer *framebuffer = l->data;
      CoglOnscreenEGL *egl_onscreen;
      CoglOnscreenXlib *xlib_onscreen;

      if (!framebuffer->type == COGL_FRAMEBUFFER_TYPE_ONSCREEN)
        continue;

      egl_onscreen = COGL_ONSCREEN (framebuffer)->winsys;
      xlib_onscreen = egl_onscreen->platform;
      if (xlib_onscreen->xwin == (Window)xid)
        return COGL_ONSCREEN (framebuffer);
    }

  return NULL;
}

static CoglFilterReturn
event_filter_cb (XEvent *xevent, void *data)
{
  CoglContext *context = data;

  if (xevent->type == ConfigureNotify)
    {
      CoglOnscreen *onscreen =
        find_onscreen_for_xid (context, xevent->xconfigure.window);

      if (onscreen)
        {
          CoglFramebuffer *framebuffer = COGL_FRAMEBUFFER (onscreen);

          _cogl_framebuffer_winsys_update_size (framebuffer,
                                                xevent->xconfigure.width,
                                                xevent->xconfigure.height);
        }
    }

  return COGL_FILTER_CONTINUE;
}

static XVisualInfo *
get_visual_info (CoglDisplay *display, EGLConfig egl_config)
{
  CoglXlibRenderer *xlib_renderer =
    _cogl_xlib_renderer_get_data (display->renderer);
  CoglRendererEGL *egl_renderer = display->renderer->winsys;
  XVisualInfo visinfo_template;
  int template_mask = 0;
  XVisualInfo *visinfo = NULL;
  int visinfos_count;
  EGLint visualid, red_size, green_size, blue_size, alpha_size;

  eglGetConfigAttrib (egl_renderer->edpy, egl_config,
                      EGL_NATIVE_VISUAL_ID, &visualid);

  if (visualid != 0)
    {
      visinfo_template.visualid = visualid;
      template_mask |= VisualIDMask;
    }
  else
    {
      /* some EGL drivers don't implement the EGL_NATIVE_VISUAL_ID
       * attribute, so attempt to find the closest match. */

      eglGetConfigAttrib (egl_renderer->edpy, egl_config,
                          EGL_RED_SIZE, &red_size);
      eglGetConfigAttrib (egl_renderer->edpy, egl_config,
                          EGL_GREEN_SIZE, &green_size);
      eglGetConfigAttrib (egl_renderer->edpy, egl_config,
                          EGL_BLUE_SIZE, &blue_size);
      eglGetConfigAttrib (egl_renderer->edpy, egl_config,
                          EGL_ALPHA_SIZE, &alpha_size);

      visinfo_template.depth = red_size + green_size + blue_size + alpha_size;
      template_mask |= VisualDepthMask;

      visinfo_template.screen = DefaultScreen (xlib_renderer->xdpy);
      template_mask |= VisualScreenMask;
    }

  visinfo = XGetVisualInfo (xlib_renderer->xdpy,
                            template_mask,
                            &visinfo_template,
                            &visinfos_count);

  return visinfo;
}

static void
_cogl_winsys_renderer_disconnect (CoglRenderer *renderer)
{
  CoglRendererEGL *egl_renderer = renderer->winsys;

  _cogl_xlib_renderer_disconnect (renderer);

  eglTerminate (egl_renderer->edpy);

  g_slice_free (CoglRendererEGL, egl_renderer);
}

static gboolean
_cogl_winsys_renderer_connect (CoglRenderer *renderer,
                               GError **error)
{
  CoglRendererEGL *egl_renderer;
  CoglXlibRenderer *xlib_renderer;

  renderer->winsys = g_slice_new0 (CoglRendererEGL);
  egl_renderer = renderer->winsys;
  xlib_renderer = _cogl_xlib_renderer_get_data (renderer);

  egl_renderer->platform_vtable = &_cogl_winsys_egl_vtable;

  if (!_cogl_xlib_renderer_connect (renderer, error))
    goto error;

  egl_renderer->edpy =
    eglGetDisplay ((NativeDisplayType) xlib_renderer->xdpy);

  if (!_cogl_winsys_egl_renderer_connect_common (renderer, error))
    goto error;

  return TRUE;

error:
  _cogl_winsys_renderer_disconnect (renderer);
  return FALSE;
}

static gboolean
_cogl_winsys_egl_display_setup (CoglDisplay *display,
                                GError **error)
{
  CoglDisplayEGL *egl_display = display->winsys;
  CoglDisplayXlib *xlib_display;

  xlib_display = g_slice_new0 (CoglDisplayXlib);
  egl_display->platform = xlib_display;

  return TRUE;
}

static void
_cogl_winsys_egl_display_destroy (CoglDisplay *display)
{
  CoglDisplayEGL *egl_display = display->winsys;

  g_slice_free (CoglDisplayXlib, egl_display->platform);
}

static gboolean
_cogl_winsys_egl_context_init (CoglContext *context,
                               GError **error)
{
  cogl_xlib_renderer_add_filter (context->display->renderer,
                                 event_filter_cb,
                                 context);

  context->feature_flags |= COGL_FEATURE_ONSCREEN_MULTIPLE;
  COGL_FLAGS_SET (context->features,
                  COGL_FEATURE_ID_ONSCREEN_MULTIPLE, TRUE);
  COGL_FLAGS_SET (context->winsys_features,
                  COGL_WINSYS_FEATURE_MULTIPLE_ONSCREEN,
                  TRUE);

  return TRUE;
}

static void
_cogl_winsys_egl_context_deinit (CoglContext *context)
{
  cogl_xlib_renderer_remove_filter (context->display->renderer,
                                    event_filter_cb,
                                    context);
}

static gboolean
_cogl_winsys_egl_onscreen_init (CoglOnscreen *onscreen,
                                EGLConfig egl_config,
                                GError **error)
{
  CoglFramebuffer *framebuffer = COGL_FRAMEBUFFER (onscreen);
  CoglContext *context = framebuffer->context;
  CoglDisplay *display = context->display;
  CoglRenderer *renderer = display->renderer;
  CoglRendererEGL *egl_renderer = renderer->winsys;
  CoglXlibRenderer *xlib_renderer =
    _cogl_xlib_renderer_get_data (renderer);
  CoglOnscreenXlib *xlib_onscreen;
  CoglOnscreenEGL *egl_onscreen = onscreen->winsys;
  Window xwin;

  /* FIXME: We need to explicitly Select for ConfigureNotify events.
   * For foreign windows we need to be careful not to mess up any
   * existing event mask.
   * We need to document that for windows we create then toolkits
   * must be careful not to clear event mask bits that we select.
   */

  /* XXX: Note we ignore the user's original width/height when
   * given a foreign X window. */
  if (onscreen->foreign_xid)
    {
      Status status;
      CoglXlibTrapState state;
      XWindowAttributes attr;
      int xerror;

      xwin = onscreen->foreign_xid;

      _cogl_xlib_renderer_trap_errors (display->renderer, &state);

      status = XGetWindowAttributes (xlib_renderer->xdpy, xwin, &attr);
      xerror = _cogl_xlib_renderer_untrap_errors (display->renderer,
                                                  &state);
      if (status == 0 || xerror)
        {
          char message[1000];
          XGetErrorText (xlib_renderer->xdpy, xerror,
                         message, sizeof (message));
          g_set_error (error, COGL_WINSYS_ERROR,
                       COGL_WINSYS_ERROR_CREATE_ONSCREEN,
                       "Unable to query geometry of foreign "
                       "xid 0x%08lX: %s",
                       xwin, message);
          return FALSE;
        }

      _cogl_framebuffer_winsys_update_size (framebuffer,
                                            attr.width, attr.height);

      /* Make sure the app selects for the events we require... */
      onscreen->foreign_update_mask_callback (onscreen,
                                              COGL_ONSCREEN_X11_EVENT_MASK,
                                              onscreen->
                                              foreign_update_mask_data);
    }
  else
    {
      int width;
      int height;
      CoglXlibTrapState state;
      XVisualInfo *xvisinfo;
      XSetWindowAttributes xattr;
      unsigned long mask;
      int xerror;

      width = cogl_framebuffer_get_width (framebuffer);
      height = cogl_framebuffer_get_height (framebuffer);

      _cogl_xlib_renderer_trap_errors (display->renderer, &state);

      xvisinfo = get_visual_info (display, egl_config);
      if (xvisinfo == NULL)
        {
          g_set_error (error, COGL_WINSYS_ERROR,
                       COGL_WINSYS_ERROR_CREATE_ONSCREEN,
                       "Unable to retrieve the X11 visual of context's "
                       "fbconfig");
          return FALSE;
        }

      /* window attributes */
      xattr.background_pixel =
        WhitePixel (xlib_renderer->xdpy,
                    DefaultScreen (xlib_renderer->xdpy));
      xattr.border_pixel = 0;
      /* XXX: is this an X resource that we are leaking‽... */
      xattr.colormap =
        XCreateColormap (xlib_renderer->xdpy,
                         DefaultRootWindow (xlib_renderer->xdpy),
                         xvisinfo->visual,
                         AllocNone);
      xattr.event_mask = COGL_ONSCREEN_X11_EVENT_MASK;

      mask = CWBorderPixel | CWColormap | CWEventMask;

      xwin = XCreateWindow (xlib_renderer->xdpy,
                            DefaultRootWindow (xlib_renderer->xdpy),
                            0, 0,
                            width, height,
                            0,
                            xvisinfo->depth,
                            InputOutput,
                            xvisinfo->visual,
                            mask, &xattr);

      XFree (xvisinfo);

      XSync (xlib_renderer->xdpy, False);
      xerror =
        _cogl_xlib_renderer_untrap_errors (display->renderer, &state);
      if (xerror)
        {
          char message[1000];
          XGetErrorText (xlib_renderer->xdpy, xerror,
                         message, sizeof (message));
          g_set_error (error, COGL_WINSYS_ERROR,
                       COGL_WINSYS_ERROR_CREATE_ONSCREEN,
                       "X error while creating Window for CoglOnscreen: %s",
                       message);
          return FALSE;
        }
    }

  xlib_onscreen = g_slice_new (CoglOnscreenXlib);
  egl_onscreen->platform = xlib_onscreen;

  xlib_onscreen->xwin = xwin;
  xlib_onscreen->is_foreign_xwin = onscreen->foreign_xid ? TRUE : FALSE;

  egl_onscreen->egl_surface =
    eglCreateWindowSurface (egl_renderer->edpy,
                            egl_config,
                            (NativeWindowType) xlib_onscreen->xwin,
                            NULL);

  return TRUE;
}

static void
_cogl_winsys_egl_onscreen_deinit (CoglOnscreen *onscreen)
{
  CoglFramebuffer *framebuffer = COGL_FRAMEBUFFER (onscreen);
  CoglContext *context = framebuffer->context;
  CoglRenderer *renderer = context->display->renderer;
  CoglXlibRenderer *xlib_renderer =
    _cogl_xlib_renderer_get_data (renderer);
  CoglXlibTrapState old_state;
  CoglOnscreenEGL *egl_onscreen = onscreen->winsys;
  CoglOnscreenXlib *xlib_onscreen = egl_onscreen->platform;

  _cogl_xlib_renderer_trap_errors (renderer, &old_state);

  if (!xlib_onscreen->is_foreign_xwin && xlib_onscreen->xwin != None)
    {
      XDestroyWindow (xlib_renderer->xdpy, xlib_onscreen->xwin);
      xlib_onscreen->xwin = None;
    }
  else
    xlib_onscreen->xwin = None;

  XSync (xlib_renderer->xdpy, False);

  if (_cogl_xlib_renderer_untrap_errors (renderer,
                                         &old_state) != Success)
    g_warning ("X Error while destroying X window");

  g_slice_free (CoglOnscreenXlib, xlib_onscreen);
}

static void
_cogl_winsys_onscreen_set_visibility (CoglOnscreen *onscreen,
                                      gboolean visibility)
{
  CoglContext *context = COGL_FRAMEBUFFER (onscreen)->context;
  CoglRenderer *renderer = context->display->renderer;
  CoglXlibRenderer *xlib_renderer =
    _cogl_xlib_renderer_get_data (renderer);
  CoglOnscreenEGL *onscreen_egl = onscreen->winsys;
  CoglOnscreenXlib *xlib_onscreen = onscreen_egl->platform;

  if (visibility)
    XMapWindow (xlib_renderer->xdpy, xlib_onscreen->xwin);
  else
    XUnmapWindow (xlib_renderer->xdpy, xlib_onscreen->xwin);
}

static guint32
_cogl_winsys_onscreen_x11_get_window_xid (CoglOnscreen *onscreen)
{
  CoglOnscreenEGL *egl_onscreen = onscreen->winsys;
  CoglOnscreenXlib *xlib_onscreen = egl_onscreen->platform;

  return xlib_onscreen->xwin;
}

static gboolean
_cogl_winsys_egl_context_created (CoglDisplay *display,
                                  GError **error)
{
  CoglRenderer *renderer = display->renderer;
  CoglDisplayEGL *egl_display = display->winsys;
  CoglRendererEGL *egl_renderer = renderer->winsys;
  CoglXlibRenderer *xlib_renderer =
    _cogl_xlib_renderer_get_data (renderer);
  CoglDisplayXlib *xlib_display = egl_display->platform;
  XVisualInfo *xvisinfo;
  XSetWindowAttributes attrs;
  const char *error_message;

  xvisinfo = get_visual_info (display, egl_display->egl_config);
  if (xvisinfo == NULL)
    {
      error_message = "Unable to find suitable X visual";
      goto fail;
    }

  attrs.override_redirect = True;
  attrs.colormap = XCreateColormap (xlib_renderer->xdpy,
                                    DefaultRootWindow (xlib_renderer->xdpy),
                                    xvisinfo->visual,
                                    AllocNone);
  attrs.border_pixel = 0;

  xlib_display->dummy_xwin =
    XCreateWindow (xlib_renderer->xdpy,
                   DefaultRootWindow (xlib_renderer->xdpy),
                   -100, -100, 1, 1,
                   0,
                   xvisinfo->depth,
                   CopyFromParent,
                   xvisinfo->visual,
                   CWOverrideRedirect |
                   CWColormap |
                   CWBorderPixel,
                   &attrs);

  XFree (xvisinfo);

  egl_display->dummy_surface =
    eglCreateWindowSurface (egl_renderer->edpy,
                            egl_display->egl_config,
                            (NativeWindowType) xlib_display->dummy_xwin,
                            NULL);

  if (egl_display->dummy_surface == EGL_NO_SURFACE)
    {
      error_message = "Unable to create an EGL surface";
      goto fail;
    }

  if (!eglMakeCurrent (egl_renderer->edpy,
                       egl_display->dummy_surface,
                       egl_display->dummy_surface,
                       egl_display->egl_context))
    {
      error_message = "Unable to eglMakeCurrent with dummy surface";
      goto fail;
    }

  return TRUE;

fail:
  g_set_error (error, COGL_WINSYS_ERROR,
               COGL_WINSYS_ERROR_CREATE_CONTEXT,
               "%s", error_message);
  return FALSE;
}

static void
_cogl_winsys_egl_cleanup_context (CoglDisplay *display)
{
  CoglDisplayEGL *egl_display = display->winsys;
  CoglDisplayXlib *xlib_display = egl_display->platform;
  CoglRenderer *renderer = display->renderer;
  CoglXlibRenderer *xlib_renderer =
    _cogl_xlib_renderer_get_data (renderer);
  CoglRendererEGL *egl_renderer = renderer->winsys;

  if (egl_display->dummy_surface != EGL_NO_SURFACE)
    {
      eglDestroySurface (egl_renderer->edpy, egl_display->dummy_surface);
      egl_display->dummy_surface = EGL_NO_SURFACE;
    }

  if (xlib_display->dummy_xwin)
    {
      XDestroyWindow (xlib_renderer->xdpy, xlib_display->dummy_xwin);
      xlib_display->dummy_xwin = None;
    }
}

/* XXX: This is a particularly hacky _cogl_winsys interface... */
static XVisualInfo *
_cogl_winsys_xlib_get_visual_info (void)
{
  CoglDisplayEGL *egl_display;

  _COGL_GET_CONTEXT (ctx, NULL);

  _COGL_RETURN_VAL_IF_FAIL (ctx->display->winsys, FALSE);

  egl_display = ctx->display->winsys;

  if (!egl_display->found_egl_config)
    return NULL;

  return get_visual_info (ctx->display, egl_display->egl_config);
}

static void
_cogl_winsys_poll_get_info (CoglContext *context,
                            CoglPollFD **poll_fds,
                            int *n_poll_fds,
                            gint64 *timeout)
{
  _cogl_xlib_renderer_poll_get_info (context->display->renderer,
                                     poll_fds,
                                     n_poll_fds,
                                     timeout);
}

static void
_cogl_winsys_poll_dispatch (CoglContext *context,
                            const CoglPollFD *poll_fds,
                            int n_poll_fds)
{
  _cogl_xlib_renderer_poll_dispatch (context->display->renderer,
                                     poll_fds,
                                     n_poll_fds);
}

#ifdef EGL_KHR_image_pixmap

static gboolean
_cogl_winsys_texture_pixmap_x11_create (CoglTexturePixmapX11 *tex_pixmap)
{
  CoglTexturePixmapEGL *egl_tex_pixmap;
  EGLint attribs[] = {EGL_IMAGE_PRESERVED_KHR, EGL_TRUE, EGL_NONE};
  CoglPixelFormat texture_format;
  CoglRendererEGL *egl_renderer;

  /* FIXME: It should be possible to get to a CoglContext from any
   * CoglTexture pointer. */
  _COGL_GET_CONTEXT (ctx, FALSE);

  egl_renderer = ctx->display->renderer->winsys;

  if (!(egl_renderer->private_features &
        COGL_EGL_WINSYS_FEATURE_EGL_IMAGE_FROM_X11_PIXMAP) ||
      !(ctx->private_feature_flags &
        COGL_PRIVATE_FEATURE_TEXTURE_2D_FROM_EGL_IMAGE))
    {
      tex_pixmap->winsys = NULL;
      return FALSE;
    }

  egl_tex_pixmap = g_new0 (CoglTexturePixmapEGL, 1);

  egl_tex_pixmap->image =
    _cogl_egl_create_image (ctx,
                            EGL_NATIVE_PIXMAP_KHR,
                            (EGLClientBuffer)tex_pixmap->pixmap,
                            attribs);
  if (egl_tex_pixmap->image == EGL_NO_IMAGE_KHR)
    {
      g_free (egl_tex_pixmap);
      return FALSE;
    }

  texture_format = (tex_pixmap->depth >= 32 ?
                    COGL_PIXEL_FORMAT_RGBA_8888_PRE :
                    COGL_PIXEL_FORMAT_RGB_888);

  egl_tex_pixmap->texture = COGL_TEXTURE (
    _cogl_egl_texture_2d_new_from_image (ctx,
                                         tex_pixmap->width,
                                         tex_pixmap->height,
                                         texture_format,
                                         egl_tex_pixmap->image,
                                         NULL));

  tex_pixmap->winsys = egl_tex_pixmap;

  return TRUE;
}

static void
_cogl_winsys_texture_pixmap_x11_free (CoglTexturePixmapX11 *tex_pixmap)
{
  CoglTexturePixmapEGL *egl_tex_pixmap;

  /* FIXME: It should be possible to get to a CoglContext from any
   * CoglTexture pointer. */
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  if (!tex_pixmap->winsys)
    return;

  egl_tex_pixmap = tex_pixmap->winsys;

  if (egl_tex_pixmap->texture)
    cogl_object_unref (egl_tex_pixmap->texture);

  if (egl_tex_pixmap->image != EGL_NO_IMAGE_KHR)
    _cogl_egl_destroy_image (ctx, egl_tex_pixmap->image);

  tex_pixmap->winsys = NULL;
  g_free (egl_tex_pixmap);
}

static gboolean
_cogl_winsys_texture_pixmap_x11_update (CoglTexturePixmapX11 *tex_pixmap,
                                        gboolean needs_mipmap)
{
  if (needs_mipmap)
    return FALSE;

  return TRUE;
}

static void
_cogl_winsys_texture_pixmap_x11_damage_notify (CoglTexturePixmapX11 *tex_pixmap)
{
}

static CoglHandle
_cogl_winsys_texture_pixmap_x11_get_texture (CoglTexturePixmapX11 *tex_pixmap)
{
  CoglTexturePixmapEGL *egl_tex_pixmap = tex_pixmap->winsys;

  return egl_tex_pixmap->texture;
}

#endif /* EGL_KHR_image_pixmap */

static const CoglWinsysEGLVtable
_cogl_winsys_egl_vtable =
  {
    .display_setup = _cogl_winsys_egl_display_setup,
    .display_destroy = _cogl_winsys_egl_display_destroy,
    .context_created = _cogl_winsys_egl_context_created,
    .cleanup_context = _cogl_winsys_egl_cleanup_context,
    .context_init = _cogl_winsys_egl_context_init,
    .context_deinit = _cogl_winsys_egl_context_deinit,
    .onscreen_init = _cogl_winsys_egl_onscreen_init,
    .onscreen_deinit = _cogl_winsys_egl_onscreen_deinit
  };

const CoglWinsysVtable *
_cogl_winsys_egl_xlib_get_vtable (void)
{
  static gboolean vtable_inited = FALSE;
  static CoglWinsysVtable vtable;

  if (!vtable_inited)
    {
      /* The EGL_X11 winsys is a subclass of the EGL winsys so we
         start by copying its vtable */

      vtable = *_cogl_winsys_egl_get_vtable ();

      vtable.id = COGL_WINSYS_ID_EGL_XLIB;
      vtable.name = "EGL_XLIB";
      vtable.constraints |= (COGL_RENDERER_CONSTRAINT_USES_X11 |
                             COGL_RENDERER_CONSTRAINT_USES_XLIB);

      vtable.renderer_connect = _cogl_winsys_renderer_connect;
      vtable.renderer_disconnect = _cogl_winsys_renderer_disconnect;

      vtable.onscreen_set_visibility =
        _cogl_winsys_onscreen_set_visibility;

      vtable.onscreen_x11_get_window_xid =
        _cogl_winsys_onscreen_x11_get_window_xid;

      vtable.xlib_get_visual_info = _cogl_winsys_xlib_get_visual_info;

      vtable.poll_get_info = _cogl_winsys_poll_get_info;
      vtable.poll_dispatch = _cogl_winsys_poll_dispatch;

#ifdef EGL_KHR_image_pixmap
      /* X11 tfp support... */
      /* XXX: instead of having a rather monolithic winsys vtable we could
       * perhaps look for a way to separate these... */
      vtable.texture_pixmap_x11_create =
        _cogl_winsys_texture_pixmap_x11_create;
      vtable.texture_pixmap_x11_free =
        _cogl_winsys_texture_pixmap_x11_free;
      vtable.texture_pixmap_x11_update =
        _cogl_winsys_texture_pixmap_x11_update;
      vtable.texture_pixmap_x11_damage_notify =
        _cogl_winsys_texture_pixmap_x11_damage_notify;
      vtable.texture_pixmap_x11_get_texture =
        _cogl_winsys_texture_pixmap_x11_get_texture;
#endif /* EGL_KHR_image_pixmap) */

      vtable_inited = TRUE;
    }

  return &vtable;
}
