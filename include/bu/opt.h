/*                         O P T . H
 * BRL-CAD
 *
 * Copyright (c) 2015-2026 United States Government as represented by
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
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */

#ifndef BU_OPT_H
#define BU_OPT_H

#include "common.h"
#include "bu/defines.h"
#include "bu/cmdschema.h"
#include "bu/ptbl.h"
#include "bu/vls.h"

__BEGIN_DECLS

/** @addtogroup bu_opt
 * @brief
 * Generalized option handling.
 *
 * This module implements a callback and assignment based mechanism
 * for generalized handling of option handling.  Functionally it is
 * intended to provide capabilities similar to getopt_long, Qt's
 * QCommandLineParser, and the tclap library.  Results are returned by
 * way of variable assignment, and function callbacks are used to
 * convert and validate argument strings.
 *
 * The bu_opt option parsing system does not make any use of global
 * values, unless a user defines an option definition array that
 * passes in pointers to global variables for setting.
 *
 * To set up a bu_opt parsing system, an array of bu_opt_desc (option
 * description) structures is defined and terminated with a
 * BU_OPT_DESC_NULL entry.  This array is then used by @link
 * bu_opt_parse @endlink to process an argv array.
 *
 * bu_opt is the succinct option interface to libbu's command parsing engine.
 * It can infer basic validation and completion from its standard readers.
 * Commands that also need typed operands, declarative constraints or ranges,
 * linting, or JSON publication can combine these rows with higher-level
 * command metadata or use the full bu_cmd_schema API in bu/cmdschema.h.
 *
 * When defining a bu_opt_desc entry, the type of the set_var
 * assignment variable needed is determined by the arg_process
 * callback.  If no callback is present, set_var is expected to be an
 * integer that will be set to 1 if the option is present in the argv
 * string.
 *
 * There are two styles in which a bu_opt_desc array may be
 * initialized.  The first is very compact but in C89 based code
 * requires static variables as set_var entries, as seen in the
 * following example:
 *
 * @code
 * #define help_str "Print help and exit"
 * static int ph = 0;
 * static int i = 0;
 * static fastf_t f = 0.0;
 * struct bu_opt_desc opt_defs[] = {
 *     {"h", "help",    "",  NULL,            &ph, help_str},
 *     {"n", "num",     "#", &bu_opt_int,     &i,  "Read int"},
 *     {"f", "fastf_t", "#", &bu_opt_fastf_t, &f,  "Read float"},
 *     BU_OPT_DESC_NULL
 * };
 * @endcode
 *
 * This style of initialization is suitable for application programs,
 * but in libraries such static variables will preclude thread and
 * reentrant safety.  For libraries, the BU_OPT and BU_OPT_NULL macros
 * are used to construct a bu_opt_desc array that does not require
 * static variables:
 *
 * @code
 * #define help_str "Print help and exit"
 * int ph = 0;
 * int i = 0;
 * fastf_t f = 0.0;
 * struct bu_opt_desc opt_defs[4];
 * BU_OPT(opt_defs[0], "h", "help",    "",  NULL,            &ph, help_str);
 * BU_OPT(opt_defs[1], "n", "num",     "#", &bu_opt_int,     &i,  "Read int");
 * BU_OPT(opt_defs[2], "f", "fastf_t", "#", &bu_opt_fastf_t, &f,  "Read float");
 * BU_OPT_NULL(opt_defs[3]);
 * @endcode
 *
 * Given the option description array and argc/argv data, @link
 * bu_opt_parse @endlink will do the rest.  The design of @link
 * bu_opt_parse @endlink is to fail early when an invalid option
 * situation is encountered, so code using this system needs to be
 * ready to handle such cases.
 *
 * For generating descriptive help strings from a bu_opt_desc array
 * use the @link bu_opt_describe @endlink function, which supports
 * multiple output styles and formats.
 */
/** @{ */
/** @file bu/opt.h */

