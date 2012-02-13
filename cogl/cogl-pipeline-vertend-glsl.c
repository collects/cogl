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

#include "cogl-context-private.h"
#include "cogl-pipeline-private.h"
#include "cogl-pipeline-opengl-private.h"

#ifdef COGL_PIPELINE_VERTEND_GLSL

#include "cogl.h"
#include "cogl-internal.h"
#include "cogl-context-private.h"
#include "cogl-handle.h"
#include "cogl-program-private.h"
#include "cogl-pipeline-vertend-glsl-private.h"
#include "cogl-pipeline-state-private.h"

const CoglPipelineVertend _cogl_pipeline_glsl_vertend;

typedef struct
{
  unsigned int ref_count;

  GLuint gl_shader;
  GString *header, *source;

  /* Age of the user program that was current when the shader was
     generated. We need to keep track of this because if the user
     program changes then we may need to redecide whether to generate
     a shader at all */
  unsigned int user_program_age;

  /* The number of tex coord attributes that the shader was generated
     for. If this changes on GLES2 then we need to regenerate the
     shader */
  int n_tex_coord_attribs;
} CoglPipelineShaderState;

static CoglUserDataKey shader_state_key;

static CoglPipelineShaderState *
shader_state_new (void)
{
  CoglPipelineShaderState *shader_state;

  shader_state = g_slice_new0 (CoglPipelineShaderState);
  shader_state->ref_count = 1;

  return shader_state;
}

static CoglPipelineShaderState *
get_shader_state (CoglPipeline *pipeline)
{
  return cogl_object_get_user_data (COGL_OBJECT (pipeline), &shader_state_key);
}

static void
destroy_shader_state (void *user_data)
{
  CoglPipelineShaderState *shader_state = user_data;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  if (--shader_state->ref_count == 0)
    {
      if (shader_state->gl_shader)
        GE( ctx, glDeleteShader (shader_state->gl_shader) );

      g_slice_free (CoglPipelineShaderState, shader_state);
    }
}

static void
set_shader_state (CoglPipeline *pipeline,
                  CoglPipelineShaderState *shader_state)
{
  cogl_object_set_user_data (COGL_OBJECT (pipeline),
                             &shader_state_key,
                             shader_state,
                             destroy_shader_state);
}

static void
dirty_shader_state (CoglPipeline *pipeline)
{
  cogl_object_set_user_data (COGL_OBJECT (pipeline),
                             &shader_state_key,
                             NULL,
                             NULL);
}

GLuint
_cogl_pipeline_vertend_glsl_get_shader (CoglPipeline *pipeline)
{
  CoglPipelineShaderState *shader_state = get_shader_state (pipeline);

  if (shader_state)
    return shader_state->gl_shader;
  else
    return 0;
}

static CoglPipelineSnippetList *
get_vertex_snippets (CoglPipeline *pipeline)
{
  pipeline =
    _cogl_pipeline_get_authority (pipeline,
                                  COGL_PIPELINE_STATE_VERTEX_SNIPPETS);

  return &pipeline->big_state->vertex_snippets;
}

static CoglPipelineSnippetList *
get_layer_vertex_snippets (CoglPipelineLayer *layer)
{
  unsigned long state = COGL_PIPELINE_LAYER_STATE_VERTEX_SNIPPETS;
  layer = _cogl_pipeline_layer_get_authority (layer, state);

  return &layer->big_state->vertex_snippets;
}

