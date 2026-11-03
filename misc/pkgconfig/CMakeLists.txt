# TODO - if we're going to keep these pkgconfig files, need to properly
# integrate their generation with the CMake build.  For a starting point look
# at https://stackoverflow.com/q/44292462

file(RELATIVE_PATH pkgconfig_to_prefix "${CMAKE_INSTALL_PREFIX}/${LIB_DIR}/pkgconfig" "${CMAKE_INSTALL_PREFIX}")
set(prefix "\${pcfiledir}/${pkgconfig_to_prefix}")
set(exec_prefix "\${prefix}")
set(libdir "\${exec_prefix}/lib")
set(includedir "\${prefix}/include")
set(datarootdir "\${prefix}/share")
set(datadir "\${datarootdir}")
if(M_LIBRARY)
  set(LIBM "-l${M_LIBRARY}")
endif(M_LIBRARY)
if(REGEX_LIBRARY)
  set(LIBREGEX "-l${REGEX_LIBRARY}")
endif(REGEX_LIBRARY)
if(UUID_LIBRARIES)
  set(LIBUUID "-l${UUID_LIBRARIES}")
endif(UUID_LIBRARIES)
# Keep bundled libraries relative to the package prefix while retaining
# search directories needed for system dependencies outside the compiler's
# implicit library paths.
function(brlcad_pkgconfig_library target result)
  if(NOT TARGET ${target})
    set(${result} "" PARENT_SCOPE)
    return()
  endif()
  set(location_properties IMPORTED_LOCATION)
  if(WIN32)
    list(PREPEND location_properties IMPORTED_IMPLIB)
  endif()
  foreach(location_property IN LISTS location_properties)
    get_target_property(library ${target} ${location_property})
    if(library)
      break()
    endif()
  endforeach()
  if(NOT library)
    get_target_property(configurations ${target} IMPORTED_CONFIGURATIONS)
    foreach(configuration IN LISTS configurations)
      string(TOUPPER "${configuration}" configuration)
      foreach(location_property IN LISTS location_properties)
        get_target_property(library ${target} "${location_property}_${configuration}")
        if(library)
          break()
        endif()
      endforeach()
      if(library)
        break()
      endif()
    endforeach()
  endif()
  if(NOT library)
    message(FATAL_ERROR "Cannot generate pkg-config flags for ${target}")
  endif()
  get_filename_component(name "${library}" NAME)
  # Strip the library suffix without discarding dots in names such as tcl8.6.
  foreach(suffix "${CMAKE_SHARED_LIBRARY_SUFFIX}" "${CMAKE_STATIC_LIBRARY_SUFFIX}")
    if(NOT suffix)
      continue()
    endif()
    string(FIND "${name}" "${suffix}" suffix_at REVERSE)
    if(NOT suffix_at LESS 0)
      string(SUBSTRING "${name}" ${suffix_at} -1 ending)
      string(LENGTH "${suffix}" suffix_length)
      string(SUBSTRING "${ending}" ${suffix_length} -1 version)
      if(version STREQUAL "" OR version MATCHES "^[.][0-9.]+$")
        string(SUBSTRING "${name}" 0 ${suffix_at} name)
        break()
      endif()
    endif()
  endforeach()
  if(CMAKE_SHARED_LIBRARY_PREFIX)
    string(REGEX REPLACE "^${CMAKE_SHARED_LIBRARY_PREFIX}" "" name "${name}")
  endif()
  set(flags "-l${name}")
  file(REAL_PATH "${CMAKE_BINARY_DIR}" build_root)
  file(REAL_PATH "${library}" real_library)
  cmake_path(IS_PREFIX build_root "${real_library}" NORMALIZE bundled)
  get_filename_component(directory "${library}" DIRECTORY)
  if(NOT bundled AND NOT directory IN_LIST CMAKE_C_IMPLICIT_LINK_DIRECTORIES AND
      NOT directory IN_LIST CMAKE_CXX_IMPLICIT_LINK_DIRECTORIES)
    string(REPLACE " " "\\ " directory "${directory}")
    set(flags "-L${directory} ${flags}")
  endif()
  set(${result} "${flags}" PARENT_SCOPE)
endfunction()

brlcad_pkgconfig_library(OPENNURBS::OPENNURBS LIBOPENNURBS)
brlcad_pkgconfig_library(LMDB::LMDB LIBLMDB)
brlcad_pkgconfig_library(TCL::TCL LIBTCL)

# Match the C++ runtime selected by the toolchain, including libc++ builds.
set(LIBSTDCXX "")
foreach(runtime c++ stdc++)
  if(runtime IN_LIST CMAKE_CXX_IMPLICIT_LINK_LIBRARIES)
    set(LIBSTDCXX "-l${runtime}")
    break()
  endif()
endforeach()
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
