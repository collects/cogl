include $(top_srcdir)/build/autotools/Makefile.am.silent

# preamble

NULL =

SUBDIRS =

BUILT_SOURCES =

EXTRA_DIST =
CLEANFILES =
DISTCLEANFILES =

noinst_LTLIBRARIES =
lib_LTLIBRARIES =

INCLUDES = \
	-I$(top_srcdir)				\
	-I$(top_builddir)			\
	-I$(srcdir)/tesselator			\
	-I$(srcdir)/winsys 			\
	-I$(srcdir)/driver/$(COGL_DRIVER) 	\
	$(NULL)

AM_CPPFLAGS = \
	-DG_LOG_DOMAIN=\"Cogl\" 	\
	-DCLUTTER_COMPILATION		\
	-DCOGL_GL_LIBNAME=\"$(COGL_GL_LIBNAME)\" \
	-DCOGL_GLES1_LIBNAME=\"$(COGL_GLES1_LIBNAME)\" \
	-DCOGL_GLES2_LIBNAME=\"$(COGL_GLES2_LIBNAME)\" \
	-DCOGL_LOCALEDIR=\""$(localedir)"\" \
	$(NULL)

AM_CFLAGS = $(COGL_DEP_CFLAGS) $(COGL_EXTRA_CFLAGS) $(MAINTAINER_CFLAGS)

BUILT_SOURCES += cogl-defines.h
DISTCLEANFILES += cogl-defines.h
EXTRA_DIST += cogl-defines.h.in

# Note: The cogl-1.0/cogl-gl-1.0 files are essentially for
# compatability only.  I'm not really sure who could possibly be using
# them so we may decide to remove them entirely at some point.
pc_files = \
	cogl-1.0.pc \
	cogl-gl-1.0.pc

pc_files += cogl-$(COGL_API_VERSION)-experimental.pc

cogl-gl-1.0.pc: cogl-1.0.pc
	$(QUIET_GEN)cp -f $< $(@F)

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = $(pc_files)

EXTRA_DIST += cogl-1.0.pc.in cogl-$(COGL_API_VERSION)-experimental.pc.in
DISTCLEANFILES += $(pc_files)

# public headers
cogl_public_h = \
	$(srcdir)/cogl-object.h 		\
	$(srcdir)/cogl-bitmap.h 		\
	$(srcdir)/cogl-buffer.h 		\
	$(srcdir)/cogl-color.h 			\
	$(srcdir)/cogl-fixed.h 			\
	$(srcdir)/cogl-depth-state.h 		\
	$(srcdir)/cogl-material-compat.h 	\
	$(srcdir)/cogl-vector.h 		\
	$(srcdir)/cogl-euler.h 			\
	$(srcdir)/cogl-quaternion.h 		\
	$(srcdir)/cogl-matrix.h 		\
	$(srcdir)/cogl-offscreen.h 		\
	$(srcdir)/cogl-primitives.h 		\
	$(srcdir)/cogl-path.h 			\
	$(srcdir)/cogl-pixel-buffer.h		\
	$(srcdir)/cogl-poll.h			\
	$(srcdir)/cogl-shader.h 		\
	$(srcdir)/cogl-texture.h 		\
	$(srcdir)/cogl-texture-3d.h             \
	$(srcdir)/cogl-texture-2d.h             \
	$(srcdir)/cogl-texture-rectangle.h      \
	$(srcdir)/cogl-texture-2d-sliced.h      \
	$(srcdir)/cogl-sub-texture.h            \
	$(srcdir)/cogl-meta-texture.h		\
	$(srcdir)/cogl-types.h 			\
	$(srcdir)/cogl-vertex-buffer.h 		\
	$(srcdir)/cogl-index-buffer.h 		\
	$(srcdir)/cogl-attribute-buffer.h 	\
	$(srcdir)/cogl-indices.h 		\
	$(srcdir)/cogl-attribute.h 		\
	$(srcdir)/cogl-primitive.h 		\
	$(srcdir)/cogl-clip-state.h		\
	$(srcdir)/cogl-framebuffer.h		\
	$(srcdir)/cogl-onscreen.h		\
	$(srcdir)/cogl-clutter.h       		\
	$(srcdir)/cogl.h			\
	$(NULL)

