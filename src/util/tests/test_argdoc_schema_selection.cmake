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
          "terminal_options": ["help"],
          "operands": []
        }
      }
    ]
  },
  {
    "schema_version": 1,
    "kind": "native",
    "name": "detailed",
    "parse_policy": "options_first",
    "options": [
      {"short": "h", "long": "help", "canonical": "help", "alias_of": "", "argument_requirement": "none", "hidden": false},
      {"short": "v", "long": "verbose", "canonical": "verbose", "alias_of": "", "argument_requirement": "none", "hidden": false, "repeat": true},
      {"short": "o", "long": "output", "canonical": "output", "alias_of": "", "argument": "file", "argument_requirement": "required", "hidden": false},
      {"short": "x", "long": "prefix", "canonical": "prefix", "alias_of": "", "argument": "text", "argument_requirement": "optional", "hidden": false, "argument_shape": {"kind": "attached_optional", "min_tokens": 0, "max_tokens": 1}}
    ],
    "terminal_options": ["help"],
    "operands": [
      {"name": "input", "type": "file", "min": 1, "max": 1}
    ],
    "operand_groups": [
      {"name": "coordinates", "min": 0, "max": 1, "roles": [
        {"name": "x", "type": "integer"},
        {"name": "y", "type": "integer"}
      ]}
    ]
  },
  {
    "schema_version": 1,
    "kind": "native",
    "name": "repeated-group",
    "parse_policy": "options_first",
    "options": [],
    "operands": [],
    "operand_groups": [
      {"name": "assignment", "min": 1, "max": null, "roles": [
        {"name": "key", "type": "string"},
        {"name": "value", "type": "string"}
      ]}
    ]
  },
  {
    "schema_version": 1,
    "kind": "native",
    "name": "large-flat",
    "parse_policy": "options_first",
    "options": [
      {"short": "a", "canonical": "a"},
      {"short": "b", "canonical": "b"},
      {"short": "c", "canonical": "c"},
      {"short": "d", "canonical": "d"},
      {"short": "e", "canonical": "e"},
      {"short": "f", "canonical": "f"},
      {"short": "g", "canonical": "g"},
      {"short": "i", "canonical": "i"},
      {"short": "j", "canonical": "j"},
      {"short": "k", "canonical": "k"},
      {"short": "l", "canonical": "l"},
      {"short": "m", "canonical": "m"},
      {"short": "n", "canonical": "n"}
    ],
    "operands": []
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
  },
  {
    "schema_version": 1,
    "kind": "native",
    "name": "presented",
    "parse_policy": "options_first",
    "options": [
      {"short": "z", "long": "zeta", "canonical": "zeta", "alias_of": "", "argument_requirement": "none", "hidden": false, "visibility": "default", "help_group": "Primary options", "help": "First option"},
      {"short": "b", "long": "basic", "canonical": "basic", "alias_of": "", "argument_requirement": "none", "hidden": false, "visibility": "default", "help_group": "Primary options", "help": "Second option"},
      {"short": "a", "long": "advanced", "canonical": "advanced", "alias_of": "", "argument_requirement": "none", "hidden": false, "visibility": "advanced", "help_group": "Advanced options", "help": "Specialized option"},
      {"short": "i", "long": "internal", "canonical": "internal", "alias_of": "", "argument_requirement": "none", "hidden": true, "visibility": "internal", "help_group": "Advanced options", "help": "Internal option"}
    ],
    "help_groups": [
      {"name": "Primary options", "help": "Ordinary controls", "first_option": "zeta", "visibility": "default"},
      {"name": "Advanced options", "help": "Specialized controls", "first_option": "advanced", "visibility": "advanced"}
    ],
    "parameter_groups": [
      {"option": "advanced", "name": "Expert", "help": "Expert controls", "parameters": [
        {"name": "quality", "argument": "level", "type": "integer", "default": "2", "help": "Rendering quality"}
      ]}
    ],
    "operands": []
  },
  {
    "schema_version": 1,
    "kind": "native",
    "name": "constrained",
    "parse_policy": "options_first",
    "options": [
      {"short": "f", "long": "file", "canonical": "file", "alias_of": "", "argument": "path", "argument_requirement": "required", "hidden": false},
      {"short": "l", "long": "length", "canonical": "length", "alias_of": "", "argument": "bytes", "argument_requirement": "required", "hidden": false}
    ],
    "operands": [
      {"name": "output", "type": "file", "min": 0, "max": 1},
      {"name": "object", "type": "db_object", "min": 1, "max": null}
    ],
    "constraints": [
      {"kind": "option_count", "condition": "always", "options": ["file", "length"], "min": 1, "max": 1, "hint": "select one input"}
    ]
  },
  {
    "schema_version": 1,
    "kind": "native",
    "name": "exclusive",
    "parse_policy": "options_first",
    "options": [
      {"short": "e", "long": "eia", "canonical": "eia", "alias_of": "", "argument_requirement": "none", "hidden": false},
      {"short": "f", "long": "fcc", "canonical": "fcc", "alias_of": "", "argument_requirement": "none", "hidden": false},
      {"short": "s", "long": "smpte", "canonical": "smpte", "alias_of": "", "argument_requirement": "none", "hidden": false},
      {"short": "F", "long": "framebuffer", "canonical": "framebuffer", "alias_of": "", "argument": "device", "argument_requirement": "required", "hidden": false}
    ],
    "operands": [],
    "constraints": [
      {"kind": "option_count", "condition": "always", "options": ["eia", "fcc", "smpte"], "min": 0, "max": 1, "hint": "select at most one standard"}
    ]
  },
  {
    "schema_version": 1,
    "kind": "native",
    "name": "cased",
    "parse_policy": "options_first",
    "options": [
      {"short": "i", "long": "input", "canonical": "input", "alias_of": "", "argument_requirement": "none", "hidden": false},
      {"short": "o", "long": "output", "canonical": "output", "alias_of": "", "argument_requirement": "none", "hidden": false}
    ],
    "operands": [],
    "operand_cases": [
      {"name": "input", "condition": "any_option_present", "options": ["input"], "schema": {"options": [], "operands": [{"name": "source", "min": 1, "max": 1}], "constraints": []}},
      {"name": "output", "condition": "all_options_present", "options": ["input", "output"], "schema": {"options": [], "operands": [{"name": "destination", "min": 1, "max": 1}], "constraints": []}}
    ]
  },
  {
    "schema_version": 1,
    "kind": "native",
    "name": "keyworded",
    "parse_policy": "options_first",
    "options": [],
    "operands": [
      {"name": "mode", "type": "keyword", "min": 1, "max": 1, "values": ["mesh", "csg"]},
      {"name": "enabled", "type": "keyword", "min": 0, "max": 1, "values": ["0", "1"]}
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

execute_process(
  COMMAND "${ARGDOC}" --render --schema "${schema_file}" --name parent --path list
  OUTPUT_VARIABLE path_render
  RESULT_VARIABLE path_render_result
  ERROR_VARIABLE path_render_error
)
if(NOT path_render_result EQUAL 0)
  message(FATAL_ERROR "argdoc could not select a command-tree path: ${path_render_error}")
endif()
string(FIND "${path_render}" "*parent list*\n" path_synopsis)
if(path_synopsis EQUAL -1)
  message(FATAL_ERROR "argdoc did not preserve the selected command-tree invocation")
endif()

execute_process(
  COMMAND "${ARGDOC}" --render --schema "${schema_file}" --name detailed
  OUTPUT_VARIABLE detailed_render
  RESULT_VARIABLE detailed_render_result
  ERROR_VARIABLE detailed_render_error
)
if(NOT detailed_render_result EQUAL 0)
  message(FATAL_ERROR "argdoc could not render a detailed synopsis: ${detailed_render_error}")
endif()
string(FIND "${detailed_render}" "*detailed* [*-v*]... [*-o* _file_] [*-x*[_text_]] _input_ [_x_ _y_]" detailed_synopsis)
string(FIND "${detailed_render}" "*-x*[_text_], *--prefix*[=_text_]" detailed_attached)
string(FIND "${detailed_render}" "*detailed* [*-h*]" detailed_help)
if(detailed_synopsis EQUAL -1 OR detailed_attached EQUAL -1 OR NOT detailed_help EQUAL -1)
  message(FATAL_ERROR "argdoc did not render explicit nonterminal options canonically")
endif()

execute_process(
  COMMAND "${ARGDOC}" --render --schema "${schema_file}" --name repeated-group
  OUTPUT_VARIABLE repeated_group_render
  RESULT_VARIABLE repeated_group_render_result
  ERROR_VARIABLE repeated_group_render_error
)
if(NOT repeated_group_render_result EQUAL 0)
  message(FATAL_ERROR "argdoc could not render a repeated operand group: ${repeated_group_render_error}")
endif()
string(FIND "${repeated_group_render}" "*repeated-group* _key_ _value_ [(_key_ _value_) ...]" repeated_group_synopsis)
if(repeated_group_synopsis EQUAL -1)
  message(FATAL_ERROR "argdoc did not render a repeated operand group canonically")
endif()

execute_process(
  COMMAND "${ARGDOC}" --render --schema "${schema_file}" --name large-flat
  OUTPUT_VARIABLE large_flat_render
  RESULT_VARIABLE large_flat_render_result
  ERROR_VARIABLE large_flat_render_error
)
if(NOT large_flat_render_result EQUAL 0)
  message(FATAL_ERROR "argdoc could not render a large flat schema: ${large_flat_render_error}")
endif()
string(FIND "${large_flat_render}" "*large-flat* [*-a*] [*-b*] [*-c*] [*-d*] [*-e*] [*-f*] [*-g*] [*-i*] [*-j*] [*-k*] [*-l*] [*-m*]\n           [*-n*]" large_flat_synopsis)
string(FIND "${large_flat_render}" "[_OPTION_]" large_flat_summary)
if(large_flat_synopsis EQUAL -1 OR NOT large_flat_summary EQUAL -1)
  message(FATAL_ERROR "argdoc did not wrap all options in a large flat schema")
endif()

execute_process(
  COMMAND "${ARGDOC}" --render --schema "${schema_file}" --name parent --path missing
  RESULT_VARIABLE missing_path_result
)
if(missing_path_result EQUAL 0)
  message(FATAL_ERROR "argdoc accepted a missing command-tree path")
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

execute_process(
  COMMAND "${ARGDOC}" --render --schema "${schema_file}" --name constrained
  OUTPUT_VARIABLE constrained_render
  RESULT_VARIABLE constrained_render_result
  ERROR_VARIABLE constrained_render_error
)
if(NOT constrained_render_result EQUAL 0)
  message(FATAL_ERROR "argdoc could not render a constrained synopsis: ${constrained_render_error}")
endif()
string(FIND "${constrained_render}" "[verse]\n*constrained* (*-f* _path_ | *-l* _bytes_) [_output_] _object_ ..." constrained_synopsis)
if(constrained_synopsis EQUAL -1)
  message(FATAL_ERROR "argdoc did not render a standards-aware constrained synopsis")
endif()

execute_process(
  COMMAND "${ARGDOC}" --render --schema "${schema_file}" --name exclusive
  OUTPUT_VARIABLE exclusive_render
  RESULT_VARIABLE exclusive_render_result
  ERROR_VARIABLE exclusive_render_error
)
if(NOT exclusive_render_result EQUAL 0)
  message(FATAL_ERROR "argdoc could not render an optional exclusive choice: ${exclusive_render_error}")
endif()
string(FIND "${exclusive_render}" "*exclusive* [*-e* | *-f* | *-s*] [*-F* _device_]" exclusive_synopsis)
if(exclusive_synopsis EQUAL -1)
  message(FATAL_ERROR "argdoc did not group optional mutually exclusive options")
endif()

execute_process(
  COMMAND "${ARGDOC}" --render --schema "${schema_file}" --name cased
  OUTPUT_VARIABLE cased_render
  RESULT_VARIABLE cased_render_result
  ERROR_VARIABLE cased_render_error
)
if(NOT cased_render_result EQUAL 0)
  message(FATAL_ERROR "argdoc could not render operand cases: ${cased_render_error}")
endif()
string(FIND "${cased_render}" "*cased* *-i* _source_\n*cased* *-i* *-o* _destination_" cased_synopsis)
if(cased_synopsis EQUAL -1)
  message(FATAL_ERROR "argdoc did not render operand cases as separate synopsis forms")
endif()

execute_process(
  COMMAND "${ARGDOC}" --render --schema "${schema_file}" --name keyworded
  OUTPUT_VARIABLE keyworded_render
  RESULT_VARIABLE keyworded_render_result
  ERROR_VARIABLE keyworded_render_error
)
if(NOT keyworded_render_result EQUAL 0)
  message(FATAL_ERROR "argdoc could not render keyword operands: ${keyworded_render_error}")
endif()
string(FIND "${keyworded_render}" "*keyworded* (*mesh* | *csg*) [(*0* | *1*)]" keyworded_synopsis)
if(keyworded_synopsis EQUAL -1)
  message(FATAL_ERROR "argdoc did not render literal keyword alternatives canonically")
endif()

file(WRITE "${page_file}" "= CONSTRAINED(1)\n\n== SYNOPSIS\n\n[source]\n----\nconstrained [options]\n----\n\n[[options]]\n== OPTIONS\n\n*-f* _path_, *--file*=_path_::\nInput file.\n\n*-l* _bytes_, *--length*=_bytes_::\nInput length.\n")
execute_process(
  COMMAND "${ARGDOC}" --update --schema "${schema_file}" --name constrained --man-page "${page_file}"
  RESULT_VARIABLE synopsis_update_result
  ERROR_VARIABLE synopsis_update_error
)
if(NOT synopsis_update_result EQUAL 0)
  message(FATAL_ERROR "argdoc could not insert a generated synopsis: ${synopsis_update_error}")
endif()
file(READ "${page_file}" updated_page)
string(FIND "${updated_page}" "// BEGIN GENERATED BU_ARG SYNOPSIS\n[verse]\n*constrained*" inserted_synopsis)
string(FIND "${updated_page}" "[[options]]\n== OPTIONS" preserved_anchor)
string(FIND "${updated_page}" "*-f* _path_, *--file*=_path_::" preserved_options)
if(inserted_synopsis EQUAL -1 OR preserved_anchor EQUAL -1 OR preserved_options EQUAL -1)
  message(FATAL_ERROR "argdoc did not insert synopsis while preserving the following section")
endif()

file(WRITE "${page_file}" "= KEYWORDED(1)\n\n== NAME\nkeyworded - test command\n\n[[description]]\n== DESCRIPTION\n\nManual prose.\n")
execute_process(
  COMMAND "${ARGDOC}" --update --schema "${schema_file}" --name keyworded --man-page "${page_file}"
  RESULT_VARIABLE missing_synopsis_result
  ERROR_VARIABLE missing_synopsis_error
)
if(NOT missing_synopsis_result EQUAL 0)
  message(FATAL_ERROR "argdoc could not add a missing synopsis section: ${missing_synopsis_error}")
endif()
file(READ "${page_file}" inserted_page)
string(FIND "${inserted_page}" "== SYNOPSIS\n\n// BEGIN GENERATED BU_ARG SYNOPSIS" inserted_section)
string(FIND "${inserted_page}" "[[description]]\n== DESCRIPTION\n\nManual prose." preserved_description)
if(inserted_section EQUAL -1 OR preserved_description EQUAL -1)
  message(FATAL_ERROR "argdoc did not add a synopsis while preserving the description")
endif()

execute_process(
  COMMAND "${ARGDOC}" --render --schema "${schema_file}" --name presented
  OUTPUT_VARIABLE presented_render
  RESULT_VARIABLE presented_render_result
  ERROR_VARIABLE presented_render_error
)
if(NOT presented_render_result EQUAL 0)
  message(FATAL_ERROR "argdoc could not render grouped options: ${presented_render_error}")
endif()
string(FIND "${presented_render}" "=== Primary options" primary_heading)
string(FIND "${presented_render}" "*-z*, *--zeta*::" zeta_term)
string(FIND "${presented_render}" "*-b*, *--basic*::" basic_term)
string(FIND "${presented_render}" "=== Advanced options" advanced_heading)
string(FIND "${presented_render}" "*-a*, *--advanced*::" advanced_term)
string(FIND "${presented_render}" "*presented* [*-z*] [*-b*]" presented_synopsis)
string(FIND "${presented_render}" "*presented* [*-z*] [*-b*] [*-a*]" advanced_synopsis)
string(FIND "${presented_render}" "==== Expert" parameter_heading)
string(FIND "${presented_render}" "*quality*=_level_::" parameter_term)
string(FIND "${presented_render}" "--internal" internal_term)
if(primary_heading EQUAL -1 OR zeta_term EQUAL -1 OR basic_term EQUAL -1 OR
   advanced_heading EQUAL -1 OR advanced_term EQUAL -1 OR
   presented_synopsis EQUAL -1 OR NOT advanced_synopsis EQUAL -1 OR
   parameter_heading EQUAL -1 OR parameter_term EQUAL -1 OR
   NOT primary_heading LESS zeta_term OR NOT zeta_term LESS basic_term OR
   NOT basic_term LESS advanced_heading OR NOT advanced_heading LESS advanced_term OR
   NOT advanced_term LESS parameter_heading OR NOT parameter_heading LESS parameter_term OR
   NOT internal_term EQUAL -1)
  message(FATAL_ERROR "argdoc did not preserve grouped schema order and visibility")
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

set(large_tree_children "")
foreach(child_index RANGE 1 11)
  if(NOT child_index EQUAL 1)
    string(APPEND large_tree_children ",")
  endif()
  string(APPEND large_tree_children
    "{\"schema\":{\"name\":\"child${child_index}\",\"options\":[],\"operands\":[]},\"subcommands\":[]}")
endforeach()
file(WRITE "${version_schema}"
  "{\"schema_version\":1,\"kind\":\"native_tree\",\"root\":{\"name\":\"large\",\"options\":[],\"operands\":[]},\"child_phase\":\"after_options\",\"subcommands\":[${large_tree_children}]}\n")
execute_process(
  COMMAND "${ARGDOC}" --render --schema "${version_schema}"
  OUTPUT_VARIABLE large_tree_render
  RESULT_VARIABLE large_tree_result
  ERROR_VARIABLE large_tree_error
)
if(NOT large_tree_result EQUAL 0)
  message(FATAL_ERROR "argdoc could not render a large command tree: ${large_tree_error}")
endif()
string(FIND "${large_tree_render}" "*large* _SUBCOMMAND_ [_ARGUMENT_]..." large_tree_summary)
string(FIND "${large_tree_render}" "*large child1*" large_tree_detail)
if(large_tree_summary EQUAL -1 OR NOT large_tree_detail EQUAL -1)
  message(FATAL_ERROR "argdoc did not summarize a large command tree")
endif()
file(WRITE "${page_file}"
  "= LARGE(1)\n\n== SYNOPSIS\n\n[source]\n----\nlarge\n----\n\n== SUBCOMMANDS\n\n// BEGIN GENERATED BU_ARG SUBCOMMANDS\nstale\n// END GENERATED BU_ARG SUBCOMMANDS\n\n== DESCRIPTION\n")
execute_process(
  COMMAND "${ARGDOC}" --update --schema "${version_schema}" --man-page "${page_file}"
  RESULT_VARIABLE large_tree_update_result
  ERROR_VARIABLE large_tree_update_error
)
if(NOT large_tree_update_result EQUAL 0)
  message(FATAL_ERROR "argdoc could not update a command-tree summary: ${large_tree_update_error}")
endif()
file(READ "${page_file}" large_tree_page)
string(FIND "${large_tree_page}" "*child1*::\nNo description." child_summary)
if(child_summary EQUAL -1)
  message(FATAL_ERROR "argdoc did not generate command-tree child summaries")
endif()

file(MAKE_DIRECTORY "${audit_dir}")
file(
  WRITE "${audit_schema}"
  [=[
[
  {
    "schema_version": 1,
    "kind": "native_tree",
    "root": {
      "name": "documented",
      "parse_policy": "options_first",
      "options": [
        {"short": "v", "long": "verbose", "canonical": "verbose", "alias_of": "", "argument_requirement": "none", "hidden": false}
      ],
      "operands": []
    },
    "child_phase": "after_options",
    "subcommands": [
      {
        "schema": {
          "name": "list",
          "parse_policy": "options_first",
          "options": [
            {"short": "q", "long": "quiet", "canonical": "quiet", "alias_of": "", "argument_requirement": "none", "hidden": false}
          ],
          "operands": []
        },
        "man_page": "documented_list",
        "subcommands": []
      }
    ]
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
file(WRITE "${audit_dir}/documented.adoc"
  "= DOCUMENTED(1)\n\n== SYNOPSIS\n\n[source]\n----\ndocumented\n----\n\n== DESCRIPTION\nDocumented command.\n\n*-v*, *--verbose*::\nVerbose output.\n\n// BEGIN GENERATED BU_ARG SUBCOMMANDS\nstale\n// END GENERATED BU_ARG SUBCOMMANDS\n")
file(WRITE "${audit_dir}/documented_list.adoc"
  "= DOCUMENTED_LIST(1)\n\n== SYNOPSIS\n\n[source]\n----\ndocumented list\n----\n\n== DESCRIPTION\nList command.\n\n*-q*, *--quiet*::\nQuiet output.\n")
file(WRITE "${audit_exemptions}" "internal\tInternal test command.\n")
execute_process(
  COMMAND
    "${ARGDOC}" --update-directory "${audit_dir}"
    --allow-undocumented "${audit_exemptions}" --schema "${audit_schema}"
  RESULT_VARIABLE audit_update_result
  ERROR_VARIABLE audit_update_error
)
if(NOT audit_update_result EQUAL 0)
  message(FATAL_ERROR "argdoc directory update failed: ${audit_update_error}")
endif()
file(READ "${audit_dir}/documented.adoc" audit_root_page)
string(FIND "${audit_root_page}" "*list*::\nNo description." audit_root_child)
if(audit_root_child EQUAL -1)
  message(FATAL_ERROR "argdoc did not update the root command-tree subcommand region")
endif()
execute_process(
  COMMAND
    "${ARGDOC}" --audit-directory "${audit_dir}"
    --allow-undocumented "${audit_exemptions}" --schema "${audit_schema}"
  RESULT_VARIABLE audit_result
  OUTPUT_VARIABLE audit_output
  ERROR_VARIABLE audit_error
)
if(NOT audit_result EQUAL 0)
  message(FATAL_ERROR "argdoc directory audit failed: ${audit_error}")
endif()
string(FIND "${audit_output}" "Checked 2 man pages" split_page_count)
if(split_page_count EQUAL -1)
  message(FATAL_ERROR "argdoc did not audit the command-tree subcommand page")
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
