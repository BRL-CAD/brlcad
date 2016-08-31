set(lib_inputs "INPUT += \"${CMAKE_SOURCE_DIR}/include/common.h\"")
set(lib_inputs "${lib_inputs}
INPUT += \"${CMAKE_BINARY_DIR}/CMakeTmp/features.dox\"")
set(lib_inputs "${lib_inputs}
INPUT += \"${CMAKE_SOURCE_DIR}/misc/doxygen/vmath.dox\"")
set(lib_inputs "${lib_inputs}
INPUT += \"${CMAKE_SOURCE_DIR}/include/vmath.h\"")
set(lib_inputs "${lib_inputs}
INPUT += \"${CMAKE_SOURCE_DIR}/misc/doxygen/bview.dox\"")
set(lib_inputs "${lib_inputs}
INPUT += \"${CMAKE_SOURCE_DIR}/include/dm/bview.h\"")
set(lib_inputs "${lib_inputs}
INPUT += \"${CMAKE_SOURCE_DIR}/include/bio.h\"")
set(lib_inputs "${lib_inputs}
INPUT += \"${CMAKE_SOURCE_DIR}/include/bnetwork.h\"")
set(lib_inputs "${lib_inputs}
INPUT += \"${CMAKE_SOURCE_DIR}/include/bresource.h\"")
set(lib_inputs "${lib_inputs}
INPUT += \"${CMAKE_SOURCE_DIR}/include/bsocket.h\"")

set(lib_predefines)

# Note: the ordering of libraries in this list is
# what controls the ordering in the Modules output
# from Doxygen
set(DOX_LIBS
  common
  libbu
  libbn
  libbg
  libbrep
  libfft
  libicv
  libpc
  libwdb
  librt
  liboptical
  libmultispectral
  libgcv
  libanalyze
  libged
  libfb
  libdm
  libtclcad
  libpkg
  libtermio
  libcursor
  )

