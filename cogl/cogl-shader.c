/*
 * Cogl
 *
 * An object oriented GL/GLES Abstraction/Utility Layer
 *
 * Copyright (C) 2007,2008,2009,2010 Intel Corporation.
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "cogl.h"
#include "cogl-shader-private.h"
#include "cogl-shader-boilerplate.h"
#include "cogl-internal.h"
#include "cogl-context-private.h"
#include "cogl-handle.h"

#include <glib.h>

#include <string.h>

static void _cogl_shader_free (CoglShader *shader);

COGL_HANDLE_DEFINE (Shader, shader);
COGL_OBJECT_DEFINE_DEPRECATED_REF_COUNTING (shader);

#ifndef GL_FRAGMENT_SHADER
#define GL_FRAGMENT_SHADER 0x8B30
#endif
#ifndef GL_VERTEX_SHADER
#define GL_VERTEX_SHADER 0x8B31
#endif

static void
_cogl_shader_free (CoglShader *shader)
{
  /* Frees shader resources but its handle is not
     released! Do that separately before this! */
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

#ifdef HAVE_COGL_GL
  if (shader->language == COGL_SHADER_LANGUAGE_ARBFP)
    {
      if (shader->gl_handle)
        GE (ctx, glDeletePrograms (1, &shader->gl_handle));
    }
  else
#endif
    if (shader->gl_handle)
      GE (ctx, glDeleteShader (shader->gl_handle));

  g_slice_free (CoglShader, shader);
}

CoglHandle
cogl_create_shader (CoglShaderType type)
{
  CoglShader *shader;

  _COGL_GET_CONTEXT (ctx, COGL_INVALID_HANDLE);

  switch (type)
    {
    case COGL_SHADER_TYPE_VERTEX:
    case COGL_SHADER_TYPE_FRAGMENT:
      break;
    default:
      g_warning ("Unexpected shader type (0x%08lX) given to "
                 "cogl_create_shader", (unsigned long) type);
      return COGL_INVALID_HANDLE;
    }

  shader = g_slice_new (CoglShader);
  shader->language = COGL_SHADER_LANGUAGE_GLSL;
  shader->gl_handle = 0;
#ifdef HAVE_COGL_GLES2
  shader->n_tex_coord_attribs = 0;
#endif
  shader->type = type;

  return _cogl_shader_handle_new (shader);
}

static void
delete_shader (CoglShader *shader)
{
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

#ifdef HAVE_COGL_GL
  if (shader->language == COGL_SHADER_LANGUAGE_ARBFP)
    {
      if (shader->gl_handle)
        GE (ctx, glDeletePrograms (1, &shader->gl_handle));
    }
  else
#endif
    {
      if (shader->gl_handle)
        GE (ctx, glDeleteShader (shader->gl_handle));
    }

  shader->gl_handle = 0;
}

void
cogl_shader_source (CoglHandle   handle,
                    const char  *source)
{
  CoglShader *shader;
  CoglShaderLanguage language;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  if (!cogl_is_shader (handle))
    return;

  shader = _cogl_shader_pointer_from_handle (handle);

#ifdef HAVE_COGL_GL
  if (strncmp (source, "!!ARBfp1.0", 10) == 0)
    language = COGL_SHADER_LANGUAGE_ARBFP;
  else
#endif
    language = COGL_SHADER_LANGUAGE_GLSL;

  /* Delete the old object if the language is changing... */
  if (G_UNLIKELY (language != shader->language) &&
      shader->gl_handle)
    delete_shader (shader);

  shader->source = g_strdup (source);

  shader->language = language;
}

void
cogl_shader_compile (CoglHandle handle)
{
  CoglShader *shader = handle;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  if (!cogl_is_shader (handle))
    return;

  if (ctx->driver == COGL_DRIVER_GL)
    _cogl_shader_compile_real (shader, 0 /* ignored */);

  /* XXX: For GLES2 we don't actually compile anything until the
   * shader gets used so we have an opportunity to add some
   * boilerplate to the shader.
   *
   * At the end of the day this is obviously a badly designed API
   * given that we are having to lie to the user. It was a mistake to
   * so thinly wrap the OpenGL shader API and the current plan is to
   * replace it with a pipeline snippets API. */
}

