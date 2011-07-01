# - Find regex 
# Find the native REGEX includes and library
#
#  REGEX_INCLUDE_DIR   - where to find regex.h, etc.
#  REGEX_LIBRARY      - List of libraries when using regex.
#  REGEX_FOUND          - True if regex found.
#
#=============================================================================

INCLUDE(CheckLibraryExists)

# Because at least one specific framework (Ruby) on OSX has been observed
# to include its own regex.h copy, check frameworks last - /usr/include
# is preferred to a package-specific copy for a generic regex search
SET(CMAKE_FIND_FRAMEWORK LAST)
FIND_PATH(REGEX_INCLUDE_DIR regex.h)
SET(REGEX_NAMES c regex compat)
FOREACH(rname ${REGEX_NAMES})
	IF(NOT REGEX_LIBRARY)
		CHECK_LIBRARY_EXISTS(${rname} regcomp "" HAVE_REGEX_LIB)
		IF(HAVE_REGEX_LIB)
			FIND_LIBRARY(REGEX_LIBRARY NAMES ${rname})
		ENDIF(HAVE_REGEX_LIB)
	ENDIF(NOT REGEX_LIBRARY)
ENDFOREACH(rname ${REGEX_NAMES})

MARK_AS_ADVANCED(REGEX_LIBRARY REGEX_INCLUDE_DIR)

# handle the QUIETLY and REQUIRED arguments and set REGEX_FOUND to TRUE if 
# all listed variables are TRUE
INCLUDE(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(REGEX DEFAULT_MSG REGEX_INCLUDE_DIR REGEX_LIBRARY)

