/*
 * Cogl
 *
 * An object oriented GL/GLES Abstraction/Utility Layer
 *
 * Copyright (C) 2008,2009,2010 Intel Corporation.
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
 *   Robert Bragg <robert@linux.intel.com>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "cogl.h"

#include "cogl-debug.h"
#include "cogl-pipeline-opengl-private.h"
#include "cogl-pipeline-private.h"
#include "cogl-context-private.h"
#include "cogl-texture-private.h"
#include "cogl-framebuffer-private.h"

/* This is needed to set the color attribute on GLES2 */
#ifdef HAVE_COGL_GLES2
#include "cogl-pipeline-progend-glsl-private.h"
#endif

#include <glib.h>
#include <string.h>

/*
 * GL/GLES compatability defines for pipeline thingies:
 */

/* These aren't defined in the GLES headers */
#ifndef GL_POINT_SPRITE
#define GL_POINT_SPRITE 0x8861
#endif
#ifndef GL_COORD_REPLACE
#define GL_COORD_REPLACE 0x8862
#endif
#ifndef GL_CLAMP_TO_BORDER
#define GL_CLAMP_TO_BORDER 0x812d
#endif


static void
texture_unit_init (CoglTextureUnit *unit, int index_)
{
  unit->index = index_;
  unit->enabled_gl_target = 0;
  unit->gl_texture = 0;
  unit->gl_target = 0;
  unit->is_foreign = FALSE;
  unit->dirty_gl_texture = FALSE;
  unit->matrix_stack = _cogl_matrix_stack_new ();

  unit->layer = NULL;
  unit->layer_changes_since_flush = 0;
  unit->texture_storage_changed = FALSE;
}

static void
texture_unit_free (CoglTextureUnit *unit)
{
  if (unit->layer)
    cogl_object_unref (unit->layer);
  cogl_object_unref (unit->matrix_stack);
}

CoglTextureUnit *
_cogl_get_texture_unit (int index_)
{
  _COGL_GET_CONTEXT (ctx, NULL);

  if (ctx->texture_units->len < (index_ + 1))
    {
      int i;
      int prev_len = ctx->texture_units->len;
      ctx->texture_units = g_array_set_size (ctx->texture_units, index_ + 1);
      for (i = prev_len; i <= index_; i++)
        {
          CoglTextureUnit *unit =
            &g_array_index (ctx->texture_units, CoglTextureUnit, i);

          texture_unit_init (unit, i);
        }
    }

  return &g_array_index (ctx->texture_units, CoglTextureUnit, index_);
}

void
_cogl_destroy_texture_units (void)
{
  int i;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  for (i = 0; i < ctx->texture_units->len; i++)
    {
      CoglTextureUnit *unit =
        &g_array_index (ctx->texture_units, CoglTextureUnit, i);
      texture_unit_free (unit);
    }
  g_array_free (ctx->texture_units, TRUE);
}

void
_cogl_set_active_texture_unit (int unit_index)
{
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  if (ctx->active_texture_unit != unit_index)
    {
      GE (ctx, glActiveTexture (GL_TEXTURE0 + unit_index));
      ctx->active_texture_unit = unit_index;
    }
}

/* Note: _cogl_bind_gl_texture_transient conceptually has slightly
 * different semantics to OpenGL's glBindTexture because Cogl never
 * cares about tracking multiple textures bound to different targets
 * on the same texture unit.
 *
 * glBindTexture lets you bind multiple textures to a single texture
 * unit if they are bound to different targets. So it does something
 * like:
 *   unit->current_texture[target] = texture;
 *
 * Cogl only lets you associate one texture with the currently active
 * texture unit, so the target is basically a redundant parameter
 * that's implicitly set on that texture.
 *
 * Technically this is just a thin wrapper around glBindTexture so
 * actually it does have the GL semantics but it seems worth
 * mentioning the conceptual difference in case anyone wonders why we
 * don't associate the gl_texture with a gl_target in the
 * CoglTextureUnit.
 */
void
_cogl_bind_gl_texture_transient (GLenum gl_target,
                                 GLuint gl_texture,
                                 gboolean is_foreign)
{
  CoglTextureUnit *unit;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  /* We choose to always make texture unit 1 active for transient
   * binds so that in the common case where multitexturing isn't used
   * we can simply ignore the state of this texture unit. Notably we
   * didn't use a large texture unit (.e.g. (GL_MAX_TEXTURE_UNITS - 1)
   * in case the driver doesn't have a sparse data structure for
   * texture units.
   */
  _cogl_set_active_texture_unit (1);
  unit = _cogl_get_texture_unit (1);

  /* NB: If we have previously bound a foreign texture to this texture
   * unit we don't know if that texture has since been deleted and we
   * are seeing the texture name recycled */
  if (unit->gl_texture == gl_texture &&
      !unit->dirty_gl_texture &&
      !unit->is_foreign)
    return;

  GE (ctx, glBindTexture (gl_target, gl_texture));

  unit->dirty_gl_texture = TRUE;
  unit->is_foreign = is_foreign;
}

void
_cogl_delete_gl_texture (GLuint gl_texture)
{
  int i;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  for (i = 0; i < ctx->texture_units->len; i++)
    {
      CoglTextureUnit *unit =
        &g_array_index (ctx->texture_units, CoglTextureUnit, i);

      if (unit->gl_texture == gl_texture)
        {
          unit->gl_texture = 0;
          unit->gl_target = 0;
          unit->dirty_gl_texture = FALSE;
        }
    }

  GE (ctx, glDeleteTextures (1, &gl_texture));
}

/* Whenever the underlying GL texture storage of a CoglTexture is
 * changed (e.g. due to migration out of a texture atlas) then we are
 * notified. This lets us ensure that we reflush that texture's state
 * if it is reused again with the same texture unit.
 */
void
_cogl_pipeline_texture_storage_change_notify (CoglTexture *texture)
{
  int i;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  for (i = 0; i < ctx->texture_units->len; i++)
    {
      CoglTextureUnit *unit =
        &g_array_index (ctx->texture_units, CoglTextureUnit, i);

      if (unit->layer &&
          _cogl_pipeline_layer_get_texture (unit->layer) == texture)
        unit->texture_storage_changed = TRUE;

      /* NB: the texture may be bound to multiple texture units so
       * we continue to check the rest */
    }
}

