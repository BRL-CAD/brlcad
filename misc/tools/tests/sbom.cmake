if(NOT DEFINED SBOM_TOOL OR NOT DEFINED TEST_ROOT)
  message(FATAL_ERROR "SBOM_TOOL and TEST_ROOT are required")
endif()

file(REMOVE_RECURSE "${TEST_ROOT}")
file(MAKE_DIRECTORY "${TEST_ROOT}/install/bin" "${TEST_ROOT}/install/lib" "${TEST_ROOT}/install/share/doc")
file(WRITE "${TEST_ROOT}/install/bin/brlcad-test.txt" "BRL-CAD\n")
file(WRITE "${TEST_ROOT}/install/lib/zlib-test.txt" "zlib\n")
file(WRITE "${TEST_ROOT}/project-files.txt" "${TEST_ROOT}/install/bin/brlcad-test.txt\n")
file(WRITE "${TEST_ROOT}/components.json" [=[
{
  "schema": "https://brlcad.org/schema/bext-component-manifest-v1",
  "platform": "regression-test",
  "install_root": "install",
  "generated_by": "bext",
  "files": [
    {"path": "lib/zlib-test.txt", "component": "zlib"}
  ],
  "selected_packages": [
    {"component": "zlib", "spdx_id": "SPDXRef-Package-zlib"}
  ]
}
]=])
file(WRITE "${TEST_ROOT}/dependencies.spdx.json" [=[
{
  "spdxVersion": "SPDX-2.3",
  "dataLicense": "CC0-1.0",
  "SPDXID": "SPDXRef-DOCUMENT",
  "name": "bext-regression-test",
  "documentNamespace": "https://brlcad.org/sbom/regression-test",
  "creationInfo": {
    "created": "2026-01-01T00:00:00Z",
    "creators": ["Tool: regression-test"]
  },
  "packages": [
    {
      "name": "zlib",
      "SPDXID": "SPDXRef-Package-zlib",
      "versionInfo": "1.3.2",
      "downloadLocation": "https://zlib.net/zlib-1.3.2.tar.gz",
      "filesAnalyzed": false,
      "primaryPackagePurpose": "LIBRARY",
      "licenseConcluded": "Zlib",
      "licenseDeclared": "Zlib",
      "copyrightText": "NOASSERTION",
      "comment": "bext project: zlib; output: install"
    },
    {
      "name": "unused bundled source",
      "SPDXID": "SPDXRef-Package-zlib-unused",
      "versionInfo": "1.0",
      "downloadLocation": "NOASSERTION",
      "filesAnalyzed": false,
      "primaryPackagePurpose": "LIBRARY",
      "licenseConcluded": "NOASSERTION",
      "licenseDeclared": "NOASSERTION",
      "copyrightText": "NOASSERTION",
      "comment": "bext project: zlib; output: install"
    }
  ],
  "relationships": [
    {
      "spdxElementId": "SPDXRef-DOCUMENT",
      "relationshipType": "DESCRIBES",
      "relatedSpdxElement": "SPDXRef-Package-zlib"
    }
  ]
}
]=])
set(output "${TEST_ROOT}/install/share/doc/brlcad.spdx.json")
set(arguments
  --install-root "${TEST_ROOT}/install"
  --project-manifest "${TEST_ROOT}/project-files.txt"
  --component-manifest "${TEST_ROOT}/components.json"
  --dependency-sbom "${TEST_ROOT}/dependencies.spdx.json"
  --output "${output}"
  --version 1.0.0
  --revision 0123456789abcdef
  --created 2026-01-01T00:00:00Z
)

execute_process(COMMAND "${SBOM_TOOL}" ${arguments} RESULT_VARIABLE first_result)
if(NOT first_result EQUAL 0 OR NOT EXISTS "${output}")
  message(FATAL_ERROR "Initial SBOM generation failed")
endif()
file(SHA256 "${output}" first_hash)
execute_process(COMMAND "${SBOM_TOOL}" ${arguments} RESULT_VARIABLE second_result)
file(SHA256 "${output}" second_hash)
if(NOT second_result EQUAL 0 OR NOT first_hash STREQUAL second_hash)
  message(FATAL_ERROR "SBOM generation is not reproducible")
endif()
file(READ "${output}" sbom)
string(JSON file_count LENGTH "${sbom}" files)
if(NOT file_count EQUAL 2)
  message(FATAL_ERROR "SBOM does not identify every installed file")
endif()

