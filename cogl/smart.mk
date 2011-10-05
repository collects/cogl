#
#  This file was added by Duzy Chan <code@duzy.info> on 2011-09-29
#

$(call sm-new-module, cogl, shared, gcc)

cogl.driver.sources :=

ifneq ($(COGL_DRIVER_GL_SUPPORTED),)
  cogl.driver.sources += \
    driver/gl/cogl-gl.c			\
    driver/gl/cogl-texture-driver-gl.c	\
    $(null)
endif

ifneq ($(COGL_DRIVER_GLES_SUPPORTED),)
  cogl.driver.sources += \
    driver/gles/cogl-gles.c			\
    driver/gles/cogl-texture-driver-gles.c	\
    $(null)
endif

cogl.winsys.common.sources := \
  winsys/cogl-winsys-private.h \
  winsys/cogl-winsys.c

cogl.tesselator.sources :=	\
  tesselator/dict-list.h 	\
  tesselator/dict.c 		\
  tesselator/dict.h 		\
  tesselator/geom.c 		\
  tesselator/geom.h 		\
  tesselator/gluos.h 		\
  tesselator/memalloc.h 	\
  tesselator/mesh.c 		\
  tesselator/mesh.h 		\
  tesselator/normal.c 		\
  tesselator/normal.h 		\
  tesselator/priorityq-heap.h 	\
  tesselator/priorityq-sort.h 	\
  tesselator/priorityq.c 	\
  tesselator/priorityq.h 	\
  tesselator/render.c 		\
  tesselator/render.h 		\
  tesselator/sweep.c 		\
  tesselator/sweep.h 		\
  tesselator/tess.c 		\
  tesselator/tess.h 		\
  tesselator/tesselator.h 	\
  tesselator/tessmono.c 	\
  tesselator/tessmono.h 	\
  tesselator/GL/glu.h 		\
  $(null)

sm.this.public.headers := \
  cogl-object.h 		\
  cogl-bitmap.h 		\
  cogl-buffer.h 		\
  cogl-color.h 			\
  cogl-fixed.h 			\
  cogl-depth-state.h 		\
  cogl-material-compat.h 	\
  cogl-vector.h 		\
  cogl-euler.h 			\
  cogl-quaternion.h 		\
  cogl-matrix.h 		\
  cogl-offscreen.h 		\
  cogl-primitives.h 		\
  cogl-path.h 			\
  cogl-pixel-buffer.h		\
  cogl-shader.h 		\
  cogl-texture.h 		\
  cogl-texture-3d.h             \
  cogl-texture-2d.h             \
  cogl-types.h 			\
  cogl-vertex-buffer.h 		\
  cogl-index-buffer.h 		\
  cogl-attribute-buffer.h 	\
  cogl-indices.h 		\
  cogl-attribute.h 		\
  cogl-primitive.h 		\
  cogl-clip-state.h		\
  cogl-framebuffer.h		\
  cogl-clutter.h       		\
  cogl.h			\

sm.this.public.headers += \
  cogl-renderer.h 		\
  cogl-swap-chain.h 		\
  cogl-onscreen-template.h 	\
  cogl-display.h 		\
  cogl-context.h 		\
  cogl-pipeline.h 		\
  cogl-pipeline-state.h 	\
  cogl-pipeline-layer-state.h 	\
  cogl2-path.h 			\
  cogl2-clip-state.h		\
  cogl2-experimental.h		\

sm.this.public.headers += \
  cogl-deprecated.h \
  cogl-pango.h \
  cogl-defines.h \
  cogl-enum-types.h

