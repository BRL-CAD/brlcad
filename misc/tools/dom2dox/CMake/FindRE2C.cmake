# The module defines the following variables:
#  RE2C_EXECUTABLE - the path to the re2c executable
#
#=============================================================================

find_program(RE2C_EXECUTABLE re2c DOC "path to the re2c executable")
mark_as_advanced(RE2C_EXECUTABLE)

include(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(RE2C DEFAULT_MSG RE2C_EXECUTABLE)