static gboolean
_cogl_pipeline_vertend_glsl_start (CoglPipeline *pipeline,
                                   int n_layers,
                                   unsigned long pipelines_difference,
                                   int n_tex_coord_attribs)
{
  CoglPipelineShaderState *shader_state;
  CoglPipeline *template_pipeline = NULL;
  CoglProgram *user_program;

  _COGL_GET_CONTEXT (ctx, FALSE);

  if (!cogl_has_feature (ctx, COGL_FEATURE_ID_GLSL))
    return FALSE;

  user_program = cogl_pipeline_get_user_program (pipeline);

  /* If the user program has a vertex shader that isn't GLSL then the
     appropriate vertend for that language should handle it */
  if (user_program &&
      _cogl_program_has_vertex_shader (user_program) &&
      _cogl_program_get_language (user_program) != COGL_SHADER_LANGUAGE_GLSL)
    return FALSE;

  /* Now lookup our glsl backend private state (allocating if
   * necessary) */
  shader_state = get_shader_state (pipeline);

  if (shader_state == NULL)
    {
      CoglPipeline *authority;

      /* Get the authority for anything affecting vertex shader
         state */
      authority = _cogl_pipeline_find_equivalent_parent
        (pipeline,
         COGL_PIPELINE_STATE_AFFECTS_VERTEX_CODEGEN &
         ~COGL_PIPELINE_STATE_LAYERS,
         COGL_PIPELINE_LAYER_STATE_AFFECTS_VERTEX_CODEGEN);

      shader_state = get_shader_state (authority);

      if (shader_state == NULL)
        {
          /* Check if there is already a similar cached pipeline whose
             shader state we can share */
          if (G_LIKELY (!(COGL_DEBUG_ENABLED
                          (COGL_DEBUG_DISABLE_PROGRAM_CACHES))))
            {
              template_pipeline =
                _cogl_pipeline_cache_get_vertex_template (ctx->pipeline_cache,
                                                          authority);

              shader_state = get_shader_state (template_pipeline);
            }

          if (shader_state)
            shader_state->ref_count++;
          else
            shader_state = shader_state_new ();

          set_shader_state (authority, shader_state);

          if (template_pipeline)
            {
              shader_state->ref_count++;
              set_shader_state (template_pipeline, shader_state);
            }
        }

      if (authority != pipeline)
        {
          shader_state->ref_count++;
          set_shader_state (pipeline, shader_state);
        }
    }

  if (shader_state->gl_shader)
    {
      /* If we already have a valid GLSL shader then we don't need to
         generate a new one. However if there's a user program and it
         has changed since the last link then we do need a new
         shader. If the number of tex coord attribs changes on GLES2
         then we need to regenerate the shader with a different boiler
         plate */
      if ((user_program == NULL ||
           shader_state->user_program_age == user_program->age)
          && (ctx->driver != COGL_DRIVER_GLES2 ||
              shader_state->n_tex_coord_attribs == n_tex_coord_attribs))
        return TRUE;

      /* We need to recreate the shader so destroy the existing one */
      GE( ctx, glDeleteShader (shader_state->gl_shader) );
      shader_state->gl_shader = 0;
    }

  /* If we make it here then we have a shader_state struct without a gl_shader
     either because this is the first time we've encountered it or
     because the user program has changed */

  if (user_program)
    shader_state->user_program_age = user_program->age;

  shader_state->n_tex_coord_attribs = n_tex_coord_attribs;

  /* If the user program contains a vertex shader then we don't need
     to generate one */
  if (user_program &&
      _cogl_program_has_vertex_shader (user_program))
    return TRUE;

  /* We reuse two grow-only GStrings for code-gen. One string
     contains the uniform and attribute declarations while the
     other contains the main function. We need two strings
     because we need to dynamically declare attributes as the
     add_layer callback is invoked */
  g_string_set_size (ctx->codegen_header_buffer, 0);
  g_string_set_size (ctx->codegen_source_buffer, 0);
  shader_state->header = ctx->codegen_header_buffer;
  shader_state->source = ctx->codegen_source_buffer;

  g_string_append (shader_state->source,
                   "void\n"
                   "cogl_generated_source ()\n"
                   "{\n");

  if (ctx->driver == COGL_DRIVER_GLES2)
    /* There is no builtin uniform for the pointsize on GLES2 so we need
       to copy it from the custom uniform in the vertex shader */
    g_string_append (shader_state->source,
                     "  cogl_point_size_out = cogl_point_size_in;\n");
  /* On regular OpenGL we'll just flush the point size builtin */
  else if (pipelines_difference & COGL_PIPELINE_STATE_POINT_SIZE)
    {
      CoglPipeline *authority =
        _cogl_pipeline_get_authority (pipeline, COGL_PIPELINE_STATE_POINT_SIZE);

      if (ctx->point_size_cache != authority->big_state->point_size)
        {
          GE( ctx, glPointSize (authority->big_state->point_size) );
          ctx->point_size_cache = authority->big_state->point_size;
        }
    }

  return TRUE;
}

