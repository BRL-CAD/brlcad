# $Id$

SUBDIRS = \
	h \
	libpng \
	libtcl8.4 \
	libtk8.4 \
	libitcl \
	iwidgets4.0.1 \
	libz \
	libbn \
	libbu \
	libcursor \
	libdm \
	libpkg \
	libfb \
	libfft \
	liboptical \
	liborle \
	librt \
	librtserver \
	libsysv \
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

DIST_SUBDIRS = $(SUBDIRS)

EXTRA_DIST = ChangeLog $(PACKAGE).spec.in

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

