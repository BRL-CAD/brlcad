# $Id$

required_dirs = \
	other \
	libbu \
	libbn \
	librt

rts_dirs = librtserver

bench_dirs = \
	libpkg \
	libfb \
	libwdb \
	liboptical \
	libsysv \
	liborle \
	conv \
	fbserv \
	rt	

# anything not included from above
kitchensink_dirs = \
	libcursor \
	libdm \
	libfft \
	libmultispectral \
	libtclcad \
	libtermio \
	anim \
	archer \
	brlman \
	burst \
	bwish \
	comgeom-g \
	fb \
	fbed \
	gtools \
	halftone \
	iges \
	irprep \
	jack \
	java \
	lgt \
	mged \
	mk \
	nirt \
	off \
	patch \
	proc-db \
	remrt \
	rttherm \
	sig \
	tab \
	tclscripts \
	unigraphics \
	util \
	vas4 \
	vdeck


#######################
# put it all together #
#######################

if ONLY_BENCHMARK

if ONLY_RTS
SUBDIRS = \
	$(required_dirs) \
	$(bench_dirs) \
	$(rts_dirs)

else !ONLY_RTS
SUBDIRS = \
	$(required_dirs) \
	$(bench_dirs)

endif
# end ONLY_RTS

else !ONLY_BENCHMARK

if ONLY_RTS
SUBDIRS = \
	$(required_dirs) \
	$(rts_dirs)

else !ONLY_RTS
SUBDIRS = \
	$(required_dirs) \
	$(bench_dirs) \
	$(rts_dirs) \
	$(kitchensink_dirs)

endif
# end ONLY_RTS

endif
# end ONLY_BENCHMARK

DIST_SUBDIRS= \
	$(SUBDIRS) \
	canon \
        adrt

MAINTAINERCLEANFILES = \
	Makefile.in

include $(top_srcdir)/misc/Makefile.defs