static gboolean
_cogl_pipeline_vertend_glsl_add_layer (CoglPipeline *pipeline,
                                       CoglPipelineLayer *layer,
                                       unsigned long layers_difference)
{
  CoglPipelineShaderState *shader_state;
  CoglPipelineSnippetData snippet_data;
  int unit_index;

  _COGL_GET_CONTEXT (ctx, FALSE);

  shader_state = get_shader_state (pipeline);

  unit_index = _cogl_pipeline_layer_get_unit_index (layer);

  if (ctx->driver != COGL_DRIVER_GLES2)
    {
      /* We are using the fixed function uniforms for the user matrices
         and the only way to set them is with the fixed function API so we
         still need to flush them here */
      if (layers_difference & COGL_PIPELINE_LAYER_STATE_USER_MATRIX)
        {
          CoglPipelineLayerState state = COGL_PIPELINE_LAYER_STATE_USER_MATRIX;
          CoglPipelineLayer *authority =
            _cogl_pipeline_layer_get_authority (layer, state);
          CoglTextureUnit *unit = _cogl_get_texture_unit (unit_index);

          _cogl_matrix_stack_set (unit->matrix_stack,
                                  &authority->big_state->matrix);

          _cogl_set_active_texture_unit (unit_index);

          _cogl_matrix_stack_flush_to_gl_builtins (ctx,
                                                   unit->matrix_stack,
                                                   COGL_MATRIX_TEXTURE,
                                                   FALSE /* do flip */);
        }
    }

  if (shader_state->source == NULL)
    return TRUE;

  /* Transform the texture coordinates by the layer's user matrix.
   *
   * FIXME: this should avoid doing the transform if there is no user
   * matrix set. This might need a separate layer state flag for
   * whether there is a user matrix
   *
   * FIXME: we could be more clever here and try to detect if the
   * fragment program is going to use the texture coordinates and
   * avoid setting them if not
   */

  g_string_append_printf (shader_state->header,
                          "vec4\n"
                          "cogl_real_transform_layer%i (mat4 matrix, "
                          "vec4 tex_coord)\n"
                          "{\n"
                          "  return matrix * tex_coord;\n"
                          "}\n",
                          unit_index);

  /* Wrap the layer code in any snippets that have been hooked */
  memset (&snippet_data, 0, sizeof (snippet_data));
  snippet_data.snippets = get_layer_vertex_snippets (layer);
  snippet_data.hook = COGL_SNIPPET_HOOK_TEXTURE_COORD_TRANSFORM;
  snippet_data.chain_function = g_strdup_printf ("cogl_real_transform_layer%i",
                                                 unit_index);
  snippet_data.final_name = g_strdup_printf ("cogl_transform_layer%i",
                                             unit_index);
  snippet_data.function_prefix = g_strdup_printf ("cogl_transform_layer%i",
                                                  unit_index);
  snippet_data.return_type = "vec4";
  snippet_data.return_variable = "cogl_tex_coord";
  snippet_data.return_variable_is_argument = TRUE;
  snippet_data.arguments = "cogl_matrix, cogl_tex_coord";
  snippet_data.argument_declarations = "mat4 cogl_matrix, vec4 cogl_tex_coord";
  snippet_data.source_buf = shader_state->header;

  _cogl_pipeline_snippet_generate_code (&snippet_data);

  g_free ((char *) snippet_data.chain_function);
  g_free ((char *) snippet_data.final_name);
  g_free ((char *) snippet_data.function_prefix);

  g_string_append_printf (shader_state->source,
                          "  cogl_tex_coord_out[%i] = "
                          "cogl_transform_layer%i (cogl_texture_matrix[%i],\n"
                          "                           "
                          "                        cogl_tex_coord%i_in);\n",
                          unit_index,
                          unit_index,
                          unit_index,
                          unit_index);

  return TRUE;
}

