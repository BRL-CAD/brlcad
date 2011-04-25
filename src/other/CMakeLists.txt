include_directories(
	${BRLCAD_BINARY_DIR}/include
	${BRLCAD_SOURCE_DIR}/include
	)

#-----------------------------------------------------------------------------
# Load some CMake macros to handle the special case of third party libraries.
INCLUDE(${BRLCAD_CMAKE_DIR}/ThirdParty.cmake)

# Quiet all warnings in this directory
IF(BRLCAD-DISABLE_SRC_OTHER_WARN)
	add_definitions(-w)
ENDIF(BRLCAD-DISABLE_SRC_OTHER_WARN)

#---------------------------------------------------------------------------------
# Optionally Building Local Copies of External Libraries 
#
# Need to get a bit fancy here - since we want to be able to turn local libs
# on/off at will, need to be careful about how we handle key variables. Approach
# is to stash values in BRLCAD_* prefixed versions, then restore them after
# the various Find*.cmake commands do their thing.  If NOT doing local build,
# then the BRLCAD_* and standard variables will be the same.  It remains to be
# seen if there will be enough consistency for a macro - the variables that
# need to be overridden will vary depending on the particular tests made by each
# subdirectory's CMake logic.
#---------------------------------------------------------------------------------

# Ignore the boost subdir - it's only used for the boost headers and is not
# compiled
DISTCHECK_IGNORE_ITEM(boost)

# The incrtTcl subdirectory isn't directly referenced, but its subdirectories
# are used - mark incrTcl as ignored
DISTCHECK_IGNORE_ITEM(incrTcl)

# zlib Library
# The autotools build needs this file, but the CMake zlib build doesn't tolerate 
# it - remove it if it's there. 
FILE(REMOVE ${CMAKE_CURRENT_SOURCE_DIR}/libz/zconf.h)
THIRD_PARTY_OPTION(ZLIB zlib)
THIRD_PARTY_SUBDIR(ZLIB libz)
SET(libz_ignore_files
	ChangeLog
	FAQ
	INDEX
	README
	amiga
	configure
	contrib
	doc
	examples
	make_vms.com
	msdos
	nintendods
	old
	qnx
	treebuild.xml
	watcom
	win32
	zconf.h.cmakein
	zconf.h.in
	zlib.3
	zlib.3.pdf
	zlib.map
	zlib.pc.in
	zlib2ansi
	win32/DLL_FAQ.txt
	win32/Makefile.bor
	win32/Makefile.emx
	win32/Makefile.gcc
	win32/Makefile.msc
	win32/README-WIN32.txt
	win32/VisualC.txt
	win32/zlib.def
	)
DISTCHECK_IGNORE(libz libz_ignore_files)

# libtermlib Library
#Only on WIN32 will the combination of the build search tests and
#libtermlib fail to find a valid term, so set variables accordingly
IF(NOT WIN32)
       SET(HAVE_TERMLIB 1 CACHE BOOL "Have some termlib" FORCE)
ELSE(NOT WIN32)
       SET(BRLCAD_BUILD_LOCAL_TERMLIB_FORCE_OFF ON CACHE BOOL "Disable libtermlib      build." FORCE)
       SET(HAVE_TERMLIB 0 CACHE BOOL "Have some termlib" FORCE)
ENDIF(NOT WIN32)
MARK_AS_ADVANCED(HAVE_TERMLIB)

IF(HAVE_TERMLIB)
   THIRD_PARTY_OPTION(TERMLIB termlib)
   THIRD_PARTY_SUBDIR(TERMLIB libtermlib)
ENDIF(HAVE_TERMLIB)
SET(termlib_ignore_files
	termcap.h
	termcap.src
	)
DISTCHECK_IGNORE(libtermlib termlib_ignore_files)

# libpng Library - Checks for ZLIB, so need to handle libpng AFTER zlib to set the variables
# if using a local copy of zlib.  If not using a local copy of zlib, FindZLIB results will 
# be identical in both cases so there is no danger of harming the libpng setup.  The PNG
# CMake system needs some options set and some variables translated to fit BRL-CAD's
# standard assumptions, so handle that here as well.
THIRD_PARTY_OPTION(PNG png)
# BRL-CAD needs PNG's IO - turn it on (i.e. turn off the disabling flags)
SET(PNG_NO_CONSOLE_IO OFF CACHE BOOL "Option to disable Console IO in PNG" FORCE)
MARK_AS_ADVANCED(PNG_NO_CONSOLE_IO)
SET(PNG_NO_STDIO OFF CACHE BOOL "Option to disable STDIO in PNG" FORCE)
MARK_AS_ADVANCED(PNG_NO_STDIO)
SET(SKIP_INSTALL_EXPORT ON CACHE BOOL "We dont't want export for this application" FORCE)
MARK_AS_ADVANCED(SKIP_INSTALL_EXPORT)
SET(PNG_MAN_DIR ${MAN_DIR} CACHE STRING "Set PNG_MAN_DIR to the global MAN_DIR" FORCE)
MARK_AS_ADVANCED(PNG_MAN_DIR)
SET(CMAKE_INSTALL_LIBDIR ${LIB_DIR})
THIRD_PARTY_SUBDIR(PNG libpng)
IF(BRLCAD_BUILD_LOCAL_PNG)
	# PNG_LIB_NAME doesn't automatically propogate to toplevel due to scoping of CMake variables - 
	# use get_directory_property to recover the value and place it in the CACHE
	get_directory_property(PNG_LIBRARY DIRECTORY libpng DEFINITION PNG_LIB_NAME)
	SET(PNG_LIBRARY "${PNG_LIBRARY}" CACHE STRING "libpng name for targets" FORCE)
ELSE(BRLCAD_BUILD_LOCAL_PNG)
	# The PNG CMake file seems to set PNG_PNG_INCLUDE_DIR rather than PNG_INCLUDE_DIR, so
	# a generic macro won't pick up the setting - handle it here
	SET(PNG_INCLUDE_DIR ${PNG_PNG_INCLUDE_DIR} CACHE STRING "PNG include directory" FORCE)
