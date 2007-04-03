# $Id$

bin_PROGRAMS = burst

burst_SOURCES = \
	burst.c \
	error.c \
	fb.c \
	glob.c \
	grid.c \
	gridrotate.c \
	Hm.c \
	HmGetc.c \
	HmGlob.c \
	idents.c \
	paint.c \
	plot.c \
	prnt.c \
	Sc.c \
	trie.c \
	ui.c

burst_CFLAGS = ${TERMLIB_CPPFLAGS} ${TCL_CPPFLAGS}
burst_LDADD = \
	${RT} ${RT_LIBS} \
	${FB} ${FB_LIBS} \
	${BN} ${BN_LIBS} \
	${BU} ${BU_LIBS} \
	${PKG} ${PKG_LIBS} \
	${TERMIO} ${TERMIO_LIBS} \
	${TERMLIB}

noinst_HEADERS = \
	ascii.h \
	burst.h \
	extern.h \
	Hm.h \
	Mm.h \
	Sc.h \
	trie.h \
	vecmath.h

EXTRA_DIST = burst.format

test:	burst $(top_builddir)/db/ktank.g
	./burst -b < $(top_builddir)/db/ktank.in
	@if test $? -ne 0
	then echo "Test failed." ; exit 1
	fi
	diff $(top_builddir)/db/ktank.burst $(top_builddir)/db/ktank.burst.bak4.0
	@if test $? -ne 0
	then echo "Test failed." ; exit 1
	fi
	diff $(top_builddir)/db/ktank.shotlines $(top_builddir)/db/ktank.shotlines.bak4.0
	@if test $? -ne 0
	then echo "Test failed." ; exit 1
	fi
	@echo "Tested OK."

untest:
	rm -f \
	$(top_builddir)/db/ktank.burst \
	$(top_builddir)/db.ktank.shotlines \
	$(top_builddir)/db/ktank.plot \
	$(top_builddir)/db/ktank.pix \
	$(top_builddir)/db/ktank.g \
	$(top_builddir)/db/ktank.log

$(top_builddir)/db/ktank.g:
	asc2g < $(top_builddir)/db/ktank.asc > $(top_builddir)/db/ktank.g

include $(top_srcdir)/misc/Makefile.defs

FAST_OBJECTS = \
	$(burst_OBJECTS) \
	$(bin_PROGRAMS)
