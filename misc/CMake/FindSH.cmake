# - Find Bourne Shell or compatible

# The following variables are set:
#
# SH_EXEC 

FIND_PROGRAM(SH_EXEC NAMES sh dash bash DOC "path to shell executable")

INCLUDE(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(SH DEFAULT_MSG SH_EXEC)
