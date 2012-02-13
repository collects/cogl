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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Authors:
 *   Robert Bragg <robert@linux.intel.com>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <string.h>

#include "cogl.h"
#include "cogl-util.h"
#include "cogl-internal.h"
#include "cogl-private.h"
#include "cogl-object.h"
#include "cogl-context-private.h"

#include "cogl-renderer.h"
#include "cogl-renderer-private.h"
#include "cogl-display-private.h"
#include "cogl-winsys-private.h"
#include "cogl-winsys-stub-private.h"
#include "cogl-config-private.h"

#ifdef COGL_HAS_EGL_PLATFORM_XLIB_SUPPORT
#include "cogl-winsys-egl-x11-private.h"
#endif
#ifdef COGL_HAS_EGL_PLATFORM_WAYLAND_SUPPORT
#include "cogl-winsys-egl-wayland-private.h"
#endif
#ifdef COGL_HAS_EGL_PLATFORM_KMS_SUPPORT
#include "cogl-winsys-egl-kms-private.h"
#endif
#ifdef COGL_HAS_EGL_PLATFORM_GDL_SUPPORT
#include "cogl-winsys-egl-gdl-private.h"
#endif
#ifdef COGL_HAS_EGL_PLATFORM_ANDROID_SUPPORT
#include "cogl-winsys-egl-android-private.h"
#endif
#ifdef COGL_HAS_EGL_PLATFORM_POWERVR_NULL_SUPPORT
#include "cogl-winsys-egl-null-private.h"
#endif

#if COGL_HAS_XLIB_SUPPORT
#include "cogl-xlib-renderer.h"
#endif

#ifdef COGL_HAS_GLX_SUPPORT
extern const CoglWinsysVtable *_cogl_winsys_glx_get_vtable (void);
#endif
#ifdef COGL_HAS_WGL_SUPPORT
extern const CoglWinsysVtable *_cogl_winsys_wgl_get_vtable (void);
#endif
#ifdef COGL_HAS_SDL_SUPPORT
extern const CoglWinsysVtable *_cogl_winsys_sdl_get_vtable (void);
#endif

typedef const CoglWinsysVtable *(*CoglWinsysVtableGetter) (void);

static CoglWinsysVtableGetter _cogl_winsys_vtable_getters[] =
{
#ifdef COGL_HAS_GLX_SUPPORT
  _cogl_winsys_glx_get_vtable,
#endif
#ifdef COGL_HAS_EGL_PLATFORM_XLIB_SUPPORT
  _cogl_winsys_egl_xlib_get_vtable,
#endif
#ifdef COGL_HAS_EGL_PLATFORM_WAYLAND_SUPPORT
  _cogl_winsys_egl_wayland_get_vtable,
#endif
#ifdef COGL_HAS_EGL_PLATFORM_KMS_SUPPORT
  _cogl_winsys_egl_kms_get_vtable,
#endif
#ifdef COGL_HAS_EGL_PLATFORM_GDL_SUPPORT
  _cogl_winsys_egl_gdl_get_vtable,
#endif
#ifdef COGL_HAS_EGL_PLATFORM_ANDROID_SUPPORT
  _cogl_winsys_egl_android_get_vtable,
#endif
#ifdef COGL_HAS_EGL_PLATFORM_POWERVR_NULL_SUPPORT
  _cogl_winsys_egl_null_get_vtable,
#endif
#ifdef COGL_HAS_WGL_SUPPORT
  _cogl_winsys_wgl_get_vtable,
#endif
#ifdef COGL_HAS_SDL_SUPPORT
  _cogl_winsys_sdl_get_vtable,
#endif
  _cogl_winsys_stub_get_vtable,
};

static void _cogl_renderer_free (CoglRenderer *renderer);

COGL_OBJECT_DEFINE (Renderer, renderer);

typedef struct _CoglNativeFilterClosure
{
  CoglNativeFilterFunc func;
  void *data;
} CoglNativeFilterClosure;

GQuark
cogl_renderer_error_quark (void)
{
  return g_quark_from_static_string ("cogl-renderer-error-quark");
}

static const CoglWinsysVtable *
_cogl_renderer_get_winsys (CoglRenderer *renderer)
{
  return renderer->winsys_vtable;
}

static void
native_filter_closure_free (CoglNativeFilterClosure *closure)
{
  g_slice_free (CoglNativeFilterClosure, closure);
}

static void
_cogl_renderer_free (CoglRenderer *renderer)
{
  const CoglWinsysVtable *winsys = _cogl_renderer_get_winsys (renderer);
  winsys->renderer_disconnect (renderer);

#ifndef HAVE_DIRECTLY_LINKED_GL_LIBRARY
  if (renderer->libgl_module)
    g_module_close (renderer->libgl_module);
#endif

  g_slist_foreach (renderer->event_filters,
                   (GFunc) native_filter_closure_free,
                   NULL);
  g_slist_free (renderer->event_filters);

  g_free (renderer);
}

