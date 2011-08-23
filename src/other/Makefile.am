
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

if BUILD_DOCS
if BUILD_TKHTML
if WITH_TK
TKHTMLDIR=tkhtml
else
TKHTMLDIR=
endif
else
TKHTMLDIR=
endif
else
TKHTMLDIR=
endif

#if BUILD_TOGL
#if WITH_TK
#TOGLDIR=togl
#else
#TOGLDIR=
#endif
#else
#TOGLDIR=
#endif

if BUILD_TKPNG
if WITH_TK
TKPNGDIR=tkpng
else !WITH_TK
TKPNGDIR=
endif
else !BUILD_TKPNG
TKPNGDIR=
endif

if BUILD_TKTABLE
if WITH_TK
TKTABLEDIR=tktable
else
TKTABLEDIR=
endif
else
TKTABLEDIR=
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
	${STEPDIR} \
	${TKPNGDIR} \
	${TKHTMLDIR} \
	${TKTABLEDIR} \
	${OPENNURBSDIR} \
	${TNTDIR} 

endif
# end ONLY_RTS

endif
# end ONLY_BENCHMARK

EXTRA_DIST = \
	boost \
	togl \
	CMakeLists.txt \
	itcl.dist \
	itk.dist \
	iwidgets.dist \
	jove.dist \
	lemon \
	libpng.dist \
	libregex.dist \
	libtermlib.dist \
	libutahrle.dist \
	libz.dist \
	openNURBS.dist \
	re2c \
	step.dist \
	tcl.dist \
	tk.dist \
	tkhtml.dist \
	tkpng.dist \
	tktable.dist \
	togl.dist \
	URToolkit.dist


# FIXME: need to only traverse tcl/tk if configured
DIST_SUBDIRS = \
	URToolkit \
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
	tkpng \
	tkhtml \
	tktable \
	tnt

include $(top_srcdir)/misc/Makefile.defs
