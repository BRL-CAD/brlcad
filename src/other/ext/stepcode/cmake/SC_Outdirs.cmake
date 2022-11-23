# in its own file so that it can be used by the schema scanner as well
if(NOT DEFINED CMAKE_LIBRARY_OUTPUT_DIRECTORY)
  set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${SC_BINARY_DIR}/lib CACHE INTERNAL "Single output directory for building all libraries.")
endif(NOT DEFINED CMAKE_LIBRARY_OUTPUT_DIRECTORY)
if(NOT DEFINED CMAKE_ARCHIVE_OUTPUT_DIRECTORY)
  set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${SC_BINARY_DIR}/lib CACHE INTERNAL "Single output directory for building all archives.")
endif(NOT DEFINED CMAKE_ARCHIVE_OUTPUT_DIRECTORY)
if(NOT DEFINED CMAKE_RUNTIME_OUTPUT_DIRECTORY)
  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${SC_BINARY_DIR}/bin CACHE INTERNAL "Single output directory for building all executables.")
endif(NOT DEFINED CMAKE_RUNTIME_OUTPUT_DIRECTORY)
foreach(CFG_TYPE ${CMAKE_CONFIGURATION_TYPES})
  string(TOUPPER "${CFG_TYPE}" CFG_TYPE)
  if(NOT "CMAKE_LIBRARY_OUTPUT_DIRECTORY_${CFG_TYPE}")
    set("CMAKE_LIBRARY_OUTPUT_DIRECTORY_${CFG_TYPE}" ${SC_BINARY_DIR}/lib CACHE INTERNAL "Single output directory for building all libraries.")
  endif(NOT "CMAKE_LIBRARY_OUTPUT_DIRECTORY_${CFG_TYPE}")
  if(NOT "CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${CFG_TYPE}")
    set("CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${CFG_TYPE}" ${SC_BINARY_DIR}/lib CACHE INTERNAL "Single output directory for building all archives.")
  endif(NOT "CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${CFG_TYPE}")
  if(NOT "CMAKE_RUNTIME_OUTPUT_DIRECTORY_${CFG_TYPE}")
    set("CMAKE_RUNTIME_OUTPUT_DIRECTORY_${CFG_TYPE}" ${SC_BINARY_DIR}/bin CACHE INTERNAL "Single output directory for building all executables.")
  endif(NOT "CMAKE_RUNTIME_OUTPUT_DIRECTORY_${CFG_TYPE}")
endforeach()

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8

