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
	fb \
	fbed \
	gtools \
	halftone \
	irprep \
	java \
	lgt \
	mged \
	mk \
	nirt \
	proc-db \
	remrt \
	rttherm \
	sig \
	tab \
	tclscripts \
	util \
	vas4 \
	vdeck

if BUILD_ADRT
adrt_dir = adrt
else
adrt_dir =
endif

#######################
# put it all together #
#######################

if ONLY_BENCHMARK

if ONLY_RTS
SUBDIRS = \
	$(required_dirs) \
	$(bench_dirs) \
	$(rts_dirs) \
	$(adrt_dir)

else !ONLY_RTS
SUBDIRS = \
	$(required_dirs) \
	$(bench_dirs) \
	$(adrt_dir)

endif
# end ONLY_RTS

else !ONLY_BENCHMARK

if ONLY_RTS
SUBDIRS = \
	$(required_dirs) \
	$(rts_dirs) \
	$(adrt_dir)

else !ONLY_RTS
SUBDIRS = \
	$(required_dirs) \
	$(bench_dirs) \
	$(rts_dirs) \
	$(kitchensink_dirs) \
	$(adrt_dir)

endif
# end ONLY_RTS

endif
# end ONLY_BENCHMARK

DIST_SUBDIRS= \
	$(required_dirs) \
	$(bench_dirs) \
	$(rts_dirs) \
	$(kitchensink_dirs) \
	adrt \
	canon

MAINTAINERCLEANFILES = \
	Makefile.in

EXTRA_DIST = README

include $(top_srcdir)/misc/Makefile.defs