cogl_experimental_h = \
	$(srcdir)/cogl-renderer.h 		\
	$(srcdir)/cogl-swap-chain.h 		\
	$(srcdir)/cogl-onscreen-template.h 	\
	$(srcdir)/cogl-display.h 		\
	$(srcdir)/cogl-context.h 		\
	$(srcdir)/cogl-pipeline.h 		\
	$(srcdir)/cogl-pipeline-state.h 	\
	$(srcdir)/cogl-pipeline-layer-state.h 	\
	$(srcdir)/cogl-snippet.h		\
	$(srcdir)/cogl2-path.h 			\
	$(srcdir)/cogl2-clip-state.h		\
	$(srcdir)/cogl2-experimental.h		\
	$(NULL)

# driver sources
cogl_driver_sources =

if COGL_DRIVER_GL_SUPPORTED
cogl_driver_sources += \
	$(srcdir)/driver/gl/cogl-gl.c			\
	$(srcdir)/driver/gl/cogl-texture-driver-gl.c	\
	$(NULL)
endif

if COGL_DRIVER_GLES_SUPPORTED
cogl_driver_sources += \
	$(srcdir)/driver/gles/cogl-gles.c			\
	$(srcdir)/driver/gles/cogl-texture-driver-gles.c	\
	$(NULL)
endif

# winsys sources, common to all backends
cogl_winsys_common_sources = \
	$(srcdir)/winsys/cogl-winsys-private.h \
	$(srcdir)/winsys/cogl-winsys.c \
	$(NULL)

# tesselator sources
cogl_tesselator_sources = \
	$(srcdir)/tesselator/dict-list.h 	\
	$(srcdir)/tesselator/dict.c 		\
	$(srcdir)/tesselator/dict.h 		\
	$(srcdir)/tesselator/geom.c 		\
	$(srcdir)/tesselator/geom.h 		\
	$(srcdir)/tesselator/gluos.h 		\
	$(srcdir)/tesselator/memalloc.h 	\
	$(srcdir)/tesselator/mesh.c 		\
	$(srcdir)/tesselator/mesh.h 		\
	$(srcdir)/tesselator/normal.c 		\
	$(srcdir)/tesselator/normal.h 		\
	$(srcdir)/tesselator/priorityq-heap.h 	\
	$(srcdir)/tesselator/priorityq-sort.h 	\
	$(srcdir)/tesselator/priorityq.c 	\
	$(srcdir)/tesselator/priorityq.h 	\
	$(srcdir)/tesselator/render.c 		\
	$(srcdir)/tesselator/render.h 		\
	$(srcdir)/tesselator/sweep.c 		\
	$(srcdir)/tesselator/sweep.h 		\
	$(srcdir)/tesselator/tess.c 		\
	$(srcdir)/tesselator/tess.h 		\
	$(srcdir)/tesselator/tesselator.h 	\
	$(srcdir)/tesselator/tessmono.c 	\
	$(srcdir)/tesselator/tessmono.h 	\
	$(srcdir)/tesselator/GL/glu.h 		\
	$(NULL)

EXTRA_DIST += \
	$(srcdir)/tesselator/README 		\
	$(srcdir)/tesselator/priorityq-heap.c	\
	$(NULL)

