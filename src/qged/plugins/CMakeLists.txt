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
  IPlugin.h
  )
CMAKEFILES(${plugin_ignore_files})


# Plugins
add_subdirectory(polygon)
add_subdirectory(view_info)
add_subdirectory(view_settings)
add_subdirectory(hierarchy)

# TODO - look into primitive editing plugins.  Things to check include:
#
# preparing selectable "schematic" 2D graphics with
# https://doc.qt.io/qt-5/qgraphicsitem.html
#
# .g object aware QLineEdit:
# https://doc.qt.io/qt-5/qlineedit.html
# https://doc.qt.io/qt-5/qvalidator.html
# https://doc.qt.io/qt-5/stylesheet-examples.html
#
# Sketch editor - will need some variation on Archer's abilities, which will
# involve a custom 2D widget.  Possibly useful resources:
# https://github.com/peizhan/psketcher
# https://github.com/cmerrill/sketchsolve

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
