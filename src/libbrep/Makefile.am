
lib_LTLIBRARIES = libnurbs.la
noinst_LTLIBRARIES = \
	libnurbs_nil.la

# cannot set per-target CPPFLAGS until automake 1.7+
AM_CPPFLAGS = \
	${OPENNURBS_CPPFLAGS} \
	${SSE} \
	${TCL_CPPFLAGS} \
	${TNT_CPPFLAGS} \
	-DOBJ_BREP=1

AM_CFLAGS = ${STRICT_FLAGS}

libnurbs_nil_la_SOURCES = \
	PullbackCurve.cpp \
	opennurbs_ext.cpp

libnurbs_nil_la_LIBADD = ${OPENNURBS}

libnurbs_la_SOURCES =
libnurbs_la_LDFLAGS = -version-info 20:1 -no-undefined -Wc,${FEXCEPTIONS}
libnurbs_la_LIBADD = \
	libnurbs_nil.la \
	${NURBS_LIBS}

noinst_HEADERS = \
	PullbackCurve.h \
	opennurbs_fit.cpp \
	opennurbs_fit.h

EXTRA_DIST = \
	CMakeLists.txt

DEPENDS = src/libbn src/libsysv src/other/openNURBS ${DEPADD}
