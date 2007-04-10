# $Id$

bin_PROGRAMS = jove findcom
bin_SCRIPTS = teach-jove

AM_CPPFLAGS = ${TERMLIB_CPPFLAGS}

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
	jove_tune.c \
	jove_wind.c

jove_LDADD = ${TERMLIB}
jove_SOURCES = \
	$(SRCS) \
	version.c

findcom_SOURCES = \
	findcom.c \
	jove_tune.c

noinst_HEADERS = \
	jove.h \
	jove_temp.h \
	termcap.h

dist_man_MANS = \
	jove.1

descdir = $(BRLCAD_DATA)/jove
desc_DATA = \
	describe.com \
	jove-tutorial

version.c: $(SRCS) Makefile
	sh $(top_srcdir)/src/other/jove/mkversion.sh

version.o: version.c
	$(COMPILE) -c version.c

EXTRA_DIST = \
	$(BUILT_SOURCES) \
	$(desc_DATA) \
	mkversion.sh \
	teach-jove \
	teach-jove.in

BUILT_SOURCES = \
	teach-jove \
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