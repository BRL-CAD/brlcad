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
	vers.c \
	worker.c

rt_SOURCES = $(COMMON) view.c
rt_simple_SOURCES = rt_simple.c
rtarea_SOURCES = $(COMMON) viewarea.c
rtcell_SOURCES = $(COMMON) viewcell.c
rtcheck_SOURCES = $(COMMON) viewcheck.c
rtedge_SOURCES = $(COMMON) viewedge.c
rtfrac_SOURCES = $(COMMON) viewfrac.c
rtg3_SOURCES = $(COMMON) viewg3.c
rthide_SOURCES = $(COMMON) viewhide.c
rtpp_SOURCES = $(COMMON) viewpp.c
rtrad_SOURCES = $(COMMON) viewrad.c
rtrange_SOURCES = $(COMMON) viewrange.c
rtray_SOURCES = $(COMMON) viewray.c
rtregis_SOURCES = rtregis.c read-rtlog.c
rtscale_SOURCES = rtscale.c read-rtlog.c
rtshot_SOURCES = rtshot.c
rtsil_SOURCES = $(COMMON) viewsil.c
rtwalk_SOURCES = rtwalk.c
rtweight_SOURCES = $(COMMON) viewweight.c
rtxray_SOURCES = $(COMMON) viewxray.c
rt_bot_faces_SOURCES = \
	rt_bot_faces_vers.c \
	view_bot_faces.c \
	main.c \
	opt.c \
	do.c \
	worker.c

noinst_HEADERS = \
	ext.h \
	light.h \
	rad.h \
	scat.h

AM_CFLAGS = -I$(top_srcdir)/h

LDADD = @RT@ @FB@ @OPTICAL@

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

EXTRA_DIST=$(man_MANS) $(ALLSRCS)

BUILT_SOURCES = vers.c rt_bot_faces_vers.c
vers.c: $(ALLSRCS)
	$(top_srcdir)/sh/newvers2.sh > $@
rt_bot_faces_vers.c: $(ALLSRCS)
	$(top_srcdir)/sh/newvers2.sh version "RT BOT FACES" > $@
