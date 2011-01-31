# - Find Programs needed to run .sh scripts.

# The following variables are set:
#
# SH_EXEC 
# CP_EXEC
# MV_EXEC
# RM_EXEC

FIND_PROGRAM(SH_EXEC NAMES sh dash bash DOC "path to shell executable")
FIND_PROGRAM(MV_EXEC NAMES mv DOC "path to move executable")
FIND_PROGRAM(CP_EXEC NAMES cp DOC "path to copy executable")
FIND_PROGRAM(RM_EXEC NAMES rm DOC "path to remove executable")

INCLUDE(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(SHELL_SUPPORTED DEFAULT_MSG SH_EXEC CP_EXEC MV_EXEC RM_EXEC)
MARK_AS_ADVANCED(SH_EXEC)
MARK_AS_ADVANCED(MV_EXEC)
MARK_AS_ADVANCED(CP_EXEC)
MARK_AS_ADVANCED(RM_EXEC)
