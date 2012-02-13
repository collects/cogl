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
 *  Robert Bragg <robert@linux.intel.com>
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "cogl-renderer-private.h"
#include "cogl-display-private.h"
#include "cogl-context-private.h"
#include "cogl-framebuffer-private.h"
#include "cogl-private.h"

#include <string.h>

static int _cogl_winsys_stub_dummy_ptr;

/* This provides a NOP winsys. This can be useful for debugging or for
 * integrating with toolkits that already have window system
 * integration code.
 */

static CoglFuncPtr
_cogl_winsys_renderer_get_proc_address (CoglRenderer *renderer,
                                        const char *name)
{
  static GModule *module = NULL;

  /* this should find the right function if the program is linked against a
   * library providing it */
  if (G_UNLIKELY (module == NULL))
    module = g_module_open (NULL, 0);

  if (module)
    {
      void *symbol;

      if (g_module_symbol (module, name, &symbol))
        return symbol;
    }

  return NULL;
}

static void
_cogl_winsys_renderer_disconnect (CoglRenderer *renderer)
{
  renderer->winsys = NULL;
}

static gboolean
_cogl_winsys_renderer_connect (CoglRenderer *renderer,
                               GError **error)
{
  renderer->winsys = &_cogl_winsys_stub_dummy_ptr;
  return TRUE;
}

static void
_cogl_winsys_display_destroy (CoglDisplay *display)
{
  display->winsys = NULL;
}

static gboolean
_cogl_winsys_display_setup (CoglDisplay *display,
                            GError **error)
{
  display->winsys = &_cogl_winsys_stub_dummy_ptr;
  return TRUE;
}

static gboolean
_cogl_winsys_context_init (CoglContext *context, GError **error)
{
  context->winsys = &_cogl_winsys_stub_dummy_ptr;

  if (!_cogl_context_update_features (context, error))
    return FALSE;

  memset (context->winsys_features, 0, sizeof (context->winsys_features));

  return TRUE;
}

static void
_cogl_winsys_context_deinit (CoglContext *context)
{
  context->winsys = NULL;
}

static gboolean
_cogl_winsys_onscreen_init (CoglOnscreen *onscreen,
                            GError **error)
{
  return TRUE;
}

static void
_cogl_winsys_onscreen_deinit (CoglOnscreen *onscreen)
{
}

static void
_cogl_winsys_onscreen_bind (CoglOnscreen *onscreen)
{
}

static void
_cogl_winsys_onscreen_swap_buffers (CoglOnscreen *onscreen)
{
}

static void
_cogl_winsys_onscreen_update_swap_throttled (CoglOnscreen *onscreen)
{
}

static void
_cogl_winsys_onscreen_set_visibility (CoglOnscreen *onscreen,
                                      gboolean visibility)
{
}

const CoglWinsysVtable *
_cogl_winsys_stub_get_vtable (void)
{
  static gboolean vtable_inited = FALSE;
  static CoglWinsysVtable vtable;

  /* It would be nice if we could use C99 struct initializers here
     like the GLX backend does. However this code is more likely to be
     compiled using Visual Studio which (still!) doesn't support them
     so we initialize it in code instead */

  if (!vtable_inited)
    {
      memset (&vtable, 0, sizeof (vtable));

      vtable.id = COGL_WINSYS_ID_STUB;
      vtable.name = "STUB";
      vtable.renderer_get_proc_address = _cogl_winsys_renderer_get_proc_address;
      vtable.renderer_connect = _cogl_winsys_renderer_connect;
      vtable.renderer_disconnect = _cogl_winsys_renderer_disconnect;
      vtable.display_setup = _cogl_winsys_display_setup;
      vtable.display_destroy = _cogl_winsys_display_destroy;
      vtable.context_init = _cogl_winsys_context_init;
      vtable.context_deinit = _cogl_winsys_context_deinit;

      vtable.onscreen_init = _cogl_winsys_onscreen_init;
      vtable.onscreen_deinit = _cogl_winsys_onscreen_deinit;
      vtable.onscreen_bind = _cogl_winsys_onscreen_bind;
      vtable.onscreen_swap_buffers = _cogl_winsys_onscreen_swap_buffers;
      vtable.onscreen_update_swap_throttled =
        _cogl_winsys_onscreen_update_swap_throttled;
      vtable.onscreen_set_visibility = _cogl_winsys_onscreen_set_visibility;

      vtable_inited = TRUE;
    }

  return &vtable;
}
