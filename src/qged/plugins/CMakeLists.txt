# QGED is set up based on a plugin architecture.

add_custom_command(
  OUTPUT "${CMAKE_BINARY_DIR}/qged_plugins.sentinel"
  COMMAND ${CMAKE_COMMAND} -E touch "${CMAKE_BINARY_DIR}/qged_plugins.sentinel"
  )
add_custom_target(qged_plugins_sentinel
  DEPENDS ${CMAKE_BINARY_DIR}/qged_plugins.sentinel
  )
set_target_properties(qged_plugins_sentinel PROPERTIES FOLDER "BRL-CAD Plugins/qged")

function(qged_plugin name)
  add_library(${name} ${ARGN})
  add_dependencies(qged_plugins_sentinel ${name})
endfunction(qged_plugin name)

# Convenience target to build all plugins
add_custom_target(qged_plugins ALL DEPENDS qged_plugins_sentinel)
set_target_properties(qged_plugins PROPERTIES FOLDER "BRL-CAD Plugins")

set(plugin_ignore_files
  CMakeLists.txt
  plugin.h
  )
CMAKEFILES(${plugin_ignore_files})


# Plugins
add_subdirectory(polygon)
add_subdirectory(view_info)
add_subdirectory(view_settings)


# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
