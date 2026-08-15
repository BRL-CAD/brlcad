/*                    C M D S C H E M A . H
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this file; see the file named COPYING for more information.
 */
/** @file bu/cmdschema.h
 *
 * Single-source command option and operand schemas.
 *
 * A schema records command syntax, validation metadata, and storage bindings
 * in one table.  bu_cmd_schema_parse is the execution-facing parser; command
 * registries publish this native representation directly.  The succinct
 * bu_opt API remains available as an option-only facade over the same parsing
 * engine for callers that do not need operands, completion, or publication.
 */

#ifndef BU_CMDSCHEMA_H
#define BU_CMDSCHEMA_H

#include "common.h"

#include <stddef.h>

#include "vmath.h"
#include "bu/defines.h"
#include "bu/vls.h"

#if defined(__cplusplus) && (defined(__clang__) || defined(__GNUC__) || defined(_MSC_VER))
#  define BU_CMD_STORAGE_OFFSET(_record, _field) \
    (offsetof(_record, _field) + \
	0 * sizeof(char[__is_standard_layout(_record) ? 1 : -1]))
#else
#  define BU_CMD_STORAGE_OFFSET(_record, _field) offsetof(_record, _field)
#endif

__BEGIN_DECLS

struct bu_color;

/** Values accepted by a compact command-schema option. */
typedef enum {
    BU_CMD_VALUE_FLAG = 0,
    BU_CMD_VALUE_BOOL,
    BU_CMD_VALUE_INTEGER,
    BU_CMD_VALUE_HEX_INTEGER,
    BU_CMD_VALUE_LONG,
    BU_CMD_VALUE_HEX_LONG,
    BU_CMD_VALUE_NUMBER,
    BU_CMD_VALUE_CHAR,
    BU_CMD_VALUE_VECTOR,
    BU_CMD_VALUE_MATRIX,
    BU_CMD_VALUE_COLOR,
    BU_CMD_VALUE_KEYWORD,
    BU_CMD_VALUE_STRING,
    BU_CMD_VALUE_VLS,
    BU_CMD_VALUE_DB_OBJECT,
    BU_CMD_VALUE_DB_PATH,
    BU_CMD_VALUE_FILE,
    BU_CMD_VALUE_RAW,
    BU_CMD_VALUE_CUSTOM,
    /* A frontend may not yet know the syntactic role's scalar type. */
    BU_CMD_VALUE_UNKNOWN
} bu_cmd_value_t;


/**
 * An option either has a required argument, an optional argument, or no
 * argument.  Required is deliberately the zero value so a compact manually
 * initialized typed option remains a required-argument option.
 */
typedef enum {
    BU_CMD_ARG_REQUIRED = 0,
    BU_CMD_ARG_OPTIONAL,
    BU_CMD_ARG_NONE
} bu_cmd_arg_requirement_t;


/** Machine-readable form of a value or multi-token option argument. */
typedef enum {
    BU_CMD_ARG_SHAPE_SCALAR = 0,
    BU_CMD_ARG_SHAPE_TOKEN_SEQUENCE,
    BU_CMD_ARG_SHAPE_COMMA_LIST,
    BU_CMD_ARG_SHAPE_KEY_VALUE_LIST,
    BU_CMD_ARG_SHAPE_AXIS_KEYED,
    BU_CMD_ARG_SHAPE_RANGE_PATTERN,
    BU_CMD_ARG_SHAPE_RGB,
    BU_CMD_ARG_SHAPE_COLOR,
    BU_CMD_ARG_SHAPE_VECTOR3,
    BU_CMD_ARG_SHAPE_CUSTOM
} bu_cmd_arg_shape_kind_t;


/**
 * Select the number of following words consumed by a variable-width option
 * shape.  available has already been bounded by the shape's max_tokens.
 * Return a value from zero through available.  This is primarily for optional
 * arguments whose first word is distinguishable from the following positional
 * operand; the selector must not modify argv or command state.
 */
typedef size_t (*bu_cmd_arg_token_count_t)(size_t available, const char **argv);


/**
 * One human- and machine-readable representation of a shaped argument.  The
 * syntax string is a compact metavar expression such as "quaternion" or
 * "qx qy qz qw"; token_count records its argv width.  Validation remains
 * owned by the shape kind/consumer, but these rows make every accepted width
 * and spelling visible to help, JSON, and lint consumers.
 */
struct bu_cmd_arg_variant {
    const char *name;
    const char *syntax;
    size_t token_count;
    const char *help;
};


/**
 * Token cardinality and presentation information for an option argument.
 * A NULL shape means one scalar token for a required argument or zero/one
 * tokens for an optional argument.  Comma/key-value/etc. shapes are normally
 * one lexical token; a command-specific consumer validates their internal
 * grammar when needed.
 */
struct bu_cmd_arg_shape {
    bu_cmd_arg_shape_kind_t kind;
    size_t min_tokens;
    size_t max_tokens;
    const char *description;
    bu_cmd_arg_token_count_t token_count;
    const struct bu_cmd_arg_variant *variants;
};

/**
 * Standard optional scalar argument shape.  It consumes one following token
 * unless there is no token or the next token begins an option.  A caller that
 * needs an optional literal beginning with '-' may use the attached
 * --option=value spelling.  This is the normal optional-text behavior for
 * command options such as --near [object].
 */
BU_EXPORT extern const struct bu_cmd_arg_shape bu_cmd_optional_scalar_arg_shape;


/** Option/operand ordering policy. */
typedef enum {
    BU_CMD_PARSE_INTERSPERSED = 0,
    BU_CMD_PARSE_OPTIONS_FIRST,
    BU_CMD_PARSE_STOP_AT_FIRST_OPERAND
} bu_cmd_parse_policy_t;

typedef enum {
    BU_CMD_VALIDATE_UNKNOWN = 0,
    BU_CMD_VALIDATE_VALID,
    BU_CMD_VALIDATE_INCOMPLETE,
    BU_CMD_VALIDATE_INVALID
} bu_cmd_validate_state_t;

typedef enum {
    BU_CMD_EXPECT_NONE = 0,
    BU_CMD_EXPECT_OPTION = 1,
    BU_CMD_EXPECT_OPTION_ARG = 2,
    BU_CMD_EXPECT_OPERAND = 4,
    BU_CMD_EXPECT_SUBCOMMAND = 8
} bu_cmd_expected_t;


/**
 * A custom parser consumes one option argument and writes to storage.
 * Validation invokes it with a NULL storage pointer; implementations must use
 * that form only for side-effect-free syntax checking.
 */
typedef int (*bu_cmd_value_parse_t)(struct bu_vls *msg, const char *arg, void *storage);

/**
 * Side-effect-free additional validation for an otherwise typed value.
 * Return zero for an accepted value.  This is useful for domain-specific
 * constraints that cannot be expressed with a declarative range or keyword
 * vocabulary, without losing the option's integer/number/string type to
 * completion and highlighting consumers.
 */
typedef int (*bu_cmd_value_validate_t)(struct bu_vls *msg, const char *arg);


/**
 * Parse a complete option argument shape.  argc is the number of option
 * argument tokens selected by the shape.  A NULL storage pointer requests a
 * side-effect-free syntax check; return zero on success.  Consumers are used
 * for multi-token values and command-specific mini-languages.  They receive
 * the address selected by storage_offset, not the enclosing argument record.
 */
typedef int (*bu_cmd_value_consume_t)(struct bu_vls *msg, size_t argc,
	const char **argv, void *storage);

/**
 * Standard one-or-three-token 8-bit RGB argument shape.  The matching
 * consumer accepts packed r/g/b, r,g,b, or r;g;b input, or three separate
 * channels, and stores a struct bu_color in the selected storage field.
 */
BU_EXPORT extern const struct bu_cmd_arg_shape bu_cmd_rgb_arg_shape;
BU_EXPORT extern int bu_cmd_rgb_consume(struct bu_vls *msg, size_t argc,
	const char **argv, void *storage);

/**
 * Compatibility one-or-three-token command color shape.  It accepts every
 * scalar bu_color_from_str spelling (including packed RGB, hexadecimal, and
 * normalized floating-point triples) or three separate RGB components.  This
 * is the native replacement for bu_opt_color's value grammar; use BU_CMD_RGB
 * instead when a command deliberately wants only strict 8-bit RGB input.
 */
BU_EXPORT extern const struct bu_cmd_arg_shape bu_cmd_color_arg_shape;
BU_EXPORT extern int bu_cmd_color_from_argv(struct bu_color *color, size_t argc,
	const char * const *argv);
BU_EXPORT extern int bu_cmd_color_consume(struct bu_vls *msg, size_t argc,
	const char **argv, void *storage);

/**
 * Standard one-or-three-token finite XYZ vector argument shape.  The matching
 * reader and consumer accept packed x/y/z, x,y,z, or x;y;z input, a quoted
 * x y z token, or three separate numeric arguments.  The reader returns the
 * number of tokens consumed (one or three), or zero on failure, and does not
 * modify xyz on failure.  The consumer stores the three components in the
 * selected point_t or vect_t field.
 */
BU_EXPORT extern const struct bu_cmd_arg_shape bu_cmd_vector3_arg_shape;
BU_EXPORT extern int bu_cmd_vector3_from_argv(fastf_t *xyz, size_t argc,
	const char * const *argv);
BU_EXPORT extern int bu_cmd_vector3_consume(struct bu_vls *msg, size_t argc,
	const char **argv, void *storage);

/**
 * Standard count-plus-vector argument shape.  It accepts an integer count
 * followed by a packed or three-token XYZ vector, consuming two or four
 * words.  The reader writes the count and vector independently, so commands
 * can bind the shared syntax to their own storage records.
 */
