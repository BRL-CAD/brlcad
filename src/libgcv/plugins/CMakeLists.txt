add_custom_command(
  OUTPUT "${CMAKE_BINARY_DIR}/gcv_plugins.sentinel"
  COMMAND ${CMAKE_COMMAND} -E touch "${CMAKE_BINARY_DIR}/gcv_plugins.sentinel"
  )
add_custom_target(gcv_plugins_sentinel
  DEPENDS ${CMAKE_BINARY_DIR}/gcv_plugins.sentinel
  )
set_target_properties(gcv_plugins_sentinel PROPERTIES FOLDER "BRL-CAD Plugins/gcv")

function(gcv_plugin_library name)
  add_library(${name} ${ARGN})
  add_dependencies(gcv_plugins_sentinel ${name})
endfunction(gcv_plugin_library name)

# This target is supplied so applications wanting to run gcv commands at build
# time can depend on the plugins being built as well as libgcv itself (otherwise
# gcv commands might fail due to their plugins not yet having been built.)
add_custom_target(gcv_plugins ALL DEPENDS gcv_plugins_sentinel)
set_target_properties(gcv_plugins PROPERTIES FOLDER "BRL-CAD Plugins")

add_subdirectory(asc)
add_subdirectory(decimate)
add_subdirectory(fastgen4)
add_subdirectory(gdal)
add_subdirectory(obj)
add_subdirectory(rhino)
add_subdirectory(stl)
add_subdirectory(vol)
add_subdirectory(vrml)

CMAKEFILES(
  CMakeLists.txt
  )

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8