# sources
cogl_sources_c = \
	$(cogl_driver_sources)				\
	$(cogl_winsys_common_sources)			\
	$(cogl_tesselator_sources)			\
	$(srcdir)/cogl-private.h			\
	$(srcdir)/cogl-debug.h 				\
	$(srcdir)/cogl-debug-options.h			\
	$(srcdir)/cogl-handle.h 			\
	$(srcdir)/cogl-context-private.h		\
	$(srcdir)/cogl-context.c			\
	$(srcdir)/cogl-renderer-private.h		\
	$(srcdir)/cogl-renderer.h			\
	$(srcdir)/cogl-renderer.c			\
	$(srcdir)/cogl-swap-chain-private.h		\
	$(srcdir)/cogl-swap-chain.h			\
	$(srcdir)/cogl-swap-chain.c			\
	$(srcdir)/cogl-onscreen-template-private.h 	\
	$(srcdir)/cogl-onscreen-template.h 		\
	$(srcdir)/cogl-onscreen-template.c 		\
	$(srcdir)/cogl-display-private.h		\
	$(srcdir)/cogl-display.h			\
	$(srcdir)/cogl-display.c			\
	$(srcdir)/cogl-internal.h			\
	$(srcdir)/cogl.c				\
	$(srcdir)/cogl-object-private.h			\
	$(srcdir)/cogl-object.h				\
	$(srcdir)/cogl-object.c				\
	$(srcdir)/cogl-util.h 				\
	$(srcdir)/cogl-util.c 				\
	$(srcdir)/cogl-bitmap-private.h 		\
	$(srcdir)/cogl-bitmap.c 			\
	$(srcdir)/cogl-bitmap-fallback.c 		\
	$(srcdir)/cogl-primitives-private.h 		\
	$(srcdir)/cogl-primitives.h 			\
	$(srcdir)/cogl-primitives.c 			\
	$(srcdir)/cogl-path-private.h 			\
	$(srcdir)/cogl-path.h 				\
	$(srcdir)/cogl-path.c 				\
	$(srcdir)/cogl2-path.h 				\
	$(srcdir)/cogl2-path.c 				\
	$(srcdir)/cogl-bitmap-pixbuf.c 			\
	$(srcdir)/cogl-clip-stack.h 			\
	$(srcdir)/cogl-clip-stack.c			\
	$(srcdir)/cogl-clip-state-private.h		\
	$(srcdir)/cogl-clip-state.h			\
	$(srcdir)/cogl-clip-state.c			\
	$(srcdir)/cogl2-clip-state.h 			\
	$(srcdir)/cogl2-clip-state.c 			\
	$(srcdir)/cogl-feature-private.h                \
	$(srcdir)/cogl-feature-private.c                \
	$(srcdir)/cogl-fixed.c		    		\
	$(srcdir)/cogl-color-private.h    		\
	$(srcdir)/cogl-color.c				\
	$(srcdir)/cogl-buffer-private.h 		\
	$(srcdir)/cogl-buffer.c				\
	$(srcdir)/cogl-pixel-buffer-private.h		\
	$(srcdir)/cogl-pixel-buffer.c			\
	$(srcdir)/cogl-vertex-buffer-private.h 		\
	$(srcdir)/cogl-vertex-buffer.c			\
	$(srcdir)/cogl-index-buffer-private.h		\
	$(srcdir)/cogl-index-buffer.c			\
	$(srcdir)/cogl-attribute-buffer-private.h	\
	$(srcdir)/cogl-attribute-buffer.c		\
	$(srcdir)/cogl-indices-private.h		\
	$(srcdir)/cogl-indices.c			\
	$(srcdir)/cogl-attribute-private.h		\
	$(srcdir)/cogl-attribute.c			\
	$(srcdir)/cogl-primitive-private.h		\
	$(srcdir)/cogl-primitive.c			\
	$(srcdir)/cogl-matrix.c				\
	$(srcdir)/cogl-vector.c				\
	$(srcdir)/cogl-euler.c				\
	$(srcdir)/cogl-quaternion-private.h 		\
	$(srcdir)/cogl-quaternion.c			\
	$(srcdir)/cogl-matrix-private.h			\
	$(srcdir)/cogl-matrix-stack.c			\
	$(srcdir)/cogl-matrix-stack.h			\
	$(srcdir)/cogl-depth-state.c			\
	$(srcdir)/cogl-depth-state-private.h		\
	$(srcdir)/cogl-node.c				\
	$(srcdir)/cogl-node-private.h			\
	$(srcdir)/cogl-pipeline.c			\
	$(srcdir)/cogl-pipeline-private.h		\
	$(srcdir)/cogl-pipeline-layer.c			\
	$(srcdir)/cogl-pipeline-layer-private.h		\
	$(srcdir)/cogl-pipeline-state.c			\
	$(srcdir)/cogl-pipeline-layer-state-private.h	\
	$(srcdir)/cogl-pipeline-layer-state.c		\
	$(srcdir)/cogl-pipeline-state-private.h		\
	$(srcdir)/cogl-pipeline-debug.c			\
	$(srcdir)/cogl-pipeline-opengl.c		\
	$(srcdir)/cogl-pipeline-opengl-private.h	\
	$(srcdir)/cogl-pipeline-fragend-glsl.c		\
	$(srcdir)/cogl-pipeline-fragend-glsl-private.h	\
	$(srcdir)/cogl-pipeline-fragend-arbfp.c		\
	$(srcdir)/cogl-pipeline-fragend-arbfp-private.h	\
	$(srcdir)/cogl-pipeline-fragend-fixed.c		\
	$(srcdir)/cogl-pipeline-fragend-fixed-private.h	\
	$(srcdir)/cogl-pipeline-vertend-glsl.c		\
	$(srcdir)/cogl-pipeline-vertend-glsl-private.h	\
	$(srcdir)/cogl-pipeline-vertend-fixed.c		\
	$(srcdir)/cogl-pipeline-vertend-fixed-private.h	\
	$(srcdir)/cogl-pipeline-progend-fixed.c		\
	$(srcdir)/cogl-pipeline-progend-fixed-private.h	\
	$(srcdir)/cogl-pipeline-progend-glsl.c		\
	$(srcdir)/cogl-pipeline-progend-glsl-private.h	\
	$(srcdir)/cogl-pipeline-snippet-private.h	\
	$(srcdir)/cogl-pipeline-snippet.c		\
	$(srcdir)/cogl-pipeline-cache.h			\
	$(srcdir)/cogl-pipeline-cache.c			\
	$(srcdir)/cogl-material-compat.c		\
	$(srcdir)/cogl-program.c			\
	$(srcdir)/cogl-program-private.h		\
	$(srcdir)/cogl-blend-string.c			\
	$(srcdir)/cogl-blend-string.h			\
	$(srcdir)/cogl-debug.c				\
	$(srcdir)/cogl-sub-texture-private.h            \
	$(srcdir)/cogl-texture-private.h		\
	$(srcdir)/cogl-texture-2d-private.h             \
	$(srcdir)/cogl-texture-2d-sliced-private.h 	\
	$(srcdir)/cogl-texture-3d-private.h             \
	$(srcdir)/cogl-texture-driver.h			\
	$(srcdir)/cogl-sub-texture.c                    \
	$(srcdir)/cogl-texture.c			\
	$(srcdir)/cogl-texture-2d.c                     \
	$(srcdir)/cogl-texture-2d-sliced.c		\
	$(srcdir)/cogl-texture-3d.c                     \
	$(srcdir)/cogl-texture-rectangle-private.h      \
	$(srcdir)/cogl-texture-rectangle.c              \
	$(srcdir)/cogl-rectangle-map.h                  \
	$(srcdir)/cogl-rectangle-map.c                  \
	$(srcdir)/cogl-atlas.h                          \
	$(srcdir)/cogl-atlas.c                          \
	$(srcdir)/cogl-atlas-texture-private.h          \
	$(srcdir)/cogl-atlas-texture.c                  \
	$(srcdir)/cogl-meta-texture.c			\
	$(srcdir)/cogl-blit.h				\
	$(srcdir)/cogl-blit.c				\
	$(srcdir)/cogl-spans.h				\
	$(srcdir)/cogl-spans.c				\
	$(srcdir)/cogl-journal-private.h		\
	$(srcdir)/cogl-journal.c			\
	$(srcdir)/cogl-framebuffer-private.h		\
	$(srcdir)/cogl-framebuffer.c 			\
	$(srcdir)/cogl-onscreen-private.h		\
	$(srcdir)/cogl-onscreen.c 			\
	$(srcdir)/cogl-profile.h 			\
	$(srcdir)/cogl-profile.c 			\
	$(srcdir)/cogl-flags.h				\
	$(srcdir)/cogl-bitmask.h                        \
	$(srcdir)/cogl-bitmask.c                        \
	$(srcdir)/cogl-shader-boilerplate.h		\
	$(srcdir)/cogl-shader-private.h			\
	$(srcdir)/cogl-shader.c                        	\
	$(srcdir)/cogl-gtype-private.h                  \
	$(srcdir)/cogl-point-in-poly-private.h       	\
	$(srcdir)/cogl-point-in-poly.c       		\
	$(srcdir)/cogl-clutter.c       			\
	$(srcdir)/winsys/cogl-winsys-stub-private.h	\
	$(srcdir)/cogl-queue.h				\
	$(srcdir)/winsys/cogl-winsys-stub.c		\
	$(srcdir)/cogl-config-private.h			\
	$(srcdir)/cogl-config.c				\
	$(srcdir)/cogl-boxed-value.h			\
	$(srcdir)/cogl-boxed-value.c			\
	$(srcdir)/cogl-snippet-private.h		\
	$(srcdir)/cogl-snippet.c			\
	$(srcdir)/cogl-poll.c				\
	$(srcdir)/gl-prototypes/cogl-all-functions.h	\
	$(srcdir)/gl-prototypes/cogl-gles1-functions.h	\
	$(srcdir)/gl-prototypes/cogl-gles2-functions.h	\
	$(srcdir)/gl-prototypes/cogl-core-functions.h	\
	$(srcdir)/gl-prototypes/cogl-in-gles-core-functions.h	\
	$(srcdir)/gl-prototypes/cogl-in-gles1-core-functions.h	\
	$(srcdir)/gl-prototypes/cogl-in-gles2-core-functions.h	\
	$(srcdir)/gl-prototypes/cogl-fixed-functions.h	\
	$(srcdir)/gl-prototypes/cogl-glsl-functions.h	\
	$(NULL)

