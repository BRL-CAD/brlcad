# $Id$

COUNT:
	@test -f $(top_srcdir)/include/conf/COUNT || echo "1" > $@
	@test -f $@ || cp $(top_srcdir)/include/conf/COUNT .
	@expr `cat COUNT` + 1 > $@

DATE:
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

# make sure that the build is updated every time make is run
all-local: COUNT DATE HOST PATH USER
	@test -f COUNT || cp $(top_srcdir)/include/conf/COUNT .
	@expr `cat COUNT` + 1 > COUNT
	@test -f DATE || cp $(top_srcdir)/include/conf/DATE .
	@echo "\"`date`\"" > DATE
	@test -f HOST || cp $(top_srcdir)/include/conf/HOST .
	@echo "\"`hostname`\"" > HOST
	@test -f PATH || cp $(top_srcdir)/include/conf/PATH .
	@echo "\"$(DESTDIR)$(prefix)\"" > PATH
	@test -f USER || cp $(top_srcdir)/include/conf/USER .
	@echo "\"`whoami`\"" > USER

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

DISTCLEANFILES = \
	$(BUILT_SOURCES)

MAINTAINERCLEANFILES = Makefile.in

include $(top_srcdir)/misc/Makefile.defs
