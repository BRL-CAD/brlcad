if (BRLCAD_ENABLE_QT)

  find_package(Qt5 COMPONENTS Core Network)

  # Include directories needed by libpkg users
  set(PKG_EXAMPLE_INCLUDES
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${BU_INCLUDE_DIRS}
    ${PKG_INCLUDE_DIRS}
    )
  LIST(REMOVE_DUPLICATES PKG_EXAMPLE_INCLUDES)
  include_directories(${PKG_EXAMPLE_INCLUDES})

  BRLCAD_ADDEXEC(qt_test_server server.cpp "libbu;libpkg;Qt5::Core;Qt5::Network" NO_INSTALL)
  BRLCAD_ADDEXEC(qt_test_client client.cpp "libbu;libpkg" NO_INSTALL)
endif (BRLCAD_ENABLE_QT)

CMAKEFILES(
  ncp.h
  server.cpp
  client.cpp
  )
CMAKEFILES(CMakeLists.txt)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
