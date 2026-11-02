if(NOT DEFINED SBOM_TOOL OR NOT DEFINED TEST_ROOT OR NOT DEFINED INSTALL_SBOM_TEMPLATE)
  message(FATAL_ERROR "SBOM_TOOL, TEST_ROOT, and INSTALL_SBOM_TEMPLATE are required")
endif()

find_program(GIT_EXECUTABLE git REQUIRED)
file(REMOVE_RECURSE "${TEST_ROOT}")
file(MAKE_DIRECTORY
  "${TEST_ROOT}/build/CMakeFiles"
  "${TEST_ROOT}/install/bin"
  "${TEST_ROOT}/install/share/doc"
  "${TEST_ROOT}/source")
file(WRITE "${TEST_ROOT}/install/bin/brlcad-test.txt" "BRL-CAD\n")
file(WRITE "${TEST_ROOT}/source/CMakeLists.txt" "cmake_minimum_required(VERSION 3.26)\n")
foreach(git_arguments
    "init"
    "config;user.name;SBOM Regression"
    "config;user.email;sbom@example.invalid"
    "config;commit.gpgsign;false"
    "add;CMakeLists.txt"
    "commit;-m;Initial")
  execute_process(
    COMMAND "${GIT_EXECUTABLE}" ${git_arguments}
    WORKING_DIRECTORY "${TEST_ROOT}/source"
    RESULT_VARIABLE git_result
    OUTPUT_QUIET
    ERROR_QUIET
  )
  if(NOT git_result EQUAL 0)
    message(FATAL_ERROR "Unable to prepare the install-SBOM Git fixture")
  endif()
endforeach()

function(configure_install_sbom output allow_dirty)
  set(CMAKE_BINARY_DIR "${TEST_ROOT}/build")
  set(CMAKE_SOURCE_DIR "${TEST_ROOT}/source")
  set(DOC_DIR "share/doc")
  set(BRLCAD_SBOM_REVISION "test-revision")
  set(BRLCAD_SBOM_CREATED "2026-01-01T00:00:00Z")
  set(BRLCAD_SBOM_ALLOW_DIRTY "${allow_dirty}")
  set(BRLCAD_BEXT_SBOM_REQUIRED FALSE)
  set(BRLCAD_SBOM_GIT_COMMAND "${GIT_EXECUTABLE}")
  set(BRLCAD_SBOM_EXECUTABLE "${SBOM_TOOL}")
  set(BRLCAD_VERSION "1.0.0")
  if(NOT DEFINED BRLCAD_BEXT_COMPONENT_MANIFEST)
    set(BRLCAD_BEXT_COMPONENT_MANIFEST "${TEST_ROOT}/missing-components.json")
  endif()
  if(NOT DEFINED BRLCAD_BEXT_SBOM)
    set(BRLCAD_BEXT_SBOM "${TEST_ROOT}/missing-dependencies.spdx.json")
  endif()
  configure_file("${INSTALL_SBOM_TEMPLATE}" "${output}" @ONLY)
endfunction()

function(configure_install_sbom_with_dependencies output)
  set(BRLCAD_BEXT_COMPONENT_MANIFEST "${TEST_ROOT}/components.json")
  set(BRLCAD_BEXT_SBOM "${TEST_ROOT}/dependencies.spdx.json")
  configure_install_sbom("${output}" OFF)
endfunction()

set(default_script "${TEST_ROOT}/build/install-sbom-default.cmake")
set(dirty_script "${TEST_ROOT}/build/install-sbom-dirty.cmake")
configure_install_sbom("${default_script}" OFF)
configure_install_sbom("${dirty_script}" ON)
set(output "${TEST_ROOT}/install/share/doc/brlcad.spdx.json")
set(installed_file "${TEST_ROOT}/install/bin/brlcad-test.txt")
set(script_arguments
  -DCMAKE_INSTALL_PREFIX=${TEST_ROOT}/install
  -DCMAKE_INSTALL_MANIFEST_FILES=${installed_file}
)

