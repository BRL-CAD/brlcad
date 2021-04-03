add_subdirectory(analyze)
add_subdirectory(bu)
add_subdirectory(bn)
add_subdirectory(bg)
add_subdirectory(brep)
add_subdirectory(bview)
add_subdirectory(dm)
add_subdirectory(gcv)
add_subdirectory(ged)
add_subdirectory(icv)
add_subdirectory(nmg)
add_subdirectory(optical)
add_subdirectory(rt)

set(public_hdrs
  RtServerImpl.h
  analyze.h
  bg.h
  bio.h
  bn.h
  bnetwork.h
  brep.h
  bresource.h
  brlcad.h
  brlcad.i
  bsocket.h
  bu.h
  bview.h
  common.h
  dm.h
  fft.h
  gcv.h
  ged.h
  icv.h
  libtermio.h
  nmg.h
  optical.h
  pc.h
  photonmap.h
  pinttypes.h
  pkg.h
  pstdint.h
  raytrace.h
  rtserver.h
  tclcad.h
  vmath.h
  wdb.h
  )
BRLCAD_MANAGE_FILES("${public_hdrs}" ${INCLUDE_DIR}/brlcad)

BUILD_CFG_HDR(brlcad_ident.h.in ${INCLUDE_DIR}/brlcad)
BUILD_CFG_HDR(brlcad_version.h.in ${INCLUDE_DIR}/brlcad)

# FIXME: shouldn't exist, needs to be absorbed into brlcad_config.h
if(MSVC)
  BUILD_CFG_HDR(config_win.h.in ${INCLUDE_DIR}/brlcad)
endif(MSVC)

set(include_misc
  CMakeLists.txt
  brlcad_ident.h.in
  brlcad_version.h.in
  conf/BrlcadConfig.tmpl
  conf/MAJOR
  conf/MINOR
  conf/PATCH
  conf/make.vbs
  config_win.h.in
  xxhash.h
  )
CMAKEFILES(${include_misc})

# Header checking target - this compiles the headers as isolated files to
# ensure they include what is needed to resolve any symbols defined in the
# public API.

# Note: don't know how to do this on Windows - skip with Visual Studio
if (DEFINED BRLCAD_HDR_CHECK AND NOT MSVC)

  add_custom_target(check-headers)
  set_target_properties(check-headers PROPERTIES FOLDER "BRL-CAD Header Checks")

  file(GLOB_RECURSE hdrfiles RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}" "*.h")

  foreach(hdrfile ${hdrfiles})

    if (NOT "${hdrfile}" STREQUAL "RtServerImpl.h")

      string(REPLACE "-" "_" STMPFILE "${hdrfile}")
      string(REPLACE "." "_" STMPFILE "${STMPFILE}")
      string(REPLACE "/" "_" STMPFILE "${STMPFILE}")

      add_custom_command(
	OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${STMPFILE}.stamp
	COMMAND ${CMAKE_CXX_COMPILER} -I${CMAKE_SOURCE_DIR}/include -I${CMAKE_BINARY_DIR}/include
	-I${CMAKE_SOURCE_DIR}/src/other/openNURBS -I${CMAKE_SOURCE_DIR}/src/other/tcl/generic
	-fsyntax-only -Wall -Wextra -Wno-deprecated
	${CMAKE_CURRENT_SOURCE_DIR}/${hdrfile}
	COMMAND ${CMAKE_COMMAND} -E touch ${CMAKE_CURRENT_BINARY_DIR}/${STMPFILE}.stamp
	DEPENDS ${hdrfile}
	)
      add_custom_target(${STMPFILE}-check DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/${STMPFILE}.stamp)
      set_target_properties(${STMPFILE}-check PROPERTIES FOLDER "BRL-CAD Header Checks")

      add_dependencies(check-headers ${STMPFILE}-check)

    endif (NOT "${hdrfile}" STREQUAL "RtServerImpl.h")

  endforeach(hdrfile ${hdrfiles})

endif (DEFINED BRLCAD_HDR_CHECK AND NOT MSVC)



# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