sm.this.sources := \
  $(cogl.driver.sources)	\
  $(cogl.winsys.common.sources)	\
  $(cogl.tesselator.sources)	\
  cogl-private.h			\
  cogl-debug.h 				\
  cogl-debug-options.h			\
  cogl-handle.h 			\
  cogl-context-private.h		\
  cogl-context.c			\
  cogl-renderer-private.h		\
  cogl-renderer.h			\
  cogl-renderer.c			\
  cogl-swap-chain-private.h		\
  cogl-swap-chain.h			\
  cogl-swap-chain.c			\
  cogl-onscreen-template-private.h 	\
  cogl-onscreen-template.h 		\
  cogl-onscreen-template.c 		\
  cogl-display-private.h		\
  cogl-display.h			\
  cogl-display.c			\
  cogl-internal.h			\
  cogl.c				\
  cogl-object-private.h			\
  cogl-object.h				\
  cogl-object.c				\
  cogl-util.h 				\
  cogl-util.c 				\
  cogl-bitmap-private.h 		\
  cogl-bitmap.c 			\
  cogl-bitmap-fallback.c 		\
  cogl-primitives-private.h 		\
  cogl-primitives.h 			\
  cogl-primitives.c 			\
  cogl-path-private.h 			\
  cogl-path.h 				\
  cogl-path.c 				\
  cogl2-path.h 				\
  cogl2-path.c 				\
  cogl-bitmap-pixbuf.c 			\
  cogl-clip-stack.h 			\
  cogl-clip-stack.c			\
  cogl-clip-state-private.h		\
  cogl-clip-state.h			\
  cogl-clip-state.c			\
  cogl2-clip-state.h 			\
  cogl2-clip-state.c 			\
  cogl-ext-functions.h			\
  cogl-feature-private.h                \
  cogl-feature-private.c                \
  cogl-fixed.c		    		\
  cogl-color-private.h    		\
  cogl-color.c				\
  cogl-buffer-private.h 		\
  cogl-buffer.c				\
  cogl-pixel-buffer-private.h		\
  cogl-pixel-buffer.c			\
  cogl-vertex-buffer-private.h 		\
  cogl-vertex-buffer.c			\
  cogl-index-buffer-private.h		\
  cogl-index-buffer.c			\
  cogl-attribute-buffer-private.h	\
  cogl-attribute-buffer.c		\
  cogl-indices-private.h		\
  cogl-indices.c			\
  cogl-attribute-private.h		\
  cogl-attribute.c			\
  cogl-primitive-private.h		\
  cogl-primitive.c			\
  cogl-matrix.c				\
  cogl-vector.c				\
  cogl-euler.c				\
  cogl-quaternion-private.h 		\
  cogl-quaternion.c			\
  cogl-matrix-private.h			\
  cogl-matrix-stack.c			\
  cogl-matrix-stack.h			\
  cogl-depth-state.c			\
  cogl-depth-state-private.h		\
  cogl-node.c				\
  cogl-node-private.h			\
  cogl-pipeline.c			\
  cogl-pipeline-private.h		\
  cogl-pipeline-layer.c			\
  cogl-pipeline-layer-private.h		\
  cogl-pipeline-state.c			\
  cogl-pipeline-layer-state-private.h	\
  cogl-pipeline-layer-state.c		\
  cogl-pipeline-state-private.h		\
  cogl-pipeline-debug.c			\
  cogl-pipeline-opengl.c		\
  cogl-pipeline-opengl-private.h	\
  cogl-pipeline-fragend-glsl.c		\
  cogl-pipeline-fragend-glsl-private.h	\
  cogl-pipeline-fragend-arbfp.c		\
  cogl-pipeline-fragend-arbfp-private.h	\
  cogl-pipeline-fragend-fixed.c		\
  cogl-pipeline-fragend-fixed-private.h	\
  cogl-pipeline-vertend-glsl.c		\
  cogl-pipeline-vertend-glsl-private.h	\
  cogl-pipeline-vertend-fixed.c		\
  cogl-pipeline-vertend-fixed-private.h	\
  cogl-pipeline-progend-glsl.c		\
  cogl-pipeline-progend-glsl-private.h	\
  cogl-pipeline-cache.h			\
  cogl-pipeline-cache.c			\
  cogl-material-compat.c		\
  cogl-program.c			\
  cogl-program-private.h		\
  cogl-blend-string.c			\
  cogl-blend-string.h			\
  cogl-debug.c				\
  cogl-sub-texture-private.h            \
  cogl-texture-private.h		\
  cogl-texture-2d-private.h             \
  cogl-texture-2d-sliced-private.h 	\
  cogl-texture-3d-private.h             \
  cogl-texture-driver.h			\
  cogl-sub-texture.c                    \
  cogl-texture.c			\
  cogl-texture-2d.c                     \
  cogl-texture-2d-sliced.c		\
  cogl-texture-3d.c                     \
  cogl-texture-rectangle-private.h      \
  cogl-texture-rectangle.c              \
  cogl-rectangle-map.h                  \
  cogl-rectangle-map.c                  \
  cogl-atlas.h                          \
  cogl-atlas.c                          \
  cogl-atlas-texture-private.h          \
  cogl-atlas-texture.c                  \
  cogl-blit.h				\
  cogl-blit.c				\
  cogl-spans.h				\
  cogl-spans.c				\
  cogl-journal-private.h		\
  cogl-journal.c			\
  cogl-framebuffer-private.h		\
  cogl-framebuffer.c 			\
  cogl-profile.h 			\
  cogl-profile.c 			\
  cogl-flags.h				\
  cogl-bitmask.h                        \
  cogl-bitmask.c                        \
  cogl-shader-boilerplate.h		\
  cogl-shader-private.h			\
  cogl-shader.c                        	\
  cogl-gtype-private.h                  \
  cogl-point-in-poly-private.h       	\
  cogl-point-in-poly.c       		\
  cogl-clutter.c       			\
  cogl-queue.h				\
  winsys/cogl-winsys-stub-private.h	\
  winsys/cogl-winsys-stub.c		\
  cogl-config-private.h			\
  cogl-config.c				\
  $(null)

