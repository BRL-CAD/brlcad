# $Id$

AM_CPPFLAGS = ${TCL_CPPFLAGS}
LDADD = \
	${OPTICAL} ${OPTICAL_LIBS} \
	${RT} ${RT_LIBS} \
	${FB} ${FB_LIBS} \
	${BN} ${BN_LIBS} \
	${BU} ${BU_LIBS} \
	${PKG} ${PKG_LIBS} \
	${TCL} \
	${SYSV} ${SYSV_LIBS}

if ONLY_BENCHMARK
bin_PROGRAMS = \
	rt
else !ONLY_BENCHMARK
bin_PROGRAMS = \
	rt \
	rt_bot_faces \
	rt_simple \
	rtarea \
	rtcell \
	rtcheck \
	rtedge \
	rtfrac \
	rtg3 \
	rthide \
	rtpp \
	rtrad \
	rtrange \
	rtray \
	rtregis \
	rtscale \
	rtshot \
	rtsil \
	rtwalk \
	rtweight \
	rtxray

noinst_PROGRAMS = \
	rtexample

endif
# end of ONLY_BENCHMARK

COMMON = \
	do.c \
	main.c \
	opt.c \
	worker.c

ALLSRCS = \
	$(COMMON) \
	libfb-dummy.c \
	read-rtlog.c \
	rt_simple.c \
	rtexample.c \
	rtregis.c \
	rtscale.c \
	rtshot.c \
	rtwalk.c \
	sh_tcl.c \
	view.c \
	viewarea.c \
	viewcell.c \
	viewcheck.c \
	viewdir.c \
	viewdummy.c \
	viewedge.c \
	viewfrac.c \
	viewg3.c \
	viewhide.c \
	viewpp.c \
	viewrad.c \
	viewrange.c \
	viewray.c \
	viewscat.c \
	viewsil.c \
	viewweight.c \
	viewxray.c

rt_simple_SOURCES = rt_simple.c
rtexample_SOURCES = rtexample.c
rtwalk_SOURCES = rtwalk.c
rtshot_SOURCES = rtshot.c

rtregis_SOURCES = \
	read-rtlog.c \
	rtregis.c

rtscale_SOURCES = \
	read-rtlog.c \
	rtscale.c


rt_SOURCES = \
	$(COMMON) \
	view.c

rt_bot_faces_SOURCES = 	\
	$(COMMON) \
	view_bot_faces.c

rtarea_SOURCES = \
	$(COMMON) \
	viewarea.c

rtcell_SOURCES = \
	$(COMMON) \
	viewcell.c

rtcheck_SOURCES = \
	$(COMMON) \
	viewcheck.c

rtedge_SOURCES = \
	$(COMMON) \
	viewedge.c

rtfrac_SOURCES = \
	$(COMMON) \
	viewfrac.c

rtg3_SOURCES = \
	$(COMMON) \
	viewg3.c

rthide_SOURCES = \
	$(COMMON) \
	viewhide.c

rtpp_SOURCES = \
	$(COMMON) \
	viewpp.c

rtrad_SOURCES = \
	$(COMMON) \
	viewrad.c

rtrange_SOURCES = \
	$(COMMON) \
	viewrange.c

rtray_SOURCES = \
	$(COMMON) \
	viewray.c

rtsil_SOURCES = \
	$(COMMON) \
	viewsil.c

rtweight_SOURCES = \
	$(COMMON) \
	viewweight.c

rtxray_SOURCES = \
	$(COMMON) \
	viewxray.c


noinst_HEADERS = \
	ext.h \
	rad.h \
	scat.h

man_MANS = \
	rt.1 \
	rt_bot_faces.1 \
	rtarea.1 \
	rtcheck.1 \
	rtedge.1 \
	rtg3.1 \
	rthide.1 \
	rtpp.1 \
	rtrange.1 \
	rtray.1 \
	rtregis.1 \
	rtscale.1 \
	rtshot.1 \
	rtwalk.1 \
	rtweight.1 \
	rtxray.1

noinst_LIBRARIES = \
	libremrt.a \
	librttherm.a

librttherm_a_SOURCES = $(COMMON)

libremrt_a_CFLAGS = -DRTSRV
libremrt_a_SOURCES = \
	do.c \
	opt.c \
	sh_tcl.c \
	view.c \
	worker.c

sample_applicationsdir=$(SAMPLE_APPLICATIONS_DIR)
sample_applications_DATA = rtexample.c

EXTRA_DIST = \
	$(ALLSRCS) \
	$(man_MANS) \
	rt.dsp

MAINTAINERCLEANFILES = Makefile.in

DEPS=src/librt src/libfb src/liboptical

include $(top_srcdir)/misc/Makefile.defs

if ONLY_BENCHMARK
FAST_OBJECTS = \
	$(rt_SOURCES) \
	$(bin_PROGRAMS)

else !ONLY_BENCHMARK
FAST_OBJECTS = \
	$(rt_OBJECTS) \
	$(rt_bot_faces_OBJECTS) \
	$(rt_simple_OBJECTS) \
	$(rtarea_OBJECTS) \
	$(rtcell_OBJECTS) \
	$(rtcheck_OBJECTS) \
	$(rtedge_OBJECTS) \
	$(rtfrac_OBJECTS) \
	$(rtg3_OBJECTS) \
	$(rthide_OBJECTS) \
	$(rtpp_OBJECTS) \
	$(rtrad_OBJECTS) \
	$(rtrange_OBJECTS) \
	$(rtray_OBJECTS) \
	$(rtregis_OBJECTS) \
	$(rtscale_OBJECTS) \
	$(rtshot_OBJECTS) \
	$(rtsil_OBJECTS) \
	$(rtwalk_OBJECTS) \
	$(rtweight_OBJECTS) \
	$(rtxray_OBJECTS) \
	$(rtexample_OBJECTS) \
	$(bin_PROGRAMS) \
	$(noinst_PROGRAMS)

endif
# end of ONLY_BENCHMARK
