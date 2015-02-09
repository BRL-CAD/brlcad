## This file should be placed in the root directory of your project.
## Then modify the CMakeLists.txt file in the root directory of your
## project to incorporate the testing dashboard.
##
## # The following are required to submit to the CDash dashboard:
##   ENABLE_TESTING()
##   INCLUDE(CTest)

set(CTEST_PROJECT_NAME "BRL-CAD")
set(CTEST_NIGHTLY_START_TIME "06:00:00 UTC")

set(CTEST_DROP_METHOD "http")
set(CTEST_DROP_SITE "brlcad.org")
set(CTEST_DROP_LOCATION "/CDash/submit.php?project=BRL-CAD")
set(CTEST_DROP_SITE_CDASH TRUE)
