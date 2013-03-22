
lib_LTLIBRARIES = libmultispectral.la

AM_CFLAGS = \
	-DRT_MULTISPECTRAL \
	${STD_C99_FLAGS} \
	${STRICT_FLAGS} \
	${TCL_CPPFLAGS}

AM_CPPFLAGS = \
	-DRT_MULTISPECTRAL \
	${STRICT_FLAGS} \
	${TCL_CPPFLAGS}

libmultispectral_la_LDFLAGS = -version-info 20:1 -no-undefined -Wc,${FEXCEPTIONS}
libmultispectral_la_LIBADD = \
	${top_builddir}/src/liboptical/librtms.la \
	${MULTISPECTRAL_LIBS}

libmultispectral_la_SOURCES = \
	sh_temp.c

DEPENDS = src/librt src/liboptical

include $(top_srcdir)/misc/Makefile.defs

EXTRA_DIST = CMakeLists.txt
