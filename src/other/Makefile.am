# $Id$

LIBZ=
if NEED_ZLIB
	LIBZ=libz
endif

PNG=
if NEED_PNG
	PNG=libpng
endif

if ONLY_BENCHMARK
SUBDIRS = libtcl

else !ONLY_BENCHMARK

if ONLY_RTS
SUBDIRS = libtcl

else !ONLY_RTS
SUBDIRS = \
	awf \
	iwidgets \
	jove \
	libitcl \
	liborle \
	${PNG} \
	libtcl \
	libtk \
	libtkGLX \
	libutahrle \
	${LIBZ}

endif
# end ONLY_RTS

endif
# end ONLY_BENCHMARK

MAINTAINERCLEANFILES = \
	Makefile.in