if USE_GLIB
cogl_public_h += \
	$(srcdir)/cogl-glib-source.h
cogl_sources_c += \
	$(srcdir)/cogl-glib-source.c
endif

if SUPPORT_XLIB
cogl_public_h += \
	$(srcdir)/cogl-clutter-xlib.h \
	$(srcdir)/cogl-xlib-renderer.h

cogl_experimental_h += \
       $(srcdir)/winsys/cogl-texture-pixmap-x11.h \
       $(srcdir)/cogl-xlib.h

cogl_sources_c += \
	$(srcdir)/cogl-x11-renderer-private.h \
	$(srcdir)/cogl-xlib-renderer-private.h \
	$(srcdir)/cogl-xlib-renderer.c \
	$(srcdir)/cogl-xlib.c \
	$(srcdir)/cogl-xlib-private.h \
	$(srcdir)/winsys/cogl-texture-pixmap-x11.c \
	$(srcdir)/winsys/cogl-texture-pixmap-x11-private.h
endif
if SUPPORT_GLX
cogl_sources_c += \
	$(srcdir)/cogl-glx-renderer-private.h \
	$(srcdir)/cogl-glx-display-private.h \
	$(srcdir)/winsys/cogl-winsys-glx-feature-functions.h \
	$(srcdir)/winsys/cogl-winsys-glx.c
