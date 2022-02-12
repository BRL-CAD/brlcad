if (BRLCAD_ENABLE_QT)

  # Include directories needed by libpkg users
  set(PKG_EXAMPLE_INCLUDES
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${BU_INCLUDE_DIRS}
    ${PKG_INCLUDE_DIRS}
    )
  LIST(REMOVE_DUPLICATES PKG_EXAMPLE_INCLUDES)
  include_directories(${PKG_EXAMPLE_INCLUDES})

  if(Qt6Widgets_FOUND)
    QT6_WRAP_CPP(pkg_moc_srcs ncp.h)
  else()
    QT5_WRAP_CPP(pkg_moc_srcs ncp.h)
  endif(Qt6Widgets_FOUND)
  DISTCLEAN(moc_ncp.cpp_parameters)

  if(Qt6Widgets_FOUND)
    BRLCAD_ADDEXEC(qt_test_server "server.cpp;${pkg_moc_srcs}" "libbu;libpkg;Qt6::Core;Qt6::Network;Qt6::Widgets" NO_INSTALL)
  else()
    BRLCAD_ADDEXEC(qt_test_server "server.cpp;${pkg_moc_srcs}" "libbu;libpkg;Qt5::Core;Qt5::Network;Qt5::Widgets" NO_INSTALL)
  endif(Qt6Widgets_FOUND)
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
