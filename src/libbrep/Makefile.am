
lib_LTLIBRARIES = libbrep.la
noinst_LTLIBRARIES = \
	libbrep_nil.la

# cannot set per-target CPPFLAGS until automake 1.7+
AM_CPPFLAGS = \
	${OPENNURBS_CPPFLAGS} \
	${SSE} \
	${TCL_CPPFLAGS} \
	${TNT_CPPFLAGS} \
	-DOBJ_BREP=1

AM_CFLAGS = ${STRICT_FLAGS}

libbrep_nil_la_SOURCES = \
	PullbackCurve.cpp \
	opennurbs_ext.cpp

libbrep_nil_la_LIBADD = ${OPENNURBS}

libbrep_la_SOURCES =
libbrep_la_LDFLAGS = -version-info 20:1 -no-undefined -Wc,${FEXCEPTIONS}
libbrep_la_LIBADD = \
	libbrep_nil.la \
	${NURBS_LIBS}

noinst_HEADERS = \
	PullbackCurve.h \
	opennurbs_fit.cpp \
	opennurbs_fit.h

EXTRA_DIST = \
	CMakeLists.txt

DEPENDS = src/libbn src/libsysv src/other/openNURBS ${DEPADD}

include $(top_srcdir)/misc/Makefile.defs
