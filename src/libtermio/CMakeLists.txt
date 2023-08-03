if (NOT WIN32)
  include_directories(
    ${BRLCAD_BINARY_DIR}/include
    ${BRLCAD_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}
    )

  BRLCAD_ADDLIB(libtermio termio.c "libbu")
  set_target_properties(libtermio PROPERTIES VERSION 20.0.1 SOVERSION 20)
endif(NOT WIN32)

CMAKEFILES(
  CMakeLists.txt
  termio.c
  libtermio.h
  )

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
