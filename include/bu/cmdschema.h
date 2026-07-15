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
 * Compact, single-source command option schemas.
 *
 * This is deliberately separate from the deprecated bu_opt_desc callback
 * table.  A schema records both the command syntax and the storage binding in
 * one row.  bu_cmd_schema_parse is the execution-facing parser; command
 * registries publish this native representation directly.
 */

#ifndef BU_CMDSCHEMA_H
#define BU_CMDSCHEMA_H

#include "common.h"

#include <stddef.h>

#include "vmath.h"
#include "bu/defines.h"
#include "bu/vls.h"

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
 * Return zero for an accepted value.  This is useful for ranges and command
 * vocabulary constraints without losing the option's integer/number/string
 * type to completion and highlighting consumers.
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
 * unit.  This accepts the same vocabulary and quantity forms as
 * bu_units_conversion, requires a positive finite result, and leaves value
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


/**
 * One canonical command option.  storage_offset is relative to the data
 * object supplied to bu_cmd_schema_parse.  Alias records set alias_of to the
 * canonical option name and do not need their own storage binding.
 */
struct bu_cmd_option {
    const char *shortopt;
    const char *longopt;
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
};


struct bu_cmd_schema;
struct bu_cmd_validate_result;

/**
 * A schema-owned validator for syntax that cannot be expressed by declarative
 * constraint rows or ordinary operand cardinality.  It must be side-effect
 * free.  A callback normally copies the schema, clears custom_validate in the
 * copy, invokes bu_cmd_schema_validate for the ordinary flat rules, and then
 * applies its additional rule.
 */
typedef int (*bu_cmd_schema_validate_t)(const struct bu_cmd_schema *schema,
	size_t argc, const char **argv, size_t cursor_arg,
	struct bu_cmd_validate_result *result);

/**
 * Context-aware companion to a native schema validator.  The ordinary
 * validator remains context-free so libbu can use it everywhere; hosts such
 * as GED may supply application state for semantic rules that depend on a
 * database, view, or other live command environment.  Implementations should
 * copy the schema, clear validation.constraint_data.context_validate in the
 * copy, and delegate the flat grammar to bu_cmd_schema_validate before
 * applying context-specific rules.
 */
typedef int (*bu_cmd_schema_context_validate_t)(const struct bu_cmd_schema *schema,
	size_t argc, const char **argv, size_t cursor_arg, void *context,
	struct bu_cmd_validate_result *result);


/**
 * Positional role declaration used by schema consumers and documentation.
 * validate augments the scalar type with a side-effect-free constraint.
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
};


/** A declarative condition for a compact command constraint. */
typedef enum {
    BU_CMD_CONDITION_ALWAYS = 0,
    BU_CMD_CONDITION_ANY_OPTION_PRESENT,
    BU_CMD_CONDITION_NO_OPTION_PRESENT,
    BU_CMD_CONDITION_ALL_OPTIONS_PRESENT
} bu_cmd_constraint_condition_t;


/** The syntax relationship enforced by a compact command constraint. */
typedef enum {
    BU_CMD_CONSTRAINT_OPTION_COUNT = 0,
    BU_CMD_CONSTRAINT_OPERAND_COUNT
} bu_cmd_constraint_kind_t;


/**
 * A compact relationship row.  options is a NULL-terminated list of canonical
 * option names (without dashes).  Option-count constraints count distinct
 * selected canonical options; repeated spellings do not create a conflict.
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


struct bu_cmd_schema_constraint_data {
    bu_cmd_schema_validate_t custom_validate;
    const struct bu_cmd_constraint *constraints;
    bu_cmd_schema_context_validate_t context_validate;
};


/**
 * The final schema slot is a callback for ordinary schemas.  A constrained
 * schema initializes the same slot with BU_CMD_SCHEMA_CONSTRAINTS, supplying
 * both an optional callback and declarative relationship rows.  The union
 * keeps pre-constraint schema initializers source-compatible.
 */
