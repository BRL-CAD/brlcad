# $Id$

lib_LTLIBRARIES = libwdb.la

SRCS = \
	arbn.c \
	ars.c \
	bot.c \
	cline.c \
	dynamic_geometry.c \
	export.c \
	extr.c \
	id.c \
	mater.c \
	nmg.c \
	nurb.c \
	pipe.c \
	reg.c \
	skt.c \
	strsol.c \
	units.c \
	wdb.c

libwdb_la_LDFLAGS = -version-info 19:1
libwdb_la_LIBADD = ${BN} ${BU} ${LIBRT}
libwdb_la_SOURCES = \
	$(SRCS) \
	vers.c

man_MANS = libwdb.3

# the C files here are deprecated
EXTRA_DIST = \
	$(man_MANS) \
	libwdb.dsp \
	libwdb.dsw \
	libwdb.ncb \
	libwdb.opt \
	poly.c \
	reg4.c \
	spl.c

BUILT_SOURCES = vers.c
vers.c: $(SRCS)
	@SH@ $(top_srcdir)/sh/vers.sh > $@

DISTCLEANFILES = version

MAINTAINERCLEANFILES = Makefile.in
