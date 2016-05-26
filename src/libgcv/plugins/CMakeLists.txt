add_custom_target(libgcv_plugins)
set_target_properties(libgcv_plugins PROPERTIES FOLDER "BRL-CAD Shared Libraries")


function(LIBGCV_ADD_PLUGIN plugin_name sources libs)
  set(target_name libgcv_${plugin_name})
  BRLCAD_ADDLIB(${target_name} "${sources}" "libgcv;${libs}" SHARED NO_INSTALL FOLDER "LibGCV Plugins")
  # TODO - not sure if a straight-up include_directories will work, but if not
  # we're going to have to up our minimum required version of CMake to >=2.8.12
  if("${CMAKE_VERSION}" VERSION_GREATER 2.8.11)
    target_include_directories(${target_name} PRIVATE "${GCV_INCLUDE_DIRS}")
  else("${CMAKE_VERSION}" VERSION_GREATER 2.8.11)
    include_directories("${GCV_INCLUDE_DIRS}")
  endif("${CMAKE_VERSION}" VERSION_GREATER 2.8.11)

  if(NOT CMAKE_CONFIGURATION_TYPES)
    get_property(current_output_dir TARGET ${target_name} PROPERTY LIBRARY_OUTPUT_DIRECTORY)
    set_property(TARGET ${target_name} PROPERTY LIBRARY_OUTPUT_DIRECTORY "${current_output_dir}/${LIBGCV_PLUGINS_DIRECTORY}")
    # Since we're installing to LIB_DIR in all cases, we need to override all of these...
    set_property(TARGET ${target_name} PROPERTY RUNTIME_OUTPUT_DIRECTORY "${current_output_dir}/${LIBGCV_PLUGINS_DIRECTORY}")
    set_property(TARGET ${target_name} PROPERTY ARCHIVE_OUTPUT_DIRECTORY "${current_output_dir}/${LIBGCV_PLUGINS_DIRECTORY}")
  else(NOT CMAKE_CONFIGURATION_TYPES)
    set(CFG_ROOT ${${PROJECT_NAME}_BINARY_DIR})
    foreach(CFG_TYPE ${CMAKE_CONFIGURATION_TYPES})
      if(NOT "${CMAKE_CFG_INTDIR}" STREQUAL ".")
	set(CFG_ROOT ${${PROJECT_NAME}_BINARY_DIR}/${CFG_TYPE})
      endif(NOT "${CMAKE_CFG_INTDIR}" STREQUAL ".")
      string(TOUPPER "${CFG_TYPE}" CFG_TYPE_UPPER)
      get_property(current_output_dir TARGET ${target_name} PROPERTY LIBRARY_OUTPUT_DIRECTORY_${CFG_TYPE_UPPER})
      set_property(TARGET ${target_name} PROPERTY LIBRARY_OUTPUT_DIRECTORY_${CFG_TYPE_UPPER} "${current_output_dir}/${LIBGCV_PLUGINS_DIRECTORY}")
      # Since we're installing to LIB_DIR in all cases, we need to override all of these...
      set_property(TARGET ${target_name} PROPERTY RUNTIME_OUTPUT_DIRECTORY_${CFG_TYPE_UPPER} "${current_output_dir}/${LIBGCV_PLUGINS_DIRECTORY}")
      set_property(TARGET ${target_name} PROPERTY ARCHIVE_OUTPUT_DIRECTORY_${CFG_TYPE_UPPER} "${current_output_dir}/${LIBGCV_PLUGINS_DIRECTORY}")
    endforeach(CFG_TYPE ${CMAKE_CONFIGURATION_TYPES})
  endif(NOT CMAKE_CONFIGURATION_TYPES)

  # We're using GCV_EXPORT for these plugins, so we need to set the DLL_EXPORTS accordingly
  if(CPP_DLL_DEFINES)
    set_property(TARGET ${target_name} APPEND PROPERTY COMPILE_DEFINITIONS "GCV_DLL_EXPORTS")
  endif(CPP_DLL_DEFINES)

  install(TARGETS ${target_name}
    RUNTIME DESTINATION ${LIB_DIR}/${LIBGCV_PLUGINS_DIRECTORY}
    LIBRARY DESTINATION ${LIB_DIR}/${LIBGCV_PLUGINS_DIRECTORY}
    ARCHIVE DESTINATION ${LIB_DIR}/${LIBGCV_PLUGINS_DIRECTORY}
    )

  add_dependencies(libgcv_plugins ${target_name})
endfunction(LIBGCV_ADD_PLUGIN plugin_name sources libs)


function(GET_LIBGCV_PLUGINS)
  file(GLOB cmakelist_files RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} */CMakeLists.txt)

  foreach(file ${cmakelist_files})
    get_filename_component(plugin_directory "${file}" PATH)
    add_subdirectory(${plugin_directory})
  endforeach(file ${cmakelist_files})
endfunction(GET_LIBGCV_PLUGINS)


GET_LIBGCV_PLUGINS()

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8

