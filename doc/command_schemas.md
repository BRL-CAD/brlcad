# Libged command schemas

Libged commands publish one native command description alongside their plugin
entry.  A command description is a flat `bu_cmd_schema`, a nested
`bu_cmd_tree`, a set of context-selected native forms, or a
`ged_cmd_grammar` adapter for a parser-owned language such as `search`.
Execution, help, validation, completion, linting, and machine-readable grammar
publication are projections of that description rather than independently
maintained option tables.

The native descriptions can express:

- ordinary options, aliases, required or optional arguments, and conflicts;
- scalar and multi-token argument shapes, bounded repetitions, and keywords;
- typed positional operands such as database paths, files, colors, vectors,
  matrices, views, and command names;
- nested subcommands and phase-specific option policies;
- named semantic providers for database- or runtime-aware validation and
  completion; and
- a command-owned, side-effect-free validator when a grammar cannot be
  represented declaratively.

Commands register a schema with `GED_DECLARE_COMMAND_SET_WITH_NATIVE_SCHEMA`
or a grammar adapter with `GED_DECLARE_COMMAND_SET_WITH_GRAMMAR`.  Aliases
normally share their canonical command's description.  An alias needs a
distinct description only when its accepted syntax differs.

## Consumer API

`ged_cmd_validate` incrementally validates a command line and reports the
expected token class, typed completion role, replacement span, semantic state,
and candidates.  `ged_cmd_analyze` reports those roles and states for every
token.  `ged_cmd_complete_result` is the editor-facing completion entry point;
it returns candidates together with the exact input range to replace and the
active command or subcommand context.

Callers release results with `ged_cmd_validate_result_clear`,
`ged_cmd_analysis_clear`, or `ged_cmd_completion_result_clear`, respectively.
GSH, MGED, and QGED all consume these common libged results.  Frontends choose
how candidates are displayed and accepted, but should preserve the candidate
order and replacement range supplied by libged.

## Machine-readable publication

`ged_cmd_schema_json(command)` returns the complete static grammar known for a
registered GED command.  The caller owns the returned string and releases it
with `bu_free`.  `ged_cmd_schema_exists(command)` reports whether a description
is available, and `ged_cmd_list` supplies the command names to enumerate.

Flat schemas identify themselves as `kind: "native"`; nested descriptions use
`kind: "native_tree"`; form selectors and parser-owned adapters identify their
own grammar kind.  The JSON includes canonical option spellings, aliases,
argument requirements and shapes, typed operands, cardinalities, parse policy,
static keyword vocabularies, semantic-provider names, and subcommand
structure.  This is the publication boundary intended for syntax highlighters,
documentation generators, and adapters that generate an ANTLR or comparable
static grammar.

The JSON deliberately distinguishes static syntax from runtime semantics.
External tools can recognize command structure and fixed vocabularies, but a
provider such as `ged.db_path` is published by name because its accepted values
depend on an open database.  Parser-owned adapters publish the static subset
they can describe without executing a command.

Applications that already own a native schema or tree can publish it directly
with `bu_cmd_schema_describe_json` or `bu_cmd_tree_describe_json`.

The installed geometry shell provides a command-line publication interface:

```sh
gsh --command-schema draw
gsh --command-schema all
```

The first command writes one JSON grammar object.  The second writes a JSON
array containing every discoverable registered grammar, so an external tool
does not need to link libged merely to import the static command language.

## Semantic and filesystem providers

Provider names are stable strings.  Built-in providers cover database objects
and paths, search paths and types, views, command names, primitive types,
attributes, units, colors, vectors, matrices, and files.  Schema lint rejects
an unresolved provider.

`BU_CMD_VALUE_FILE` requests common filesystem completion.  Libged delegates
to `bu_file_complete`, keeping filtering, sorting, prefix preservation, and
platform path handling consistent while allowing a GUI to substitute a native
file dialog.

## Linting and consistency

`ged_cmd_schema_lint(command, messages)` checks one registered description;
passing `NULL` as the command audits the full registry.  Native schema and tree
linting detect malformed rows, duplicate names, invalid shapes and policies,
unresolved providers, and inconsistent alias metadata.

The libged command-analysis and completion-corpus tests audit published JSON,
completion replacement ranges, candidate ordering, round-trip parsing, nested
commands, parser-owned grammars, and semantic states against representative
databases and manual examples.  The static no-legacy check ensures internal
commands use `bu_cmd_schema` rather than the deprecated `bu_opt` compatibility
API.
