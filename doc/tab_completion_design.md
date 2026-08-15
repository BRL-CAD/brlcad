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

`bu_opt` remains the normal option parser for conventional commands.  Standard
`bu_opt_desc` readers provide option spellings and enough type information for
basic option and option-value completion.  A `BU_OPT_DESC_BUILDER` keeps such
descriptions reentrant: execution obtains storage appropriate to that
invocation, while metadata consumers obtain a storage-free view.

A libged command using `bu_opt` normally supplies a `ged_opt_spec` alongside
its option builder.  `GED_OPT` declares ordinary typed operands compactly;
`GED_OPT_WITH` adds a small `ged_opt_rule` table for aliases, constraints,
option-selected forms, and context-sensitive refinements.  This is the
preferred representation for an ordinary GED command.  It preserves the
existing parser while making its full command language available to tools.

Use a native `bu_cmd_schema` only for syntax the compact form cannot express:
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

## Completion query contract

`ged_cmd_validate()` is the lightweight, non-enumerating operation.  It parses
the line at a byte cursor and reports the active token's expected syntax,
semantic type, and source range.  `ged_cmd_analyze()` applies the same model to
every token for editor coloring and diagnostics.

An editor normally calls `ged_cmd_complete_query()` with a
`ged_cmd_completion_request`.  It returns a `ged_cmd_completion_result`:

* `replacement_start` and `replacement_end` are the exact original-input byte
  range to replace; do not infer them from whitespace or from candidate text.
* candidates are complete replacements for that range, in the provider's
  stable order;
* `total_count`, `truncated`, and `common_prefix` describe the whole matching
  set, even when only a bounded display-sized prefix was materialized; and
* `completion_type`, expected syntax, hint, and active command path let a
  frontend explain or style the result without reparsing it.

Initialize result structures with their supplied initializer/init function and
release them with the matching `*_clear` function.  Public validation,
analysis, and completion calls are transactional: success replaces a prior
result; failure leaves it initialized and empty.  This makes a stale candidate
list impossible if a query fails.

Use `max_candidates` for interactive requests.  Zero intentionally asks for a
complete set and is appropriate only for consumers that actually need one.
The older `ged_cmd_complete_result()` remains useful for compatibility, but a
frontend that has a display budget should use the bounded query API.

Input is parsed with frontend-appropriate quoting before libged is consulted.
The returned spans still refer to the original input buffer.  Consequently,
frontends must preserve those spans rather than attempting to map decoded argv
tokens back to source text themselves.

## Semantic providers

A semantic provider is registered under a stable name and supplies validation
and/or completion for a typed runtime value.  Built-in `ged.*` names are
reserved; extensions use their own namespace.  The registry copies the
provider record and name, but not its `data` pointer, so the provider and all
referenced data must remain valid until libged shuts down.

Providers should implement `complete_query` and declare
`GED_CMD_PROVIDER_BOUNDED_QUERY`.  A bounded provider must:

1. apply `candidate_filter` before imposing `max_candidates`;
2. report the exact full filtered count;
3. set a common prefix when the returned candidate array is truncated; and
4. return replacements matching the supplied seed.  A database-path provider
   instead matches and replaces the component after the last unescaped slash.

GED supplies read-only parsed command context (`argc`, `argv`, and
`cursor_arg`) to a provider.  Use it to make values depend on earlier operands
or options, rather than making a frontend guess that state.  The optional
candidate filter carries a scalar predicate inherited from the active schema
value; providers must honor it before applying the limit.

Legacy providers without `complete_query` still work through a compatibility
path, but can require full enumeration.  New providers must be bounded to
avoid unbounded allocations and latency on large databases.

## Database object and path completion

Database object completion uses a private, per-`ged` C++ index.  No C++ or STL
object crosses a public C API boundary.  On the first query after database
change it collects directory names, encodes path-significant characters, and
builds lexical and natural-order indexes.  A nonempty prefix is located with a
lexical range lookup rather than scanning every database object.  The empty
seed is inherently global, but returns only the requested bounded prefix.

Policy-specific views (for example, ordinary geometry versus a hidden-object
capable command) are built lazily and cached with an LRU bound of 16 views.
This avoids multiplying cold-query cost and retained memory by every possible
policy combination.  Database change callbacks mark the index dirty; the next
query refreshes it.  GED teardown and database close explicitly release it.

Path completion is hierarchical.  After a parent path is supplied, libged
walks the applicable combination tree and queries only children of that parent;
it does not form every full database path and filter it afterward.  For APIs
with a parent-path operand and a leaf operand, the same narrowed subtree is
used.  The root/no-parent case is the only naturally database-wide path query.

This design gives prefix queries approximately logarithmic range location plus
work proportional to returned/matching candidates, rather than an O(database
size) scan for each keystroke.  Keep new provider implementations within the
same bounded-query discipline.

## Frontend responsibilities

GSH, classic MGED, Tk MGED, and QGED consume the same libged result and should
preserve candidate order and replacement ranges.  They share completion modes:
filter (the default), cycle, prefix, legacy, and off.  A filtered preview is
presentation-only; it must never be inserted into the command or history until
the user accepts it.

Candidate rows are transient UI state, not terminal output or command text.
Tk MGED and QGED insert styled transient text inside the console; GSH and
classic MGED render equivalent rows below the input.  Before any edit,
acceptance, cancellation, or new query, erase the transient display.  Recompute
the layout on resize; never reuse a layout calculated for a different viewport.

The row budget reserves the input row(s) and at least one third of the console
for prior output.  Completion may use the remaining space, up to two thirds of
the viewport.  If necessary, move the input upward only enough to meet the
budget.  If every candidate will not fit, summarize it as a prefix frontier;
the underlying Tab cycle still visits individual candidates.  This is what
allows a large database to remain usable without requesting or drawing an
unbounded list.

Frontend code owns presentation timing, key bindings, colors, and scrolling.
Libged owns candidate semantics, escaping, and replacement spans.  Keeping
that boundary is essential for matching behavior across terminal, Tk, and Qt
consoles.

## Extension and verification checklist

When adding or changing a command:

1. Start with `bu_opt` plus `ged_opt_spec`; choose native schemas or grammar
   adapters only when the syntax genuinely requires them.
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

Useful test areas are `src/libbu/tests/test_cmdschema.c`,
`src/libbu/tests/test_opt.c`, and the libged command-analysis, completion
corpus, lifecycle, and scale tests.  The scale tests exercise large database
object sets and the bounded policy-view cache; treat a regression there as an
interactive usability defect, not merely a benchmark change.
