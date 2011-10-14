
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
	CMake/BRLCAD_CPackOptions.cmake.in \
	CMake/BRLCAD_CheckFunctions.cmake \
	CMake/BRLCAD_CompilerFlags.cmake \
	CMake/BRLCAD_Dependent_Options.cmake \
	CMake/BRLCAD_Util.cmake \
	CMake/CheckCFileRuns.cmake \
	CMake/CheckPrototypeExists.cmake \
	CMake/CompilerFlags.cmake \
	CMake/DiffCache.cmake \
	CMake/FindGL.cmake \
	CMake/FindLEMON.cmake \
	CMake/FindLEX.cmake \
	CMake/FindOIIO.cmake \
	CMake/FindOPENNURBS.cmake \
	CMake/FindOSL.cmake \
	CMake/FindOpenEXR.cmake \
	CMake/FindPkgMacros.cmake \
	CMake/FindRE2C.cmake \
	CMake/FindREGEX.cmake \
	CMake/FindSCL.cmake \
	CMake/FindShellDeps.cmake \
	CMake/FindTBB.cmake \
	CMake/FindTCL.cmake \
	CMake/FindTERMLIB.cmake \
	CMake/FindUTAHRLE.cmake \
	CMake/FindX11.cmake \
	CMake/FindYACC.cmake \
	CMake/FindZLIB.cmake \
	CMake/Fink_MacPorts.cmake \
	CMake/NSIS.template.in \
	CMake/ResetCache.cmake \
	CMake/ResolveCompilerPaths.cmake \
	CMake/ThirdParty.cmake \
	CMake/ThirdParty_TCL.cmake \
	CMake/distcheck_buildsys.cmake.in \
	CMake/distcheck_message.in \
	CMake/test_srcs/alloca_header_test.c \
	CMake/test_srcs/alloca_test.c \
	CMake/test_srcs/builddelta_end.c.in \
	CMake/test_srcs/builddelta_start.c.in \
	CMake/test_srcs/ctypes_test.c \
	CMake/test_srcs/lex_test.l \
	CMake/test_srcs/print_timestamp.c \
	CMake/test_srcs/report_hostname.c.in \
	CMake/test_srcs/report_username.c.in \
	CMake/test_srcs/sys_wait_test.c \
	CMake/test_srcs/termlib.c.in \
	CMake/test_srcs/time.c.in \
	CMake/test_srcs/timedelta_end.c.in \
	CMake/test_srcs/timedelta_start.c.in \
	CMake/util_macros.cmake \
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
