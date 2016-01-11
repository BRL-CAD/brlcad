function(GET_SOURCE_DIRECTORIES result)
    set(dirlist "")
    file(GLOB cmakelist_files RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} */CMakeLists.txt)

    foreach(file ${cmakelist_files})
        get_filename_component(parent_dir "${file}" DIRECTORY)
        list(APPEND dirlist ${parent_dir})
    endforeach(file ${cmakelist_files})

    set (${result} ${dirlist} PARENT_SCOPE)
endfunction(GET_SOURCE_DIRECTORIES result)


GET_SOURCE_DIRECTORIES(plugin_directories)

foreach(plugin_directory ${plugin_directories})
    add_subdirectory(${plugin_directory})
endforeach(plugin_directory ${plugin_directories})
