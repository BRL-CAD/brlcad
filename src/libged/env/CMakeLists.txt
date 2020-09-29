include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_BINARY_DIR}
  ${BRLCAD_BINARY_DIR}/include
  ${BRLCAD_SOURCE_DIR}/include
  ${BU_INCLUDE_DIRS}
  ${GED_INCLUDE_DIRS}
  )

find_package(Threads)
include(CheckCXXCompilerFlag)
check_cxx_compiler_flag(-O3 O3_COMPILER_FLAG)

add_executable(env2c ../../../misc/tools/env2c/env2c.cxx)
if (O3_COMPILER_FLAG)
  # If we have the O3 flag, use it
  target_compile_options(env2c PRIVATE "-O3")
endif (O3_COMPILER_FLAG)
target_link_libraries(env2c ${CMAKE_THREAD_LIBS_INIT})

set(ENV_CMD_C "${CMAKE_CURRENT_BINARY_DIR}/env_cmd.c")
add_custom_command(OUTPUT ${ENV_CMD_C}
  COMMAND env2c ${CMAKE_BINARY_DIR}/cmakefiles.cmake ${ENV_CMD_C}
  DEPENDS env2c
  COMMENT "[ENV2C] Scanning for environment variable usage"
  )
DISTCLEAN(${ENV_CMD_C})
set_source_files_properties(env.c PROPERTIES OBJECT_DEPENDS ${ENV_CMD_C})

add_definitions(-DGED_PLUGIN)
ged_plugin_library(ged-env SHARED env.c)
target_link_libraries(ged-env libged libbu)
set_property(TARGET ged-env APPEND PROPERTY COMPILE_DEFINITIONS BRLCADBUILD HAVE_CONFIG_H)
VALIDATE_STYLE(ged-env env.c)
PLUGIN_SETUP(ged-env ged)

CMAKEFILES(
  CMakeLists.txt
  env.c
  )

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
