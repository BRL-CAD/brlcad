set(naca456_srcs
  naca456.cpp
  )

find_package_gte(REQUIRED)

brlcad_addexec(naca456 "${naca456_srcs}" "libwdb;libbrep;libbg;OPENNURBS::OPENNURBS;${M_LIBRARY}")
target_include_directories(naca456 BEFORE PRIVATE ${BRLCAD_SOURCE_DIR}/src/libbg/GTE ${GTE_INCLUDE_DIR})

cmakefiles(
  CMakeLists.txt
  README.md
  naca456.cpp
  )

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