void
_cogl_shader_set_source_with_boilerplate (GLuint shader_gl_handle,
                                          GLenum shader_gl_type,
                                          int n_tex_coord_attribs,
                                          GLsizei count_in,
                                          const char **strings_in,
                                          const GLint *lengths_in)
{
  const char *vertex_boilerplate;
  const char *fragment_boilerplate;

  const char **strings = g_alloca (sizeof (char *) * (count_in + 3));
  GLint *lengths = g_alloca (sizeof (GLint) * (count_in + 3));
  int count = 0;
  char *tex_coord_declarations = NULL;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  if (ctx->driver == COGL_DRIVER_GLES2)
    {
      vertex_boilerplate = _COGL_VERTEX_SHADER_BOILERPLATE_GLES2;
      fragment_boilerplate = _COGL_FRAGMENT_SHADER_BOILERPLATE_GLES2;
    }
  else
    {
      vertex_boilerplate = _COGL_VERTEX_SHADER_BOILERPLATE_GL;
      fragment_boilerplate = _COGL_FRAGMENT_SHADER_BOILERPLATE_GL;
    }

  if (ctx->driver == COGL_DRIVER_GLES2 &&
      cogl_has_feature (ctx, COGL_FEATURE_ID_TEXTURE_3D))
    {
      static const char texture_3d_extension[] =
        "#extension GL_OES_texture_3D : enable\n";
      strings[count] = texture_3d_extension;
      lengths[count++] = sizeof (texture_3d_extension) - 1;
    }

  if (shader_gl_type == GL_VERTEX_SHADER)
    {
      strings[count] = vertex_boilerplate;
      lengths[count++] = strlen (vertex_boilerplate);
    }
  else if (shader_gl_type == GL_FRAGMENT_SHADER)
    {
      strings[count] = fragment_boilerplate;
      lengths[count++] = strlen (fragment_boilerplate);
    }

  if (ctx->driver == COGL_DRIVER_GLES2 &&
      n_tex_coord_attribs)
    {
      GString *declarations = g_string_new (NULL);

      g_string_append_printf (declarations,
                              "varying vec4 _cogl_tex_coord[%d];\n",
                              n_tex_coord_attribs);

      if (shader_gl_type == GL_VERTEX_SHADER)
        {
          int i;

          g_string_append_printf (declarations,
                                  "uniform mat4 cogl_texture_matrix[%d];\n",
                                  n_tex_coord_attribs);

          for (i = 0; i < n_tex_coord_attribs; i++)
            g_string_append_printf (declarations,
                                    "attribute vec4 cogl_tex_coord%d_in;\n",
                                    i);
        }

      tex_coord_declarations = g_string_free (declarations, FALSE);
      strings[count] = tex_coord_declarations;
      lengths[count++] = -1; /* null terminated */
    }

  memcpy (strings + count, strings_in, sizeof (char *) * count_in);
  if (lengths_in)
    memcpy (lengths + count, lengths_in, sizeof (GLint) * count_in);
  else
    {
      int i;

      for (i = 0; i < count_in; i++)
        lengths[count + i] = -1; /* null terminated */
    }
  count += count_in;

  if (G_UNLIKELY (COGL_DEBUG_ENABLED (COGL_DEBUG_SHOW_SOURCE)))
    {
      GString *buf = g_string_new (NULL);
      int i;

      g_string_append_printf (buf,
                              "%s shader:\n",
                              shader_gl_type == GL_VERTEX_SHADER ?
                              "vertex" : "fragment");
      for (i = 0; i < count; i++)
        if (lengths[i] != -1)
          g_string_append_len (buf, strings[i], lengths[i]);
        else
          g_string_append (buf, strings[i]);

      g_message ("%s", buf->str);

      g_string_free (buf, TRUE);
    }

  GE( ctx, glShaderSource (shader_gl_handle, count,
                           (const char **) strings, lengths) );

  g_free (tex_coord_declarations);
}

void
_cogl_shader_compile_real (CoglHandle handle,
                           int n_tex_coord_attribs)
{
  CoglShader *shader = handle;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

#ifdef HAVE_COGL_GL
  if (shader->language == COGL_SHADER_LANGUAGE_ARBFP)
    {
#ifdef COGL_GL_DEBUG
      GLenum gl_error;
#endif

      if (shader->gl_handle)
        return;

      GE (ctx, glGenPrograms (1, &shader->gl_handle));

      GE (ctx, glBindProgram (GL_FRAGMENT_PROGRAM_ARB, shader->gl_handle));

      if (G_UNLIKELY (COGL_DEBUG_ENABLED (COGL_DEBUG_SHOW_SOURCE)))
        g_message ("user ARBfp program:\n%s", shader->source);

#ifdef COGL_GL_DEBUG
      while ((gl_error = ctx->glGetError ()) != GL_NO_ERROR)
        ;
#endif
      ctx->glProgramString (GL_FRAGMENT_PROGRAM_ARB,
                            GL_PROGRAM_FORMAT_ASCII_ARB,
                            strlen (shader->source),
                            shader->source);
#ifdef COGL_GL_DEBUG
      gl_error = ctx->glGetError ();
      if (gl_error != GL_NO_ERROR)
        {
          g_warning ("%s: GL error (%d): Failed to compile ARBfp:\n%s\n%s",
                     G_STRLOC,
                     gl_error,
                     shader->source,
                     ctx->glGetString (GL_PROGRAM_ERROR_STRING_ARB));
        }
#endif
    }
  else
#endif
    {
      GLenum gl_type;

      if (shader->gl_handle
#ifdef HAVE_COGL_GLES2
          &&
          (ctx->driver != COGL_DRIVER_GLES2 ||
           shader->n_tex_coord_attribs == n_tex_coord_attribs)
#endif
         )
        return;

      if (shader->gl_handle)
        delete_shader (shader);

      switch (shader->type)
        {
        case COGL_SHADER_TYPE_VERTEX:
          gl_type = GL_VERTEX_SHADER;
          break;
        case COGL_SHADER_TYPE_FRAGMENT:
          gl_type = GL_FRAGMENT_SHADER;
          break;
        default:
          g_assert_not_reached ();
          break;
        }

      shader->gl_handle = ctx->glCreateShader (gl_type);

      _cogl_shader_set_source_with_boilerplate (shader->gl_handle,
                                                gl_type,
                                                n_tex_coord_attribs,
                                                1,
                                                (const char **) &shader->source,
                                                NULL);

      GE (ctx, glCompileShader (shader->gl_handle));

#ifdef HAVE_COGL_GLES2
      shader->n_tex_coord_attribs = n_tex_coord_attribs;
#endif

#ifdef COGL_GL_DEBUG
      if (!cogl_shader_is_compiled (handle))
        {
          char *log = cogl_shader_get_info_log (handle);
          g_warning ("Failed to compile GLSL program:\nsrc:\n%s\nerror:\n%s\n",
                     shader->source,
                     log);
        }
#endif
    }
}

