# $Id$

SUBDIRS=\
	h\
	libpng\
	libtcl8.4\
	libtk8.4\
	libitcl3.2\
	iwidgets4.0.1\
	libz\
	libbn\
	libbu\
	libcursor\
	libdm\
	libpkg\
	libfb\
	libfft\
	liboptical\
	liborle\
	librt\
	libsysv\
	libtermio\
	libutahrle\
	libwdb\
	anim\
	awf\
	bench\
	brlman\
	burst\
	bwish\
	comgeom-g\
	conv\
	doc\
	fb\
	fbed\
	fbserv\
	gtools\
	halftone\
	html\
	iges\
	irprep\
	jack\
	jove\
	lgt\
	mged\
	mk\
	nirt\
	off\
	patch\
	pro-engineer\
	remrt\
	rt\
	sample_applications\
	sh\
	sig\
	tab\
	tclscripts\
	tools\
	unigraphics\
	util\
	vas4\
	vdeck\
	vfont

EXTRA_DIST=ChangeLog $(PACKAGE).spec.in
 
.PHONY : rpm
rpm: dist
	cp $(PACKAGE)-$(VERSION).tar.gz ${HOME}/rpm/SOURCES/
	rpmbuild -ba $(PACKAGE).spec
	cp ${HOME}/rpm/RPMS/i386/$(PACKAGE)-$(VERSION)-*.i386.rpm .
	cp ${HOME}/rpm/SRPMS/$(PACKAGE)-$(VERSION)-*.src.rpm .

packages: rpm

