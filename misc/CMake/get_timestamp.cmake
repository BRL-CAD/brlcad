# Unfortunately, older CMake versions don't give you variables with current
# day, month, etc.  There are several possible approaches to this, but most
# (e.g. the date command) are not cross platform. We build a small C file which
# writes out the needed values to files in the build directory. Those files are
# then read and stripped by CMake.  This can go away once we require a version
# of CMake that supports string(TIMESTAMP...) command - i.e. > 2.8.10

set(CONFIG_TIME_DAY_FILE "${BRLCAD_BINARY_DIR}/include/conf/CONFIG_TIME_DAY")
set(CONFIG_TIME_MONTH_FILE "${BRLCAD_BINARY_DIR}/include/conf/CONFIG_TIME_MONTH")
set(CONFIG_TIME_YEAR_FILE "${BRLCAD_BINARY_DIR}/include/conf/CONFIG_TIME_YEAR")
set(CONFIG_TIMESTAMP_FILE "${BRLCAD_BINARY_DIR}/include/conf/CONFIG_TIMESTAMP")
DISTCLEAN(${CONFIG_TIME_DAY_FILE} ${CONFIG_TIME_MONTH_FILE}
  ${CONFIG_TIME_YEAR_FILE} ${CONFIG_TIMESTAMP_FILE})
file(MAKE_DIRECTORY "${BRLCAD_BINARY_DIR}/include")
file(MAKE_DIRECTORY "${BRLCAD_BINARY_DIR}/include/conf")
configure_file("${CMAKE_SOURCE_DIR}/misc/CMake/test_srcs/time.c.in" "${CMAKE_BINARY_DIR}/CMakeTmp/time.c")
TRY_RUN(TIME_RESULT TIME_COMPILED
  "${CMAKE_BINARY_DIR}/CMakeTmp"
  "${CMAKE_BINARY_DIR}/CMakeTmp/time.c"
  OUTPUT_VARIABLE COMPILEMESSAGES)
if(TIME_RESULT MATCHES "^0$")
  file(READ ${CONFIG_TIME_DAY_FILE} CONFIG_DAY)
  string(STRIP ${CONFIG_DAY} CONFIG_DAY)
  file(READ ${CONFIG_TIME_MONTH_FILE} CONFIG_MONTH)
  string(STRIP ${CONFIG_MONTH} CONFIG_MONTH)
  file(READ ${CONFIG_TIME_YEAR_FILE} CONFIG_YEAR)
  string(STRIP ${CONFIG_YEAR} CONFIG_YEAR)
  set(CONFIG_DATE "${CONFIG_YEAR}${CONFIG_MONTH}${CONFIG_DAY}")
else(TIME_RESULT MATCHES "^0$")
  message(FATAL_ERROR "Code to determine current date and time failed!\n")
endif(TIME_RESULT MATCHES "^0$")


# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8

