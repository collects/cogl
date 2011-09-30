#
#  This file was added by Duzy Chan <code@duzy.info> on 2011-09-30
#
$(call sm-new-module, crate, exe, gcc)

$(call sm-use, cogl)
$(call sm-use, cogl-pango)

sm.this.verbose := true
sm.this.sources := crate.c
sm.this.defines := \
  -DCOGL_EXAMPLES_DATA=\"$(COGL_EXAMPLES_DATA)\"

$(sm-build-this)
