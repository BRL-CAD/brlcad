# $Id$

COUNT: DATE HOST PATH USER
	expr "`cat COUNT`" + 1 > $@

DATE: HOST PATH USER
	echo "\"`date`\"" > $@

HOST:
	echo "\"`hostname`\"" > $@

PATH:
	echo "\"$(DESTDIR)$(prefix)\"" > $@

USER:
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
DISTCLEANFILES = $(CLEANFILES)
MAINTAINERCLEANFILES = \
	COUNT \
	Makefile.in

include $(top_srcdir)/misc/Makefile.defs
