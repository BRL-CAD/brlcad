#  For the moment, this is marked NOSYS - it's possible that some Debian
#  systems would have 0.9 of vdslib installed, but it's unmaintained and we're
#  likely to be making changes.  If our own copy of VDSlib ever spins back off
#  into its own project, revisit the NOSYS

set(libvds_DESCRIPTION "
Option for enabling and disabling compilation of the libvds triangle
simplification library provided with BRL-CAD's source code.  Default
is AUTO, responsive to the toplevel BRLCAD_BUNDLED_LIBS option and
testing first for a system version if BRLCAD_BUNDLED_LIBS is also
AUTO.
")
THIRD_PARTY(libvds VDS libvds libvds_DESCRIPTION ALIASES ENABLE_VDS FLAGS NOSYS)

if (${CMAKE_PROJECT_NAME}_VDS_BUILD)

  if (MSVC)
    set(VDS_BASENAME vds)
  else (MSVC)
    set(VDS_BASENAME libvds)
  endif (MSVC)


  ExternalProject_Add(VDS_BLD
    SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../other/libvds"
    BUILD_ALWAYS ${EXTERNAL_BUILD_UPDATE} ${LOG_OPTS}
    CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${CMAKE_BINARY_DIR} -DLIB_DIR=${LIB_DIR} -DBIN_DIR=${BIN_DIR}
               -DCMAKE_INSTALL_RPATH=${CMAKE_BUILD_RPATH} -DBUILD_STATIC_LIBS=${BUILD_STATIC_LIBS}
    )
  ExternalProject_Target(vds VDS_BLD
    OUTPUT_FILE ${VDS_BASENAME}${CMAKE_SHARED_LIBRARY_SUFFIX}
    STATIC_OUTPUT_FILE ${VDS_BASENAME}${CMAKE_STATIC_LIBRARY_SUFFIX}
    RPATH
    )

  ExternalProject_ByProducts(VDS_BLD ${INCLUDE_DIR}
    vds.h
    )

  set(VDS_LIBRARIES vds CACHE STRING "Building bundled netpbm" FORCE)
  set(VDS_INCLUDE_DIRS "${CMAKE_BINARY_DIR}/${INCLUDE_DIR}" CACHE STRING "Directory containing vds headers." FORCE)

  list(APPEND BRLCAD_DEPS VDS_BLD)

  SetTargetFolder(VDS_BLD "Third Party Libraries")

endif (${CMAKE_PROJECT_NAME}_VDS_BUILD)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8