endif
if SUPPORT_WGL
cogl_public_h += \
	$(srcdir)/cogl-win32-renderer.h

cogl_sources_c += \
	$(srcdir)/cogl-win32-renderer.c \
	$(srcdir)/winsys/cogl-winsys-wgl.c \
	$(srcdir)/winsys/cogl-winsys-wgl-feature-functions.h
endif
if SUPPORT_EGL_PLATFORM_WAYLAND
cogl_public_h += \
	$(srcdir)/cogl-wayland-renderer.h
cogl_sources_c += \
	$(srcdir)/winsys/cogl-winsys-egl-wayland.c \
	$(srcdir)/winsys/cogl-winsys-egl-wayland-private.h
endif
if SUPPORT_EGL_PLATFORM_KMS
cogl_public_h += \
	$(srcdir)/cogl-kms-renderer.h
cogl_sources_c += \
	$(srcdir)/winsys/cogl-winsys-egl-kms.c \
	$(srcdir)/winsys/cogl-winsys-egl-kms-private.h
endif
if SUPPORT_EGL_PLATFORM_XLIB
cogl_sources_c += \
	$(srcdir)/winsys/cogl-winsys-egl-x11.c \
	$(srcdir)/winsys/cogl-winsys-egl-x11-private.h
endif
if SUPPORT_EGL_PLATFORM_POWERVR_NULL
cogl_sources_c += \
	$(srcdir)/winsys/cogl-winsys-egl-null.c \
	$(srcdir)/winsys/cogl-winsys-egl-null-private.h
endif
if SUPPORT_EGL_PLATFORM_GDL
cogl_sources_c += \
	$(srcdir)/winsys/cogl-winsys-egl-gdl.c \
	$(srcdir)/winsys/cogl-winsys-egl-gdl-private.h
endif
if SUPPORT_EGL_PLATFORM_ANDROID
cogl_sources_c += \
	$(srcdir)/winsys/cogl-winsys-egl-android.c \
	$(srcdir)/winsys/cogl-winsys-egl-android-private.h
endif
if SUPPORT_EGL
cogl_sources_c += \
       $(srcdir)/winsys/cogl-winsys-egl.c \
       $(srcdir)/winsys/cogl-winsys-egl-feature-functions.h \
       $(srcdir)/winsys/cogl-winsys-egl-private.h