static void
set_glsl_program (GLuint gl_program)
{
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  if (ctx->current_gl_program != gl_program)
    {
      GLenum gl_error;

      while ((gl_error = ctx->glGetError ()) != GL_NO_ERROR)
        ;
      ctx->glUseProgram (gl_program);
      if (ctx->glGetError () == GL_NO_ERROR)
        ctx->current_gl_program = gl_program;
      else
        {
          GE( ctx, glUseProgram (0) );
          ctx->current_gl_program = 0;
        }
    }
}

void
_cogl_use_fragment_program (GLuint gl_program, CoglPipelineProgramType type)
{
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  /* If we're changing program type... */
  if (type != ctx->current_fragment_program_type)
    {
      /* ... disable the old type */
      switch (ctx->current_fragment_program_type)
        {
        case COGL_PIPELINE_PROGRAM_TYPE_GLSL:
          /* If the program contains a vertex shader then we shouldn't
             disable it */
          if (ctx->current_vertex_program_type !=
              COGL_PIPELINE_PROGRAM_TYPE_GLSL)
            set_glsl_program (0);
          break;

        case COGL_PIPELINE_PROGRAM_TYPE_ARBFP:
#ifdef HAVE_COGL_GL
          GE( ctx, glDisable (GL_FRAGMENT_PROGRAM_ARB) );
#endif
          break;

        case COGL_PIPELINE_PROGRAM_TYPE_FIXED:
          /* don't need to to anything */
          break;
        }

      /* ... and enable the new type */
      switch (type)
        {
        case COGL_PIPELINE_PROGRAM_TYPE_ARBFP:
#ifdef HAVE_COGL_GL
          GE( ctx, glEnable (GL_FRAGMENT_PROGRAM_ARB) );
#endif
          break;

        case COGL_PIPELINE_PROGRAM_TYPE_GLSL:
        case COGL_PIPELINE_PROGRAM_TYPE_FIXED:
          /* don't need to to anything */
          break;
        }
    }

  if (type == COGL_PIPELINE_PROGRAM_TYPE_GLSL)
    {
#ifdef COGL_PIPELINE_FRAGEND_GLSL
      set_glsl_program (gl_program);

#else

      g_warning ("Unexpected use of GLSL fragend!");

#endif /* COGL_PIPELINE_FRAGEND_GLSL */
    }
#ifndef COGL_PIPELINE_FRAGEND_ARBFP
  else if (type == COGL_PIPELINE_PROGRAM_TYPE_ARBFP)
    g_warning ("Unexpected use of ARBFP fragend!");
#endif /* COGL_PIPELINE_FRAGEND_ARBFP */

  ctx->current_fragment_program_type = type;
}

void
_cogl_use_vertex_program (GLuint gl_program, CoglPipelineProgramType type)
{
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  /* If we're changing program type... */
  if (type != ctx->current_vertex_program_type)
    {
      /* ... disable the old type */
      switch (ctx->current_vertex_program_type)
        {
        case COGL_PIPELINE_PROGRAM_TYPE_GLSL:
          /* If the program contains a fragment shader then we shouldn't
             disable it */
          if (ctx->current_fragment_program_type !=
              COGL_PIPELINE_PROGRAM_TYPE_GLSL)
            set_glsl_program (0);
          break;

        case COGL_PIPELINE_PROGRAM_TYPE_ARBFP:
          /* It doesn't make sense to enable ARBfp for the vertex program */
          g_assert_not_reached ();
          break;

        case COGL_PIPELINE_PROGRAM_TYPE_FIXED:
          /* don't need to to anything */
          break;
        }

      /* ... and enable the new type */
      switch (type)
        {
        case COGL_PIPELINE_PROGRAM_TYPE_ARBFP:
          /* It doesn't make sense to enable ARBfp for the vertex program */
          g_assert_not_reached ();
          break;

        case COGL_PIPELINE_PROGRAM_TYPE_GLSL:
        case COGL_PIPELINE_PROGRAM_TYPE_FIXED:
          /* don't need to to anything */
          break;
        }
    }

  if (type == COGL_PIPELINE_PROGRAM_TYPE_GLSL)
    {
#ifdef COGL_PIPELINE_VERTEND_GLSL
      set_glsl_program (gl_program);

#else

      g_warning ("Unexpected use of GLSL vertend!");

#endif /* COGL_PIPELINE_VERTEND_GLSL */
    }
#ifndef COGL_PIPELINE_VERTEND_ARBFP
  else if (type == COGL_PIPELINE_PROGRAM_TYPE_ARBFP)
    g_warning ("Unexpected use of ARBFP vertend!");
#endif /* COGL_PIPELINE_VERTEND_ARBFP */

  ctx->current_vertex_program_type = type;
}

#if defined(HAVE_COGL_GLES2) || defined(HAVE_COGL_GL)

static gboolean
blend_factor_uses_constant (GLenum blend_factor)
{
  return (blend_factor == GL_CONSTANT_COLOR ||
          blend_factor == GL_ONE_MINUS_CONSTANT_COLOR ||
          blend_factor == GL_CONSTANT_ALPHA ||
          blend_factor == GL_ONE_MINUS_CONSTANT_ALPHA);
}

#endif

static void
flush_depth_state (CoglDepthState *depth_state)
{
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  if (ctx->depth_test_function_cache != depth_state->test_function)
    {
      GE (ctx, glDepthFunc (depth_state->test_function));
      ctx->depth_test_function_cache = depth_state->test_function;
    }

  if (ctx->depth_writing_enabled_cache != depth_state->write_enabled)
    {
      GE (ctx, glDepthMask (depth_state->write_enabled ?
                            GL_TRUE : GL_FALSE));
      ctx->depth_writing_enabled_cache = depth_state->write_enabled;
    }

  if (ctx->driver != COGL_DRIVER_GLES1 &&
      (ctx->depth_range_near_cache != depth_state->range_near ||
       ctx->depth_range_far_cache != depth_state->range_far))
    {
      if (ctx->driver == COGL_DRIVER_GLES2)
        GE (ctx, glDepthRangef (depth_state->range_near,
                                depth_state->range_far));
      else
        GE (ctx, glDepthRange (depth_state->range_near,
                               depth_state->range_far));

      ctx->depth_range_near_cache = depth_state->range_near;
      ctx->depth_range_far_cache = depth_state->range_far;
    }
}

