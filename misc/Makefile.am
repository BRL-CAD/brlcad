
SUBDIRS = \
	enigma \
	macosx \
	pkgconfig

sample_applicationsdir = $(SAMPLE_APPLICATIONS_DIR)
sample_applications_DATA = fortran_example.f

bin_SCRIPTS = brlcad-config
man_MANS = brlcad-config.1

DATAFILES = \
	GQA_SAMPLE_DENSITIES \
	NIST_DENSITIES

EXTRA_DIST = \
	$(man_MANS) \
	$(sample_applications_DATA) \
	${DATAFILES} \
	CMake \
	CMakeLists.txt \
	Description.txt \
	Doxyfile \
	Doxyfile.in \
	Makefile.defs \
	archlinux/PKGBUILD \
	archlinux/brlcad.install \
	archlinux/brlcad.sh \
	batch-indent-region.el \
	brlcad-config \
	brlcad-config.in \
	brlcad.spec \
	brlcad.spec.in \
	debian/brlcad-archer.desktop \
	debian/brlcad-archer.png \
	debian/brlcad-db.desktop \
	debian/brlcad-db.png \
	debian/brlcad-doc-animation.desktop \
	debian/brlcad-doc-mged.desktop \
	debian/brlcad-doc.desktop \
	debian/brlcad-doc.directory \
	debian/brlcad-doc.png \
	debian/brlcad-mged.desktop \
	debian/brlcad-mged.png \
	debian/brlcad-rtwizard.desktop \
	debian/brlcad-v4.png \
	debian/brlcad-v5.png \
	debian/brlcad.directory \
	debian/brlcad.docs \
	debian/brlcad.install \
	debian/brlcad.menu \
	debian/brlcad.postinst \
	debian/brlcad.postrm \
	debian/brlcad.sh \
	debian/brlcad.xml \
	debian/changelog \
	debian/compat \
	debian/control \
	debian/copyright \
	debian/rules \
	debian/source/format \
	debian/source/include-binaries \
	doxygen_structure \
	flawfinder \
	libtool.m4 \
	nsis/archer.ico \
	nsis/brlcad.ico \
	nsis/brlcad.nsi \
	nsis/header.bmp \
	nsis/side.bmp \
	nsis/uninstall.ico \
	nsis/updateVersion.tcl \
	win32-msvc/CMakeLists.txt \
	win32-msvc/Dll/BrlcadCore.def \
	win32-msvc/Dll/BrlcadCore.rc \
	win32-msvc/Dll/CMakeLists.txt \
	win32-msvc/Dll/TclDummies.c \
	win32-msvc/Dll/brlcad.def \
	win32-msvc/Dll/brlcad.rc

datafilesdir = $(BRLCAD_DATA)/data

datafiles_DATA = \
	$(DATAFILES)

DISTCLEANFILES = \
	brlcad-config \
	brlcad.spec \
	compile \
	config.guess \
	config.guess.backup \
	config.sub \
	config.sub.backup \
	install-sh \
	ltmain.sh \
	ltmain.sh.backup \
	mkinstalldirs \
	py-compile \
	ylwrap

include $(top_srcdir)/misc/Makefile.defs
