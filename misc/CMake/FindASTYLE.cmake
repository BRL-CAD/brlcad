# The module defines the following variables:
#  ASTYLE_EXECUTABLE - the path to the astyle executable
#
#=============================================================================

find_program(ASTYLE_EXECUTABLE astyle DOC "path to the astyle executable")
mark_as_advanced(ASTYLE_EXECUTABLE)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(ASTYLE DEFAULT_MSG ASTYLE_EXECUTABLE)
