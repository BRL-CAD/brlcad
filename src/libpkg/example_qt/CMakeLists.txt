if(BRLCAD_ENABLE_QT)
  find_package_qt(REQUIRED)

  # Include directories needed by libpkg users
  set(PKG_EXAMPLE_INCLUDES ${CMAKE_CURRENT_SOURCE_DIR} ${BU_INCLUDE_DIRS} ${PKG_INCLUDE_DIRS})
  list(REMOVE_DUPLICATES PKG_EXAMPLE_INCLUDES)
  include_directories(${PKG_EXAMPLE_INCLUDES})

  if(Qt6Widgets_FOUND)
    qt6_wrap_cpp(pkg_moc_srcs ncp.h)
  else()
    qt5_wrap_cpp(pkg_moc_srcs ncp.h)
  endif(Qt6Widgets_FOUND)
  distclean(moc_ncp.cpp_parameters)

  if(Qt6Widgets_FOUND)
    brlcad_addexec(qt_test_server "server.cpp;${pkg_moc_srcs}" "libbu;libpkg;Qt6::Core;Qt6::Network;Qt6::Widgets" TEST)
  else()
    brlcad_addexec(qt_test_server "server.cpp;${pkg_moc_srcs}" "libbu;libpkg;Qt5::Core;Qt5::Network;Qt5::Widgets" TEST)
  endif(Qt6Widgets_FOUND)
  brlcad_addexec(qt_test_client client.cpp "libbu;libpkg" NO_INSTALL)
endif(BRLCAD_ENABLE_QT)

cmakefiles(
  ncp.h
  server.cpp
  client.cpp
)
cmakefiles(CMakeLists.txt)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
