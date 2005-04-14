# $Id$

if BUILD_REGEX
REGEXDIR=libregex
else !BUILD_REGEX
REGEXDIR=
endif

if BUILD_ZLIB
ZDIR=libz
else !BUILD_ZLIB
ZDIR=
endif

if BUILD_PNG
PNGDIR=libpng
else !BUILD_PNG
PNGDIR=
endif

if BUILD_JOVE
JOVEDIR=jove
else !BUILD_JOVE
JOVEDIR=
endif

if BUILD_LIBTKGLX
TKGLXDIR=libtkGLX
else !BUILD_LIBTKGLX
TKGLXDIR=
endif

if BUILD_URT
URTDIR=URToolkit
UTAHRLEDIR=libutahrle
else !BUILD_URT
URTDIR=
UTAHRLEDIR=
endif

if ONLY_BENCHMARK
SUBDIRS = libtcl

else !ONLY_BENCHMARK

if ONLY_RTS
SUBDIRS = libtcl

else !ONLY_RTS
# do not sort SUBDIRS
SUBDIRS = \
	${ZDIR} \
	${PNGDIR} \
	${REGEXDIR} \
	${TKGLXDIR} \
	${JOVEDIR} \
	$(UTAHRLEDIR) \
	$(URTDIR) \
	awf \
	libtcl \
	libtk \
	incrTcl \
	iwidgets

endif
# end ONLY_RTS

endif
# end ONLY_BENCHMARK

MAINTAINERCLEANFILES = Makefile.in
