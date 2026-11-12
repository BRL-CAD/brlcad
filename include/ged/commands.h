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
#include "bu/arg.h"
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
    bu_arg_value_t value_type;
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
    bu_arg_value_t completion_type;
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

#define GED_CMD_COMPLETION_RESULT_NULL {0, NULL, 0, 0, NULL, BU_ARG_VALUE_UNKNOWN, BU_ARG_EXPECT_NONE, NULL, NULL, 0, 0, NULL}

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
    bu_arg_validate_state_t state;
    size_t token_start;
    size_t token_end;
    unsigned int expected;
    /** Owned after a successful public validation call and released by
     * ged_cmd_validate_result_clear. */
    const char *hint;
    size_t completion_count;
    const char **completion_candidates;
    bu_arg_value_t completion_type;
    /** Owned after a successful public validation call and released by
     * ged_cmd_validate_result_clear. */
    const char *semantic_provider;
    size_t char_start;
    size_t char_end;
    size_t completion_total;
    int completion_truncated;
    char *completion_common_prefix;
    /** Predicate inherited from the active native option or operand. */
    bu_arg_value_validate_t candidate_validate;
    /** Internal ownership bits for outward-facing borrowed metadata. */
    unsigned int owned_strings;
};

#define GED_CMD_VALIDATE_RESULT_NULL {BU_ARG_VALIDATE_UNKNOWN, 0, 0, BU_ARG_EXPECT_NONE, NULL, 0, NULL, BU_ARG_VALUE_UNKNOWN, NULL, 0, 0, 0, 0, NULL, NULL, 0}

/** Initialize a validation result before first use. */
GED_EXPORT extern void ged_cmd_validate_result_init(struct ged_cmd_validate_result *result);

/** Clear an initialized result; repeated calls are safe. */
GED_EXPORT extern void ged_cmd_validate_result_clear(struct ged_cmd_validate_result *result);

typedef ged_cmd_semantic_state_t (*ged_cmd_semantic_validate_func_t)(struct ged *gedp, bu_arg_value_t type, const char *token, const void *data);
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
    bu_arg_value_t value_type;
    /** Provider capability/contract flags. */
    unsigned int flags;
};

/** complete_query honors the bounded-query contract documented above. */
#define GED_CMD_PROVIDER_BOUNDED_QUERY 0x01u
#define GED_CMD_SEMANTIC_PROVIDER_NULL \
    {NULL, NULL, NULL, NULL, NULL, BU_ARG_VALUE_UNKNOWN, 0}

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
    BU_ARG_OPERAND(_name, BU_ARG_VALUE_DB_OBJECT, _min, _max, _help, \
	GED_CMD_PROVIDER_DB_OBJECT)
#define GED_CMD_OPERAND_DB_PATH(_name, _min, _max, _help) \
    BU_ARG_OPERAND(_name, BU_ARG_VALUE_DB_PATH, _min, _max, _help, \
	GED_CMD_PROVIDER_DB_PATH)
#define GED_CMD_OPERAND_FILE(_name, _min, _max, _help) \
    BU_ARG_OPERAND(_name, BU_ARG_VALUE_FILE, _min, _max, _help, \
	GED_CMD_PROVIDER_FILE_PATH)


/**
 * Parser-owned command grammar adapter.
 *
 * Flat commands publish a bu_arg_schema.  Commands such as search have a
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
 * bu_arg_tree. */
#define GED_CMD_TREE_HELP(_func, _tree) \
    static char *_func(const char *invocation) \
    { return bu_arg_tree_help(&(_tree), invocation); }


struct bu_arg_tree;

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
	const struct bu_arg_schema *schema, const char *input, size_t cursor_pos,
	struct ged_cmd_validate_result *result);
GED_EXPORT int ged_cmd_native_analyze(struct ged *gedp,
	const struct bu_arg_schema *schema, const char *input,
	struct ged_cmd_analysis *analysis);
GED_EXPORT int ged_cmd_tree_validate(struct ged *gedp,
	const struct bu_arg_tree *tree, const char *input, size_t cursor_pos,
	struct ged_cmd_validate_result *result);
GED_EXPORT int ged_cmd_tree_analyze(struct ged *gedp,
	const struct bu_arg_tree *tree, const char *input,
	struct ged_cmd_analysis *analysis);

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