static void
_cogl_pipeline_flush_color_blend_alpha_depth_state (
                                            CoglPipeline *pipeline,
                                            unsigned long pipelines_difference,
                                            gboolean      skip_gl_color)
{
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  /* On GLES2 we'll flush the color later */
  if (ctx->driver != COGL_DRIVER_GLES2 &&
      !skip_gl_color)
    {
      if ((pipelines_difference & COGL_PIPELINE_STATE_COLOR) ||
          /* Assume if we were previously told to skip the color, then
           * the current color needs updating... */
          ctx->current_pipeline_skip_gl_color)
        {
          CoglPipeline *authority =
            _cogl_pipeline_get_authority (pipeline, COGL_PIPELINE_STATE_COLOR);
          GE (ctx, glColor4ub (cogl_color_get_red_byte (&authority->color),
                               cogl_color_get_green_byte (&authority->color),
                               cogl_color_get_blue_byte (&authority->color),
                               cogl_color_get_alpha_byte (&authority->color)));
        }
    }

  if (pipelines_difference & COGL_PIPELINE_STATE_BLEND)
    {
      CoglPipeline *authority =
        _cogl_pipeline_get_authority (pipeline, COGL_PIPELINE_STATE_BLEND);
      CoglPipelineBlendState *blend_state =
        &authority->big_state->blend_state;

      /* GLES 1 only has glBlendFunc */
      if (ctx->driver == COGL_DRIVER_GLES1)
        {
          GE (ctx, glBlendFunc (blend_state->blend_src_factor_rgb,
                                blend_state->blend_dst_factor_rgb));
        }
#if defined(HAVE_COGL_GLES2) || defined(HAVE_COGL_GL)
      else
        {
          if (blend_factor_uses_constant (blend_state->blend_src_factor_rgb) ||
              blend_factor_uses_constant (blend_state
                                          ->blend_src_factor_alpha) ||
              blend_factor_uses_constant (blend_state->blend_dst_factor_rgb) ||
              blend_factor_uses_constant (blend_state->blend_dst_factor_alpha))
            {
              float red =
                cogl_color_get_red_float (&blend_state->blend_constant);
              float green =
                cogl_color_get_green_float (&blend_state->blend_constant);
              float blue =
                cogl_color_get_blue_float (&blend_state->blend_constant);
              float alpha =
                cogl_color_get_alpha_float (&blend_state->blend_constant);


              GE (ctx, glBlendColor (red, green, blue, alpha));
            }

          if (ctx->glBlendEquationSeparate &&
              blend_state->blend_equation_rgb !=
              blend_state->blend_equation_alpha)
            GE (ctx,
                glBlendEquationSeparate (blend_state->blend_equation_rgb,
                                         blend_state->blend_equation_alpha));
          else
            GE (ctx, glBlendEquation (blend_state->blend_equation_rgb));

          if (ctx->glBlendFuncSeparate &&
              (blend_state->blend_src_factor_rgb !=
               blend_state->blend_src_factor_alpha ||
               (blend_state->blend_src_factor_rgb !=
                blend_state->blend_src_factor_alpha)))
            GE (ctx, glBlendFuncSeparate (blend_state->blend_src_factor_rgb,
                                          blend_state->blend_dst_factor_rgb,
                                          blend_state->blend_src_factor_alpha,
                                          blend_state->blend_dst_factor_alpha));
          else
            GE (ctx, glBlendFunc (blend_state->blend_src_factor_rgb,
                                  blend_state->blend_dst_factor_rgb));
        }
#endif
    }

#if defined (HAVE_COGL_GL) || defined (HAVE_COGL_GLES)

  if (ctx->driver != COGL_DRIVER_GLES2)
    {
      /* Under GLES2 the alpha function is implemented as part of the
         fragment shader */
      if (pipelines_difference & (COGL_PIPELINE_STATE_ALPHA_FUNC |
                                  COGL_PIPELINE_STATE_ALPHA_FUNC_REFERENCE))
        {
          CoglPipeline *authority =
            _cogl_pipeline_get_authority (pipeline,
                                          COGL_PIPELINE_STATE_ALPHA_FUNC);
          CoglPipelineAlphaFuncState *alpha_state =
            &authority->big_state->alpha_state;

          /* NB: Currently the Cogl defines are compatible with the GL ones: */
          GE (ctx, glAlphaFunc (alpha_state->alpha_func,
                                alpha_state->alpha_func_reference));
        }

      /* Under GLES2 the lighting parameters are implemented as uniforms
         in the progend */
      if (pipelines_difference & COGL_PIPELINE_STATE_LIGHTING)
        {
          CoglPipeline *authority =
            _cogl_pipeline_get_authority (pipeline,
                                          COGL_PIPELINE_STATE_LIGHTING);
          CoglPipelineLightingState *lighting_state =
            &authority->big_state->lighting_state;

          GE (ctx, glMaterialfv (GL_FRONT_AND_BACK, GL_AMBIENT,
                                 lighting_state->ambient));
          GE (ctx, glMaterialfv (GL_FRONT_AND_BACK, GL_DIFFUSE,
                                 lighting_state->diffuse));
          GE (ctx, glMaterialfv (GL_FRONT_AND_BACK, GL_SPECULAR,
                                 lighting_state->specular));
          GE (ctx, glMaterialfv (GL_FRONT_AND_BACK, GL_EMISSION,
                                 lighting_state->emission));
          GE (ctx, glMaterialfv (GL_FRONT_AND_BACK, GL_SHININESS,
                                 &lighting_state->shininess));
        }
    }

#endif

  if (pipelines_difference & COGL_PIPELINE_STATE_DEPTH)
    {
      CoglPipeline *authority =
        _cogl_pipeline_get_authority (pipeline, COGL_PIPELINE_STATE_DEPTH);
      CoglDepthState *depth_state = &authority->big_state->depth_state;

      if (depth_state->test_enabled)
        {
          if (ctx->depth_test_enabled_cache != TRUE)
            {
              GE (ctx, glEnable (GL_DEPTH_TEST));
              ctx->depth_test_enabled_cache = depth_state->test_enabled;
            }
          flush_depth_state (depth_state);
        }
      else if (ctx->depth_test_enabled_cache != FALSE)
        {
          GE (ctx, glDisable (GL_DEPTH_TEST));
          ctx->depth_test_enabled_cache = depth_state->test_enabled;
        }
    }

  if (pipelines_difference & COGL_PIPELINE_STATE_LOGIC_OPS)
    {
      CoglPipeline *authority =
        _cogl_pipeline_get_authority (pipeline, COGL_PIPELINE_STATE_LOGIC_OPS);
      CoglPipelineLogicOpsState *logic_ops_state = &authority->big_state->logic_ops_state;
      CoglColorMask color_mask = logic_ops_state->color_mask;
      CoglFramebuffer *draw_framebuffer = cogl_get_draw_framebuffer ();

      if (draw_framebuffer)
        color_mask &= draw_framebuffer->color_mask;

      GE (ctx, glColorMask (!!(color_mask & COGL_COLOR_MASK_RED),
                            !!(color_mask & COGL_COLOR_MASK_GREEN),
                            !!(color_mask & COGL_COLOR_MASK_BLUE),
                            !!(color_mask & COGL_COLOR_MASK_ALPHA)));
      ctx->current_gl_color_mask = color_mask;
    }

  if (pipelines_difference & COGL_PIPELINE_STATE_CULL_FACE)
    {
      CoglPipeline *authority =
        _cogl_pipeline_get_authority (pipeline, COGL_PIPELINE_STATE_CULL_FACE);
      CoglPipelineCullFaceState *cull_face_state
        = &authority->big_state->cull_face_state;

      if (cull_face_state->mode == COGL_PIPELINE_CULL_FACE_MODE_NONE)
        GE( ctx, glDisable (GL_CULL_FACE) );
      else
        {
          CoglFramebuffer *draw_framebuffer = cogl_get_draw_framebuffer ();
          gboolean invert_winding;

          GE( ctx, glEnable (GL_CULL_FACE) );

          switch (cull_face_state->mode)
            {
            case COGL_PIPELINE_CULL_FACE_MODE_NONE:
              g_assert_not_reached ();

            case COGL_PIPELINE_CULL_FACE_MODE_FRONT:
              GE( ctx, glCullFace (GL_FRONT) );
              break;

            case COGL_PIPELINE_CULL_FACE_MODE_BACK:
              GE( ctx, glCullFace (GL_BACK) );
              break;

            case COGL_PIPELINE_CULL_FACE_MODE_BOTH:
              GE( ctx, glCullFace (GL_FRONT_AND_BACK) );
              break;
            }

          /* If we are painting to an offscreen framebuffer then we
             need to invert the winding of the front face because
             everything is painted upside down */
          invert_winding = cogl_is_offscreen (draw_framebuffer);

          switch (cull_face_state->front_winding)
            {
            case COGL_WINDING_CLOCKWISE:
              GE( ctx, glFrontFace (invert_winding ? GL_CCW : GL_CW) );
              break;

            case COGL_WINDING_COUNTER_CLOCKWISE:
              GE( ctx, glFrontFace (invert_winding ? GL_CW : GL_CCW) );
              break;
            }
        }
    }

  if (pipeline->real_blend_enable != ctx->gl_blend_enable_cache)
    {
      if (pipeline->real_blend_enable)
        GE (ctx, glEnable (GL_BLEND));
      else
        GE (ctx, glDisable (GL_BLEND));
      /* XXX: we shouldn't update any other blend state if blending
       * is disabled! */
      ctx->gl_blend_enable_cache = pipeline->real_blend_enable;
    }
}