BU_EXPORT extern const struct bu_cmd_arg_shape bu_cmd_counted_vector3_arg_shape;
BU_EXPORT extern int bu_cmd_counted_vector3_from_argv(int *count, fastf_t *xyz,
	size_t argc, const char * const *argv);

/**
 * Strict scalar readers used by native schemas and by command execution when
 * a typed positional role must be stored after schema validation.  They
 * accept BRL-CAD boolean spellings, base-zero integers (or hexadecimal
 * integers where named), and finite floating-point values respectively,
 * return nonzero on success, and leave output storage unchanged on failure.
 */
BU_EXPORT extern int bu_cmd_integer_from_str(int *value, const char *arg);
BU_EXPORT extern int bu_cmd_bool_from_str(int *value, const char *arg);
BU_EXPORT extern int bu_cmd_hex_integer_from_str(unsigned int *value, const char *arg);
BU_EXPORT extern int bu_cmd_long_from_str(long *value, const char *arg);
BU_EXPORT extern int bu_cmd_hex_long_from_str(long *value, const char *arg);
BU_EXPORT extern int bu_cmd_number_from_str(fastf_t *value, const char *arg);
BU_EXPORT extern int bu_cmd_char_from_str(char *value, const char *arg);

/**
 * Read a BRL-CAD unit expression into its conversion factor to the base
 * unit.  This accepts the same vocabulary and quantity forms as bu_mm_value,
 * requires a positive finite result, and leaves value
 * unchanged on failure.  The validator is suitable for a string-valued
 * native-schema option.
 */
BU_EXPORT extern int bu_cmd_units_from_str(double *value, const char *arg);
BU_EXPORT extern int bu_cmd_units_validate(struct bu_vls *msg, const char *arg);

/** Validate a lower-case ISO 639-1 two-letter language code. */
BU_EXPORT extern int bu_cmd_iso639_1_validate(struct bu_vls *msg,
	const char *arg);

/** Validate one of BRL-CAD's supported manual-page section identifiers. */
BU_EXPORT extern int bu_cmd_man_section_validate(struct bu_vls *msg,
	const char *arg);

/**
 * Standard scalar range validators for native command schemas.  They accept
 * the same base-0 integer and finite floating-point syntax as the matching
 * BU_CMD_INTEGER and BU_CMD_NUMBER option types.  They are supplied as
 * reusable building blocks for the common command-line cases; commands with
 * a domain-specific range may still provide their own validator.
 */
BU_EXPORT extern int bu_cmd_positive_integer_validate(struct bu_vls *msg,
	const char *arg);
BU_EXPORT extern int bu_cmd_nonnegative_integer_validate(struct bu_vls *msg,
	const char *arg);
BU_EXPORT extern int bu_cmd_positive_number_validate(struct bu_vls *msg,
	const char *arg);
BU_EXPORT extern int bu_cmd_nonnegative_number_validate(struct bu_vls *msg,
	const char *arg);


/**
 * One canonical keyword value.  aliases are accepted spellings but are not
 * offered as default completion candidates.  The description is intended for
 * help/JSON and rich completion clients; it may be NULL.
 */
struct bu_cmd_value_keyword {
    const char *canonical;
    const char * const *aliases;
    const char *description;
};


/** A declarative numeric domain for an option or operand value. */
typedef enum {
    BU_CMD_RANGE_NONE = 0,
    BU_CMD_RANGE_INTEGER,
    BU_CMD_RANGE_NUMBER
} bu_cmd_range_kind_t;


/**
 * Numeric bounds are interpreted according to kind.  An unset bound is
 * ignored; set bounds may be inclusive or exclusive.  Keeping integer and
 * floating-point storage separate avoids losing integer precision.
 */
struct bu_cmd_value_range {
    bu_cmd_range_kind_t kind;
    int has_minimum;
    int has_maximum;
    int minimum_inclusive;
    int maximum_inclusive;
    long integer_minimum;
    long integer_maximum;
    fastf_t number_minimum;
    fastf_t number_maximum;
};


/**
 * One canonical command option.  storage_offset is relative to the data
 * object supplied to bu_cmd_schema_parse.  Alias records set alias_of to the
 * canonical option name and do not need their own storage binding.
 */
struct bu_cmd_option {
    const char *shortopt;
    const char *longopt;
    /* May be NULL for a short-only convenience-macro declaration; use
     * bu_cmd_option_canonical() when consuming this field. */
    const char *canonical;
    const char *argument;
    const char *help;
    bu_cmd_value_t value_type;
    size_t storage_offset;
    bu_cmd_value_parse_t custom_parse;
    bu_cmd_value_validate_t validate;
    const char *semantic_provider;
    const char *alias_of;
    /* A repeatable flag accumulates occurrences in its int storage field. */
    int repeat;
    int hidden;
    const char * const *value_keywords;
    bu_cmd_arg_requirement_t arg_requirement;
    const struct bu_cmd_arg_shape *arg_shape;
    bu_cmd_value_consume_t consume;
    const struct bu_cmd_value_keyword *keyword_values;
    struct bu_cmd_value_range range;
};

/** Return an option's stable name, preferring its long spelling. */
BU_EXPORT extern const char *bu_cmd_option_canonical(const struct bu_cmd_option *option);

/** True for every option entry other than the terminal NULL entry. */
BU_EXPORT extern int bu_cmd_option_is_valid(const struct bu_cmd_option *option);


struct bu_cmd_schema;
struct bu_cmd_validate_result;

/**
 * A schema-owned validator for syntax that cannot be expressed by declarative
 * constraint rows or ordinary operand cardinality.  It must be side-effect
 * free.  bu_cmd_schema_validate always applies the ordinary structural rules
 * and declarative constraints first, then invokes this callback with their
 * result.  The callback may refine that result without recursively invoking
 * bu_cmd_schema_validate.  Returning nonzero discards the complete result,
 * including candidates allocated by the callback.
 */
typedef int (*bu_cmd_schema_validate_t)(const struct bu_cmd_schema *schema,
	size_t argc, const char **argv, size_t cursor_arg,
	struct bu_cmd_validate_result *result);

/**
 * Context-aware companion to a native schema validator.  The ordinary
 * validator remains context-free so libbu can use it everywhere; hosts such
 * as GED may supply application state for semantic rules that depend on a
 * database, view, or other live command environment.  Ordinary and custom
 * context-free validation has already populated result when this callback is
 * invoked.  Returning nonzero discards the complete result.
 */
typedef int (*bu_cmd_schema_context_validate_t)(const struct bu_cmd_schema *schema,
	size_t argc, const char **argv, size_t cursor_arg, void *context,
	struct bu_cmd_validate_result *result);


/**
 * Positional role declaration used by schema consumers and documentation.
 * validate augments the scalar type with a side-effect-free constraint.  For
 * a keyword role that also publishes static candidates, a validator is
 * authoritative and may accept aliases or expressions that are intentionally
 * omitted from the canonical completion list.
 * semantic_provider is an optional opaque identifier for an application-level
 * validator/completer; libbu carries it but does not interpret it.
 */
struct bu_cmd_operand {
    const char *name;
    size_t min_count;
    size_t max_count;
    const char *help;
    bu_cmd_value_t value_type;
    bu_cmd_value_validate_t validate;
    const char *semantic_provider;
    const char * const *value_keywords;
    const struct bu_cmd_value_keyword *keyword_values;
    const struct bu_cmd_arg_shape *shape;
    struct bu_cmd_value_range range;
};


/**
 * A repeated heterogeneous positional group.  roles is a NULL-terminated
 * sequence of scalar operand declarations, each of which must occur exactly
 * once per group repetition.  min_count and max_count count complete group
 * repetitions, not individual tokens.  Groups follow the schema's ordinary
 * operands and are emitted explicitly in machine-readable descriptions.
 */
struct bu_cmd_operand_group {
    const char *name;
    const struct bu_cmd_operand *roles;
    size_t min_count;
    size_t max_count;
    const char *help;
};


/** A declarative condition for a compact command constraint. */
typedef enum {
    BU_CMD_CONDITION_ALWAYS = 0,
    BU_CMD_CONDITION_ANY_OPTION_PRESENT,
    BU_CMD_CONDITION_NO_OPTION_PRESENT,
    BU_CMD_CONDITION_ALL_OPTIONS_PRESENT
} bu_cmd_constraint_condition_t;


/**
 * One option-selected operand layout for a flat command.  All cases share the
 * enclosing schema's option table and parse policy; only the positional
 * structure changes.  options contains canonical option names and is
 * interpreted according to condition.  Cases are tested in declaration
 * order, making intentional precedence explicit.  A final ALWAYS case is the
 * ordinary default.
 */
struct bu_cmd_schema_case {
    const char *name;
    const char *help;
    bu_cmd_constraint_condition_t condition;
    const char * const *options;
    const struct bu_cmd_operand *operands;
    const struct bu_cmd_operand_group *operand_groups;
};


/** The syntax relationship enforced by a compact command constraint. */
typedef enum {
    BU_CMD_CONSTRAINT_OPTION_COUNT = 0,
    /** Count every occurrence, including repeated uses of one option. */
    BU_CMD_CONSTRAINT_OPTION_OCCURRENCE_COUNT,
    BU_CMD_CONSTRAINT_OPERAND_COUNT,
    /** The first listed option requires every following option. */
    BU_CMD_CONSTRAINT_OPTION_REQUIRES,
    /** The first listed option conflicts with every following option. */
    BU_CMD_CONSTRAINT_OPTION_CONFLICTS
} bu_cmd_constraint_kind_t;


/**
 * A compact relationship row.  options is a NULL-terminated list of canonical
 * option names (without dashes).  For REQUIRES and CONFLICTS, options[0] is
 * the trigger and every following entry is respectively required or
 * forbidden.  Option-count constraints count distinct selected canonical
 * options; occurrence-count constraints include repeated spellings.
 * Operand-count constraints apply only when condition matches the option set.
 */
