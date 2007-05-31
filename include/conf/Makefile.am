# $Id$

COUNT: DATE HOST PATH USER
	if test -f $(top_srcdir)/include/conf/COUNT ; then \
		touch $@ ; chmod ug+w $@ ;\
		cat $(top_srcdir)/include/conf/COUNT > $@ ;\
	else \
		test -f $@ || echo "1" > $@ ;\
	fi
	expr `cat COUNT` + 1 > $@

DATE: HOST PATH USER
	if test -f $(top_srcdir)/include/conf/DATE ; then \
		touch $@ ; chmod ug+w $@ ;\
		cat $(top_srcdir)/include/conf/DATE > $@ ;\
	else \
		test -f $@ || echo "\"`date`\"" > $@ ;\
	fi

HOST:
	if test -f $(top_srcdir)/include/conf/HOST ; then \
		touch $@ ; chmod ug+w $@ ;\
		cat $(top_srcdir)/include/conf/HOST > $@ ;\
	else \
		test -f $@ || echo "\"`hostname`\"" > $@ ;\
	fi

PATH:
	if test -f $(top_srcdir)/include/conf/PATH ; then \
		touch $@ ; chmod ug+w $@ ;\
		cat $(top_srcdir)/include/conf/PATH > $@ ;\
	else \
		test -f $@ || echo "\"$(DESTDIR)$(prefix)\"" > $@ ;\
	fi

USER:
	if test -f $(top_srcdir)/include/conf/USER ; then \
		touch $@ ; chmod ug+w $@ ;\
		cat $(top_srcdir)/include/conf/USER > $@ ;\
	else \
		test -f $@ || echo "\"`whoami`\"" > $@ ;\
	fi

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
