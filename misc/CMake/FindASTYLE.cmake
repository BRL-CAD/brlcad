# The module defines the following variables:
#  ASTYLE_EXECUTABLE - the path to the astyle executable
#
#=============================================================================

find_program(ASTYLE_EXECUTABLE astyle DOC "path to the astyle executable")
mark_as_advanced(ASTYLE_EXECUTABLE)

include(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(ASTYLE DEFAULT_MSG ASTYLE_EXECUTABLE)