char *
cogl_shader_get_info_log (CoglHandle handle)
{
  CoglShader *shader;

  _COGL_GET_CONTEXT (ctx, NULL);

  if (!cogl_is_shader (handle))
    return NULL;

  shader = _cogl_shader_pointer_from_handle (handle);

#ifdef HAVE_COGL_GL
  if (shader->language == COGL_SHADER_LANGUAGE_ARBFP)
    {
      /* ARBfp exposes a program error string, but since cogl_program
       * doesn't have any API to query an error log it is not currently
       * exposed. */
      return g_strdup ("");
    }
  else
#endif
    {
      char buffer[512];
      int len = 0;

      /* We don't normally compile the shader when the user calls
       * cogl_shader_compile() because we want to be able to add
       * boilerplate code that depends on how it ends up finally being
       * used.
       *
       * Here we force an early compile if the user is interested in
       * log information to increase the chance that the log will be
       * useful! We have to guess the number of texture coordinate
       * attributes that may be used since that affects the
       * boilerplate. We use four so that the shader will still
       * compile if the user is using more than one
       * layer. Unfortunately this is likely to end up causing it to
       * be compiled again when we know the actual number of layers */
      if (!shader->gl_handle)
        _cogl_shader_compile_real (handle, 4);

      ctx->glGetShaderInfoLog (shader->gl_handle, 511, &len, buffer);
      buffer[len] = '\0';
      return g_strdup (buffer);
    }
}

CoglShaderType
cogl_shader_get_type (CoglHandle  handle)
{
  CoglShader *shader;

  _COGL_GET_CONTEXT (ctx, COGL_SHADER_TYPE_VERTEX);

  if (!cogl_is_shader (handle))
    {
      g_warning ("Non shader handle type passed to cogl_shader_get_type");
      return COGL_SHADER_TYPE_VERTEX;
    }

  shader = _cogl_shader_pointer_from_handle (handle);
  return shader->type;
}

gboolean
cogl_shader_is_compiled (CoglHandle handle)
{
#if defined (HAVE_COGL_GL) || defined (HAVE_COGL_GLES2)
  GLint status;
  CoglShader *shader;

  _COGL_GET_CONTEXT (ctx, FALSE);

  if (!cogl_is_shader (handle))
    return FALSE;

  shader = _cogl_shader_pointer_from_handle (handle);

#ifdef HAVE_COGL_GL
  if (shader->language == COGL_SHADER_LANGUAGE_ARBFP)
    return TRUE;
  else
#endif
    {
      /* FIXME: We currently have an arbitrary limit of 4 texture
       * coordinate attributes since our API means we have to add
       * some boilerplate to the users GLSL program (for GLES2)
       * before we actually know how many attributes are in use.
       *
       * 4 will probably be enough (or at least that limitation should
       * be enough until we can replace this API with the pipeline
       * snippets API) but if it isn't then the shader won't compile,
       * through no fault of the user.
       *
       * To some extent this is just a symptom of bad API design; it
       * was a mistake for Cogl to so thinly wrap the OpenGL shader
       * API. Eventually we plan for this whole API will be deprecated
       * by the pipeline snippets framework.
       */
      if (!shader->gl_handle)
        _cogl_shader_compile_real (handle, 4);

      GE (ctx, glGetShaderiv (shader->gl_handle, GL_COMPILE_STATUS, &status));
      if (status == GL_TRUE)
        return TRUE;
      else
        return FALSE;
    }
#else
  return FALSE;
#endif
}
