# $Id$

required_files = \
	h \
	libtcl8.4 \
	libbn \
	libbu \
	librt

if ONLY_BENCHMARK
bench_files = \
	libpkg \
	libfb \
	libwdb \
	libsysv \
	bench \
	conv \
	db \
	fbserv \
	rt	
else
bench_files =
endif

if ONLY_RTS
rts_files = librtserver
else
rts_files = 
endif

if ONLY_BENCHMARK
kitchensink_files =

else
# else !ONLY_BENCHMARK

if ONLY_RTS
kitchensink_files =

else
# else !ONLY_RTS
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
	liboptical \
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
endif
# end ONLY_RTS

endif
# end ONLY_BENCHMARK


SUBDIRS = \
	$(required_files) \
	$(bench_files) \
	$(rts_files) \
	$(kitchensink_files)


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

.PHONY : rpm
rpm: dist
	cp $(PACKAGE)-$(VERSION).tar.gz ${HOME}/rpm/SOURCES/
	rpmbuild -ba $(PACKAGE).spec
	cp ${HOME}/rpm/RPMS/i386/$(PACKAGE)-$(VERSION)-*.i386.rpm .
	cp ${HOME}/rpm/SRPMS/$(PACKAGE)-$(VERSION)-*.src.rpm .

packages: rpm

