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
	conv \
	fbserv \
	rt	

kitchensink_dirs = \
	libcursor \
	libdm \
	libfb \
	libfft \
	libmultispectral \
	liborle \
	libpkg \
	librtserver \
	libtclcad \
	libtermio \
	libwdb \
	anim \
	brlman \
	burst \
	bwish \
	comgeom-g \
	canon \
	conv \
	fb \
	fbed \
	fbserv \
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
	rt \
	rttherm \
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
