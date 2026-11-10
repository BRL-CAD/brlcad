#       C H E C K _ N O _ L E G A C Y _ B U O P T . C M A K E
# BRL-CAD
#
# Copyright (c) 2026 United States Government as represented by
# the U.S. Army Research Laboratory.
#
# This library is free software; you can redistribute it and/or modify it
# under the terms of the GNU Lesser General Public License version 2.1 as
# published by the Free Software Foundation.

# Libged commands publish a native schema or parser-owned grammar adapter for
# validation, completion, and grammar introspection.  This source-level test
# supplements the runtime schema audit by pinning the reviewed set of unbound
# native-schema adapters.
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
  "src/libged/comb/comb.c|1"
  "src/libged/db/db.c|2"
  "src/libged/edit/edit.cpp|24"
  "src/libged/mater/mater.cpp|4"
  "src/libged/material/material.c|2"
  "src/libged/pipe/pipe.c|1"
  "src/libged/view/view.c|8"
)

set(violations)
foreach(source_file IN LISTS source_files)
  file(RELATIVE_PATH relative_file "${SOURCE_DIR}" "${source_file}")
  # Unit-test fixtures intentionally exercise malformed or unbound schemas.
  # They are consumers of the command API, not production definitions subject
  # to this source audit.
  if(relative_file MATCHES "^src/libged/tests/")
    continue()
  endif()
  # These sources have no command parsing role.  dbi_state uses scalar readers
  # for database attributes; the registry and declaration header implement the
  # command infrastructure being audited.
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

  string(REGEX MATCHALL "BU_ARG_UNBOUND" unbound_rows "${contents}")
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

endforeach()

if(violations)
  list(JOIN violations "\n  " listed)
  message(FATAL_ERROR
    "Unreviewed native schema adapter found:\n  ${listed}\n"
    "Libged commands must publish a native schema or grammar adapter.")
endif()
