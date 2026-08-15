/*                       C O M M A N D S . H
 * BRL-CAD
 *
 * Copyright (c) 2008-2026 United States Government as represented by
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
/** @addtogroup libged
 *
 * Geometry EDiting Library Commands
 *
 */
/** @{ */
/** @file ged/commands.h */
/** @} */

#ifndef GED_COMMANDS_H
#define GED_COMMANDS_H

#include "common.h"
#include "bu/cmdschema.h"
#include "bu/opt.h"
#include "ged/defines.h"


__BEGIN_DECLS

typedef enum {
    GED_CMD_TOKEN_UNKNOWN = 0,
    GED_CMD_TOKEN_COMMAND,
    GED_CMD_TOKEN_SUBCOMMAND,
    GED_CMD_TOKEN_OPTION,
    GED_CMD_TOKEN_OPTION_ARG,
    GED_CMD_TOKEN_OPERAND
} ged_cmd_token_role_t;

typedef enum {
    GED_CMD_SEMANTIC_UNKNOWN = 0,
    GED_CMD_SEMANTIC_VALID,
    GED_CMD_SEMANTIC_INVALID,
    GED_CMD_SEMANTIC_INCOMPLETE,
    GED_CMD_SEMANTIC_PENDING
} ged_cmd_semantic_state_t;

struct ged_cmd_analysis_token {
    size_t token_start;
    size_t token_end;
    size_t char_start;
    size_t char_end;
    ged_cmd_token_role_t role;
    bu_cmd_value_t value_type;
    ged_cmd_semantic_state_t semantic_state;
    /** Strings owned by the enclosing analysis result after a successful
     * public analysis call; do not release them individually. */
    const char *validator;
    const char *hint;
};

struct ged_cmd_analysis {
    size_t token_count;
    struct ged_cmd_analysis_token *tokens;
    /** Internal result-owned string block.  Initialize with
     * GED_CMD_ANALYSIS_NULL, do not inspect or release directly, and release
     * the complete result with ged_cmd_analysis_clear. */
    void *owned_storage;
};

#define GED_CMD_ANALYSIS_NULL {0, NULL, 0}

struct ged_cmd_completion_result {
    /** Number of materialized entries in completion_candidates. */
    size_t completion_count;
    /** Owned, NULL-terminated values in the provider's token grammar;
     * release with ged_cmd_completion_result_clear. */
    const char **completion_candidates;
    /** Half-open byte range in the original input to replace. */
    size_t replacement_start;
    size_t replacement_end;
    /** Decoded prefix used to select completion_candidates. */
    char *prefix;
    bu_cmd_value_t completion_type;
    unsigned int expected;
    /** Owned explanatory text; release with ged_cmd_completion_result_clear. */
    char *hint;
    /** Optional owned command/subcommand path, requested with WANT_CONTEXT. */
    char *active_command_path;
    /** Full-set metadata, independent of the materialized candidate budget. */
    size_t total_count;
    int truncated;
    char *common_prefix;
};

#define GED_CMD_COMPLETION_RESULT_NULL {0, NULL, 0, 0, NULL, BU_CMD_VALUE_UNKNOWN, BU_CMD_EXPECT_NONE, NULL, NULL, 0, 0, NULL}

/** Optional provider-side filter for one candidate.  Return zero to accept
 * the candidate.  Query providers must apply this before max_candidates so
 * total-count, truncation, and common-prefix metadata describe the filtered
 * set. */
typedef int (*ged_cmd_completion_filter_t)(const char *candidate,
	const void *data);

/** Resource limits and cursor location for one completion query.  A zero
 * max_candidates requests the complete candidate set.  candidate_filter and
 * its data are populated by GED when an active schema value has an additional
 * scalar predicate; callers normally leave them NULL.  argc, argv, and
 * cursor_arg are likewise GED-populated, read-only command context available
 * to providers whose candidate scope depends on earlier arguments. */
struct ged_cmd_completion_request {
    size_t cursor_pos;
    size_t max_candidates;
    unsigned int flags;
    ged_cmd_completion_filter_t candidate_filter;
    const void *candidate_filter_data;
    size_t argc;
    const char * const *argv;
    size_t cursor_arg;
};

#define GED_CMD_COMPLETION_REQUEST_NULL {0, 0, 0, NULL, NULL, 0, NULL, 0}
#define GED_CMD_COMPLETION_WANT_CONTEXT 0x01u

/*
 * GED enriches the native result with source-character spans.  Native schemas
 * work in argv indexes; editors need the precise byte range to redraw.
 */
struct ged_cmd_validate_result {
    bu_cmd_validate_state_t state;
    size_t token_start;
    size_t token_end;
    unsigned int expected;
    /** Owned after a successful public validation call and released by
     * ged_cmd_validate_result_clear. */
    const char *hint;
    size_t completion_count;
    const char **completion_candidates;
    bu_cmd_value_t completion_type;
    /** Owned after a successful public validation call and released by
     * ged_cmd_validate_result_clear. */
    const char *semantic_provider;
    size_t char_start;
    size_t char_end;
    size_t completion_total;
    int completion_truncated;
    char *completion_common_prefix;
    /** Predicate inherited from the active native option or operand. */
    bu_cmd_value_validate_t candidate_validate;
    /** Internal ownership bits for outward-facing borrowed metadata. */
    unsigned int owned_strings;
};

#define GED_CMD_VALIDATE_RESULT_NULL {BU_CMD_VALIDATE_UNKNOWN, 0, 0, BU_CMD_EXPECT_NONE, NULL, 0, NULL, BU_CMD_VALUE_UNKNOWN, NULL, 0, 0, 0, 0, NULL, NULL, 0}

/** Initialize a validation result before first use. */
GED_EXPORT extern void ged_cmd_validate_result_init(struct ged_cmd_validate_result *result);

