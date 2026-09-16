set(propeller_srcs
  propeller.cpp
  )

brlcad_addexec(propeller "${propeller_srcs}" "libwdb;libbrep;OPENNURBS::OPENNURBS;${M_LIBRARY}" FOLDER Shapes)

cmakefiles(
  CMakeLists.txt
  propeller.cpp
  )

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