static int
get_max_activateable_texture_units (void)
{
  _COGL_GET_CONTEXT (ctx, 0);

  if (G_UNLIKELY (ctx->max_activateable_texture_units == -1))
    {
      GLint values[3];
      int n_values = 0;
      int i;

#ifdef HAVE_COGL_GL
      if (ctx->driver == COGL_DRIVER_GL)
        {
          /* GL_MAX_TEXTURE_COORDS is provided for both GLSL and ARBfp. It
             defines the number of texture coordinates that can be
             uploaded (but doesn't necessarily relate to how many texture
             images can be sampled) */
          if (cogl_has_feature (ctx, COGL_FEATURE_ID_GLSL) ||
              cogl_has_feature (ctx, COGL_FEATURE_ID_ARBFP))
            /* Previously this code subtracted the value by one but there
               was no explanation for why it did this and it doesn't seem
               to make sense so it has been removed */
            GE (ctx, glGetIntegerv (GL_MAX_TEXTURE_COORDS,
                                    values + n_values++));

          /* GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS is defined for GLSL but
             not ARBfp */
          if (cogl_has_feature (ctx, COGL_FEATURE_ID_GLSL))
            GE (ctx, glGetIntegerv (GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS,
                                    values + n_values++));
        }
#endif /* HAVE_COGL_GL */

#ifdef HAVE_COGL_GLES2
      if (ctx->driver == COGL_DRIVER_GLES2)
        {
          GE (ctx, glGetIntegerv (GL_MAX_VERTEX_ATTRIBS, values + n_values));
          /* Two of the vertex attribs need to be used for the position
             and color */
          values[n_values++] -= 2;

          GE (ctx, glGetIntegerv (GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS,
                                  values + n_values++));
        }
#endif

#if defined (HAVE_COGL_GL) || defined (HAVE_COGL_GLES) /* not GLES2 */
      if (ctx->driver != COGL_DRIVER_GLES2)
        {
          /* GL_MAX_TEXTURE_UNITS defines the number of units that are
             usable from the fixed function pipeline, therefore it isn't
             available in GLES2. These are also tied to the number of
             texture coordinates that can be uploaded so it should be less
             than that available from the shader extensions */
          GE (ctx, glGetIntegerv (GL_MAX_TEXTURE_UNITS,
                                  values + n_values++));

        }
#endif

      g_assert (n_values <= G_N_ELEMENTS (values) &&
                n_values > 0);

      /* Use the maximum value */
      ctx->max_activateable_texture_units = values[0];
      for (i = 1; i < n_values; i++)
        ctx->max_activateable_texture_units =
          MAX (values[i], ctx->max_activateable_texture_units);
    }

  return ctx->max_activateable_texture_units;
}