foreach(libname ${DOX_LIBS})
  set(doxlibname ${libname})
  string(REPLACE "lib" "" sublibname ${libname})
  string(TOUPPER ${sublibname} upper_name)

  # Build local input list
  set(INPUTS "
INPUT += \"${CMAKE_SOURCE_DIR}/misc/doxygen/${libname}.dox\"")
  if(EXISTS "${CMAKE_SOURCE_DIR}/include/${sublibname}/")
    set(INPUTS "${INPUTS}
INPUT += \"${CMAKE_SOURCE_DIR}/include/${sublibname}/\"")
  endif(EXISTS "${CMAKE_SOURCE_DIR}/include/${sublibname}/")
  if(EXISTS "${CMAKE_SOURCE_DIR}/include/${sublibname}.h")
    set(INPUTS "${INPUTS}
INPUT += \"${CMAKE_SOURCE_DIR}/include/${sublibname}.h\"
    ")
  endif(EXISTS "${CMAKE_SOURCE_DIR}/include/${sublibname}.h")

  # libbn header doesn't fit the template
  if("${libname}" STREQUAL "libbn")
  set(INPUTS "${INPUTS}
INPUT += \"${CMAKE_SOURCE_DIR}/include/bn/vector_fpu.h\"
    ")
    set(INPUTS "${INPUTS}
INPUT += \"${CMAKE_SOURCE_DIR}/include/bn/dvec.h\"
    ")
  endif("${libname}" STREQUAL "libbn")

  # librt header doesn't fit the template
  if("${libname}" STREQUAL "librt")
    set(INPUTS "${INPUTS}
INPUT += \"${CMAKE_SOURCE_DIR}/include/raytrace.h\"
    ")
  set(INPUTS "${INPUTS}
INPUT += \"${CMAKE_SOURCE_DIR}/include/nmg.h\"
    ")
  endif("${libname}" STREQUAL "librt")

  # libmultispectral header doesn't fit the template
  if("${libname}" STREQUAL "libmultispectral")
    set(INPUTS "${INPUTS}
INPUT += \"${CMAKE_SOURCE_DIR}/include/spectrum.h\"
    ")
  endif("${libname}" STREQUAL "libmultispectral")

  # libtermio header doesn't fit the template
  if("${libname}" STREQUAL "libtermio")
    set(INPUTS "${INPUTS}
INPUT += \"${CMAKE_SOURCE_DIR}/include/libtermio.h\"
    ")
  endif("${libname}" STREQUAL "libtermio")

  if(DEFINED BRLCAD_DOXYGEN_ADD_SRCS)
    set(INPUTS "${INPUTS}
INPUT += \"${CMAKE_SOURCE_DIR}/src/${libname}/\"")
  endif(DEFINED BRLCAD_DOXYGEN_ADD_SRCS)

  set(PREDEFINES "
PREDEFINED += \"${upper_name}_EXTERN(type_and_name,args)=extern type_and_name args\"
PREDEFINED += \"${upper_name}_EXPORT=\"
  ")

  # Build top level input list
  set(lib_inputs "${lib_inputs}
INPUT += \"${CMAKE_SOURCE_DIR}/misc/doxygen/${libname}.dox\"")
  if(EXISTS "${CMAKE_SOURCE_DIR}/include/${sublibname}/")
    set(lib_inputs "${lib_inputs}
INPUT += \"${CMAKE_SOURCE_DIR}/include/${sublibname}/\"")
  endif(EXISTS "${CMAKE_SOURCE_DIR}/include/${sublibname}/")
  if(EXISTS "${CMAKE_SOURCE_DIR}/include/${sublibname}.h")
    set(lib_inputs "${lib_inputs}
INPUT += \"${CMAKE_SOURCE_DIR}/include/${sublibname}.h\"
    ")
  endif(EXISTS "${CMAKE_SOURCE_DIR}/include/${sublibname}.h")

  # librt header doesn't fit the template
  if("${libname}" STREQUAL "librt")
    set(lib_inputs "${lib_inputs}
INPUT += \"${CMAKE_SOURCE_DIR}/include/raytrace.h\"
    ")
    set(lib_inputs "${lib_inputs}
INPUT += \"${CMAKE_SOURCE_DIR}/include/nmg.h\"
    ")
  endif("${libname}" STREQUAL "librt")

  # libmultispectral header doesn't fit the template
  if("${libname}" STREQUAL "libmultispectral")
    set(lib_inputs "${lib_inputs}
INPUT += \"${CMAKE_SOURCE_DIR}/include/spectrum.h\"
    ")
  endif("${libname}" STREQUAL "libmultispectral")

  # libtermio header doesn't fit the template
  if("${libname}" STREQUAL "libtermio")
    set(lib_inputs "${lib_inputs}
INPUT += \"${CMAKE_SOURCE_DIR}/include/libtermio.h\"
    ")
  endif("${libname}" STREQUAL "libtermio")

  if(DEFINED BRLCAD_DOXYGEN_ADD_SRCS)
    set(lib_inputs "${lib_inputs}
INPUT += \"${CMAKE_SOURCE_DIR}/src/${libname}/\"")
  endif(DEFINED BRLCAD_DOXYGEN_ADD_SRCS)

  set(lib_predefines "${lib_predefines}
PREDEFINED += \"${upper_name}_EXTERN(type_and_name,args)=extern type_and_name args\"
PREDEFINED += \"${upper_name}_EXPORT=\"
  ")


  configure_file(Doxyfile.in "${CMAKE_BINARY_DIR}/CMakeTmp/Doxyfile_${libname}" @ONLY)
  add_custom_command(
    OUTPUT "${CMAKE_BINARY_DIR}/doc/doxygen_${libname}/html/index.html"
    COMMAND "${DOXYGEN_EXECUTABLE}" "${CMAKE_BINARY_DIR}/CMakeTmp/Doxyfile_${libname}"
    WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/doc"
    )
  add_custom_target(dox-${libname} DEPENDS "${CMAKE_BINARY_DIR}/doc/doxygen_${libname}/html/index.html")
  CMAKEFILES(${libname}.dox)
endforeach(libname ${DOX_LIBS})

set(doxlibname output)
set(INPUTS ${lib_inputs})
set(PREDEFINES ${lib_predefines})
configure_file(Doxyfile.in "${CMAKE_BINARY_DIR}/CMakeTmp/Doxyfile" @ONLY)
add_custom_command(
  OUTPUT "${CMAKE_BINARY_DIR}/doc/doxygen_output/html/index.html"
  COMMAND "${DOXYGEN_EXECUTABLE}" "${CMAKE_BINARY_DIR}/CMakeTmp/Doxyfile"
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/doc"
  )
add_custom_target(dox     DEPENDS "${CMAKE_BINARY_DIR}/doc/doxygen_output/html/index.html")
add_custom_target(doxygen DEPENDS "${CMAKE_BINARY_DIR}/doc/doxygen_output/html/index.html")

set(doxygen_ignore_files
  Doxyfile.in
  intro.dox
  bview.dox
  vmath.dox
  images
  )
CMAKEFILES(${doxygen_ignore_files})

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