/**
 * Callback function signature for bu_opt_desc argument processing
 * functions. Any user defined argument processing function should
 * match this signature and return values as documented below.
 *
 * @param[out]    msg     If not NULL, callback messages (usually
 *                        error descriptions) may be appended here.
 * @param[in]     argc    Number of arguments in argv.
 * @param[in]     argv    @em All arguments that follow the option flag.
 * @param[in, out] set_var The value specified in the associated bu_opt_desc.
 *
 * @returns
 * Val | Interpretation
 * --- | --------------
 * -1  | Invalid argument encountered, or argument expected but not found.
 *  0  | No argument processed (not an error.)
 * >0  | Number of argv elements used in valid argument processing.
 *
 * An example user-defined argument processing function:
 * @code
 * static int
 * parse_opt_mode(struct bu_vls *msg, size_t argc, const char **argv, void *set_var)
 * {
 *     int ret, mode;
 *
 *     BU_OPT_CHECK_ARGV0(msg, argc, argv, "mode");
 *
 *     ret = bu_opt_int(msg, argc, argv, set_var);
 *     mode = *(int *)set_var;
 *
 *     if (mode < 0 || mode > 2) {
 *         ret = -1;
 *         if (msg) {
 *             bu_vls_printf(msg, "Error: mode must be 0, 1, or 2.");
 *         }
 *     }
 *     return ret;
 * }
 * @endcode
 */
typedef int (*bu_opt_arg_process_t)(struct bu_vls *msg, size_t argc, const char **argv, void *set_var);


/**
 * A common task when writing bu_opt_arg_process_t validators is to
 * check the first argument of the argv array.  This macro
 * encapsulates that into a standard check.
 */
#define BU_OPT_CHECK_ARGV0(_msg, _argc, _argv, _opt_name) do { \
	if ((_argc) < 1 || !(_argv) || !(_argv)[0] || (_argv)[0][0] == '\0') { \
	    if ((_msg)) { \
		bu_vls_printf((_msg), "ERROR: missing required argument: %s\n", (_opt_name)); \
	    } \
	    return -1; \
	} \
    } while (0)


/**
 * @brief
 * "Option description" structure.
 *
 * Arrays of this structure are used to define command line options.
 */
struct bu_opt_desc {
    const char *shortopt;             /**< @brief "Short" option (i.e. -h for help option) */
    const char *longopt;              /**< @brief "Long" option (i.e. --help for help option) */
    const char *arg_helpstr;          /**< @brief Documentation describing option argument, if any (i.e. "file" in --input file)*/
    bu_opt_arg_process_t arg_process; /**< @brief Argument processing function pointer */
    void *set_var;                    /**< @brief Pointer to the variable or structure that collects this option's results */
    const char *help_string;          /**< @brief Option description */
};


/** Option scanning policy.  The legacy bu_opt_parse behavior is
 * BU_OPT_PARSE_INTERSPERSED.  OPTIONS_FIRST matches traditional getopt:
 * scanning stops at the first operand, or after a standalone -- marker. */
typedef enum {
    BU_OPT_PARSE_INTERSPERSED = 0,
    BU_OPT_PARSE_OPTIONS_FIRST
} bu_opt_parse_policy_t;


/** Convenience initializer for NULL bu_opt_desc array terminator */
#define BU_OPT_DESC_NULL {NULL, NULL, NULL, NULL, NULL, NULL}

/** Macro for assigning values to bu_opt_desc array entries. */
#define BU_OPT(_desc, _so, _lo, _ahelp, _aprocess, _var, _help) do { \
	(_desc).shortopt = _so; \
	(_desc).longopt = _lo; \
	(_desc).arg_helpstr = _ahelp; \
	(_desc).arg_process = _aprocess; \
	(_desc).set_var = (void *)_var;  \
	(_desc).help_string = _help; \
    } while (0)

/** Convenience macro for setting a bu_opt_desc struct to BU_OPT_DESC_NULL */
#define BU_OPT_NULL(_desc) do { \
	(_desc).shortopt = NULL; \
	(_desc).longopt = NULL; \
	(_desc).arg_helpstr = NULL; \
	(_desc).arg_process = NULL; \
	(_desc).set_var = NULL; \
	(_desc).help_string = NULL; \
    } while (0)


/**
 * Parse @p argv array using option descriptions.
 *
 * The bu_opt_desc array @p ds must be terminated with
 * BU_OPT_DESC_NULL.
 *
 * @returns
 * Val | Interpretation
 * --- | --------------
 * -1  | Fatal error in parsing.  Program must decide to recover or exit.
 *  0  | All argv options handled.
 *  >0 | Number of unused argv entries returned at the beginning of the argv array.
 *
 * @param[out] msgs    will collect any informational messages
 *                     generated by the parser (typically used for
 *                     error reporting)
 * @param[in] ac       number of input arguments in argv
 * @param[in, out]     argv a return value >0 indicates that argv has been
 *                     reordered to move the indicated number of
 *                     unused args to the beginning of the array
 * @param[in] ds       option structure
 */
