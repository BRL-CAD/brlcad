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
set(lemon_DESCRIPTION "
Option for enabling and disabling compilation of the lemon parser
provided with BRL-CAD's source distribution.  Default is AUTO,
responsive to the toplevel BRLCAD_BUNDLED_LIBS option and testing
first for a system version if BRLCAD_BUNDLED_LIBS is also AUTO.
")
THIRD_PARTY_EXECUTABLE(lemon LEMON lemon REQUIRED "BRLCAD_LEVEL2" ALIASES "ENABLE_LEMON" DESCRIPTION lemon_DESCRIPTION)
if (${CMAKE_PROJECT_NAME}_LEMON_BUILD)

  ExternalProject_Add(LEMON_BLD
    SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../misc/tools/lemon"
    BUILD_ALWAYS ${EXTERNAL_BUILD_UPDATE} ${LOG_OPTS}
    CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${CMAKE_BINARY_DIR} -DBIN_DIR=${BIN_DIR}
    -DCMAKE_INSTALL_RPATH=${CMAKE_BUILD_RPATH}
    )

  list(APPEND BRLCAD_DEPS LEMON_BLD)

  set(LEMON_TEMPLATE "${CMAKE_SOURCE_DIR}/misc/tools/lemon/lempar.c" CACHE STRING "Lemon template file" FORCE)
  SetTargetFolder(LEMON_BLD "Third Party Libraries")
endif (${CMAKE_PROJECT_NAME}_LEMON_BUILD)

set(re2c_DESCRIPTION "
Option for enabling and disabling compilation of the re2c scanner
utility provided with BRL-CAD's source distribution.  Default is AUTO,
responsive to the toplevel BRLCAD_BUNDLED_LIBS option and testing
first for a system version if BRLCAD_BUNDLED_LIBS is also AUTO.
")
THIRD_PARTY_EXECUTABLE(re2c RE2C re2c REQUIRED "BRLCAD_LEVEL2" ALIASES "ENABLE_RE2C" DESCRIPTION re2c_DESCRIPTION)
if (${CMAKE_PROJECT_NAME}_RE2C_BUILD)

  if (TARGET LEMON_BLD)
    set(LEMON_TARGET LEMON_BLD)
    list(APPEND RE2C_DEPS LEMON_BLD)
  endif (TARGET LEMON_BLD)

  ExternalProject_Add(RE2C_BLD
    SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../misc/tools/re2c"
    BUILD_ALWAYS ${EXTERNAL_BUILD_UPDATE} ${LOG_OPTS}
    CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${CMAKE_BINARY_DIR} -DBIN_DIR=${BIN_DIR}
    -DCMAKE_INSTALL_RPATH=${CMAKE_BUILD_RPATH}
    -DLEMON_ROOT=$<$<BOOL:${LEMON_TARGET}>:${CMAKE_BINARY_DIR}> -DLEMON_TEMPLATE=$<$<BOOL:${LEMON_TARGET}>:${LEMON_TEMPLATE}>
    DEPENDS ${RE2C_DEPS}
    )

  list(APPEND BRLCAD_DEPS RE2C_BLD)

  SetTargetFolder(RE2C_BLD "Third Party Libraries")
endif (${CMAKE_PROJECT_NAME}_RE2C_BUILD)

set(perplex_DESCRIPTION "
Option for enabling and disabling compilation of the perplex scanner
generator provided with BRL-CAD's source distribution.  Default is
AUTO, responsive to the toplevel BRLCAD_BUNDLED_LIBS option and
testing first for a system version if BRLCAD_BUNDLED_LIBS is also
AUTO.  perplex requires a working re2c.
")
THIRD_PARTY_EXECUTABLE(perplex PERPLEX perplex REQUIRED "BRLCAD_LEVEL2" ALIASES "ENABLE_PERPLEX" DESCRIPTION perplex_DESCRIPTION NOSYS)
if (${CMAKE_PROJECT_NAME}_PERPLEX_BUILD)

  if (TARGET LEMON_BLD)
    set(LEMON_TARGET LEMON_BLD)
    list(APPEND PERPLEX_DEPS LEMON_BLD)
  endif (TARGET LEMON_BLD)

  if (TARGET RE2C_BLD)
    set(RE2C_TARGET RE2C_BLD)
    list(APPEND PERPLEX_DEPS RE2C_BLD)
  endif (TARGET RE2C_BLD)

  ExternalProject_Add(PERPLEX_BLD
    SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../misc/tools/perplex"
    BUILD_ALWAYS ${EXTERNAL_BUILD_UPDATE} ${LOG_OPTS}
    CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${CMAKE_BINARY_DIR} -DBIN_DIR=${BIN_DIR}
    -DCMAKE_INSTALL_RPATH=${CMAKE_BUILD_RPATH}
    -DLEMON_ROOT=$<$<BOOL:${LEMON_TARGET}>:${CMAKE_BINARY_DIR}> -DLEMON_TEMPLATE=$<$<BOOL:${LEMON_TARGET}>:${LEMON_TEMPLATE}>
    -DRE2C_ROOT=$<$<BOOL:${RE2C_TARGET}>:${CMAKE_BINARY_DIR}>
    DEPENDS ${PERPLEX_DEPS}
    )

  list(APPEND BRLCAD_DEPS PERPLEX_BLD)

  SetTargetFolder(PERPLEX_BLD "Third Party Libraries")
endif (${CMAKE_PROJECT_NAME}_PERPLEX_BUILD)

# Make sure we load FindPERPLEX.cmake to be able to define PERPLEX targets.
include(${BRLCAD_CMAKE_DIR}/FindPERPLEX.cmake)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8

