
if WITH_JAVA
lib_LTLIBRARIES = librtserver.la
else
lib_LTLIBRARIES =
endif

librtserver_la_SOURCES = rtserver.c

AM_CFLAGS = \
	${STRICT_FLAGS} \
	${JAVA_CFLAGS}

AM_CPPFLAGS = ${TCL_CPPFLAGS}

librtserver_la_LDFLAGS = -version-info 19:1 ${JAVA_LDFLAGS} -module -no-undefined -Wc,${FEXCEPTIONS}

if BUILD_OPENNURBS
ON_NIL = $(top_builddir)/src/other/openNURBS/libopenNURBS_nil.la
else
ON_NIL =
endif

librtserver_la_LIBADD = \
	$(top_builddir)/src/libbu/libbu_nil.la \
	$(top_builddir)/src/libbn/libbn_nil.la \
	$(top_builddir)/src/librt/librt_nil.la \
	${RT_LIBS} \
	${BN_LIBS} \
	${BU_LIBS} \
	${ON_NIL} \
	${TCL}

EXTRA_PROGRAMS = rtserverTest

EXTRA_DIST = CMakeLists.txt

rtserverTest_SOURCES = rtserverTest.c

rtserverTest_LDADD = ${lib_LTLIBRARIES}

DEPENDS = src/librt

include $(top_srcdir)/misc/Makefile.defs
