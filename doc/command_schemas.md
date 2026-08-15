# Libged command schemas

Libged commands publish one command description alongside their plugin entry.
Most flat commands use a compact `ged_opt_spec`: familiar `bu_opt_desc`
rows define options, while one short `ged_opt_rule` table declares positional
forms and the uncommon relationships that option parsing alone cannot express.  More
complex commands may publish a flat `bu_cmd_schema`, a nested `bu_cmd_tree`, a
set of context-selected native forms, or a `ged_cmd_grammar` adapter for a
parser-owned language such as `search`.
Execution, help, validation, completion, linting, and machine-readable grammar
publication are projections of that description rather than independently
maintained option tables.

The native descriptions can express:

- ordinary options, aliases, required or optional arguments, and conflicts;
- declarative numeric ranges whose bounds are shared by parsing, validation,
  linting, and machine-readable publication;
- scalar and multi-token argument shapes, bounded repetitions, keywords, and
  repeated heterogeneous operand groups such as `(operation, object)+`;
- typed positional operands such as database paths, files, colors, vectors,
  matrices, views, and command names;
- nested subcommands and phase-specific option policies;
- named semantic providers for database- or runtime-aware validation and
  completion; and
- a command-owned, side-effect-free validator when a grammar cannot be
  represented declaratively.

Structural validation and declarative constraints always run before a
command-owned validator.  The callback receives that result and may refine it;
it does not copy the schema or recursively invoke `bu_cmd_schema_validate`.
Context-aware validation follows the same rule after context-free validation.

Ordinary commands register an option specification with
`GED_DECLARE_COMMAND_SET_WITH_OPT_SPEC`.  Advanced schemas and grammar
adapters use `GED_DECLARE_COMMAND_SET_WITH_NATIVE_SCHEMA` and
`GED_DECLARE_COMMAND_SET_WITH_GRAMMAR`, respectively.  Aliases normally share
their canonical command's description.  An alias needs a distinct description
only when its accepted syntax differs.

## Ordinary command declarations

`BU_OPT_DESC_BUILDER` turns one initializer-row macro into a reentrant option
table.  It supplies invocation-local storage during execution and null storage
when libged extracts help or completion metadata.  The row count is inferred,
so adding an option does not require maintaining a parallel count or table.

All standard readers declared by `bu/opt.h` are understood without a sidecar:
flags, booleans, integers, longs, hexadecimal longs, incrementing counters,
finite numbers, characters, C strings, VLS strings, colors, vectors, ISO 639-1
languages, and manual-page sections.  The option spellings are candidates,
and supplied values receive side-effect-free syntax validation based on the
reader.  Color and vector readers retain their one-or-three-token behavior.

An optional `GED_RULE_TYPE`, `GED_RULE_CANDIDATES`,
`GED_RULE_CARDINALITY`, `GED_RULE_SELECT`, or `GED_RULE_VALUE_VALIDATE` row is
needed only for an opaque custom reader or to refine inferred behavior.  A
`GED_RULE_ALIAS` row explicitly maps a compatibility spelling to its canonical
option.  The validator receives the complete option argv, cursor index, opaque
command context, and row data, so an option value may depend on another
selected option without putting that policy in the parser.  Identical metadata
may cover several whitespace-separated option names in one rule.

The `ged_opt_spec` supplies the command-level portion: typed
positional operands, ordering policy, repeated groups, declarative constraints,
and—only when those forms are insufficient—a structural or context-aware
validator.  Libged translates the option builder, compact syntax, and optional
flat rule table to its internal schema engine once at registration.  Parsing still uses the same option-row
builder through `bu_opt_parse_build`, so execution and editor metadata share
the option spellings and reader functions.

Most commands need not initialize `bu_cmd_operand` rows directly.
`GED_OPT` accepts a compact positional description that is parsed
and linted once during registration.  Its first word may select
`options-first`, `interspersed`, or `stop-at-first-operand`; following words
have `name:type` form:

```c
GED_OPT("keep", "Copy selected objects", keep_options,
    "options-first file:file objects:object+")
```

For the uncommon details that go beyond the syntax itself, use one terminated
rule table next to the option builder.  This keeps aliases, forms, constraints,
database roles, semantic refinements, and validators in one readable place:

```c
static const ged_opt_rule bb_opt_rules[] = {
    GED_RULE_OPERANDS(BU_CMD_CONDITION_ANY_OPTION_PRESENT, "o", 1, 1,
        "-o requires exactly one BoT object"),
    GED_RULE_NULL
};
static const ged_opt_spec bb_opt_spec =
    GED_OPT_WITH("bb", "Report object bounds", bb_options,
        "interspersed objects:object+", bb_opt_rules);
```

Use `GED_OPT` when no rules are needed and `GED_OPT_WITH` when they are.
`GED_OPT_FORMS` selects option-dependent positional forms from the rule table;
`GED_OPT_NATIVE` retains an uncommon explicit flat operand table.  Aliases are
never inferred merely because two parser rows happen to write the same field:
shared storage can also represent distinct operations, as with terse and
verbose counters.  State semantic aliases explicitly with `GED_RULE_ALIAS`.

Within a `BU_OPT_DESC_BUILDER` row macro, use `BU_OPT_FLAG`, `BU_OPT_STR`, and
their similarly short peers when a row binds one ordinary field.  The helpers
cover every standard libbu reader (flag,
string, integer, long and hex long, character, number, vls, bool, color,
vector, increment, language, and manual section).  Use the regular six-field
row or `BU_OPT_CUSTOM` for a command-specific reader.