/** Clear an initialized result; repeated calls are safe. */
GED_EXPORT extern void ged_cmd_validate_result_clear(struct ged_cmd_validate_result *result);

typedef ged_cmd_semantic_state_t (*ged_cmd_semantic_validate_func_t)(struct ged *gedp, bu_cmd_value_t type, const char *token, const void *data);
typedef int (*ged_cmd_semantic_complete_func_t)(struct ged *gedp, const char *seed, struct ged_cmd_validate_result *result, const void *data);
typedef int (*ged_cmd_semantic_complete_query_func_t)(struct ged *gedp, const char *seed,
	const struct ged_cmd_completion_request *request, struct ged_cmd_validate_result *result, const void *data);

/**
 * Named semantic validation/completion service.
 *
 * Completion callbacks receive an initialized, empty result and return a
 * nonnegative value on success or a negative value on failure.  Candidate
 * arrays, candidate strings, and common-prefix strings must be allocated with
 * libbu allocation routines; ownership transfers to libged on return.  The
 * candidate array must have completion_count entries followed by NULL.  Each
 * candidate is a complete replacement for the supplied seed and must begin
 * with that seed.  DB-path providers are the exception: their replacement is
 * the final path component and must begin with the text after the seed's last
 * unescaped slash.
 *
 * A bounded complete_query callback honors request->max_candidates and
 * candidate_filter, reports the exact completion_total, and supplies the
 * common prefix of the full filtered set when that set is truncated.  The
 * registry copies this record and its name, but not data; data and everything
 * it references must remain valid until libged shutdown.
 */
struct ged_cmd_semantic_provider {
    const char *name;
    ged_cmd_semantic_validate_func_t validate;
    ged_cmd_semantic_complete_func_t complete;
    const void *data;
    ged_cmd_semantic_complete_query_func_t complete_query;
    /** Semantic type produced by validation/completion. */
    bu_cmd_value_t value_type;
    /** Provider capability/contract flags. */
    unsigned int flags;
};

/** complete_query honors the bounded-query contract documented above. */
#define GED_CMD_PROVIDER_BOUNDED_QUERY 0x01u
#define GED_CMD_SEMANTIC_PROVIDER_NULL \
    {NULL, NULL, NULL, NULL, NULL, BU_CMD_VALUE_UNKNOWN, 0}

/* Stable names for GED's common built-in semantic providers.  Command schemas
 * should use these symbols rather than duplicating the registry's string
 * keys. */
#define GED_CMD_PROVIDER_DB_OBJECT "ged.db_object"
#define GED_CMD_PROVIDER_DB_OBJECT_ANY "ged.db_object_any"
#define GED_CMD_PROVIDER_DB_OBJECT_ALL "ged.db_object_all"
#define GED_CMD_PROVIDER_DB_OBJECT_BINARY "ged.db_object_binary"
#define GED_CMD_PROVIDER_DB_PATH "ged.db_path"
#define GED_CMD_PROVIDER_DB_PATH_OR_PATTERN "ged.db_path_or_pattern"
#define GED_CMD_PROVIDER_FILE_PATH "ged.file_path"
#define GED_CMD_PROVIDER_FILE_PATH_SVG "ged.file_path.svg"
#define GED_CMD_PROVIDER_COLOR "ged.color"
#define GED_CMD_PROVIDER_VECTOR "ged.vector"
#define GED_CMD_PROVIDER_VECTOR_GROUP "ged.vector_group"
#define GED_CMD_PROVIDER_UNIT "ged.unit"

#define GED_CMD_OPERAND_DB_OBJECT(_name, _min, _max, _help) \
    BU_CMD_OPERAND(_name, BU_CMD_VALUE_DB_OBJECT, _min, _max, _help, \
	GED_CMD_PROVIDER_DB_OBJECT)
#define GED_CMD_OPERAND_DB_PATH(_name, _min, _max, _help) \
    BU_CMD_OPERAND(_name, BU_CMD_VALUE_DB_PATH, _min, _max, _help, \
	GED_CMD_PROVIDER_DB_PATH)
#define GED_CMD_OPERAND_FILE(_name, _min, _max, _help) \
    BU_CMD_OPERAND(_name, BU_CMD_VALUE_FILE, _min, _max, _help, \
	GED_CMD_PROVIDER_FILE_PATH)


/**
 * Parser-owned command grammar adapter.
 *
 * Flat commands publish a bu_cmd_schema.  Commands such as search have a
 * non-flat language whose parser is owned elsewhere, so they register this
 * adapter instead.  The execution callback remains the ordinary ged command
 * implementation; these hooks supply the same cursor-aware validation,
 * token analysis, JSON description, and lint contract consumed by frontends.
 * Validation, analysis, and completion callbacks receive initialized result
 * storage.  A nonzero callback return discards every partial result field;
 * registration borrows this record and everything it references through
 * completion of libged shutdown.
 */
typedef int (*ged_cmd_grammar_validate_func_t)(struct ged *gedp, const char *input,
	size_t cursor_pos, struct ged_cmd_validate_result *result);
typedef int (*ged_cmd_grammar_analyze_func_t)(struct ged *gedp, const char *input,
	struct ged_cmd_analysis *analysis);
typedef char *(*ged_cmd_grammar_json_func_t)(void);
typedef char *(*ged_cmd_grammar_help_func_t)(const char *invocation);
typedef int (*ged_cmd_grammar_lint_func_t)(struct bu_vls *msgs);
typedef int (*ged_cmd_grammar_complete_func_t)(struct ged *gedp,
	const char *input, size_t cursor_pos,
	const struct ged_cmd_completion_request *request,
	struct ged_cmd_validate_result *result);

