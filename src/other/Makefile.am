# $Id$

if BUILD_REGEX
REGEX=libregex
else !BUILD_REGEX
REGEX=
endif

if BUILD_ZLIB
Z=libz
else !BUILD_ZLIB
Z=
endif

if BUILD_PNG
PNG=libpng
else !BUILD_PNG
PNG=
endif

# this seems to have serious breakage for linux and fbsd
if IRIX
TKGLX=libtkGLX
else !IRIX
TKGLX=
endif

if BUILD_JOVE
JOVEDIR=jove
else !BUILD_JOVE
JOVEDIR=
endif

if ONLY_BENCHMARK
SUBDIRS = libtcl

else !ONLY_BENCHMARK

if ONLY_RTS
SUBDIRS = libtcl

else !ONLY_RTS
SUBDIRS = \
	${Z} \
	${PNG} \
	${REGEX} \
	${TKGLX} \
	awf \
	${JOVEDIR} \
	libutahrle \
	libtcl \
	libtk \
	incrTcl \
	iwidgets

endif
# end ONLY_RTS

endif
# end ONLY_BENCHMARK

MAINTAINERCLEANFILES = Makefile.in
