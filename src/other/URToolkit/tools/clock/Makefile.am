
bin_PROGRAMS = rleClock

rleClock_SOURCES = \
	font.c \
	rleClock.c

noinst_HEADERS = font.h

font.c: ${srcdir}/makeFont ${srcdir}/font.src
	@SH@ ${srcdir}/makeFont ${srcdir}/font.src > font.c

rleClock_LDADD = ${RLE}
rleClock_CFLAGS = ${RLE_CPPFLAGS} ${NOWARN}

EXTRA_DIST = \
	README \
	font.src \
	makeFont \
	CMakeLists.txt

include $(top_srcdir)/misc/Makefile.defs