typedef struct
{
  int i;
  unsigned long *layer_differences;
} CoglPipelineFlushLayerState;

static gboolean
flush_layers_common_gl_state_cb (CoglPipelineLayer *layer, void *user_data)
{
  CoglPipelineFlushLayerState *flush_state = user_data;
  int                          unit_index = flush_state->i;
  CoglTextureUnit             *unit = _cogl_get_texture_unit (unit_index);
  unsigned long                layers_difference =
    flush_state->layer_differences[unit_index];

  _COGL_GET_CONTEXT (ctx, FALSE);

  /* There may not be enough texture units so we can bail out if
   * that's the case...
   */
  if (G_UNLIKELY (unit_index >= get_max_activateable_texture_units ()))
    {
      static gboolean shown_warning = FALSE;

      if (!shown_warning)
        {
          g_warning ("Your hardware does not have enough texture units"
                     "to handle this many texture layers");
          shown_warning = TRUE;
        }
      return FALSE;
    }

  if (layers_difference & COGL_PIPELINE_LAYER_STATE_TEXTURE_DATA)
    {
      unsigned long state = COGL_PIPELINE_LAYER_STATE_TEXTURE_DATA;
      CoglPipelineLayer *authority =
        _cogl_pipeline_layer_get_authority (layer, state);
      CoglTexture *texture;
      GLuint gl_texture;
      GLenum gl_target;

      texture = (authority->texture == NULL ?
                 ctx->default_gl_texture_2d_tex :
                 authority->texture);

      cogl_texture_get_gl_texture (texture,
                                   &gl_texture,
                                   &gl_target);

      _cogl_set_active_texture_unit (unit_index);

      /* NB: There are several Cogl components and some code in
       * Clutter that will temporarily bind arbitrary GL textures to
       * query and modify texture object parameters. If you look at
       * _cogl_bind_gl_texture_transient() you can see we make sure
       * that such code always binds to texture unit 1 which means we
       * can't rely on the unit->gl_texture state if unit->index == 1.
       *
       * Because texture unit 1 is a bit special we actually defer any
       * necessary glBindTexture for it until the end of
       * _cogl_pipeline_flush_gl_state().
       *
       * NB: we get notified whenever glDeleteTextures is used (see
       * _cogl_delete_gl_texture()) where we invalidate
       * unit->gl_texture references to deleted textures so it's safe
       * to compare unit->gl_texture with gl_texture.  (Without the
       * hook it would be possible to delete a GL texture and create a
       * new one with the same name and comparing unit->gl_texture and
       * gl_texture wouldn't detect that.)
       *
       * NB: for foreign textures we don't know how the deletion of
       * the GL texture objects correspond to the deletion of the
       * CoglTextures so if there was previously a foreign texture
       * associated with the texture unit then we can't assume that we
       * aren't seeing a recycled texture name so we have to bind.
       */
      if (unit->gl_texture != gl_texture || unit->is_foreign)
        {
          if (unit_index == 1)
            unit->dirty_gl_texture = TRUE;
          else
            GE (ctx, glBindTexture (gl_target, gl_texture));
          unit->gl_texture = gl_texture;
          unit->gl_target = gl_target;
        }

      unit->is_foreign = _cogl_texture_is_foreign (texture);

      /* The texture_storage_changed boolean indicates if the
       * CoglTexture's underlying GL texture storage has changed since
       * it was flushed to the texture unit. We've just flushed the
       * latest state so we can reset this. */
      unit->texture_storage_changed = FALSE;
    }

  /* Under GLES2 the fragment shader will use gl_PointCoord instead of
     replacing the texture coordinates */
#if defined (HAVE_COGL_GLES) || defined (HAVE_COGL_GL)
  if (ctx->driver != COGL_DRIVER_GLES2 &&
      (layers_difference & COGL_PIPELINE_LAYER_STATE_POINT_SPRITE_COORDS))
    {
      CoglPipelineState change = COGL_PIPELINE_LAYER_STATE_POINT_SPRITE_COORDS;
      CoglPipelineLayer *authority =
        _cogl_pipeline_layer_get_authority (layer, change);
      CoglPipelineLayerBigState *big_state = authority->big_state;

      _cogl_set_active_texture_unit (unit_index);

      GE (ctx, glTexEnvi (GL_POINT_SPRITE, GL_COORD_REPLACE,
                          big_state->point_sprite_coords));
    }
#endif

  cogl_handle_ref (layer);
  if (unit->layer != COGL_INVALID_HANDLE)
    cogl_handle_unref (unit->layer);

  unit->layer = layer;
  unit->layer_changes_since_flush = 0;

  flush_state->i++;

  return TRUE;
}

static void
_cogl_pipeline_flush_common_gl_state (CoglPipeline  *pipeline,
                                      unsigned long  pipelines_difference,
                                      unsigned long *layer_differences,
                                      gboolean       skip_gl_color)
{
  CoglPipelineFlushLayerState state;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  _cogl_pipeline_flush_color_blend_alpha_depth_state (pipeline,
                                                      pipelines_difference,
                                                      skip_gl_color);

  state.i = 0;
  state.layer_differences = layer_differences;
  _cogl_pipeline_foreach_layer_internal (pipeline,
                                         flush_layers_common_gl_state_cb,
                                         &state);
}

/* Re-assert the layer's wrap modes on the given CoglTexture.
 *
 * Note: we don't simply forward the wrap modes to layer->texture
 * since the actual texture being used may have been overridden.
 */
