# Tab completion design

This is the developer overview of BRL-CAD command completion.  It describes
the boundary between command authors, libbu/libged, and the interactive
frontends.  For the compact schema-authoring reference, see
[`command_schemas.md`](command_schemas.md).  For the user-visible interaction
and configuration, see [`brlcad-completion(5)`](asciidoc/system/man5/brlcad-completion.adoc).

## Purpose and model

Completion is one projection of a command description.  The same description
also supplies option parsing, structural validation, generated help, semantic
feedback, linting, and machine-readable grammar publication.  This prevents
the usual drift between a command's parser, help text, and editor behavior.

The system deliberately separates three jobs:

```text
command description -> parse and validate -> completion role + replacement span
                                             |
                                             v
                                  semantic provider -> bounded candidates
                                             |
                                             v
                         GSH / MGED / QGED -> preview and transient display
```

* A command description answers what syntax is valid.
* A provider answers which runtime values are valid in the current GED and
  database context.
* A frontend decides how to preview, cycle, and display the returned values.

The latter must not invent command syntax or reimplement database filtering.

## Command descriptions

Libged commands publish native `bu_arg_schema`, `bu_arg_tree`, or
`ged_cmd_grammar` descriptions. The native declaration binds execution state
and also supplies option/operand types, validation, help, and completion.

Use a native `bu_arg_schema` only for syntax the compact form cannot express:
multi-token values, repeated heterogeneous groups, command trees, or unusually
complex structural constraints.  Use a `ged_cmd_grammar` adapter when an
established parser owns a real language that should not be duplicated in a
schema.  Command-owned callbacks refine the result of declarative validation;
they do not recursively parse the command or duplicate static rules.

Schema value types select standard providers where appropriate.  For example,
`object`, `path`, and `file` select the standard GED object/path/file services.
Choose the narrowest object provider that reflects command semantics: ordinary
geometry commands should not advertise `_GLOBAL` or other normally hidden
objects, while commands that support them can request an explicitly broader
provider.  An option-dependent rule or validator can select the broader
meaning only when its enabling option is active.

All registered descriptions are linted.  The registry audit intentionally
rejects a libged command that has only a `bu_opt` parser but no corresponding
GED metadata: parsing alone does not communicate positional syntax or runtime
completion policy.

## Current command-description survey

The following is a snapshot of the built-in registry in this branch.  It is
included to help an author find a comparable command before choosing a
representation.  The registry contains **353 published command descriptions**.
Those are command spellings, so aliases and compatibility entry points count
separately when they deliberately publish separate syntax.

The registry publishes normalized native schemas, native forms and trees, and
grammar adapters. Consumers do not need to distinguish how a command source
organizes its native declaration.

Frontend code owns presentation timing, key bindings, colors, and scrolling.
Libged owns candidate semantics, escaping, and replacement spans.  Keeping
that boundary is essential for matching behavior across terminal, Tk, and Qt
consoles.

## Extension and verification checklist

When adding or changing a command:

1. Start with a native schema or tree; use a grammar adapter only when an
   established parser owns syntax that should not be duplicated.
2. Give every positional value a precise type/provider and encode aliases and
   option-dependent forms in metadata or a small validation rule.
3. Prefer a narrow object/path provider.  Test both ordinary and
   hidden-object-enabled option states when applicable.
4. Run schema lint and add completion corpus coverage for command examples,
   aliases, quoted input, replacement ranges, and invalid/incomplete states.
5. For a new provider, implement the bounded-query contract and test its
   truncation, filter, common-prefix, and large-data behavior.
6. For a frontend change, test preview acceptance/cancellation, transient-row
   cleanup, resize relayout, and constrained viewport scrolling.

Useful test areas are `src/libbu/tests/test_arg.c`,
`src/libbu/tests/test_opt.c`, and the libged command-analysis, completion
corpus, lifecycle, and scale tests.  The scale tests exercise large database
object sets and the bounded policy-view cache; treat a regression there as an
interactive usability defect, not merely a benchmark change.