struct bu_cmd_constraint {
    bu_cmd_constraint_kind_t kind;
    bu_cmd_constraint_condition_t condition;
    const char * const *options;
    size_t min_count;
    size_t max_count;
    const char *hint;
};


struct bu_cmd_schema_validation {
    bu_cmd_schema_validate_t custom_validate;
    const struct bu_cmd_constraint *constraints;
    bu_cmd_schema_context_validate_t context_validate;
    const struct bu_cmd_schema_case *cases;
    /**
     * Conventional terminal actions selected by BU_CMD_TERMINAL_* bits.
     * A present terminal option still participates in ordinary option parsing,
     * but bypasses positional syntax, constraints, cases, and custom/context
     * validators.  This models actions such as help and version that do not
     * execute the command's ordinary operation.
     */
    unsigned int terminal_flags;
    /** Additional canonical options with the same terminal behavior. */
    const char * const *terminal_options;
    /** Nonzero when execution is owned by an external parser rather than
     * bu_cmd_schema_parse.  Validation, help, and completion metadata remain
     * authoritative even though option storage offsets are intentionally
     * absent. */
    int external_execution;
};

#define BU_CMD_TERMINAL_HELP 0x1u
#define BU_CMD_TERMINAL_VERSION 0x2u
#define BU_CMD_TERMINAL_MASK (BU_CMD_TERMINAL_HELP | BU_CMD_TERMINAL_VERSION)

struct bu_cmd_schema {
    const char *name;
    const char *help;
    const struct bu_cmd_option *options;
    const struct bu_cmd_operand *operands;
    bu_cmd_parse_policy_t parse_policy;
    const struct bu_cmd_operand_group *operand_groups;
    struct bu_cmd_schema_validation validation;
};


/** Position at which a tree node selects one of its child commands. */
typedef enum {
    /* The child word follows the parent option phase. */
    BU_CMD_TREE_CHILD_AFTER_OPTIONS = 0,
    /* The child word is the first argument of the parent node. */
    BU_CMD_TREE_CHILD_FIRST,
    /* The child follows a non-interspersed, fixed scalar parent operand prefix. */
    BU_CMD_TREE_CHILD_AFTER_FIXED_OPERANDS
} bu_cmd_tree_child_phase_t;


/**
 * Optional execution callback for one native command-tree node.  The argv
 * array starts with the selected node's canonical schema name, even when the
 * caller selected the node through an alias.  The callback result is returned
 * through bu_cmd_tree_dispatch; a dispatch failure is reported separately.
 */
typedef int (*bu_cmd_tree_execute_t)(void *context, int argc, const char *argv[]);


/**
 * One native command-tree node.  The schema is the node's only syntax/help
 * source; its name is the canonical child spelling.  aliases, when needed,
 * add accepted child spellings without duplicating the schema.  A node may
 * own further children and an optional executor.
 */
struct bu_cmd_tree_node {
    const struct bu_cmd_schema *schema;
    const char * const *aliases;
    const struct bu_cmd_tree_node *subcommands;
    bu_cmd_tree_child_phase_t child_phase;
    bu_cmd_tree_execute_t execute;
};


/**
 * A compact native command tree.  The root and every child refer directly to
 * executable flat schemas, so parsing, help, JSON, completion, and execution
 * do not maintain parallel option descriptions.
 */
struct bu_cmd_tree {
    const struct bu_cmd_schema *root_schema;
    const struct bu_cmd_tree_node *subcommands;
    bu_cmd_tree_child_phase_t child_phase;
};


/**
 * One complete alternative command form.  Exactly one of schema or tree must
 * be set.  The optional help text explains when this form is appropriate;
 * syntax, options, operands, and subcommands remain owned by the referenced
 * schema/tree so every consumer sees the same declaration.
 */
struct bu_cmd_form {
    const char *name;
    const char *help;
    const struct bu_cmd_schema *schema;
    const struct bu_cmd_tree *tree;
};


struct bu_cmd_forms;

/**
 * Select a command form for one argv sequence.  argv includes the command
 * spelling at index zero, matching interactive command-analysis callers.
 * context is supplied by the host and may be NULL.  Returning NULL asks the
 * generic form matcher to select the best syntax match.
 */
typedef const struct bu_cmd_form *(*bu_cmd_form_select_t)(
	const struct bu_cmd_forms *forms, size_t argc,
	const char * const *argv, void *context);


/**
 * A first-class set of mutually exclusive command forms.  This declaration
 * is shared by validation, parsing/dispatch selection, help, JSON, lint, and
 * completion adapters instead of maintaining parallel form descriptions.
 */
struct bu_cmd_forms {
    const char *name;
    const char *help;
    const struct bu_cmd_form *forms;
    bu_cmd_form_select_t select;
};

struct bu_cmd_validate_result {
    bu_cmd_validate_state_t state;
    size_t token_start;
    size_t token_end;
    unsigned int expected;
    const char *hint;
    size_t completion_count;
    const char **completion_candidates;
    bu_cmd_value_t completion_type;
    const char *semantic_provider;
    /** Optional side-effect-free predicate for a completed value.  Completion
     * clients may use this to discard provider candidates without reparsing
     * and validating the whole command. */
    bu_cmd_value_validate_t candidate_validate;
};

/**
 * Populate a validation result with heap-owned completion candidates from a
 * NULL-terminated canonical keyword list.  Only candidates beginning with
 * prefix are returned.  This is for command-owned mini-languages whose next
 * token is selected from a fixed vocabulary; callers set the result's state,
 * role, and type first.  Any candidates already owned by result are replaced.
 */
BU_EXPORT extern void bu_cmd_keyword_candidates(struct bu_cmd_validate_result *result,
	const char * const *values, const char *prefix);


/**
 * Validate an optional strict 8-bit RGB positional value.  This is the
 * query-or-set form used by commands whose absent value queries the current
 * color: zero words, one packed r/g/b (r,g,b, or r;g;b) word, or three
 * separate 0..255 channel words are accepted.  The result describes the
 * whole RGB argument group, including a valid partial channel sequence as
 * incomplete rather than invalid.
 */
BU_EXPORT extern int bu_cmd_rgb_optional_validate(size_t argc, const char **argv,
	size_t cursor_arg, struct bu_cmd_validate_result *result);

/**
 * Validate an optional compatibility color positional value.  This is the
 * query-or-set counterpart to BU_CMD_COLOR_COMPAT: zero words, one scalar
 * bu_color_from_str spelling, or three separate RGB components are accepted.
 * A valid partial three-word component sequence is incomplete so editors can
 * continue to highlight and complete it while it is being entered.
 */
BU_EXPORT extern int bu_cmd_color_optional_validate(size_t argc, const char **argv,
	size_t cursor_arg, struct bu_cmd_validate_result *result);

/**
 * Validate an optional finite XYZ positional value.  This query-or-set form
 * accepts zero words, one packed x/y/z (x,y,z, x;y;z, or quoted x y z) word,
 * or three separate finite-number words.  A valid partial three-word form is
 * reported as incomplete for incremental editors.
 */
BU_EXPORT extern int bu_cmd_vector3_optional_validate(size_t argc, const char **argv,
	size_t cursor_arg, struct bu_cmd_validate_result *result);

/**
 * Validate a required finite XYZ positional value.  This is the set-only
 * counterpart to bu_cmd_vector3_optional_validate: one packed x/y/z word or
 * three separate finite-number words are accepted, while zero words and a
 * valid partial component sequence are incomplete.
 */
BU_EXPORT extern int bu_cmd_vector3_required_validate(size_t argc, const char **argv,
	size_t cursor_arg, struct bu_cmd_validate_result *result);

/**
 * Validate an optional pair of base-zero integer positional values.  The
 * query-or-set form accepts zero or exactly two words; one valid component is
 * incomplete so an editor can keep it editable while the second is entered.
 */
BU_EXPORT extern int bu_cmd_integer_pair_from_argv(int pair[2], size_t argc,
	const char * const *argv);
BU_EXPORT extern int bu_cmd_integer_pair_optional_validate(size_t argc,
	const char **argv, size_t cursor_arg, struct bu_cmd_validate_result *result);


/** Initializers for no range and inclusive one- or two-sided ranges. */
#define BU_CMD_VALUE_RANGE_NONE {BU_CMD_RANGE_NONE, 0, 0, 1, 1, 0, 0, 0.0, 0.0}
#define BU_CMD_INTEGER_RANGE_INIT(_min, _max) {BU_CMD_RANGE_INTEGER, 1, 1, 1, 1, _min, _max, 0.0, 0.0}
#define BU_CMD_INTEGER_MIN_INIT(_min) {BU_CMD_RANGE_INTEGER, 1, 0, 1, 1, _min, 0, 0.0, 0.0}
#define BU_CMD_INTEGER_MAX_INIT(_max) {BU_CMD_RANGE_INTEGER, 0, 1, 1, 1, 0, _max, 0.0, 0.0}
#define BU_CMD_NUMBER_RANGE_INIT(_min, _max) {BU_CMD_RANGE_NUMBER, 1, 1, 1, 1, 0, 0, _min, _max}
#define BU_CMD_NUMBER_MIN_INIT(_min) {BU_CMD_RANGE_NUMBER, 1, 0, 1, 1, 0, 0, _min, 0.0}
#define BU_CMD_NUMBER_MAX_INIT(_max) {BU_CMD_RANGE_NUMBER, 0, 1, 1, 1, 0, 0, 0.0, _max}
#define BU_CMD_OPTION_NULL {NULL, NULL, NULL, NULL, NULL, BU_CMD_VALUE_FLAG, 0, NULL, NULL, NULL, NULL, 0, 0, NULL, BU_CMD_ARG_NONE, NULL, NULL, NULL, BU_CMD_VALUE_RANGE_NONE}
#define BU_CMD_OPERAND(_name, _type, _min, _max, _help, _provider) {_name, _min, _max, _help, _type, NULL, _provider, NULL, NULL, NULL, BU_CMD_VALUE_RANGE_NONE}
#define BU_CMD_OPERAND_KEYWORDS(_name, _type, _min, _max, _help, _provider, _keywords) {_name, _min, _max, _help, _type, NULL, _provider, _keywords, NULL, NULL, BU_CMD_VALUE_RANGE_NONE}
/** Publish concise canonical candidates while a validator recognizes the full
 * accepted operand vocabulary. */
