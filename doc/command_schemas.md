# Libged command schemas

Libged commands publish a versioned `bu_opt_cmd_desc` alongside their plugin
entry.  The schema is the machine-readable contract used by command-line and
GUI front ends; command implementations and their existing help text remain
authoritative while the schema migration is in progress.

Each schema can describe:

- ordinary options and canonical aliases;
- required, optional, repeated, conflicting, and dependent options;
- scalar, multi-token, comma-list, key/value, axis-keyed, range, and custom
  argument shapes;
- typed positional operands, including database objects and paths, filesystem
  paths, views, colors, vectors, matrices, keywords, and raw pass-through data;
- nested subcommands and phase-specific parsing policies;
- named semantic providers for database- or runtime-aware validation and
  completion; and
- a custom incremental validator for grammars that cannot be expressed by the
  declarative fields alone.

Plugins register schemas with `GED_DECLARE_COMMAND_SET_WITH_SCHEMA` and
`GED_DECLARE_PLUGIN_MANIFEST_WITH_SCHEMA`.  Aliases normally point at a shared
schema; use an alias-specific descriptor when argument order or behavior
differs.  Schema version 1 is independent of the plugin ABI version.

## Consumer API

`ged_cmd_validate` incrementally validates a command line and reports the
expected token class, typed completion role, token/character range, semantic
state, and candidates.  `ged_cmd_analyze` reports those roles for every token.
`ged_cmd_complete_result` is the preferred completion entry point: it also
returns the exact replacement range and prefix needed by an editor widget,
the active command/subcommand path and role, whether options remain legal,
and whether the next token enters a new subcommand phase.
Callers release results with `bu_opt_validate_result_clear`,
`ged_cmd_analysis_clear`, or `ged_cmd_completion_result_clear` respectively.

GSH, the MGED bridge, and QGED's `GEDShellCompleter` consume the common result
API.  Front ends should preserve the supplied candidate order and replacement
range rather than reparsing slash-separated paths or dumping command output to
discover possible values.

## Semantic and filesystem providers

Provider names are stable strings.  Built-in providers cover database objects
and paths, search paths/types, views, command names, primitive types, and the
combined database-object/primitive role used by descriptor-driven editing.
Schema lint rejects unresolved provider names.

`BU_OPT_VAL_FILE_PATH` requests common filesystem completion.  Libged delegates
to `bu_file_complete`, whose flags control hidden entries, directory-only
results, and trailing separators; callers may also supply an extension filter.
This keeps CLI filtering, sorting, prefix preservation, and platform path
handling consistent while allowing a GUI to substitute a native file dialog.

## Migration and linting

Every registered libged command has a schema-bearing manifest entry.  External
engine commands model libged-owned boundaries conservatively and leave unknown
downstream arguments as raw pass-through tokens.  Rich command families use
nested trees or descriptor-backed completion rather than flattened option
lists.

`ged_cmd_schema_lint` checks registered schemas for duplicate option names,
invalid shapes/policies, unresolved providers, and malformed canonical alias
metadata.  The libged command-analysis tests load both `moss.g` and `m35.g` to
exercise schema publication, semantic states, candidate ordering, nested
completion, and known migration risks.
