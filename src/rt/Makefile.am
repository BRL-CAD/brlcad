# $Id$

if ONLY_BENCHMARK
bin_PROGRAMS = \
	rt
else
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
endif

ALLSRCS = \
	do.c \
	libfb-dummy.c \
	main.c \
	opt.c \
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
	viewxray.c \
	worker.c

COMMON = \
	do.c \
	main.c \
	opt.c \
	worker.c

rt_SOURCES = \
	$(COMMON) \
	vers.c \
	view.c

BOTFACESSRC= \
	do.c \
	main.c \
	opt.c \
	view_bot_faces.c \
	worker.c

rt_bot_faces_vers.c: $(BOTFACESRC)
	sh $(top_srcdir)/sh/vers.sh version "RT BOT FACES" > $@
rt_bot_faces_SOURCES = 	$(BOTFACESSRC) rt_bot_faces_vers.c

rt_simple_vers.c: rt_simple.c
	sh $(top_srcdir)/sh/vers.sh version "RT simple?" > $@
rt_simple_SOURCES = \
	rt_simple.c \
	rt_simple_vers.c

rtarea_vers.c: $(COMMON) viewarea.c
	sh $(top_srcdir)/sh/vers.sh version "RT area?" > $@
rtarea_SOURCES = \
	$(COMMON) \
	rtarea_vers.c \
	viewarea.c

rtcell_vers.c: $(COMMON) viewcell.c
	sh $(top_srcdir)/sh/vers.sh version "RT cell?" > $@
rtcell_SOURCES = \
	$(COMMON) \
	rtcell_vers.c \
	viewcell.c

rtcheck_vers.c: $(COMMON) viewcheck.c
	sh $(top_srcdir)/sh/vers.sh version "RT Check" > $@
rtcheck_SOURCES = \
	$(COMMON) \
	rtcheck_vers.c \
	viewcheck.c

rtedge_vers.c: $(COMMON) viewedge.c
	sh $(top_srcdir)/sh/vers.sh version "RT LGT-Style Hidden-Line Renderer" > $@
rtedge_SOURCES = \
	$(COMMON) \
	rtedge_vers.c \
	viewedge.c

rtfrac_vers.c: $(COMMON) viewfrac.c
	sh $(top_srcdir)/sh/vers.sh version "RT frac?" > $@
rtfrac_SOURCES = \
	$(COMMON) \
	rtfrac_vers.c \
	viewfrac.c

rtg3_vers.c: $(COMMON) viewg3.c
	sh $(top_srcdir)/sh/vers.sh version "RTG3" > $@
rtg3_SOURCES = \
	$(COMMON) \
	rtg3_vers.c \
	viewg3.c 

rthide_vers.c: $(COMMON) viewhide.c
	sh $(top_srcdir)/sh/vers.sh version "RT Hidden-Line Plot" > $@
rthide_SOURCES = \
	$(COMMON) \
	rthide_vers.c \
	viewhide.c

rtpp_vers.c: $(COMMON) viewpp.c
	sh $(top_srcdir)/sh/vers.sh version "RT Pretty Picture" > $@
rtpp_SOURCES = \
	$(COMMON) \
	rtpp_vers.c \
	viewpp.c

rtrad_vers.c: $(COMMON) viewrad.c
	sh $(top_srcdir)/sh/vers.sh version "RT rad?" > $@
rtrad_SOURCES = \
	$(COMMON) \
	rtrad_vers.c \
	viewrad.c

rtrange_vers.c: $(COMMON) viewrange.c
	sh $(top_srcdir)/sh/vers.sh version "RT Range Plot" > $@
rtrange_SOURCES = \
	$(COMMON) \
	rtrange_vers.c \
	viewrange.c

rtray_vers.c: $(COMMON) viewray.c
	sh $(top_srcdir)/sh/vers.sh version "RT Ray" > $@
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
	sh $(top_srcdir)/sh/vers.sh version "RT sil?" > $@
rtsil_SOURCES = \
	$(COMMON) \
	rtsil_vers.c \
	viewsil.c

rtwalk_SOURCES = \
	rtwalk.c

rtweight_vers.c: $(COMMON) viewweight.c
	sh $(top_srcdir)/sh/vers.sh version "RT Weight" > $@
rtweight_SOURCES = \
	$(COMMON) \
	rtweight_vers.c \
	viewweight.c

rtxray_vers.c: $(COMMON) viewxray.c
	sh $(top_srcdir)/sh/vers.sh version "RT X-Ray" > $@
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
	rtcheck_vers.c \
	rtedge_vers.c \
	rtg3_vers.c \
	rthide_vers.c \
	rtpp_vers.c \
	rtrange_vers.c \
	rtray_vers.c \
	rtweight_vers.c \
	rtxray_vers.c \
	vers.c


vers.c: $(ALLSRCS)
	sh $(top_srcdir)/sh/vers.sh version "The BRL-CAD Raytracer RT" > $@


AM_CFLAGS = -I$(top_srcdir)/h

LDADD = @RT@ @FB@ @OPTICAL@

EXTRA_DIST = \
	$(ALLSRCS) \
	$(man_MANS)

