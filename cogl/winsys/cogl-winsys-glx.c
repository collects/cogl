/*
 * Cogl
 *
 * An object oriented GL/GLES Abstraction/Utility Layer
 *
 * Copyright (C) 2007,2008,2009,2010,2011 Intel Corporation.
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
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "cogl.h"

#include "cogl-winsys-private.h"
#include "cogl-feature-private.h"
#include "cogl-context-private.h"
#include "cogl-framebuffer.h"
#include "cogl-swap-chain-private.h"
#include "cogl-renderer-private.h"
#include "cogl-glx-renderer-private.h"
#include "cogl-onscreen-template-private.h"
#include "cogl-xlib-display-private.h"
#include "cogl-glx-display-private.h"
#include "cogl-private.h"
#include "cogl-texture-2d-private.h"
#include "cogl-texture-rectangle-private.h"
#include "cogl-pipeline-opengl-private.h"

#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <glib/gi18n-lib.h>

#include <dlfcn.h>
#include <GL/glx.h>
#include <X11/Xlib.h>

#ifdef HAVE_DRM
#include <drm.h>
#include <sys/ioctl.h>
#include <errno.h>
#endif

#define COGL_ONSCREEN_X11_EVENT_MASK StructureNotifyMask

typedef struct _CoglContextGLX
{
  GLXDrawable current_drawable;
} CoglContextGLX;

typedef struct _CoglOnscreenXlib
{
  Window xwin;
  gboolean is_foreign_xwin;
} CoglOnscreenXlib;

typedef struct _CoglOnscreenGLX
{
  CoglOnscreenXlib _parent;
  GLXDrawable glxwin;
  guint32 last_swap_vsync_counter;
  GList *swap_callbacks;
} CoglOnscreenGLX;

typedef struct _CoglSwapBuffersNotifyEntry
{
  CoglSwapBuffersNotify callback;
  void *user_data;
  unsigned int id;
} CoglSwapBuffersNotifyEntry;

typedef struct _CoglTexturePixmapGLX
{
  GLXPixmap glx_pixmap;
  gboolean has_mipmap_space;
  gboolean can_mipmap;

  CoglHandle glx_tex;

  gboolean bind_tex_image_queued;
  gboolean pixmap_bound;
} CoglTexturePixmapGLX;

/* Define a set of arrays containing the functions required from GL
   for each winsys feature */
#define COGL_WINSYS_FEATURE_BEGIN(name, namespaces, extension_names,    \
                                  feature_flags, feature_flags_private, \
                                  winsys_feature)                       \
  static const CoglFeatureFunction                                      \
  cogl_glx_feature_ ## name ## _funcs[] = {
#define COGL_WINSYS_FEATURE_FUNCTION(ret, name, args)                   \
  { G_STRINGIFY (name), G_STRUCT_OFFSET (CoglGLXRenderer, pf_ ## name) },
#define COGL_WINSYS_FEATURE_END()               \
  { NULL, 0 },                                  \
    };
#include "cogl-winsys-glx-feature-functions.h"

/* Define an array of features */
#undef COGL_WINSYS_FEATURE_BEGIN
#define COGL_WINSYS_FEATURE_BEGIN(name, namespaces, extension_names,    \
                                  feature_flags, feature_flags_private, \
                                  winsys_feature)                       \
  { 255, 255, 0, namespaces, extension_names,                           \
      feature_flags, feature_flags_private,                             \
      winsys_feature, \
      cogl_glx_feature_ ## name ## _funcs },
#undef COGL_WINSYS_FEATURE_FUNCTION
#define COGL_WINSYS_FEATURE_FUNCTION(ret, name, args)
#undef COGL_WINSYS_FEATURE_END
#define COGL_WINSYS_FEATURE_END()

static const CoglFeatureData winsys_feature_data[] =
  {
#include "cogl-winsys-glx-feature-functions.h"
  };

static CoglFuncPtr
_cogl_winsys_renderer_get_proc_address (CoglRenderer *renderer,
                                        const char *name)
{
  CoglGLXRenderer *glx_renderer = renderer->winsys;

  return glx_renderer->glXGetProcAddress ((const GLubyte *) name);
}

static CoglOnscreen *
find_onscreen_for_xid (CoglContext *context, guint32 xid)
{
  GList *l;

  for (l = context->framebuffers; l; l = l->next)
    {
      CoglFramebuffer *framebuffer = l->data;
      CoglOnscreenXlib *xlib_onscreen;

      if (framebuffer->type != COGL_FRAMEBUFFER_TYPE_ONSCREEN)
        continue;

      /* Does the GLXEvent have the GLXDrawable or the X Window? */
      xlib_onscreen = COGL_ONSCREEN (framebuffer)->winsys;
      if (xlib_onscreen->xwin == (Window)xid)
        return COGL_ONSCREEN (framebuffer);
    }

  return NULL;
}

static void
notify_swap_buffers (CoglContext *context, GLXDrawable drawable)
{
  CoglOnscreen *onscreen = find_onscreen_for_xid (context, (guint32)drawable);
  CoglOnscreenGLX *glx_onscreen;
  GList *l;

  if (!onscreen)
    return;

  glx_onscreen = onscreen->winsys;

  for (l = glx_onscreen->swap_callbacks; l; l = l->next)
    {
      CoglSwapBuffersNotifyEntry *entry = l->data;
      entry->callback (COGL_FRAMEBUFFER (onscreen), entry->user_data);
    }
}

static CoglFilterReturn
glx_event_filter_cb (XEvent *xevent, void *data)
{
  CoglContext *context = data;
#ifdef GLX_INTEL_swap_event
  CoglGLXRenderer *glx_renderer;
#endif

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

      /* we let ConfigureNotify pass through */
      return COGL_FILTER_CONTINUE;
    }

#ifdef GLX_INTEL_swap_event
  glx_renderer = context->display->renderer->winsys;

  if (xevent->type == (glx_renderer->glx_event_base + GLX_BufferSwapComplete))
    {
      GLXBufferSwapComplete *swap_event = (GLXBufferSwapComplete *) xevent;

      notify_swap_buffers (context, swap_event->drawable);

      /* remove SwapComplete events from the queue */
      return COGL_FILTER_REMOVE;
    }
#endif /* GLX_INTEL_swap_event */

  return COGL_FILTER_CONTINUE;
}

static void
_cogl_winsys_renderer_disconnect (CoglRenderer *renderer)
{
  CoglGLXRenderer *glx_renderer = renderer->winsys;

  _cogl_xlib_renderer_disconnect (renderer);

  if (glx_renderer->libgl_module)
    g_module_close (glx_renderer->libgl_module);

  g_slice_free (CoglGLXRenderer, renderer->winsys);
}

static gboolean
resolve_core_glx_functions (CoglRenderer *renderer,
                            GError **error)
{
  CoglGLXRenderer *glx_renderer;

  glx_renderer = renderer->winsys;

  if (!g_module_symbol (glx_renderer->libgl_module, "glXCreatePixmap",
                        (void **) &glx_renderer->glXCreatePixmap) ||
      !g_module_symbol (glx_renderer->libgl_module, "glXDestroyPixmap",
                        (void **) &glx_renderer->glXDestroyPixmap) ||
      !g_module_symbol (glx_renderer->libgl_module, "glXChooseFBConfig",
                        (void **) &glx_renderer->glXChooseFBConfig) ||
      !g_module_symbol (glx_renderer->libgl_module, "glXCreateNewContext",
                        (void **) &glx_renderer->glXCreateNewContext) ||
      !g_module_symbol (glx_renderer->libgl_module, "glXGetFBConfigAttrib",
                        (void **) &glx_renderer->glXGetFBConfigAttrib) ||
      !g_module_symbol (glx_renderer->libgl_module, "glXQueryVersion",
                        (void **) &glx_renderer->glXQueryVersion) ||
      !g_module_symbol (glx_renderer->libgl_module, "glXDestroyContext",
                        (void **) &glx_renderer->glXDestroyContext) ||
      !g_module_symbol (glx_renderer->libgl_module, "glXMakeContextCurrent",
                        (void **) &glx_renderer->glXMakeContextCurrent) ||
      !g_module_symbol (glx_renderer->libgl_module, "glXSwapBuffers",
                        (void **) &glx_renderer->glXSwapBuffers) ||
      !g_module_symbol (glx_renderer->libgl_module, "glXQueryExtension",
                        (void **) &glx_renderer->glXQueryExtension) ||
      !g_module_symbol (glx_renderer->libgl_module, "glXIsDirect",
                        (void **) &glx_renderer->glXIsDirect) ||
      !g_module_symbol (glx_renderer->libgl_module, "glXGetVisualFromFBConfig",
                        (void **) &glx_renderer->glXGetVisualFromFBConfig) ||
      !g_module_symbol (glx_renderer->libgl_module, "glXSelectEvent",
                        (void **) &glx_renderer->glXSelectEvent) ||
      !g_module_symbol (glx_renderer->libgl_module, "glXCreateWindow",
                        (void **) &glx_renderer->glXCreateWindow) ||
      !g_module_symbol (glx_renderer->libgl_module, "glXGetFBConfigs",
                        (void **) &glx_renderer->glXGetFBConfigs) ||
      !g_module_symbol (glx_renderer->libgl_module, "glXDestroyWindow",
                        (void **) &glx_renderer->glXDestroyWindow) ||
      !g_module_symbol (glx_renderer->libgl_module, "glXQueryExtensionsString",
                        (void **) &glx_renderer->glXQueryExtensionsString) ||
      (!g_module_symbol (glx_renderer->libgl_module, "glXGetProcAddress",
                         (void **) &glx_renderer->glXGetProcAddress) &&
       !g_module_symbol (glx_renderer->libgl_module, "glXGetProcAddressARB",
                         (void **) &glx_renderer->glXGetProcAddress)))
    {
      g_set_error (error, COGL_WINSYS_ERROR,
                   COGL_WINSYS_ERROR_INIT,
                   "Failed to resolve required GLX symbol");
      return FALSE;
    }

  return TRUE;
}