BU_EXPORT extern int bu_opt_parse(struct bu_vls *msgs, size_t ac, const char **argv, const struct bu_opt_desc *ds);

/** Parse with an explicit scanning policy.  Unused arguments are moved to the
 * beginning of argv and their count is returned, as for bu_opt_parse. */
BU_EXPORT extern int bu_opt_parse_with_policy(struct bu_vls *msgs, size_t ac,
	const char **argv, const struct bu_opt_desc *ds,
	bu_opt_parse_policy_t policy);


/**
 * Value classes inferred from standard bu_opt argument processors.  These are
 * intentionally independent of the command-schema implementation: option
 * users can request lightweight validation and completion without adopting a
 * second option-description API.
 */
typedef enum {
    BU_OPT_VALUE_UNKNOWN = 0,
    BU_OPT_VALUE_FLAG,
    BU_OPT_VALUE_BOOL,
    BU_OPT_VALUE_INTEGER,
    BU_OPT_VALUE_LONG,
    BU_OPT_VALUE_HEX_LONG,
    BU_OPT_VALUE_INCREMENT,
    BU_OPT_VALUE_NUMBER,
    BU_OPT_VALUE_CHAR,
    BU_OPT_VALUE_STRING,
    BU_OPT_VALUE_VLS,
    BU_OPT_VALUE_COLOR,
    BU_OPT_VALUE_VECTOR,
    BU_OPT_VALUE_LANGUAGE,
    BU_OPT_VALUE_MAN_SECTION
} bu_opt_value_t;


typedef enum {
    BU_OPT_VALIDATE_UNKNOWN = 0,
    BU_OPT_VALIDATE_VALID,
    BU_OPT_VALIDATE_INVALID,
    BU_OPT_VALIDATE_INCOMPLETE
} bu_opt_validate_state_t;


typedef enum {
    BU_OPT_EXPECT_NONE = 0,
    BU_OPT_EXPECT_OPTION = 1,
    BU_OPT_EXPECT_OPTION_ARG = 2
} bu_opt_expected_t;


struct bu_opt_validate_result;
struct bu_opt_desc_opts;


/**
 * Optional side-effect-free validation/completion for one option argument.
 * The callback receives the full option argv and cursor position so values
 * may depend on sibling options.  @p context is supplied by the command-level
 * caller; @p data comes from the matching bu_opt_value_spec.  A callback may
 * refine the initialized result and allocate a NULL-terminated candidate
 * array with bu_calloc/bu_strdup.  It must never alter command state.
 * Returning nonzero rejects the validation and discards every result field,
 * including candidates allocated by the callback.
 */
typedef int (*bu_opt_value_validate_t)(const struct bu_opt_desc *option,
	size_t argc, const char **argv, size_t cursor_arg, void *context,
	void *data, struct bu_opt_validate_result *result);

/**
 * Select how many following argv words belong to a variable-width option
 * value.  @p available has already been limited to max_args.  Returning a
 * value greater than @p available rejects the selection.  The callback must
 * be side-effect free.
 */
typedef size_t (*bu_opt_arg_count_t)(size_t available, const char **argv);


/**
 * Optional metadata for a bu_opt_desc row.  @p option names the row by its
 * long spelling when available and otherwise by its short spelling, without
 * leading dashes.  @p alias_of names another row's canonical spelling when a
 * separate short or long row is an alias; most rows leave it NULL.  A NULL
 * option terminates the array.  Zero min/max values retain the cardinality
 * inferred from the standard argument processor.
 */
struct bu_opt_value_spec {
    const char *option;
    const char *alias_of;
    bu_opt_value_t value_type;
    size_t min_args;
    size_t max_args;
    bu_opt_arg_count_t arg_count;
    const char *hint;
    const char * const *candidates;
    bu_opt_value_validate_t validate;
    void *data;
};


