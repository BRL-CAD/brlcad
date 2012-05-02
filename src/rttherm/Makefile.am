
ssampview_SRCS = \
	spectrum.c \
	ssampview.c


if WITH_X11
SSAMPVIEW = ssampview

ssampview_SOURCES = ${ssampview_SRCS}
ssampview_CFLAGS = \
	${X_CFLAGS} \
	${TK_CPPFLAGS}
ssampview_LDADD = \
	${OPTICAL} \
	${FB} \
	${TK}
SSAMPVIEW_EXTRA =

else
SSAMPVIEW =
SSAMPVIEW_EXTRA = ${ssampview_SRCS}
endif

# pixtest removed, looks like an outdated sample_application
bin_PROGRAMS = \
	rttherm \
	ssamp-bw \
	${SSAMPVIEW}

noinst_PROGRAMS = pixtest

AM_CPPFLAGS = \
	${STRICT_FLAGS} \
	${TCL_CPPFLAGS}

dist_bin_SCRIPTS = ssampview.tcl

man_MANS = \
	rttherm.1 \
	ssamp-bw.1

rttherm_SOURCES = \
	spectrum.c \
	viewtherm.c

# have to link static due to librttherm.a, the aix linker, and duplicate symbols in optical/multispectral
rttherm_LDFLAGS = -static
rttherm_CFLAGS = -DRT_MULTISPECTRAL
rttherm_LDADD = \
	$(top_builddir)/src/rt/librttherm.a \
	${MULTISPECTRAL} \
	${OPTICAL} \
	${FB} \
	${GED}

ssamp_bw_SOURCES = ssamp-bw.c
ssamp_bw_LDADD = ${OPTICAL}

# pixtest removed, looks like an outdated sample_application
pixtest_SOURCES = \
	pixtest.c \
	spectrum.c
pixtest_LDADD = \
	${MULTISPECTRAL} \
	${OPTICAL}

EXTRA_DIST = \
	$(man_MANS) \
	${SSAMPVIEW_EXTRA} \
	std_day_1km.dat \
	test.sh \
	CMakeLists.txt

DEPENDS = src/rt src/libged src/libmultispectral

include $(top_srcdir)/misc/Makefile.defs

FAST_OBJECTS = \
	$(rttherm_OBJECTS) \
	$(ssamp_bw_OBJECTS) \
	$(ssampview_OBJECTS) \
	$(bin_PROGRAMS)