static gboolean
_cogl_winsys_renderer_connect (CoglRenderer *renderer,
                               GError **error)
{
  CoglGLXRenderer *glx_renderer;
  CoglXlibRenderer *xlib_renderer;

  renderer->winsys = g_slice_new0 (CoglGLXRenderer);

  glx_renderer = renderer->winsys;
  xlib_renderer = renderer->winsys;

  if (!_cogl_xlib_renderer_connect (renderer, error))
    goto error;

  if (renderer->driver != COGL_DRIVER_GL)
    {
      g_set_error (error, COGL_WINSYS_ERROR,
                   COGL_WINSYS_ERROR_INIT,
                   "GLX Backend can only be used in conjunction with OpenGL");
      goto error;
    }

  glx_renderer->libgl_module = g_module_open (COGL_GL_LIBNAME,
                                              G_MODULE_BIND_LAZY);

  if (glx_renderer->libgl_module == NULL)
    {
      g_set_error (error, COGL_WINSYS_ERROR,
                   COGL_WINSYS_ERROR_INIT,
                   "Failed to dynamically open the OpenGL library");
      goto error;
    }

  if (!resolve_core_glx_functions (renderer, error))
    goto error;

  if (!glx_renderer->glXQueryExtension (xlib_renderer->xdpy,
                                        &glx_renderer->glx_error_base,
                                        &glx_renderer->glx_event_base))
    {
      g_set_error (error, COGL_WINSYS_ERROR,
                   COGL_WINSYS_ERROR_INIT,
                   "XServer appears to lack required GLX support");
      goto error;
    }

  /* XXX: Note: For a long time Mesa exported a hybrid GLX, exporting
   * extensions specified to require GLX 1.3, but still reporting 1.2
   * via glXQueryVersion. */
  if (!glx_renderer->glXQueryVersion (xlib_renderer->xdpy,
                                      &glx_renderer->glx_major,
                                      &glx_renderer->glx_minor)
      || !(glx_renderer->glx_major == 1 && glx_renderer->glx_minor >= 2))
    {
      g_set_error (error, COGL_WINSYS_ERROR,
                   COGL_WINSYS_ERROR_INIT,
                   "XServer appears to lack required GLX 1.2 support");
      goto error;
    }

  glx_renderer->dri_fd = -1;

  return TRUE;

error:
  _cogl_winsys_renderer_disconnect (renderer);
  return FALSE;
}

static gboolean
update_winsys_features (CoglContext *context, GError **error)
{
  CoglGLXDisplay *glx_display = context->display->winsys;
  CoglXlibRenderer *xlib_renderer = context->display->renderer->winsys;
  CoglGLXRenderer *glx_renderer = context->display->renderer->winsys;
  const char *glx_extensions;
  int default_screen;
  int i;

  g_return_val_if_fail (glx_display->glx_context, FALSE);

  if (!_cogl_context_update_features (context, error))
    return FALSE;

  memset (context->winsys_features, 0, sizeof (context->winsys_features));

  default_screen = DefaultScreen (xlib_renderer->xdpy);
  glx_extensions =
    glx_renderer->glXQueryExtensionsString (xlib_renderer->xdpy,
                                            default_screen);

  COGL_NOTE (WINSYS, "  GLX Extensions: %s", glx_extensions);

  context->feature_flags |= COGL_FEATURE_ONSCREEN_MULTIPLE;
  COGL_FLAGS_SET (context->winsys_features,
                  COGL_WINSYS_FEATURE_MULTIPLE_ONSCREEN,
                  TRUE);

  for (i = 0; i < G_N_ELEMENTS (winsys_feature_data); i++)
    if (_cogl_feature_check (context->display->renderer,
                             "GLX", winsys_feature_data + i, 0, 0,
                             COGL_DRIVER_GL, /* the driver isn't used */
                             glx_extensions,
                             glx_renderer))
      {
        context->feature_flags |= winsys_feature_data[i].feature_flags;
        if (winsys_feature_data[i].winsys_feature)
          COGL_FLAGS_SET (context->winsys_features,
                          winsys_feature_data[i].winsys_feature,
                          TRUE);
      }

  /* Note: the GLX_SGI_video_sync spec explicitly states this extension
   * only works for direct contexts. */
  if (!glx_renderer->is_direct)
    {
      glx_renderer->pf_glXGetVideoSync = NULL;
      glx_renderer->pf_glXWaitVideoSync = NULL;
    }

  if (glx_renderer->pf_glXWaitVideoSync)
    COGL_FLAGS_SET (context->winsys_features,
                    COGL_WINSYS_FEATURE_VBLANK_WAIT,
                    TRUE);

#ifdef HAVE_DRM
  /* drm is really an extreme fallback -rumoured to work with Via
   * chipsets... */
  if (!glx_renderer->pf_glXWaitVideoSync)
    {
      if (glx_renderer->dri_fd < 0)
        glx_renderer->dri_fd = open("/dev/dri/card0", O_RDWR);
      if (glx_renderer->dri_fd >= 0)
        COGL_FLAGS_SET (context->winsys_features,
                        COGL_WINSYS_FEATURE_VBLANK_WAIT,
                        TRUE);
    }
#endif

  if (glx_renderer->pf_glXCopySubBuffer || context->glBlitFramebuffer)
    COGL_FLAGS_SET (context->winsys_features,
                    COGL_WINSYS_FEATURE_SWAP_REGION, TRUE);

  /* Note: glXCopySubBuffer and glBlitFramebuffer won't be throttled
   * by the SwapInterval so we have to throttle swap_region requests
   * manually... */
  if (_cogl_winsys_has_feature (COGL_WINSYS_FEATURE_SWAP_REGION) &&
      _cogl_winsys_has_feature (COGL_WINSYS_FEATURE_VBLANK_WAIT))
    COGL_FLAGS_SET (context->winsys_features,
                    COGL_WINSYS_FEATURE_SWAP_REGION_THROTTLE, TRUE);

  return TRUE;
}

/* It seems the GLX spec never defined an invalid GLXFBConfig that
 * we could overload as an indication of error, so we have to return
 * an explicit boolean status. */
static gboolean
find_fbconfig (CoglDisplay *display,
               gboolean with_alpha,
               GLXFBConfig *config_ret,
               GError **error)
{
  CoglXlibRenderer *xlib_renderer = display->renderer->winsys;
  CoglGLXRenderer *glx_renderer = display->renderer->winsys;
  GLXFBConfig *configs = NULL;
  int n_configs, i;
  static const int attributes[] = {
    GLX_DRAWABLE_TYPE,    GLX_WINDOW_BIT,
    GLX_RENDER_TYPE,      GLX_RGBA_BIT,
    GLX_DOUBLEBUFFER,     GL_TRUE,
    GLX_RED_SIZE,         1,
    GLX_GREEN_SIZE,       1,
    GLX_BLUE_SIZE,        1,
    GLX_ALPHA_SIZE,       1,
    GLX_DEPTH_SIZE,       1,
    GLX_STENCIL_SIZE,     1,
    None
  };
  gboolean ret = TRUE;
  int xscreen_num = DefaultScreen (xlib_renderer->xdpy);

  configs = glx_renderer->glXChooseFBConfig (xlib_renderer->xdpy,
                                             xscreen_num,
                                             attributes,
                                             &n_configs);
  if (!configs || n_configs == 0)
    {
      g_set_error (error, COGL_WINSYS_ERROR,
                   COGL_WINSYS_ERROR_CREATE_CONTEXT,
                   "Failed to find any compatible fbconfigs");
      ret = FALSE;
      goto done;
    }

  if (with_alpha)
    {
      for (i = 0; i < n_configs; i++)
        {
          XVisualInfo *vinfo;

          vinfo = glx_renderer->glXGetVisualFromFBConfig (xlib_renderer->xdpy,
                                                          configs[i]);
          if (vinfo == NULL)
            continue;

          if (vinfo->depth == 32 &&
              (vinfo->red_mask | vinfo->green_mask | vinfo->blue_mask)
              != 0xffffffff)
            {
              COGL_NOTE (WINSYS, "Found an ARGB FBConfig [index:%d]", i);
              *config_ret = configs[i];
              goto done;
            }
        }

      /* If we make it here then we didn't find an RGBA config so
         we'll fall back to using an RGB config */
      g_set_error (error, COGL_WINSYS_ERROR,
                   COGL_WINSYS_ERROR_CREATE_CONTEXT,
                   "Unable to find fbconfig with rgba visual");
      ret = FALSE;
      goto done;
    }
  else
    {
      COGL_NOTE (WINSYS, "Using the first available FBConfig");
      *config_ret = configs[0];
    }

done:
  XFree (configs);
  return ret;
}

