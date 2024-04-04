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

include("${CMAKE_CURRENT_SOURCE_DIR}/Third_Party_Exec.cmake")

function(SetTargetFolder targetname folder)
  if(TARGET ${targetname})
    set_target_properties(${targetname} PROPERTIES FOLDER "${folder}")
  endif(TARGET ${targetname})
endfunction(SetTargetFolder)

# Clear all pre-defined CMake flags
CLEAR_BUILD_FLAGS()

# Restore CMake defaults, in case they are needed
RESTORE_CACHED_BUILD_FLAGS(_CMAKE_DEFAULT)

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

# patchelf (https://github.com/NixOS/patchelf) is used to set/update RPATH
# values on certain platforms.  Build a copy if we don't already have it and
# we're not on Windows or Mac.  If we're going to use it, it needs to be
# defined before we start setting up the external targets.
#
# Notes - patchelf is GPL, so we cannot combine it or use it with any of the
# BRL-CAD distributed libraries.  It is bundled STRICTLY as a build time
# convenience for platforms that might not have it installed by default - a
# system version is fine, even preferred.  As of the time of this writing (2022)
# patchelf seems to be the best way to do the necessary RPATH manipulation for
# this third party management approach to work.

# This copy of patchelf is not quite the newest version on github, as the newer
# code needs newer C++ standards than BRL-CAD currently requires - see
# UPGRADE_NOTE.txt in the patchelf directory.
find_program(PATCHELF_EXECUTABLE patchelf)
if (NOT PATCHELF_EXECUTABLE)
  add_subdirectory(patchelf)
endif (NOT PATCHELF_EXECUTABLE)
set(PATCHELF_FILES
  patchelf/CMakeLists.txt
  patchelf/COPYING
  patchelf/patchelf.cc
  patchelf/elf.h
  patchelf/UPGRADE_NOTE.txt
  )
mark_as_advanced(PATCHELF_EXECUTABLE)
CMAKEFILES(${PATCHELF_FILES})


###############################################################################
# BRL-CAD and some of its dependencies require the lemon, re2c and perplex
# tools for compilation.  They are not installed, but must be available.  We
# ensure they can be found by building them up front if not present on the
# system.
#
# Since perplex is not part of any third party packaging system (as of 2023)
# and it uses a customized re2c, we build perplex directly rather than adding
# the Third Party management complexity to the process.  However, as it is not
# installed with BRL-CAD and uses 3rd party tools that (in principle) we would
# someday like to support in vanilla form, it is currently maintained in ext
###############################################################################
set(PERPLEX_SKIP_INSTALL ON)
add_subdirectory(perplex)
set(LEMON_TEMPLATE "${CMAKE_CURRENT_SOURCE_DIR}/perplex/lempar.c" CACHE PATH "lemon template" FORCE)
set(PERPLEX_TEMPLATE "${CMAKE_CURRENT_SOURCE_DIR}/perplex/perplex/perplex_template.c" CACHE PATH "perplex template" FORCE)
set(LEMON_EXECUTABLE lemon CACHE STRING "lemon" FORCE)
set(RE2C_EXECUTABLE re2c CACHE STRING "re2c" FORCE)
set(PERPLEX_EXECUTABLE perplex CACHE STRING "perplex" FORCE)
mark_as_advanced(LEMON_TEMPLATE)
mark_as_advanced(PERPLEX_TEMPLATE)
mark_as_advanced(LEMON_EXECUTABLE)
mark_as_advanced(RE2C_EXECUTABLE)
mark_as_advanced(PERPLEX_EXECUTABLE)
SetTargetFolder(perplex "Compilation Utilities")
SetTargetFolder(lemon "Compilation Utilities")
SetTargetFolder(re2c "Compilation Utilities")
SetTargetFolder(re2c_bootstrap "Compilation Utilities")
include("${CMAKE_CURRENT_SOURCE_DIR}/perplex.dist")
CMAKEFILES_IN_DIR(perplex_ignore_files perplex)
DISTCLEAN(
  ${CMAKE_CURRENT_BINARY_DIR}/perplex/config.h
  ${CMAKE_CURRENT_BINARY_DIR}/perplex/bootstrap_parser/parser.yy
  ${CMAKE_CURRENT_BINARY_DIR}/perplex/perplex_wd/parser.y
  ${CMAKE_CURRENT_BINARY_DIR}/perplex/re2c_parser/parser.yy
  )

CMAKEFILES(
  CMakeLists.txt
  README
  Third_Party_Exec.cmake
  )

# Locac Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