struct ged_cmd_grammar {
    const char *name;
    const char *help;
    ged_cmd_grammar_validate_func_t validate;
    ged_cmd_grammar_analyze_func_t analyze;
    ged_cmd_grammar_json_func_t describe_json;
    ged_cmd_grammar_lint_func_t lint;
    /** Optional command-context completion after validate has classified the
     * active role.  Implementations honor request->max_candidates and augment
     * the initialized result without repeating structural validation.  A
     * truncated result must report the exact full-set total and an allocated
     * (possibly empty) full-set common prefix. */
    ged_cmd_grammar_complete_func_t complete;
    /** Optional authoritative human-readable description for parser-owned
     * grammars.  The caller releases the returned string with bu_free. */
    ged_cmd_grammar_help_func_t describe_help;
};

/** Define the standard help callback for a grammar backed by one native
 * bu_cmd_tree. */
#define GED_CMD_TREE_HELP(_func, _tree) \
    static char *_func(const char *invocation) \
    { return bu_cmd_tree_help(&(_tree), invocation); }


/** Libged-specific completion semantics for an otherwise ordinary bu_opt
 * value.  This keeps database, view, and application concepts out of libbu. */
struct ged_opt_semantic {
    const char *option;
    bu_cmd_value_t value_type;
    const char *semantic_provider;
    const char *hint;
};

typedef struct ged_opt_semantic ged_opt_semantic;

#define GED_OPT_SEMANTIC_NULL {NULL, BU_CMD_VALUE_UNKNOWN, NULL, NULL}
#define GED_OPT_SEMANTIC(_option, _type, _provider, _hint) \
    {_option, _type, _provider, _hint}

/**
 * One compact option-selected operand form.  option_names is a
 * whitespace-separated list of option spellings resolved to canonical names
 * when the command is registered.  Forms use first-match semantics and must
 * end with one GED_OPT_OTHERWISE row.  syntax uses the ordinary compact
 * operand notation and additionally accepts repeated heterogeneous groups,
 * for example "(attribute:string value:string)+".
 */
struct ged_opt_form {
    const char *name;
    const char *help;
    bu_cmd_constraint_condition_t condition;
    const char *option_names;
    const char *syntax;
};

typedef struct ged_opt_form ged_opt_form;

#define GED_OPT_FORM(_name, _help, _condition, _options, _syntax) \
    {_name, _help, _condition, _options, _syntax}
#define GED_OPT_WHEN(_option, _syntax) \
    GED_OPT_FORM(_option, NULL, BU_CMD_CONDITION_ALL_OPTIONS_PRESENT, _option, _syntax)
#define GED_OPT_WHEN_HELP(_option, _help, _syntax) \
    GED_OPT_FORM(_option, _help, BU_CMD_CONDITION_ALL_OPTIONS_PRESENT, _option, _syntax)
#define GED_OPT_WHEN_ALL(_name, _options, _syntax) \
    GED_OPT_FORM(_name, NULL, BU_CMD_CONDITION_ALL_OPTIONS_PRESENT, _options, _syntax)
#define GED_OPT_WHEN_ANY(_name, _options, _syntax) \
    GED_OPT_FORM(_name, NULL, BU_CMD_CONDITION_ANY_OPTION_PRESENT, _options, _syntax)
#define GED_OPT_UNLESS(_name, _options, _syntax) \
    GED_OPT_FORM(_name, NULL, BU_CMD_CONDITION_NO_OPTION_PRESENT, _options, _syntax)
#define GED_OPT_OTHERWISE(_syntax) \
    GED_OPT_FORM("default", NULL, BU_CMD_CONDITION_ALWAYS, NULL, _syntax)
#define GED_OPT_OTHERWISE_HELP(_help, _syntax) \
    GED_OPT_FORM("default", _help, BU_CMD_CONDITION_ALWAYS, NULL, _syntax)
#define GED_OPT_FORM_NULL {NULL, NULL, BU_CMD_CONDITION_ALWAYS, NULL, NULL}


/** Database completion vocabulary used by compact GED operand policies. */
typedef enum {
    GED_OPT_DB_OBJECT = 0,
    GED_OPT_DB_PATH_OR_PATTERN
} ged_opt_db_kind_t;

typedef enum {
    GED_OPT_DB_GEOMETRY = 0,
    GED_OPT_DB_ANY,
    GED_OPT_DB_ALL,
    GED_OPT_DB_ANY_HIDDEN,
    GED_OPT_DB_PRIMITIVES,
    GED_OPT_DB_COMBINATIONS,
    GED_OPT_DB_REGIONS
} ged_opt_db_scope_t;

struct ged_opt_db_type_case {
    const char *option;
    ged_opt_db_scope_t scope;
};

typedef struct ged_opt_db_type_case ged_opt_db_type_case;

#define GED_OPT_DB_TYPE(_option, _scope) {_option, _scope}
#define GED_OPT_DB_TYPE_NULL {NULL, GED_OPT_DB_GEOMETRY}

/**
 * Refine one named operand's database completion provider.  The first
 * selected type case wins.  modifier_option switches the default to
 * modified_default_scope and adds hidden objects to a selected type scope.
 */
struct ged_opt_db_completion {
    const char *operand;
    ged_opt_db_kind_t kind;
    const struct ged_opt_db_type_case *type_cases;
    ged_opt_db_scope_t default_scope;
    const char *modifier_option;
    ged_opt_db_scope_t modified_default_scope;
    const char *hint;
};

typedef struct ged_opt_db_completion ged_opt_db_completion;

#define GED_OPT_DB_COMPLETION(_operand, _kind, _types, _default, _modifier, _modified, _hint) \
    {_operand, _kind, _types, _default, _modifier, _modified, _hint}
#define GED_OPT_DB_OBJECTS(_operand, _types, _default, _modifier, _modified) \
    GED_OPT_DB_COMPLETION(_operand, GED_OPT_DB_OBJECT, _types, _default, \
	_modifier, _modified, NULL)