The cardinality suffixes are `?` (zero or one), `*` (zero or more), `+` (one
or more), `{n}`, and `{min,max}`.  `object`, `path`, and `file` automatically
select the standard GED completion providers; another provider may be written
as `type@provider.name`.  Scalar domains and fixed vocabularies stay inline:

```text
mode:keyword(wireframe|shaded)
channel:int(0:255)
samples:positive-int{2,3}
```

The other scalar types are `bool`, `int`, `hex-int`, `long`, `hex-long`,
`number`, `char`, `vector`, `vector3`, `matrix`, `rgb`, `color`, `keyword`,
`string`, `pattern`, `vls`, `raw`, and `custom`.  `rgb`, `color`, and `vector3`
publish their standard one-or-three-token shapes; `pattern` publishes a scalar
pattern shape.  `positive-int`, `nonnegative-int`, `positive-number`, and
`nonnegative-number` provide common validated domains.  Integer and number
types accept an inclusive `(min:max)` domain with either bound omitted.

Keep a native operand row for a command-specific validator or token shape.
Keep native repeated groups, command trees, and grammar adapters for genuinely
non-flat languages.  The textual form is deliberately not a callback-name
registry or a general-purpose parser language.

## Native-schema authoring

Use a native schema only when the compact declaration cannot describe the
command. `BU_CMD_SCHEMA_BOUND` is the preferred declaration: unlike the older
tail macros, it presents every independent command-level property in one
place and permits them to be combined.

```c
static const struct bu_cmd_schema command_schema =
    BU_CMD_SCHEMA_BOUND("command", "Command summary", command_options,
        command_operands, BU_CMD_PARSE_OPTIONS_FIRST, command_groups,
        command_constraints, command_structure_validate,
        command_context_validate);
```

Pass `NULL` for a property that is not needed. The arguments after the parse
policy are, in order: repeated groups, declarative constraints, a
side-effect-free structural validator, and a context-aware validator. Use the
ordinary short `BU_CMD_*` option and operand rows for scalar values. For a
multi-token option, define a named `bu_cmd_arg_shape` and a named consumer;
this is clearer and more reusable than an inline custom parser. Use
`BU_CMD_OPTION_CONSUME` for the common required shape; reserve the fully
explicit `BU_CMD_OPTION_SHAPED` form for an optional argument or a canonical
spelling different from the long option spelling.

`BU_CMD_SCHEMA_EXTERNAL` is specifically for an established command whose
executor still owns option parsing. It makes the schema metadata-only and
permits `BU_CMD_VALUE_UNBOUND` or `BU_CMD_FLAG_UNBOUND` rows for help,
validation, and completion. New commands should use `BU_CMD_SCHEMA_BOUND`;
the native linter rejects unbound rows in a bound schema. An external schema
has no context callback slot, since it must not become a second execution
parser—use a `ged_cmd_grammar` adapter when live context is part of that
legacy language.

Tables are non-owning and must have static lifetime. Terminate options,
operands, groups, constraints, and tree-node lists with their corresponding
`*_NULL` entry. `BU_CMD_ALIASES(...)` and `BU_CMD_TREE(...)` provide compact
child-alias and tree declarations. Choose either simple
keyword strings or rich keyword rows with aliases and help; do not supply
both for one option or operand.

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
static keyword vocabularies, repeated operand groups, semantic-provider names,
and subcommand structure.  This is the publication boundary intended for
syntax highlighters, documentation generators, and adapters that generate an
ANTLR or comparable static grammar.

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

At the lower level, `bu_cmd_schema_lint` checks one flat schema and
`bu_cmd_tree_lint` recursively checks the root and every child schema.

The libged command-analysis and completion-corpus tests audit published JSON,
completion replacement ranges, candidate ordering, round-trip parsing, nested
commands, parser-owned grammars, and semantic states against representative
databases and manual examples.  A static source audit ensures any libged
command using `bu_opt` also registers a `ged_opt_spec`; an option-only
parser is not sufficient command metadata.

## Choosing between `bu_opt` and `bu_cmd_schema`

`bu_opt` remains the concise public API for conventional option-only parsing.
Its descriptor table and callback readers are translated to the same parser
engine used by `bu_cmd_schema`, preserving the established API and argument
behavior without maintaining a second parser.  It is a good fit when a caller
only needs to consume options and retain the leftover arguments.

Use `bu_opt_desc_validate` when an option-only consumer also wants incremental
option completion and standard-reader type checking.  `bu_opt_desc_build`,
`bu_opt_parse_build`, `bu_opt_describe_build`, and `bu_opt_validate_build`
project a reentrant builder into the corresponding operation.

For ordinary libged commands, add a `ged_opt_spec` rather than replacing
the option rows with a second API.  Use `bu_cmd_schema` directly only when a
command genuinely benefits from its advanced typed ranges, forms, or parser
bindings.  For example, an inclusive integer bound is declared directly:

```c
BU_CMD_INTEGER_RANGE("l", "level", struct args, level,
    1, 5, "count", "Refinement level")
```

The parser and validator both enforce the bound, the linter verifies that the
range is compatible with the value type, and JSON consumers receive the same
minimum and maximum.  `bu_cmd_schema_parse_known` supports layered parsers by
consuming recognized options while leaving unknown words in the operand
suffix.
