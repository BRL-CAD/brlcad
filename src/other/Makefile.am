
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
if BUILD_TKHTML3
if WITH_TK
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

all-local: tkhtml3-all
install-exec-local: tkhtml3-install
clean-local: tkhtml3-clean
distclean-local: tkhtml3-distclean
uninstall-local: tkhtml3-uninstall

.PHONY : tkhtml3-all tkhtml3-install tkhtml3-clean tkhtml3-distclean tkhtml3-uninstall

if BUILD_TKPNG
if WITH_TK
TKPNG_MAKE=$(MAKE)
else
TKPNG_MAKE=:
endif
else
TKPNG_MAKE=:
endif

tkpng-all:
	cd tkpng && ${TKPNG_MAKE} $(AM_MAKEFLAGS) LIBTOOL="../../../libtool --mode=link" LIBTOOLDIR=".libs/" LIBS="${TKPNGZLIB}" all

tkpng-install:
	cd tkpng && ${TKPNG_MAKE} $(AM_MAKEFLAGS) LIBTOOL="../../../libtool --mode=link" LIBTOOLDIR=".libs/" LIBS="${TKPNGZLIB}" install

tkpng-clean:
	cd tkpng && ${TKPNG_MAKE} $(AM_MAKEFLAGS) clean

tkpng-distclean:
	cd tkpng && ${TKPNG_MAKE} $(AM_MAKEFLAGS) distclean

tkpng-uninstall:
	cd tkpng && ${TKPNG_MAKE} $(AM_MAKEFLAGS) uninstall

all-local: tkpng-all
install-exec-local: tkpng-install
clean-local: tkpng-clean
distclean-local: tkpng-distclean
uninstall-local: tkpng-uninstall

.PHONY : tkpng-all tkpng-install tkpng-clean tkpng-distclean tkpng-uninstall

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
	tkpng \
	tnt

include $(top_srcdir)/misc/Makefile.defs