ENDIF(BRLCAD_BUILD_LOCAL_PNG)
# Other PNG options to mark as advanced
MARK_AS_ADVANCED(PNGARG)
MARK_AS_ADVANCED(PNG_DEBUG)
MARK_AS_ADVANCED(PNG_SHARED)
MARK_AS_ADVANCED(PNG_STATIC)
MARK_AS_ADVANCED(PNG_TESTS)
MARK_AS_ADVANCED(uname_executable)
SET(png_ignore_files
	ANNOUNCE
	CHANGES
	INSTALL
	LICENSE
	README
	TODO
	autogen.sh
	configure.ac
	contrib
	example.c
	libpng-1.4.5.txt
	libpng-config.in
	libpng.3
	libpng.pc.in
	libpngpf.3
	png.5
	pngbar.jpg
	pngbar.png
	pngnow.png
	pngtest.png
	projects
	scripts
	test-pngtest.sh
	)
DISTCHECK_IGNORE(libpng png_ignore_files)

# libregex Library
THIRD_PARTY_OPTION(REGEX regex)
THIRD_PARTY_SUBDIR(REGEX libregex)
BRLCAD_INCLUDE_FILE(regex.h HAVE_REGEX_H)
SET(regex_ignore_files
	cclass.h
	cname.h
	engine.c
	regex.h
	regex2.h
	utils.h
	)
DISTCHECK_IGNORE(libregex regex_ignore_files)


# libutahrle Library - The directory to perform ADD_SUBDIRECTORY on and the
# include directory for utahrle are different, so override the macro's setting
# of UTAHRLE_INCLUDE_DIR here.
THIRD_PARTY_OPTION(UTAHRLE utahrle)
THIRD_PARTY_SUBDIR(UTAHRLE libutahrle)
IF(BRLCAD_BUILD_LOCAL_UTAHRLE)
	SET(UTAHRLE_INCLUDE_DIR "${BRLCAD_SOURCE_DIR}/src/other/libutahrle/include" CACHE STRING "directory with rle.h header" FORCE)
	SET(BRLCAD_UTAHRLE_INCLUDE_DIR "${UTAHRLE_INCLUDE_DIR}" CACHE STRING "directory with rle.h header" FORCE)
	MARK_AS_ADVANCED(UTAHRLE_INCLUDE_DIR)
	MARK_AS_ADVANCED(BRLCAD_UTAHRLE_INCLUDE_DIR)
ENDIF(BRLCAD_BUILD_LOCAL_UTAHRLE)
SET(utahrle_ignore_files
	buildmap.3
	bwdithermap.3
	colorquant.3
	dither.3
	ditherbw.3
	dithergb.3
	dithermap.3
	float_to_exp.3
	getopt.c
	hilbert.3
	hilbert_c2i.3
	hilbert_i2c.3
	include
	inv_cmap.3
	librle.3
	libutahrle.3
	make_square.3
	rgb_to_bw.3
	rle.5
	rle_addhist.3
	rle_cp.3
	rle_debug.3
	rle_delcom.3
	rle_freeraw.3
	rle_get_error.3
	rle_get_set.3
	rle_get_setup.3
	rle_get_setup_ok.3
	rle_getcom.3
	rle_getraw.3
	rle_getrow.3
	rle_getskip.3
	rle_hdr.3
	rle_op.3
	rle_open_f.3
	rle_open_f_noexit.3
	rle_put_init.3
	rle_put_setup.3
	rle_putcom.3
	rle_puteof.3
	rle_putraw.3
	rle_putrow.3
	rle_raw_alloc.3
	rle_raw_free.3
	rle_rawtorow.3
	rle_row_alloc.3
	rle_row_free.3
	rle_skiprow.3
	scanargs.3
	CMake
	)
DISTCHECK_IGNORE(libutahrle utahrle_ignore_files)


# URTToolkit
# Programs, not a library - needs libutahrle, but the "check for system install" logic
# will be different here.  Ignore for now, revisit later.
ADD_SUBDIRECTORY(URToolkit)
SET(urtoolkit_ignore_files
	3.1-changes
	CHANGES
	MANIFEST
	README
	blurb
	copyright
	patchlevel.h
	)
DISTCHECK_IGNORE(URToolkit urtoolkit_ignore_files)
SET(urtoolkit_cnv_ignore_files
	README
	iristorle.c
	pgmtorle.c
	ppmtorle.c
	rastorle.c
	rla_header.h
	rlatorle.c
	rlb_header.h
	rletoabA62
	rletocgm.c
	rletogif
	rletoiris.c
	rletoppm.c
	rletorast.c
	rletotiff.c
	tifftorle.c
	rletoabA62/COPYRIGHT
	rletogif/rletogif.h
	)
DISTCHECK_IGNORE(URToolkit/cnv urtoolkit_cnv_ignore_files)
SET(urtoolkit_tools_ignore_files
	rleaddeof.c
	rleskel.c
	)
DISTCHECK_IGNORE(URToolkit/tools urtoolkit_tools_ignore_files)
SET(urtoolkit_clock_ignore_files
	README
	font.h
	font.src
	makeFont
	)
DISTCHECK_IGNORE(URToolkit/tools/clock urtoolkit_clock_ignore_files)


# Tcl/Tk presents a number of complexities for BRL-CAD and CMake.  BRL-CAD requires
# far more knowledge of the details of a Tcl/Tk installation than most programs,
# which unfortunately means the standard FindTCL.cmake is not sufficient. We have our
# own FindTCL routine which handles Tcl/Tk and its stubs, so trigger this using
# the THIRD_PARTY_OPTION for TCL.

# Set Tcl/Tk requirements for BRL-CAD so FindTCL.cmake knows what to reject
SET(TCL_PATH_NOMATCH_PATTERNS "/usr/brlcad;brlcad-install;${CMAKE_INSTALL_PREFIX}" CACHE STRING "Paths to avoid when looking for tcl/tk" FORCE)
SET(TCL_MIN_VERSION "8.5" CACHE STRING "Minimum acceptable Tcl/Tk version" FORCE)

IF(BRLCAD-ENABLE_TK)
	SET(TCL_REQUIRE_TK 1)
ELSE(BRLCAD-ENABLE_TK)
	SET(TCL_REQUIRE_TK 0)
ENDIF(BRLCAD-ENABLE_TK)

THIRD_PARTY_OPTION(TCL tcl)
IF(BRLCAD_BUILD_LOCAL_TCL AND TCL_REQUIRE_TK)
	OPTION(BRLCAD_BUILD_LOCAL_TK "Build the local versions of Tcl/Tk" ON)
	SET(BRLCAD_BUILD_LOCAL_TK ON CACHE BOOL "local tk build" FORCE)
