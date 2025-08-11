add_subdirectory(analyze)
add_subdirectory(bu)
add_subdirectory(bn)
add_subdirectory(bg)
add_subdirectory(brep)
add_subdirectory(bv)
add_subdirectory(dm)
add_subdirectory(gcv)
add_subdirectory(ged)
add_subdirectory(icv)
add_subdirectory(nmg)
add_subdirectory(optical)
add_subdirectory(qtcad)
add_subdirectory(rt)
add_subdirectory(tclcad)

brlcad_manage_files("analyze.h" ${INCLUDE_DIR}/brlcad REQUIRED libanalyze)
brlcad_manage_files("bg.h" ${INCLUDE_DIR}/brlcad REQUIRED libbg)
brlcad_manage_files("bio.h" ${INCLUDE_DIR}/brlcad)
brlcad_manage_files("bn.h" ${INCLUDE_DIR}/brlcad REQUIRED libbn)
brlcad_manage_files("bnetwork.h" ${INCLUDE_DIR}/brlcad)
brlcad_manage_files("brep.h" ${INCLUDE_DIR}/brlcad REQUIRED libbrep)
brlcad_manage_files("bresource.h" ${INCLUDE_DIR}/brlcad)
brlcad_manage_files("brlcad.h" ${INCLUDE_DIR}/brlcad REQUIRED libwdb)
brlcad_manage_files("bsocket.h" ${INCLUDE_DIR}/brlcad)
brlcad_manage_files("bu.h" ${INCLUDE_DIR}/brlcad REQUIRED libbu)
brlcad_manage_files("bv.h" ${INCLUDE_DIR}/brlcad REQUIRED libbv)
brlcad_manage_files("common.h" ${INCLUDE_DIR}/brlcad)
brlcad_manage_files("dm.h" ${INCLUDE_DIR}/brlcad REQUIRED libdm)
brlcad_manage_files("fft.h" ${INCLUDE_DIR}/brlcad REQUIRED libfft)
brlcad_manage_files("gcv.h" ${INCLUDE_DIR}/brlcad REQUIRED libgcv)
brlcad_manage_files("ged.h" ${INCLUDE_DIR}/brlcad REQUIRED libged)
brlcad_manage_files("icv.h" ${INCLUDE_DIR}/brlcad REQUIRED libicv)
brlcad_manage_files("nmg.h" ${INCLUDE_DIR}/brlcad REQUIRED libnmg)
brlcad_manage_files("optical.h" ${INCLUDE_DIR}/brlcad REQUIRED liboptical)
brlcad_manage_files("pc.h" ${INCLUDE_DIR}/brlcad REQUIRED librt)
brlcad_manage_files("photonmap.h" ${INCLUDE_DIR}/brlcad REQUIRED liboptical)
brlcad_manage_files("pinttypes.h" ${INCLUDE_DIR}/brlcad)
brlcad_manage_files("pkg.h" ${INCLUDE_DIR}/brlcad REQUIRED libpkg)
brlcad_manage_files("pstdint.h" ${INCLUDE_DIR}/brlcad)
brlcad_manage_files("raytrace.h" ${INCLUDE_DIR}/brlcad REQUIRED librt)
brlcad_manage_files("tclcad.h" ${INCLUDE_DIR}/brlcad REQUIRED libtclcad)
brlcad_manage_files("vmath.h" ${INCLUDE_DIR}/brlcad)
brlcad_manage_files("wdb.h" ${INCLUDE_DIR}/brlcad REQUIRED libwdb)

build_cfg_hdr(brlcad_ident.h.in ${INCLUDE_DIR}/brlcad)

# Define the amendment count if we have one
set(BRLCAD_VERSION_AMEND_STR "")
if(BRLCAD_VERSION_AMEND)
  set(BRLCAD_VERSION_AMEND_STR "#define BRLCAD_VERSION_AMEND ${BRLCAD_VERSION_AMEND}")
endif(BRLCAD_VERSION_AMEND)

# Generate the version header
build_cfg_hdr(brlcad_version.h.in ${INCLUDE_DIR}/brlcad)

# FIXME: shouldn't exist, needs to be absorbed into brlcad_config.h
build_cfg_hdr(config_win.h.in ${INCLUDE_DIR}/brlcad)

set(
  include_misc
  CMakeLists.txt
  brlcad_ident.h.in
  brlcad_version.h.in
  conf/BrlcadConfig.tmpl
  conf/MAJOR
  conf/MINOR
  conf/PATCH
  conf/make.vbs
  config_win.h.in
  RTree.h
)
cmakefiles(${include_misc})

# Header checking target - this compiles the headers as isolated files to
# ensure they include what is needed to resolve any symbols defined in the
# public API.

# Note: don't know how to do this on Windows - skip with Visual Studio
if(DEFINED BRLCAD_HDR_CHECK AND NOT MSVC)
  add_custom_target(check-headers)
  set_target_properties(check-headers PROPERTIES FOLDER "BRL-CAD Header Checks")

  file(GLOB_RECURSE hdrfiles RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}" "*.h")

  foreach(hdrfile ${hdrfiles})
    if(NOT "${hdrfile}" STREQUAL "RtServerImpl.h")
      string(REPLACE "-" "_" STMPFILE "${hdrfile}")
      string(REPLACE "." "_" STMPFILE "${STMPFILE}")
      string(REPLACE "/" "_" STMPFILE "${STMPFILE}")

      add_custom_command(
        OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${STMPFILE}.stamp
        COMMAND
          ${CMAKE_CXX_COMPILER} -I${CMAKE_SOURCE_DIR}/include -I${CMAKE_BINARY_DIR}/include -fsyntax-only -Wall -Wextra
          -Wno-deprecated ${CMAKE_CURRENT_SOURCE_DIR}/${hdrfile}
        COMMAND ${CMAKE_COMMAND} -E touch ${CMAKE_CURRENT_BINARY_DIR}/${STMPFILE}.stamp
        DEPENDS ${hdrfile}
      )
      add_custom_target(${STMPFILE}-check DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/${STMPFILE}.stamp)
      set_target_properties(${STMPFILE}-check PROPERTIES FOLDER "BRL-CAD Header Checks")

      add_dependencies(check-headers ${STMPFILE}-check)
    endif(NOT "${hdrfile}" STREQUAL "RtServerImpl.h")
  endforeach(hdrfile ${hdrfiles})
endif(DEFINED BRLCAD_HDR_CHECK AND NOT MSVC)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
