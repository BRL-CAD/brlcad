# $Id$

LDADD = ${LIBRT} ${FB} ${OPTICAL} ${TCL}

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

rt_SOURCES = \
	$(COMMON) \
	vers.c \
	view.c

rt_bot_faces_vers.c: view_bot_faces.c
	@SH@ $(top_srcdir)/sh/vers.sh version "RT BoT Faces" > $@
rt_bot_faces_SOURCES = 	\
	$(COMMON) \
	view_bot_faces.c \
	rt_bot_faces_vers.c

rt_simple_vers.c: rt_simple.c
	@SH@ $(top_srcdir)/sh/vers.sh version "RT Simple" > $@
rt_simple_SOURCES = \
	rt_simple.c \
	rt_simple_vers.c

rtarea_vers.c: $(COMMON) viewarea.c
	@SH@ $(top_srcdir)/sh/vers.sh version "RT Area" > $@
rtarea_SOURCES = \
	$(COMMON) \
	rtarea_vers.c \
	viewarea.c

rtcell_vers.c: $(COMMON) viewcell.c
	@SH@ $(top_srcdir)/sh/vers.sh version "RT Cell" > $@
rtcell_SOURCES = \
	$(COMMON) \
	rtcell_vers.c \
	viewcell.c

rtcheck_vers.c: $(COMMON) viewcheck.c
	@SH@ $(top_srcdir)/sh/vers.sh version "RT Check" > $@
rtcheck_SOURCES = \
	$(COMMON) \
	rtcheck_vers.c \
	viewcheck.c

rtedge_vers.c: $(COMMON) viewedge.c
	@SH@ $(top_srcdir)/sh/vers.sh version "RT LGT-Style Hidden-Line Renderer" > $@
rtedge_SOURCES = \
	$(COMMON) \
	rtedge_vers.c \
	viewedge.c

rtexample_SOURCES = \
	rtexample.c

rtfrac_vers.c: $(COMMON) viewfrac.c
	@SH@ $(top_srcdir)/sh/vers.sh version "RT Volume Fractions" > $@
rtfrac_SOURCES = \
	$(COMMON) \
	rtfrac_vers.c \
	viewfrac.c

rtg3_vers.c: $(COMMON) viewg3.c
	@SH@ $(top_srcdir)/sh/vers.sh version "RTG3" > $@
rtg3_SOURCES = \
	$(COMMON) \
	rtg3_vers.c \
	viewg3.c

rthide_vers.c: $(COMMON) viewhide.c
	@SH@ $(top_srcdir)/sh/vers.sh version "RT Hidden-Line Plot" > $@
rthide_SOURCES = \
	$(COMMON) \
	rthide_vers.c \
	viewhide.c

rtpp_vers.c: $(COMMON) viewpp.c
	@SH@ $(top_srcdir)/sh/vers.sh version "RT Pretty Picture" > $@
rtpp_SOURCES = \
	$(COMMON) \
	rtpp_vers.c \
	viewpp.c

rtrad_vers.c: $(COMMON) viewrad.c
	@SH@ $(top_srcdir)/sh/vers.sh version "RTRAD" > $@
rtrad_SOURCES = \
	$(COMMON) \
	rtrad_vers.c \
	viewrad.c

rtrange_vers.c: $(COMMON) viewrange.c
	@SH@ $(top_srcdir)/sh/vers.sh version "RT Range Plot" > $@
rtrange_SOURCES = \
	$(COMMON) \
	rtrange_vers.c \
	viewrange.c

rtray_vers.c: $(COMMON) viewray.c
	@SH@ $(top_srcdir)/sh/vers.sh version "RT Ray" > $@
rtray_SOURCES = \
	$(COMMON) \
	rtray_vers.c \
	viewray.c

rtregis_SOURCES = \
	read-rtlog.c \
	rtregis.c

rtscale_SOURCES = \
	read-rtlog.c \
	rtscale.c

rtshot_SOURCES = \
	rtshot.c

rtsil_vers.c: $(COMMON) viewsil.c
	@SH@ $(top_srcdir)/sh/vers.sh version "RT Simple Intersection Lightmap" > $@
rtsil_SOURCES = \
	$(COMMON) \
	rtsil_vers.c \
	viewsil.c

rtwalk_SOURCES = \
	rtwalk.c

rtweight_vers.c: $(COMMON) viewweight.c
	@SH@ $(top_srcdir)/sh/vers.sh version "RT Weight" > $@
rtweight_SOURCES = \
	$(COMMON) \
	rtweight_vers.c \
	viewweight.c

rtxray_vers.c: $(COMMON) viewxray.c
	@SH@ $(top_srcdir)/sh/vers.sh version "RT X-Ray" > $@
rtxray_SOURCES = \
	$(COMMON) \
	rtxray_vers.c \
	viewxray.c


noinst_HEADERS = \
	ext.h \
	light.h \
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

BUILT_SOURCES = \
	rt_bot_faces_vers.c \
	rt_simple_vers.c \
	rtarea_vers.c \
	rtcell_vers.c \
	rtcheck_vers.c \
	rtedge_vers.c \
	rtfrac_vers.c \
	rtg3_vers.c \
	rthide_vers.c \
	rtpp_vers.c \
	rtrad_vers.c \
	rtrange_vers.c \
	rtray_vers.c \
	rtsil_vers.c \
	rtweight_vers.c \
	rtxray_vers.c \
	vers.c

vers.c: $(COMMON) view.c
	@SH@ $(top_srcdir)/sh/vers.sh version "The BRL-CAD Raytracer RT" > $@

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
	rt.dsp \
	rtcheck_vers_win.c \
	rtedge_vers_win.c \
	vers_win.c

CLEANFILES = $(BUILT_SOURCES)
DISTCLEANFILES = version
MAINTAINERCLEANFILES = Makefile.in

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
