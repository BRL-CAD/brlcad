# - Find UtahRLE binaries and libraries
#
# The following variables are set:
#
# UTAHRLE_LIBRARY

FIND_LIBRARY(UTAHRLE_LIBRARY NAMES utahrle)

INCLUDE(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(UTAHRLE DEFAULT_MSG UTAHRLE_LIBRARY)
