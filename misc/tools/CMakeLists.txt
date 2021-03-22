#---------------------------------------------------------------------------------
# Local Copies of Tools
#---------------------------------------------------------------------------------

# Because we sometimes need to override system versions of tools, it
# is essential that the variables containing the executable to be run
# for each of the following tools use full paths - if just the name of
# the target is used, there is a chance that name is the same as the
# executable name itself.  In that case, if the system executable is
# in the path, it may get called instead of resolving to the compiled
# exectuable associated with the CMake target.  (Information from the
# CMake devs indicates this is not expected behavior, but at some
# point during the development efforts a problem apparently related to
# this came up.)  On single configuration systems we can reference
# specify the full path to the binary directory at CMake configure
# time, but this isn't possible for multi-configuration systems like
# Visual Studio since that path changes at run-time.  For those
# situations, CMake provides a variable CMAKE_CFG_INTDIR that will
# insert the correct logic for config-dependent paths.

# Clear all pre-defined CMake flags
CLEAR_BUILD_FLAGS()

# Restore CMake defaults, in case they are needed
RESTORE_CACHED_BUILD_FLAGS(_CMAKE_DEFAULT)

# For misc/tools, be liberal about standards
if(NOT MSVC)
  set(CMAKE_C_EXTENSIONS ON)
  set(CMAKE_C_STANDARD 99)
  set(CMAKE_C_STANDARD_REQUIRED OFF)
endif(NOT MSVC)

# Quiet all warnings in this directory
DISABLE_WARNINGS()

# Most third party items have a list calling out files for distcheck -
# these are stored in files in the dlists directory.  Ignore that
# directory for distcheck
file(GLOB dlists "*.dist")
foreach(ITEM ${dlists})
  get_filename_component(dlist ${ITEM} NAME)
  CMAKEFILES(${dlist})
endforeach(ITEM ${dlists})