file(WRITE "${TEST_ROOT}/source/dirty.txt" "dirty\n")
file(WRITE "${output}" "stale\n")
execute_process(
  COMMAND "${CMAKE_COMMAND}" ${script_arguments} -P "${default_script}"
  RESULT_VARIABLE dirty_rejection_result
  ERROR_QUIET
)
if(dirty_rejection_result EQUAL 0 OR EXISTS "${output}")
  message(FATAL_ERROR "Install SBOM did not reject dirty sources and remove stale output")
endif()

execute_process(
  COMMAND "${CMAKE_COMMAND}" ${script_arguments} -P "${dirty_script}"
  RESULT_VARIABLE dirty_override_result
)
if(NOT dirty_override_result EQUAL 0 OR NOT EXISTS "${output}")
  message(FATAL_ERROR "Install SBOM dirty-source override failed")
endif()
file(READ "${output}" dirty_sbom)
string(JSON source_info GET "${dirty_sbom}" packages 0 packageSourceInfo)
if(NOT source_info MATCHES "BRL-CAD source revision [0-9a-f]+-dirty-[0-9a-f]+$")
  message(FATAL_ERROR "Install SBOM dirty-source provenance is incomplete: ${source_info}")
endif()

file(REMOVE "${TEST_ROOT}/source/dirty.txt")
execute_process(
  COMMAND "${CMAKE_COMMAND}" ${script_arguments}
    -D_brlcad_spdx_validator=${GIT_EXECUTABLE}
    -P "${default_script}"
  RESULT_VARIABLE validation_result
  ERROR_QUIET
)
if(validation_result EQUAL 0 OR EXISTS "${output}")
  message(FATAL_ERROR "Install SBOM validation failure left a publishable output")
endif()

file(WRITE "${TEST_ROOT}/install/lib/zlib-test.txt" "zlib\n")
file(WRITE "${TEST_ROOT}/components.json" [=[
{
  "schema": "https://brlcad.org/schema/bext-component-manifest-v2",
  "platform": "regression-test",
  "install_root": "install",
  "generated_by": "bext",
  "files": [
    {"path": "lib/zlib-test.txt", "component": "zlib"}
  ],
  "packages": [
    {"component": "zlib", "id": "zlib/zlib", "spdx_id": "SPDXRef-Package-zlib", "primary": true}
  ]
}
]=])
file(WRITE "${TEST_ROOT}/dependencies.spdx.json" [=[
{
  "spdxVersion": "SPDX-2.3",
  "dataLicense": "CC0-1.0",
  "SPDXID": "SPDXRef-DOCUMENT",
  "name": "bext-regression-test",
  "documentNamespace": "https://brlcad.org/sbom/install-regression-test",
  "creationInfo": {"created": "2026-01-01T00:00:00Z", "creators": ["Tool: regression-test"]},
  "packages": [
    {
      "name": "zlib",
      "SPDXID": "SPDXRef-Package-zlib",
      "versionInfo": "1.3.2",
      "downloadLocation": "https://zlib.net/zlib-1.3.2.tar.gz",
      "filesAnalyzed": false,
      "licenseConcluded": "Zlib",
      "licenseDeclared": "Zlib",
      "copyrightText": "NOASSERTION"
    }
  ],
  "relationships": []
}
]=])
set(dependency_script "${TEST_ROOT}/build/install-sbom-dependencies.cmake")
configure_install_sbom_with_dependencies("${dependency_script}")
execute_process(
  COMMAND "${CMAKE_COMMAND}"
    -DCMAKE_INSTALL_PREFIX=${TEST_ROOT}/install
    "-DCMAKE_INSTALL_MANIFEST_FILES=${installed_file};${TEST_ROOT}/install/lib/zlib-test.txt"
    -P "${dependency_script}"
  RESULT_VARIABLE dependency_result
)
if(NOT dependency_result EQUAL 0 OR NOT EXISTS "${output}")
  message(FATAL_ERROR "Install SBOM treated a staged bext file as BRL-CAD-owned")
endif()
