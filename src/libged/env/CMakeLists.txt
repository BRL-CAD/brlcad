include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_BINARY_DIR}
  ${BRLCAD_BINARY_DIR}/include
  ${BRLCAD_SOURCE_DIR}/include
  ${BU_INCLUDE_DIRS}
  ${GED_INCLUDE_DIRS}
)

set(ENV_CMD_C "${CMAKE_CURRENT_BINARY_DIR}/env_cmd.c")
add_custom_command(
  OUTPUT ${ENV_CMD_C}
  COMMAND env2c ${CMAKE_BINARY_DIR}/cmakefiles.cmake ${ENV_CMD_C}
  DEPENDS env2c
  COMMENT "[ENV2C] Scanning for environment variable usage"
)
distclean(${ENV_CMD_C})
set_source_files_properties(env.c PROPERTIES OBJECT_DEPENDS ${ENV_CMD_C})

ged_plugin_library(ged-env env.c)
target_link_libraries(ged-env libged libbu)

cmakefiles(
  CMakeLists.txt
  env.c
)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