#define GED_OPT_DB_PATHS(_operand, _types, _default, _modifier, _modified) \
    GED_OPT_DB_COMPLETION(_operand, GED_OPT_DB_PATH_OR_PATTERN, _types, _default, \
	_modifier, _modified, NULL)
#define GED_OPT_DB_COMPLETION_NULL \
    {NULL, GED_OPT_DB_OBJECT, NULL, GED_OPT_DB_GEOMETRY, NULL, GED_OPT_DB_GEOMETRY, NULL}

/* Normalized compact-command metadata used by the registry.  Production
 * command declarations should normally author GED_RULE_* rows instead; this
 * table form remains visible for registry normalization and compatibility. */
struct ged_opt_meta {
    const struct bu_opt_value_spec *option_values;
    const ged_opt_semantic *option_semantics;
    const struct bu_cmd_constraint *constraints;
    bu_cmd_schema_validate_t validate;
    bu_cmd_schema_context_validate_t context_validate;
    const struct ged_opt_db_completion *db_completions;
};

typedef struct ged_opt_meta ged_opt_meta;

#define GED_OPT_META(_values, _semantics, _constraints, _validate, _context) \
    {_values, _semantics, _constraints, _validate, _context, NULL}
#define GED_OPT_META_DB(_values, _semantics, _constraints, _validate, _context, _db) \
    {_values, _semantics, _constraints, _validate, _context, _db}
#define GED_OPT_DB(_values, _semantics, _constraints, _db) \
    GED_OPT_META_DB(_values, _semantics, _constraints, NULL, NULL, _db)
#define GED_OPT_VALUES(_values) \
    GED_OPT_META(_values, NULL, NULL, NULL, NULL)
#define GED_OPT_SEMANTICS(_values, _semantics) \
    GED_OPT_META(_values, _semantics, NULL, NULL, NULL)
#define GED_OPT_CONSTRAINTS(_constraints) \
    GED_OPT_META(NULL, NULL, _constraints, NULL, NULL)
#define GED_OPT_VALIDATE(_values, _semantics, _constraints, _validate) \
    GED_OPT_META(_values, _semantics, _constraints, _validate, NULL)
#define GED_OPT_CONTEXT(_context) \
    GED_OPT_META(NULL, NULL, NULL, NULL, _context)


/** Common declarative details for one compact GED option specification.
 *
 * Command authors normally supply one terminated array of these rows rather
 * than constructing separate value, semantic, form, constraint, and database
 * policy tables.  Registration expands the rows once into the ordinary owned
 * command schema.  Option-name strings in value and semantic rows, and the
 * option lists in constraint rows, are whitespace separated; this permits
 * identical metadata such as the "H ?" help aliases to be stated once.
 */
typedef enum {
    GED_OPT_RULE_END = 0,
    GED_OPT_RULE_VALUE,
    GED_OPT_RULE_SEMANTIC,
    GED_OPT_RULE_FORM,
    GED_OPT_RULE_CONSTRAINT,
    GED_OPT_RULE_DB_COMPLETION,
    GED_OPT_RULE_DB_TYPE,
    GED_OPT_RULE_SCHEMA_VALIDATE,
    GED_OPT_RULE_CONTEXT_VALIDATE
} ged_opt_rule_kind_t;

struct ged_opt_constraint_rule {
    bu_cmd_constraint_kind_t kind;
    bu_cmd_constraint_condition_t condition;
    const char *option_names;
    size_t min_count;
    size_t max_count;
    const char *hint;
};

typedef struct ged_opt_constraint_rule ged_opt_constraint_rule;

struct ged_opt_db_type_rule {
    const char *operand;
    const char *option_names;
    ged_opt_db_scope_t scope;
};

typedef struct ged_opt_db_type_rule ged_opt_db_type_rule;

struct ged_opt_rule {
    ged_opt_rule_kind_t kind;
    struct bu_opt_value_spec value;
    ged_opt_semantic semantic;
    ged_opt_form form;
    ged_opt_constraint_rule constraint;
    ged_opt_db_completion db_completion;
    ged_opt_db_type_rule db_type;
    bu_cmd_schema_validate_t validate;
    bu_cmd_schema_context_validate_t context_validate;
};

typedef struct ged_opt_rule ged_opt_rule;

#define GED_OPT_CONSTRAINT_RULE_NULL \
    {BU_CMD_CONSTRAINT_OPTION_COUNT, BU_CMD_CONDITION_ALWAYS, NULL, 0, 0, NULL}
#define GED_OPT_DB_TYPE_RULE_NULL {NULL, NULL, GED_OPT_DB_GEOMETRY}
#define GED_OPT_RULE_EMPTY(_kind) \
    {_kind, BU_OPT_VALUE_SPEC_NULL, GED_OPT_SEMANTIC_NULL, \
	GED_OPT_FORM_NULL, GED_OPT_CONSTRAINT_RULE_NULL, \
	GED_OPT_DB_COMPLETION_NULL, GED_OPT_DB_TYPE_RULE_NULL, NULL, NULL}

#define GED_RULE_VALUE(_value) \
    {GED_OPT_RULE_VALUE, _value, GED_OPT_SEMANTIC_NULL, \
	GED_OPT_FORM_NULL, GED_OPT_CONSTRAINT_RULE_NULL, \
	GED_OPT_DB_COMPLETION_NULL, GED_OPT_DB_TYPE_RULE_NULL, NULL, NULL}
#define GED_RULE_ALIAS(_options, _canonical) \
    GED_RULE_VALUE(BU_OPT_VALUE_ALIAS(_options, _canonical))
#define GED_RULE_TYPE(_options, _type, _hint) \
    GED_RULE_VALUE(BU_OPT_VALUE_TYPE(_options, _type, _hint))
