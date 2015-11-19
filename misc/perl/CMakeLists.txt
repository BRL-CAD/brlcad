set(misc_ignore_files rt.i)
CMAKEFILES(${misc_ignore_files})

# see p. 48 in CMake book
if(SWIG_FOUND AND NOT "${SWIG_USE_FILE}" STREQUAL "")

  set(CMAKE_SWIG_FLAGS "")

  #add_library(swiglib MODULE IMPORTED)

  include(${SWIG_USE_FILE})

  #set(swig_SRC
  #  ${CMAKE_SOURCE_DIR}/misc/perl/rt.i
  #)

#  include_directories(/usr/lib/gcc/x86_64-linux-gnu/4.4/include)
#  include_directories(/usr/include)
#  include_directories(/usr/include/linux)
#  include_directories(${CMAKE_SOURCE_DIR}/include)
#  include_directories(${CMAKE_SOURCE_DIR}/src/other/tcl/generic)

  set_source_files_properties(rt.i
    PROPERTIES SWIG_FLAGS "-includeall"
  )

  #swig_add_module(RT perl ${swig_SRC})

  #message("Swig module: \"${SWIG_MODULE_${RT}_REAL_NAME}\"")

endif(SWIG_FOUND AND NOT "${SWIG_USE_FILE}" STREQUAL "")

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