struct bu_opt_validate_result {
    bu_opt_validate_state_t state;
    size_t token_start;
    size_t token_end;
    unsigned int expected;
    bu_opt_value_t value_type;
    const char *hint;
    const struct bu_opt_desc *option;
    const char *option_name;
    size_t completion_count;
    const char **completion_candidates;
};


/**
 * Reentrant option-table builder.  A NULL @p descs asks only for the number of
 * non-terminal rows.  Otherwise @p capacity includes room for the terminal
 * BU_OPT_DESC_NULL row and @p storage is the invocation-local argument record
 * whose fields are assigned to set_var by ordinary BU_OPT calls.  Passing a
 * NULL storage pointer builds an unbound table for help and completion.
 */
typedef size_t (*bu_opt_desc_builder_t)(struct bu_opt_desc *descs,
	size_t capacity, void *storage);

/** Reentrant builder for commands that have no options. */
BU_EXPORT extern size_t bu_opt_desc_empty_builder(struct bu_opt_desc *descs,
	size_t capacity, void *storage);


/**
 * Define a file-local reentrant builder from an initializer-row macro.  The
 * row macro is invoked with a pointer named by its argument and must expand
 * to ordinary comma-terminated bu_opt_desc initializer rows.  For example:
 *
 * @code
 * #define APP_OPTIONS(a) \
 *     BU_OPT_INT(a, "n", "number", number, "#", "Number"),
 * BU_OPT_DESC_BUILDER(app_options, struct app_args, APP_OPTIONS);
 * @endcode
 *
 * The generated builder is file-local, derives its row count from the array,
 * and binds no storage when called for description or completion metadata.
 */
#define BU_OPT_DESC_BUILDER_JOIN_(_a, _b) _a##_b
#define BU_OPT_DESC_BUILDER_JOIN(_a, _b) BU_OPT_DESC_BUILDER_JOIN_(_a, _b)
#define BU_OPT_DESC_BUILDER(_name, _record_type, _rows) \
    static size_t _name(struct bu_opt_desc *descs, size_t capacity, void *storage) \
    { \
	_record_type *args = (_record_type *)storage; \
	struct bu_opt_desc local[] = { \
	    _rows(args) \
	    BU_OPT_DESC_NULL \
	}; \
	return bu_opt_desc_copy(descs, capacity, local); \
    } \
    enum { BU_OPT_DESC_BUILDER_JOIN(_name, _builder_defined) = 1 }

/* Concise field-binding rows for use inside a BU_OPT_DESC_BUILDER row list.
 * Custom readers retain the ordinary six-field form, or use BU_OPT_CUSTOM. */
#define BU_OPT_FLAG(_a, _short, _long, _field, _help) \
    {_short, _long, "", NULL, _a ? &(_a)->_field : NULL, _help}
#define BU_OPT_STR(_a, _short, _long, _field, _arg, _help) \
    {_short, _long, _arg, bu_opt_str, _a ? &(_a)->_field : NULL, _help}
#define BU_OPT_INT(_a, _short, _long, _field, _arg, _help) \
    {_short, _long, _arg, bu_opt_int, _a ? &(_a)->_field : NULL, _help}
#define BU_OPT_LONG(_a, _short, _long, _field, _arg, _help) \
    {_short, _long, _arg, bu_opt_long, _a ? &(_a)->_field : NULL, _help}
#define BU_OPT_HEX_LONG(_a, _short, _long, _field, _arg, _help) \
    {_short, _long, _arg, bu_opt_long_hex, _a ? &(_a)->_field : NULL, _help}
#define BU_OPT_CHAR(_a, _short, _long, _field, _arg, _help) \
    {_short, _long, _arg, bu_opt_char, _a ? &(_a)->_field : NULL, _help}
#define BU_OPT_NUM(_a, _short, _long, _field, _arg, _help) \
    {_short, _long, _arg, bu_opt_fastf_t, _a ? &(_a)->_field : NULL, _help}
#define BU_OPT_VLS(_a, _short, _long, _field, _arg, _help) \
    {_short, _long, _arg, bu_opt_vls, _a ? &(_a)->_field : NULL, _help}
#define BU_OPT_BOOL(_a, _short, _long, _field, _arg, _help) \
    {_short, _long, _arg, bu_opt_bool, _a ? &(_a)->_field : NULL, _help}
