#
#  This file was added by Duzy Chan <code@duzy.info> on 2011-09-30
#
this_dir := $(sm-this-dir)

$(call sm-load-module, $(this_dir)/hello.mk)
$(call sm-load-module, $(this_dir)/crate.mk)
$(call sm-load-module, $(this_dir)/x11-foreign.mk)
$(call sm-load-module, $(this_dir)/x11-tfp.mk)