ifneq ($(SUPPORT_XLIB),)
sm.this.sources += \
  cogl-x11-renderer-private.h \
  cogl-xlib-renderer-private.h \
  cogl-xlib-renderer.c \
  cogl-xlib-display-private.h \
  cogl-xlib.c \
  cogl-xlib-private.h \
  winsys/cogl-texture-pixmap-x11.c \
  winsys/cogl-texture-pixmap-x11-private.h

sm.this.public.headers += \
  cogl-xlib-renderer.h

sm.this.public.headers += \
  winsys/cogl-texture-pixmap-x11.h \
  cogl-xlib.h

endif

ifneq ($(SUPPORT_GLX),)
sm.this.sources += \
  cogl-glx-renderer-private.h \
  cogl-glx-display-private.h \
  winsys/cogl-winsys-glx-feature-functions.h \
  winsys/cogl-winsys-glx.c
endif

ifneq ($(SUPPORT_WGL),)
sm.this.sources += \
  cogl-win32-renderer.c \
  winsys/cogl-winsys-wgl.c \
  winsys/cogl-winsys-wgl-feature-functions.h
endif

ifneq ($(SUPPORT_EGL_PLATFORM_WAYLAND),)
sm.this.sources += \
  winsys/cogl-winsys-egl.c \
  winsys/cogl-winsys-egl-feature-functions.h \
  winsys/cogl-winsys-egl-private.h
endif

sm.this.verbose := true

sm.this.sources := $(filter-out %.h,$(sm.this.sources))

sm.this.includes := \
  $(sm.this.dir) \
  $(sm.this.dir)/.. \
  $(sm.this.dir)/winsys \
  $(sm.this.dir)/$(COGL_DRIVER) \
  /usr/include/drm

sm.this.defines := \
  -DCLUTTER_COMPILATION \
  -DG_LOG_DOMAIN=\"Cogl\" \
  -DCOGL_GL_LIBNAME=\"$(COGL_GL_LIBNAME)\" \
  -DCOGL_GLES1_LIBNAME=\"$(COGL_GLES1_LIBNAME)\" \
  -DCOGL_GLES2_LIBNAME=\"$(COGL_GLES2_LIBNAME)\" \
  -DCOGL_LOCALEDIR=\""$(localedir)"\" \
  -DCOGL_ENABLE_DEBUG \
  -DHAVE_CONFIG_H \
  -DHAVE_COGL_GL \
  -DHAVE_DIRECTLY_LINKED_GL_LIBRARY \

sm.this.compile.flags := -fPIC \
  $(shell pkg-config --cflags glib-2.0) \
  $(shell pkg-config --cflags gdk-pixbuf-2.0) \
  $(shell pkg-config --cflags cairo) \

sm.this.link.flags := \
  -Wl,-no-undefined \
  -Wl,-export-dynamic \

#  -Wl,version-info 1:0:0 \
#  -Wl,export-symbols-regex "^(cogl|_cogl_debug_flags|_cogl_atlas_new|_cogl_atlas_add_reorganize_callback|_cogl_atlas_reserve_space|_cogl_callback|_cogl_util_get_eye_planes_for_screen_poly|_cogl_atlas_texture_remove_reorganize_callback|_cogl_atlas_texture_add_reorganize_callback|_cogl_texture_foreach_sub_texture_in_region|_cogl_atlas_texture_new_with_size|_cogl_profile_trace_message|_cogl_context_get_default).*"

sm.this.libs := \
  $(shell pkg-config --libs gdk-pixbuf-2.0) \
  $(shell pkg-config --libs gobject-2.0) \
  $(shell pkg-config --libs gthread-2.0) \
  $(shell pkg-config --libs gmodule-2.0) \
  $(shell pkg-config --libs glib-2.0) \
  $(shell pkg-config --libs cairo) \
  -lGL -ldrm -lX11 -lXext -lXdamage -lXfixes -lXcomposite -lm -ldl

sm.this.export.defines := \
  -DHAVE_CONFIG_H \
  -DHAVE_COGL_GL \
  -DCOGL_ENABLE_EXPERIMENTAL_API \

sm.this.export.includes := \
  $(sm.this.dir)/../$(sm.out.inc) \

sm.this.export.compile.flags := \
  $(shell pkg-config --cflags glib-2.0) \
  $(shell pkg-config --cflags gdk-pixbuf-2.0) \
  $(shell pkg-config --cflags cairo) \

sm.this.export.link.flags := \
  -Wl,-rpath=$(sm.out.lib) \

sm.this.export.libdirs := $(sm.this.dir)/../$(sm.out.lib)
sm.this.export.libs := cogl \
  $(sm.this.libs)

$(call sm-copy-headers, $(sm.this.public.headers), cogl)

$(sm-generate-implib)
$(sm-build-this)