#define BU_OPT_COLOR(_a, _short, _long, _field, _arg, _help) \
    {_short, _long, _arg, bu_opt_color, _a ? &(_a)->_field : NULL, _help}
#define BU_OPT_VEC(_a, _short, _long, _field, _arg, _help) \
    {_short, _long, _arg, bu_opt_vect_t, _a ? &(_a)->_field : NULL, _help}
#define BU_OPT_INC(_a, _short, _long, _field, _help) \
    {_short, _long, "", bu_opt_incr_long, _a ? &(_a)->_field : NULL, _help}
#define BU_OPT_LANG(_a, _short, _long, _field, _arg, _help) \
    {_short, _long, _arg, bu_opt_lang, _a ? &(_a)->_field : NULL, _help}
#define BU_OPT_SECTION(_a, _short, _long, _field, _arg, _help) \
    {_short, _long, _arg, bu_opt_man_section, _a ? &(_a)->_field : NULL, _help}
#define BU_OPT_CUSTOM(_a, _short, _long, _field, _arg, _reader, _help) \
    {_short, _long, _arg, _reader, _a ? &(_a)->_field : NULL, _help}

#define BU_OPT_VALUE_SPEC_NULL {NULL, NULL, BU_OPT_VALUE_UNKNOWN, 0, 0, NULL, NULL, NULL, NULL, NULL}
/** Describe an opaque reader's scalar type while retaining inferred arity. */
#define BU_OPT_VALUE_TYPE(_option, _type, _hint) \
    {_option, NULL, _type, 0, 0, NULL, _hint, NULL, NULL, NULL}
/** Describe an opaque reader and publish its fixed completion candidates. */
#define BU_OPT_VALUE_CANDIDATES(_option, _type, _hint, _candidates) \
    {_option, NULL, _type, 0, 0, NULL, _hint, _candidates, NULL, NULL}
/** Describe an opaque reader with side-effect-free value validation. */
#define BU_OPT_VALUE_VALIDATE(_option, _type, _hint, _validate, _data) \
    {_option, NULL, _type, 0, 0, NULL, _hint, NULL, _validate, _data}
/** Describe a fixed-width opaque reader. */
#define BU_OPT_VALUE_CARDINALITY(_option, _type, _min, _max, _hint) \
    {_option, NULL, _type, _min, _max, NULL, _hint, NULL, NULL, NULL}
/** Describe a variable-width opaque reader whose callback selects its argv span. */
#define BU_OPT_VALUE_SELECT(_option, _type, _min, _max, _select, _hint) \
    {_option, NULL, _type, _min, _max, _select, _hint, NULL, NULL, NULL}
#define BU_OPT_VALUE_ALIAS(_option, _canonical) \
    {_option, _canonical, BU_OPT_VALUE_UNKNOWN, 0, 0, NULL, NULL, NULL, NULL, NULL}
#define BU_OPT_VALUES(_name, ...) \
    static const struct bu_opt_value_spec _name[] = { __VA_ARGS__ BU_OPT_VALUE_SPEC_NULL }
#define BU_OPT_VALIDATE_RESULT_NULL {BU_OPT_VALIDATE_UNKNOWN, 0, 0, BU_OPT_EXPECT_NONE, BU_OPT_VALUE_UNKNOWN, NULL, NULL, NULL, 0, NULL}


/** Return the value class implied by a standard bu_opt argument processor. */
BU_EXPORT extern bu_opt_value_t bu_opt_desc_value_type(const struct bu_opt_desc *desc);

/** Return canonical value candidates known for a standard reader, if any. */
BU_EXPORT extern const char * const *bu_opt_desc_candidates(const struct bu_opt_desc *desc);

/** Map a compact bu_opt value class to its command-schema representation. */
BU_EXPORT extern bu_cmd_value_t bu_opt_cmd_type(bu_opt_value_t type);

/**
 * Owned command-schema option metadata derived from a bu_opt declaration.
 * This adapter is useful to registries and other consumers that need the
 * richer, read-only command representation without duplicating bu_opt's
 * reader, cardinality, candidate, and alias inference.  Descriptor and
 * sidecar strings remain borrowed; the option and argument-shape arrays are
 * owned by this object.
 */
struct bu_opt_cmd {
    size_t option_count;
    struct bu_cmd_option *options;
    struct bu_cmd_arg_shape *shapes;
};