#define GED_RULE_CANDIDATES(_options, _type, _hint, _candidates) \
    GED_RULE_VALUE(BU_OPT_VALUE_CANDIDATES(_options, _type, _hint, _candidates))
#define GED_RULE_VALUE_VALIDATE(_options, _type, _hint, _validate, _data) \
    GED_RULE_VALUE(BU_OPT_VALUE_VALIDATE(_options, _type, _hint, _validate, _data))
#define GED_RULE_CARDINALITY(_options, _type, _min, _max, _hint) \
    GED_RULE_VALUE(BU_OPT_VALUE_CARDINALITY(_options, _type, _min, _max, _hint))
#define GED_RULE_SELECT(_options, _type, _min, _max, _select, _hint) \
    GED_RULE_VALUE(BU_OPT_VALUE_SELECT(_options, _type, _min, _max, _select, _hint))

#define GED_RULE_SEMANTIC(_options, _type, _provider, _hint) \
    {GED_OPT_RULE_SEMANTIC, BU_OPT_VALUE_SPEC_NULL, \
	GED_OPT_SEMANTIC(_options, _type, _provider, _hint), GED_OPT_FORM_NULL, \
	GED_OPT_CONSTRAINT_RULE_NULL, GED_OPT_DB_COMPLETION_NULL, \
	GED_OPT_DB_TYPE_RULE_NULL, NULL, NULL}

#define GED_RULE_FORM(_name, _help, _condition, _options, _syntax) \
    {GED_OPT_RULE_FORM, BU_OPT_VALUE_SPEC_NULL, \
	GED_OPT_SEMANTIC_NULL, GED_OPT_FORM(_name, _help, _condition, _options, _syntax), \
	GED_OPT_CONSTRAINT_RULE_NULL, GED_OPT_DB_COMPLETION_NULL, \
	GED_OPT_DB_TYPE_RULE_NULL, NULL, NULL}
#define GED_RULE_WHEN(_option, _syntax) \
    GED_RULE_FORM(_option, NULL, BU_CMD_CONDITION_ALL_OPTIONS_PRESENT, _option, _syntax)
#define GED_RULE_WHEN_HELP(_option, _help, _syntax) \
    GED_RULE_FORM(_option, _help, BU_CMD_CONDITION_ALL_OPTIONS_PRESENT, _option, _syntax)
#define GED_RULE_WHEN_ALL(_name, _options, _syntax) \
    GED_RULE_FORM(_name, NULL, BU_CMD_CONDITION_ALL_OPTIONS_PRESENT, _options, _syntax)
#define GED_RULE_WHEN_ANY(_name, _options, _syntax) \
    GED_RULE_FORM(_name, NULL, BU_CMD_CONDITION_ANY_OPTION_PRESENT, _options, _syntax)
#define GED_RULE_UNLESS(_name, _options, _syntax) \
    GED_RULE_FORM(_name, NULL, BU_CMD_CONDITION_NO_OPTION_PRESENT, _options, _syntax)
#define GED_RULE_OTHERWISE(_syntax) \
    GED_RULE_FORM("default", NULL, BU_CMD_CONDITION_ALWAYS, NULL, _syntax)
#define GED_RULE_OTHERWISE_HELP(_help, _syntax) \
    GED_RULE_FORM("default", _help, BU_CMD_CONDITION_ALWAYS, NULL, _syntax)

#define GED_OPT_CONSTRAINT_RULE(_kind, _condition, _options, _min, _max, _hint) \
    {_kind, _condition, _options, _min, _max, _hint}
#define GED_RULE_CONSTRAINT(_kind, _condition, _options, _min, _max, _hint) \
    {GED_OPT_RULE_CONSTRAINT, BU_OPT_VALUE_SPEC_NULL, \
	GED_OPT_SEMANTIC_NULL, GED_OPT_FORM_NULL, \
	GED_OPT_CONSTRAINT_RULE(_kind, _condition, _options, _min, _max, _hint), \
	GED_OPT_DB_COMPLETION_NULL, GED_OPT_DB_TYPE_RULE_NULL, NULL, NULL}
#define GED_RULE_OPTIONS_IF(_condition, _options, _min, _max, _hint) \
    GED_RULE_CONSTRAINT(BU_CMD_CONSTRAINT_OPTION_COUNT, _condition, _options, _min, _max, _hint)
#define GED_RULE_OPTIONS(_options, _min, _max, _hint) \
    GED_RULE_OPTIONS_IF(BU_CMD_CONDITION_ALWAYS, _options, _min, _max, _hint)
#define GED_RULE_OPTION_OCCURRENCES(_options, _min, _max, _hint) \
    GED_RULE_CONSTRAINT(BU_CMD_CONSTRAINT_OPTION_OCCURRENCE_COUNT, \
	BU_CMD_CONDITION_ALWAYS, _options, _min, _max, _hint)
#define GED_RULE_OPERANDS(_condition, _options, _min, _max, _hint) \
    GED_RULE_CONSTRAINT(BU_CMD_CONSTRAINT_OPERAND_COUNT, _condition, \
	_options, _min, _max, _hint)
#define GED_RULE_REQUIRES(_options, _hint) \
    GED_RULE_CONSTRAINT(BU_CMD_CONSTRAINT_OPTION_REQUIRES, \
	BU_CMD_CONDITION_ALWAYS, _options, 0, 0, _hint)
#define GED_RULE_CONFLICTS(_options, _hint) \
    GED_RULE_CONSTRAINT(BU_CMD_CONSTRAINT_OPTION_CONFLICTS, \
	BU_CMD_CONDITION_ALWAYS, _options, 0, 0, _hint)