ELSE(BRLCAD_BUILD_LOCAL_TCL AND TCL_REQUIRE_TK)
	OPTION(BRLCAD_BUILD_LOCAL_TK "Build the local versions of Tcl/Tk" OFF)
	SET(BRLCAD_BUILD_LOCAL_TK OFF CACHE BOOL "local tk build" FORCE)
ENDIF(BRLCAD_BUILD_LOCAL_TCL AND TCL_REQUIRE_TK)
MARK_AS_ADVANCED(BRLCAD_BUILD_LOCAL_TK)
IF(BRLCAD_BUILD_LOCAL_TCL OR BRLCAD-ENABLE_ALL_LOCAL_LIBS)
	ADD_SUBDIRECTORY(tcl)
   # Set the variables ourselves since FindTCL.cmake doesn't know about our build
	SET(TCL_LIBRARY tcl CACHE STRING "TCL_LIBRARY" FORCE)
	SET(TCL_LIBRARIES tcl CACHE STRING "TCL_LIBRARY" FORCE)
	SET(TCL_CONF_PREFIX "NONE-CMake" CACHE STRING "TCL_CONF_PREFIX" FORCE)
	SET(TCL_STUB_LIBRARY tclstub CACHE STRING "TCL_LIBRARY" FORCE)
	SET(TCL_STUB_LIBRARIES tclstub CACHE STRING "TCL_LIBRARY" FORCE)
	get_directory_property(TCL_INCLUDE_DIRS DIRECTORY tcl DEFINITION TCL_INCLUDE_DIRS)
	SET(TCL_INCLUDE_DIRS "${TCL_INCLUDE_DIRS}" CACHE STRING "Tcl include paths" FORCE)
	SET(TCL_INCLUDE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/tcl/generic" CACHE STRING "Tcl include path" FORCE)
	SET(TCL_TCLSH_EXECUTABLE tclsh CACHE STRING "Tcl shell" FORCE)
	SET(TCL_TCLSH tclsh CACHE STRING "Tcl shell" FORCE)
	get_directory_property(TCL_VERSION_MAJOR DIRECTORY tcl DEFINITION TCL_VERSION_MAJOR)
	SET(TCL_VERSION_MAJOR "${TCL_VERSION_MAJOR}" CACHE STRING "Tcl MAJOR version" FORCE)
	get_directory_property(TCL_VERSION_MINOR DIRECTORY tcl DEFINITION TCL_VERSION_MINOR)
	SET(TCL_VERSION_MINOR "${TCL_VERSION_MINOR}" CACHE STRING "Tcl MINOR version" FORCE)
	FILE(APPEND  ${CONFIG_H_FILE} "#define HAVE_TCL_H 1\n")
	FILE(APPEND ${CONFIG_H_FILE} "#define TCL_SYSTEM_INITTCL_PATH \"\"\n")
ELSE(BRLCAD_BUILD_LOCAL_TCL OR BRLCAD-ENABLE_ALL_LOCAL_LIBS)
	BRLCAD_INCLUDE_FILE(tcl.h HAVE_TCL_H)
	# We're going to need the path to the system init.tcl for btclsh and bwish
	SET(inittcl_script "
set filename \"${CMAKE_BINARY_DIR}/CMakeTmp/tcl_inittcl\"
set fileID [open $filename \"w\"]
puts $fileID $auto_path
close $fileID
exit
")
   SET(inittcl_scriptfile "${CMAKE_BINARY_DIR}/CMakeTmp/tcl_inittcl.tcl")
	FILE(WRITE ${inittcl_scriptfile} ${inittcl_script})
	EXEC_PROGRAM(${TCL_TCLSH_EXECUTABLE} ARGS ${inittcl_scriptfile} OUTPUT_VARIABLE EXECOUTPUT)
   FILE(READ ${CMAKE_BINARY_DIR}/CMakeTmp/tcl_inittcl tcl_inittcl_raw)
   STRING(REGEX REPLACE "\n" "" tcl_inittcl_paths_1 ${tcl_inittcl_raw})
   STRING(REGEX REPLACE " " ";" tcl_inittcl_paths ${tcl_inittcl_paths_1})
	FIND_PATH(tcl_inittcl NAMES init.tcl PATHS ${tcl_inittcl_paths})
	MARK_AS_ADVANCED(tcl_inittcl)
	FILE(APPEND ${CONFIG_H_FILE} "#define TCL_SYSTEM_INITTCL_PATH \"${tcl_inittcl}\"\n")
ENDIF(BRLCAD_BUILD_LOCAL_TCL OR BRLCAD-ENABLE_ALL_LOCAL_LIBS)

SET(tcl_ignore_files
	CMake
	ChangeLog
	ChangeLog.1999
	ChangeLog.2000
	ChangeLog.2001
	ChangeLog.2002
	ChangeLog.2003
	ChangeLog.2004
	ChangeLog.2005
	README
	changes
	compat
	generic/README
	generic/regc_color.c
	generic/regc_cvec.c
	generic/regc_lex.c
	generic/regc_locale.c
	generic/regc_nfa.c
	generic/regcustom.h
	generic/rege_dfa.c
	generic/regerrs.h
	generic/regextcl.h
	generic/regfronts.c
	generic/regguts.h
	generic/tcl.decls
	generic/tcl.h
	generic/tclCompile.h
	generic/tclDTrace.d
	generic/tclDecls.h
	generic/tclFileSystem.h
	generic/tclGetDate.y
	generic/tclIO.h
	generic/tclInt.decls
	generic/tclInt.h
	generic/tclIntDecls.h
	generic/tclIntPlatDecls.h
	generic/tclLoadNone.c
	generic/tclPlatDecls.h
	generic/tclPort.h
	generic/tclRegexp.h
	generic/tclTest.c
	generic/tclTestObj.c
	generic/tclTestProcBodyObj.c
	generic/tclThreadTest.c
	generic/tclTomMath.decls
	generic/tclTomMath.h
	generic/tclTomMathDecls.h
	generic/tclTomMathInt.h
	generic/tommath.h
	libtommath/tommath.h
	libtommath/tommath_class.h
	libtommath/tommath_superclass.h
	license.terms
	macosx
	macosx/GNUmakefile
	macosx/Tcl-Common.xcconfig
	macosx/Tcl-Debug.xcconfig
	macosx/Tcl-Info.plist.in
	macosx/Tcl-Release.xcconfig
	macosx/Tcl.pbproj
	macosx/Tcl.xcode
	macosx/Tcl.xcodeproj
	macosx/Tclsh-Info.plist.in
	macosx/configure
	macosx/configure.ac
	macosx/license.terms
	tests
	tools
	unix
	unix/Makefile.in
	unix/README
	unix/aclocal.m4
	unix/configure.in
	unix/dltest
	unix/install-sh
	unix/installManPage
	unix/ldAix
	unix/tcl.m4
	unix/tcl.spec
	unix/tclConfig.h.in
	unix/tclConfig.sh.in
	unix/tclLoadAix.c
	unix/tclLoadDl.c
	unix/tclLoadDyld.c
	unix/tclLoadNext.c
	unix/tclLoadOSF.c
	unix/tclLoadShl.c
	unix/tclUnixPort.h
	unix/tclUnixThrd.h
	unix/tclXtNotify.c
	unix/tclXtTest.c
	win
	)
DISTCHECK_IGNORE(tcl tcl_ignore_files)


IF(BRLCAD_BUILD_LOCAL_TK)
	ADD_SUBDIRECTORY(tk)
   # Set the variables ourselves since FindTCL.cmake doesn't know about our build
	SET(TCL_TK_LIBRARY tk CACHE STRING "TCL_TK_LIBRARY" FORCE)
	SET(TCL_TK_CONF_PREFIX "NONE-CMake" CACHE STRING "TCL_TK_CONF_PREFIX" FORCE)
	SET(TCL_LIBRARIES ${TCL_LIBRARIES} tk CACHE STRING "TCL_LIBRARIES" FORCE)
	SET(TK_LIBRARY tk CACHE STRING "TK_LIBRARY" FORCE)
	SET(TCL_STUB_LIBRARIES tkstub ${TCL_STUB_LIBRARIES} CACHE STRING "TCL_TK_STUBLIBRARIES" FORCE)
	SET(TCL_TK_STUB_LIBRARY tkstub CACHE STRING "TCL_TK_STUB_LIBRARY" FORCE)
	SET(TK_STUB_LIBRARY tkstub CACHE STRING "TK_STUB_LIBRARY" FORCE)
	SET(TK_INCLUDE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/tk/generic" CACHE STRING "Tk include path" FORCE)
	SET(TCL_INCLUDE_DIRS ${TCL_INCLUDE_DIRS} ${TK_INCLUDE_PATH} CACHE STRING "Tcl include paths" FORCE)
	IF(WIN32)
		SET(TCL_INCLUDE_DIRS ${TCL_INCLUDE_DIRS} ${CMAKE_CURRENT_SOURCE_DIR}/tk/win ${CMAKE_CURRENT_SOURCE_DIR}/tk/xlib CACHE STRING "Tcl include paths" FORCE)
	ENDIF(WIN32)
	SET(TCL_WISH_EXECUTABLE wish CACHE STRING "Tk shell" FORCE)
	SET(TK_WISH wish CACHE STRING "Tk shell" FORCE)
	IF(WIN32)
		SET(TK_SYSTEM_GRAPHICS "win32" CACHE STRING "Tk system graphics" FORCE)
	ELSEIF(APPLE)
		IF(OPENGL_USE_AQUA)
			SET(TK_SYSTEM_GRAPHICS "aqua" CACHE STRING "Tk system graphics" FORCE)
		ELSE(OPENGL_USE_AQUA)
			SET(TK_SYSTEM_GRAPHICS "x11" CACHE STRING "Tk system graphics" FORCE)
		ENDIF(OPENGL_USE_AQUA)
	ELSEIF(UNIX)
		SET(TK_SYSTEM_GRAPHICS "x11" CACHE STRING "Tk system graphics" FORCE)
	ENDIF(WIN32)
	FILE(APPEND  ${CONFIG_H_FILE} "#define HAVE_TK_H 1\n")
	ADD_DEPENDENCIES(tk tcl)
ELSE(BRLCAD_BUILD_LOCAL_TK)
	BRLCAD_INCLUDE_FILE(tk.h HAVE_TK_H)
ENDIF(BRLCAD_BUILD_LOCAL_TK)
MARK_AS_ADVANCED(TK_INCLUDE_PATH)
MARK_AS_ADVANCED(TK_LIBRARY)
MARK_AS_ADVANCED(TK_STUB_LIBRARY)
MARK_AS_ADVANCED(TK_SYSTEM_GRAPHICS)
MARK_AS_ADVANCED(TK_WISH)

SET(tk_ignore_files
	CMake
	ChangeLog
	ChangeLog.2002
	ChangeLog.2004
	README
	bitmaps
	changes
	compat
	generic/README
	generic/default.h
	generic/ks_names.h
	generic/prolog.ps
	generic/tk.decls
	generic/tk.h
	generic/tk3d.h
	generic/tkButton.h
	generic/tkCanvas.h
	generic/tkColor.h
	generic/tkDecls.h
	generic/tkEntry.h
	generic/tkFileFilter.c
	generic/tkFileFilter.h
	generic/tkFont.h
	generic/tkImgUtil.c
	generic/tkInt.decls
	generic/tkInt.h
	generic/tkIntDecls.h
	generic/tkIntPlatDecls.h
	generic/tkIntXlibDecls.h
	generic/tkMacWinMenu.c
	generic/tkMenu.h
	generic/tkMenubutton.h
	generic/tkOldTest.c
	generic/tkPlatDecls.h
	generic/tkPointer.c
	generic/tkPort.h
	generic/tkScale.h
	generic/tkScrollbar.h
	generic/tkSelect.h
	generic/tkSquare.c
	generic/tkTest.c
	generic/tkText.h
	generic/tkUndo.h
	generic/ttk/ttk.decls
	generic/ttk/ttkDecls.h
	generic/ttk/ttkGenStubs.tcl
	generic/ttk/ttkManager.h
	generic/ttk/ttkTheme.h
	generic/ttk/ttkThemeInt.h
	generic/ttk/ttkWidget.h
	license.terms
	macosx
	tests
	unix
	unix/Makefile.in
	unix/README
	unix/aclocal.m4
	unix/configure.in
	unix/install-sh
	unix/installManPage
	unix/license.terms
	unix/tcl.m4
	unix/tk.spec
	unix/tkConfig.h.in
	unix/tkConfig.sh.in
	unix/tkUnixDefault.h
	unix/tkUnixDialog.c
	unix/tkUnixInt.h
	unix/tkUnixPort.h
	unix/tkUnixRFont.c
	win
	xlib
	)
DISTCHECK_IGNORE(tk tk_ignore_files)

# Now that Tcl/Tk is settled, define the HAVE_TK flag for the config.h file
IF(TCL_TK_LIBRARY AND BRLCAD-ENABLE_TK)
	SET(HAVE_TK 1 CACHE STRING "C level Tk flag" FORCE)
	FILE(APPEND ${CONFIG_H_FILE} "#cmakedefine HAVE_TK\n")
ELSE(TCL_TK_LIBRARY AND BRLCAD-ENABLE_TK)
	SET(HAVE_TK 0 CACHE STRING "C level Tk flag" FORCE)
ENDIF(TCL_TK_LIBRARY AND BRLCAD-ENABLE_TK)
MARK_AS_ADVANCED(HAVE_TK)

# Depending on the Tcl/Tk results, look for packages required by BRL-CAD.  If
# building local Tcl/Tk, go ahead and enable all the packages as well - otherwise,
# check the found Tcl/Tk and enable what it doesn't supply.  If doing a non-graphical
# build, only compile extensions that don't use Tk.
INCLUDE(${BRLCAD_CMAKE_DIR}/FindTclPackage.cmake)
IF(NOT BRLCAD_BUILD_LOCAL_TCL)
	IF(TCL_TCLSH_EXECUTABLE)
		FIND_TCL_PACKAGE(${TCL_TCLSH_EXECUTABLE} Itcl)
	ENDIF(TCL_TCLSH_EXECUTABLE)
	IF(NOT BRLCAD_BUILD_LOCAL_TK)
		IF(TCL_WISH_EXECUTABLE)
			FIND_TCL_PACKAGE(${TCL_WISH_EXECUTABLE} Itk)
			FIND_TCL_PACKAGE(${TCL_WISH_EXECUTABLE} Iwidgets)
			FIND_TCL_PACKAGE(${TCL_WISH_EXECUTABLE} Tkhtml)
			FIND_TCL_PACKAGE(${TCL_WISH_EXECUTABLE} tkpng)
			FIND_TCL_PACKAGE(${TCL_WISH_EXECUTABLE} Tktable)
		ENDIF(TCL_WISH_EXECUTABLE)
	ENDIF(NOT BRLCAD_BUILD_LOCAL_TK)
ELSE(NOT BRLCAD_BUILD_LOCAL_TCL)
	OPTION(BRLCAD_BUILD_LOCAL_ITCL "Build the local ITCL package" ON)
	SET(BRLCAD_BUILD_LOCAL_ITCL ON CACHE BOOL "Build the local ITCL package" FORCE)
	IF(BRLCAD_BUILD_LOCAL_TK)
		OPTION(BRLCAD_BUILD_LOCAL_ITK "Build the local ITK package" ON)
		SET(BRLCAD_BUILD_LOCAL_ITK ON CACHE BOOL "Build the local ITK package" FORCE)
		OPTION(BRLCAD_BUILD_LOCAL_IWIDGETS "Build the local IWIDGETS package" ON)
		SET(BRLCAD_BUILD_LOCAL_IWIDGETS ON CACHE BOOL "Build the local IWIDGETS package" FORCE)
		OPTION(BRLCAD_BUILD_LOCAL_TKHTML "Build the local TKHTML package" ON)
		SET(BRLCAD_BUILD_LOCAL_TKHTML ON CACHE BOOL "Build the local TKHTML package" FORCE)
		OPTION(BRLCAD_BUILD_LOCAL_TKPNG "Build the local TKPNG package" ON)
		SET(BRLCAD_BUILD_LOCAL_TKPNG ON CACHE BOOL "Build the local TKPNG package" FORCE)
		OPTION(BRLCAD_BUILD_LOCAL_TKTABLE "Build the local TKTABLE package" ON)
		SET(BRLCAD_BUILD_LOCAL_TKTABLE ON CACHE BOOL "Build the local TKTABLE package" FORCE)
	ELSE(BRLCAD_BUILD_LOCAL_TK)
		OPTION(BRLCAD_BUILD_LOCAL_ITK "Build the local ITK package" OFF)
		SET(BRLCAD_BUILD_LOCAL_ITK OFF CACHE BOOL "Build the local ITK package" FORCE)
		OPTION(BRLCAD_BUILD_LOCAL_IWIDGETS "Build the local IWIDGETS package" OFF)
		SET(BRLCAD_BUILD_LOCAL_IWIDGETS OFF CACHE BOOL "Build the local IWIDGETS package" FORCE)
		OPTION(BRLCAD_BUILD_LOCAL_TKHTML "Build the local TKHTML package" OFF)
		SET(BRLCAD_BUILD_LOCAL_TKHTML OFF CACHE BOOL "Build the local TKHTML package" FORCE)
		OPTION(BRLCAD_BUILD_LOCAL_TKPNG "Build the local TKPNG package" OFF)
		SET(BRLCAD_BUILD_LOCAL_TKPNG OFF CACHE BOOL "Build the local TKPNG package" FORCE)
		OPTION(BRLCAD_BUILD_LOCAL_TKTABLE "Build the local TKTABLE package" OFF)
		SET(BRLCAD_BUILD_LOCAL_TKTABLE OFF CACHE BOOL "Build the local TKTABLE package" FORCE)
	ENDIF(BRLCAD_BUILD_LOCAL_TK)
ENDIF(NOT BRLCAD_BUILD_LOCAL_TCL)

# Handle Tcl/Tk packages
IF(BRLCAD_BUILD_LOCAL_ITCL)
	IF(NOT BRLCAD_BUILD_LOCAL_TCL)
		IF(WIN32)
			SET(TCL_INCLUDE_DIRS ${TCL_INCLUDE_DIRS} ${CMAKE_CURRENT_SOURCE_DIR}/tcl/generic ${CMAKE_CURRENT_SOURCE_DIR}/tcl/win ${CMAKE_CURRENT_SOURCE_DIR}/tcl/libtommath)
		ELSE(WIN32)
			SET(TCL_INCLUDE_DIRS ${TCL_INCLUDE_DIRS} ${CMAKE_CURRENT_SOURCE_DIR}/tcl/generic ${CMAKE_CURRENT_SOURCE_DIR}/tcl/unix ${CMAKE_CURRENT_SOURCE_DIR}/tcl/libtommath)
		ENDIF(WIN32)
	ENDIF(NOT BRLCAD_BUILD_LOCAL_TCL)
	ADD_SUBDIRECTORY(incrTcl/itcl)
	ADD_DEPENDENCIES(itcl tcl)
	SET(ITCL_LIBRARY itcl CACHE STRING "ITCL_LIBRARY" FORCE)
	MARK_AS_ADVANCED(ITCL_LIBRARY)
	SET(ITCL_STUB_LIBRARY itclstub CACHE STRING "ITCL_STUB_LIBRARY" FORCE)
	MARK_AS_ADVANCED(ITCL_STUB_LIBRARY)
	get_directory_property(ITCL_INCLUDE_DIRS DIRECTORY incrTcl/itcl DEFINITION ITCL_INCLUDE_DIRS)
	SET(ITCL_INCLUDE_DIRS "${ITCL_INCLUDE_DIRS}" CACHE STRING "Itcl include paths" FORCE)
	MARK_AS_ADVANCED(ITCL_INCLUDE_DIRS)
ENDIF(BRLCAD_BUILD_LOCAL_ITCL)
IF(BRLCAD_BUILD_LOCAL_ITCL)
	SET(ITCL_VERSION "3.4" CACHE STRING "ITCL_VERSION" FORCE)
ELSE(BRLCAD_BUILD_LOCAL_ITCL)
	SET(ITCL_VERSION "${ITCL_PACKAGE_VERSION}" CACHE STRING "ITCL_VERSION" FORCE)
ENDIF(BRLCAD_BUILD_LOCAL_ITCL)
FILE(APPEND ${CONFIG_H_FILE} "#cmakedefine ITCL_VERSION	\"${ITCL_VERSION}\"\n")
MARK_AS_ADVANCED(ITCL_VERSION)
MARK_AS_ADVANCED(BRLCAD_BUILD_LOCAL_ITCL)
SET(itcl_ignore_files
	CMake
	aclocal.m4
	doc
	generic/itcl.decls
	generic/itcl.h
	generic/itclDecls.h
	generic/itclInt.decls
	generic/itclInt.h
	generic/itclIntDecls.h
	itclConfig.sh.in
	library
	license.terms
	pkgIndex.tcl.in
	tests
	win
	)
DISTCHECK_IGNORE(incrTcl/itcl itcl_ignore_files)

IF(BRLCAD_BUILD_LOCAL_ITK)
	IF(NOT BRLCAD_BUILD_LOCAL_TK)
		IF(WIN32)
			SET(TK_INCLUDE_PATH ${TK_INCLUDE_PATH} ${CMAKE_CURRENT_SOURCE_DIR}/tk/generic ${CMAKE_CURRENT_SOURCE_DIR}/tk/bitmaps)
		ELSE(WIN32)
			SET(TK_INCLUDE_PATH ${TK_INCLUDE_PATH} ${CMAKE_CURRENT_SOURCE_DIR}/tk/generic ${CMAKE_CURRENT_SOURCE_DIR}/tk/win ${CMAKE_CURRENT_SOURCE_DIR}/tk/bitmaps)
		ENDIF(WIN32)
	ENDIF(NOT BRLCAD_BUILD_LOCAL_TK)
	ADD_SUBDIRECTORY(incrTcl/itk)
	ADD_DEPENDENCIES(itk itcl tcl tk)
	SET(ITK_LIBRARY itk CACHE STRING "ITK_LIBRARY" FORCE)
	MARK_AS_ADVANCED(ITK_LIBRARY)
	SET(ITK_VERSION "3.3" CACHE STRING "ITK_VERSION" FORCE)
ELSE(BRLCAD_BUILD_LOCAL_ITK)
   SET(ITK_VERSION "${ITCL_PACKAGE_VERSION}" CACHE STRING "ITK_VERSION" FORCE)
ENDIF(BRLCAD_BUILD_LOCAL_ITK)
#FILE(APPEND ${CONFIG_H_FILE} "#cmakedefine ITK_VERSION	\"${ITK_VERSION}\"\n")
MARK_AS_ADVANCED(ITK_VERSION)
MARK_AS_ADVANCED(BRLCAD_BUILD_LOCAL_ITK)
IF(BRLCAD_BUILD_LOCAL_ITCL OR BRLCAD_BUILD_LOCAL_ITK)
	SET(BRLCAD_BUILD_LOCAL_INCRTCL ON CACHE BOOL "incrtcl build status" FORCE)
ELSE(BRLCAD_BUILD_LOCAL_ITCL OR BRLCAD_BUILD_LOCAL_ITK)
	SET(BRLCAD_BUILD_LOCAL_INCRTCL OFF CACHE BOOL "incrtcl build status" FORCE)
ENDIF(BRLCAD_BUILD_LOCAL_ITCL OR BRLCAD_BUILD_LOCAL_ITK)
MARK_AS_ADVANCED(BRLCAD_BUILD_LOCAL_INCRTCL)
SET(itk_ignore_files
	CMake
	aclocal.m4
	demos
	doc
	examples
	generic/itk.decls
	generic/itk.h
	generic/itkDecls.h
	itkConfig.sh.in
	library
	license.terms
	pkgIndex.tcl.in
	tests
	win
	)
DISTCHECK_IGNORE(incrTcl/itk itk_ignore_files)


IF(BRLCAD_BUILD_LOCAL_IWIDGETS)
	ADD_SUBDIRECTORY(iwidgets)
	SET(IWIDGETS_VERSION "4.0.1" CACHE STRING "IWIDGETS_VERSION" FORCE)
ELSE(BRLCAD_BUILD_LOCAL_IWIDGETS)
   SET(IWIDGETS_VERSION "${IWIDGETS_PACKAGE_VERSION}" CACHE STRING "IWIDGETS_VERSION" FORCE)
ENDIF(BRLCAD_BUILD_LOCAL_IWIDGETS)
FILE(APPEND ${CONFIG_H_FILE} "#define IWIDGETS_VERSION \"${IWIDGETS_VERSION}\"\n")
MARK_AS_ADVANCED(IWIDGETS_VERSION)
MARK_AS_ADVANCED(BRLCAD_BUILD_LOCAL_IWIDGETS)
SET(iwidgets_ignore_files
	CHANGES
	ChangeLog
	README
	aclocal.m4
	configure.in
	demos
	doc
	incoming
	iwidgets.tcl.in
	license.terms
	outgoing
	pkgIndex.tcl
	pkgIndex.tcl.in
	tclconfig
	tests
	win
	)
DISTCHECK_IGNORE(iwidgets iwidgets_ignore_files)

IF(BRLCAD_BUILD_LOCAL_TKHTML)
        ADD_SUBDIRECTORY(tkhtml)
	IF(BRLCAD_BUILD_LOCAL_TCL)
	   ADD_DEPENDENCIES(Tkhtml tcl)
	ENDIF(BRLCAD_BUILD_LOCAL_TCL)
	IF(BRLCAD_BUILD_LOCAL_TK)
	   ADD_DEPENDENCIES(Tkhtml tk)
	ENDIF(BRLCAD_BUILD_LOCAL_TK)
ENDIF(BRLCAD_BUILD_LOCAL_TKHTML)
MARK_AS_ADVANCED(BRLCAD_BUILD_LOCAL_TKHTML)
SET(tkhtml_ignore_files
	AUTHORS
	COPYING
	ChangeLog
	INSTALL
	NEWS
	README
	acinclude.m4
	configure.ac
	doc
	src/css.h
	src/cssInt.h
	src/cssprop.tcl
	src/html.css
	src/html.h
	src/htmllayout.h
	src/htmlmacros.h
	src/htmlprop.h
	src/main.c
	src/mkdefaultstyle.tcl
	src/quirks.css
	src/restrack.h
	src/swproc.h
	src/tkhtml.tcl
	src/tokenlist.txt
	tclconfig
	CMake
	)
DISTCHECK_IGNORE(tkhtml tkhtml_ignore_files)

IF(BRLCAD_BUILD_LOCAL_TKPNG)
	ADD_SUBDIRECTORY(tkpng)
	IF(BRLCAD_BUILD_LOCAL_TCL)
	   ADD_DEPENDENCIES(tkpng tcl)
	ENDIF(BRLCAD_BUILD_LOCAL_TCL)
	IF(BRLCAD_BUILD_LOCAL_TK)
	   ADD_DEPENDENCIES(tkpng tk)
	ENDIF(BRLCAD_BUILD_LOCAL_TK)
ENDIF(BRLCAD_BUILD_LOCAL_TKPNG)
MARK_AS_ADVANCED(BRLCAD_BUILD_LOCAL_TKPNG)
SET(tkpng_ignore_files
	ChangeLog
	README
	license.terms
	pkgIndex.tcl.in
	CMake
	)
DISTCHECK_IGNORE(tkpng tkpng_ignore_files)

IF(BRLCAD_BUILD_LOCAL_TKTABLE)
	ADD_SUBDIRECTORY(tktable)
	IF(BRLCAD_BUILD_LOCAL_TCL)
	   ADD_DEPENDENCIES(Tktable tcl)
	ENDIF(BRLCAD_BUILD_LOCAL_TCL)
	IF(BRLCAD_BUILD_LOCAL_TK)
	   ADD_DEPENDENCIES(Tktable tk)
	ENDIF(BRLCAD_BUILD_LOCAL_TK)
ENDIF(BRLCAD_BUILD_LOCAL_TKTABLE)
MARK_AS_ADVANCED(BRLCAD_BUILD_LOCAL_TKTABLE)
SET(tktable_ignore_files
	ChangeLog
	Makefile.in
	README.blt
	README.txt
	TODO.txt
	aclocal.m4
	configure.in
	demos
	doc
	generic/tkAppInit.c
	generic/tkTable.h
	generic/tkTableInitScript.h
	generic/tkTablePs.c
	generic/version.h
	library
	license.txt
	mac
	misc
	tclconfig
	tests
	unix
	win
	CMake
	)
DISTCHECK_IGNORE(tktable tktable_ignore_files)

# OpenNURBS Library
THIRD_PARTY_OPTION(OPENNURBS openNURBS)
THIRD_PARTY_SUBDIR(OPENNURBS openNURBS)
IF(BRLCAD_BUILD_LOCAL_OPENNURBS)
	get_directory_property(OPENNURBS_HEADERS DIRECTORY openNURBS DEFINITION	OPENNURBS_HEADERS)
	SET(opennurbs_ignore_files
		${OPENNURBS_HEADERS}
		BRL-CAD_changes.txt
		example_brep
		example_dump
		example_gl
		example_read
		example_roundtrip
		example_userdata
		example_write
		faq.txt
		license.txt
		opennurbs.rc
		opennurbs.sln
		opennurbs.vcproj
		opennurbs_dll.cpp
		opennurbs_dll_resource.h
		opennurbs_gl.cpp
		opennurbs_memory_new.cpp
		opennurbs_precompiledheader.cpp
		opennurbs_staticlib.vcproj
		opennurbs_x.cpp
		opennurbs_zlib_memory.cpp
		readme.txt
		)
	DISTCHECK_IGNORE(openNURBS opennurbs_ignore_files)
ELSE(BRLCAD_BUILD_LOCAL_OPENNURBS)
	CMAKEFILES(openNURBS)
ENDIF(BRLCAD_BUILD_LOCAL_OPENNURBS)

# STEP Class Libraries
# If we don't have Lex/Yacc, we can't do STEP - conditionalize
IF(LEX_EXECUTABLE AND YACC_EXECUTABLE)
	THIRD_PARTY_OPTION(SCL scl)
	THIRD_PARTY_SUBDIR(SCL step)
	IF(BRLCAD_BUILD_LOCAL_SCL)
		SET(BRLCAD_SCL_INCLUDE_DIR "${BRLCAD_SOURCE_DIR}/src/other/step/include" CACHE STRING "Directory containing SCL headers" FORCE)
		MARK_AS_ADVANCED(BRLCAD_SCL_INCLUDE_DIR)
	ENDIF(BRLCAD_BUILD_LOCAL_SCL)
	SET(step_ignore_files
		AUTHORS
		CMake
		COPYING
		ChangeLog
		INSTALL
		NEWS
		README
		TODO
		config
		configure.ac
		m4
		misc
		src
		)
	DISTCHECK_IGNORE(step step_ignore_files)
	SET(step_express_ignore_files
		Changes
		README
		conf.h
		expparse.y
		expscan.l
		exptoks.h
		stack.h
		y.tok.h
		)
	DISTCHECK_IGNORE(step/src/express step_express_ignore_files)
	SET(step_exppp_ignore_files
		README
		)
	DISTCHECK_IGNORE(step/src/exppp step_exppp_ignore_files)
	SET(step_fedex_plus_ignore_files
		classes.h
		complexSupport.h
		)
	DISTCHECK_IGNORE(step/src/fedex_plus step_fedex_plus_ignore_files)
	SET(step_clstepcore_ignore_files
		ExpDict.h
		ExpDict.h.s
		README
		Registry.h
		STEPaggregate.h
		STEPattribute.h
		STEPattributeList.h
		STEPcomplex.h
		STEPundefined.h
		Select.h
		SingleLinkList.h
		baseType.h
		complexSupport.h
		dict-pic.txt
		dictdefs.h
		notes.txt
		read_func.h
		scl_osschema.h
		sdai.h
		sdaiApplication_instance.h
		sdaiBinary.h
		sdaiEnum.h
		sdaiSelect.h
		sdaiString.h
		)
	DISTCHECK_IGNORE(step/src/clstepcore step_clstepcore_ignore_files)
	SET(step_cleditor_ignore_files
		README
		STEPfile.h
		cmdmgr.h
		dispnode.h
		dispnodelist.h
		editordefines.h
		instmgr.h
		mgrnode.h
		mgrnodearray.h
		mgrnodelist.h
		needFunc.h
		s_HEADER_SCHEMA.h
		seeinfodefault.h
		)
	DISTCHECK_IGNORE(step/src/cleditor step_cleditor_ignore_files)
	SET(step_cldai_ignore_files
		ErrorMap.cc
		ErrorMap.hh
		ErrorRpt.cc
		ErrorRpt.hh
		StrUtil.cc
		StrUtil.hh
		impconv.hh
		imptypes.hh
		sdaiApplication_instance_set.h
		sdaiDaObject.h
		sdaiEntity_extent.h
		sdaiEntity_extent_set.h
		sdaiModel_contents.h
		sdaiModel_contents_list.h
		sdaiObject.h
		sdaiSession_instance.h
		)
	DISTCHECK_IGNORE(step/src/cldai step_cldai_ignore_files)
	SET(step_clutils_ignore_files
		README
		Str.h
		dirobj.h
		errordesc.h
		gennode.h
		gennodearray.h
		gennodelist.h
		scl_char_str_list.h
		scl_hash.h
		scl_string.h
		scldir.h
		stat.h
		str_list.h
		)
	DISTCHECK_IGNORE(step/src/clutils step_clutils_ignore_files)
ELSE(LEX_EXECUTABLE AND YACC_EXECUTABLE)
	DISTCHECK_IGNORE_ITEM(step)
ENDIF(LEX_EXECUTABLE AND YACC_EXECUTABLE)

# The jama/tnt headers are installed by default - BRL-CAD requires the altered headers
# to build (I think?)
ADD_SUBDIRECTORY(tnt)
SET(TNT_INCLUDE_DIR "${BRLCAD_SOURCE_DIR}/src/other/tnt" CACHE STRING "Directory containing tnt headers" FORCE)
MARK_AS_ADVANCED(TNT_INCLUDE_DIR)

# For now only turn on Togl for X11, but will need it everywhere eventually.  Will need
# some logic to both look for package require and the C library, as both APIs will be needed.
IF(BRLCAD-ENABLE_X11 AND BRLCAD-ENABLE_OPENGL)
   ADD_SUBDIRECTORY(togl)
	SET(togl_ignore_files
		CMake
		LICENSE
		README
		configure
		configure.in
		doc
		include
		tools
		)
	DISTCHECK_IGNORE(togl togl_ignore_files)
	SET(togl_src_ignore_files
		Togl.py
		Xmu/README
		Xmu/README.togl
		Xmu/StdCmap.h
		togl.decls
		toglAGL.c
		toglGLX.c
		toglNSOpenGL.c
		toglWGL.c
		)
	DISTCHECK_IGNORE(togl/src togl_src_ignore_files)
	SET(togl_demo_ignore_files
		ben.rgb
		double.c
		double.tcl
		gears.tcl
		image.h
		index.c
		index.tcl
		multisample.tcl
		overlay.c
		overlay.tcl
		pbuffer.c
		pbuffer.tcl
		stereo.c
		stereo.tcl
		texture.c
		texture.tcl
		tree2.rgba
		)
	DISTCHECK_IGNORE(togl/demo togl_demo_ignore_files)
ELSE(BRLCAD-ENABLE_X11 AND BRLCAD-ENABLE_OPENGL)
	DISTCHECK_IGNORE_ITEM(togl)
ENDIF(BRLCAD-ENABLE_X11 AND BRLCAD-ENABLE_OPENGL)

# jove editor - technically this isn't a library,
# but traditionally it's grouped into the "enable-all"
# logic. Almost certainly won't build on Windows, and
# it's not worth trying to make it do so.  Defaults to
# on if BRLCAD-ENABLE_ALL_LOCAL_LIBS is enabled before
# the first run of CMake, otherwise defaults to off.
#IF(BRLCAD-ENABLE_JOVE)
#	IF(NOT WIN32)
#		ADD_SUBDIRECTORY(jove)
#		SET(jove_ignore_files
#			BUGS
#			README
#			describe.com
#			jove-tutorial
#			jove.1
#			jove.emacs
#			jove.h
#			jove_temp.h
#			teach-jove.in
#			termcap.h
#			)
#		DISTCHECK_IGNORE(jove jove_ignore_files)
#	ELSE(NOT WIN32)
#		MESSAGE("jove was requested, but does not build on Windows - disabling")
#		SET(BRLCAD-ENABLE_JOVE OFF CACHE BOOL "jove disabled on WIN32" FORCE)
#		DISTCHECK_IGNORE_ITEM(jove)
#	ENDIF(NOT WIN32)
#ELSE(BRLCAD-ENABLE_JOVE)
	DISTCHECK_IGNORE_ITEM(jove)
#ENDIF(BRLCAD-ENABLE_JOVE)
