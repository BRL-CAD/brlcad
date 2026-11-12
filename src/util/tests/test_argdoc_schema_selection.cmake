if(NOT DEFINED ARGDOC OR NOT DEFINED TEST_DIR)
  message(FATAL_ERROR "ARGDOC and TEST_DIR are required")
endif()

set(schema_file "${TEST_DIR}/argdoc-schema-selection.json")
set(page_file "${TEST_DIR}/argdoc-schema-selection.adoc")

file(
  WRITE "${schema_file}"
  [=[
[
  {
    "schema_version": 1,
    "kind": "native_tree",
    "root": {"name": "parent"},
    "subcommands": [
      {
        "schema": {
          "kind": "native",
          "name": "list",
          "parse_policy": "options_first",
          "options": [
            {"short": "h", "long": "help", "canonical": "help", "alias_of": "", "argument_requirement": "none", "hidden": false}
          ],
          "operands": []
        }
      }
    ]
  },
  {
    "schema_version": 1,
    "kind": "native",
    "name": "list",
    "parse_policy": "options_first",
    "options": [
      {"short": "r", "long": "", "canonical": "r", "alias_of": "", "argument_requirement": "none", "hidden": false}
    ],
    "operands": []
  },
  {
    "schema_version": 1,
    "kind": "native",
    "name": "expression",
    "parse_policy": "stop_at_first_operand",
    "options": [],
    "operands": [
      {"name": "expression", "type": "raw", "min": 1, "max": null}
    ]
  },
  {
    "schema_version": 1,
    "kind": "native",
    "name": "symbolic",
    "parse_policy": "options_first",
    "options": [
      {"short": "*", "long": "", "canonical": "*", "alias_of": "", "argument": "value", "argument_requirement": "required", "hidden": false}
    ],
    "operands": []
  }
]
]=]
)

file(WRITE "${page_file}" "*-r*::\nTop-level option.\n")
execute_process(
  COMMAND "${ARGDOC}" --check --schema "${schema_file}" --name list --man-page "${page_file}"
  RESULT_VARIABLE selection_result
  ERROR_VARIABLE selection_error
)
if(NOT selection_result EQUAL 0)
  message(FATAL_ERROR "argdoc selected a nested schema: ${selection_error}")
endif()

file(WRITE "${page_file}" "*-c* _object_::\nExpression primary.\n\n_object_ *-beq* _object_::\nExpression operator.\n")
execute_process(
  COMMAND "${ARGDOC}" --check --schema "${schema_file}" --name expression --man-page "${page_file}"
  RESULT_VARIABLE opaque_result
  ERROR_VARIABLE opaque_error
)
if(NOT opaque_result EQUAL 0)
  message(FATAL_ERROR "argdoc rejected opaque grammar tokens: ${opaque_error}")
endif()

file(WRITE "${page_file}" "`-*` _value_::\nSymbolic short option.\n")
execute_process(
  COMMAND "${ARGDOC}" --check --schema "${schema_file}" --name symbolic --man-page "${page_file}"
  RESULT_VARIABLE symbolic_result
  ERROR_VARIABLE symbolic_error
)
if(NOT symbolic_result EQUAL 0)
  message(FATAL_ERROR "argdoc rejected a symbolic short option: ${symbolic_error}")
endif()

execute_process(
  COMMAND "${ARGDOC}" --render --schema "${schema_file}" --name symbolic
  OUTPUT_VARIABLE symbolic_render
  RESULT_VARIABLE symbolic_render_result
  ERROR_VARIABLE symbolic_render_error
)
if(NOT symbolic_render_result EQUAL 0)
  message(FATAL_ERROR "argdoc could not render a symbolic short option: ${symbolic_render_error}")
endif()
string(FIND "${symbolic_render}" "`-*` _value_::" symbolic_term)
if(symbolic_term EQUAL -1)
  message(FATAL_ERROR "argdoc did not render the symbolic short option canonically")
endif()

set(audit_dir "${TEST_DIR}/argdoc-audit-pages")
set(audit_schema "${TEST_DIR}/argdoc-audit.json")
set(audit_exemptions "${TEST_DIR}/argdoc-audit-exemptions.txt")
set(version_schema "${TEST_DIR}/argdoc-version.json")

file(
  WRITE "${version_schema}"
  [=[
{"kind":"native","name":"version-test","options":[],"operands":[]}
]=]
)
execute_process(
  COMMAND "${ARGDOC}" --render --schema "${version_schema}"
  RESULT_VARIABLE missing_version_result
)
if(missing_version_result EQUAL 0)
  message(FATAL_ERROR "argdoc accepted a schema without schema_version")
endif()

file(
  WRITE "${version_schema}"
  [=[
{"schema_version":2,"kind":"native","name":"version-test","options":[],"operands":[]}
]=]
)
execute_process(
  COMMAND "${ARGDOC}" --render --schema "${version_schema}"
  RESULT_VARIABLE unsupported_version_result
)
if(unsupported_version_result EQUAL 0)
  message(FATAL_ERROR "argdoc accepted an unsupported schema_version")
endif()

file(MAKE_DIRECTORY "${audit_dir}")
file(
  WRITE "${audit_schema}"
  [=[
[
  {
    "schema_version": 1,
    "kind": "native",
    "name": "documented",
    "parse_policy": "options_first",
    "options": [
      {"short": "v", "long": "verbose", "canonical": "verbose", "alias_of": "", "argument_requirement": "none", "hidden": false}
    ],
    "operands": []
  },
  {
    "schema_version": 1,
    "kind": "native",
    "name": "internal",
    "parse_policy": "options_first",
    "options": [],
    "operands": []
  }
]
]=]
)
file(WRITE "${audit_dir}/documented.adoc" "*-v*, *--verbose*::\nVerbose output.\n")
file(WRITE "${audit_exemptions}" "internal\tInternal test command.\n")
execute_process(
  COMMAND
    "${ARGDOC}" --audit-directory "${audit_dir}"
    --allow-undocumented "${audit_exemptions}" --schema "${audit_schema}"
  RESULT_VARIABLE audit_result
  ERROR_VARIABLE audit_error
)
if(NOT audit_result EQUAL 0)
  message(FATAL_ERROR "argdoc directory audit failed: ${audit_error}")
endif()

file(WRITE "${audit_exemptions}" "")
execute_process(
  COMMAND
    "${ARGDOC}" --audit-directory "${audit_dir}"
    --allow-undocumented "${audit_exemptions}" --schema "${audit_schema}"
  RESULT_VARIABLE missing_exemption_result
)
if(missing_exemption_result EQUAL 0)
  message(FATAL_ERROR "argdoc accepted an undocumented schema without an exemption")
endif()

file(
  REMOVE_RECURSE
  "${audit_dir}"
)
file(
  REMOVE
  "${schema_file}"
  "${page_file}"
  "${audit_schema}"
  "${audit_exemptions}"
  "${version_schema}"
)