static void
_cogl_pipeline_layer_forward_wrap_modes (CoglPipelineLayer *layer,
                                         CoglTexture *texture)
{
  CoglPipelineWrapModeInternal wrap_mode_s, wrap_mode_t, wrap_mode_p;
  GLenum gl_wrap_mode_s, gl_wrap_mode_t, gl_wrap_mode_p;

  if (texture == NULL)
    return;

  _cogl_pipeline_layer_get_wrap_modes (layer,
                                       &wrap_mode_s,
                                       &wrap_mode_t,
                                       &wrap_mode_p);

  /* Update the wrap mode on the texture object. The texture backend
     should cache the value so that it will be a no-op if the object
     already has the same wrap mode set. The backend is best placed to
     do this because it knows how many of the coordinates will
     actually be used (ie, a 1D texture only cares about the 's'
     coordinate but a 3D texture would use all three). GL uses the
     wrap mode as part of the texture object state but we are
     pretending it's part of the per-layer environment state. This
     will break if the application tries to use different modes in
     different layers using the same texture. */

  if (wrap_mode_s == COGL_PIPELINE_WRAP_MODE_INTERNAL_AUTOMATIC)
    gl_wrap_mode_s = GL_CLAMP_TO_EDGE;
  else
    gl_wrap_mode_s = wrap_mode_s;

  if (wrap_mode_t == COGL_PIPELINE_WRAP_MODE_INTERNAL_AUTOMATIC)
    gl_wrap_mode_t = GL_CLAMP_TO_EDGE;
  else
    gl_wrap_mode_t = wrap_mode_t;

  if (wrap_mode_p == COGL_PIPELINE_WRAP_MODE_INTERNAL_AUTOMATIC)
    gl_wrap_mode_p = GL_CLAMP_TO_EDGE;
  else
    gl_wrap_mode_p = wrap_mode_p;

  _cogl_texture_set_wrap_mode_parameters (texture,
                                          gl_wrap_mode_s,
                                          gl_wrap_mode_t,
                                          gl_wrap_mode_p);
}

/* OpenGL associates the min/mag filters and repeat modes with the
 * texture object not the texture unit so we always have to re-assert
 * the filter and repeat modes whenever we use a texture since it may
 * be referenced by multiple pipelines with different modes.
 *
 * XXX: GL_ARB_sampler_objects fixes this in OpenGL so we should
 * eventually look at using this extension when available.
 */
static void
foreach_texture_unit_update_filter_and_wrap_modes (void)
{
  int i;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  for (i = 0; i < ctx->texture_units->len; i++)
    {
      CoglTextureUnit *unit =
        &g_array_index (ctx->texture_units, CoglTextureUnit, i);

      if (unit->layer)
        {
          CoglTexture *texture = _cogl_pipeline_layer_get_texture (unit->layer);

          if (texture != NULL)
            {
              CoglPipelineFilter min;
              CoglPipelineFilter mag;

              _cogl_pipeline_layer_get_filters (unit->layer, &min, &mag);
              _cogl_texture_set_filters (texture, min, mag);

              _cogl_pipeline_layer_forward_wrap_modes (unit->layer, texture);
            }
        }
    }
}

typedef struct
{
  int i;
  unsigned long *layer_differences;
} CoglPipelineCompareLayersState;

static gboolean
compare_layer_differences_cb (CoglPipelineLayer *layer, void *user_data)
{
  CoglPipelineCompareLayersState *state = user_data;
  CoglTextureUnit *unit = _cogl_get_texture_unit (state->i);

  if (unit->layer == layer)
    state->layer_differences[state->i] = unit->layer_changes_since_flush;
  else if (unit->layer)
    {
      state->layer_differences[state->i] = unit->layer_changes_since_flush;
      state->layer_differences[state->i] |=
        _cogl_pipeline_layer_compare_differences (layer, unit->layer);
    }
  else
    state->layer_differences[state->i] = COGL_PIPELINE_LAYER_STATE_ALL_SPARSE;

  /* XXX: There is always a possibility that a CoglTexture's
   * underlying GL texture storage has been changed since it was last
   * bound to a texture unit which is why we have a callback into
   * _cogl_pipeline_texture_storage_change_notify whenever a textures
   * underlying GL texture storage changes which will set the
   * unit->texture_intern_changed flag. If we see that's been set here
   * then we force an update of the texture state...
   */
  if (unit->texture_storage_changed)
    state->layer_differences[state->i] |=
      COGL_PIPELINE_LAYER_STATE_TEXTURE_DATA;

  state->i++;

  return TRUE;
}

typedef struct
{
  const CoglPipelineFragend *fragend;
  CoglPipeline *pipeline;
  unsigned long *layer_differences;
  gboolean error_adding_layer;
  gboolean added_layer;
} CoglPipelineFragendAddLayerState;


static gboolean
fragend_add_layer_cb (CoglPipelineLayer *layer,
                      void *user_data)
{
  CoglPipelineFragendAddLayerState *state = user_data;
  const CoglPipelineFragend *fragend = state->fragend;
  CoglPipeline *pipeline = state->pipeline;
  int unit_index = _cogl_pipeline_layer_get_unit_index (layer);

  _COGL_GET_CONTEXT (ctx, FALSE);

  /* Either generate per layer code snippets or setup the
   * fixed function glTexEnv for each layer... */
  if (G_LIKELY (fragend->add_layer (pipeline,
                                    layer,
                                    state->layer_differences[unit_index])))
    state->added_layer = TRUE;
  else
    {
      state->error_adding_layer = TRUE;
      return FALSE;
    }

  return TRUE;
}

typedef struct
{
  const CoglPipelineVertend *vertend;
  CoglPipeline *pipeline;
  unsigned long *layer_differences;
  gboolean error_adding_layer;
  gboolean added_layer;
} CoglPipelineVertendAddLayerState;


static gboolean
vertend_add_layer_cb (CoglPipelineLayer *layer,
                      void *user_data)
{
  CoglPipelineVertendAddLayerState *state = user_data;
  const CoglPipelineVertend *vertend = state->vertend;
  CoglPipeline *pipeline = state->pipeline;
  int unit_index = _cogl_pipeline_layer_get_unit_index (layer);

  _COGL_GET_CONTEXT (ctx, FALSE);

  /* Either enerate per layer code snippets or setup the
   * fixed function matrix uniforms for each layer... */
  if (G_LIKELY (vertend->add_layer (pipeline,
                                    layer,
                                    state->layer_differences[unit_index])))
    state->added_layer = TRUE;
  else
    {
      state->error_adding_layer = TRUE;
      return FALSE;
    }

  return TRUE;
}

