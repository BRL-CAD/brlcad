# - Find UtahRLE libraries
#
# The following variables are set:
#
# UTAHRLE_LIBRARY
# The following variables are set:
#
#  UTAHRLE_INCLUDE_DIRS   - where to find zlib.h, etc.
#  UTAHRLE_LIBRARIES      - List of libraries when using zlib.
#  UTAHRLE_FOUND          - True if zlib found.

FIND_PATH(UTAHRLE_INCLUDE_DIR rle.h)
FIND_LIBRARY(UTAHRLE_LIBRARY NAMES UTAHRLE)

INCLUDE(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(UTAHRLE DEFAULT_MSG UTAHRLE_LIBRARY UTAHRLE_INCLUDE_DIR)

IF (UTAHRLE_FOUND)
    SET(UTAHRLE_INCLUDE_DIRS ${UTAHRLE_INCLUDE_DIR})
    SET(UTAHRLE_LIBRARIES    ${UTAHRLE_LIBRARY})
ENDIF()