bin_PROGRAMS = \
	fedex_os \
	fedex_idl \
	fedex_plus

AM_CPPFLAGS = \
	-I$(top_srcdir)/include/express \
	-I$(top_srcdir)/include/exppp \
	${NOWARN}

COMMON = \
	classes_misc.c

fedex_os_SOURCES = \
	$(COMMON) \
	fedex_os.c	

fedex_idl_SOURCES = \
	$(COMMON) \
	fedex_idl.c

fedex_plus_SOURCES = \
	$(COMMON) \
	fedex_main.c \
	classes_wrapper.cc \
	classes.c \
	selects.c \
	multpass.c \
	collect.cc \
	complexlist.cc \
	entlist.cc \
	multlist.cc \
	orlist.cc \
	entnode.cc \
	expressbuild.cc \
	non-ors.cc \
	match-ors.cc \
	trynext.cc \
	write.cc \
	print.cc

fedex_libs = \
	${EXPRESS} \
	${EXPPP} \
	$(top_builddir)/src/express/fedex.o

fedex_os_LDADD = $(fedex_libs)
fedex_idl_LDADD = $(fedex_libs)
fedex_plus_LDADD = $(fedex_libs)

noinst_HEADERS = \
	classes.h \
	complexSupport.h

EXTRA_DIST = CMakeLists.txt

DEPS = \
	src/express \
	src/exppp

include $(top_srcdir)/misc/Makefile.defs

FAST_OBJECTS = \
	$(fedex_os_OBJECTS) \
	$(fedex_idl_OBJECTS) \
	$(fedex_plus_OBJECTS)