endif
if SUPPORT_SDL
cogl_sources_c += \
       $(srcdir)/winsys/cogl-winsys-sdl.c
endif

EXTRA_DIST += stb_image.c

# glib-mkenums rules
glib_enum_h = cogl-enum-types.h
glib_enum_c = cogl-enum-types.c
glib_enum_headers = $(cogl_public_h)
include $(top_srcdir)/build/autotools/Makefile.am.enums

lib_LTLIBRARIES += libcogl.la

libcogl_la_LIBADD = -lm $(COGL_DEP_LIBS) $(COGL_EXTRA_LDFLAGS)
if SUPPORT_GLX
libcogl_la_LIBADD += -ldl
endif
# XXX: The aim is to eventually get rid of all private API exports
# for cogl-pango.
libcogl_la_LDFLAGS = \
	-no-undefined \
	-version-info @COGL_LT_CURRENT@:@COGL_LT_REVISION@:@COGL_LT_AGE@ \
	-export-dynamic \
	-export-symbols-regex "^(cogl|_cogl_debug_flags|_cogl_atlas_new|_cogl_atlas_add_reorganize_callback|_cogl_atlas_reserve_space|_cogl_callback|_cogl_util_get_eye_planes_for_screen_poly|_cogl_atlas_texture_remove_reorganize_callback|_cogl_atlas_texture_add_reorganize_callback|_cogl_texture_foreach_sub_texture_in_region|_cogl_atlas_texture_new_with_size|_cogl_profile_trace_message|_cogl_context_get_default).*"

libcogl_la_SOURCES = $(cogl_sources_c)
nodist_libcogl_la_SOURCES = $(BUILT_SOURCES)

# Cogl installed headers
cogl_headers = \
	$(cogl_public_h) \
	$(srcdir)/cogl-deprecated.h \
	$(srcdir)/cogl-pango.h \
	$(NULL)

coglincludedir = $(includedir)/cogl/cogl
coglinclude_HEADERS = $(cogl_headers) $(cogl_experimental_h)
nodist_coglinclude_HEADERS = cogl-defines.h cogl-enum-types.h

dist-hook: ../build/win32/vs9/cogl.vcproj ../build/win32/vs10/cogl.vcxproj ../build/win32/vs10/cogl.vcxproj.filters ../build/win32/gen-enums.bat

# I know those filters below don't look nice, but this is to ensure the right files are in the Project files only *once*
../build/win32/vs9/cogl.vcproj: $(top_srcdir)/build/win32/vs9/cogl.vcprojin
	for F in `echo $(cogl_sources_c) $(BUILT_SOURCES) | tr '/' '\\'`; do \
		case $$F in \
		*-egl.c|*-glx.c|*-xlib*.c|*-x11.c|*.wgl.c|*-gl.c|*-gles.c|*-kms.c|*-sdl.c|*.h) ;; \
		*.c)	echo '   <File RelativePath="..\..\..\cogl\'$$F'" />' \
			;; \
		esac; \
	done >cogl.sourcefiles
	$(CPP) -P - <$(top_srcdir)/build/win32/vs9/cogl.vcprojin >$@
	rm cogl.sourcefiles

../build/win32/vs10/cogl.vcxproj: $(top_srcdir)/build/win32/vs10/cogl.vcxprojin
	for F in `echo $(cogl_sources_c) $(BUILT_SOURCES) | tr '/' '\\'`; do \
		case $$F in \
		*-egl.c|*-glx.c|*-xlib*.c|*-x11.c|*.wgl.c|*-gl.c|*-gles.c|*-kms.c|*-sdl.c|*.h) ;; \
		*.c)	echo '    <ClCompile Include="..\..\..\cogl\'$$F'" />' \
			;; \
		esac; \
	done >cogl.vs10.sourcefiles
	$(CPP) -P - <$(top_srcdir)/build/win32/vs10/cogl.vcxprojin >$@
	rm cogl.vs10.sourcefiles

../build/win32/vs10/cogl.vcxproj.filters: $(top_srcdir)/build/win32/vs10/cogl.vcxproj.filtersin
	for F in `echo $(cogl_sources_c) $(BUILT_SOURCES) | tr '/' '\\'`; do \
		case $$F in \
		*-egl.c|*-glx.c|*-xlib*.c|*-x11.c|*.wgl.c|*-gl.c|*-gles.c|*-kms.c|*-sdl.c|*.h) ;; \
		*.c)	echo '    <ClCompile Include="..\..\..\cogl\'$$F'"><Filter>Sources</Filter></ClCompile>' \
			;; \
		esac; \
	done >cogl.vs10.sourcefiles.filters
	$(CPP) -P - <$(top_srcdir)/build/win32/vs10/cogl.vcxproj.filtersin >$@
	rm cogl.vs10.sourcefiles.filters

