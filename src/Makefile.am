# $Id$

required_files = \
	other \
	libbn \
	libbu \
	librt

rts_files = librtserver

bench_files = \
	libpkg \
	libfb \
	libwdb \
	liboptical \
	libsysv \
	bench \
	conv \
	db \
	fbserv \
	rt	

kitchensink_files = \
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
	bench \
	brlman \
	burst \
	bwish \
	comgeom-g \
	conv \
	db \
	doc \
	fb \
	fbed \
	fbserv \
	gtools \
	halftone \
	html \
	iges \
	irprep \
	jack \
	lgt \
	mged \
	mk \
	nirt \
	off \
	patch \
	pro-engineer \
	remrt \
	rt \
	sample_applications \
	sh \
	sig \
	tab \
	tclscripts \
	tools \
	unigraphics \
	util \
	vas4 \
	vdeck \
	vfont


#######################
# put it all together #
#######################

if ONLY_BENCHMARK

if ONLY_RTS
SUBDIRS = \
	$(required_files) \
	$(bench_files) \
	$(rts_files)

else !ONLY_RTS
SUBDIRS = \
	$(required_files) \
	$(bench_files)

endif
# end ONLY_RTS

else !ONLY_BENCHMARK

if ONLY_RTS
SUBDIRS = \
	$(required_files) \
	$(rts_files)

else !ONLY_RTS
SUBDIRS = \
	$(required_files) \
	$(bench_files) \
	$(rts_files) \
	$(kitchensink_files)

endif
# end ONLY_RTS

endif
# end ONLY_BENCHMARK


MAINTAINERCLEANFILES = \
	Makefile.in
