
lib_LTLIBRARIES = libsysv.la

libsysv_la_SOURCES = \
	memset.c \
	strchr.c \
	strdup.c \
	strsep.c \
	strtok.c \
	tcl.c

libsysv_la_CFLAGS = ${TCL_CPPFLAGS}
libsysv_la_LDFLAGS = -version-info 19:1 -no-undefined
libsysv_la_LIBADD = ${SYSV_LIBS}

AM_CFLAGS = ${STRICT_FLAGS}

EXTRA_DIST = CMakeLists.txt

include $(top_srcdir)/misc/Makefile.defs