static gboolean
_cogl_pipeline_vertend_glsl_end (CoglPipeline *pipeline,
                                 unsigned long pipelines_difference)
{
  CoglPipelineShaderState *shader_state;

  _COGL_GET_CONTEXT (ctx, FALSE);

  shader_state = get_shader_state (pipeline);

  if (shader_state->source)
    {
      const char *source_strings[2];
      GLint lengths[2];
      GLint compile_status;
      GLuint shader;
      CoglPipelineSnippetData snippet_data;
      CoglPipelineSnippetList *vertex_snippets;

      COGL_STATIC_COUNTER (vertend_glsl_compile_counter,
                           "glsl vertex compile counter",
                           "Increments each time a new GLSL "
                           "vertex shader is compiled",
                           0 /* no application private data */);
      COGL_COUNTER_INC (_cogl_uprof_context, vertend_glsl_compile_counter);

      g_string_append (shader_state->header,
                       "void\n"
                       "cogl_real_vertex_transform ()\n"
                       "{\n"
                       "  cogl_position_out = "
                       "cogl_modelview_projection_matrix * "
                       "cogl_position_in;\n"
                       "}\n");

      g_string_append (shader_state->source,
                       "  cogl_vertex_transform ();\n"
                       "  cogl_color_out = cogl_color_in;\n"
                       "}\n");

      vertex_snippets = get_vertex_snippets (pipeline);

      /* Add hooks for the vertex transform part */
      memset (&snippet_data, 0, sizeof (snippet_data));
      snippet_data.snippets = vertex_snippets;
      snippet_data.hook = COGL_SNIPPET_HOOK_VERTEX_TRANSFORM;
      snippet_data.chain_function = "cogl_real_vertex_transform";
      snippet_data.final_name = "cogl_vertex_transform";
      snippet_data.function_prefix = "cogl_vertex_transform";
      snippet_data.source_buf = shader_state->header;
      _cogl_pipeline_snippet_generate_code (&snippet_data);

      /* Add all of the hooks for vertex processing */
      memset (&snippet_data, 0, sizeof (snippet_data));
      snippet_data.snippets = vertex_snippets;
      snippet_data.hook = COGL_SNIPPET_HOOK_VERTEX;
      snippet_data.chain_function = "cogl_generated_source";
      snippet_data.final_name = "cogl_vertex_hook";
      snippet_data.function_prefix = "cogl_vertex_hook";
      snippet_data.source_buf = shader_state->source;
      _cogl_pipeline_snippet_generate_code (&snippet_data);

      g_string_append (shader_state->source,
                       "void\n"
                       "main ()\n"
                       "{\n"
                       "  cogl_vertex_hook ();\n");

      /* If there are any snippets then we can't rely on the
         projection matrix to flip the rendering for offscreen buffers
         so we'll need to flip it using an extra statement and a
         uniform */
      if (_cogl_pipeline_has_vertex_snippets (pipeline))
        {
          g_string_append (shader_state->header,
                           "uniform vec4 _cogl_flip_vector;\n");
          g_string_append (shader_state->source,
                           "  cogl_position_out *= _cogl_flip_vector;\n");
        }

      g_string_append (shader_state->source,
                       "}\n");

      GE_RET( shader, ctx, glCreateShader (GL_VERTEX_SHADER) );

      lengths[0] = shader_state->header->len;
      source_strings[0] = shader_state->header->str;
      lengths[1] = shader_state->source->len;
      source_strings[1] = shader_state->source->str;

      _cogl_shader_set_source_with_boilerplate (shader, GL_VERTEX_SHADER,
                                                shader_state
                                                ->n_tex_coord_attribs,
                                                2, /* count */
                                                source_strings, lengths);

      GE( ctx, glCompileShader (shader) );
      GE( ctx, glGetShaderiv (shader, GL_COMPILE_STATUS, &compile_status) );

      if (!compile_status)
        {
          GLint len = 0;
          char *shader_log;

          GE( ctx, glGetShaderiv (shader, GL_INFO_LOG_LENGTH, &len) );
          shader_log = g_alloca (len);
          GE( ctx, glGetShaderInfoLog (shader, len, &len, shader_log) );
          g_warning ("Shader compilation failed:\n%s", shader_log);
        }

      shader_state->header = NULL;
      shader_state->source = NULL;
      shader_state->gl_shader = shader;
    }

  return TRUE;
}

static void
_cogl_pipeline_vertend_glsl_pre_change_notify (CoglPipeline *pipeline,
                                               CoglPipelineState change,
                                               const CoglColor *new_color)
{
  if ((change & COGL_PIPELINE_STATE_AFFECTS_VERTEX_CODEGEN))
    dirty_shader_state (pipeline);
}

/* NB: layers are considered immutable once they have any dependants
 * so although multiple pipelines can end up depending on a single
 * static layer, we can guarantee that if a layer is being *changed*
 * then it can only have one pipeline depending on it.
 *
 * XXX: Don't forget this is *pre* change, we can't read the new value
 * yet!
 */
static void
_cogl_pipeline_vertend_glsl_layer_pre_change_notify (
                                                CoglPipeline *owner,
                                                CoglPipelineLayer *layer,
                                                CoglPipelineLayerState change)
{
  CoglPipelineShaderState *shader_state;

  shader_state = get_shader_state (owner);
  if (!shader_state)
    return;

  if ((change & COGL_PIPELINE_LAYER_STATE_AFFECTS_VERTEX_CODEGEN))
    {
      dirty_shader_state (owner);
      return;
    }

  /* TODO: we could be saving snippets of texture combine code along
   * with each layer and then when a layer changes we would just free
   * the snippet. */
}

const CoglPipelineVertend _cogl_pipeline_glsl_vertend =
  {
    _cogl_pipeline_vertend_glsl_start,
    _cogl_pipeline_vertend_glsl_add_layer,
    _cogl_pipeline_vertend_glsl_end,
    _cogl_pipeline_vertend_glsl_pre_change_notify,
    _cogl_pipeline_vertend_glsl_layer_pre_change_notify
  };

#endif /* COGL_PIPELINE_VERTEND_GLSL */
