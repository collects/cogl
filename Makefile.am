SUBDIRS = cogl

if BUILD_COGL_PANGO
SUBDIRS += cogl-pango
endif

SUBDIRS += examples doc po

ACLOCAL_AMFLAGS = -I build/autotools ${ACLOCAL_FLAGS}

# .changelog expects these to be initializes
CLEANFILES=
DISTCLEANFILES=

# .release requires .changelog rules
include $(top_srcdir)/build/autotools/Makefile.am.changelog
include $(top_srcdir)/build/autotools/Makefile.am.release

EXTRA_DIST = README.in