#define BU_CMD_OPERAND_KEYWORDS_VALIDATE(_name, _type, _min, _max, _validator, _help, _provider, _keywords) \
    {_name, _min, _max, _help, _type, _validator, _provider, _keywords, NULL, \
	NULL, BU_CMD_VALUE_RANGE_NONE}
#define BU_CMD_OPERAND_KEYWORD_VALUES(_name, _type, _min, _max, _help, _provider, _keywords) {_name, _min, _max, _help, _type, NULL, _provider, NULL, _keywords, NULL, BU_CMD_VALUE_RANGE_NONE}
#define BU_CMD_OPERAND_VALIDATE(_name, _type, _min, _max, _validator, _help, _provider) {_name, _min, _max, _help, _type, _validator, _provider, NULL, NULL, NULL, BU_CMD_VALUE_RANGE_NONE}
#define BU_CMD_OPERAND_SHAPED(_name, _type, _min, _max, _validator, _help, _provider, _shape) {_name, _min, _max, _help, _type, _validator, _provider, NULL, NULL, _shape, BU_CMD_VALUE_RANGE_NONE}
#define BU_CMD_OPERAND_INTEGER_RANGE(_name, _min_count, _max_count, _minimum, _maximum, _help, _provider) {_name, _min_count, _max_count, _help, BU_CMD_VALUE_INTEGER, NULL, _provider, NULL, NULL, NULL, BU_CMD_INTEGER_RANGE_INIT(_minimum, _maximum)}
#define BU_CMD_OPERAND_INTEGER_MIN(_name, _min_count, _max_count, _minimum, _help, _provider) {_name, _min_count, _max_count, _help, BU_CMD_VALUE_INTEGER, NULL, _provider, NULL, NULL, NULL, BU_CMD_INTEGER_MIN_INIT(_minimum)}
#define BU_CMD_OPERAND_INTEGER_MAX(_name, _min_count, _max_count, _maximum, _help, _provider) {_name, _min_count, _max_count, _help, BU_CMD_VALUE_INTEGER, NULL, _provider, NULL, NULL, NULL, BU_CMD_INTEGER_MAX_INIT(_maximum)}
#define BU_CMD_OPERAND_NUMBER_RANGE(_name, _min_count, _max_count, _minimum, _maximum, _help, _provider) {_name, _min_count, _max_count, _help, BU_CMD_VALUE_NUMBER, NULL, _provider, NULL, NULL, NULL, BU_CMD_NUMBER_RANGE_INIT(_minimum, _maximum)}
#define BU_CMD_OPERAND_NUMBER_MIN(_name, _min_count, _max_count, _minimum, _help, _provider) {_name, _min_count, _max_count, _help, BU_CMD_VALUE_NUMBER, NULL, _provider, NULL, NULL, NULL, BU_CMD_NUMBER_MIN_INIT(_minimum)}
#define BU_CMD_OPERAND_NUMBER_MAX(_name, _min_count, _max_count, _maximum, _help, _provider) {_name, _min_count, _max_count, _help, BU_CMD_VALUE_NUMBER, NULL, _provider, NULL, NULL, NULL, BU_CMD_NUMBER_MAX_INIT(_maximum)}
#define BU_CMD_OPERAND_NULL {NULL, 0, 0, NULL, BU_CMD_VALUE_STRING, NULL, NULL, NULL, NULL, NULL, BU_CMD_VALUE_RANGE_NONE}
#define BU_CMD_OPERAND_GROUP(_name, _roles, _min, _max, _help) {_name, _roles, _min, _max, _help}
#define BU_CMD_OPERAND_GROUP_NULL {NULL, NULL, 0, 0, NULL}
#define BU_CMD_SCHEMA_CASE(_name, _help, _condition, _options, _operands, _groups) \
    {_name, _help, _condition, _options, _operands, _groups}
#define BU_CMD_SCHEMA_CASE_DEFAULT(_name, _help, _operands, _groups) \
    BU_CMD_SCHEMA_CASE(_name, _help, BU_CMD_CONDITION_ALWAYS, NULL, _operands, _groups)
#define BU_CMD_SCHEMA_CASE_NULL {NULL, NULL, BU_CMD_CONDITION_ALWAYS, NULL, NULL, NULL}
#define BU_CMD_CONSTRAINT_OPTIONS_IF(_condition, _options, _min, _max, _hint) {BU_CMD_CONSTRAINT_OPTION_COUNT, _condition, _options, _min, _max, _hint}
#define BU_CMD_CONSTRAINT_OPTIONS(_options, _min, _max, _hint) BU_CMD_CONSTRAINT_OPTIONS_IF(BU_CMD_CONDITION_ALWAYS, _options, _min, _max, _hint)
#define BU_CMD_CONSTRAINT_OPTION_OCCURRENCES(_options, _min, _max, _hint) \
    {BU_CMD_CONSTRAINT_OPTION_OCCURRENCE_COUNT, BU_CMD_CONDITION_ALWAYS, \
	_options, _min, _max, _hint}
#define BU_CMD_CONSTRAINT_OPERANDS(_condition, _options, _min, _max, _hint) {BU_CMD_CONSTRAINT_OPERAND_COUNT, _condition, _options, _min, _max, _hint}
#define BU_CMD_CONSTRAINT_REQUIRES(_options, _hint) \
    {BU_CMD_CONSTRAINT_OPTION_REQUIRES, BU_CMD_CONDITION_ALWAYS, _options, 0, 0, _hint}
#define BU_CMD_CONSTRAINT_CONFLICTS(_options, _hint) \
    {BU_CMD_CONSTRAINT_OPTION_CONFLICTS, BU_CMD_CONDITION_ALWAYS, _options, 0, 0, _hint}
#define BU_CMD_CONSTRAINT_NULL {BU_CMD_CONSTRAINT_OPTION_COUNT, BU_CMD_CONDITION_ALWAYS, NULL, 0, 0, NULL}
/**
 * Compatibility initializers for a schema's operand-group and validation tail.
 * Prefer BU_CMD_SCHEMA with BU_CMD_SCHEMA_META for new native schemas: it
 * makes all independent metadata dimensions visible in one declaration.
 */
#define BU_CMD_SCHEMA_CONSTRAINTS(_validator, _constraints) NULL, {_validator, _constraints, NULL, NULL, 0, NULL, 0}
#define BU_CMD_SCHEMA_CONTEXT_VALIDATOR(_validator) NULL, {NULL, NULL, _validator, NULL, 0, NULL, 0}
#define BU_CMD_SCHEMA_GROUPS(_validator, _constraints, _groups) _groups, {_validator, _constraints, NULL, NULL, 0, NULL, 0}
#define BU_CMD_SCHEMA_META(_groups, _constraints, _validator, _context) \
    _groups, {_validator, _constraints, _context, NULL, 0, NULL, 0}
#define BU_CMD_SCHEMA_META_CASES(_groups, _constraints, _validator, _context, _cases) \
    _groups, {_validator, _constraints, _context, _cases, 0, NULL, 0}
#define BU_CMD_SCHEMA_META_TERMINALS(_groups, _constraints, _validator, _context, _cases, _terminals) \
    _groups, {_validator, _constraints, _context, _cases, 0, _terminals, 0}
#define BU_CMD_SCHEMA_META_HELP(_groups, _constraints, _validator, _context, _cases) \
    _groups, {_validator, _constraints, _context, _cases, BU_CMD_TERMINAL_HELP, NULL, 0}
#define BU_CMD_SCHEMA_META_HELP_VERSION(_groups, _constraints, _validator, _context, _cases) \
    _groups, {_validator, _constraints, _context, _cases, \
	BU_CMD_TERMINAL_HELP | BU_CMD_TERMINAL_VERSION, NULL, 0}
#define BU_CMD_SCHEMA_META_EXTERNAL(_groups, _constraints, _validator) \
    _groups, {_validator, _constraints, NULL, NULL, 0, NULL, 1}
/**
 * Declare a complete native flat schema.  Metadata is deliberately a single
 * argument so an author can see groups, declarative constraints, ordinary
 * validation, context validation, and execution ownership together.
 */
#define BU_CMD_SCHEMA(_name, _help, _options, _operands, _policy, _metadata) \
    {_name, _help, _options, _operands, _policy, _metadata}
#define BU_CMD_SCHEMA_BOUND(_name, _help, _options, _operands, _policy, _groups, _constraints, _validator, _context) \
    BU_CMD_SCHEMA(_name, _help, _options, _operands, _policy, \
        BU_CMD_SCHEMA_META(_groups, _constraints, _validator, _context))
/** Common case-only schema.  Combine BU_CMD_SCHEMA with
 * BU_CMD_SCHEMA_META_CASES directly when constraints or callbacks are also
 * needed. */
#define BU_CMD_SCHEMA_CASED(_name, _help, _options, _policy, _cases) \
    BU_CMD_SCHEMA(_name, _help, _options, NULL, _policy, \
	BU_CMD_SCHEMA_META_CASES(NULL, NULL, NULL, NULL, _cases))
