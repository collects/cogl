#
#  This file was added by Duzy Chan <code@duzy.info> on 2011-09-30
#
$(call sm-new-module, x11-foreign, exe, gcc)

$(call sm-use, cogl)

sm.this.verbose := true
sm.this.sources := x11-foreign.c

$(sm-build-this)
