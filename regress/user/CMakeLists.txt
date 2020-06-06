# This program is intended to test the usability of BRL-CAD's libraries without
# the internal compile-time APIs - i.e., to simulate what a BRL-CAD client code
# might experience when trying to use the BRL-CAD libraries.

# Due to the nature of this test, it should not see any of the defines used by
# BRL-CAD to enable compile-time API.  We first clear any inherited include
# directories or compile definitions as a precaution:
set_property(DIRECTORY PROPERTY COMPILE_DEFINITIONS "")
set_property(DIRECTORY PROPERTY INCLUDE_DIRECTORIES "")


# NOTE - for the moment we are not overriding the compile flags (optimization,
# warnings, etc.) from the parent build, although eventually we should probably
# look into doing so...  We want the strictness of those checks to apply to
# this code as well, so long as they don't also contain anything that ties this
# code to any internal, compile time behaviors or features.

# We deliberately do not use any of the BRL-CAD wrapper functions such as
# BRLCAD_ADDEXEC in order to avoid their automatic addtiion of compile
# defintions or other magic specific to BRL-CAD's internal build.
add_executable(cad_user cad_user.c)
target_link_libraries(cad_user libged)
target_include_directories(cad_user PUBLIC "${CMAKE_BINARY_DIR}/${INCLUDE_DIR}/brlcad")

set_target_properties(cad_user PROPERTIES FOLDER "BRL-CAD Regression Tests/user")

set(LOG_FILE "${CMAKE_CURRENT_BINARY_DIR}/regress-user.log")
BRLCAD_REGRESSION_TEST(regress-user "cad_user;moss.g" EXEC cad_user)
DISTCLEAN(${LOG_FILE})

CMAKEFILES(
  CMakeLists.txt
  regress-user.cmake.in
  )

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