cogl.vsenums_h:
	echo 'perl %1\bin\glib-mkenums --template ../../cogl/cogl-enum-types.h.in ' >vsenums_h.temp1
	for F in `echo $(cogl_public_h) $(srcdir)/cogl-win32-renderer.h`; do \
		case $$F in \
		*xlib*.h|*wayland*.h) ;; \
		*.h) echo '../../cogl'$$F' '	\
			;;	\
		esac;	\
	done >>vsenums_h.temp1
	cat vsenums_h.temp1 | sed 's_/cogl./_/cogl/_' >vsenums_h.temp
	cat vsenums_h.temp | tr -d '\n' >>$@
	echo '> ..\..\cogl\cogl-enum-types.h' >>$@
	rm vsenums_h.temp1
	rm vsenums_h.temp

cogl.vsenums_c:
	echo 'perl %1\bin\glib-mkenums --template ../../cogl/cogl-enum-types.c.in ' >vsenums_c.temp1
	for F in `echo $(cogl_public_h) $(srcdir)/cogl-win32-renderer.h`; do \
		case $$F in \
		*xlib*.h|*wayland*.h) ;; \
		*.h) echo '../../cogl'$$F' '	\
			;;	\
		esac;	\
	done >>vsenums_c.temp1
	cat vsenums_c.temp1 | sed 's_/cogl./_/cogl/_' >vsenums_c.temp
	cat vsenums_c.temp | tr -d '\n' >>$@
	echo '> ..\..\cogl\cogl-enum-types.c' >>$@
	rm vsenums_c.temp1
	rm vsenums_c.temp

../build/win32/gen-enums.bat: cogl.vsenums_h cogl.vsenums_c
	echo 'if exist ..\..\cogl\cogl-enum-types.h goto DONE_COGL_ENUMS_H' >$@
	cat cogl.vsenums_h >>$@
	echo ':DONE_COGL_ENUMS_H' >>$@
	echo 'if exist ..\..\cogl\cogl-enum-types.c goto DONE_COGL_ENUMS_C' >>$@
	cat cogl.vsenums_c >>$@
	echo ':DONE_COGL_ENUMS_C' >>$@
	rm cogl.vsenums_h
	rm cogl.vsenums_c

EXTRA_DIST += \
	cogl-defines.h.win32	\
	cogl-defines.h.win32_SDL	\
	cogl.symbols	\
	cogl.rc.in	\
	cogl.rc

DISTCLEANFILES += ../build/win32/vs9/cogl.vcproj ../build/win32/vs10/cogl.vcxproj ../build/win32/vs10/cogl.vcxproj.filters ../build/win32/gen-enums.bat cogl.rc

-include $(INTROSPECTION_MAKEFILE)

INTROSPECTION_GIRS =

if HAVE_INTROSPECTION
Cogl-1.0.gir: libcogl.la Makefile

Cogl_1_0_gir_NAMESPACE = Cogl
Cogl_1_0_gir_VERSION = 1.0
Cogl_1_0_gir_LIBS = libcogl.la
Cogl_1_0_gir_FILES = $(cogl_public_h) cogl-enum-types.h
Cogl_1_0_gir_CFLAGS = $(INCLUDES) $(AM_CPPFLAGS) $(COGL_DEP_CFLAGS)
Cogl_1_0_gir_INCLUDES = GL-1.0 GObject-2.0
Cogl_1_0_gir_EXPORT_PACKAGES = cogl-1.0
Cogl_1_0_gir_SCANNERFLAGS = --warn-all --c-include='cogl/cogl.h'

INTROSPECTION_GIRS += Cogl-1.0.gir

girdir = $(datadir)/gir-1.0
gir_DATA = $(INTROSPECTION_GIRS)

typelibdir = $(libdir)/girepository-1.0
typelib_DATA = $(INTROSPECTION_GIRS:.gir=.typelib)

CLEANFILES += $(gir_DATA) $(typelib_DATA)
endif
