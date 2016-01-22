add_custom_target(libgcv_plugins)

function(LIBGCV_ADD_PLUGIN plugin_name sources libs)
    set(target_name libgcv_${plugin_name})
    BRLCAD_ADDLIB(${target_name} "${sources}" "${libs}" SHARED NO_INSTALL FOLDER "LibGCV Plugins")
    target_include_directories(${target_name} PRIVATE "${GCV_INCLUDE_DIRS}")
    get_property(current_output_dir TARGET ${target_name} PROPERTY LIBRARY_OUTPUT_DIRECTORY)
    set_property(TARGET ${target_name} PROPERTY LIBRARY_OUTPUT_DIRECTORY "${current_output_dir}/${LIBGCV_PLUGINS_DIRECTORY}")
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
        get_filename_component(plugin_directory "${file}" DIRECTORY)
        add_subdirectory(${plugin_directory})
    endforeach(file ${cmakelist_files})
endfunction(GET_LIBGCV_PLUGINS)


GET_LIBGCV_PLUGINS()
