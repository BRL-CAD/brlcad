# - Find timezone information
#
#  TIMEZONE_DATA_DIR - where to find timezone information.
#  TIMEZONE_FOUND    - True if tzdata found.
#
#=============================================================================

SET(TZ_SEARCH_PATHS
	/usr/share/zoneinfo
	/usr/share/lib/zoneinfo
	/usr/lib/zoneinfo
	)
FIND_PATH(TIMEZONE_DATA_DIR UTC PATHS ${TZ_SEARCH_PATHS} NO_DEFAULT_PATH)
MARK_AS_ADVANCED(TIMEZONE_DATA_DIR)

# handle the QUIETLY and REQUIRED arguments and set REGEX_FOUND to TRUE if 
# all listed variables are TRUE
INCLUDE(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(TIMEZONE DEFAULT_MSG TIMEZONE_DATA_DIR)

