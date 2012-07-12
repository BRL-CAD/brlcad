#
# - Find perplex executable 
#
#  PERPLEX_EXECUTABLE - path to the perplex program
#  ====================================================================

find_program(PERPLEX_EXECUTABLE perplex DOC "path to the perplex executable")
mark_as_advanced(PERPLEX_EXECUTABLE)
include(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(PERPLEX DEFAULT_MSG PERPLEX_EXECUTABLE)
