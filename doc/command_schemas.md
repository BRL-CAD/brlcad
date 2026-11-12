# Libged command schemas

Libged commands publish one command description alongside their plugin entry.
Flat commands use `bu_arg_schema`; nested command families use `bu_arg_tree` or
context-selected native forms; parser-owned languages such as `search` expose a
`ged_cmd_grammar` adapter. Execution, help, validation, completion, linting,
and machine-readable publication are projections of the same native
description.

Native schemas express:

- options, aliases, required or optional arguments, and conflicts;
- numeric ranges, keywords, scalar and multi-token argument shapes;
- typed positional operands and repeated heterogeneous operand groups;
- nested subcommands and phase-specific option policies;
- semantic providers for context-aware validation and completion; and
- side-effect-free custom validators when declarative rules are insufficient.

## Flat commands

Define invocation state in one record and bind each option with the
`BU_ARG_FLAG`, `BU_ARG_VALUE`, `BU_ARG_CUSTOM`, or shaped-value macros. A
terminated `bu_arg_operand` array describes positional values. The schema
selects `BU_ARG_PARSE_INTERSPERSED`, `BU_ARG_PARSE_OPTIONS_FIRST`, or
`BU_ARG_PARSE_STOP_AT_FIRST_OPERAND`.

```c
struct keep_args {
    int help;
    const char *output;
};

static const struct bu_arg_option keep_options[] = {
    BU_ARG_FLAG("h", "help", struct keep_args, help,
        "Print help and exit"),
    BU_ARG_VALUE("o", "output", struct keep_args, output,
        BU_ARG_VALUE_STRING, BU_ARG_REQUIRED, "file",
        "Write the result to file"),
    BU_ARG_OPTION_NULL
};

static const struct bu_arg_operand keep_operands[] = {
    BU_ARG_OPERAND("objects", BU_ARG_VALUE_DB_OBJECT, 1,
        BU_ARG_COUNT_UNLIMITED, "Objects to copy", "ged.db.geometry"),
    BU_ARG_OPERAND_NULL
};

static const struct bu_arg_schema keep_cmd_schema = {
    "keep", "Copy selected objects", keep_options, keep_operands,
    BU_ARG_PARSE_OPTIONS_FIRST, BU_ARG_SCHEMA_CONSTRAINTS(NULL, NULL)
};
```

Execution calls `bu_arg_schema_parse` when command code performs additional
operand handling, or `bu_arg_schema_parse_complete` when the declaration fully
describes accepted syntax. Both return the index of the first operand.
`bu_arg_schema_help` supplies consistent user-facing help.

Register flat schemas with
`GED_DECLARE_COMMAND_SET_WITH_NATIVE_SCHEMA`. Plugin builds use
`GED_DECLARE_PLUGIN_MANIFEST_WITH_NATIVE_SCHEMA`. A translation unit
containing both native schemas and grammar adapters uses the corresponding
`WITH_NATIVE_SCHEMA_AND_GRAMMAR` forms.

## Nested commands

Use `bu_arg_tree` when a command selects child operations. Each node owns a
native schema and may select children after options, after fixed operands, or
at the first operand. `bu_arg_tree_dispatch` and the validation APIs share
that structure, preventing execution and completion from disagreeing about
child boundaries.

Use `bu_arg_schema_option_span` when parser-owned code must locate a child
after options. It understands attached arguments and multi-token values, so
an option value cannot be mistaken for a subcommand.

## Validation

Declarative structural validation runs before a schema callback. Custom
callbacks refine the initialized result and must not modify command state.
Context-aware validation follows the same rule and receives the owning GED
context through the adapter.

Semantic providers name runtime vocabularies such as database objects, views,
files, or command names. They belong on the option or operand that consumes
the value. Keep database and application policy in libged rather than in
libbu.

## Publication

Registered schemas participate in `ged_cmd_schema_json`, `ged_cmd_help`,
`ged_cmd_validate`, `ged_cmd_complete_result`, `ged_cmd_analyze`, and
`ged_cmd_schema_lint`. Grammar adapters should delegate to native schema/tree
functions wherever possible and implement only syntax that cannot be modeled
natively.

## Documentation auditing

The `argdoc` maintenance executable renders canonical AsciiDoc synopsis and
option fragments from the JSON description of a BU_ARG schema. It can read a
saved JSON file or run a command whose standard output is the description:

```
argdoc --render -- 3dm-g --command-schema
argdoc --check --man-page doc/asciidoc/system/man1/3dm-g.adoc \
  -- 3dm-g --command-schema
argdoc --render --name draw -- gsh --command-schema all
```

Standalone programs participating in this audit reserve the exact invocation
`--command-schema` as a hidden, side-effect-free request that prints their
schema JSON and exits successfully. GED command descriptions are available
collectively from `gsh --command-schema all`.

The published JSON object contains `schema_version`. Consumers must reject
versions they do not understand rather than inferring compatibility from
individual members. Additive changes retain the current version; incompatible
changes require a version increment and coordinated consumer updates.

With testing enabled, the repository registers `argdoc_*` tests for every
introspection-enabled standalone program and one `argdoc_ged_commands` test for
the complete GED registry. The GED audit matches top-level schema names to
`mann` page filenames and requires a reason for every intentionally
undocumented internal schema in
`src/util/tests/argdoc_ged_undocumented.txt`. New public schemas therefore
cannot silently omit documentation, and stale exemptions fail once a page is
added.

Standalone hosts use one representation-neutral `bu_arg_grammar` declaration
and call `bu_arg_introspect` before ordinary parsing. The helper recognizes and
generates the reserved response but does not print, log, or exit; stream and
process policy remain with the program.

Audit mode compares documented option spellings and argument presence with the
schema. Hidden aliases may be documented but are not required. Update mode only
replaces regions explicitly delimited by these markers, so manually maintained
prose is never inferred or overwritten:

```
// BEGIN GENERATED BU_ARG SYNOPSIS
// END GENERATED BU_ARG SYNOPSIS
// BEGIN GENERATED BU_ARG OPTIONS
// END GENERATED BU_ARG OPTIONS
```
