
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

if BUILD_URT
URTDIR=URToolkit
UTAHRLEDIR=libutahrle
else !BUILD_URT
URTDIR=
UTAHRLEDIR=
endif

if BUILD_TCL
TCLDIR=tcl
else !BUILD_TCL
TCLDIR=
endif

if BUILD_TK
TKDIR=tk
else !BUILD_TK
TKDIR=
endif

if BUILD_ITCL
ITCLDIR=incrTcl
else !BUILD_ITCL
ITCLDIR=
endif

if BUILD_IWIDGETS
IWIDGETSDIR=iwidgets
else !BUILD_IWIDGETS
IWIDGETSDIR=
endif

if BUILD_TERMLIB
TERMLIBDIR=libtermlib
else !BUILD_TERMLIB
TERMLIBDIR=
endif

if WITH_OPENGL
TOGL_MAKE=$(MAKE)
else
TOGL_MAKE=:
endif

togl-all:
	cd togl && ${TOGL_MAKE} $(AM_MAKEFLAGS) LIBS="${X_LIBS} ${LIBGL} -lXmu" all

togl-install:
	cd togl && ${TOGL_MAKE} $(AM_MAKEFLAGS) LIBS="${X_LIBS} ${LIBGL} -lXmu" install

togl-clean:
	cd togl && ${TOGL_MAKE} $(AM_MAKEFLAGS) LIBS="${X_LIBS} ${LIBGL} -lXmu" clean

togl-distclean:
	cd togl && ${TOGL_MAKE} $(AM_MAKEFLAGS) LIBS="${X_LIBS} ${LIBGL} -lXmu" distclean

togl-uninstall:
	cd togl && ${TOGL_MAKE} $(AM_MAKEFLAGS) LIBS="${X_LIBS} ${LIBGL} -lXmu" uninstall

if BUILD_DOCS
if BUILD_TKHTML3
if WITH_TK
TKHTMLDIR=tkhtml3
else
TKHTMLDIR=
endif
else
TKHTMLDIR=
endif
else
TKHTMLDIR=
endif

if BUILD_TKIMG
if WITH_TK
TKIMGDIR=tkimg
else !WITH_TK
TKIMGDIR=
endif
else !BUILD_TKIMG
TKIMGDIR=
endif

if BUILD_GOBLIN
GOBLINDIR=goblin
else !BUILD_GOBLIN
GOBLINDIR=
endif

if BUILD_OPENNURBS
OPENNURBSDIR=openNURBS
else !BUILD_OPENNURBS
OPENNURBSDIR=
endif

if BUILD_STEP
STEPDIR=step
else !BUILD_STEP
STEPDIR=
endif

if BUILD_TNT
TNTDIR=tnt
else !BUILD_TNT
TNTDIR=
endif


if ONLY_BENCHMARK
SUBDIRS = \
	${ZDIR} \
	${PNGDIR} \
	${REGEXDIR} \
	${TCLDIR} \
	${OPENNURBSDIR} \
	${TNTDIR}

else !ONLY_BENCHMARK

if ONLY_RTS
SUBDIRS = \
	${ZDIR} \
	${PNGDIR} \
	${REGEXDIR} \
	${TCLDIR} \
	${OPENNURBSDIR} \
	${TNTDIR}

else !ONLY_RTS
# do not sort SUBDIRS
SUBDIRS = \
	${ZDIR} \
	${PNGDIR} \
	${REGEXDIR} \
	$(UTAHRLEDIR) \
	$(URTDIR) \
	${TCLDIR} \
	${TKDIR} \
	${ITCLDIR} \
	${IWIDGETSDIR} \
	${TERMLIBDIR} \
	${JOVEDIR} \
	${GOBLINDIR} \
	${OPENNURBSDIR} \
	${STEPDIR} \
	${TKIMGDIR} \
	${TKHTMLDIR} \
	${TNTDIR} \
	awf

endif
# end ONLY_RTS

endif
# end ONLY_BENCHMARK

EXTRA_DIST = \
	boost 

# FIXME: need to only traverse tcl/tk if configured
DIST_SUBDIRS = \
	URToolkit \
	awf \
	incrTcl \
	iwidgets \
	jove \
	libpng \
	libregex \
	libtermlib \
	libutahrle \
	libz \
	openNURBS \
	step \
	tcl \
	tk \
	tkimg \
	tkhtml3 \
	tnt

include $(top_srcdir)/misc/Makefile.defs