/*
 * _cogl_pipeline_flush_gl_state:
 *
 * Details of override options:
 * ->fallback_mask: is a bitmask of the pipeline layers that need to be
 *    replaced with the default, fallback textures. The fallback textures are
 *    fully transparent textures so they hopefully wont contribute to the
 *    texture combining.
 *
 *    The intention of fallbacks is to try and preserve
 *    the number of layers the user is expecting so that texture coordinates
 *    they gave will mostly still correspond to the textures they intended, and
 *    have a fighting chance of looking close to their originally intended
 *    result.
 *
 * ->disable_mask: is a bitmask of the pipeline layers that will simply have
 *    texturing disabled. It's only really intended for disabling all layers
 *    > X; i.e. we'd expect to see a contiguous run of 0 starting from the LSB
 *    and at some point the remaining bits flip to 1. It might work to disable
 *    arbitrary layers; though I'm not sure a.t.m how OpenGL would take to
 *    that.
 *
 *    The intention of the disable_mask is for emitting geometry when the user
 *    hasn't supplied enough texture coordinates for all the layers and it's
 *    not possible to auto generate default texture coordinates for those
 *    layers.
 *
 * ->layer0_override_texture: forcibly tells us to bind this GL texture name for
 *    layer 0 instead of plucking the gl_texture from the CoglTexture of layer
 *    0.
 *
 *    The intention of this is for any primitives that supports sliced textures.
 *    The code will can iterate each of the slices and re-flush the pipeline
 *    forcing the GL texture of each slice in turn.
 *
 * ->wrap_mode_overrides: overrides the wrap modes set on each
 *    layer. This is used to implement the automatic wrap mode.
 *
 * XXX: It might also help if we could specify a texture matrix for code
 *    dealing with slicing that would be multiplied with the users own matrix.
 *
 *    Normaly texture coords in the range [0, 1] refer to the extents of the
 *    texture, but when your GL texture represents a slice of the real texture
 *    (from the users POV) then a texture matrix would be a neat way of
 *    transforming the mapping for each slice.
 *
 *    Currently for textured rectangles we manually calculate the texture
 *    coords for each slice based on the users given coords, but this solution
 *    isn't ideal, and can't be used with CoglVertexBuffers.
 */
