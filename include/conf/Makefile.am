# $Id$

COUNT: DATE HOST PATH USER
	@test -f $(top_srcdir)/include/conf/COUNT || echo "1" > $@
	@test -f $@ || cp $(top_srcdir)/include/conf/COUNT .
	@expr `cat COUNT` + 1 > $@

DATE: HOST PATH USER
	@test -f $(top_srcdir)/include/conf/DATE || echo "\"`date`\"" > $@
	@test -f $@ || cp $(top_srcdir)/include/conf/DATE .
	@echo "\"`date`\"" > $@

HOST:
	@test -f $(top_srcdir)/include/conf/HOST || echo "\"`hostname`\"" > $@
	@test -f $@ || cp $(top_srcdir)/include/conf/HOST .
	@echo "\"`hostname`\"" > $@

PATH:
	@test -f $(top_srcdir)/include/conf/PATH || echo "\"$(DESTDIR)$(prefix)\"" > $@
	@test -f $@ || cp $(top_srcdir)/include/conf/PATH .
	@echo "\"$(DESTDIR)$(prefix)\"" > $@

USER:
	@test -f $(top_srcdir)/include/conf/USER || echo "\"`whoami`\"" > $@
	@test -f $@ || cp $(top_srcdir)/include/conf/USER .
	@echo "\"`whoami`\"" > $@


all-local: COUNT DATE HOST PATH USER


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
	PATCH

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
