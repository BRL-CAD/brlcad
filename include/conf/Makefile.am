
COUNT: DATE HOST PATH USER
	test -f $@ && expr `cat COUNT` + 1 > $@ || $(ECHO) "1" > $@

DATE: HOST PATH USER $(top_builddir)/include/brlcad_config.h
	$(ECHO) "\"`LANG=C date -R 2>/dev/null || date +'%a, %d %b %Y %H:%M:%S %z' 2>/dev/null || date`\"" > $@

HOST: $(top_builddir)/include/brlcad_config.h
	$(ECHO) "\"`hostname`\"" > $@

PATH: $(top_builddir)/include/brlcad_config.h
	$(ECHO) "\"$(DESTDIR)$(prefix)\"" > $@

USER: $(top_builddir)/include/brlcad_config.h
	$(ECHO) "\"`whoami`\"" > $@

# updated every time make is invoked for timing the compile
TS:
	@$(ECHO) "\"`LANG=C date -R 2>/dev/null || date +'%a, %d %b %Y %H:%M:%S %z' 2>/dev/null || date`\"" > $@
	@cat TS

.PHONY: TS

# this causes them to be dependencies on the 'all' target
BUILT_SOURCES = \
	COUNT \
	DATE \
	HOST \
	PATH \
	TS \
	USER

EXTRA_DIST = \
	$(BUILT_SOURCES) \
	BrlcadComfig.tmpl \
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
	TS \
	USER

DISTCLEANFILES = \
	$(CLEANFILES) \
	COUNT \
	TS

include $(top_srcdir)/misc/Makefile.defs