#define BU_CMD_SCHEMA_EXTERNAL(_name, _help, _options, _operands, _policy, _groups, _constraints, _validator) \
    BU_CMD_SCHEMA(_name, _help, _options, _operands, _policy, \
	BU_CMD_SCHEMA_META_EXTERNAL(_groups, _constraints, _validator))
#define BU_CMD_COUNT_UNLIMITED ((size_t)-1)
#define BU_CMD_STORAGE_NONE ((size_t)-1)
#if defined(__cplusplus) && (defined(__clang__) || defined(__GNUC__) || defined(_MSC_VER))
/** Optional declaration-site assertion.  Bound option macros enforce this
 * automatically; this form is useful when documenting a shared record. */
#  define BU_CMD_RECORD_ASSERT(_record) \
    static_assert(__is_standard_layout(_record), \
	"bu_cmd_schema offset bindings require a standard-layout record; " \
	"use an unbound option in a BU_CMD_SCHEMA_EXTERNAL schema otherwise")
#else
#  define BU_CMD_RECORD_ASSERT(_record)
#endif
#define BU_CMD_ARG_VARIANT(_name, _syntax, _count, _help) {_name, _syntax, _count, _help}
#define BU_CMD_ARG_VARIANT_NULL {NULL, NULL, 0, NULL}
#define BU_CMD_ARG_SHAPE(_kind, _min, _max, _description) {_kind, _min, _max, _description, NULL, NULL}
#define BU_CMD_ARG_SHAPE_FORMS(_kind, _min, _max, _description, _count, _variants) \
    {_kind, _min, _max, _description, _count, _variants}
#define BU_CMD_VALIDATE_RESULT_NULL {BU_CMD_VALIDATE_UNKNOWN, 0, 0, BU_CMD_EXPECT_NONE, NULL, 0, NULL, BU_CMD_VALUE_UNKNOWN, NULL, NULL}
#define BU_CMD_TREE_NODE(_schema, _aliases, _children, _phase, _execute) {_schema, _aliases, _children, _phase, _execute}
#define BU_CMD_TREE_NODE_NULL {NULL, NULL, NULL, BU_CMD_TREE_CHILD_AFTER_OPTIONS, NULL}
#define BU_CMD_TREE(_root, _children, _phase) {_root, _children, _phase}
#define BU_CMD_FORM_SCHEMA(_name, _help, _schema) {_name, _help, _schema, NULL}
#define BU_CMD_FORM_TREE(_name, _help, _tree) {_name, _help, NULL, _tree}
#define BU_CMD_FORM_NULL {NULL, NULL, NULL, NULL}
#define BU_CMD_FORMS(_name, _help, _forms, _select) {_name, _help, _forms, _select}
#define BU_CMD_ALIASES(...) ((const char * const[]){__VA_ARGS__, NULL})


/** Compact option declaration helpers. */
#define BU_CMD_FLAG(_short, _long, _record, _field, _help) \
    {_short, _long, _long, NULL, _help, BU_CMD_VALUE_FLAG, BU_CMD_STORAGE_OFFSET(_record, _field), NULL, NULL, NULL, NULL, 0, 0, NULL, BU_CMD_ARG_NONE, NULL, NULL, NULL, BU_CMD_VALUE_RANGE_NONE}
/** A no-argument flag whose int storage field counts occurrences. */
#define BU_CMD_COUNTING_FLAG(_short, _long, _record, _field, _help) \
    {_short, _long, _long, NULL, _help, BU_CMD_VALUE_FLAG, BU_CMD_STORAGE_OFFSET(_record, _field), NULL, NULL, NULL, NULL, 1, 0, NULL, BU_CMD_ARG_NONE, NULL, NULL, NULL, BU_CMD_VALUE_RANGE_NONE}
/** A no-argument flag whose long storage field counts occurrences. */
#define BU_CMD_COUNTING_LONG_FLAG(_short, _long, _record, _field, _help) \
    {_short, _long, _long, NULL, _help, BU_CMD_VALUE_LONG, BU_CMD_STORAGE_OFFSET(_record, _field), NULL, NULL, NULL, NULL, 1, 0, NULL, BU_CMD_ARG_NONE, NULL, NULL, NULL, BU_CMD_VALUE_RANGE_NONE}
/**
 * A no-argument option that invokes a command-specific state transition.
 * The parser receives a NULL argument and the address of the selected field.
 */
#define BU_CMD_CUSTOM_FLAG(_short, _long, _canonical, _record, _field, _parser, _help) \
    {_short, _long, _canonical, NULL, _help, BU_CMD_VALUE_CUSTOM, BU_CMD_STORAGE_OFFSET(_record, _field), _parser, NULL, NULL, NULL, 0, 0, NULL, BU_CMD_ARG_NONE, NULL, NULL, NULL, BU_CMD_VALUE_RANGE_NONE}
/** A syntax-only flag for commands whose execution parser owns its state. */
#define BU_CMD_FLAG_UNBOUND(_short, _long, _canonical, _help) \
    {_short, _long, _canonical, NULL, _help, BU_CMD_VALUE_FLAG, BU_CMD_STORAGE_NONE, NULL, NULL, NULL, NULL, 0, 0, NULL, BU_CMD_ARG_NONE, NULL, NULL, NULL, BU_CMD_VALUE_RANGE_NONE}
/**
 * A syntax-only typed option for a command whose execution parser owns its
 * state.  It participates in native help, validation, and completion but
 * bu_cmd_schema_parse deliberately rejects it because it has no storage
 * binding.  Use this only while a command's independently owned executor is
 * being migrated; new parsers should bind an argument record directly.
 */
#define BU_CMD_VALUE_UNBOUND(_short, _long, _canonical, _type, _arg, _help) \
    {_short, _long, _canonical, _arg, _help, _type, BU_CMD_STORAGE_NONE, NULL, NULL, NULL, NULL, 0, 0, NULL, BU_CMD_ARG_REQUIRED, NULL, NULL, NULL, BU_CMD_VALUE_RANGE_NONE}
/** A syntax-only variable-width option for an independently owned parser. */
#define BU_CMD_SHAPED_UNBOUND(_short, _long, _canonical, _type, _arg, _help, _shape) \
    {_short, _long, _canonical, _arg, _help, _type, BU_CMD_STORAGE_NONE, NULL, NULL, NULL, NULL, 0, 0, NULL, BU_CMD_ARG_REQUIRED, _shape, NULL, NULL, BU_CMD_VALUE_RANGE_NONE}
#define BU_CMD_BOOL(_short, _long, _record, _field, _arg, _help) \
    {_short, _long, _long, _arg, _help, BU_CMD_VALUE_BOOL, BU_CMD_STORAGE_OFFSET(_record, _field), NULL, NULL, NULL, NULL, 0, 0, NULL, BU_CMD_ARG_REQUIRED, NULL, NULL, NULL, BU_CMD_VALUE_RANGE_NONE}
#define BU_CMD_INTEGER(_short, _long, _record, _field, _arg, _help) \
    {_short, _long, _long, _arg, _help, BU_CMD_VALUE_INTEGER, BU_CMD_STORAGE_OFFSET(_record, _field), NULL, NULL, NULL, NULL, 0, 0, NULL, BU_CMD_ARG_REQUIRED, NULL, NULL, NULL, BU_CMD_VALUE_RANGE_NONE}
/** An integer option constrained to the inclusive [_minimum, _maximum] range. */
#define BU_CMD_INTEGER_RANGE(_short, _long, _record, _field, _minimum, _maximum, _arg, _help) \
    {_short, _long, _long, _arg, _help, BU_CMD_VALUE_INTEGER, BU_CMD_STORAGE_OFFSET(_record, _field), NULL, NULL, NULL, NULL, 0, 0, NULL, BU_CMD_ARG_REQUIRED, NULL, NULL, NULL, BU_CMD_INTEGER_RANGE_INIT(_minimum, _maximum)}
#define BU_CMD_INTEGER_MIN(_short, _long, _record, _field, _minimum, _arg, _help) \
    {_short, _long, _long, _arg, _help, BU_CMD_VALUE_INTEGER, BU_CMD_STORAGE_OFFSET(_record, _field), NULL, NULL, NULL, NULL, 0, 0, NULL, BU_CMD_ARG_REQUIRED, NULL, NULL, NULL, BU_CMD_INTEGER_MIN_INIT(_minimum)}
#define BU_CMD_INTEGER_MAX(_short, _long, _record, _field, _maximum, _arg, _help) \
    {_short, _long, _long, _arg, _help, BU_CMD_VALUE_INTEGER, BU_CMD_STORAGE_OFFSET(_record, _field), NULL, NULL, NULL, NULL, 0, 0, NULL, BU_CMD_ARG_REQUIRED, NULL, NULL, NULL, BU_CMD_INTEGER_MAX_INIT(_maximum)}
#define BU_CMD_HEX_INTEGER(_short, _long, _record, _field, _arg, _help) \
    {_short, _long, _long, _arg, _help, BU_CMD_VALUE_HEX_INTEGER, BU_CMD_STORAGE_OFFSET(_record, _field), NULL, NULL, NULL, NULL, 0, 0, NULL, BU_CMD_ARG_REQUIRED, NULL, NULL, NULL, BU_CMD_VALUE_RANGE_NONE}
#define BU_CMD_LONG(_short, _long, _record, _field, _arg, _help) \
    {_short, _long, _long, _arg, _help, BU_CMD_VALUE_LONG, BU_CMD_STORAGE_OFFSET(_record, _field), NULL, NULL, NULL, NULL, 0, 0, NULL, BU_CMD_ARG_REQUIRED, NULL, NULL, NULL, BU_CMD_VALUE_RANGE_NONE}
