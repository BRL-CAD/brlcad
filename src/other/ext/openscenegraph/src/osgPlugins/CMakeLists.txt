if(NOT DYNAMIC_OPENSCENEGRAPH)
    add_definitions(-DOSG_LIBRARY_STATIC)
endif()


# Cache output directories
macro(CMAKE_CACHE_OUTPUT_DIRS)
if(NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_CACHED ${CMAKE_LIBRARY_OUTPUT_DIRECTORY})
  set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_CACHED ${CMAKE_ARCHIVE_OUTPUT_DIRECTORY})
  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_CACHED ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})
else(NOT CMAKE_CONFIGURATION_TYPES)
  foreach(CFG_TYPE ${CMAKE_CONFIGURATION_TYPES})
    string(TOUPPER "${CFG_TYPE}" CFG_TYPE_UPPER)
    set("CMAKE_LIBRARY_OUTPUT_DIRECTORY_${CFG_TYPE_UPPER}_CACHED" "${CMAKE_LIBRARY_OUTPUT_DIRECTORY_${CFG_TYPE_UPPER}}" )
    set("CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${CFG_TYPE_UPPER}_CACHED" "${CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${CFG_TYPE_UPPER}}")
    set("CMAKE_RUNTIME_OUTPUT_DIRECTORY_${CFG_TYPE_UPPER}_CACHED" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY_${CFG_TYPE_UPPER}}")
    set("CMAKE_BINARY_DIR_${CFG_TYPE_UPPER}_CACHED" "${CMAKE_BINARY_DIR_${CFG_TYPE_UPPER}}")
  endforeach()
endif(NOT CMAKE_CONFIGURATION_TYPES)
endmacro(CMAKE_CACHE_OUTPUT_DIRS)

# Restore output directories
macro(CMAKE_RESTORE_OUTPUT_DIRS)
if(NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_LIBRARY_OUTPUT_DIRECTORY_CACHED} CACHE INTERNAL "Single output directory for building all libraries.")
  set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_ARCHIVE_OUTPUT_DIRECTORY_CACHED} CACHE INTERNAL "Single output directory for building all archives.")
  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY_CACHED} CACHE INTERNAL "Single output directory for building all executables.")
else(NOT CMAKE_CONFIGURATION_TYPES)
  foreach(CFG_TYPE ${CMAKE_CONFIGURATION_TYPES})
    string(TOUPPER "${CFG_TYPE}" CFG_TYPE_UPPER)
    set("CMAKE_LIBRARY_OUTPUT_DIRECTORY_${CFG_TYPE_UPPER}" "${CMAKE_LIBRARY_OUTPUT_DIRECTORY_${CFG_TYPE_UPPER}_CACHED}"  CACHE INTERNAL "Single output directory for building ${CFG_TYPE} libraries.")
    set("CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${CFG_TYPE_UPPER}" "${CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${CFG_TYPE_UPPER}_CACHED}" CACHE INTERNAL "Single output directory for building ${CFG_TYPE} archives.")
    set("CMAKE_RUNTIME_OUTPUT_DIRECTORY_${CFG_TYPE_UPPER}" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY_${CFG_TYPE_UPPER}_CACHED}" CACHE INTERNAL "Single output directory for building ${CFG_TYPE} executables.")
    set("CMAKE_BINARY_DIR_${CFG_TYPE_UPPER}" "${CMAKE_BINARY_DIR_${CFG_TYPE_UPPER}_CACHED}" CACHE INTERNAL "Toplevel binary dir for ${CFG_TYPE} building.")
  endforeach()
endif(NOT CMAKE_CONFIGURATION_TYPES)
endmacro(CMAKE_RESTORE_OUTPUT_DIRS)