set(zlib_file_id "")
math(EXPR file_last "${file_count} - 1")
foreach(index RANGE ${file_last})
  string(JSON file_name GET "${sbom}" files ${index} fileName)
  if(file_name STREQUAL "./lib/zlib-test.txt")
    string(JSON zlib_file_id GET "${sbom}" files ${index} SPDXID)
  endif()
endforeach()
if(NOT zlib_file_id)
  message(FATAL_ERROR "SBOM omits the installed dependency file")
endif()

string(JSON package_count LENGTH "${sbom}" packages)
math(EXPR package_last "${package_count} - 1")
set(zlib_package_analyzed FALSE)
set(unused_package_found FALSE)
foreach(index RANGE ${package_last})
  string(JSON package_id GET "${sbom}" packages ${index} SPDXID)
  if(package_id STREQUAL "SPDXRef-Package-zlib")
    string(JSON zlib_package_analyzed GET "${sbom}" packages ${index} filesAnalyzed)
  elseif(package_id STREQUAL "SPDXRef-Package-zlib-unused")
    set(unused_package_found TRUE)
  endif()
endforeach()
if(NOT zlib_package_analyzed)
  message(FATAL_ERROR "Installed dependency package is not marked as analyzed")
endif()
if(unused_package_found)
  message(FATAL_ERROR "Installed SBOM retained an unselected dependency package")
endif()

string(JSON relationship_count LENGTH "${sbom}" relationships)
math(EXPR relationship_last "${relationship_count} - 1")
set(zlib_contains_file FALSE)
foreach(index RANGE ${relationship_last})
  string(JSON source GET "${sbom}" relationships ${index} spdxElementId)
  string(JSON type GET "${sbom}" relationships ${index} relationshipType)
  string(JSON target GET "${sbom}" relationships ${index} relatedSpdxElement)
  if(source STREQUAL "SPDXRef-Package-zlib" AND type STREQUAL "CONTAINS" AND target STREQUAL zlib_file_id)
    set(zlib_contains_file TRUE)
  endif()
endforeach()
if(NOT zlib_contains_file)
  message(FATAL_ERROR "Installed dependency file is not attributed to its package")
endif()

file(READ "${TEST_ROOT}/components.json" valid_component_manifest)
string(REPLACE "SPDXRef-Package-zlib" "SPDXRef-Package-missing" invalid_component_manifest "${valid_component_manifest}")
file(WRITE "${TEST_ROOT}/components.json" "${invalid_component_manifest}")
execute_process(COMMAND "${SBOM_TOOL}" ${arguments} RESULT_VARIABLE missing_package_result ERROR_QUIET)
if(missing_package_result EQUAL 0 OR EXISTS "${output}")
  message(FATAL_ERROR "SBOM generation did not reject an unavailable selected dependency package")
endif()
file(WRITE "${TEST_ROOT}/components.json" "${valid_component_manifest}")

file(WRITE "${TEST_ROOT}/install/unattributed.txt" "unattributed\n")
execute_process(COMMAND "${SBOM_TOOL}" ${arguments} RESULT_VARIABLE audit_result ERROR_QUIET)
if(audit_result EQUAL 0 OR EXISTS "${output}")
  message(FATAL_ERROR "SBOM generation did not reject unattributed content")
endif()
file(REMOVE "${TEST_ROOT}/install/unattributed.txt")

if(UNIX)
  file(CREATE_LINK "zlib-test.txt" "${TEST_ROOT}/install/lib/zlib-link.txt"
    SYMBOLIC RESULT link_result)
  if(link_result)
    message(FATAL_ERROR "Unable to create SBOM symlink test fixture: ${link_result}")
  endif()
  string(REPLACE
    [=[{"path": "lib/zlib-test.txt", "component": "zlib"}]=]
    [=[{"path": "lib/zlib-test.txt", "component": "zlib"},
    {"path": "lib/zlib-link.txt", "component": "zlib"}]=]
    symlink_component_manifest "${valid_component_manifest}")
  file(WRITE "${TEST_ROOT}/components.json" "${symlink_component_manifest}")
  execute_process(COMMAND "${SBOM_TOOL}" ${arguments} RESULT_VARIABLE symlink_result)
  if(NOT symlink_result EQUAL 0)
    message(FATAL_ERROR "SBOM generation failed for an installed symlink")
  endif()
  file(READ "${output}" symlink_sbom)
  string(JSON symlink_file_count LENGTH "${symlink_sbom}" files)
  if(NOT symlink_file_count EQUAL 3)
    message(FATAL_ERROR "SBOM did not preserve the installed symlink path")
  endif()
endif()
