set(LIBMCPCAD_SOURCES
  exec.c
  frame.c
  parse.c
  session.c
)

# Note - libmcpcad_deps is defined by ${BRLCAD_SOURCE_DIR}/src/source_dirs.cmake
brlcad_addlib(libmcpcad "${LIBMCPCAD_SOURCES}" "" "${CMAKE_CURRENT_SOURCE_DIR}"
  PRIVATE_LIBS ${libmcpcad_deps}
)
set_target_properties(libmcpcad PROPERTIES VERSION 20.0.1 SOVERSION 20)

add_subdirectory(tests)

cmakefiles(CMakeLists.txt)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