CoglRenderer *
cogl_renderer_new (void)
{
  CoglRenderer *renderer = g_new0 (CoglRenderer, 1);

  _cogl_init ();

  renderer->connected = FALSE;
  renderer->event_filters = NULL;

#ifdef COGL_HAS_XLIB_SUPPORT
  renderer->xlib_enable_event_retrieval = TRUE;
#endif

  return _cogl_renderer_object_new (renderer);
}

#if COGL_HAS_XLIB_SUPPORT
void
cogl_xlib_renderer_set_foreign_display (CoglRenderer *renderer,
                                        Display *xdisplay)
{
  _COGL_RETURN_IF_FAIL (cogl_is_renderer (renderer));

  /* NB: Renderers are considered immutable once connected */
  _COGL_RETURN_IF_FAIL (!renderer->connected);

  renderer->foreign_xdpy = xdisplay;

  /* If the application is using a foreign display then we can assume
     it will also do its own event retrieval */
  cogl_xlib_renderer_set_event_retrieval_enabled (renderer, FALSE);
}

Display *
cogl_xlib_renderer_get_foreign_display (CoglRenderer *renderer)
{
  _COGL_RETURN_VAL_IF_FAIL (cogl_is_renderer (renderer), NULL);

  return renderer->foreign_xdpy;
}

void
cogl_xlib_renderer_set_event_retrieval_enabled (CoglRenderer *renderer,
                                                gboolean enable)
{
  _COGL_RETURN_IF_FAIL (cogl_is_renderer (renderer));
  /* NB: Renderers are considered immutable once connected */
  _COGL_RETURN_IF_FAIL (!renderer->connected);

  renderer->xlib_enable_event_retrieval = enable;
}
#endif /* COGL_HAS_XLIB_SUPPORT */

gboolean
cogl_renderer_check_onscreen_template (CoglRenderer *renderer,
                                       CoglOnscreenTemplate *onscreen_template,
                                       GError **error)
{
  CoglDisplay *display;

  if (!cogl_renderer_connect (renderer, error))
    return FALSE;

  display = cogl_display_new (renderer, onscreen_template);
  if (!cogl_display_setup (display, error))
    {
      cogl_object_unref (display);
      return FALSE;
    }

  cogl_object_unref (display);

  return TRUE;
}

static gboolean
_cogl_renderer_choose_driver (CoglRenderer *renderer,
                              GError **error)
{
  const char *driver_name = g_getenv ("COGL_DRIVER");
  const char *libgl_name;

  if (!driver_name)
    driver_name = _cogl_config_driver;

#ifdef HAVE_COGL_GL
  if (driver_name == NULL || !g_ascii_strcasecmp (driver_name, "gl"))
    {
      renderer->driver = COGL_DRIVER_GL;
      libgl_name = COGL_GL_LIBNAME;
      goto found;
    }
#endif

#ifdef HAVE_COGL_GLES2
  if (driver_name == NULL || !g_ascii_strcasecmp (driver_name, "gles2"))
    {
      renderer->driver = COGL_DRIVER_GLES2;
      libgl_name = COGL_GLES2_LIBNAME;
      goto found;
    }
#endif

#ifdef HAVE_COGL_GLES
  if (driver_name == NULL || !g_ascii_strcasecmp (driver_name, "gles1"))
    {
      renderer->driver = COGL_DRIVER_GLES1;
      libgl_name = COGL_GLES1_LIBNAME;
      goto found;
    }
#endif

  g_set_error (error,
               COGL_DRIVER_ERROR,
               COGL_DRIVER_ERROR_NO_SUITABLE_DRIVER_FOUND,
               "No suitable driver found");
  return FALSE;

 found:

#ifndef HAVE_DIRECTLY_LINKED_GL_LIBRARY

  renderer->libgl_module = g_module_open (libgl_name,
                                          G_MODULE_BIND_LAZY);

  if (renderer->libgl_module == NULL)
    {
      g_set_error (error, COGL_DRIVER_ERROR,
                   COGL_DRIVER_ERROR_FAILED_TO_LOAD_LIBRARY,
                   "Failed to dynamically open the GL library \"%s\"",
                   libgl_name);
      return FALSE;
    }

#endif /* HAVE_DIRECTLY_LINKED_GL_LIBRARY */

  return TRUE;
}

/* Final connection API */

