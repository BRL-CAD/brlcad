# - Find Apache FOP (pdf generator)
#
# The following variables are set:
#
# APACHE_FOP

FIND_PROGRAM(APACHE_FOP fop DOC "path to the exec script for Apache FOP")

INCLUDE(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(FOP DEFAULT_MSG APACHE_FOP)
