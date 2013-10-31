# Third party tools

# Clear all pre-defined CMake flags
CLEAR_BUILD_FLAGS()

# Quiet all warnings in this directory
if(BRLCAD_DISABLE_SRC_OTHER_WARN)
  add_definitions(-w)
endif(BRLCAD_DISABLE_SRC_OTHER_WARN)

# For lower build levels, some of the third party components are not
# needed.  define some variables we can use for testing.
set(BRLCAD_LEVEL2 0)
set(BRLCAD_LEVEL3 0)
if(NOT BRLCAD_ENABLE_TARGETS)
  set(BRLCAD_LEVEL2 1)
  set(BRLCAD_LEVEL3 1)
else(NOT BRLCAD_ENABLE_TARGETS)
  if(${BRLCAD_ENABLE_TARGETS} GREATER 1)
    set(BRLCAD_LEVEL2 1)
  endif(${BRLCAD_ENABLE_TARGETS} GREATER 1)
  if(${BRLCAD_ENABLE_TARGETS} GREATER 2)
    set(BRLCAD_LEVEL3 1)
  endif(${BRLCAD_ENABLE_TARGETS} GREATER 2)
endif(NOT BRLCAD_ENABLE_TARGETS)

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


# Locac Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
