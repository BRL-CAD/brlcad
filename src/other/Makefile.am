
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
if WITH_X11
TKDIR=tk
else !WITH_X11
TKDIR=
endif
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
if WITH_X11
TKHTML3_MAKE=$(MAKE)
else
TKHTML3_MAKE=:
endif
else
TKHTML3_MAKE=:
endif
else
TKHTML3_MAKE=:
endif

tkhtml3-all:
	cd tkhtml3 && ${TKHTML3_MAKE} $(AM_MAKEFLAGS) LIBS="${X_LIBS} ${LIBS}" all

tkhtml3-install:
	cd tkhtml3 && ${TKHTML3_MAKE} $(AM_MAKEFLAGS) LIBS="${X_LIBS} ${LIBS}" install

tkhtml3-clean:
	cd tkhtml3 && ${TKHTML3_MAKE} $(AM_MAKEFLAGS) LIBS="${X_LIBS} ${LIBS}" clean

tkhtml3-distclean:
	cd tkhtml3 && ${TKHTML3_MAKE} $(AM_MAKEFLAGS) LIBS="${X_LIBS} ${LIBS}" distclean

tkhtml3-uninstall:
	cd tkhtml3 && ${TKHTML3_MAKE} $(AM_MAKEFLAGS) LIBS="${X_LIBS} ${LIBS}" uninstall

all-local: tkhtml3-all togl-all
install-exec-local: tkhtml3-install togl-install
clean-local: tkhtml3-clean togl-clean
distclean-local: tkhtml3-distclean togl-distclean
uninstall-local: tkhtml3-uninstall togl-uninstall

.PHONY : tkhtml3-all tkhtml3-install tkhtml3-clean tkhtml3-distclean tkhtml3-uninstall togl-all togl-install togl-clean togl-distclean togl-uninstall



if BUILD_TKIMG
if WITH_X11
TKIMGDIR=tkimg
else !WITH_X11
TKIMGDIR=
endif
else !BUILD_TKIMG
TKIMGDIR=
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
	${TKIMGDIR} \
	${OPENNURBSDIR} \
	${TNTDIR} \
	awf

endif
# end ONLY_RTS

endif
# end ONLY_BENCHMARK

EXTRA_DIST = \
	boost \
	tkhtml3/AUTHORS \
	tkhtml3/COPYING \
	tkhtml3/ChangeLog \
	tkhtml3/INSTALL \
	tkhtml3/Makefile \
	tkhtml3/Makefile.in \
	tkhtml3/NEWS \
	tkhtml3/README \
	tkhtml3/aclocal.m4 \
	tkhtml3/configure \
	tkhtml3/configure.in \
	tkhtml3/doc \
	tkhtml3/src \
	tkhtml3/tclconfig

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
	tnt

include $(top_srcdir)/misc/Makefile.defs