#define BU_CMD_HEX_LONG(_short, _long, _record, _field, _arg, _help) \
    {_short, _long, _long, _arg, _help, BU_CMD_VALUE_HEX_LONG, BU_CMD_STORAGE_OFFSET(_record, _field), NULL, NULL, NULL, NULL, 0, 0, NULL, BU_CMD_ARG_REQUIRED, NULL, NULL, NULL, BU_CMD_VALUE_RANGE_NONE}
#define BU_CMD_CHAR(_short, _long, _record, _field, _arg, _help) \
    {_short, _long, _long, _arg, _help, BU_CMD_VALUE_CHAR, BU_CMD_STORAGE_OFFSET(_record, _field), NULL, NULL, NULL, NULL, 0, 0, NULL, BU_CMD_ARG_REQUIRED, NULL, NULL, NULL, BU_CMD_VALUE_RANGE_NONE}
#define BU_CMD_OPTIONAL_INTEGER(_short, _long, _record, _field, _arg, _help) \
    {_short, _long, _long, _arg, _help, BU_CMD_VALUE_INTEGER, BU_CMD_STORAGE_OFFSET(_record, _field), NULL, NULL, NULL, NULL, 0, 0, NULL, BU_CMD_ARG_OPTIONAL, NULL, NULL, NULL, BU_CMD_VALUE_RANGE_NONE}
#define BU_CMD_NUMBER(_short, _long, _record, _field, _arg, _help) \
    {_short, _long, _long, _arg, _help, BU_CMD_VALUE_NUMBER, BU_CMD_STORAGE_OFFSET(_record, _field), NULL, NULL, NULL, NULL, 0, 0, NULL, BU_CMD_ARG_REQUIRED, NULL, NULL, NULL, BU_CMD_VALUE_RANGE_NONE}
/** A number option constrained to the inclusive [_minimum, _maximum] range. */
#define BU_CMD_NUMBER_RANGE(_short, _long, _record, _field, _minimum, _maximum, _arg, _help) \
    {_short, _long, _long, _arg, _help, BU_CMD_VALUE_NUMBER, BU_CMD_STORAGE_OFFSET(_record, _field), NULL, NULL, NULL, NULL, 0, 0, NULL, BU_CMD_ARG_REQUIRED, NULL, NULL, NULL, BU_CMD_NUMBER_RANGE_INIT(_minimum, _maximum)}
#define BU_CMD_NUMBER_MIN(_short, _long, _record, _field, _minimum, _arg, _help) \
    {_short, _long, _long, _arg, _help, BU_CMD_VALUE_NUMBER, BU_CMD_STORAGE_OFFSET(_record, _field), NULL, NULL, NULL, NULL, 0, 0, NULL, BU_CMD_ARG_REQUIRED, NULL, NULL, NULL, BU_CMD_NUMBER_MIN_INIT(_minimum)}
#define BU_CMD_NUMBER_MAX(_short, _long, _record, _field, _maximum, _arg, _help) \
    {_short, _long, _long, _arg, _help, BU_CMD_VALUE_NUMBER, BU_CMD_STORAGE_OFFSET(_record, _field), NULL, NULL, NULL, NULL, 0, 0, NULL, BU_CMD_ARG_REQUIRED, NULL, NULL, NULL, BU_CMD_NUMBER_MAX_INIT(_maximum)}
#define BU_CMD_COLOR(_short, _long, _record, _field, _arg, _help) \
    {_short, _long, _long, _arg, _help, BU_CMD_VALUE_COLOR, BU_CMD_STORAGE_OFFSET(_record, _field), NULL, NULL, NULL, NULL, 0, 0, NULL, BU_CMD_ARG_REQUIRED, NULL, NULL, NULL, BU_CMD_VALUE_RANGE_NONE}
/**
 * A one-or-three-token color option with bu_opt_color-compatible value
 * grammar.  It accepts any scalar bu_color_from_str spelling or three
 * separate RGB components.  Use BU_CMD_RGB for strict 8-bit RGB only.
 */
#define BU_CMD_COLOR_COMPAT(_short, _long, _record, _field, _arg, _help) \
    {_short, _long, _long, _arg, _help, BU_CMD_VALUE_COLOR, BU_CMD_STORAGE_OFFSET(_record, _field), NULL, NULL, NULL, NULL, 0, 0, NULL, BU_CMD_ARG_REQUIRED, NULL, bu_cmd_color_consume, NULL, BU_CMD_VALUE_RANGE_NONE}
/**
 * A standard RGB option bound to a struct bu_color field.  It accepts one
 * packed r/g/b, r,g,b, or r;g;b token, or three separate 0..255 channels.
 */
#define BU_CMD_RGB(_short, _long, _record, _field, _arg, _help) \
    {_short, _long, _long, _arg, _help, BU_CMD_VALUE_COLOR, BU_CMD_STORAGE_OFFSET(_record, _field), NULL, NULL, NULL, NULL, 0, 0, NULL, BU_CMD_ARG_REQUIRED, NULL, bu_cmd_rgb_consume, NULL, BU_CMD_VALUE_RANGE_NONE}
/**
 * A standard finite XYZ vector option bound to a point_t or vect_t field.
 * It accepts packed x/y/z, x,y,z, or x;y;z input, a quoted x y z token, or
 * three separate numeric arguments.
 */
#define BU_CMD_VECTOR3(_short, _long, _record, _field, _arg, _help) \
    {_short, _long, _long, _arg, _help, BU_CMD_VALUE_VECTOR, BU_CMD_STORAGE_OFFSET(_record, _field), NULL, NULL, NULL, NULL, 0, 0, NULL, BU_CMD_ARG_REQUIRED, NULL, bu_cmd_vector3_consume, NULL, BU_CMD_VALUE_RANGE_NONE}
#define BU_CMD_STRING(_short, _long, _record, _field, _arg, _help) \
    {_short, _long, _long, _arg, _help, BU_CMD_VALUE_STRING, BU_CMD_STORAGE_OFFSET(_record, _field), NULL, NULL, NULL, NULL, 0, 0, NULL, BU_CMD_ARG_REQUIRED, NULL, NULL, NULL, BU_CMD_VALUE_RANGE_NONE}
#define BU_CMD_OPTIONAL_STRING(_short, _long, _record, _field, _arg, _help) \
    {_short, _long, _long, _arg, _help, BU_CMD_VALUE_STRING, BU_CMD_STORAGE_OFFSET(_record, _field), NULL, NULL, NULL, NULL, 0, 0, NULL, BU_CMD_ARG_OPTIONAL, NULL, NULL, NULL, BU_CMD_VALUE_RANGE_NONE}
#define BU_CMD_VLS_APPEND(_short, _long, _record, _field, _arg, _help) \
    {_short, _long, _long, _arg, _help, BU_CMD_VALUE_VLS, BU_CMD_STORAGE_OFFSET(_record, _field), NULL, NULL, NULL, NULL, 0, 0, NULL, BU_CMD_ARG_REQUIRED, NULL, NULL, NULL, BU_CMD_VALUE_RANGE_NONE}
#define BU_CMD_FILE(_short, _long, _record, _field, _arg, _help) \
    {_short, _long, _long, _arg, _help, BU_CMD_VALUE_FILE, BU_CMD_STORAGE_OFFSET(_record, _field), NULL, NULL, NULL, NULL, 0, 0, NULL, BU_CMD_ARG_REQUIRED, NULL, NULL, NULL, BU_CMD_VALUE_RANGE_NONE}
#define BU_CMD_DB_OBJECT(_short, _long, _record, _field, _arg, _help) \
    {_short, _long, _long, _arg, _help, BU_CMD_VALUE_DB_OBJECT, BU_CMD_STORAGE_OFFSET(_record, _field), NULL, NULL, NULL, NULL, 0, 0, NULL, BU_CMD_ARG_REQUIRED, NULL, NULL, NULL, BU_CMD_VALUE_RANGE_NONE}
#define BU_CMD_DB_PATH(_short, _long, _record, _field, _arg, _help) \
    {_short, _long, _long, _arg, _help, BU_CMD_VALUE_DB_PATH, BU_CMD_STORAGE_OFFSET(_record, _field), NULL, NULL, NULL, NULL, 0, 0, NULL, BU_CMD_ARG_REQUIRED, NULL, NULL, NULL, BU_CMD_VALUE_RANGE_NONE}
#define BU_CMD_CUSTOM(_short, _long, _record, _field, _parser, _arg, _help) \
    {_short, _long, _long, _arg, _help, BU_CMD_VALUE_CUSTOM, BU_CMD_STORAGE_OFFSET(_record, _field), _parser, NULL, NULL, NULL, 0, 0, NULL, BU_CMD_ARG_REQUIRED, NULL, NULL, NULL, BU_CMD_VALUE_RANGE_NONE}
#define BU_CMD_INTEGER_VALIDATE(_short, _long, _record, _field, _validator, _arg, _help) \
    {_short, _long, _long, _arg, _help, BU_CMD_VALUE_INTEGER, BU_CMD_STORAGE_OFFSET(_record, _field), NULL, _validator, NULL, NULL, 0, 0, NULL, BU_CMD_ARG_REQUIRED, NULL, NULL, NULL, BU_CMD_VALUE_RANGE_NONE}
#define BU_CMD_NUMBER_VALIDATE(_short, _long, _record, _field, _validator, _arg, _help) \
    {_short, _long, _long, _arg, _help, BU_CMD_VALUE_NUMBER, BU_CMD_STORAGE_OFFSET(_record, _field), NULL, _validator, NULL, NULL, 0, 0, NULL, BU_CMD_ARG_REQUIRED, NULL, NULL, NULL, BU_CMD_VALUE_RANGE_NONE}
#define BU_CMD_POSITIVE_INTEGER(_short, _long, _record, _field, _arg, _help) \
    BU_CMD_INTEGER_VALIDATE(_short, _long, _record, _field, bu_cmd_positive_integer_validate, _arg, _help)
