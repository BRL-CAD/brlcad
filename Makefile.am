# $Id$

required_files = \
	h \
	libtcl8.4 \
	libbn \
	libbu \
	librt

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

rts_files = librtserver

kitchensink_files = \
	libpng \
	libtk8.4 \
	libitcl \
	iwidgets4.0.1 \
	libz \
	libcursor \
	libdm \
	libpkg \
	libfb \
	libfft \
	liborle \
	librtserver \
	libtclcad \
	libtermio \
	libutahrle \
	libwdb \
	anim \
	awf \
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


####################
# dist maintenance #
####################

DIST_SUBDIRS = $(SUBDIRS)

EXTRA_DIST = $(PACKAGE).spec.in

MAINTAINERCLEANFILES = \
	COPYING \
	INSTALL \
	Makefile.in \
	aclocal.m4 \
	autom4te.cache \
	brlcad.spec \
	config.guess \
	config.log \
	config.status \
	config.sub \
	configure \
	depcomp \
	install-sh \
	libtool \
	ltmain.sh \
	missing \
	mkinstalldirs


##############
# the rules  #
##############

.PHONY : rpm
rpm: dist
	cp $(PACKAGE)-$(VERSION).tar.gz ${HOME}/rpm/SOURCES/
	rpmbuild -ba $(PACKAGE).spec
	cp ${HOME}/rpm/RPMS/i386/$(PACKAGE)-$(VERSION)-*.i386.rpm .
	cp ${HOME}/rpm/SRPMS/$(PACKAGE)-$(VERSION)-*.src.rpm .

packages: rpm

benchmark: all
	sh $(srcdir)/bench/run.sh

bench: benchmark
