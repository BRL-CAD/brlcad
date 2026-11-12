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
    "kind": "native",
    "name": "list",
    "parse_policy": "options_first",
    "options": [
      {"short": "r", "long": "", "canonical": "r", "alias_of": "", "argument_requirement": "none", "hidden": false}
    ],
    "operands": []
  },
  {
    "kind": "native",
    "name": "expression",
    "parse_policy": "stop_at_first_operand",
    "options": [],
    "operands": [
      {"name": "expression", "type": "raw", "min": 1, "max": null}
    ]
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

file(REMOVE "${schema_file}" "${page_file}")
