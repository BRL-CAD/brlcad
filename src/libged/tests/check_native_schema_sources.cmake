#       C H E C K _ N O _ L E G A C Y _ B U O P T . C M A K E
# BRL-CAD
#
# Copyright (c) 2026 United States Government as represented by
# the U.S. Army Research Laboratory.
#
# This library is free software; you can redistribute it and/or modify it
# under the terms of the GNU Lesser General Public License version 2.1 as
# published by the Free Software Foundation.

# Libged commands publish either a compact bu_opt-backed specification, a full
# native schema, or a parser-owned grammar adapter for validation, completion,
# and grammar introspection.  This source-level test supplements the runtime
# schema audit by flagging an option-only bu_opt parser that does not publish
# its higher-level command structure.
if(NOT DEFINED SOURCE_DIR OR "${SOURCE_DIR}" STREQUAL "")
  message(FATAL_ERROR "check_native_schema_sources.cmake requires -DSOURCE_DIR=...")
endif()

set(scan_roots
  "${SOURCE_DIR}/src/libged"
)
set(source_files)
foreach(root IN LISTS scan_roots)
  file(GLOB_RECURSE root_files LIST_DIRECTORIES FALSE
    "${root}/*.c"
    "${root}/*.cc"
    "${root}/*.cpp"
    "${root}/*.cxx"
    "${root}/*.h"
    "${root}/*.hpp"
  )
  list(APPEND source_files ${root_files})
endforeach()
list(SORT source_files)

# These commands still have independently owned execution parsers which
# cannot yet bind directly to a schema argument record (for example, check's
# parser forwards options to an external process).  Pin the reviewed adapter
# surface by file and descriptor count: adding an unbound row, or introducing
# one in another command, must be an explicit audit update rather than silent
# grammar drift.
set(unbound_adapter_allowlist
  "src/libged/attr/attr.cpp|1"
  "src/libged/check/check.c|24"
  "src/libged/comb/comb.c|1"
  "src/libged/db/db.c|2"
  "src/libged/edit/edit.cpp|24"
  "src/libged/joint/joint.c|8"
  "src/libged/mater/mater.cpp|4"
  "src/libged/material/material.c|2"
  "src/libged/pipe/pipe.c|1"
  "src/libged/rot/rot.c|2"
  "src/libged/tra/tra.c|2"
  "src/libged/view/view.c|8"
)

# draw has a reviewed legacy bu_opt execution adapter alongside the canonical
# native schema used by command analysis and the new command form.  Keep this
# exception explicit: another split execution/metadata parser must trigger a
# source-audit decision rather than being accepted merely because a native
# schema is registered somewhere in the file.
set(bu_opt_native_adapter_allowlist
  "src/libged/draw/draw.c"
)

