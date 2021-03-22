set(CTEST_CUSTOM_WARNING_MATCH ${CTEST_CUSTOM_WARNING_MATCH} "{standard input}:[0-9][0-9]*: WARNING: ")
set(CTEST_CUSTOM_ERROR_MATCH ${CTEST_CUSTOM_ERROR_MATCH} "{standard input}:[0-9][0-9]*: ERROR  : ")

set(CTEST_PROJECT_NAME "StepClassLibrary")
set(CTEST_NIGHTLY_START_TIME "00:00:00 EST")

set(CTEST_DROP_METHOD "http")
set(CTEST_DROP_SITE "my.cdash.org")
set(CTEST_DROP_LOCATION "/submit.php?project=StepClassLibrary")
set(CTEST_DROP_SITE_CDASH TRUE)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8