static gboolean
create_context (CoglDisplay *display, GError **error)
{
  CoglGLXDisplay *glx_display = display->winsys;
  CoglXlibDisplay *xlib_display = display->winsys;
  CoglXlibRenderer *xlib_renderer = display->renderer->winsys;
  CoglGLXRenderer *glx_renderer = display->renderer->winsys;
  gboolean support_transparent_windows;
  GLXFBConfig config;
  GError *fbconfig_error = NULL;
  XSetWindowAttributes attrs;
  XVisualInfo *xvisinfo;
  GLXDrawable dummy_drawable;
  CoglXlibTrapState old_state;

  g_return_val_if_fail (glx_display->glx_context == NULL, TRUE);

  if (display->onscreen_template->swap_chain &&
      display->onscreen_template->swap_chain->has_alpha)
    support_transparent_windows = TRUE;
  else
    support_transparent_windows = FALSE;

  glx_display->found_fbconfig =
    find_fbconfig (display, support_transparent_windows, &config,
                   &fbconfig_error);
  if (!glx_display->found_fbconfig)
    {
      g_set_error (error, COGL_WINSYS_ERROR,
                   COGL_WINSYS_ERROR_CREATE_CONTEXT,
                   "Unable to find suitable fbconfig for the GLX context: %s",
                   fbconfig_error->message);
      g_error_free (fbconfig_error);
      return FALSE;
    }

  glx_display->fbconfig = config;
  glx_display->fbconfig_has_rgba_visual = support_transparent_windows;

  COGL_NOTE (WINSYS, "Creating GLX Context (display: %p)",
             xlib_renderer->xdpy);

  glx_display->glx_context =
    glx_renderer->glXCreateNewContext (xlib_renderer->xdpy,
                                       config,
                                       GLX_RGBA_TYPE,
                                       NULL,
                                       True);
  if (glx_display->glx_context == NULL)
    {
      g_set_error (error, COGL_WINSYS_ERROR,
                   COGL_WINSYS_ERROR_CREATE_CONTEXT,
                   "Unable to create suitable GL context");
      return FALSE;
    }

  glx_renderer->is_direct =
    glx_renderer->glXIsDirect (xlib_renderer->xdpy, glx_display->glx_context);

  COGL_NOTE (WINSYS, "Setting %s context",
             glx_renderer->is_direct ? "direct" : "indirect");

  /* XXX: GLX doesn't let us make a context current without a window
   * so we create a dummy window that we can use while no CoglOnscreen
   * framebuffer is in use.
   */

  xvisinfo = glx_renderer->glXGetVisualFromFBConfig (xlib_renderer->xdpy,
                                                     config);
  if (xvisinfo == NULL)
    {
      g_set_error (error, COGL_WINSYS_ERROR,
                   COGL_WINSYS_ERROR_CREATE_CONTEXT,
                   "Unable to retrieve the X11 visual");
      return FALSE;
    }

  _cogl_xlib_renderer_trap_errors (display->renderer, &old_state);

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
                   CWOverrideRedirect | CWColormap | CWBorderPixel,
                   &attrs);

  /* Try and create a GLXWindow to use with extensions dependent on
   * GLX versions >= 1.3 that don't accept regular X Windows as GLX
   * drawables. */
  if (glx_renderer->glx_major == 1 && glx_renderer->glx_minor >= 3)
    {
      glx_display->dummy_glxwin =
        glx_renderer->glXCreateWindow (xlib_renderer->xdpy,
                                       config,
                                       xlib_display->dummy_xwin,
                                       NULL);
    }

  if (glx_display->dummy_glxwin)
    dummy_drawable = glx_display->dummy_glxwin;
  else
    dummy_drawable = xlib_display->dummy_xwin;

  COGL_NOTE (WINSYS, "Selecting dummy 0x%x for the GLX context",
             (unsigned int) dummy_drawable);

  glx_renderer->glXMakeContextCurrent (xlib_renderer->xdpy,
                                       dummy_drawable,
                                       dummy_drawable,
                                       glx_display->glx_context);

  XFree (xvisinfo);

  if (_cogl_xlib_renderer_untrap_errors (display->renderer, &old_state))
    {
      g_set_error (error, COGL_WINSYS_ERROR,
                   COGL_WINSYS_ERROR_CREATE_CONTEXT,
                   "Unable to select the newly created GLX context");
      return FALSE;
    }

  return TRUE;
}

static void
_cogl_winsys_display_destroy (CoglDisplay *display)
{
  CoglGLXDisplay *glx_display = display->winsys;
  CoglXlibDisplay *xlib_display = display->winsys;
  CoglXlibRenderer *xlib_renderer = display->renderer->winsys;
  CoglGLXRenderer *glx_renderer = display->renderer->winsys;

  g_return_if_fail (glx_display != NULL);

  if (glx_display->glx_context)
    {
      glx_renderer->glXMakeContextCurrent (xlib_renderer->xdpy,
                                           None, None, NULL);
      glx_renderer->glXDestroyContext (xlib_renderer->xdpy,
                                       glx_display->glx_context);
      glx_display->glx_context = NULL;
    }

  if (glx_display->dummy_glxwin)
    {
      glx_renderer->glXDestroyWindow (xlib_renderer->xdpy,
                                      glx_display->dummy_glxwin);
      glx_display->dummy_glxwin = None;
    }

  if (xlib_display->dummy_xwin)
    {
      XDestroyWindow (xlib_renderer->xdpy, xlib_display->dummy_xwin);
      xlib_display->dummy_xwin = None;
    }

  g_slice_free (CoglGLXDisplay, display->winsys);
  display->winsys = NULL;
}

static gboolean
_cogl_winsys_display_setup (CoglDisplay *display,
                            GError **error)
{
  CoglGLXDisplay *glx_display;
  int i;

  g_return_val_if_fail (display->winsys == NULL, FALSE);

  glx_display = g_slice_new0 (CoglGLXDisplay);
  display->winsys = glx_display;

  if (!create_context (display, error))
    goto error;

  for (i = 0; i < COGL_GLX_N_CACHED_CONFIGS; i++)
    glx_display->glx_cached_configs[i].depth = -1;

  return TRUE;

error:
  _cogl_winsys_display_destroy (display);
  return FALSE;
}

static gboolean
_cogl_winsys_context_init (CoglContext *context, GError **error)
{
  context->winsys = g_new0 (CoglContextGLX, 1);

  cogl_xlib_renderer_add_filter (context->display->renderer,
                                 glx_event_filter_cb,
                                 context);
  return update_winsys_features (context, error);
}

static void
_cogl_winsys_context_deinit (CoglContext *context)
{
  cogl_xlib_renderer_remove_filter (context->display->renderer,
                                    glx_event_filter_cb,
                                    context);
  g_free (context->winsys);
}

