# env2c - Scanner to generate a list of environment variables used by
# source code
add_subdirectory(env2c)
settargetfolder(env2c "Compilation Utilities")
distclean(${CMAKE_CURRENT_BINARY_DIR}/env2c/Makefile)

set(ENV_CMD_C "${CMAKE_CURRENT_BINARY_DIR}/env_cmd.c")
add_custom_command(
  OUTPUT ${ENV_CMD_C}
  COMMAND env2c ${CMAKE_BINARY_DIR}/cmakefiles.cmake ${ENV_CMD_C}
  DEPENDS env2c
  COMMENT "[ENV2C] Scanning for environment variable usage"
)
distclean(${ENV_CMD_C})
set_source_files_properties(env.c PROPERTIES OBJECT_DEPENDS ${ENV_CMD_C})

set(PLUGIN_LIBS libged libbu)

ged_plugin_library(ged-env
  env.c
  INCLUDES ${CMAKE_CURRENT_BINARY_DIR}
  LIBS ${PLUGIN_LIBS}
  PLUGIN_ONLY
)

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
