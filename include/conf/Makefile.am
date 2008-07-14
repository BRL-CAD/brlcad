# $Id$

COUNT: DATE HOST PATH USER
	test -f $@ && expr `cat COUNT` + 1 > $@ || echo "1" > $@

DATE: HOST PATH USER $(top_builddir)/include/brlcad_config.h
	echo "\"`LANG=C date -R 2>/dev/null || date +'%a, %d %b %Y %H:%M:%S %z' 2>/dev/null || date`\"" > $@

HOST: $(top_builddir)/include/brlcad_config.h
	echo "\"`hostname`\"" > $@

PATH: $(top_builddir)/include/brlcad_config.h
	echo "\"$(DESTDIR)$(prefix)\"" > $@

USER: $(top_builddir)/include/brlcad_config.h
	echo "\"`whoami`\"" > $@

# this causes them to be dependencies on the 'all' target
BUILT_SOURCES = \
	COUNT \
	DATE \
	HOST \
	PATH \
	USER

EXTRA_DIST = \
	$(BUILT_SOURCES) \
        CMakeLists.txt \
	MAJOR \
	MINOR \
	PATCH \
	make.vbs

# remove all except COUNT
CLEANFILES = \
	DATE \
	HOST \
	PATH \
	USER

DISTCLEANFILES = \
	$(CLEANFILES) \
	COUNT

all-am: $(BUILT_SOURCES)

include $(top_srcdir)/misc/Makefile.defs
