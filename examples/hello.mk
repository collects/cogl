#
#  This file was added by Duzy Chan <code@duzy.info> on 2011-09-30
#
$(call sm-new-module, hello, exe, gcc)

$(call sm-use, cogl)
#$(call sm-use, cogl-pango)

sm.this.verbose := true
sm.this.sources := hello.c

$(sm-build-this)
