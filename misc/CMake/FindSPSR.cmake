# Defines:
#
#  SPSR_FOUND - system has SPSR lib
#  SPSR_INCLUDE_DIR - the SPSR include directory
#  SPSR::SPSR - The header-only SPSR library
#
# This module reads hints about search locations from
# the following variable:
#
# SPSR_ROOT

set(_SPSR_SEARCHES)
# Search SPSR_ROOT first if it is set.
if(SPSR_ROOT)
  set(_SPSR_SEARCH_ROOT PATHS ${SPSR_ROOT} NO_DEFAULT_PATH)
  list(APPEND _SPSR_SEARCHES _SPSR_SEARCH_ROOT)
endif()

# Try each search configuration.
foreach(search ${_SPSR_SEARCHES})
  find_path(SPSR_INCLUDE_DIR NAMES SPSR/Reconstructors.h ${${search}} PATH_SUFFIXES include include/SPSR)
endforeach()

mark_as_advanced(SPSR_INCLUDE_DIR)

# handle the QUIETLY and REQUIRED arguments and set ZLIB_FOUND to TRUE if
# all listed variables are TRUE
include(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(SPSR REQUIRED_VARS SPSR_INCLUDE_DIR)

if(SPSR_FOUND AND NOT TARGET SPSR::SPSR)
  add_library(SPSR::SPSR INTERFACE IMPORTED)
  set_target_properties(SPSR::SPSR PROPERTIES INTERFACE_INCLUDE_DIRECTORIES "${SPSR_INCLUDE_DIR}")
endif()

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8

