/*
 * Cogl
 *
 * An object oriented GL/GLES Abstraction/Utility Layer
 *
 * Copyright (C) 2009, 2011 Intel Corporation.
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

/* This is included multiple times with different definitions for
 * these macros. The macros are given the following arguments:
 *
 * COGL_EXT_BEGIN:
 *
 * @name: a unique symbol name for this feature
 *
 * @min_gl_major: the major part of the minimum GL version where these
 * functions are available in core, or 255 if it isn't available in
 * any version.
 * @min_gl_minor: the minor part of the minimum GL version where these
 * functions are available in core, or 255 if it isn't available in
 * any version.
 *
 * @gles_availability: flags to specify which versions of GLES the
 * functions are available in. Should be a combination of
 * COGL_EXT_IN_GLES and COGL_EXT_IN_GLES2.
 *
 * @extension_suffixes: A zero-separated list of suffixes in a
 * string. These are appended to the extension name to get a complete
 * extension name to try. The suffix is also appended to all of the
 * function names. The suffix can optionally include a ':' to specify
 * an alternate suffix for the function names.
 *
 * @extension_names: A list of extension names to try. If any of these
 * extensions match then it will be used.
 */

/* These are the core GL functions which we assume will always be
   available */
COGL_EXT_BEGIN (core,
                0, 0,
                COGL_EXT_IN_GLES | COGL_EXT_IN_GLES2,
                "\0",
                "\0")
COGL_EXT_FUNCTION (void, glBindTexture,
                   (GLenum target, GLuint texture))
COGL_EXT_FUNCTION (void, glBlendFunc,
                   (GLenum sfactor, GLenum dfactor))
COGL_EXT_FUNCTION (void, glClear,
                   (GLbitfield mask))
COGL_EXT_FUNCTION (void, glClearColor,
                   (GLclampf red,
                    GLclampf green,
                    GLclampf blue,
                    GLclampf alpha))
COGL_EXT_FUNCTION (void, glClearStencil,
                   (GLint s))
COGL_EXT_FUNCTION (void, glColorMask,
                   (GLboolean red,
                    GLboolean green,
                    GLboolean blue,
                    GLboolean alpha))
COGL_EXT_FUNCTION (void, glCopyTexSubImage2D,
                   (GLenum target,
                    GLint level,
                    GLint xoffset,
                    GLint yoffset,
                    GLint x,
                    GLint y,
                    GLsizei width,
                    GLsizei height))
COGL_EXT_FUNCTION (void, glDeleteTextures,
                   (GLsizei n, const GLuint* textures))
COGL_EXT_FUNCTION (void, glDepthFunc,
                   (GLenum func))
COGL_EXT_FUNCTION (void, glDepthMask,
                   (GLboolean flag))
COGL_EXT_FUNCTION (void, glDisable,
                   (GLenum cap))
COGL_EXT_FUNCTION (void, glDrawArrays,
                   (GLenum mode, GLint first, GLsizei count))
COGL_EXT_FUNCTION (void, glDrawElements,
                   (GLenum mode,
                    GLsizei count,
                    GLenum type,
                    const GLvoid* indices))
COGL_EXT_FUNCTION (void, glEnable,
                   (GLenum cap))
COGL_EXT_FUNCTION (void, glFinish,
                   (void))
COGL_EXT_FUNCTION (void, glFlush,
                   (void))
COGL_EXT_FUNCTION (void, glFrontFace,
                   (GLenum mode))
COGL_EXT_FUNCTION (void, glCullFace,
                   (GLenum mode))
COGL_EXT_FUNCTION (void, glGenTextures,
                   (GLsizei n, GLuint* textures))
COGL_EXT_FUNCTION (GLenum, glGetError,
                   (void))
COGL_EXT_FUNCTION (void, glGetIntegerv,
                   (GLenum pname, GLint* params))
COGL_EXT_FUNCTION (void, glGetBooleanv,
                   (GLenum pname, GLboolean* params))
COGL_EXT_FUNCTION (void, glGetFloatv,
                   (GLenum pname, GLfloat* params))
COGL_EXT_FUNCTION (const GLubyte*, glGetString,
                   (GLenum name))
COGL_EXT_FUNCTION (void, glHint,
                   (GLenum target, GLenum mode))
COGL_EXT_FUNCTION (GLboolean, glIsTexture,
                   (GLuint texture))
COGL_EXT_FUNCTION (void, glPixelStorei,
                   (GLenum pname, GLint param))
COGL_EXT_FUNCTION (void, glReadPixels,
                   (GLint x,
                    GLint y,
                    GLsizei width,
                    GLsizei height,
                    GLenum format,
                    GLenum type,
                    GLvoid* pixels))
COGL_EXT_FUNCTION (void, glScissor,
                   (GLint x, GLint y, GLsizei width, GLsizei height))
COGL_EXT_FUNCTION (void, glStencilFunc,
                   (GLenum func, GLint ref, GLuint mask))
COGL_EXT_FUNCTION (void, glStencilMask,
                   (GLuint mask))
COGL_EXT_FUNCTION (void, glStencilOp,
                   (GLenum fail, GLenum zfail, GLenum zpass))
COGL_EXT_FUNCTION (void, glTexImage2D,
                   (GLenum target,
                    GLint level,
                    GLint internalformat,
                    GLsizei width,
                    GLsizei height,
                    GLint border,
                    GLenum format,
                    GLenum type,
                    const GLvoid* pixels))
COGL_EXT_FUNCTION (void, glTexParameterfv,
                   (GLenum target, GLenum pname, const GLfloat* params))
COGL_EXT_FUNCTION (void, glTexParameteri,
                   (GLenum target, GLenum pname, GLint param))
COGL_EXT_FUNCTION (void, glTexParameteriv,
                   (GLenum target, GLenum pname, const GLint* params))
COGL_EXT_FUNCTION (void, glGetTexParameterfv,
                   (GLenum target, GLenum pname, GLfloat* params))
COGL_EXT_FUNCTION (void, glGetTexParameteriv,
                   (GLenum target, GLenum pname, GLint* params))
COGL_EXT_FUNCTION (void, glTexSubImage2D,
                   (GLenum target,
                    GLint level,
                    GLint xoffset,
                    GLint yoffset,
                    GLsizei width,
                    GLsizei height,
                    GLenum format,
                    GLenum type,
                    const GLvoid* pixels))
COGL_EXT_FUNCTION (void, glCopyTexImage2D,
                   (GLenum target,
                    GLint level,
                    GLenum internalformat,
                    GLint x,
                    GLint y,
                    GLsizei width,
                    GLsizei height,
                    GLint border))
COGL_EXT_FUNCTION (void, glViewport,
                   (GLint x, GLint y, GLsizei width, GLsizei height))
COGL_EXT_FUNCTION (GLboolean, glIsEnabled, (GLenum cap))
COGL_EXT_FUNCTION (void, glLineWidth, (GLfloat width))
COGL_EXT_FUNCTION (void, glPolygonOffset, (GLfloat factor, GLfloat units))
COGL_EXT_END ()
