# $Id$

bin_PROGRAMS = jove findcom

bin_SCRIPTS = teach-jove

SRCS = \
	jove_buf.c \
	jove_c.c \
	jove_delete.c \
	jove_disp.c \
	jove_draw.c \
	jove_extend.c \
	jove_funcs.c \
	jove_insert.c \
	jove_io.c \
	jove_main.c \
	jove_marks.c \
	jove_misc.c \
	jove_proc.c \
	jove_re.c \
	jove_screen.c \
	jove_term.c \
	jove_wind.c \
	tune.c

jove_SOURCES = \
	$(SRCS) \
	version.c

jove_LDADD = ${TERMLIB}

findcom_SOURCES = \
	findcom.c \
	tune.c

noinst_HEADERS = \
	jove.h \
	jove_temp.h \
	termcap.h

dist_man_MANS = \
	jove.1 \
	describe.com.1 \
	teach-jove.1

tune.c:	$(top_srcdir)/src/other/jove/jove_tune.c
	rm -f tune.c
	sed -e 's;BinDir;BINDIR;' -e 's;LibDir;LIBDIR;' < $(top_srcdir)/src/other/jove/jove_tune.c > tune.c

version.c: $(SRCS) Makefile
	sh $(top_srcdir)/src/other/jove/mkversion.sh

version.o: version.c
	$(COMPILE) -c version.c

teach-jove:	$(top_srcdir)/src/other/jove/teach-jove.sh
	sed -e 's;BinDir;BINDIR;' -e 's;LibDir;LIBDIR;' < $(top_srcdir)/src/other/jove/teach-jove.sh > teach-jove

install-data-local:
	$(INSTALL_DATA) $(srcdir)/describe.com.1 $(DESTDIR)$(libdir)/describe.com
	$(INSTALL_DATA) $(srcdir)/teach-jove.1 $(DESTDIR)$(libdir)/teach-jove

EXTRA_DIST = \
	$(BUILT_SOURCES) \
	describe.com.1 \
	jove_tune.c \
	mkversion.sh \
	teach-jove.1 \
	teach-jove.sh

BUILT_SOURCES = \
	tune.c \
	version.c

DISTCLEANFILES = \
	$(BUILT_SOURCES) \
	version.number

MAINTAINERCLEANFILES = Makefile.in

include $(top_srcdir)/misc/Makefile.defs

FAST_OBJECTS = \
	$(jove_OBJECTS) \
	$(findcom_OBJECTS) \
	$(bin_PROGRAMS)