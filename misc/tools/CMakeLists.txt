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

# Quiet all warnings in this directory
if(BRLCAD_DISABLE_SRC_OTHER_WARN)
  add_definitions(-w)
endif(BRLCAD_DISABLE_SRC_OTHER_WARN)

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
set(lemon_ALIASES ENABLE_LEMON)
set(lemon_DESCRIPTION "
Option for enabling and disabling compilation of the lemon parser
provided with BRL-CAD's source distribution.  Default is AUTO,
responsive to the toplevel BRLCAD_BUNDLED_LIBS option and testing
first for a system version if BRLCAD_BUNDLED_LIBS is also AUTO.
")
if("${BRLCAD_BUNDLED_LIBS}" STREQUAL "BUNDLED" OR "${BRLCAD_LEMON}" MATCHES "BUNDLED")
  set(LEMON_TEMPLATE "${BRLCAD_SOURCE_DIR}/misc/tools/lemon/lempar.c")
endif("${BRLCAD_BUNDLED_LIBS}" STREQUAL "BUNDLED" OR "${BRLCAD_LEMON}" MATCHES "BUNDLED")
THIRD_PARTY_EXECUTABLE(lemon lemon "BRLCAD_LEVEL2" lemon_ALIASES lemon_DESCRIPTION)
DISTCLEAN(${CMAKE_CURRENT_BINARY_DIR}/lemon/Makefile)
# Unless some parent logic has already defined the macro, we need to
# load LEMON_Util to be able to define LEMON targets.
if(NOT COMMAND LEMON_TARGET)
  include(${BRLCAD_CMAKE_DIR}/LEMON_Util.cmake)
endif(NOT COMMAND LEMON_TARGET)
SetTargetFolder(lemon "Compilation Utilities")
SetTargetFolder(lempar_cp "Compilation Utilities")

set(re2c_ALIASES ENABLE_RE2C)
set(re2c_DESCRIPTION "
Option for enabling and disabling compilation of the re2c scanner
utility provided with BRL-CAD's source distribution.  Default is AUTO,
responsive to the toplevel BRLCAD_BUNDLED_LIBS option and testing
first for a system version if BRLCAD_BUNDLED_LIBS is also AUTO.
")
THIRD_PARTY_EXECUTABLE(re2c re2c "BRLCAD_LEVEL2" re2c_ALIASES re2c_DESCRIPTION)
DISTCLEAN(${CMAKE_CURRENT_BINARY_DIR}/re2c/Makefile)
SetTargetFolder(re2c "Compilation Utilities")
SetTargetFolder(re2c_bootstrap "Compilation Utilities")

# Unless some parent logic has already defined the macro, we need to
# load RE2C_Util to be able to define RE2C targets.

if(NOT COMMAND RE2C_TARGET)
  include(${BRLCAD_SOURCE_DIR}/misc/CMake/RE2C_Util.cmake)
endif(NOT COMMAND RE2C_TARGET)

# Perplex is set up as a misc/tools sub-build, but at this time it is
# entirely a BRL-CAD effort and not distributed or maintained as its
# own project.

# For all BRL-CAD projects, we will use the perplex template in
# misc/tools/perplex
set(PERPLEX_TEMPLATE "${CMAKE_CURRENT_SOURCE_DIR}/perplex/perplex_template.c" CACHE STRING "Perplex template file" FORCE)
mark_as_advanced(PERPLEX_TEMPLATE)

set(perplex_ALIASES ENABLE_PERPLEX)
set(perplex_DESCRIPTION "
Option for enabling and disabling compilation of the perplex scanner
generator provided with BRL-CAD's source distribution.  Default is
AUTO, responsive to the toplevel BRLCAD_BUNDLED_LIBS option and
testing first for a system version if BRLCAD_BUNDLED_LIBS is also
AUTO.  perplex requires a working re2c.
")
THIRD_PARTY_EXECUTABLE(perplex perplex "BRLCAD_LEVEL2" perplex_ALIASES perplex_DESCRIPTION NOSYS)
# Unless some parent logic has already defined the macro, we need to
# load PERPLEX_Util to be able to define PERPLEX targets.
if(NOT COMMAND PERPLEX_TARGET)
  include(${BRLCAD_SOURCE_DIR}/misc/CMake/PERPLEX_Util.cmake)
endif(NOT COMMAND PERPLEX_TARGET)
DISTCLEAN(${CMAKE_CURRENT_BINARY_DIR}/perplex/Makefile)
SetTargetFolder(perplex "Compilation Utilities")
SetTargetFolder(perplex_template_cp "Compilation Utilities")


# To enforce style guidelines rigorously, BRL-CAD bundles the
# Artistic Style code formatter.  This tool is LGPLv3 licensed
# and its code can *not* be used in BRL-CAD itself - it
# is present striclty and only to be used as a tool in the
# compilation process.
set(astyle_ALIASES ENABLE_ASTYLE)
set(astyle_DESCRIPTION "
Option for enabling and disabling compilation of the Artistic Style
(astyle) utility provided with BRL-CAD's source distribution.  Default is
AUTO,
responsive to the toplevel BRLCAD_BUNDLED_LIBS option and testing
first for a system version if BRLCAD_BUNDLED_LIBS is also AUTO.
")
THIRD_PARTY_EXECUTABLE(astyle astyle "BRLCAD_LEVEL2" astyle_ALIASES astyle_DESCRIPTION)
DISTCLEAN(${CMAKE_CURRENT_BINARY_DIR}/astyle/Makefile)

CMAKEFILES(astyle.dist)

# dom2dox - Doc-O-Matic to Doxygen C++ source converter
include(${CMAKE_CURRENT_SOURCE_DIR}/dom2dox.dist)
CMAKEFILES_IN_DIR(dom2dox_ignore_files dom2dox)

# libxml and libxslt tools for DocBook processing.  We build just
# enough library support to get these two tools working.
set(xsltproc_ALIASES ENABLE_XSLTPROC)
set(xsltproc_DESCRIPTION "
Option for enabling and disabling compilation of the xsltproc XML
transformation utility provided with BRL-CAD's source distribution.
Used for DocBook documentation processing.  Default is AUTO,
responsive to the toplevel BRLCAD_BUNDLED_LIBS option and testing
first for a system version if BRLCAD_BUNDLED_LIBS is also AUTO.
Depends on BRLCAD_EXTRADOCS.
")
THIRD_PARTY_EXECUTABLE(xsltproc xmltools "BRLCAD_EXTRADOCS;BRLCAD_LEVEL3" xsltproc_ALIASES xsltproc_DESCRIPTION)
SetTargetFolder(xsltproc "Compilation Utilities")

# Used only when validating XML for schema compliance
# (BRLCAD_EXTRADOCS_VALIDATE must be on to use this, although it is compiled
# regardless if the xmltools subdirectory build is added.)
set(xmllint_ALIASES ENABLE_XMLLINT)
set(xmllint_DESCRIPTION "
Option for enabling and disabling compilation of the xmllint XML
validation utility provided with BRL-CAD's source distribution.  Used
for DocBook documentation validation.  Default is AUTO, responsive to
the toplevel BRLCAD_BUNDLED_LIBS option and testing first for a system
version if BRLCAD_BUNDLED_LIBS is also AUTO.
")
THIRD_PARTY_EXECUTABLE(xmllint xmltools "BRLCAD_EXTRADOCS_VALIDATE;BRLCAD_LEVEL3" xmllint_ALIASES xmllint_DESCRIPTION)
DISTCLEAN(${CMAKE_CURRENT_BINARY_DIR}/xmltools/include)
SetTargetFolder(xmllint "Compilation Utilities")

SetTargetFolder(xml "Third Party Libraries")
SetTargetFolder(xslt "Third Party Libraries")
SetTargetFolder(exslt "Third Party Libraries")

# Locac Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