#define BU_CMD_NONNEGATIVE_INTEGER(_short, _long, _record, _field, _arg, _help) \
    BU_CMD_INTEGER_VALIDATE(_short, _long, _record, _field, bu_cmd_nonnegative_integer_validate, _arg, _help)
#define BU_CMD_POSITIVE_NUMBER(_short, _long, _record, _field, _arg, _help) \
    BU_CMD_NUMBER_VALIDATE(_short, _long, _record, _field, bu_cmd_positive_number_validate, _arg, _help)
#define BU_CMD_NONNEGATIVE_NUMBER(_short, _long, _record, _field, _arg, _help) \
    BU_CMD_NUMBER_VALIDATE(_short, _long, _record, _field, bu_cmd_nonnegative_number_validate, _arg, _help)
#define BU_CMD_UNITS(_short, _long, _record, _field, _arg, _help) \
    BU_CMD_STRING_VALIDATE(_short, _long, _record, _field, bu_cmd_units_validate, _arg, _help)
#define BU_CMD_STRING_VALIDATE(_short, _long, _record, _field, _validator, _arg, _help) \
    {_short, _long, _long, _arg, _help, BU_CMD_VALUE_STRING, BU_CMD_STORAGE_OFFSET(_record, _field), NULL, _validator, NULL, NULL, 0, 0, NULL, BU_CMD_ARG_REQUIRED, NULL, NULL, NULL, BU_CMD_VALUE_RANGE_NONE}
#define BU_CMD_CHAR_VALIDATE(_short, _long, _record, _field, _validator, _arg, _help) \
    {_short, _long, _long, _arg, _help, BU_CMD_VALUE_CHAR, BU_CMD_STORAGE_OFFSET(_record, _field), NULL, _validator, NULL, NULL, 0, 0, NULL, BU_CMD_ARG_REQUIRED, NULL, NULL, NULL, BU_CMD_VALUE_RANGE_NONE}
#define BU_CMD_VLS_APPEND_VALIDATE(_short, _long, _record, _field, _validator, _arg, _help) \
    {_short, _long, _long, _arg, _help, BU_CMD_VALUE_VLS, BU_CMD_STORAGE_OFFSET(_record, _field), NULL, _validator, NULL, NULL, 0, 0, NULL, BU_CMD_ARG_REQUIRED, NULL, NULL, NULL, BU_CMD_VALUE_RANGE_NONE}
/** Append one validated ISO 639-1 language code to a bu_vls field. */
#define BU_CMD_ISO639_1(_short, _long, _record, _field, _arg, _help) \
    BU_CMD_VLS_APPEND_VALIDATE(_short, _long, _record, _field, bu_cmd_iso639_1_validate, _arg, _help)
/** Bind a validated BRL-CAD manual-page section identifier to a char field. */
#define BU_CMD_MAN_SECTION(_short, _long, _record, _field, _arg, _help) \
    BU_CMD_CHAR_VALIDATE(_short, _long, _record, _field, bu_cmd_man_section_validate, _arg, _help)
#define BU_CMD_KEYWORD_VALUES(_short, _long, _record, _field, _arg, _help, _values) \
    {_short, _long, _long, _arg, _help, BU_CMD_VALUE_KEYWORD, BU_CMD_STORAGE_OFFSET(_record, _field), NULL, NULL, NULL, NULL, 0, 0, NULL, BU_CMD_ARG_REQUIRED, NULL, NULL, _values, BU_CMD_VALUE_RANGE_NONE}
#define BU_CMD_OPTION_SHAPED(_short, _long, _canonical, _record, _field, _type, _arg, _help, _requirement, _shape, _consume) \
    {_short, _long, _canonical, _arg, _help, _type, BU_CMD_STORAGE_OFFSET(_record, _field), NULL, NULL, NULL, NULL, 0, 0, NULL, _requirement, _shape, _consume, NULL, BU_CMD_VALUE_RANGE_NONE}
/**
 * The common required multi-token binding.  Use the fully explicit
 * BU_CMD_OPTION_SHAPED only for an optional shape or a canonical spelling
 * different from its long spelling.
 */
#define BU_CMD_OPTION_CONSUME(_short, _long, _record, _field, _type, _arg, _help, _shape, _consume) \
    BU_CMD_OPTION_SHAPED(_short, _long, _long, _record, _field, _type, _arg, _help, \
	BU_CMD_ARG_REQUIRED, _shape, _consume)
#define BU_CMD_OPTIONAL_CONSUME(_short, _long, _record, _field, _type, _arg, _help, _shape, _consume) \
    BU_CMD_OPTION_SHAPED(_short, _long, _long, _record, _field, _type, _arg, _help, \
	BU_CMD_ARG_OPTIONAL, _shape, _consume)
#define BU_CMD_ALIAS_SHORT(_short, _canonical, _hidden) \
    {_short, NULL, _canonical, NULL, NULL, BU_CMD_VALUE_FLAG, 0, NULL, NULL, NULL, _canonical, 0, _hidden, NULL, BU_CMD_ARG_NONE, NULL, NULL, NULL, BU_CMD_VALUE_RANGE_NONE}
#define BU_CMD_ALIAS_LONG(_long, _canonical, _hidden) \
    {NULL, _long, _canonical, NULL, NULL, BU_CMD_VALUE_FLAG, 0, NULL, NULL, NULL, _canonical, 0, _hidden, NULL, BU_CMD_ARG_NONE, NULL, NULL, NULL, BU_CMD_VALUE_RANGE_NONE}

/**
 * Parse command arguments using a compact schema.
 *
 * Returns the index of the first unconsumed operand, or -1 on error.  The
 * data object is owned by the caller and is required only when the schema has
 * bound options; an optionless schema may pass NULL.  For options-first and
 * stop-at-first-operand schemas argv is left in place.  For an
 * interspersed-option schema, recognized options (and their arguments) are
 * compacted at the front of argv and positional operands are compacted after
 * them in their original order.  This gives the caller one contiguous
 * operand suffix without duplicating the schema's option scanner.  A compact
 * short-option cluster made solely of no-argument flags (for example, -ah)
 * is accepted as the corresponding sequence of flags.  A one-letter short
 * option taking a compatible argument also accepts the conventional attached
 * spelling (for example, -m3).  The argv pointer array, but never the argument
 * strings, may therefore be reordered.
 */
BU_EXPORT extern int bu_cmd_schema_parse(const struct bu_cmd_schema *schema,
	void *data, struct bu_vls *msg, int argc, const char *argv[]);

/**
 * Parse all recognized options while preserving unknown option-like words as
 * operands.  This is useful for layered parsers: with an interspersed schema,
 * recognized options are compacted at the front and every leftover word is
 * kept in the returned operand suffix.  Other parsing and storage semantics
 * match bu_cmd_schema_parse.
 */
BU_EXPORT extern int bu_cmd_schema_parse_known(const struct bu_cmd_schema *schema,
	void *data, struct bu_vls *msg, int argc, const char *argv[]);

/**
 * Parse options and require the complete argv to satisfy the schema's
 * structural operand rules.  On success it returns the index of the first
 * operand, as bu_cmd_schema_parse does.  It deliberately does not invoke
 * application semantic providers: database and UI-context checks remain the
 * responsibility of the owning command.  A dash-leading numeric token may
 * be a positional integer/long/number without an explicit `--` marker when
 * the next operand role is numeric; other dash-leading operands need `--`.
 */
BU_EXPORT extern int bu_cmd_schema_parse_complete(const struct bu_cmd_schema *schema,
	void *data, struct bu_vls *msg, int argc, const char *argv[]);

/** Build a human-readable option listing from a compact command schema. */
BU_EXPORT extern char *bu_cmd_schema_describe(const struct bu_cmd_schema *schema);

/** Build a POSIX-style one-line synopsis from schema option and operand
 * structure.  invocation may name a command path such as "bot repair"; when
 * NULL or empty, schema->name is used.  The returned string begins with
 * "Usage:" and ends with a newline.  The caller owns it. */
BU_EXPORT extern char *bu_cmd_schema_usage(const struct bu_cmd_schema *schema,
	const char *invocation);

/** Build standard user-facing help containing a generated synopsis, schema
 * description, option rows, operand rows, repeated groups, and declarative
 * constraint hints.  Commands may append examples or other command-specific
 * notes without duplicating structural syntax.  The caller owns the result. */
BU_EXPORT extern char *bu_cmd_schema_help(const struct bu_cmd_schema *schema,
	const char *invocation);
/** Append schema help to an initialized dynamic string without exposing
 * ownership of the temporary generated string to the caller.  Returns 0 on
 * success and -1 for invalid input or generation failure. */
BU_EXPORT extern int bu_cmd_schema_help_append(struct bu_vls *output,
	const struct bu_cmd_schema *schema, const char *invocation);

/**
 * Build a human-readable listing for only the named canonical options.  A
 * NULL selected list includes every public option, matching
 * bu_cmd_schema_describe.  Commands can therefore organize one native schema
 * into topic-specific help without duplicating parser rows or help metadata.
 * The caller owns the result.
 */
BU_EXPORT extern char *bu_cmd_schema_describe_selected(const struct bu_cmd_schema *schema,
	const char * const *selected);

/** Build machine-readable schema metadata from a compact command schema. */
BU_EXPORT extern char *bu_cmd_schema_describe_json(const struct bu_cmd_schema *schema);

/** Append one correctly quoted and escaped JSON string.  NULL becomes "". */
BU_EXPORT extern void bu_cmd_json_string(struct bu_vls *out, const char *value);

/** Return the number of structural errors in one flat native schema. */
BU_EXPORT extern int bu_cmd_schema_lint(const struct bu_cmd_schema *schema,
	struct bu_vls *msgs);

