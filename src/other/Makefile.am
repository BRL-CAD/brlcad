# $Id$

LIBZ=
if NEED_ZLIB
	LIBZ=libz
endif

PNG=
if NEED_PNG
	PNG=libpng
endif

# this seems to have serious breakage for linux and fbsd
TKGLX=
if IRIX
	TKGLX=libtkGLX
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
	${TKGLX} \
	libutahrle \
	${LIBZ}

endif
# end ONLY_RTS

endif
# end ONLY_BENCHMARK

MAINTAINERCLEANFILES = \
	Makefile.in
