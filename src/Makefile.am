# $Id$

required_dirs = \
	other \
	libbn \
	libbu \
	librt

rts_dirs = librtserver

bench_dirs = \
	libpkg \
	libfb \
	libwdb \
	liboptical \
	libsysv \
	conv \
	fbserv \
	rt	

kitchensink_dirs = \
	libcursor \
	libdm \
	libpkg \
	libfb \
	libfft \
	librtserver \
	libtclcad \
	libtermio \
	libwdb \
	anim \
	brlman \
	burst \
	bwish \
	comgeom-g \
	conv \
	fb \
	fbed \
	fbserv \
	gtools \
	halftone \
	iges \
	irprep \
	jack \
	lgt \
	mged \
	mk \
	nirt \
	off \
	patch \
	remrt \
	rt \
	sample_applications \
	sig \
	tab \
	tclscripts \
	tools \
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


MAINTAINERCLEANFILES = \
	Makefile.in