#---------------------------------------------------------------------
# Output directories - this is where built library and executable
# files will be placed after building but prior to install.  The
# necessary variables change between single and multi configuration
# build systems, so it is necessary to handle both cases on a
# conditional basis.
macro(CMAKE_SET_OUTPUT_DIRS)
  if(NOT CMAKE_CONFIGURATION_TYPES)
    # If we're not doing multi-configuration, just set the three main
    # variables to the correct values.
    if(NOT CMAKE_LIBRARY_OUTPUT_DIRECTORY)
      set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${${PROJECT_NAME}_BINARY_DIR}/${LIB_DIR} CACHE INTERNAL "Single output directory for building all libraries.")
    endif(NOT CMAKE_LIBRARY_OUTPUT_DIRECTORY)
    if(NOT CMAKE_ARCHIVE_OUTPUT_DIRECTORY)
      set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${${PROJECT_NAME}_BINARY_DIR}/${LIB_DIR} CACHE INTERNAL "Single output directory for building all archives.")
    endif(NOT CMAKE_ARCHIVE_OUTPUT_DIRECTORY)
    if(NOT CMAKE_RUNTIME_OUTPUT_DIRECTORY)
      set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${${PROJECT_NAME}_BINARY_DIR}/${BIN_DIR} CACHE INTERNAL "Single output directory for building all executables.")
    endif(NOT CMAKE_RUNTIME_OUTPUT_DIRECTORY)
  else(NOT CMAKE_CONFIGURATION_TYPES)
    # Multi-configuration is more difficult.  Not only do we need to
    # properly set the output directories, but we also need to
    # identify the "toplevel" directory for each configuration so
    # we can place files, documentation, etc. in the correct
    # relative positions.  Because files may be placed by CMake
    # without a build target to put them in their proper relative build
    # directory position using these paths, we must fully qualify them
    # without using CMAKE_CFG_INTDIR.
    #
    # We define directories that may not be quite "standard"
    # for a particular build tool - for example, native VS2010 projects use
    # another directory to denote CPU type being compiled for - but CMake only
    # supports multi-configuration setups having multiple configurations,
    # not multiple compilers.
    #
    # One additional wrinkle we must watch for here is the case where
    # a multi-configuration setup uses "." for its internal directory -
    # if that's the case, we need to just set the various config output
    # directories to the same value.
    set(CFG_ROOT ${${PROJECT_NAME}_BINARY_DIR})
    foreach(CFG_TYPE ${CMAKE_CONFIGURATION_TYPES})
      if(NOT "${CMAKE_CFG_INTDIR}" STREQUAL ".")
	set(CFG_ROOT ${${PROJECT_NAME}_BINARY_DIR}/${CFG_TYPE})
      endif(NOT "${CMAKE_CFG_INTDIR}" STREQUAL ".")
      string(TOUPPER "${CFG_TYPE}" CFG_TYPE_UPPER)
      if(NOT "CMAKE_LIBRARY_OUTPUT_DIRECTORY_${CFG_TYPE_UPPER}")
	set("CMAKE_LIBRARY_OUTPUT_DIRECTORY_${CFG_TYPE_UPPER}" ${CFG_ROOT}/${LIB_DIR} CACHE INTERNAL "Single output directory for building ${CFG_TYPE} libraries.")
      endif(NOT "CMAKE_LIBRARY_OUTPUT_DIRECTORY_${CFG_TYPE_UPPER}")
      if(NOT "CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${CFG_TYPE_UPPER}")
	set("CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${CFG_TYPE_UPPER}" ${CFG_ROOT}/${LIB_DIR} CACHE INTERNAL "Single output directory for building ${CFG_TYPE} archives.")
      endif(NOT "CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${CFG_TYPE_UPPER}")
      if(NOT "CMAKE_RUNTIME_OUTPUT_DIRECTORY_${CFG_TYPE_UPPER}")
	set("CMAKE_RUNTIME_OUTPUT_DIRECTORY_${CFG_TYPE_UPPER}" ${CFG_ROOT}/${BIN_DIR} CACHE INTERNAL "Single output directory for building ${CFG_TYPE} executables.")
      endif(NOT "CMAKE_RUNTIME_OUTPUT_DIRECTORY_${CFG_TYPE_UPPER}")
      if(NOT "CMAKE_BINARY_DIR_${CFG_TYPE_UPPER}")
	set("CMAKE_BINARY_DIR_${CFG_TYPE_UPPER}" ${CFG_ROOT} CACHE INTERNAL "Toplevel binary dir for ${CFG_TYPE} building.")
      endif(NOT "CMAKE_BINARY_DIR_${CFG_TYPE_UPPER}")
      if(NOT "${PROJECT_NAME}_BINARY_DIR_${CFG_TYPE_UPPER}")
	set("${PROJECT_NAME}_BINARY_DIR_${CFG_TYPE_UPPER}" ${CFG_ROOT} CACHE INTERNAL "Toplevel binary dir for ${CFG_TYPE} building.")
      endif(NOT "${PROJECT_NAME}_BINARY_DIR_${CFG_TYPE_UPPER}")
    endforeach()
  endif(NOT CMAKE_CONFIGURATION_TYPES)
endmacro(CMAKE_SET_OUTPUT_DIRS)


set(LIB_DIR ${LIB_DIR}/osgPlugins)

CMAKE_CACHE_OUTPUT_DIRS()

set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "")

CMAKE_SET_OUTPUT_DIRS()

SET(CMAKE_SHARED_MODULE_PREFIX ${OSG_PLUGIN_PREFIX})
add_subdirectory(osg)
add_subdirectory(png)
if(FREETYPE_FOUND)
    add_subdirectory(freetype)
endif()

CMAKE_RESTORE_OUTPUT_DIRS()