#define GED_RULE_DB_COMPLETION(_operand, _kind, _default, _modifier, _modified, _hint) \
    {GED_OPT_RULE_DB_COMPLETION, BU_OPT_VALUE_SPEC_NULL, \
	GED_OPT_SEMANTIC_NULL, GED_OPT_FORM_NULL, GED_OPT_CONSTRAINT_RULE_NULL, \
	GED_OPT_DB_COMPLETION(_operand, _kind, NULL, _default, _modifier, _modified, _hint), \
	GED_OPT_DB_TYPE_RULE_NULL, NULL, NULL}
#define GED_RULE_DB_OBJECTS(_operand, _default, _modifier, _modified) \
    GED_RULE_DB_COMPLETION(_operand, GED_OPT_DB_OBJECT, _default, _modifier, _modified, NULL)
#define GED_RULE_DB_PATHS(_operand, _default, _modifier, _modified) \
    GED_RULE_DB_COMPLETION(_operand, GED_OPT_DB_PATH_OR_PATTERN, _default, _modifier, _modified, NULL)
#define GED_RULE_DB_TYPE(_operand, _options, _scope) \
    {GED_OPT_RULE_DB_TYPE, BU_OPT_VALUE_SPEC_NULL, \
	GED_OPT_SEMANTIC_NULL, GED_OPT_FORM_NULL, GED_OPT_CONSTRAINT_RULE_NULL, \
	GED_OPT_DB_COMPLETION_NULL, {_operand, _options, _scope}, NULL, NULL}
#define GED_RULE_SCHEMA_VALIDATE(_validate) \
    {GED_OPT_RULE_SCHEMA_VALIDATE, BU_OPT_VALUE_SPEC_NULL, \
	GED_OPT_SEMANTIC_NULL, GED_OPT_FORM_NULL, GED_OPT_CONSTRAINT_RULE_NULL, \
	GED_OPT_DB_COMPLETION_NULL, GED_OPT_DB_TYPE_RULE_NULL, _validate, NULL}
#define GED_RULE_CONTEXT_VALIDATE(_validate) \
    {GED_OPT_RULE_CONTEXT_VALIDATE, BU_OPT_VALUE_SPEC_NULL, \
	GED_OPT_SEMANTIC_NULL, GED_OPT_FORM_NULL, GED_OPT_CONSTRAINT_RULE_NULL, \
	GED_OPT_DB_COMPLETION_NULL, GED_OPT_DB_TYPE_RULE_NULL, NULL, _validate}
#define GED_RULE_NULL GED_OPT_RULE_EMPTY(GED_OPT_RULE_END)


/**
 * Compact description for the common GED command shape: a reentrant bu_opt
 * table plus declarative positional structure.  The registry translates this
 * facade to the internal command-schema engine once, so command authors keep
 * ordinary BU_OPT declarations and execution may use bu_opt_parse_build.
 * GED_OPT_WITH, GED_OPT_FORMS, and GED_OPT_NATIVE accept one GED_RULE_* table
 * and registration expands and owns its normalized arrays.  GED_OPT is the
 * no-extra-rules form.  Strings, callbacks, candidates, and user data named by
 * a rule must remain valid while the command is registered (normally static
 * storage).
 */
struct ged_opt_spec {
    const char *name;
    const char *help;
    bu_opt_desc_builder_t option_builder;
    /** Optional compact positional syntax.  When present, registration parses
     * it once and @p operands must be NULL.  See GED_OPT and GED_OPT_WITH. */
    const char *syntax;
    /** Optional first-match operand forms.  Forms are mutually exclusive with
     * syntax and operands and share this declaration's options and policy. */
    const struct ged_opt_form *forms;
    const struct bu_cmd_operand *operands;
    bu_cmd_parse_policy_t parse_policy;
    const struct bu_cmd_operand_group *operand_groups;
    const struct ged_opt_meta *metadata;
    /** Optional flat authoring rules.  These are mutually exclusive with
     * forms and metadata and are normalized during registration.  The public
     * macros record the array bound, so GED_RULE_NULL is optional and retained
     * only as a visual terminator for existing declarations. */
    const struct ged_opt_rule *rules;
    size_t rule_count;
    const char *source_file;
    int source_line;
};

typedef struct ged_opt_spec ged_opt_spec;

#define GED_OPT_RULE_COUNT(_rules) (sizeof(_rules) / sizeof((_rules)[0]))
#define GED_OPT_RULE_TEXT(_name, _help, _builder, _syntax, _rules, _count) \
    {_name, _help, _builder, _syntax, NULL, NULL, BU_CMD_PARSE_INTERSPERSED, NULL, NULL, _rules, _count, __FILE__, __LINE__}
#define GED_OPT_NATIVE(_name, _help, _builder, _operands, _policy, _rules) \
    {_name, _help, _builder, NULL, NULL, _operands, _policy, NULL, NULL, _rules, GED_OPT_RULE_COUNT(_rules), __FILE__, __LINE__}
#define GED_OPT_FORMS(_name, _help, _builder, _rules) \
    {_name, _help, _builder, NULL, NULL, NULL, BU_CMD_PARSE_INTERSPERSED, NULL, NULL, _rules, GED_OPT_RULE_COUNT(_rules), __FILE__, __LINE__}
#define GED_OPT(_name, _help, _builder, _syntax) \
    GED_OPT_RULE_TEXT(_name, _help, _builder, _syntax, NULL, 0)
#define GED_OPT_WITH(_name, _help, _builder, _syntax, _rules) \
    GED_OPT_RULE_TEXT(_name, _help, _builder, _syntax, _rules, GED_OPT_RULE_COUNT(_rules))

struct bu_cmd_tree;

/**
 * Validate or analyze a native flat schema or command tree from a grammar
 * adapter.
 *
 * These adapters honor each level's native option phase, canonical child
 * names and aliases, and child schema.  They are exported so dynamically
 * loaded GED command plugins can publish native trees without duplicating the
 * editor-facing parser.
 */
