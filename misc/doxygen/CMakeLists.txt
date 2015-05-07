set(lib_inputs
"INPUT += ${CMAKE_SOURCE_DIR}/include/common.h"
)
set(lib_predefines)
set(DOX_LIBS
  common
  libbu
  libbn
  libgm
  libsysv
  libbrep
  librt
  libwdb
  libpkg
  libfb
  libgcv
  libanalyze
  libged
  liboptical
  libcursor
  libdm
  libfft
  libicv
  libmultispectral
  libpc
  libtclcad
  libtermio
  )
foreach(libname ${DOX_LIBS})
  set(doxlibname ${libname})
  string(REPLACE "lib" "" sublibname ${libname})
  string(TOUPPER ${sublibname} upper_name)

  # Build local input list
  set(INPUTS "
INPUT += ${CMAKE_SOURCE_DIR}/misc/doxygen/${libname}.dox
INPUT += ${CMAKE_SOURCE_DIR}/include/common.h")
  if(EXISTS ${CMAKE_SOURCE_DIR}/include/${sublibname}/)
    set(INPUTS "${INPUTS}
INPUT += ${CMAKE_SOURCE_DIR}/include/${sublibname}/")
  endif(EXISTS ${CMAKE_SOURCE_DIR}/include/${sublibname}/)
  if(EXISTS ${CMAKE_SOURCE_DIR}/include/${sublibname}.h)
    set(INPUTS "${INPUTS}
INPUT += ${CMAKE_SOURCE_DIR}/include/${sublibname}.h
")
  elseif("${libname}" STREQUAL "librt")
    set(INPUTS "${INPUTS}
INPUT += ${CMAKE_SOURCE_DIR}/include/raytrace.h
")
  endif(EXISTS ${CMAKE_SOURCE_DIR}/include/${sublibname}.h)
if(DEFINED BRLCAD_DOXYGEN_ADD_SRCS)
  set(INPUTS "${INPUTS}
INPUT += ${CMAKE_SOURCE_DIR}/src/${libname}/")
endif(DEFINED BRLCAD_DOXYGEN_ADD_SRCS)

set(PREDEFINES "
PREDEFINED += \"${upper_name}_EXTERN(type_and_name,args)=extern type_and_name args\"
PREDEFINED += \"${upper_name}_EXPORT=\"
")

  # Build top level input list
  set(lib_inputs "${lib_inputs}
INPUT += ${CMAKE_SOURCE_DIR}/misc/doxygen/${libname}.dox")
  if(EXISTS ${CMAKE_SOURCE_DIR}/include/${sublibname}/)
    set(lib_inputs "${lib_inputs}
INPUT += ${CMAKE_SOURCE_DIR}/include/${sublibname}/")
  endif(EXISTS ${CMAKE_SOURCE_DIR}/include/${sublibname}/)
  if(EXISTS ${CMAKE_SOURCE_DIR}/include/${sublibname}.h)
    set(lib_inputs "${lib_inputs}
INPUT += ${CMAKE_SOURCE_DIR}/include/${sublibname}.h
")
  elseif("${libname}" STREQUAL "librt")
    set(lib_inputs "${lib_inputs}
INPUT += ${CMAKE_SOURCE_DIR}/include/raytrace.h
")
  endif(EXISTS ${CMAKE_SOURCE_DIR}/include/${sublibname}.h)
if(DEFINED BRLCAD_DOXYGEN_ADD_SRCS)
  set(lib_inputs "${lib_inputs}
INPUT += ${CMAKE_SOURCE_DIR}/src/${libname}/")
endif(DEFINED BRLCAD_DOXYGEN_ADD_SRCS)

set(lib_predefines "${lib_predefines}
PREDEFINED += \"${upper_name}_EXTERN(type_and_name,args)=extern type_and_name args\"
PREDEFINED += \"${upper_name}_EXPORT=\"
")


  configure_file(Doxyfile.in ${CMAKE_BINARY_DIR}/CMakeTmp/Doxyfile_${libname} @ONLY)
  add_custom_command(
    OUTPUT ${CMAKE_BINARY_DIR}/doc/doxygen_${libname}/html/index.html
    COMMAND ${DOXYGEN_EXECUTABLE} ${CMAKE_BINARY_DIR}/CMakeTmp/Doxyfile_${libname}
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/doc
    )
  add_custom_target(dox-${libname} DEPENDS ${CMAKE_BINARY_DIR}/doc/doxygen_${libname}/html/index.html)
  CMAKEFILES(${libname}.dox)
endforeach(libname ${DOX_LIBS})

set(doxlibname output)
set(INPUTS ${lib_inputs})
set(PREDEFINES ${lib_predefines})
configure_file(Doxyfile.in ${CMAKE_BINARY_DIR}/CMakeTmp/Doxyfile @ONLY)
add_custom_command(
  OUTPUT ${CMAKE_BINARY_DIR}/doc/doxygen_output/html/index.html
  COMMAND ${DOXYGEN_EXECUTABLE} ${CMAKE_BINARY_DIR}/CMakeTmp/Doxyfile
  WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/doc
  )
add_custom_target(dox     DEPENDS ${CMAKE_BINARY_DIR}/doc/doxygen_output/html/index.html)
add_custom_target(doxygen DEPENDS ${CMAKE_BINARY_DIR}/doc/doxygen_output/html/index.html)

set(doxygen_ignore_files
  Doxyfile.in
  intro.dox
  )
CMAKEFILES(${doxygen_ignore_files})

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