set(violations)
foreach(source_file IN LISTS source_files)
  file(RELATIVE_PATH relative_file "${SOURCE_DIR}" "${source_file}")
  # Unit-test fixtures intentionally exercise bu_opt and malformed/unbound
  # schemas without registering production commands.  They are consumers of
  # the command API, not command definitions subject to this source audit.
  if(relative_file MATCHES "^src/libged/tests/")
    continue()
  endif()
  # These sources have no command parsing role.  Documentation-audit code
  # recognizes historical BU_OPT examples, and dbi_state uses scalar readers
  # for database attributes.  Standalone tools and non-libged libraries
  # intentionally remain free to use the supported bu_opt facade; only
  # interactive libged command definitions require native schemas.
  set(compatibility_sources
    "src/libged/dbi_state.cpp"
    "src/libged/ged_init.cpp"
    "src/libged/include/plugin.h"
  )
  list(FIND compatibility_sources "${relative_file}" compatibility_index)
  if(NOT compatibility_index EQUAL -1)
    continue()
  endif()
  file(READ "${source_file}" contents)

  # Reentrant bu_opt builders should use the typed field-binding rows.  A raw
  # row remains appropriate when a custom callback intentionally receives the
  # whole argument record, but spelling out the common conditional field
  # address defeats the concise builder API and invites copy/paste mistakes.
  if(contents MATCHES
      "\\{[^\n]*[?][ \t]*&[A-Za-z_][A-Za-z0-9_]*->[A-Za-z_][A-Za-z0-9_.>-]*[ \t]*:[ \t]*NULL")
    list(APPEND violations
      "${relative_file} (raw conditional bu_opt field binding; use BU_OPT_* row helpers)"
    )
  endif()

  # Compact GED declarations normalize one flat typed rule table.  Do not
  # regress to the former forest of per-kind sidecar arrays or out-of-line
  # alias metadata in production command sources.
  if(contents MATCHES
      "static[ \t\r\n]+const[ \t]+(struct[ \t]+)?ged_opt_meta")
    list(APPEND violations
      "${relative_file} (legacy compact-option metadata sidecar)"
    )
  endif()
  if(contents MATCHES
      "static[ \t\r\n]+const[ \t]+(struct[ \t]+)?(bu_opt_value_spec|ged_opt_semantic|ged_opt_form|ged_opt_db_completion|ged_opt_db_type_case)[ \t\r\n]+[A-Za-z_]")
    list(APPEND violations
      "${relative_file} (legacy compact-option per-kind sidecar; use one ged_opt_rule table)"
    )
  endif()
  if(contents MATCHES "BU_OPT_VALUE_ALIAS")
    list(APPEND violations
      "${relative_file} (out-of-line option alias; use GED_RULE_ALIAS)"
    )
  endif()

  string(REGEX MATCHALL "BU_CMD_(FLAG|VALUE|SHAPED)_UNBOUND" unbound_rows "${contents}")
  list(LENGTH unbound_rows unbound_count)
  set(expected_unbound_count -1)
  foreach(adapter IN LISTS unbound_adapter_allowlist)
    string(REPLACE "|" ";" adapter_fields "${adapter}")
    list(GET adapter_fields 0 adapter_file)
    list(GET adapter_fields 1 adapter_count)
    if("${relative_file}" STREQUAL "${adapter_file}")
      set(expected_unbound_count "${adapter_count}")
      break()
    endif()
  endforeach()
  if(unbound_count GREATER 0 AND expected_unbound_count LESS 0)
    list(APPEND violations
      "${relative_file} (unreviewed unbound schema adapter)"
    )
  elseif(expected_unbound_count GREATER_EQUAL 0 AND
      NOT unbound_count EQUAL expected_unbound_count)
    list(APPEND violations
      "${relative_file} (expected ${expected_unbound_count} unbound rows, found ${unbound_count})"
    )
  endif()

  # bu_getopt and its bu_opt* state variables are a separate getopt-compatible
  # API.  Match only the descriptor/parser facade and its reader
  # callbacks, not such names as bu_optind or bu_optarg.
  string(REGEX MATCH
    "(bu_opt_(parse|describe|bool|int|long|long_hex|fastf_t|char|str|vls|color|vect_t|incr_long|lang|man_section|validate_[A-Za-z0-9_]+)|bu_opt_(desc|cmd_desc|operand_desc|validate_result|value_type_t)|BU_OPT_[A-Za-z0-9_]+)"
    legacy_reference
    "${contents}"
  )
  if(legacy_reference)
    string(FIND "${contents}" "GED_DECLARE_COMMAND_SET_WITH_OPT_SPEC" opt_spec_registration)
    string(FIND "${contents}" "GED_DECLARE_COMMAND_SET_WITH_MIXED_SCHEMA" mixed_spec_registration)
    list(FIND bu_opt_native_adapter_allowlist "${relative_file}" native_adapter_index)
    if(opt_spec_registration EQUAL -1 AND mixed_spec_registration EQUAL -1 AND
        native_adapter_index EQUAL -1)
      list(APPEND violations "${relative_file} (bu_opt use without a registered command specification)")
    endif()
  endif()
endforeach()

if(violations)
  list(JOIN violations "\n  " listed)
  message(FATAL_ERROR
    "Unregistered bu_opt or unreviewed schema adapter found:\n  ${listed}\n"
    "Libged commands must publish a GED option specification, native schema, or grammar adapter.")
endif()