#define BU_OPT_CMD_INIT_ZERO {0, NULL, NULL}

/**
 * Build command-schema option metadata from a terminated bu_opt table.
 * Initialize @p cmd with BU_OPT_CMD_INIT_ZERO before the first call.  On
 * success, release its owned arrays with bu_opt_cmd_clear().  On failure,
 * @p cmd is unchanged and remains safe to clear.
 */
BU_EXPORT extern int bu_opt_cmd_create(struct bu_opt_cmd *cmd,
	const struct bu_opt_desc *descs, const struct bu_opt_value_spec *specs);

/**
 * Resolve alias chains and inherit all canonical argument semantics.  This
 * may be called again after a host decorates canonical options with richer
 * semantic metadata.  Alias spelling, help, and storage identity are kept.
 */
BU_EXPORT extern int bu_opt_cmd_aliases(struct bu_opt_cmd *cmd);

/**
 * Release arrays owned by a bu_opt command adapter and restore its
 * BU_OPT_CMD_INIT_ZERO state.  @p cmd must have been zero-initialized or
 * produced by bu_opt_cmd_create().
 */
BU_EXPORT extern void bu_opt_cmd_clear(struct bu_opt_cmd *cmd);


/**
 * Incrementally validate options and produce option/value completions.  This
 * operation never invokes bu_opt_desc::arg_process.  Standard bu_opt handlers
 * are recognized directly; an unrecognized custom handler is treated as an
 * opaque value unless a sidecar supplies more information.  @p result must be
 * initialized with BU_OPT_VALIDATE_RESULT_NULL or
 * bu_opt_validate_result_init().  Each call replaces its previous contents;
 * success supplies the new result and failure leaves it initialized and empty.
 */
BU_EXPORT extern int bu_opt_desc_validate(const struct bu_opt_desc *descs,
	const struct bu_opt_value_spec *specs, size_t argc, const char **argv,
	size_t cursor_arg, void *context, struct bu_opt_validate_result *result);


/** Initialize or release a lightweight option-validation result.  Clear may
 * be called repeatedly, but must not receive uninitialized storage. */
BU_EXPORT extern void bu_opt_validate_result_init(struct bu_opt_validate_result *result);
BU_EXPORT extern void bu_opt_validate_result_clear(struct bu_opt_validate_result *result);


/** Build a transient option table.  The caller releases it with bu_free. */
BU_EXPORT extern struct bu_opt_desc *bu_opt_desc_build(bu_opt_desc_builder_t builder,
	void *storage, size_t *count);

/**
 * Copy a terminated local option table into builder output.  This is the
 * usual final statement of a bu_opt_desc_builder_t implementation.  A NULL
 * @p output returns the number of non-terminal rows; otherwise @p capacity
 * must also accommodate the terminal row.
 */
BU_EXPORT extern size_t bu_opt_desc_copy(struct bu_opt_desc *output,
	size_t capacity, const struct bu_opt_desc *local);

/** Parse, describe, or validate the table emitted by a reentrant builder. */
BU_EXPORT extern int bu_opt_parse_build(struct bu_vls *msgs, size_t argc,
	const char **argv, bu_opt_desc_builder_t builder, void *storage);
BU_EXPORT extern int bu_opt_parse_build_with_policy(struct bu_vls *msgs,
	size_t argc, const char **argv, bu_opt_desc_builder_t builder,
	void *storage, bu_opt_parse_policy_t policy);
BU_EXPORT extern char *bu_opt_describe_build(bu_opt_desc_builder_t builder,
	struct bu_opt_desc_opts *settings);
/** Build a standard one-line synopsis for an option-only declaration.  The
 * caller supplies the program or command spelling and any positional operand
 * synopsis because bu_opt_desc intentionally describes options only.  The
 * returned string begins with "Usage:" and ends with a newline. */
BU_EXPORT extern char *bu_opt_usage(const struct bu_opt_desc *descs,
	const char *invocation, const char *operands);
/** Build standard user-facing help from a bu_opt declaration.  The result
 * contains the generated synopsis, optional summary, and an Options section.
 * The caller owns the returned string. */
BU_EXPORT extern char *bu_opt_help(const struct bu_opt_desc *descs,
	const char *invocation, const char *operands, const char *summary);