GED_EXPORT int ged_cmd_native_validate(struct ged *gedp,
	const struct bu_cmd_schema *schema, const char *input, size_t cursor_pos,
	struct ged_cmd_validate_result *result);
GED_EXPORT int ged_cmd_native_analyze(struct ged *gedp,
	const struct bu_cmd_schema *schema, const char *input,
	struct ged_cmd_analysis *analysis);
GED_EXPORT int ged_cmd_tree_validate(struct ged *gedp,
	const struct bu_cmd_tree *tree, const char *input, size_t cursor_pos,
	struct ged_cmd_validate_result *result);
GED_EXPORT int ged_cmd_tree_analyze(struct ged *gedp,
	const struct bu_cmd_tree *tree, const char *input,
	struct ged_cmd_analysis *analysis);

/** Check a compact option declaration without registering it.  Diagnostics
 * are appended to @p msgs when non-NULL. */
GED_EXPORT int ged_opt_lint(const ged_opt_spec *spec, struct bu_vls *msgs);

/** @addtogroup ged_plugins */
/** @{ */
/** Execute plugin based command */
#if !defined(GED_PLUGIN) && !defined(GED_EXEC_NORAW)
GED_EXPORT extern int ged_exec(struct ged *gedp, int argc, const char *argv[]);
#endif
/** @} */

/* Return a stable snapshot of public command names.  The result contains the
 * returned number of owned strings followed by NULL; release it with
 * bu_argv_free(count, (char **)cmd_list).  Synthetic `_mged_` compatibility
 * aliases are omitted. */
GED_EXPORT size_t ged_cmd_list(const char * const **cmd_list);

/* Report whether a string identifies a valid LIBGED command.
 *
 * Returns 1 if cmd is a valid GED command, else returns 0.
 */
GED_EXPORT int ged_cmd_exists(const char *cmd);

/* Determine whether cmd1 and cmd2 both refer to the same function pointer
 * (i.e., they are aliases for the same command.)
 *
 * Returns 1 if both cmd1 and cmd2 invoke the same LIBGED function, else
 * returns 0
 */
GED_EXPORT int ged_cmd_same(const char *cmd1, const char *cmd2);

/* Report whether a string identifies a valid LIBGED command.  If func is
 * non-NULL, check that cmd and func both refer to the same function pointer
 * (i.e., they are aliases for the same command.)
 *
 * If func is NULL, a 0 return indicates an valid GED command and non-zero
 * indicates an invalid command.
 *
 * If func is non-null:
 * 0 indicates both cmd and func strings invoke the same LIBGED function
 * 1 indicates that either or both of cmd and func were invalid GED commands
 * 2 indicates that both were valid commands, but they did not match.
 *
 * DEPRECATED - use ged_cmd_same and ged_cmd_exists instead.
 */
DEPRECATED GED_EXPORT int ged_cmd_valid(const char *cmd, const char *func);

/* Given a candidate cmd name, find the closest match to it among defined
 * GED commands.  Returns the bu_editdist distance between cmd and *ncmd
 * (0 if they match exactly - i.e. cmd does define a command.)
 *
 * Useful for suggesting corrections to commands which are not found.
 */
/** Find the closest public command spelling.  @p ncmd receives thread-local
 * borrowed storage which remains valid until the next ged_cmd_lookup call on
 * the same thread.  Synthetic `_mged_` compatibility aliases are omitted. */
GED_EXPORT extern int
ged_cmd_lookup(const char **ncmd, const char *cmd);


/* Given a partial command string, analyze it and return possible command
 * completions of the seed string.  Typically this functionality is used to
 * implement "tab completion" or "tab expansion" features in applications.  The
 * possible completions are returned in the completions array, and the returned
 * value is the count of possible completions found.
 *
 * If completions array returned is non-NULL, caller is responsible for freeing
 * it using bu_argv_free.
 */
GED_EXPORT extern int
ged_cmd_completions(const char ***completions, const char *seed);

/* Given a object name or path string, analyze it and return possible
 * object-based completions.  Typically this functionality is used to implement
 * "tab completion" or "tab expansion" features in applications.  The possible
 * completions are returned in the completions array, and the returned value is
 * the count of possible completions found.
 *
 * If completions array returned is non-NULL, caller is responsible for freeing
 * it using bu_argv_free.
 */
GED_EXPORT extern int
ged_geom_completions(const char ***completions, struct bu_vls *cprefix, struct db_i *dbip, const char *seed);

/** Database-directory classes accepted by ged_geom_completions_filtered. */
#define GED_DB_COMPLETION_GEOMETRY     0x01u
#define GED_DB_COMPLETION_NON_GEOMETRY 0x02u
#define GED_DB_COMPLETION_HIDDEN       0x04u
#define GED_DB_COMPLETION_GLOBAL       0x08u
#define GED_DB_COMPLETION_BINARY_ONLY  0x10u
#define GED_DB_COMPLETION_PRIMITIVES   0x20u
#define GED_DB_COMPLETION_COMBINATIONS 0x40u
#define GED_DB_COMPLETION_REGIONS      0x80u
#define GED_DB_COMPLETION_HIDDEN_ONLY  0x100u

/**
 * Filtered form of ged_geom_completions for command-specific schemas and
 * grammar validators.  HIDDEN permits hidden entries from the selected object
 * classes.  HIDDEN_ONLY excludes visible entries and implies HIDDEN.  GLOBAL
 * must be requested explicitly; it is never implied by NON_GEOMETRY, HIDDEN,
 * or HIDDEN_ONLY.  BINARY_ONLY selects binary-uniform objects instead of the
 * GEOMETRY/NON_GEOMETRY classes.  PRIMITIVES, COMBINATIONS, and REGIONS
 * optionally narrow GEOMETRY to the union of those directory types.
 */