gboolean
cogl_renderer_connect (CoglRenderer *renderer, GError **error)
{
  int i;
  GString *error_message;

  if (renderer->connected)
    return TRUE;

  /* The driver needs to be chosen before connecting the renderer
     because eglInitialize requires the library containing the GL API
     to be loaded before its called */
  if (!_cogl_renderer_choose_driver (renderer, error))
    return FALSE;

  error_message = g_string_new ("");
  for (i = 0; i < G_N_ELEMENTS (_cogl_winsys_vtable_getters); i++)
    {
      const CoglWinsysVtable *winsys = _cogl_winsys_vtable_getters[i]();
      GError *tmp_error = NULL;
      GList *l;
      gboolean constraints_failed = FALSE;

      if (renderer->winsys_id_override != COGL_WINSYS_ID_ANY)
        {
          if (renderer->winsys_id_override != winsys->id)
            continue;
        }
      else
        {
          char *user_choice = getenv ("COGL_RENDERER");
          if (!user_choice)
            user_choice = _cogl_config_renderer;
          if (user_choice &&
              g_ascii_strcasecmp (winsys->name, user_choice) != 0)
            continue;
        }

      for (l = renderer->constraints; l; l = l->next)
        {
          CoglRendererConstraint constraint = GPOINTER_TO_UINT (l->data);
          if (!(winsys->constraints & constraint))
            {
              constraints_failed = TRUE;
              break;
            }
        }
      if (constraints_failed)
        continue;

      /* At least temporarily we will associate this winsys with
       * the renderer in-case ->renderer_connect calls API that
       * wants to query the current winsys... */
      renderer->winsys_vtable = winsys;

      if (!winsys->renderer_connect (renderer, &tmp_error))
        {
          g_string_append_c (error_message, '\n');
          g_string_append (error_message, tmp_error->message);
          g_error_free (tmp_error);
        }
      else
        {
          renderer->connected = TRUE;
          g_string_free (error_message, TRUE);
          return TRUE;
        }
    }

  if (!renderer->connected)
    {
      renderer->winsys_vtable = NULL;
      g_set_error (error, COGL_WINSYS_ERROR,
                   COGL_WINSYS_ERROR_INIT,
                   "Failed to connected to any renderer: %s",
                   error_message->str);
      g_string_free (error_message, TRUE);
      return FALSE;
    }

  return TRUE;
}

CoglFilterReturn
_cogl_renderer_handle_native_event (CoglRenderer *renderer,
                                    void *event)
{
  GSList *l, *next;

  /* Pass the event on to all of the registered filters in turn */
  for (l = renderer->event_filters; l; l = next)
    {
      CoglNativeFilterClosure *closure = l->data;

      /* The next pointer is taken now so that we can handle the
         closure being removed during emission */
      next = l->next;

      if (closure->func (event, closure->data) == COGL_FILTER_REMOVE)
        return COGL_FILTER_REMOVE;
    }

  /* If the backend for the renderer also wants to see the events, it
     should just register its own filter */

  return COGL_FILTER_CONTINUE;
}

void
_cogl_renderer_add_native_filter (CoglRenderer *renderer,
                                  CoglNativeFilterFunc func,
                                  void *data)
{
  CoglNativeFilterClosure *closure;

  closure = g_slice_new (CoglNativeFilterClosure);
  closure->func = func;
  closure->data = data;

  renderer->event_filters = g_slist_prepend (renderer->event_filters, closure);
}

void
_cogl_renderer_remove_native_filter (CoglRenderer *renderer,
                                     CoglNativeFilterFunc func,
                                     void *data)
{
  GSList *l, *prev = NULL;

  for (l = renderer->event_filters; l; prev = l, l = l->next)
    {
      CoglNativeFilterClosure *closure = l->data;

      if (closure->func == func && closure->data == data)
        {
          native_filter_closure_free (closure);
          if (prev)
            prev->next = g_slist_delete_link (prev->next, l);
          else
            renderer->event_filters =
              g_slist_delete_link (renderer->event_filters, l);
          break;
        }
    }
}

void
cogl_renderer_set_winsys_id (CoglRenderer *renderer,
                             CoglWinsysID winsys_id)
{
  _COGL_RETURN_IF_FAIL (!renderer->connected);

  renderer->winsys_id_override = winsys_id;
}

CoglWinsysID
cogl_renderer_get_winsys_id (CoglRenderer *renderer)
{
  _COGL_RETURN_VAL_IF_FAIL (renderer->connected, 0);

  return renderer->winsys_vtable->id;
}

void *
_cogl_renderer_get_proc_address (CoglRenderer *renderer,
                                 const char *name)
{
  const CoglWinsysVtable *winsys = _cogl_renderer_get_winsys (renderer);

  return winsys->renderer_get_proc_address (renderer, name);
}

int
cogl_renderer_get_n_fragment_texture_units (CoglRenderer *renderer)
{
  int n = 0;

  _COGL_GET_CONTEXT (ctx, 0);

#if defined (HAVE_COGL_GL) || defined (HAVE_COGL_GLES2)
  if (ctx->driver == COGL_DRIVER_GL || ctx->driver == COGL_DRIVER_GLES2)
    GE (ctx, glGetIntegerv (GL_MAX_TEXTURE_IMAGE_UNITS, &n));
#endif

  return n;
}

void
cogl_renderer_add_constraint (CoglRenderer *renderer,
                              CoglRendererConstraint constraint)
{
  g_return_if_fail (!renderer->connected);
  renderer->constraints = g_list_prepend (renderer->constraints,
                                          GUINT_TO_POINTER (constraint));
}

void
cogl_renderer_remove_constraint (CoglRenderer *renderer,
                                 CoglRendererConstraint constraint)
{
  g_return_if_fail (!renderer->connected);
  renderer->constraints = g_list_remove (renderer->constraints,
                                         GUINT_TO_POINTER (constraint));
}

