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
    const char *validator;
    const char *hint;
};

struct ged_cmd_analysis {
    size_t token_count;
    struct ged_cmd_analysis_token *tokens;
};

struct ged_cmd_completion_result {
    size_t completion_count;
    const char **completion_candidates;
    size_t replacement_start;
    size_t replacement_end;
    char *prefix;
    bu_cmd_value_t completion_type;
    unsigned int expected;
    const char *hint;
    char *active_command_path;
    char *active_role;
    int options_legal;
    int starts_new_phase;
};

#define GED_CMD_COMPLETION_RESULT_NULL {0, NULL, 0, 0, NULL, BU_CMD_VALUE_UNKNOWN, BU_CMD_EXPECT_NONE, NULL, NULL, NULL, 0, 0}

/*
 * GED enriches the native result with source-character spans.  Native schemas
 * work in argv indexes; editors need the precise byte range to redraw.
 */
struct ged_cmd_validate_result {
    bu_cmd_validate_state_t state;
    size_t token_start;
    size_t token_end;
    unsigned int expected;
    const char *hint;
    size_t completion_count;
    const char **completion_candidates;
    bu_cmd_value_t completion_type;
    const char *semantic_provider;
    size_t char_start;
    size_t char_end;
};

#define GED_CMD_VALIDATE_RESULT_NULL {BU_CMD_VALIDATE_UNKNOWN, 0, 0, BU_CMD_EXPECT_NONE, NULL, 0, NULL, BU_CMD_VALUE_UNKNOWN, NULL, 0, 0}

GED_EXPORT extern void ged_cmd_validate_result_clear(struct ged_cmd_validate_result *result);

typedef ged_cmd_semantic_state_t (*ged_cmd_semantic_validate_func_t)(struct ged *gedp, bu_cmd_value_t type, const char *token, void *data);
typedef int (*ged_cmd_semantic_complete_func_t)(struct ged *gedp, const char *seed, struct ged_cmd_validate_result *result, void *data);

struct ged_cmd_semantic_provider {
    const char *name;
    ged_cmd_semantic_validate_func_t validate;
    ged_cmd_semantic_complete_func_t complete;
    void *data;
};


/**
 * Parser-owned command grammar adapter.
 *
 * Flat commands publish a bu_cmd_schema.  Commands such as search have a
 * non-flat language whose parser is owned elsewhere, so they register this
 * adapter instead.  The execution callback remains the ordinary ged command
 * implementation; these hooks supply the same cursor-aware validation,
 * token analysis, JSON description, and lint contract consumed by frontends.
 */
typedef int (*ged_cmd_grammar_validate_func_t)(struct ged *gedp, const char *input,
	size_t cursor_pos, struct ged_cmd_validate_result *result);
typedef int (*ged_cmd_grammar_analyze_func_t)(struct ged *gedp, const char *input,
	struct ged_cmd_analysis *analysis);
typedef char *(*ged_cmd_grammar_json_func_t)(void);
typedef int (*ged_cmd_grammar_lint_func_t)(struct bu_vls *msgs);

struct ged_cmd_grammar {
    const char *name;
    const char *help;
    ged_cmd_grammar_validate_func_t validate;
    ged_cmd_grammar_analyze_func_t analyze;
    ged_cmd_grammar_json_func_t describe_json;
    ged_cmd_grammar_lint_func_t lint;
};

struct bu_cmd_tree;

/**
 * Validate or analyze a native command tree from a grammar adapter.
 *
 * These adapters honor each level's native option phase, canonical child
 * names and aliases, and child schema.  They are exported so dynamically
 * loaded GED command plugins can publish native trees without duplicating the
 * editor-facing parser.
 */
GED_EXPORT int ged_cmd_tree_validate(struct ged *gedp,
	const struct bu_cmd_tree *tree, const char *input, size_t cursor_pos,
	struct ged_cmd_validate_result *result);
GED_EXPORT int ged_cmd_tree_analyze(struct ged *gedp,
	const struct bu_cmd_tree *tree, const char *input,
	struct ged_cmd_analysis *analysis);

/** @addtogroup ged_plugins */
/** @{ */
/** Execute plugin based command */
#if !defined(GED_PLUGIN) && !defined(GED_EXEC_NORAW)
GED_EXPORT extern int ged_exec(struct ged *gedp, int argc, const char *argv[]);
#endif
/** @} */

/* LIBGED maintains this list - callers should regard it as read only.  This
 * list will change (size and pointers to individual command strings if
 * commands are added or removed - caller is responsible for performing a new
 * call to get an updated list and size if commands are altered.  */
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

/* Report whether a GED command has schema metadata available. */
GED_EXPORT extern int
ged_cmd_schema_exists(const char *cmd);

/* Return JSON schema metadata for a GED command.  Caller must free with bu_free. */
GED_EXPORT extern char *
ged_cmd_schema_json(const char *cmd);

/* Register a named semantic provider for command schemas. */
GED_EXPORT extern int
ged_cmd_semantic_provider_register(const struct ged_cmd_semantic_provider *provider);

/* Report whether a named semantic provider is available. */
GED_EXPORT extern int
ged_cmd_semantic_provider_exists(const char *name);

/* Lint one command schema, or all registered schemas if cmd is NULL. */
GED_EXPORT extern int
ged_cmd_schema_lint(const char *cmd, struct bu_vls *msgs);

/* Validate a GED command line against available schema metadata. */
GED_EXPORT extern int
ged_cmd_validate(struct ged *gedp, const char *input, size_t cursor_pos, struct ged_cmd_validate_result *result);

/* Complete the token at cursor_pos using schema metadata and existing fallbacks. */
GED_EXPORT extern int
ged_cmd_complete(const char ***completions, struct bu_vls *prefix, struct ged *gedp, const char *input, size_t cursor_pos);

/* Complete the token at cursor_pos, including the original-input range to replace. */
GED_EXPORT extern int
ged_cmd_complete_result(struct ged *gedp, const char *input, size_t cursor_pos, struct ged_cmd_completion_result *result);

/* Free data owned by a ged_cmd_completion_result. */
GED_EXPORT extern void
ged_cmd_completion_result_clear(struct ged_cmd_completion_result *result);

/* Analyze all command-line tokens for structured highlighting and diagnostics. */
GED_EXPORT extern int
ged_cmd_analyze(struct ged *gedp, const char *input, struct ged_cmd_analysis *analysis);

/* Free data owned by a ged_cmd_analysis result. */
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