GED_EXPORT extern int
ged_geom_completions_filtered(const char ***completions, struct bu_vls *cprefix,
	struct db_i *dbip, const char *seed, unsigned int filters);

/* Report whether a GED command has schema metadata available. */
GED_EXPORT extern int
ged_cmd_schema_exists(const char *cmd);

/* Return JSON schema metadata for a GED command.  Caller must free with bu_free. */
GED_EXPORT extern char *
ged_cmd_schema_json(const char *cmd);

/** Return standard human-readable help generated from a command's native
 * schema.  @p invocation controls the synopsis command spelling and may be
 * NULL to use the schema name.  The caller owns the returned string and must
 * release it with bu_free.  NULL means no native schema is registered. */
GED_EXPORT extern char *
ged_cmd_help(const char *cmd, const char *invocation);

/** Append standard schema-generated help to @p output.  Returns 0 when help
 * was appended and -1 when no command metadata is available or an argument is
 * invalid. */
GED_EXPORT extern int
ged_cmd_help_append(struct bu_vls *output, const char *cmd, const char *invocation);

/* Register a named semantic provider for command schemas.  Names beginning
 * with "ged." are reserved for libged.  Returns 0 on success, 1 for a
 * duplicate name, and -1 for an invalid declaration or reserved name. */
GED_EXPORT extern int
ged_cmd_semantic_provider_register(const struct ged_cmd_semantic_provider *provider);

/* Report whether a named semantic provider is available. */
GED_EXPORT extern int
ged_cmd_semantic_provider_exists(const char *name);

/* Lint one command schema, or all registered schemas if cmd is NULL. */
GED_EXPORT extern int
ged_cmd_schema_lint(const char *cmd, struct bu_vls *msgs);

/* Validate a GED command line and identify its active value role without
 * enumerating semantic candidates.  Use ged_cmd_complete_query to obtain
 * candidate values.  Initialize result before first use.  Success replaces
 * its contents; failure leaves it initialized and empty. */
GED_EXPORT extern int
ged_cmd_validate(struct ged *gedp, const char *input, size_t cursor_pos, struct ged_cmd_validate_result *result);

/* Complete the token at cursor_pos using schema metadata and existing fallbacks. */
GED_EXPORT extern int
ged_cmd_complete(const char ***completions, struct bu_vls *prefix, struct ged *gedp, const char *input, size_t cursor_pos);

/* Complete the token at cursor_pos, including the original-input range to
 * replace.  Initialize result before first use.  Success replaces its
 * contents; failure leaves it initialized and empty. */
GED_EXPORT extern int
ged_cmd_complete_result(struct ged *gedp, const char *input, size_t cursor_pos, struct ged_cmd_completion_result *result);

/* Bounded completion query.  Returns zero on success and -1 on error; the
 * materialized count is result->completion_count.  The result reports the total match count,
 * truncation status, and common prefix even when only the first candidates
 * requested for display are materialized.  Initialize result before first
 * use.  Success replaces its contents; failure leaves it initialized and
 * empty. */
GED_EXPORT extern int
ged_cmd_complete_query(struct ged *gedp, const char *input,
	const struct ged_cmd_completion_request *request, struct ged_cmd_completion_result *result);

/* Initialize a completion result before first use. */
GED_EXPORT extern void
ged_cmd_completion_result_init(struct ged_cmd_completion_result *result);

/* Free data owned by an initialized result; repeated calls are safe. */
GED_EXPORT extern void
ged_cmd_completion_result_clear(struct ged_cmd_completion_result *result);

/* Analyze all command-line tokens for structured highlighting and diagnostics.
 * Initialize the result with GED_CMD_ANALYSIS_NULL or ged_cmd_analysis_init
 * before first use.  A successful call replaces any prior result contents;
 * failure leaves it initialized and empty. */
GED_EXPORT extern int
ged_cmd_analyze(struct ged *gedp, const char *input, struct ged_cmd_analysis *analysis);

/* Initialize an analysis result before first use. */
GED_EXPORT extern void
ged_cmd_analysis_init(struct ged_cmd_analysis *analysis);

/* Free data owned by an initialized analysis result; repeated calls are safe. */
GED_EXPORT extern void
ged_cmd_analysis_clear(struct ged_cmd_analysis *analysis);


/**
 * Use bu_editor to set up an editor for use with GED
 * commands that require launching a text editor.
 *
 * Will first try to respect environment variables (including looking for
 * terminal options to launch text editors normally used only in graphical
 * mode) and then fall back to lookups.
 *
 * Applications may supply their own argv array of editors to check using
 * app_editors_cnt and app_editors in the ged struct - see bu_editor
 * documentation for more details.
 */
GED_EXPORT extern int ged_set_editor(struct ged *gedp, int non_gui);

/**
 * Clear editor data set by ged_set_editor.  User specified app_editors data
 * is left unchanged.
 */
GED_EXPORT extern void ged_clear_editor(struct ged *gedp);


/* defined in track.c */
GED_EXPORT extern int ged_track2(struct bu_vls *log_str, struct rt_wdb *wdbp, const char *argv[]);


/* defined in wdb_importFg4Section.c */
GED_EXPORT int wdb_importFg4Section_cmd(void *data, int argc, const char *argv[]);


/* defined in inside.c */
GED_EXPORT extern int ged_inside_internal(struct ged *gedp,
					  struct rt_db_internal *ip,
					  int argc,
					  const char *argv[],
					  int arg,
					  char *o_name);


GED_EXPORT void draw_scene(struct bv_scene_obj *s, struct bview *v);


/** @} */


__END_DECLS


#endif /* GED_COMMANDS_H */

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