static gboolean
_cogl_winsys_onscreen_init (CoglOnscreen *onscreen,
                            GError **error)
{
  CoglFramebuffer *framebuffer = COGL_FRAMEBUFFER (onscreen);
  CoglContext *context = framebuffer->context;
  CoglDisplay *display = context->display;
  CoglGLXDisplay *glx_display = display->winsys;
  CoglXlibRenderer *xlib_renderer = display->renderer->winsys;
  CoglGLXRenderer *glx_renderer = display->renderer->winsys;
  Window xwin;
  CoglOnscreenXlib *xlib_onscreen;
  CoglOnscreenGLX *glx_onscreen;

  g_return_val_if_fail (glx_display->glx_context, FALSE);

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
      XSync (xlib_renderer->xdpy, False);
      xerror = _cogl_xlib_renderer_untrap_errors (display->renderer, &state);
      if (status == 0 || xerror)
        {
          char message[1000];
          XGetErrorText (xlib_renderer->xdpy, xerror, message, sizeof(message));
          g_set_error (error, COGL_WINSYS_ERROR,
                       COGL_WINSYS_ERROR_CREATE_ONSCREEN,
                       "Unable to query geometry of foreign xid 0x%08lX: %s",
                       xwin, message);
          return FALSE;
        }

      _cogl_framebuffer_winsys_update_size (framebuffer,
                                            attr.width, attr.height);

      /* Make sure the app selects for the events we require... */
      onscreen->foreign_update_mask_callback (onscreen,
                                              COGL_ONSCREEN_X11_EVENT_MASK,
                                              onscreen->foreign_update_mask_data);
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

      xvisinfo = glx_renderer->glXGetVisualFromFBConfig (xlib_renderer->xdpy,
                                                         glx_display->fbconfig);
      if (xvisinfo == NULL)
        {
          g_set_error (error, COGL_WINSYS_ERROR,
                       COGL_WINSYS_ERROR_CREATE_ONSCREEN,
                       "Unable to retrieve the X11 visual of context's "
                       "fbconfig");
          return FALSE;
        }

      /* window attributes */
      xattr.background_pixel = WhitePixel (xlib_renderer->xdpy,
                                           DefaultScreen (xlib_renderer->xdpy));
      xattr.border_pixel = 0;
      /* XXX: is this an X resource that we are leaking‽... */
      xattr.colormap = XCreateColormap (xlib_renderer->xdpy,
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
      xerror = _cogl_xlib_renderer_untrap_errors (display->renderer, &state);
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

  onscreen->winsys = g_slice_new0 (CoglOnscreenGLX);
  xlib_onscreen = onscreen->winsys;
  glx_onscreen = onscreen->winsys;

  xlib_onscreen->xwin = xwin;
  xlib_onscreen->is_foreign_xwin = onscreen->foreign_xid ? TRUE : FALSE;

  /* Try and create a GLXWindow to use with extensions dependent on
   * GLX versions >= 1.3 that don't accept regular X Windows as GLX
   * drawables. */
  if (glx_renderer->glx_major == 1 && glx_renderer->glx_minor >= 3)
    {
      glx_onscreen->glxwin =
        glx_renderer->glXCreateWindow (xlib_renderer->xdpy,
                                       glx_display->fbconfig,
                                       xlib_onscreen->xwin,
                                       NULL);
    }

#ifdef GLX_INTEL_swap_event
  if (_cogl_winsys_has_feature (COGL_WINSYS_FEATURE_SWAP_BUFFERS_EVENT))
    {
      GLXDrawable drawable =
        glx_onscreen->glxwin ? glx_onscreen->glxwin : xlib_onscreen->xwin;

      /* similarly to above, we unconditionally select this event
       * because we rely on it to advance the master clock, and
       * drive redraw/relayout, animations and event handling.
       */
      glx_renderer->glXSelectEvent (xlib_renderer->xdpy,
                                    drawable,
                                    GLX_BUFFER_SWAP_COMPLETE_INTEL_MASK);
    }
#endif /* GLX_INTEL_swap_event */

  return TRUE;
}

static void
_cogl_winsys_onscreen_deinit (CoglOnscreen *onscreen)
{
  CoglFramebuffer *framebuffer = COGL_FRAMEBUFFER (onscreen);
  CoglContext *context = framebuffer->context;
  CoglXlibRenderer *xlib_renderer = context->display->renderer->winsys;
  CoglGLXRenderer *glx_renderer = context->display->renderer->winsys;
  CoglXlibTrapState old_state;
  CoglOnscreenXlib *xlib_onscreen = onscreen->winsys;
  CoglOnscreenGLX *glx_onscreen = onscreen->winsys;

  /* If we never successfully allocated then there's nothing to do */
  if (glx_onscreen == NULL)
    return;

  _cogl_xlib_renderer_trap_errors (context->display->renderer, &old_state);

  if (glx_onscreen->glxwin != None)
    {
      glx_renderer->glXDestroyWindow (xlib_renderer->xdpy,
                                      glx_onscreen->glxwin);
      glx_onscreen->glxwin = None;
    }

  if (!xlib_onscreen->is_foreign_xwin && xlib_onscreen->xwin != None)
    {
      XDestroyWindow (xlib_renderer->xdpy, xlib_onscreen->xwin);
      xlib_onscreen->xwin = None;
    }
  else
    xlib_onscreen->xwin = None;

  XSync (xlib_renderer->xdpy, False);

  _cogl_xlib_renderer_untrap_errors (context->display->renderer, &old_state);

  g_slice_free (CoglOnscreenGLX, onscreen->winsys);
  onscreen->winsys = NULL;
}

static void
_cogl_winsys_onscreen_bind (CoglOnscreen *onscreen)
{
  CoglContext *context = COGL_FRAMEBUFFER (onscreen)->context;
  CoglContextGLX *glx_context = context->winsys;
  CoglXlibDisplay *xlib_display = context->display->winsys;
  CoglGLXDisplay *glx_display = context->display->winsys;
  CoglXlibRenderer *xlib_renderer = context->display->renderer->winsys;
  CoglGLXRenderer *glx_renderer = context->display->renderer->winsys;
  CoglOnscreenXlib *xlib_onscreen = onscreen->winsys;
  CoglOnscreenGLX *glx_onscreen = onscreen->winsys;
  CoglXlibTrapState old_state;
  GLXDrawable drawable;

  if (G_UNLIKELY (!onscreen))
    {
      drawable =
        glx_display->dummy_glxwin ?
        glx_display->dummy_glxwin : xlib_display->dummy_xwin;

      if (glx_context->current_drawable == drawable)
        return;

      _cogl_xlib_renderer_trap_errors (context->display->renderer, &old_state);

      glx_renderer->glXMakeContextCurrent (xlib_renderer->xdpy,
                                           drawable, drawable,
                                           glx_display->glx_context);
    }
  else
    {
      drawable =
        glx_onscreen->glxwin ? glx_onscreen->glxwin : xlib_onscreen->xwin;

      if (glx_context->current_drawable == drawable)
        return;

      _cogl_xlib_renderer_trap_errors (context->display->renderer, &old_state);

      COGL_NOTE (WINSYS,
                 "MakeContextCurrent dpy: %p, window: 0x%x (%s), context: %p",
                 xlib_renderer->xdpy,
                 (unsigned int) drawable,
                 xlib_onscreen->is_foreign_xwin ? "foreign" : "native",
                 glx_display->glx_context);

      glx_renderer->glXMakeContextCurrent (xlib_renderer->xdpy,
                                           drawable,
                                           drawable,
                                           glx_display->glx_context);

      /* In case we are using GLX_SGI_swap_control for vblank syncing
       * we need call glXSwapIntervalSGI here to make sure that it
       * affects the current drawable.
       *
       * Note: we explicitly set to 0 when we aren't using the swap
       * interval to synchronize since some drivers have a default
       * swap interval of 1. Sadly some drivers even ignore requests
       * to disable the swap interval.
       *
       * NB: glXSwapIntervalSGI applies to the context not the
       * drawable which is why we can't just do this once when the
       * framebuffer is allocated.
       *
       * FIXME: We should check for GLX_EXT_swap_control which allows
       * per framebuffer swap intervals. GLX_MESA_swap_control also
       * allows per-framebuffer swap intervals but the semantics tend
       * to be more muddled since Mesa drivers tend to expose both the
       * MESA and SGI extensions which should technically be mutually
       * exclusive.
       */
      if (glx_renderer->pf_glXSwapInterval)
        {
          if (onscreen->swap_throttled)
            glx_renderer->pf_glXSwapInterval (1);
          else
            glx_renderer->pf_glXSwapInterval (0);
        }
    }

  XSync (xlib_renderer->xdpy, False);

  /* FIXME: We should be reporting a GError here
   */
  if (_cogl_xlib_renderer_untrap_errors (context->display->renderer,
                                         &old_state))
    {
      g_warning ("X Error received while making drawable 0x%08lX current",
                 drawable);
      return;
    }

  glx_context->current_drawable = drawable;
}

#ifdef HAVE_DRM
static int
drm_wait_vblank (int fd, drm_wait_vblank_t *vbl)
{
    int ret, rc;

    do
      {
        ret = ioctl (fd, DRM_IOCTL_WAIT_VBLANK, vbl);
        vbl->request.type &= ~_DRM_VBLANK_RELATIVE;
        rc = errno;
      }
    while (ret && rc == EINTR);

    return rc;
}
#endif /* HAVE_DRM */

static void
_cogl_winsys_wait_for_vblank (void)
{
  CoglGLXRenderer *glx_renderer;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  glx_renderer = ctx->display->renderer->winsys;

  if (glx_renderer->pf_glXGetVideoSync)
    {
      guint32 current_count;

      glx_renderer->pf_glXGetVideoSync (&current_count);
      glx_renderer->pf_glXWaitVideoSync (2,
                                         (current_count + 1) % 2,
                                         &current_count);
    }
#ifdef HAVE_DRM
  else
    {
      drm_wait_vblank_t blank;

      COGL_NOTE (WINSYS, "Waiting for vblank (drm)");
      blank.request.type = _DRM_VBLANK_RELATIVE;
      blank.request.sequence = 1;
      blank.request.signal = 0;
      drm_wait_vblank (glx_renderer->dri_fd, &blank);
    }
#endif /* HAVE_DRM */
}

static guint32
_cogl_winsys_get_vsync_counter (void)
{
  guint32 video_sync_count;
  CoglGLXRenderer *glx_renderer;

  _COGL_GET_CONTEXT (ctx, 0);

  glx_renderer = ctx->display->renderer->winsys;

  glx_renderer->pf_glXGetVideoSync (&video_sync_count);

  return video_sync_count;
}

static void
_cogl_winsys_onscreen_swap_region (CoglOnscreen *onscreen,
                                   const int *user_rectangles,
                                   int n_rectangles)
{
  CoglFramebuffer *framebuffer = COGL_FRAMEBUFFER (onscreen);
  CoglContext *context = framebuffer->context;
  CoglXlibRenderer *xlib_renderer = context->display->renderer->winsys;
  CoglGLXRenderer *glx_renderer = context->display->renderer->winsys;
  CoglOnscreenXlib *xlib_onscreen = onscreen->winsys;
  CoglOnscreenGLX *glx_onscreen = onscreen->winsys;
  GLXDrawable drawable =
    glx_onscreen->glxwin ? glx_onscreen->glxwin : xlib_onscreen->xwin;
  guint32 end_frame_vsync_counter = 0;
  gboolean have_counter;
  gboolean can_wait;

  /*
   * We assume that glXCopySubBuffer is synchronized which means it won't prevent multiple
   * blits per retrace if they can all be performed in the blanking period. If that's the
   * case then we still want to use the vblank sync menchanism but
   * we only need it to throttle redraws.
   */
  gboolean blit_sub_buffer_is_synchronized =
     _cogl_winsys_has_feature (COGL_WINSYS_FEATURE_SWAP_REGION_SYNCHRONIZED);

  int framebuffer_height =  cogl_framebuffer_get_height (framebuffer);
  int *rectangles = g_alloca (sizeof (int) * n_rectangles * 4);
  int i;

  /* glXCopySubBuffer expects rectangles relative to the bottom left corner but
   * we are given rectangles relative to the top left so we need to flip
   * them... */
  memcpy (rectangles, user_rectangles, sizeof (int) * n_rectangles * 4);
  for (i = 0; i < n_rectangles; i++)
    {
      int *rect = &rectangles[4 * i];
      rect[1] = framebuffer_height - rect[1] - rect[3];
    }

  _cogl_framebuffer_flush_state (framebuffer,
                                 framebuffer,
                                 COGL_FRAMEBUFFER_FLUSH_BIND_ONLY);

  if (onscreen->swap_throttled)
    {
      have_counter =
        _cogl_winsys_has_feature (COGL_WINSYS_FEATURE_VBLANK_COUNTER);
      can_wait = _cogl_winsys_has_feature (COGL_WINSYS_FEATURE_VBLANK_WAIT);
    }
  else
    {
      have_counter = FALSE;
      can_wait = FALSE;
    }

  /* We need to ensure that all the rendering is done, otherwise
   * redraw operations that are slower than the framerate can
   * queue up in the pipeline during a heavy animation, causing a
   * larger and larger backlog of rendering visible as lag to the
   * user.
   *
   * For an exaggerated example consider rendering at 60fps (so 16ms
   * per frame) and you have a really slow frame that takes 160ms to
   * render, even though painting the scene and issuing the commands
   * to the GPU takes no time at all. If all we did was use the
   * video_sync extension to throttle the painting done by the CPU
   * then every 16ms we would have another frame queued up even though
   * the GPU has only rendered one tenth of the current frame. By the
   * time the GPU would get to the 2nd frame there would be 9 frames
   * waiting to be rendered.
   *
   * The problem is that we don't currently have a good way to throttle
   * the GPU, only the CPU so we have to resort to synchronizing the
   * GPU with the CPU to throttle it.
   *
   * Note: since calling glFinish() and synchronizing the CPU with
   * the GPU is far from ideal, we hope that this is only a short
   * term solution.
   * - One idea is to using sync objects to track render
   *   completion so we can throttle the backlog (ideally with an
   *   additional extension that lets us get notifications in our
   *   mainloop instead of having to busy wait for the
   *   completion.)
   * - Another option is to support clipped redraws by reusing the
   *   contents of old back buffers such that we can flip instead
   *   of using a blit and then we can use GLX_INTEL_swap_events
   *   to throttle. For this though we would still probably want an
   *   additional extension so we can report the limited region of
   *   the window damage to X/compositors.
   */
  context->glFinish ();

  if (blit_sub_buffer_is_synchronized && have_counter && can_wait)
    {
      end_frame_vsync_counter = _cogl_winsys_get_vsync_counter ();

      /* If we have the GLX_SGI_video_sync extension then we can
       * be a bit smarter about how we throttle blits by avoiding
       * any waits if we can see that the video sync count has
       * already progressed. */
      if (glx_onscreen->last_swap_vsync_counter == end_frame_vsync_counter)
        _cogl_winsys_wait_for_vblank ();
    }
  else if (can_wait)
    _cogl_winsys_wait_for_vblank ();

  if (glx_renderer->pf_glXCopySubBuffer)
    {
      Display *xdpy = xlib_renderer->xdpy;
      int i;
      for (i = 0; i < n_rectangles; i++)
        {
          int *rect = &rectangles[4 * i];
          glx_renderer->pf_glXCopySubBuffer (xdpy, drawable,
                                             rect[0], rect[1], rect[2], rect[3]);
        }
    }
  else if (context->glBlitFramebuffer)
    {
      int i;
      /* XXX: checkout how this state interacts with the code to use
       * glBlitFramebuffer in Neil's texture atlasing branch */
      context->glDrawBuffer (GL_FRONT);
      for (i = 0; i < n_rectangles; i++)
        {
          int *rect = &rectangles[4 * i];
          int x2 = rect[0] + rect[2];
          int y2 = rect[1] + rect[3];
          context->glBlitFramebuffer (rect[0], rect[1], x2, y2,
                                      rect[0], rect[1], x2, y2,
                                      GL_COLOR_BUFFER_BIT, GL_NEAREST);
        }
      context->glDrawBuffer (GL_BACK);
    }

  /* NB: unlike glXSwapBuffers, glXCopySubBuffer and
   * glBlitFramebuffer don't issue an implicit glFlush() so we
   * have to flush ourselves if we want the request to complete in
   * a finite amount of time since otherwise the driver can batch
   * the command indefinitely. */
  context->glFlush ();

  /* NB: It's important we save the counter we read before acting on
   * the swap request since if we are mixing and matching different
   * swap methods between frames we don't want to read the timer e.g.
   * after calling glFinish() some times and not for others.
   *
   * In other words; this way we consistently save the time at the end
   * of the applications frame such that the counter isn't muddled by
   * the varying costs of different swap methods.
   */
  if (have_counter)
    glx_onscreen->last_swap_vsync_counter = end_frame_vsync_counter;
}

static void
_cogl_winsys_onscreen_swap_buffers (CoglOnscreen *onscreen)
{
  CoglFramebuffer *framebuffer = COGL_FRAMEBUFFER (onscreen);
  CoglContext *context = framebuffer->context;
  CoglXlibRenderer *xlib_renderer = context->display->renderer->winsys;
  CoglGLXRenderer *glx_renderer = context->display->renderer->winsys;
  CoglOnscreenXlib *xlib_onscreen = onscreen->winsys;
  CoglOnscreenGLX *glx_onscreen = onscreen->winsys;
  gboolean have_counter;
  GLXDrawable drawable;

  /* XXX: theoretically this shouldn't be necessary but at least with
   * the Intel drivers we have see that if we don't call
   * glXMakeContextCurrent for the drawable we are swapping then
   * we get a BadDrawable error from the X server. */
  _cogl_framebuffer_flush_state (framebuffer,
                                 framebuffer,
                                 COGL_FRAMEBUFFER_FLUSH_BIND_ONLY);

  drawable = glx_onscreen->glxwin ? glx_onscreen->glxwin : xlib_onscreen->xwin;

  if (onscreen->swap_throttled)
    {
      guint32 end_frame_vsync_counter = 0;

      have_counter =
        _cogl_winsys_has_feature (COGL_WINSYS_FEATURE_VBLANK_COUNTER);

      /* If the swap_region API is also being used then we need to track
       * the vsync counter for each swap request so we can manually
       * throttle swap_region requests. */
      if (have_counter)
        end_frame_vsync_counter = _cogl_winsys_get_vsync_counter ();

      if (!glx_renderer->pf_glXSwapInterval)
        {
          gboolean can_wait =
            _cogl_winsys_has_feature (COGL_WINSYS_FEATURE_VBLANK_WAIT);

          /* If we are going to wait for VBLANK manually, we not only
           * need to flush out pending drawing to the GPU before we
           * sleep, we need to wait for it to finish. Otherwise, we
           * may end up with the situation:
           *
           *        - We finish drawing      - GPU drawing continues
           *        - We go to sleep         - GPU drawing continues
           * VBLANK - We call glXSwapBuffers - GPU drawing continues
           *                                 - GPU drawing continues
           *                                 - Swap buffers happens
           *
           * Producing a tear. Calling glFinish() first will cause us
           * to properly wait for the next VBLANK before we swap. This
           * obviously does not happen when we use _GLX_SWAP and let
           * the driver do the right thing
           */
          context->glFinish ();

          if (have_counter && can_wait)
            {
              if (glx_onscreen->last_swap_vsync_counter ==
                  end_frame_vsync_counter)
                _cogl_winsys_wait_for_vblank ();
            }
          else if (can_wait)
            _cogl_winsys_wait_for_vblank ();
        }
    }
  else
    have_counter = FALSE;

  glx_renderer->glXSwapBuffers (xlib_renderer->xdpy, drawable);

  if (have_counter)
    glx_onscreen->last_swap_vsync_counter = _cogl_winsys_get_vsync_counter ();
}

static guint32
_cogl_winsys_onscreen_x11_get_window_xid (CoglOnscreen *onscreen)
{
  CoglOnscreenXlib *xlib_onscreen = onscreen->winsys;
  return xlib_onscreen->xwin;
}

static unsigned int
_cogl_winsys_onscreen_add_swap_buffers_callback (CoglOnscreen *onscreen,
                                                 CoglSwapBuffersNotify callback,
                                                 void *user_data)
{
  CoglOnscreenGLX *glx_onscreen = onscreen->winsys;
  CoglSwapBuffersNotifyEntry *entry = g_slice_new0 (CoglSwapBuffersNotifyEntry);
  static int next_swap_buffers_callback_id = 0;

  entry->callback = callback;
  entry->user_data = user_data;
  entry->id = next_swap_buffers_callback_id++;

  glx_onscreen->swap_callbacks =
    g_list_prepend (glx_onscreen->swap_callbacks, entry);

  return entry->id;
}

static void
_cogl_winsys_onscreen_remove_swap_buffers_callback (CoglOnscreen *onscreen,
                                                    unsigned int id)
{
  CoglOnscreenGLX *glx_onscreen = onscreen->winsys;
  GList *l;

  for (l = glx_onscreen->swap_callbacks; l; l = l->next)
    {
      CoglSwapBuffersNotifyEntry *entry = l->data;
      if (entry->id == id)
        {
          g_slice_free (CoglSwapBuffersNotifyEntry, entry);
          glx_onscreen->swap_callbacks =
            g_list_delete_link (glx_onscreen->swap_callbacks, l);
          return;
        }
    }
}

static void
_cogl_winsys_onscreen_update_swap_throttled (CoglOnscreen *onscreen)
{
  CoglContext *context = COGL_FRAMEBUFFER (onscreen)->context;
  CoglContextGLX *glx_context = context->winsys;
  CoglOnscreenGLX *glx_onscreen = onscreen->winsys;
  CoglOnscreenXlib *xlib_onscreen = onscreen->winsys;
  GLXDrawable drawable =
    glx_onscreen->glxwin ? glx_onscreen->glxwin : xlib_onscreen->xwin;

  if (glx_context->current_drawable != drawable)
    return;

  glx_context->current_drawable = 0;
  _cogl_winsys_onscreen_bind (onscreen);
}

static void
_cogl_winsys_onscreen_set_visibility (CoglOnscreen *onscreen,
                                      gboolean visibility)
{
  CoglContext *context = COGL_FRAMEBUFFER (onscreen)->context;
  CoglXlibRenderer *xlib_renderer = context->display->renderer->winsys;
  CoglOnscreenXlib *xlib_onscreen = onscreen->winsys;

  if (visibility)
    XMapWindow (xlib_renderer->xdpy, xlib_onscreen->xwin);
  else
    XUnmapWindow (xlib_renderer->xdpy, xlib_onscreen->xwin);
}

/* XXX: This is a particularly hacky _cogl_winsys interface... */
static XVisualInfo *
_cogl_winsys_xlib_get_visual_info (void)
{
  CoglGLXDisplay *glx_display;
  CoglXlibRenderer *xlib_renderer;
  CoglGLXRenderer *glx_renderer;

  _COGL_GET_CONTEXT (ctx, NULL);

  g_return_val_if_fail (ctx->display->winsys, FALSE);

  glx_display = ctx->display->winsys;
  xlib_renderer = ctx->display->renderer->winsys;
  glx_renderer = ctx->display->renderer->winsys;

  if (!glx_display->found_fbconfig)
    return NULL;

  return glx_renderer->glXGetVisualFromFBConfig (xlib_renderer->xdpy,
                                                 glx_display->fbconfig);
}

static gboolean
get_fbconfig_for_depth (CoglContext *context,
                        unsigned int depth,
                        GLXFBConfig *fbconfig_ret,
                        gboolean *can_mipmap_ret)
{
  CoglXlibRenderer *xlib_renderer;
  CoglGLXRenderer *glx_renderer;
  CoglGLXDisplay *glx_display;
  Display *dpy;
  GLXFBConfig *fbconfigs;
  int n_elements, i;
  int db, stencil, alpha, mipmap, rgba, value;
  int spare_cache_slot = 0;
  gboolean found = FALSE;

  xlib_renderer = context->display->renderer->winsys;
  glx_renderer = context->display->renderer->winsys;
  glx_display = context->display->winsys;

  /* Check if we've already got a cached config for this depth */
  for (i = 0; i < COGL_GLX_N_CACHED_CONFIGS; i++)
    if (glx_display->glx_cached_configs[i].depth == -1)
      spare_cache_slot = i;
    else if (glx_display->glx_cached_configs[i].depth == depth)
      {
        *fbconfig_ret = glx_display->glx_cached_configs[i].fb_config;
        *can_mipmap_ret = glx_display->glx_cached_configs[i].can_mipmap;
        return glx_display->glx_cached_configs[i].found;
      }

  dpy = xlib_renderer->xdpy;

  fbconfigs = glx_renderer->glXGetFBConfigs (dpy, DefaultScreen (dpy),
                                             &n_elements);

  db = G_MAXSHORT;
  stencil = G_MAXSHORT;
  mipmap = 0;
  rgba = 0;

  for (i = 0; i < n_elements; i++)
    {
      XVisualInfo *vi;
      int visual_depth;

      vi = glx_renderer->glXGetVisualFromFBConfig (dpy, fbconfigs[i]);
      if (vi == NULL)
        continue;

      visual_depth = vi->depth;

      XFree (vi);

      if (visual_depth != depth)
        continue;

      glx_renderer->glXGetFBConfigAttrib (dpy,
                                          fbconfigs[i],
                                          GLX_ALPHA_SIZE,
                                          &alpha);
      glx_renderer->glXGetFBConfigAttrib (dpy,
                                          fbconfigs[i],
                                          GLX_BUFFER_SIZE,
                                          &value);
      if (value != depth && (value - alpha) != depth)
        continue;

      value = 0;
      if (depth == 32)
        {
          glx_renderer->glXGetFBConfigAttrib (dpy,
                                              fbconfigs[i],
                                              GLX_BIND_TO_TEXTURE_RGBA_EXT,
                                              &value);
          if (value)
            rgba = 1;
        }

      if (!value)
        {
          if (rgba)
            continue;

          glx_renderer->glXGetFBConfigAttrib (dpy,
                                              fbconfigs[i],
                                              GLX_BIND_TO_TEXTURE_RGB_EXT,
                                              &value);
          if (!value)
            continue;
        }

      glx_renderer->glXGetFBConfigAttrib (dpy,
                                          fbconfigs[i],
                                          GLX_DOUBLEBUFFER,
                                          &value);
      if (value > db)
        continue;

      db = value;

      glx_renderer->glXGetFBConfigAttrib (dpy,
                                          fbconfigs[i],
                                          GLX_STENCIL_SIZE,
                                          &value);
      if (value > stencil)
        continue;

      stencil = value;

      /* glGenerateMipmap is defined in the offscreen extension */
      if (cogl_features_available (COGL_FEATURE_OFFSCREEN))
        {
          glx_renderer->glXGetFBConfigAttrib (dpy,
                                              fbconfigs[i],
                                              GLX_BIND_TO_MIPMAP_TEXTURE_EXT,
                                              &value);

          if (value < mipmap)
            continue;

          mipmap =  value;
        }

      *fbconfig_ret = fbconfigs[i];
      *can_mipmap_ret = mipmap;
      found = TRUE;
    }

  if (n_elements)
    XFree (fbconfigs);

  glx_display->glx_cached_configs[spare_cache_slot].depth = depth;
  glx_display->glx_cached_configs[spare_cache_slot].found = found;
  glx_display->glx_cached_configs[spare_cache_slot].fb_config = *fbconfig_ret;
  glx_display->glx_cached_configs[spare_cache_slot].can_mipmap = mipmap;

  return found;
}

static gboolean
should_use_rectangle (CoglContext *context)
{

  if (context->rectangle_state == COGL_WINSYS_RECTANGLE_STATE_UNKNOWN)
    {
      if (cogl_features_available (COGL_FEATURE_TEXTURE_RECTANGLE))
        {
          const char *rect_env;

          /* Use the rectangle only if it is available and either:

             the COGL_PIXMAP_TEXTURE_RECTANGLE environment variable is
             set to 'force'

             *or*

             the env var is set to 'allow' or not set and NPOTs textures
             are not available */

          context->rectangle_state =
            cogl_features_available (COGL_FEATURE_TEXTURE_NPOT) ?
            COGL_WINSYS_RECTANGLE_STATE_DISABLE :
            COGL_WINSYS_RECTANGLE_STATE_ENABLE;

          if ((rect_env = g_getenv ("COGL_PIXMAP_TEXTURE_RECTANGLE")) ||
              /* For compatibility, we'll also look at the old Clutter
                 environment variable */
              (rect_env = g_getenv ("CLUTTER_PIXMAP_TEXTURE_RECTANGLE")))
            {
              if (g_ascii_strcasecmp (rect_env, "force") == 0)
                context->rectangle_state =
                  COGL_WINSYS_RECTANGLE_STATE_ENABLE;
              else if (g_ascii_strcasecmp (rect_env, "disable") == 0)
                context->rectangle_state =
                  COGL_WINSYS_RECTANGLE_STATE_DISABLE;
              else if (g_ascii_strcasecmp (rect_env, "allow"))
                g_warning ("Unknown value for COGL_PIXMAP_TEXTURE_RECTANGLE, "
                           "should be 'force' or 'disable'");
            }
        }
      else
        context->rectangle_state = COGL_WINSYS_RECTANGLE_STATE_DISABLE;
    }

  return context->rectangle_state == COGL_WINSYS_RECTANGLE_STATE_ENABLE;
}

/* GCC's population count builtin is available since version 3.4 */
#if __GNUC__ > 3 || (__GNUC__ == 3 && __GNUC_MINOR__ >= 4)
#define POPCOUNTL(n) __builtin_popcountl(n)
#else
/* HAKMEM 169 */
static int
hakmem_popcountl (unsigned long n)
{
  unsigned long tmp;

  tmp = n - ((n >> 1) & 033333333333) - ((n >> 2) & 011111111111);
  return ((tmp + (tmp >> 3)) & 030707070707) % 63;
}
#define POPCOUNTL(n) hakmem_popcountl(n)
#endif

static gboolean
try_create_glx_pixmap (CoglContext *context,
                       CoglTexturePixmapX11 *tex_pixmap,
                       gboolean mipmap)
{
  CoglTexturePixmapGLX *glx_tex_pixmap = tex_pixmap->winsys;
  CoglRenderer *renderer;
  CoglXlibRenderer *xlib_renderer;
  CoglGLXRenderer *glx_renderer;
  Display *dpy;
  /* We have to initialize this *opaque* variable because gcc tries to
   * be too smart for its own good and warns that the variable may be
   * used uninitialized otherwise. */
  GLXFBConfig fb_config = (GLXFBConfig)0;
  int attribs[7];
  int i = 0;
  GLenum target;
  CoglXlibTrapState trap_state;

  unsigned int depth = tex_pixmap->depth;
  Visual* visual = tex_pixmap->visual;

  renderer = context->display->renderer;
  xlib_renderer = renderer->winsys;
  glx_renderer = renderer->winsys;
  dpy = xlib_renderer->xdpy;

  if (!get_fbconfig_for_depth (context, depth, &fb_config,
                               &glx_tex_pixmap->can_mipmap))
    {
      COGL_NOTE (TEXTURE_PIXMAP, "No suitable FBConfig found for depth %i",
                 depth);
      return FALSE;
    }

  if (should_use_rectangle (context))
    {
      target = GLX_TEXTURE_RECTANGLE_EXT;
      glx_tex_pixmap->can_mipmap = FALSE;
    }
  else
    target = GLX_TEXTURE_2D_EXT;

  if (!glx_tex_pixmap->can_mipmap)
    mipmap = FALSE;

  attribs[i++] = GLX_TEXTURE_FORMAT_EXT;

  /* Check whether an alpha channel is used by comparing the total
   * number of 1-bits in color masks against the color depth requested
   * by the client.
   */
  if (POPCOUNTL(visual->red_mask|visual->green_mask|visual->blue_mask) == depth)
    attribs[i++] = GLX_TEXTURE_FORMAT_RGB_EXT;
  else
    attribs[i++] = GLX_TEXTURE_FORMAT_RGBA_EXT;

  attribs[i++] = GLX_MIPMAP_TEXTURE_EXT;
  attribs[i++] = mipmap;

  attribs[i++] = GLX_TEXTURE_TARGET_EXT;
  attribs[i++] = target;

  attribs[i++] = None;

  /* We need to trap errors from glXCreatePixmap because it can
   * sometimes fail during normal usage. For example on NVidia it gets
   * upset if you try to create two GLXPixmaps for the same drawable.
   */

  _cogl_xlib_renderer_trap_errors (renderer, &trap_state);

  glx_tex_pixmap->glx_pixmap =
    glx_renderer->glXCreatePixmap (dpy,
                                   fb_config,
                                   tex_pixmap->pixmap,
                                   attribs);
  glx_tex_pixmap->has_mipmap_space = mipmap;

  XSync (dpy, False);

  if (_cogl_xlib_renderer_untrap_errors (renderer, &trap_state))
    {
      COGL_NOTE (TEXTURE_PIXMAP, "Failed to create pixmap for %p", tex_pixmap);
      _cogl_xlib_renderer_trap_errors (renderer, &trap_state);
      glx_renderer->glXDestroyPixmap (dpy, glx_tex_pixmap->glx_pixmap);
      XSync (dpy, False);
      _cogl_xlib_renderer_untrap_errors (renderer, &trap_state);

      glx_tex_pixmap->glx_pixmap = None;
      return FALSE;
    }

  return TRUE;
}

static gboolean
_cogl_winsys_texture_pixmap_x11_create (CoglTexturePixmapX11 *tex_pixmap)
{
  CoglTexturePixmapGLX *glx_tex_pixmap;

  /* FIXME: It should be possible to get to a CoglContext from any
   * CoglTexture pointer. */
  _COGL_GET_CONTEXT (ctx, FALSE);

  if (!_cogl_winsys_has_feature (COGL_WINSYS_FEATURE_TEXTURE_FROM_PIXMAP))
    {
      tex_pixmap->winsys = NULL;
      return FALSE;
    }

  glx_tex_pixmap = g_new0 (CoglTexturePixmapGLX, 1);

  glx_tex_pixmap->glx_pixmap = None;
  glx_tex_pixmap->can_mipmap = FALSE;
  glx_tex_pixmap->has_mipmap_space = FALSE;

  glx_tex_pixmap->glx_tex = COGL_INVALID_HANDLE;

  glx_tex_pixmap->bind_tex_image_queued = TRUE;
  glx_tex_pixmap->pixmap_bound = FALSE;

  tex_pixmap->winsys = glx_tex_pixmap;

  if (!try_create_glx_pixmap (ctx, tex_pixmap, FALSE))
    {
      tex_pixmap->winsys = NULL;
      g_free (glx_tex_pixmap);
      return FALSE;
    }

  return TRUE;
}

static void
free_glx_pixmap (CoglContext *context,
                 CoglTexturePixmapGLX *glx_tex_pixmap)
{
  CoglXlibTrapState trap_state;
  CoglRenderer *renderer;
  CoglXlibRenderer *xlib_renderer;
  CoglGLXRenderer *glx_renderer;

  renderer = context->display->renderer;
  xlib_renderer = renderer->winsys;
  glx_renderer = renderer->winsys;

  if (glx_tex_pixmap->pixmap_bound)
    glx_renderer->pf_glXReleaseTexImage (xlib_renderer->xdpy,
                                         glx_tex_pixmap->glx_pixmap,
                                         GLX_FRONT_LEFT_EXT);

  /* FIXME - we need to trap errors and synchronize here because
   * of ordering issues between the XPixmap destruction and the
   * GLXPixmap destruction.
   *
   * If the X pixmap is destroyed, the GLX pixmap is destroyed as
   * well immediately, and thus, when Cogl calls glXDestroyPixmap()
   * it'll cause a BadDrawable error.
   *
   * this is technically a bug in the X server, which should not
   * destroy either pixmaps until the call to glXDestroyPixmap(); so
   * at some point we should revisit this code and remove the
   * trap+sync after verifying that the destruction is indeed safe.
   *
   * for reference, see:
   *   http://bugzilla.clutter-project.org/show_bug.cgi?id=2324
   */
  _cogl_xlib_renderer_trap_errors (renderer, &trap_state);
  glx_renderer->glXDestroyPixmap (xlib_renderer->xdpy,
                                  glx_tex_pixmap->glx_pixmap);
  XSync (xlib_renderer->xdpy, False);
  _cogl_xlib_renderer_untrap_errors (renderer, &trap_state);

  glx_tex_pixmap->glx_pixmap = None;
  glx_tex_pixmap->pixmap_bound = FALSE;
}

static void
_cogl_winsys_texture_pixmap_x11_free (CoglTexturePixmapX11 *tex_pixmap)
{
  CoglTexturePixmapGLX *glx_tex_pixmap;

  /* FIXME: It should be possible to get to a CoglContext from any
   * CoglTexture pointer. */
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  if (!tex_pixmap->winsys)
    return;

  glx_tex_pixmap = tex_pixmap->winsys;

  free_glx_pixmap (ctx, glx_tex_pixmap);

  if (glx_tex_pixmap->glx_tex)
    cogl_handle_unref (glx_tex_pixmap->glx_tex);

  tex_pixmap->winsys = NULL;
  g_free (glx_tex_pixmap);
}

static gboolean
_cogl_winsys_texture_pixmap_x11_update (CoglTexturePixmapX11 *tex_pixmap,
                                        gboolean needs_mipmap)
{
  CoglTexturePixmapGLX *glx_tex_pixmap = tex_pixmap->winsys;
  CoglGLXRenderer *glx_renderer;

  /* FIXME: It should be possible to get to a CoglContext from any CoglTexture
   * pointer. */
  _COGL_GET_CONTEXT (ctx, FALSE);

  /* If we don't have a GLX pixmap then fallback */
  if (glx_tex_pixmap->glx_pixmap == None)
    return FALSE;

  glx_renderer = ctx->display->renderer->winsys;

  /* Lazily create a texture to hold the pixmap */
  if (glx_tex_pixmap->glx_tex == COGL_INVALID_HANDLE)
    {
      CoglPixelFormat texture_format;

      texture_format = (tex_pixmap->depth >= 32 ?
                        COGL_PIXEL_FORMAT_RGBA_8888_PRE :
                        COGL_PIXEL_FORMAT_RGB_888);

      if (should_use_rectangle (ctx))
        {
          glx_tex_pixmap->glx_tex =
            _cogl_texture_rectangle_new_with_size (tex_pixmap->width,
                                                   tex_pixmap->height,
                                                   COGL_TEXTURE_NO_ATLAS,
                                                   texture_format);

          if (glx_tex_pixmap->glx_tex)
            COGL_NOTE (TEXTURE_PIXMAP, "Created a texture rectangle for %p",
                       tex_pixmap);
          else
            {
              COGL_NOTE (TEXTURE_PIXMAP, "Falling back for %p because a "
                         "texture rectangle could not be created",
                         tex_pixmap);
              free_glx_pixmap (ctx, glx_tex_pixmap);
              return FALSE;
            }
        }
      else
        {
          glx_tex_pixmap->glx_tex =
            cogl_texture_2d_new_with_size (ctx,
                                           tex_pixmap->width,
                                           tex_pixmap->height,
                                           texture_format,
                                           NULL);

          if (glx_tex_pixmap->glx_tex)
            COGL_NOTE (TEXTURE_PIXMAP, "Created a texture 2d for %p",
                       tex_pixmap);
          else
            {
              COGL_NOTE (TEXTURE_PIXMAP, "Falling back for %p because a "
                         "texture 2d could not be created",
                         tex_pixmap);
              free_glx_pixmap (ctx, glx_tex_pixmap);
              return FALSE;
            }
        }
    }

  if (needs_mipmap)
    {
      /* If we can't support mipmapping then temporarily fallback */
      if (!glx_tex_pixmap->can_mipmap)
        return FALSE;

      /* Recreate the GLXPixmap if it wasn't previously created with a
       * mipmap tree */
      if (!glx_tex_pixmap->has_mipmap_space)
        {
          free_glx_pixmap (ctx, glx_tex_pixmap);

          COGL_NOTE (TEXTURE_PIXMAP, "Recreating GLXPixmap with mipmap "
                     "support for %p", tex_pixmap);
          if (!try_create_glx_pixmap (ctx, tex_pixmap, TRUE))

            {
              /* If the pixmap failed then we'll permanently fallback
               * to using XImage. This shouldn't happen. */
              COGL_NOTE (TEXTURE_PIXMAP, "Falling back to XGetImage "
                         "updates for %p because creating the GLXPixmap "
                         "with mipmap support failed", tex_pixmap);

              if (glx_tex_pixmap->glx_tex)
                cogl_handle_unref (glx_tex_pixmap->glx_tex);
              return FALSE;
            }

          glx_tex_pixmap->bind_tex_image_queued = TRUE;
        }
    }

  if (glx_tex_pixmap->bind_tex_image_queued)
    {
      GLuint gl_handle, gl_target;
      CoglXlibRenderer *xlib_renderer = ctx->display->renderer->winsys;

      cogl_texture_get_gl_texture (glx_tex_pixmap->glx_tex,
                                   &gl_handle, &gl_target);

      COGL_NOTE (TEXTURE_PIXMAP, "Rebinding GLXPixmap for %p", tex_pixmap);

      _cogl_bind_gl_texture_transient (gl_target, gl_handle, FALSE);

      if (glx_tex_pixmap->pixmap_bound)
        glx_renderer->pf_glXReleaseTexImage (xlib_renderer->xdpy,
                                             glx_tex_pixmap->glx_pixmap,
                                             GLX_FRONT_LEFT_EXT);

      glx_renderer->pf_glXBindTexImage (xlib_renderer->xdpy,
                                        glx_tex_pixmap->glx_pixmap,
                                        GLX_FRONT_LEFT_EXT,
                                        NULL);

      /* According to the recommended usage in the spec for
       * GLX_EXT_texture_pixmap we should release the texture after
       * we've finished drawing with it and it is undefined what
       * happens if you render to a pixmap that is bound to a texture.
       * However that would require the texture backend to know when
       * Cogl has finished painting and it may be more expensive to
       * keep unbinding the texture. Leaving it bound appears to work
       * on Mesa and NVidia drivers and it is also what Compiz does so
       * it is probably ok */

      glx_tex_pixmap->bind_tex_image_queued = FALSE;
      glx_tex_pixmap->pixmap_bound = TRUE;

      _cogl_texture_2d_externally_modified (glx_tex_pixmap->glx_tex);
    }

  return TRUE;
}

static void
_cogl_winsys_texture_pixmap_x11_damage_notify (CoglTexturePixmapX11 *tex_pixmap)
{
  CoglTexturePixmapGLX *glx_tex_pixmap = tex_pixmap->winsys;

  glx_tex_pixmap->bind_tex_image_queued = TRUE;
}

static CoglHandle
_cogl_winsys_texture_pixmap_x11_get_texture (CoglTexturePixmapX11 *tex_pixmap)
{
  CoglTexturePixmapGLX *glx_tex_pixmap = tex_pixmap->winsys;

  return glx_tex_pixmap->glx_tex;
}


static CoglWinsysVtable _cogl_winsys_vtable =
  {
    .id = COGL_WINSYS_ID_GLX,
    .name = "GLX",
    .renderer_get_proc_address = _cogl_winsys_renderer_get_proc_address,
    .renderer_connect = _cogl_winsys_renderer_connect,
    .renderer_disconnect = _cogl_winsys_renderer_disconnect,
    .display_setup = _cogl_winsys_display_setup,
    .display_destroy = _cogl_winsys_display_destroy,
    .context_init = _cogl_winsys_context_init,
    .context_deinit = _cogl_winsys_context_deinit,
    .xlib_get_visual_info = _cogl_winsys_xlib_get_visual_info,
    .onscreen_init = _cogl_winsys_onscreen_init,
    .onscreen_deinit = _cogl_winsys_onscreen_deinit,
    .onscreen_bind = _cogl_winsys_onscreen_bind,
    .onscreen_swap_buffers = _cogl_winsys_onscreen_swap_buffers,
    .onscreen_swap_region = _cogl_winsys_onscreen_swap_region,
    .onscreen_update_swap_throttled =
      _cogl_winsys_onscreen_update_swap_throttled,
    .onscreen_x11_get_window_xid =
      _cogl_winsys_onscreen_x11_get_window_xid,
    .onscreen_add_swap_buffers_callback =
      _cogl_winsys_onscreen_add_swap_buffers_callback,
    .onscreen_remove_swap_buffers_callback =
      _cogl_winsys_onscreen_remove_swap_buffers_callback,
    .onscreen_set_visibility = _cogl_winsys_onscreen_set_visibility,

    /* X11 tfp support... */
    /* XXX: instead of having a rather monolithic winsys vtable we could
     * perhaps look for a way to separate these... */
    .texture_pixmap_x11_create =
      _cogl_winsys_texture_pixmap_x11_create,
    .texture_pixmap_x11_free =
      _cogl_winsys_texture_pixmap_x11_free,
    .texture_pixmap_x11_update =
      _cogl_winsys_texture_pixmap_x11_update,
    .texture_pixmap_x11_damage_notify =
      _cogl_winsys_texture_pixmap_x11_damage_notify,
    .texture_pixmap_x11_get_texture =
      _cogl_winsys_texture_pixmap_x11_get_texture,
  };

/* XXX: we use a function because no doubt someone will complain
 * about using c99 member initializers because they aren't portable
 * to windows. We want to avoid having to rigidly follow the real
 * order of members since some members are #ifdefd and we'd have
 * to mirror the #ifdefing to add padding etc. For any winsys that
 * can assume the platform has a sane compiler then we can just use
 * c99 initializers for insane platforms they can initialize
 * the members by name in a function.
 */
const CoglWinsysVtable *
_cogl_winsys_glx_get_vtable (void)
{
  return &_cogl_winsys_vtable;
}
