# $Id$

COUNT: DATE HOST PATH USER
	if test -f $(top_srcdir)/include/conf/COUNT ; then \
		cp $(top_srcdir)/include/conf/COUNT $@ ;\
		chmod ug+w $@ ;\
	else \
		test -f $@ || echo "1" > $@ ;\
	fi
	expr `cat COUNT` + 1 > $@

DATE: HOST PATH USER
	if test -f $(top_srcdir)/include/conf/DATE ; then \
		cp $(top_srcdir)/include/conf/DATE $@ ;\
		chmod ug+w $@ ;\
	else \
		test -f $@ || echo "\"`date`\"" > $@ ;\
	fi

HOST:
	if test -f $(top_srcdir)/include/conf/HOST ; then \
		cp $(top_srcdir)/include/conf/HOST $@ ;\
		chmod ug+w $@ ;\
	else \
		test -f $@ || echo "\"`hostname`\"" > $@ ;\
	fi

PATH:
	if test -f $(top_srcdir)/include/conf/PATH ; then \
		cp $(top_srcdir)/include/conf/PATH $@ ;\
		chmod ug+w $@ ;\
	else \
		test -f $@ || echo "\"$(DESTDIR)$(prefix)\"" > $@ ;\
	fi

USER:
	if test -f $(top_srcdir)/include/conf/USER ; then \
		cp $(top_srcdir)/include/conf/USER $@ ;\
		chmod ug+w $@ ;\
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