/** Reentrant-builder form of bu_opt_help. */
BU_EXPORT extern char *bu_opt_help_build(bu_opt_desc_builder_t builder,
	const char *invocation, const char *operands, const char *summary);
BU_EXPORT extern int bu_opt_validate_build(bu_opt_desc_builder_t builder,
	const struct bu_opt_value_spec *specs, size_t argc, const char **argv,
	size_t cursor_arg, void *context, struct bu_opt_validate_result *result);


/** Output format options for bu_opt documentation generation */
typedef enum {
    BU_OPT_ASCII,
    BU_OPT_DOCBOOK /* TODO */
} bu_opt_format_t;


/**
 * Construct a textual description of the options defined by the
 * array.
 *
 * The structure is as follows:
 *
 * Offset    Options      Descriptions
 * ******--------------*********************
 *       --test-option This is a test option
 *
 * Opt_col specifies how wide the options column is, and desc_cols
 * specifies how wide the description column is.
 *
 * This structure is currently experimental and likely will change as
 * we find out what is needed.
 */

/* TODO - support actually using the struct... */
struct bu_opt_desc_opts {
    bu_opt_format_t format;
    int offset;
    int option_columns;
    int description_columns;
    /* The application needs to inform the printer if certain options
     * have special status */
    struct bu_opt_desc *required;
    struct bu_opt_desc *repeated;
    struct bu_opt_desc *optional;
    /* Report the longopt version(s) of an option even when it has a
     * shortopt */
    int show_all_longopts;
    /* It may not be desirable to print all options.  The caller may
     * supply a space separated list of options to accept or reject.
     * Only one list may be supplied at a time.  Filtering is either
     * accept or reject, not both at once.*/
    const char *accept;
    const char *reject;
};


/**
 * initialize an bu_opt_desc_opts struct.
 *
 * Out of the box, assume an overall column width of 80 characters.
 * Given that width, we do a default partitioning.  The first three
 * numbers tell the option printer what column breakout to use for
 * various components of the lines:
 *
 * offset = 2 is the default column offsetting from the left edge
 * option_columns = The next 28 columns are for printing the option
 * and its aliases description_columns = The remaining 50 columns are
 * for human readable explanations of the option
 *
 * These values were chosen after some casual
 * experimentation/observation to see what "looked right" for Linux
 * command line option printing - if better values (perhaps based on
 * some OS convention or standard) are available, it would be better
 * to use those and document their source.
 */
#define BU_OPT_DESC_OPTS_INIT_ZERO { BU_OPT_ASCII, 2, 28, 50, NULL, NULL, NULL, 1, NULL, NULL }

/**
 *
 * Using the example definition:
 *
 * @code
 * struct bu_opt_desc opt_defs[] = {
 *     {"h", "help",    "",  NULL,            &ph, "Print help string and exit."},
 *     {"n", "num",     "#", &bu_opt_int,     &i,  "Read int"},
 *     {"f", "fastf_t", "#", &bu_opt_fastf_t, &f,  "Read float"},
 *     BU_OPT_DESC_NULL
 * };
 * @endcode
 *
 * bu_opt_describe would generate the following help string by
 * default:
 *
 @verbatim
 -h, --help                    Print help string and exit.
 -n #, --num #                 Read int
 -f #, --fastf_t #             Read float
 @endverbatim
 *
 * When multiple options use the same set_var to capture their effect,
 * they are considered aliases for documentation purposes.  For
 * example, if we add multiple aliases to the help option and make it
 * more elaborate:
 *
 * @code
 * #define help_str "Print help and exit. If a type is specified to --help, print help specific to that type"
 * struct help_struct hs;
 * struct bu_opt_desc opt_defs[] = {
 *     {"h", "help",    "[type]",  &hfun, &hs, help_str},
 *     {"H", "HELP",    "[type]",  &hfun, &hs, help_str},
 *     {"?", "",        "[type]",  &hfun, &hs, help_str},
 *     {"n", "num",     "#", &bu_opt_int,     &i,  "Read int"},
 *     {"f", "fastf_t", "#", &bu_opt_fastf_t, &f,  "Read float"},
 *     BU_OPT_DESC_NULL
 * };
 * @endcode
 *
 * the generated help string reflects this:
 *
 @verbatim
 -h [type], -H [type], -? [type], --help [type], --HELP [type]
 Print help and exit. If a type is specified to
 --help, print help specific to that type
 -n #, --num #                 Read int
 -f #, --fastf_t #             Read float
 @endverbatim
 *
 * @returns
 * The generated help string. Note that the string uses allocated
 * memory and it is the responsibility of the caller to free it with
 * @link bu_free @endlink.
 */