# For those wanting to use a system version of the LEMON parser
# generator from sqlite, remember that the presence of /usr/bin/lemon
# is not enough.  LEMON needs a template file, lempar.c, and by
# default it needs it in the same directory as /usr/bin/lemon.  The
# typical approach to avoiding that requirement is to patch lemon,
# like this Gentoo ebuild:
#
# http://gentoo-overlays.zugaina.org/gentoo-zh/portage/dev-util/lemon/
#
# LEMON packages for other major Linux/BSD distros will do the same.
# BRL-CAD's FindLEMON.cmake macros will look for the template file in
# the executable directory first, and if not there will check in
# /usr/share/lemon (the location used by several distributions.)  If
# your distribution has a working lemon with the lempar.c template
# file in a custom location, specify the full path to the template
# with the variable LEMON_TEMPLATE - something like:
#
# cmake .. -DLEMON_TEMPLATE=/etc/lemon/lempar.c
#
# This is not to tell LEMON what template to use - that information is
# usually hardcoded in LEMON itself - but to let FindLEMON.cmake know
# there is a working LEMON installation.
set(LEMON_SKIP_INSTALL 1)
set(lemon_DESCRIPTION "
Option for enabling and disabling compilation of the lemon parser
provided with BRL-CAD's source distribution.  Default is AUTO,
responsive to the toplevel BRLCAD_BUNDLED_LIBS option and testing
first for a system version if BRLCAD_BUNDLED_LIBS is also AUTO.
")
THIRD_PARTY_EXECUTABLE(lemon LEMON lemon REQUIRED "BRLCAD_LEVEL2" ALIASES "ENABLE_LEMON" DESCRIPTION lemon_DESCRIPTION)
if("${BRLCAD_BUNDLED_LIBS}" STREQUAL "BUNDLED" OR "${BRLCAD_LEMON}" MATCHES "BUNDLED")
  set(LEMON_TEMPLATE "${BRLCAD_SOURCE_DIR}/misc/tools/lemon/lempar.c" CACHE STRING "Lemon template file" FORCE)
  set(LEMON_EXECUTABLE_TARGET "lemon" CACHE STRING "lemon target" FORCE)
endif("${BRLCAD_BUNDLED_LIBS}" STREQUAL "BUNDLED" OR "${BRLCAD_LEMON}" MATCHES "BUNDLED")
DISTCLEAN(${CMAKE_CURRENT_BINARY_DIR}/lemon/Makefile)
# Make sure we load FindLEMON.cmake to be able to define LEMON targets.
include(${BRLCAD_CMAKE_DIR}/FindLEMON.cmake)
SetTargetFolder(lemon "Compilation Utilities")
SetTargetFolder(lempar_cp "Compilation Utilities")

set(RE2C_SKIP_INSTALL 1)
set(re2c_DESCRIPTION "
Option for enabling and disabling compilation of the re2c scanner
utility provided with BRL-CAD's source distribution.  Default is AUTO,
responsive to the toplevel BRLCAD_BUNDLED_LIBS option and testing
first for a system version if BRLCAD_BUNDLED_LIBS is also AUTO.
")
THIRD_PARTY_EXECUTABLE(re2c RE2C re2c REQUIRED "BRLCAD_LEVEL2" ALIASES "ENABLE_RE2C" DESCRIPTION re2c_DESCRIPTION)
DISTCLEAN(${CMAKE_CURRENT_BINARY_DIR}/re2c/Makefile)
DISTCLEAN(${CMAKE_CURRENT_BINARY_DIR}/re2c/re2cParse_parser/parser.yy)
DISTCLEAN(${CMAKE_CURRENT_BINARY_DIR}/re2c/re2cParse_bootstrap_parser/parser.yy)
SetTargetFolder(re2c "Compilation Utilities")
SetTargetFolder(re2c_bootstrap "Compilation Utilities")
# Make sure we load FindRE2C.cmake to be able to define RE2C targets.
include(${BRLCAD_CMAKE_DIR}/FindRE2C.cmake)

# Perplex is set up as a misc/tools sub-build, but at this time it is
# entirely a BRL-CAD effort and not distributed or maintained as its
# own project.

# For all BRL-CAD projects, we will use the perplex template in
# misc/tools/perplex
set(PERPLEX_TEMPLATE "${CMAKE_CURRENT_SOURCE_DIR}/perplex/perplex_template.c" CACHE STRING "Perplex template file" FORCE)
mark_as_advanced(PERPLEX_TEMPLATE)

set(PERPLEX_SKIP_INSTALL 1)
set(perplex_DESCRIPTION "
Option for enabling and disabling compilation of the perplex scanner
generator provided with BRL-CAD's source distribution.  Default is
AUTO, responsive to the toplevel BRLCAD_BUNDLED_LIBS option and
testing first for a system version if BRLCAD_BUNDLED_LIBS is also
AUTO.  perplex requires a working re2c.
")
THIRD_PARTY_EXECUTABLE(perplex PERPLEX perplex REQUIRED "BRLCAD_LEVEL2" ALIASES "ENABLE_PERPLEX" DESCRIPTION perplex_DESCRIPTION NOSYS)
# Make sure we load FindPERPLEX.cmake to be able to define PERPLEX targets.
include(${BRLCAD_CMAKE_DIR}/FindPERPLEX.cmake)

DISTCLEAN(${CMAKE_CURRENT_BINARY_DIR}/perplex/Makefile)
DISTCLEAN(${CMAKE_CURRENT_BINARY_DIR}/perplex/PERPLEX_parser/parser.y)
SetTargetFolder(perplex "Compilation Utilities")
SetTargetFolder(perplex_template_cp "Compilation Utilities")


# To enforce style guidelines rigorously, BRL-CAD bundles the
# Artistic Style code formatter.  This tool is LGPLv3 licensed
# and its code can *not* be used in BRL-CAD itself - it
# is present striclty and only to be used as a tool in the
# compilation process.
set(astyle_DESCRIPTION "
Option for enabling and disabling compilation of the Artistic Style
(astyle) utility provided with BRL-CAD's source distribution.  Default is
AUTO,
responsive to the toplevel BRLCAD_BUNDLED_LIBS option and testing
first for a system version if BRLCAD_BUNDLED_LIBS is also AUTO.
")
THIRD_PARTY_EXECUTABLE(astyle ASTYLE astyle REQUIRED "BRLCAD_LEVEL2" ALIASES "ENABLE_ASTYLE" DESCRIPTION astyle_DESCRIPTION)
DISTCLEAN(${CMAKE_CURRENT_BINARY_DIR}/astyle/Makefile)

# dom2dox - Doc-O-Matic to Doxygen C++ source converter
include(${CMAKE_CURRENT_SOURCE_DIR}/dom2dox.dist)
CMAKEFILES_IN_DIR(dom2dox_ignore_files dom2dox)

# svn2cl - Make a GNU-style ChangeLog from Subversion logs
include(${CMAKE_CURRENT_SOURCE_DIR}/svn2cl.dist)
CMAKEFILES_IN_DIR(svn2cl_ignore_files svn2cl)

# libxml and libxslt tools for DocBook processing.  We build just
# enough library support to get these two tools working.
set(xsltproc_DESCRIPTION "
Option for enabling and disabling compilation of the xsltproc XML
transformation utility provided with BRL-CAD's source distribution.
Used for DocBook documentation processing.  Default is AUTO,
responsive to the toplevel BRLCAD_BUNDLED_LIBS option and testing
first for a system version if BRLCAD_BUNDLED_LIBS is also AUTO.
Depends on BRLCAD_EXTRADOCS.
")
THIRD_PARTY_EXECUTABLE(xmltools XSLTPROC xsltproc REQUIRED "BRLCAD_EXTRADOCS;BRLCAD_LEVEL3" ALIASES "ENABLE_XSLTPROC" DESCRIPTION xsltproc_DESCRIPTION)
SetTargetFolder(xsltproc "Compilation Utilities")

# Used only when validating XML for schema compliance
# (BRLCAD_EXTRADOCS_VALIDATE must be on to use this, although it is compiled
# regardless if the xmltools subdirectory build is added.)
set(xmllint_DESCRIPTION "
Option for enabling and disabling compilation of the xmllint XML
validation utility provided with BRL-CAD's source distribution.  Used
for DocBook documentation validation.  Default is AUTO, responsive to
the toplevel BRLCAD_BUNDLED_LIBS option and testing first for a system
version if BRLCAD_BUNDLED_LIBS is also AUTO.
")
THIRD_PARTY_EXECUTABLE(xmltools XMLLINT xmllint REQUIRED "BRLCAD_EXTRADOCS_VALIDATE;BRLCAD_LEVEL3" ALIASES "ENABLE_XMLLINT" DESCRIPTION xmllint_DESCRIPTION)
DISTCLEAN(${CMAKE_CURRENT_BINARY_DIR}/xmltools/include)
SetTargetFolder(xmllint "Compilation Utilities")

SetTargetFolder(xml "Third Party Libraries")
SetTargetFolder(xslt "Third Party Libraries")
SetTargetFolder(exslt "Third Party Libraries")

# env2c - Scanner to generate a list of environment variables used by
# source code
THIRD_PARTY_EXECUTABLE(env2c ENV2C env2c REQUIRED "BRLCAD_LEVEL2" NOSYS)
SetTargetFolder(env2c "Compilation Utilities")
DISTCLEAN(${CMAKE_CURRENT_BINARY_DIR}/env2c/Makefile)

# debug2c - Scanner to generate a list of debug variables used by
# source code
THIRD_PARTY_EXECUTABLE(debug2c DEBUG2C debug2c REQUIRED "BRLCAD_LEVEL2" NOSYS)
SetTargetFolder(debug2c "Compilation Utilities")
DISTCLEAN(${CMAKE_CURRENT_BINARY_DIR}/debug2c/Makefile)

# inactvhdrs - utility that tries to find local headers unused by other
# source code in a file hierarchy
CMAKEFILES(inactvhdrs/inactvhdrs.cpp)

CMAKEFILES(README)
CMAKEFILES(CMakeLists.txt)

# Locac Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
