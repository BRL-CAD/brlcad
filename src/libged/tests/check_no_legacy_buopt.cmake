#       C H E C K _ N O _ L E G A C Y _ B U O P T . C M A K E
# BRL-CAD
#
# Copyright (c) 2026 United States Government as represented by
# the U.S. Army Research Laboratory.
#
# This library is free software; you can redistribute it and/or modify it
# under the terms of the GNU Lesser General Public License version 2.1 as
# published by the Free Software Foundation.

# The native command-schema cutover deliberately leaves bu/opt.h available to
# external users during its deprecation window.  It must not silently return
# to internal command processing.  This source-level test supplements the
# runtime schema audit: it makes a new legacy parser, descriptor, result, or
# reader use fail immediately in CI.
if(NOT DEFINED SOURCE_DIR OR "${SOURCE_DIR}" STREQUAL "")
  message(FATAL_ERROR "check_no_legacy_buopt.cmake requires -DSOURCE_DIR=...")
endif()

set(scan_roots
  "${SOURCE_DIR}/include"
  "${SOURCE_DIR}/src"
  "${SOURCE_DIR}/regress"
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

set(violations)
foreach(source_file IN LISTS source_files)
  file(RELATIVE_PATH relative_file "${SOURCE_DIR}" "${source_file}")
  # The public compatibility API must remain source- and binary-compatible
  # until its documented removal release.  Its implementation and direct
  # regression test, plus the two external extension adapters that still
  # expose it, are intentionally the only in-tree exceptions.
  set(compatibility_sources
    "include/bu/opt.h"
    # The native public header documents the replacement relationship.
    "include/bu/cmdschema.h"
    "src/libbu/opt.c"
    "src/libbu/tests/test_opt.c"
    "include/analyze/nirt.h"
    "src/libanalyze/nirt/opts.cpp"
    "include/gcv/api.h"
    "src/libgcv/gcv.c"
    # This documentation-audit test intentionally recognizes historical
    # BU_OPT examples.  It has no command implementation role.
    "src/libged/tests/check_mann_docs.cpp"
  )
  list(FIND compatibility_sources "${relative_file}" compatibility_index)
  if(NOT compatibility_index EQUAL -1)
    continue()
  endif()
  file(READ "${source_file}" contents)
  # bu_getopt and its bu_opt* state variables are a separate getopt-compatible
  # API.  Match only the retired descriptor/parser API and its reader
  # callbacks, not such names as bu_optind or bu_optarg.
  string(REGEX MATCH
    "(bu_opt_(parse|describe|bool|int|long|long_hex|fastf_t|char|str|vls|color|vect_t|incr_long|lang|man_section|validate_[A-Za-z0-9_]+)|bu_opt_(desc|cmd_desc|operand_desc|validate_result|value_type_t)|BU_OPT_[A-Za-z0-9_]+)"
    legacy_reference
    "${contents}"
  )
  if(legacy_reference)
    list(APPEND violations "${relative_file}")
  endif()
endforeach()

if(violations)
  list(JOIN violations "\n  " listed)
  message(FATAL_ERROR
    "Deprecated bu_opt API found in internal source:\n  ${listed}\n"
    "Use bu/cmdschema.h or a native grammar adapter instead.")
endif()
