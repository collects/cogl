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
 *
 * Authors:
 *   Neil Roberts <neil@linux.intel.com>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include "cogl-pipeline-private.h"

#ifdef COGL_PIPELINE_PROGEND_FIXED

#include "cogl-context.h"
#include "cogl-context-private.h"

static void
_cogl_pipeline_progend_fixed_pre_paint (CoglPipeline *pipeline)
{
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  if (pipeline->vertend != COGL_PIPELINE_VERTEND_FIXED)
    return;

  if (ctx->current_projection_stack)
    _cogl_matrix_stack_flush_to_gl_builtins (ctx,
                                             ctx->current_projection_stack,
                                             COGL_MATRIX_PROJECTION,
                                             FALSE /* enable flip */);
  if (ctx->current_modelview_stack)
    _cogl_matrix_stack_flush_to_gl_builtins (ctx,
                                             ctx->current_modelview_stack,
                                             COGL_MATRIX_MODELVIEW,
                                             FALSE /* enable flip */);
}

const CoglPipelineProgend _cogl_pipeline_fixed_progend =
  {
    NULL, /* end */
    NULL, /* pre_change_notify */
    NULL, /* layer_pre_change_notify */
    _cogl_pipeline_progend_fixed_pre_paint
  };

#endif /* COGL_PIPELINE_PROGEND_FIXED */