union bu_cmd_schema_validation {
    bu_cmd_schema_validate_t custom_validate;
    struct bu_cmd_schema_constraint_data constraint_data;
#ifdef __cplusplus
    constexpr bu_cmd_schema_validation(bu_cmd_schema_validate_t validator = NULL) : custom_validate(validator) {}
    constexpr bu_cmd_schema_validation(bu_cmd_schema_validate_t validator,
	const struct bu_cmd_constraint *constraint_rows) : constraint_data{validator, constraint_rows, NULL} {}
    constexpr bu_cmd_schema_validation(bu_cmd_schema_validate_t validator,
	const struct bu_cmd_constraint *constraint_rows,
	bu_cmd_schema_context_validate_t context_validator) : constraint_data{validator, constraint_rows, context_validator} {}
#endif
};


struct bu_cmd_schema {
    const char *name;
    const char *help;
    const struct bu_cmd_option *options;
    const struct bu_cmd_operand *operands;
    bu_cmd_parse_policy_t parse_policy;
    union bu_cmd_schema_validation validation;
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


#define BU_CMD_OPTION_NULL {NULL, NULL, NULL, NULL, NULL, BU_CMD_VALUE_FLAG, 0, NULL, NULL, NULL, NULL, 0, 0, NULL, BU_CMD_ARG_NONE, NULL, NULL, NULL}
#define BU_CMD_OPERAND(_name, _type, _min, _max, _help, _provider) {_name, _min, _max, _help, _type, NULL, _provider, NULL, NULL, NULL}
#define BU_CMD_OPERAND_KEYWORDS(_name, _type, _min, _max, _help, _provider, _keywords) {_name, _min, _max, _help, _type, NULL, _provider, _keywords, NULL, NULL}
#define BU_CMD_OPERAND_KEYWORD_VALUES(_name, _type, _min, _max, _help, _provider, _keywords) {_name, _min, _max, _help, _type, NULL, _provider, NULL, _keywords, NULL}
#define BU_CMD_OPERAND_VALIDATE(_name, _type, _min, _max, _validator, _help, _provider) {_name, _min, _max, _help, _type, _validator, _provider, NULL, NULL, NULL}
#define BU_CMD_OPERAND_SHAPED(_name, _type, _min, _max, _validator, _help, _provider, _shape) {_name, _min, _max, _help, _type, _validator, _provider, NULL, NULL, _shape}
#define BU_CMD_OPERAND_NULL {NULL, 0, 0, NULL, BU_CMD_VALUE_STRING, NULL, NULL, NULL, NULL, NULL}
#define BU_CMD_CONSTRAINT_OPTIONS(_options, _min, _max, _hint) {BU_CMD_CONSTRAINT_OPTION_COUNT, BU_CMD_CONDITION_ALWAYS, _options, _min, _max, _hint}
#define BU_CMD_CONSTRAINT_OPERANDS(_condition, _options, _min, _max, _hint) {BU_CMD_CONSTRAINT_OPERAND_COUNT, _condition, _options, _min, _max, _hint}
#define BU_CMD_CONSTRAINT_NULL {BU_CMD_CONSTRAINT_OPTION_COUNT, BU_CMD_CONDITION_ALWAYS, NULL, 0, 0, NULL}
/** Initialize a schema's final slot with a validator and constraint table. */
#ifdef __cplusplus
#  define BU_CMD_SCHEMA_CONSTRAINTS(_validator, _constraints) bu_cmd_schema_validation(_validator, _constraints)
#  define BU_CMD_SCHEMA_CONTEXT_VALIDATOR(_validator) bu_cmd_schema_validation(NULL, NULL, _validator)
#else
#  define BU_CMD_SCHEMA_CONSTRAINTS(_validator, _constraints) {.constraint_data = {_validator, _constraints, NULL}}
#  define BU_CMD_SCHEMA_CONTEXT_VALIDATOR(_validator) {.constraint_data = {NULL, NULL, _validator}}
#endif
#define BU_CMD_COUNT_UNLIMITED ((size_t)-1)
#define BU_CMD_STORAGE_NONE ((size_t)-1)
#define BU_CMD_ARG_SHAPE(_kind, _min, _max, _description) {_kind, _min, _max, _description, NULL}
#define BU_CMD_VALIDATE_RESULT_NULL {BU_CMD_VALIDATE_UNKNOWN, 0, 0, BU_CMD_EXPECT_NONE, NULL, 0, NULL, BU_CMD_VALUE_STRING, NULL}
#define BU_CMD_TREE_NODE(_schema, _aliases, _children, _phase, _execute) {_schema, _aliases, _children, _phase, _execute}
#define BU_CMD_TREE_NODE_NULL {NULL, NULL, NULL, BU_CMD_TREE_CHILD_AFTER_OPTIONS, NULL}


/** Compact option declaration helpers. */
#define BU_CMD_FLAG(_short, _long, _record, _field, _help) \
    {_short, _long, ((_long) ? (_long) : (_short)), NULL, _help, BU_CMD_VALUE_FLAG, offsetof(_record, _field), NULL, NULL, NULL, NULL, 0, 0, NULL, BU_CMD_ARG_NONE, NULL, NULL, NULL}
/** A no-argument flag whose int storage field counts occurrences. */
#define BU_CMD_COUNTING_FLAG(_short, _long, _record, _field, _help) \
    {_short, _long, ((_long) ? (_long) : (_short)), NULL, _help, BU_CMD_VALUE_FLAG, offsetof(_record, _field), NULL, NULL, NULL, NULL, 1, 0, NULL, BU_CMD_ARG_NONE, NULL, NULL, NULL}
/** A no-argument flag whose long storage field counts occurrences. */
#define BU_CMD_COUNTING_LONG_FLAG(_short, _long, _record, _field, _help) \
    {_short, _long, ((_long) ? (_long) : (_short)), NULL, _help, BU_CMD_VALUE_LONG, offsetof(_record, _field), NULL, NULL, NULL, NULL, 1, 0, NULL, BU_CMD_ARG_NONE, NULL, NULL, NULL}
/**
 * A no-argument option that invokes a command-specific state transition.
 * The parser receives a NULL argument and the address of the selected field.
 */
#define BU_CMD_CUSTOM_FLAG(_short, _long, _canonical, _record, _field, _parser, _help) \
    {_short, _long, _canonical, NULL, _help, BU_CMD_VALUE_CUSTOM, offsetof(_record, _field), _parser, NULL, NULL, NULL, 0, 0, NULL, BU_CMD_ARG_NONE, NULL, NULL, NULL}
/** A syntax-only flag for commands whose execution parser owns its state. */
#define BU_CMD_FLAG_UNBOUND(_short, _long, _canonical, _help) \
    {_short, _long, _canonical, NULL, _help, BU_CMD_VALUE_FLAG, BU_CMD_STORAGE_NONE, NULL, NULL, NULL, NULL, 0, 0, NULL, BU_CMD_ARG_NONE, NULL, NULL, NULL}
/**
 * A syntax-only typed option for a command whose execution parser owns its
 * state.  It participates in native help, validation, and completion but
 * bu_cmd_schema_parse deliberately rejects it because it has no storage
 * binding.  Use this only while a command's independently owned executor is
 * being migrated; new parsers should bind an argument record directly.
 */
#define BU_CMD_VALUE_UNBOUND(_short, _long, _canonical, _type, _arg, _help) \
    {_short, _long, _canonical, _arg, _help, _type, BU_CMD_STORAGE_NONE, NULL, NULL, NULL, NULL, 0, 0, NULL, BU_CMD_ARG_REQUIRED, NULL, NULL, NULL}
#define BU_CMD_BOOL(_short, _long, _record, _field, _arg, _help) \
    {_short, _long, ((_long) ? (_long) : (_short)), _arg, _help, BU_CMD_VALUE_BOOL, offsetof(_record, _field), NULL, NULL, NULL, NULL, 0, 0, NULL, BU_CMD_ARG_REQUIRED, NULL, NULL, NULL}
#define BU_CMD_INTEGER(_short, _long, _record, _field, _arg, _help) \
    {_short, _long, ((_long) ? (_long) : (_short)), _arg, _help, BU_CMD_VALUE_INTEGER, offsetof(_record, _field), NULL, NULL, NULL, NULL, 0, 0, NULL, BU_CMD_ARG_REQUIRED, NULL, NULL, NULL}
#define BU_CMD_HEX_INTEGER(_short, _long, _record, _field, _arg, _help) \
    {_short, _long, ((_long) ? (_long) : (_short)), _arg, _help, BU_CMD_VALUE_HEX_INTEGER, offsetof(_record, _field), NULL, NULL, NULL, NULL, 0, 0, NULL, BU_CMD_ARG_REQUIRED, NULL, NULL, NULL}
#define BU_CMD_LONG(_short, _long, _record, _field, _arg, _help) \
    {_short, _long, ((_long) ? (_long) : (_short)), _arg, _help, BU_CMD_VALUE_LONG, offsetof(_record, _field), NULL, NULL, NULL, NULL, 0, 0, NULL, BU_CMD_ARG_REQUIRED, NULL, NULL, NULL}
#define BU_CMD_HEX_LONG(_short, _long, _record, _field, _arg, _help) \
    {_short, _long, ((_long) ? (_long) : (_short)), _arg, _help, BU_CMD_VALUE_HEX_LONG, offsetof(_record, _field), NULL, NULL, NULL, NULL, 0, 0, NULL, BU_CMD_ARG_REQUIRED, NULL, NULL, NULL}
#define BU_CMD_CHAR(_short, _long, _record, _field, _arg, _help) \
    {_short, _long, ((_long) ? (_long) : (_short)), _arg, _help, BU_CMD_VALUE_CHAR, offsetof(_record, _field), NULL, NULL, NULL, NULL, 0, 0, NULL, BU_CMD_ARG_REQUIRED, NULL, NULL, NULL}
#define BU_CMD_OPTIONAL_INTEGER(_short, _long, _record, _field, _arg, _help) \
    {_short, _long, ((_long) ? (_long) : (_short)), _arg, _help, BU_CMD_VALUE_INTEGER, offsetof(_record, _field), NULL, NULL, NULL, NULL, 0, 0, NULL, BU_CMD_ARG_OPTIONAL, NULL, NULL, NULL}
#define BU_CMD_NUMBER(_short, _long, _record, _field, _arg, _help) \
    {_short, _long, ((_long) ? (_long) : (_short)), _arg, _help, BU_CMD_VALUE_NUMBER, offsetof(_record, _field), NULL, NULL, NULL, NULL, 0, 0, NULL, BU_CMD_ARG_REQUIRED, NULL, NULL, NULL}
#define BU_CMD_COLOR(_short, _long, _record, _field, _arg, _help) \
    {_short, _long, ((_long) ? (_long) : (_short)), _arg, _help, BU_CMD_VALUE_COLOR, offsetof(_record, _field), NULL, NULL, NULL, NULL, 0, 0, NULL, BU_CMD_ARG_REQUIRED, NULL, NULL, NULL}
/**
 * A one-or-three-token color option with bu_opt_color-compatible value
 * grammar.  It accepts any scalar bu_color_from_str spelling or three
 * separate RGB components.  Use BU_CMD_RGB for strict 8-bit RGB only.
 */
#define BU_CMD_COLOR_COMPAT(_short, _long, _record, _field, _arg, _help) \
    {_short, _long, ((_long) ? (_long) : (_short)), _arg, _help, BU_CMD_VALUE_COLOR, offsetof(_record, _field), NULL, NULL, NULL, NULL, 0, 0, NULL, BU_CMD_ARG_REQUIRED, &bu_cmd_color_arg_shape, bu_cmd_color_consume, NULL}
/**
 * A standard RGB option bound to a struct bu_color field.  It accepts one
 * packed r/g/b, r,g,b, or r;g;b token, or three separate 0..255 channels.
 */
#define BU_CMD_RGB(_short, _long, _record, _field, _arg, _help) \
    {_short, _long, ((_long) ? (_long) : (_short)), _arg, _help, BU_CMD_VALUE_COLOR, offsetof(_record, _field), NULL, NULL, NULL, NULL, 0, 0, NULL, BU_CMD_ARG_REQUIRED, &bu_cmd_rgb_arg_shape, bu_cmd_rgb_consume, NULL}
/**
 * A standard finite XYZ vector option bound to a point_t or vect_t field.
 * It accepts packed x/y/z, x,y,z, or x;y;z input, a quoted x y z token, or
 * three separate numeric arguments.
 */
#define BU_CMD_VECTOR3(_short, _long, _record, _field, _arg, _help) \
    {_short, _long, ((_long) ? (_long) : (_short)), _arg, _help, BU_CMD_VALUE_VECTOR, offsetof(_record, _field), NULL, NULL, NULL, NULL, 0, 0, NULL, BU_CMD_ARG_REQUIRED, &bu_cmd_vector3_arg_shape, bu_cmd_vector3_consume, NULL}
#define BU_CMD_STRING(_short, _long, _record, _field, _arg, _help) \
    {_short, _long, ((_long) ? (_long) : (_short)), _arg, _help, BU_CMD_VALUE_STRING, offsetof(_record, _field), NULL, NULL, NULL, NULL, 0, 0, NULL, BU_CMD_ARG_REQUIRED, NULL, NULL, NULL}
#define BU_CMD_OPTIONAL_STRING(_short, _long, _record, _field, _arg, _help) \
    {_short, _long, ((_long) ? (_long) : (_short)), _arg, _help, BU_CMD_VALUE_STRING, offsetof(_record, _field), NULL, NULL, NULL, NULL, 0, 0, NULL, BU_CMD_ARG_OPTIONAL, &bu_cmd_optional_scalar_arg_shape, NULL, NULL}
#define BU_CMD_VLS_APPEND(_short, _long, _record, _field, _arg, _help) \
    {_short, _long, ((_long) ? (_long) : (_short)), _arg, _help, BU_CMD_VALUE_VLS, offsetof(_record, _field), NULL, NULL, NULL, NULL, 0, 0, NULL, BU_CMD_ARG_REQUIRED, NULL, NULL, NULL}
#define BU_CMD_FILE(_short, _long, _record, _field, _arg, _help) \
    {_short, _long, ((_long) ? (_long) : (_short)), _arg, _help, BU_CMD_VALUE_FILE, offsetof(_record, _field), NULL, NULL, NULL, NULL, 0, 0, NULL, BU_CMD_ARG_REQUIRED, NULL, NULL, NULL}
#define BU_CMD_DB_OBJECT(_short, _long, _record, _field, _arg, _help) \
    {_short, _long, ((_long) ? (_long) : (_short)), _arg, _help, BU_CMD_VALUE_DB_OBJECT, offsetof(_record, _field), NULL, NULL, NULL, NULL, 0, 0, NULL, BU_CMD_ARG_REQUIRED, NULL, NULL, NULL}
#define BU_CMD_DB_PATH(_short, _long, _record, _field, _arg, _help) \
    {_short, _long, ((_long) ? (_long) : (_short)), _arg, _help, BU_CMD_VALUE_DB_PATH, offsetof(_record, _field), NULL, NULL, NULL, NULL, 0, 0, NULL, BU_CMD_ARG_REQUIRED, NULL, NULL, NULL}
#define BU_CMD_CUSTOM(_short, _long, _record, _field, _parser, _arg, _help) \
    {_short, _long, ((_long) ? (_long) : (_short)), _arg, _help, BU_CMD_VALUE_CUSTOM, offsetof(_record, _field), _parser, NULL, NULL, NULL, 0, 0, NULL, BU_CMD_ARG_REQUIRED, NULL, NULL, NULL}
#define BU_CMD_INTEGER_VALIDATE(_short, _long, _record, _field, _validator, _arg, _help) \
    {_short, _long, ((_long) ? (_long) : (_short)), _arg, _help, BU_CMD_VALUE_INTEGER, offsetof(_record, _field), NULL, _validator, NULL, NULL, 0, 0, NULL, BU_CMD_ARG_REQUIRED, NULL, NULL, NULL}
#define BU_CMD_NUMBER_VALIDATE(_short, _long, _record, _field, _validator, _arg, _help) \
    {_short, _long, ((_long) ? (_long) : (_short)), _arg, _help, BU_CMD_VALUE_NUMBER, offsetof(_record, _field), NULL, _validator, NULL, NULL, 0, 0, NULL, BU_CMD_ARG_REQUIRED, NULL, NULL, NULL}
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
    {_short, _long, ((_long) ? (_long) : (_short)), _arg, _help, BU_CMD_VALUE_STRING, offsetof(_record, _field), NULL, _validator, NULL, NULL, 0, 0, NULL, BU_CMD_ARG_REQUIRED, NULL, NULL, NULL}
#define BU_CMD_CHAR_VALIDATE(_short, _long, _record, _field, _validator, _arg, _help) \
    {_short, _long, ((_long) ? (_long) : (_short)), _arg, _help, BU_CMD_VALUE_CHAR, offsetof(_record, _field), NULL, _validator, NULL, NULL, 0, 0, NULL, BU_CMD_ARG_REQUIRED, NULL, NULL, NULL}
#define BU_CMD_VLS_APPEND_VALIDATE(_short, _long, _record, _field, _validator, _arg, _help) \
    {_short, _long, ((_long) ? (_long) : (_short)), _arg, _help, BU_CMD_VALUE_VLS, offsetof(_record, _field), NULL, _validator, NULL, NULL, 0, 0, NULL, BU_CMD_ARG_REQUIRED, NULL, NULL, NULL}
/** Append one validated ISO 639-1 language code to a bu_vls field. */
#define BU_CMD_ISO639_1(_short, _long, _record, _field, _arg, _help) \
    BU_CMD_VLS_APPEND_VALIDATE(_short, _long, _record, _field, bu_cmd_iso639_1_validate, _arg, _help)
/** Bind a validated BRL-CAD manual-page section identifier to a char field. */
#define BU_CMD_MAN_SECTION(_short, _long, _record, _field, _arg, _help) \
    BU_CMD_CHAR_VALIDATE(_short, _long, _record, _field, bu_cmd_man_section_validate, _arg, _help)
#define BU_CMD_KEYWORD_VALUES(_short, _long, _record, _field, _arg, _help, _values) \
    {_short, _long, ((_long) ? (_long) : (_short)), _arg, _help, BU_CMD_VALUE_KEYWORD, offsetof(_record, _field), NULL, NULL, NULL, NULL, 0, 0, NULL, BU_CMD_ARG_REQUIRED, NULL, NULL, _values}
#define BU_CMD_OPTION_SHAPED(_short, _long, _canonical, _record, _field, _type, _arg, _help, _requirement, _shape, _consume) \
    {_short, _long, _canonical, _arg, _help, _type, offsetof(_record, _field), NULL, NULL, NULL, NULL, 0, 0, NULL, _requirement, _shape, _consume, NULL}
#define BU_CMD_ALIAS_SHORT(_short, _canonical, _hidden) \
    {_short, NULL, _canonical, NULL, NULL, BU_CMD_VALUE_FLAG, 0, NULL, NULL, NULL, _canonical, 0, _hidden, NULL, BU_CMD_ARG_NONE, NULL, NULL, NULL}
#define BU_CMD_ALIAS_LONG(_long, _canonical, _hidden) \
    {NULL, _long, _canonical, NULL, NULL, BU_CMD_VALUE_FLAG, 0, NULL, NULL, NULL, _canonical, 0, _hidden, NULL, BU_CMD_ARG_NONE, NULL, NULL, NULL}


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
 * is accepted as the corresponding sequence of flags.  The argv pointer
 * array, but never the argument strings, may therefore be reordered.
 */
BU_EXPORT extern int bu_cmd_schema_parse(const struct bu_cmd_schema *schema,
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

/** Build generic help and JSON descriptions directly from a native tree. */
BU_EXPORT extern char *bu_cmd_tree_describe(const struct bu_cmd_tree *tree);
BU_EXPORT extern char *bu_cmd_tree_describe_json(const struct bu_cmd_tree *tree);

/** Return the number of structural errors in a native tree. */
BU_EXPORT extern int bu_cmd_tree_lint(const struct bu_cmd_tree *tree,
	struct bu_vls *msgs);

/** Validate a cursor position and collect static completion candidates. */
BU_EXPORT extern int bu_cmd_schema_validate(const struct bu_cmd_schema *schema,
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

/** Release candidate data allocated by bu_cmd_schema_validate. */
BU_EXPORT extern void bu_cmd_validate_result_clear(struct bu_cmd_validate_result *result);

__END_DECLS

#endif /* BU_CMDSCHEMA_H */