/** Find a direct child by canonical spelling or an accepted alias. */
BU_EXPORT extern const struct bu_cmd_tree_node *bu_cmd_tree_find_subcommand(
	const struct bu_cmd_tree *tree, const char *name);

/** Dispatch an executable child.  When a matching intermediate child has no
 * executor, dispatch descends through its nested tree and invokes the first
 * matching executable leaf.  Returns zero when a child was found and
 * dispatched, -1 otherwise; the callback's result is stored in result.
 */
BU_EXPORT extern int bu_cmd_tree_dispatch(const struct bu_cmd_tree *tree,
	void *context, int argc, const char *argv[], int *result);

/**
 * Validate the command words after a tree's root command and collect its
 * static child candidates.  This is the argv-level counterpart to tree
 * dispatch: argv begins with the root option/child phase, not the root command
 * name itself.  It is intended for native grammar adapters that have a
 * parser-owned prefix before they delegate to a reusable child tree.
 */
BU_EXPORT extern int bu_cmd_tree_validate_argv(const struct bu_cmd_tree *tree,
	size_t argc, const char **argv, size_t cursor_arg,
	struct bu_cmd_validate_result *result);

/** Context-aware form of bu_cmd_tree_validate_argv.  Schema context hooks at
 * every visited tree level receive @p context. */
BU_EXPORT extern int bu_cmd_tree_validate_argv_ctx(const struct bu_cmd_tree *tree,
	size_t argc, const char **argv, size_t cursor_arg, void *context,
	struct bu_cmd_validate_result *result);

/** Build standardized user-facing help directly from a native tree.  The
 * invocation may supply a qualified command path; NULL uses the root schema
 * name. */
BU_EXPORT extern char *bu_cmd_tree_help(const struct bu_cmd_tree *tree,
	const char *invocation);
/** Append the same generated text as bu_cmd_tree_help to an initialized vls.
 * Returns 0 on success and -1 on failure. */
BU_EXPORT extern int bu_cmd_tree_help_append(struct bu_vls *output,
	const struct bu_cmd_tree *tree, const char *invocation);
/** Build help for one selected subcommand path.  path contains only child
 * spellings after the root command; invocation names the root.  Intermediate
 * nodes produce help for their remaining subtree and leaves produce their
 * complete schema help.  Returns NULL if the path does not exist. */
BU_EXPORT extern char *bu_cmd_tree_help_path(const struct bu_cmd_tree *tree,
	const char *invocation, size_t path_argc, const char * const *path_argv);
/** Compatibility spelling for bu_cmd_tree_help using the root schema name. */
BU_EXPORT extern char *bu_cmd_tree_describe(const struct bu_cmd_tree *tree);
/** Build a machine-readable description directly from a native tree. */
BU_EXPORT extern char *bu_cmd_tree_describe_json(const struct bu_cmd_tree *tree);

/** Return the number of structural errors in a native tree. */
BU_EXPORT extern int bu_cmd_tree_lint(const struct bu_cmd_tree *tree,
	struct bu_vls *msgs);

/** Return the selected member of a form set, or NULL for an invalid selection.
 * A NULL selector result is also returned when automatic syntax matching is
 * ambiguous. */
BU_EXPORT extern const struct bu_cmd_form *bu_cmd_forms_select(
	const struct bu_cmd_forms *forms, size_t argc,
	const char * const *argv, void *context);

/** Validate one argv position using the selected command form.  argv includes
 * the command spelling at index zero; validation of the selected schema/tree
 * starts at argv[1]. */
BU_EXPORT extern int bu_cmd_forms_validate(const struct bu_cmd_forms *forms,
	size_t argc, const char **argv, size_t cursor_arg, void *context,
	struct bu_cmd_validate_result *result);

/** Parse a selected flat-schema form.  Tree forms are dispatched with
 * bu_cmd_forms_dispatch instead.  The return value matches
 * bu_cmd_schema_parse_complete and excludes argv[0]. */
BU_EXPORT extern int bu_cmd_forms_parse(const struct bu_cmd_forms *forms,
	void *data, struct bu_vls *msg, int argc, const char *argv[],
	void *context, const struct bu_cmd_form **selected);

/** Dispatch a selected tree form.  Flat-schema forms are parsed with
 * bu_cmd_forms_parse instead. */
BU_EXPORT extern int bu_cmd_forms_dispatch(const struct bu_cmd_forms *forms,
	void *context, int argc, const char *argv[], int *result,
	const struct bu_cmd_form **selected);

/** Generate standardized help, JSON metadata, and structural diagnostics for
 * every alternative in a command form set. */
BU_EXPORT extern char *bu_cmd_forms_help(const struct bu_cmd_forms *forms,
	const char *invocation);
/** Append the same generated text as bu_cmd_forms_help to an initialized vls.
 * Returns 0 on success and -1 on failure. */
BU_EXPORT extern int bu_cmd_forms_help_append(struct bu_vls *output,
	const struct bu_cmd_forms *forms, const char *invocation);
BU_EXPORT extern char *bu_cmd_forms_describe_json(const struct bu_cmd_forms *forms);
BU_EXPORT extern int bu_cmd_forms_lint(const struct bu_cmd_forms *forms,
	struct bu_vls *msgs);

/**
 * Validate a cursor position and collect static completion candidates.
 *
 * @p result must be initialized with BU_CMD_VALIDATE_RESULT_NULL or
 * bu_cmd_validate_result_init().  This and the tree/form validation APIs
 * replace its previous contents on every call.  Success supplies the new
 * result; failure leaves the result initialized and empty.
 */
BU_EXPORT extern int bu_cmd_schema_validate(const struct bu_cmd_schema *schema,
	size_t argc, const char **argv, size_t cursor_arg,
	struct bu_cmd_validate_result *result);

/** Validate only the declarative option/operand grammar and constraints.
 * Schema-owned validation callbacks use this entry point to refine the base
 * result without recursively invoking themselves. */
BU_EXPORT extern int bu_cmd_schema_validate_syntax(const struct bu_cmd_schema *schema,
	size_t argc, const char **argv, size_t cursor_arg,
	struct bu_cmd_validate_result *result);

/**
 * Validate using the ordinary native schema grammar plus an optional
 * application-supplied context validator.  With a NULL context, or for a
 * schema without a context validator, this is identical to
 * bu_cmd_schema_validate.
 */
BU_EXPORT extern int bu_cmd_schema_validate_ctx(const struct bu_cmd_schema *schema,
	size_t argc, const char **argv, size_t cursor_arg, void *context,
	struct bu_cmd_validate_result *result);

/**
 * Report whether a canonical option name is selected in argv.  This applies
 * the schema's parse policy, aliases, compact flag clusters, option argument
 * shapes, and the standalone -- marker exactly as the parser does.
 */
BU_EXPORT extern int bu_cmd_schema_option_present(const struct bu_cmd_schema *schema,
	size_t argc, const char **argv, const char *canonical);

/**
 * Count positional operands in argv using the schema's parse policy and
 * option grammar.  This is primarily for a schema-owned validator that needs
 * a conditional role or cardinality rule beyond declarative constraints.
 */
BU_EXPORT extern size_t bu_cmd_schema_operand_count(const struct bu_cmd_schema *schema,
	size_t argc, const char **argv);

/** Resolve the declarative role for one zero-based positional index,
 * including repeated heterogeneous operand groups. */
BU_EXPORT extern const struct bu_cmd_operand *bu_cmd_schema_operand(
	const struct bu_cmd_schema *schema, size_t operand_index);

/** Resolve a positional role after applying the first option-selected operand
 * case matching argv.  Schemas without cases behave exactly like
 * bu_cmd_schema_operand. */
BU_EXPORT extern const struct bu_cmd_operand *bu_cmd_schema_active_operand(
	const struct bu_cmd_schema *schema, size_t argc, const char **argv,
	size_t operand_index);

/** Validate one positional value using an operand row's declared type,
 * keyword vocabulary, callback, and range without scanning a command. */
BU_EXPORT extern int bu_cmd_operand_validate(const struct bu_cmd_operand *operand,
	const char *arg);

/**
 * Return the number of tokens occupied by the option beginning at argv[0].
 *
 * A positive result includes the option word and every argument token that
 * the schema's own option scanner would consume.  Zero means argv[0] is not
 * a recognized option (or is an ordinary operand).  A negative result means
 * it names an option but its spelling or supplied value is incomplete or
 * malformed.  This is intended for command-tree adapters that need to find
 * the first parser-owned subcommand without duplicating option-argument
 * rules.  It does not bind values or invoke semantic providers.
 */
BU_EXPORT extern int bu_cmd_schema_option_span(const struct bu_cmd_schema *schema,
	size_t argc, const char **argv);

/** Initialize @p result before its first validation or clear operation. */
BU_EXPORT extern void bu_cmd_validate_result_init(struct bu_cmd_validate_result *result);

/**
 * Release candidate data owned by an initialized result.  Validation APIs
 * replace any candidates already owned by the result, so initialized result
 * objects may be reused.  Calling clear repeatedly is safe; passing
 * uninitialized storage is not.  BU_CMD_VALIDATE_RESULT_NULL is the static
 * initializer.
 */
BU_EXPORT extern void bu_cmd_validate_result_clear(struct bu_cmd_validate_result *result);

/** Replace an initialized validation result with one concise scalar
 * classification.  Any owned candidates from the previous result are
 * released first. */
BU_EXPORT extern void bu_cmd_validate_result_set(struct bu_cmd_validate_result *result,
	bu_cmd_validate_state_t state, size_t token, unsigned int expected,
	bu_cmd_value_t type, const char *hint, const char *semantic_provider);

__END_DECLS

#endif /* BU_CMDSCHEMA_H */
