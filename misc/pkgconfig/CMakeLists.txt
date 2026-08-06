# TODO - if we're going to keep these pkgconfig files, need to properly
# integrate their generation with the CMake build.  For a starting point look
# at https://stackoverflow.com/q/44292462

set(prefix "${CMAKE_INSTALL_PREFIX}")
set(exec_prefix "\${prefix}")
set(libdir "\${exec_prefix}/lib")
set(includedir "\${prefix}/include")
set(datarootdir "\${prefix}/share")
set(datadir "\${datarootdir}")
set(LIBTCL "-ltcl${TCL_VERSION_MAJOR}.${TCL_VERSION_MINOR}")
if(M_LIBRARY)
  set(LIBM "-l${M_LIBRARY}")
endif(M_LIBRARY)
if(REGEX_LIBRARY)
  set(LIBREGEX "-l${REGEX_LIBRARY}")
endif(REGEX_LIBRARY)
if(UUID_LIBRARIES)
  set(LIBUUID "-l${UUID_LIBRARIES}")
endif(UUID_LIBRARIES)
if(TARGET OPENNURBS::OPENNURBS)
  # pkg-config cannot express a CMake imported target.  Emit the discovered
  # library's link name without embedding a build-machine path in the .pc file.
  get_target_property(_opennurbs_library OPENNURBS::OPENNURBS IMPORTED_LOCATION)
  if(NOT _opennurbs_library)
    get_target_property(_opennurbs_configs OPENNURBS::OPENNURBS IMPORTED_CONFIGURATIONS)
    foreach(_opennurbs_config IN LISTS _opennurbs_configs)
      string(TOUPPER "${_opennurbs_config}" _opennurbs_config_upper)
      get_target_property(
        _opennurbs_library
        OPENNURBS::OPENNURBS
        "IMPORTED_LOCATION_${_opennurbs_config_upper}"
      )
      if(_opennurbs_library)
        break()
      endif()
    endforeach()
  endif()
  get_filename_component(_opennurbs_libname "${_opennurbs_library}" NAME_WE)
  string(REGEX REPLACE "^${CMAKE_SHARED_LIBRARY_PREFIX}" "" _opennurbs_libname "${_opennurbs_libname}")
  set(LIBOPENNURBS "-l${_opennurbs_libname}")
  unset(_opennurbs_config)
  unset(_opennurbs_configs)
  unset(_opennurbs_config_upper)
  unset(_opennurbs_library)
  unset(_opennurbs_libname)
endif(TARGET OPENNURBS::OPENNURBS)

#The CMake system relies on the C++ compiler to find
#this, so we set it manually for pkgconfig
set(LIBSTDCXX "-lstdc++")
#Also provide the X11 and PNG link strings - generally
#speaking CMake tends to use full paths, so this is a
#bit hackish - need to investigate best practices for later
#also, are pkgconfig systems generally X11 only? (Haiku
#and Aqua build on OSX come to mind, but are pkgconfig setups
#used there?
set(X_LIBS "-lX11 -lXext -lXi")
set(LIBPNG "-lpng")

set(
  pkgconfig_DATA
  libbu.pc.in
  libbn.pc.in
  libbg.pc.in
  libbv.pc.in
  libnmg.pc.in
  libbrep.pc.in
  librt.pc.in
  libwdb.pc.in
)
if("${BRLCAD_ENABLE_TARGETS}" GREATER 1 OR "${BRLCAD_ENABLE_TARGETS}" STREQUAL "")
  set(
    pkgconfig_DATA
    ${pkgconfig_DATA}
    libanalyze.pc.in
    libbrlcad.pc.in
    libdm.pc.in
    libfft.pc.in
    libgcv.pc.in
    libged.pc.in
    libicv.pc.in
    liboptical.pc.in
    libpkg.pc.in
  )
endif("${BRLCAD_ENABLE_TARGETS}" GREATER 1 OR "${BRLCAD_ENABLE_TARGETS}" STREQUAL "")

foreach(pkgfile ${pkgconfig_DATA})
  string(REGEX REPLACE "([0-9a-z_-]*).pc.in" "\\1" filename_root "${pkgfile}")
  configure_file(${pkgfile} ${CMAKE_CURRENT_BINARY_DIR}/${filename_root}.pc @ONLY)
  distclean("${CMAKE_CURRENT_BINARY_DIR}/${filename_root}.pc")
  install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${filename_root}.pc DESTINATION ${LIB_DIR}/pkgconfig)
endforeach(pkgfile ${pkgconfig_DATA})

cmakefiles(${pkgconfig_DATA})
cmakefiles(CMakeLists.txt)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
