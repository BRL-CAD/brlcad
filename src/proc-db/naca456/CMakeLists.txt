set(naca456_srcs
  naca456.cpp
  )

brlcad_addexec(naca456 "${naca456_srcs}" "libwdb;libbrep;OPENNURBS::OPENNURBS;${M_LIBRARY}" NO_STRICT NO_INSTALL)

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
