# - Find STEP Class Library binaries and libraries
#
# The following variables are set:
#
# OPENNURBS_LIBRARY

FIND_LIBRARY(OPENNURBS_LIBRARY NAMES opennurbs openNURBS OpenNURBS NO_CMAKE_PATH)

INCLUDE(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(OPENNURBS DEFAULT_MSG OPENNURBS_LIBRARY)