void
_cogl_pipeline_flush_gl_state (CoglPipeline *pipeline,
                               gboolean skip_gl_color,
                               int n_tex_coord_attribs)
{
  unsigned long    pipelines_difference;
  int              n_layers;
  unsigned long   *layer_differences;
  int              i;
  CoglTextureUnit *unit1;

  COGL_STATIC_TIMER (pipeline_flush_timer,
                     "Mainloop", /* parent */
                     "Material Flush",
                     "The time spent flushing material state",
                     0 /* no application private data */);

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  COGL_TIMER_START (_cogl_uprof_context, pipeline_flush_timer);

  if (ctx->current_pipeline == pipeline)
    {
      /* Bail out asap if we've been asked to re-flush the already current
       * pipeline and we can see the pipeline hasn't changed */
      if (ctx->current_pipeline_age == pipeline->age &&
          ctx->current_pipeline_skip_gl_color == skip_gl_color)
        goto done;

      pipelines_difference = ctx->current_pipeline_changes_since_flush;
    }
  else if (ctx->current_pipeline)
    {
      pipelines_difference = ctx->current_pipeline_changes_since_flush;
      pipelines_difference |=
        _cogl_pipeline_compare_differences (ctx->current_pipeline,
                                            pipeline);
    }
  else
    pipelines_difference = COGL_PIPELINE_STATE_ALL_SPARSE;

  /* Get a layer_differences mask for each layer to be flushed */
  n_layers = cogl_pipeline_get_n_layers (pipeline);
  if (n_layers)
    {
      CoglPipelineCompareLayersState state;
      layer_differences = g_alloca (sizeof (unsigned long *) * n_layers);
      memset (layer_differences, 0, sizeof (layer_differences));
      state.i = 0;
      state.layer_differences = layer_differences;
      _cogl_pipeline_foreach_layer_internal (pipeline,
                                             compare_layer_differences_cb,
                                             &state);
    }
  else
    layer_differences = NULL;

  /* Make sure we generate the texture coordinate array to be at least
     the number of layers. This is important because the vertend will
     try to pass along the corresponding varying for each layer
     regardless of whether the fragment shader is actually using
     it. Also it is possible that the application is assuming that if
     the attribute isn't passed then it will default to 0,0. This is
     what test-cogl-primitive does */
  if (n_layers > n_tex_coord_attribs)
    n_tex_coord_attribs = n_layers;

  /* First flush everything that's the same regardless of which
   * pipeline backend is being used...
   *
   * 1) top level state:
   *  glColor (or skip if a vertex attribute is being used for color)
   *  blend state
   *  alpha test state (except for GLES 2.0)
   *
   * 2) then foreach layer:
   *  determine gl_target/gl_texture
   *  bind texture
   *
   *  Note: After _cogl_pipeline_flush_common_gl_state you can expect
   *  all state of the layers corresponding texture unit to be
   *  updated.
   */
  _cogl_pipeline_flush_common_gl_state (pipeline,
                                        pipelines_difference,
                                        layer_differences,
                                        skip_gl_color);

  /* Now flush the fragment processing state according to the current
   * fragment processing backend.
   *
   * Note: Some of the backends may not support the current pipeline
   * configuration and in that case it will report an error and we
   * will fallback to a different backend.
   *
   * NB: if pipeline->backend != COGL_PIPELINE_FRAGEND_UNDEFINED then
   * we have previously managed to successfully flush this pipeline
   * with the given backend so we will simply use that to avoid
   * fallback code paths.
   */

  if (pipeline->fragend == COGL_PIPELINE_FRAGEND_UNDEFINED)
    _cogl_pipeline_set_fragend (pipeline, COGL_PIPELINE_FRAGEND_DEFAULT);

  for (i = pipeline->fragend;
       i < G_N_ELEMENTS (_cogl_pipeline_fragends);
       i++, _cogl_pipeline_set_fragend (pipeline, i))
    {
      const CoglPipelineFragend *fragend = _cogl_pipeline_fragends[i];
      CoglPipelineFragendAddLayerState state;

      /* E.g. For fragends generating code they can setup their
       * scratch buffers here... */
      if (G_UNLIKELY (!fragend->start (pipeline,
                                       n_layers,
                                       pipelines_difference,
                                       n_tex_coord_attribs)))
        continue;

      state.fragend = fragend;
      state.pipeline = pipeline;
      state.layer_differences = layer_differences;
      state.error_adding_layer = FALSE;
      state.added_layer = FALSE;
      _cogl_pipeline_foreach_layer_internal (pipeline,
                                             fragend_add_layer_cb,
                                             &state);

      if (G_UNLIKELY (state.error_adding_layer))
        continue;

      if (!state.added_layer &&
          fragend->passthrough &&
          G_UNLIKELY (!fragend->passthrough (pipeline)))
        continue;

      /* For fragends generating code they may compile and link their
       * programs here, update any uniforms and tell OpenGL to use
       * that program.
       */
      if (G_UNLIKELY (!fragend->end (pipeline, pipelines_difference)))
        continue;

      break;
    }

  if (G_UNLIKELY (i >= G_N_ELEMENTS (_cogl_pipeline_fragends)))
    g_warning ("No usable pipeline fragment backend was found!");

  /* Now flush the vertex processing state according to the current
   * vertex processing backend.
   */

  if (pipeline->vertend == COGL_PIPELINE_VERTEND_UNDEFINED)
    _cogl_pipeline_set_vertend (pipeline, COGL_PIPELINE_VERTEND_DEFAULT);

  for (i = pipeline->vertend;
       i < G_N_ELEMENTS (_cogl_pipeline_vertends);
       i++, _cogl_pipeline_set_vertend (pipeline, i))
    {
      const CoglPipelineVertend *vertend = _cogl_pipeline_vertends[i];
      CoglPipelineVertendAddLayerState state;

      /* E.g. For vertends generating code they can setup their
       * scratch buffers here... */
      if (G_UNLIKELY (!vertend->start (pipeline,
                                       n_layers,
                                       pipelines_difference,
                                       n_tex_coord_attribs)))
        continue;

      state.vertend = vertend;
      state.pipeline = pipeline;
      state.layer_differences = layer_differences;
      state.error_adding_layer = FALSE;
      state.added_layer = FALSE;
      _cogl_pipeline_foreach_layer_internal (pipeline,
                                             vertend_add_layer_cb,
                                             &state);

      if (G_UNLIKELY (state.error_adding_layer))
        continue;

      /* For vertends generating code they may compile and link their
       * programs here, update any uniforms and tell OpenGL to use
       * that program.
       */
      if (G_UNLIKELY (!vertend->end (pipeline, pipelines_difference)))
        continue;

      break;
    }

  if (G_UNLIKELY (i >= G_N_ELEMENTS (_cogl_pipeline_vertends)))
    g_warning ("No usable pipeline vertex backend was found!");

  for (i = 0; i < COGL_PIPELINE_N_PROGENDS; i++)
    if (_cogl_pipeline_progends[i]->end)
      _cogl_pipeline_progends[i]->end (pipeline, pipelines_difference,
                                       n_tex_coord_attribs);

  /* FIXME: This reference is actually resulting in lots of
   * copy-on-write reparenting because one-shot pipelines end up
   * living for longer than necessary and so any later modification of
   * the parent will cause a copy-on-write.
   *
   * XXX: The issue should largely go away when we switch to using
   * weak pipelines for overrides.
   */
  cogl_object_ref (pipeline);
  if (ctx->current_pipeline != NULL)
    cogl_object_unref (ctx->current_pipeline);
  ctx->current_pipeline = pipeline;
  ctx->current_pipeline_changes_since_flush = 0;
  ctx->current_pipeline_skip_gl_color = skip_gl_color;
  ctx->current_pipeline_age = pipeline->age;

done:

  /* We can't assume the color will be retained between flushes on
     GLES2 because the generic attribute values are not stored as part
     of the program object so they could be overridden by any
     attribute changes in another program */
#ifdef HAVE_COGL_GLES2
  if (ctx->driver == COGL_DRIVER_GLES2 && !skip_gl_color)
    {
      int attribute;
      CoglPipeline *authority =
        _cogl_pipeline_get_authority (pipeline, COGL_PIPELINE_STATE_COLOR);
      int name_index = COGL_ATTRIBUTE_COLOR_NAME_INDEX;

      attribute =
        _cogl_pipeline_progend_glsl_get_attrib_location (pipeline, name_index);
      if (attribute != -1)
        GE (ctx,
            glVertexAttrib4f (attribute,
                              cogl_color_get_red_float (&authority->color),
                              cogl_color_get_green_float (&authority->color),
                              cogl_color_get_blue_float (&authority->color),
                              cogl_color_get_alpha_float (&authority->color)));
    }
#endif

  /* Give any progends a chance to update any uniforms that might not
     depend on the material state. This is used on GLES2 to update the
     matrices */
  for (i = 0; i < COGL_PIPELINE_N_PROGENDS; i++)
    if (_cogl_pipeline_progends[i]->pre_paint)
      _cogl_pipeline_progends[i]->pre_paint (pipeline);

  /* Handle the fact that OpenGL associates texture filter and wrap
   * modes with the texture objects not the texture units... */
  foreach_texture_unit_update_filter_and_wrap_modes ();

  /* If this pipeline has more than one layer then we always need
   * to make sure we rebind the texture for unit 1.
   *
   * NB: various components of Cogl may temporarily bind arbitrary
   * textures to texture unit 1 so they can query and modify texture
   * object parameters. cogl-pipeline.c (See
   * _cogl_bind_gl_texture_transient)
   */
  unit1 = _cogl_get_texture_unit (1);
  if (cogl_pipeline_get_n_layers (pipeline) > 1 && unit1->dirty_gl_texture)
    {
      _cogl_set_active_texture_unit (1);
      GE (ctx, glBindTexture (unit1->gl_target, unit1->gl_texture));
      unit1->dirty_gl_texture = FALSE;
    }

  COGL_TIMER_STOP (_cogl_uprof_context, pipeline_flush_timer);
}