BU_EXPORT extern char *bu_opt_describe(const struct bu_opt_desc *ds, struct bu_opt_desc_opts *settings);


/** @} */

/** @addtogroup bu_opt_arg_process
 *
 * Standard option validators.  If a custom option argument validation isn't
 * needed, the functions below can be used for most valid data types.  When
 * data conversion is successful, the user_data pointer in bu_opt_data will
 * point to the results of the string->[type] translation in order to allow a
 * calling program to use the int/long/etc. without having to repeat the
 * conversion.
 *
 * These functions should return -1 if there was a problem processing the
 * value, and the number of argv entries processed otherwise.  (Some validators
 * such as bu_opt_color may read different numbers of args depending on what is
 * found so calling code can't assume a successful validation will always
 * return 1.  Hence -1 is the error return - option validation will never
 * "revert" previously processed argv entries.)
 */
/** @{ */

/**
 * Process 1 argument to set a boolean type
 */
BU_EXPORT extern int bu_opt_bool(struct bu_vls *msg, size_t argc, const char **argv, void *set_var);

/**
 * Process 1 argument to set an integer
 */
BU_EXPORT extern int bu_opt_int(struct bu_vls *msg, size_t argc, const char **argv, void *set_var);

/**
 * Process 1 argument to set a long
 */
BU_EXPORT extern int bu_opt_long(struct bu_vls *msg, size_t argc, const char **argv, void *set_var);
/**
 * Process 1 argument (hex style) to set a long
 */
BU_EXPORT extern int bu_opt_long_hex(struct bu_vls *msg, size_t argc, const char **argv, void *set_var);

/**
 * Process 1 argument to set a @link fastf_t @endlink (either a float
 * or a double, depending on how BRL-CAD was compiled)
 */
BU_EXPORT extern int bu_opt_fastf_t(struct bu_vls *msg, size_t argc, const char **argv, void *set_var);

/**
 * Process 1 argument to set a char
 */
BU_EXPORT extern int bu_opt_char(struct bu_vls *msg, size_t argc, const char **argv, void *set_var);

/**
 * Process 1 argument to set a char pointer (uses the original argv
 * string, does not make a copy)
 */
BU_EXPORT extern int bu_opt_str(struct bu_vls *msg, size_t argc, const char **argv, void *set_var);

/**
 * Process 1 argument to append to a vls (places a space before the
 * new entry if the target vls is not empty)
 */
BU_EXPORT extern int bu_opt_vls(struct bu_vls *msg, size_t argc, const char **argv, void *set_var);

/**
 * Process one packed color or three separate arguments to set a bu_color.
 * Packed integer RGB accepts slash, comma, and semicolon separators.
 */
BU_EXPORT extern int bu_opt_color(struct bu_vls *msg, size_t argc, const char **argv, void *set_var);

/**
 * Process 1 or 3 arguments to set a vect_t
 */
BU_EXPORT extern int bu_opt_vect_t(struct bu_vls *msg, size_t argc, const char **argv, void *set_var);

/** @} */

/**
 * Process 0 arguments, incrementing the value held by a long.  This is
 * useful for situations where multiple specifications of identical options are
 * intended to change output, such as multiple -v options to increase
 * verbosity.
 */
BU_EXPORT extern int bu_opt_incr_long(struct bu_vls *msg, size_t argc, const char **argv, void *set_var);

/**
 * Looking for a string that defines a language per ISO 639-1 language codes.
 */
BU_EXPORT extern int bu_opt_lang(struct bu_vls *msg, size_t argc, const char **argv, void *set_var);

/**
 * Look for a valid man page section identifier (for BRL-CAD purposes valid
 * choices are 1, 3, 5, n)
 */
#define BRLCAD_MAN_SECTIONS {'1', '3', '5', 'n', '\0'}
BU_EXPORT extern int bu_opt_man_section(struct bu_vls *msg, size_t argc, const char **argv, void *set_var);

__END_DECLS

#endif  /* BU_OPT_H */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
