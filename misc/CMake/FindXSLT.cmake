# - Find XSLT processor
#
# The following variables are set:
#
# XSLTPROC_EXEC

FIND_PROGRAM(XSLTPROC_EXEC xsltproc DOC "path to the xsltproc executable")

INCLUDE(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(XSLTPROC DEFAULT_MSG XSLTPROC_EXEC)
MARK_AS_ADVANCED(XSLTPROC_EXEC)
