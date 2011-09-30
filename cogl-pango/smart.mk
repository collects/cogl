#
#  This file was added by Duzy Chan <code@duzy.info> on 2011-09-30
#
$(call sm-new-module, cogl-pango, shared, gcc)

$(call sm-use, cogl)

sm.this.verbose := true

sm.this.sources := \
  cogl-pango-display-list.c   \
  cogl-pango-fontmap.c        \
  cogl-pango-render.c         \
  cogl-pango-glyph-cache.c    \
  cogl-pango-pipeline-cache.c \

sm.this.defines := \
  -DCLUTTER_COMPILATION \
  -DG_LOG_DOMAIN=\"CoglPango\" \

sm.this.includes := \
  $(sm.this.dir)/../cogl \
  $(sm.this.dir)/../cogl/winsys \

sm.this.compile.flags := -fPIC \
  $(shell pkg-config --cflags pango) \

sm.this.link.flags := \
  -Wl,-no-undefined \
  -Wl,-export-dynamic \

sm.this.libs := \
  $(shell pkg-config --libs pango) \
  $(shell pkg-config --libs pangocairo) \

sm.this.export.compile.flags := \
  $(shell pkg-config --cflags pango) \

sm.this.export.libdirs := $(sm.out.lib)
sm.this.export.libs := cogl-pango \
  $(sm.this.libs)

$(sm-generate-implib)
$(sm-build-this)
