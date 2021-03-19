project(SC_SUBPROJECT_SCHEMA_SCANNER)
cmake_minimum_required(VERSION 3.6.3)

if(NOT ("${CALLED_FROM}" STREQUAL "STEPCODE_CMAKELISTS" AND DEFINED SC_ROOT AND DEFINED SC_BUILDDIR))
  message(" ${CALLED_FROM}  ${SC_ROOT} ${SC_BUILDDIR}")
  message(FATAL_ERROR "This is not a truly independent project; it should be built during the STEPcode configuration stage.")
endif()

# SC_ROOT: SC root dir
# SC_BUILDDIR: SC build dir, so generated headers can be found

set(CMAKE_BUILD_TYPE Debug)

# set all target location variables so that MSVC will put the executable the same place as other compilers
set(SC_BINARY_DIR ${SC_BUILDDIR})
include(${CMAKE_CURRENT_SOURCE_DIR}/../SC_Outdirs.cmake)

set(schema_scanner_src
  ${SC_ROOT}/src/base/sc_mkdir.c
  ${SC_ROOT}/src/exp2cxx/genCxxFilenames.c
  ${SC_ROOT}/src/exp2cxx/class_strings.c
  ${SC_ROOT}/src/express/generated/expparse.c
  ${SC_ROOT}/src/express/generated/expscan.c
  ${SC_ROOT}/src/express/alg.c
  ${SC_ROOT}/src/express/alloc.c
  ${SC_ROOT}/src/express/caseitem.c
  ${SC_ROOT}/src/express/dict.c
  ${SC_ROOT}/src/express/entity.c
  ${SC_ROOT}/src/express/error.c
  ${SC_ROOT}/src/express/exp_kw.c
  ${SC_ROOT}/src/express/expr.c
  ${SC_ROOT}/src/express/express.c
  ${SC_ROOT}/src/express/factory.c
  ${SC_ROOT}/src/express/hash.c
  ${SC_ROOT}/src/express/lexact.c
  ${SC_ROOT}/src/express/linklist.c
  ${SC_ROOT}/src/express/memory.c
  ${SC_ROOT}/src/express/object.c
  ${SC_ROOT}/src/express/info.c
  ${SC_ROOT}/src/express/resolve.c
  ${SC_ROOT}/src/express/resolve2.c
  ${SC_ROOT}/src/express/schema.c
  ${SC_ROOT}/src/express/scope.c
  ${SC_ROOT}/src/express/stmt.c
  ${SC_ROOT}/src/express/symbol.c
  ${SC_ROOT}/src/express/type.c
  ${SC_ROOT}/src/express/variable.c
  ${CMAKE_CURRENT_SOURCE_DIR}/schemaScanner.cc
 )

include_directories(
  ${SC_ROOT}/include/
  ${SC_ROOT}/src/express/
  ${SC_ROOT}/src/express/generated
  ${SC_ROOT}/src/base
  ${SC_ROOT}/src/exp2cxx
  ${SC_BUILDDIR}/include
 )

add_executable(schema_scanner ${schema_scanner_src})

if(MSVC)
  # Disable warning for preferred usage of secure functions (example strcpy should be strcpy_s, ...)
  target_compile_definitions(schema_scanner PUBLIC _CRT_SECURE_NO_WARNINGS _CRT_NONSTDC_NO_WARNINGS)
else()
  target_compile_options(schema_scanner PUBLIC -pedantic -W -Wall -Wundef -Wfloat-equal -Wshadow -Winline -Wno-long-long)
endif()

target_compile_definitions(schema_scanner PUBLIC SC_STATIC SCHEMA_SCANNER)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8

