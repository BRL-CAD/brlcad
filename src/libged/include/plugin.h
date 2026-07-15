/*                        P L U G I N . H
 * BRL-CAD
 *
 * Copyright (c) 2025-2026 United States Government as represented by
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
/** @file plugin.h
 *
 */

#ifndef LIBGED_PLUGIN_H
#define LIBGED_PLUGIN_H

#include "../ged_private.h"
#include "brlcad_version.h"
#include "bu/cmdschema.h"

#define BU_PLUGIN_NAME ged
#define BU_PLUGIN_CMD_RET int
#define BU_PLUGIN_CMD_ARGS struct ged *, int, const char **
#include "../../libbu/bu_plugin.h"

#define GED_API (2*1000000 + (BRLCAD_VERSION_MAJOR*10000) + (BRLCAD_VERSION_MINOR*100) + BRLCAD_VERSION_PATCH)

#define GED_CMD_DEFAULT       0
#define GED_CMD_INTERACTIVE   (1ULL << 0)
#define GED_CMD_UPDATE_SCENE  (1ULL << 1)
#define GED_CMD_UPDATE_VIEW   (1ULL << 2)
#define GED_CMD_AUTOVIEW      (1ULL << 3)
#define GED_CMD_ALL_VIEWS     (1ULL << 4)
#define GED_CMD_VIEW_CALLBACK (1ULL << 5)

struct ged_cmd_impl {
    const char *cname;
    ged_func_ptr cmd;
    unsigned long long opts;
    const struct bu_cmd_schema *native_schema;
    const struct ged_cmd_grammar *grammar;
};

struct ged_cmd_process_impl {
    ged_process_ptr func;
};

struct ged_cmd_schema {
    const char *cname;
    const struct bu_cmd_schema *native_schema;
    const struct ged_cmd_grammar *grammar;
};

#define GED_PLUGIN_SCHEMA_ABI_VERSION 3

struct ged_plugin_schema_manifest {
    const char *plugin_name;
    unsigned int version;
    unsigned int schema_count;
    const struct ged_cmd_schema *schemas;
    unsigned int abi_version;
    size_t struct_size;
};

/* Compact declaration for commands whose syntax is one repeated typed
 * positional role and no options.  This emits the native schema directly;
 * callers supply BU_CMD_VALUE_* and BU_CMD_COUNT_UNLIMITED values rather
 * than retired descriptor-metadata types. */
#define GED_DEFINE_TYPED_OPERAND_SCHEMA(id, cmdstr, helpstr, role, valtype, mincnt, maxcnt, rolehelp) \
    static const struct bu_cmd_operand id##_schema_operands[] = { \
	BU_CMD_OPERAND(role, valtype, mincnt, maxcnt, rolehelp, NULL), \
	BU_CMD_OPERAND_NULL \
    }; \
    static const struct bu_cmd_schema id##_cmd_schema = { \
	cmdstr, helpstr, NULL, id##_schema_operands, \
	BU_CMD_PARSE_STOP_AT_FIRST_OPERAND, {NULL} \
    }

/* Validator helper for parsers accepting a small set of exact positional
 * counts (for example, query with one object or set with object + XYZ). */
#define GED_SCHEMA_COUNT_NONE ((size_t)-2)
#define GED_DEFINE_NATIVE_DISCRETE_COUNT_VALIDATOR(id, count1, count2, count3) \
    static int id##_schema_validate(const struct bu_cmd_schema *cmd, size_t argc, \
	const char **argv, size_t cursor_arg, struct bu_cmd_validate_result *result) \
    { \
	struct bu_cmd_schema flat = *cmd; \
	flat.parse_policy = BU_CMD_PARSE_STOP_AT_FIRST_OPERAND; \
	flat.validation.custom_validate = NULL; \
	int ret = bu_cmd_schema_validate(&flat, argc, argv, cursor_arg, result); \
	if (ret || result->state == BU_CMD_VALIDATE_INVALID || cursor_arg < argc) \
	    return ret; \
	if (argc != (size_t)(count1) && argc != (size_t)(count2) && argc != (size_t)(count3)) { \
	    bu_cmd_value_t ctype = result->completion_type; \
	    bu_cmd_validate_result_clear(result); \
	    result->state = BU_CMD_VALIDATE_INCOMPLETE; \
	    result->token_start = argc; \
	    result->token_end = argc; \
	    result->expected = BU_CMD_EXPECT_OPERAND; \
	    result->completion_type = ctype; \
	    result->hint = "additional operands required"; \
	} \
	return 0; \
    }

#ifdef __cplusplus
extern "C" {
#endif

    /* Registry API: ensure all callers have prototypes in scope */
    int ged_register_command(const struct ged_cmd *cmd);
    int ged_command_exists(const char *name);
    size_t ged_registered_count(void);
    void ged_list_command_names(struct bu_vls *out_csv);
    void ged_list_command_array(const char * const **cl, size_t *cnt);
    int ged_register_command_native_schema(const char *name, const struct bu_cmd_schema *schema);
    int ged_register_command_grammar(const char *name, const struct ged_cmd_grammar *grammar);
    void ged_ensure_initialized(void);
    void libged_shutdown(void);

#ifdef __cplusplus
}
#endif

#if defined(LIBGED_STATIC_CORE) && !defined(GED_PLUGIN_ONLY)

/* We only need two things:
 *  - the canonical struct ged_cmd object (so there is a stable data symbol)
 *  - a stable anchor pointer __ged_cmd_ptr_<cmd> the scanner can reference
 * No constructors, no linker sections, no CRT $XCU.
 */
#if defined(__GNUC__) || defined(__clang__)
#define GED_CMD_USED __attribute__((used))
#else
#define GED_CMD_USED
#endif

#ifdef __cplusplus
#define REGISTER_GED_COMMAND(cmd_symbol)                                              \
    GED_CMD_USED const struct ged_cmd cmd_symbol##_rcmd = { &cmd_symbol##_impl };     \
    extern "C" GED_CMD_USED const struct ged_cmd * const                              \
        __ged_cmd_ptr_##cmd_symbol = &cmd_symbol##_rcmd
#else
#define REGISTER_GED_COMMAND(cmd_symbol)                                              \
    GED_CMD_USED const struct ged_cmd cmd_symbol##_rcmd = { &cmd_symbol##_impl };     \
    GED_CMD_USED const struct ged_cmd * const                                         \
        __ged_cmd_ptr_##cmd_symbol = &cmd_symbol##_rcmd
#endif

/* If we have odd characters in command names (for example the ? command) we
 * can't use REGISTER_GED_COMMAND.  In those cases we need to construct the
 * REGISTER_GED_COMMAND structures manually using legal names.  However, we
 * also still need the scanner to pick up the command - to achieve this, we add
 * a no-op marker to identify commands for the scanner in such cases.
 *
 * See the ? command in the help command's code for a worked example.
 *
 * Argument MUST be the sanitized C identifier (e.g., questionmark, threeptarb)
 * rather than the cmd string.
 */
#define LABEL_GED_COMMAND(cmd_symbol)

#else
#define REGISTER_GED_COMMAND(cmd_symbol) /* static registration disabled */
#endif /* LIBGED_STATIC_CORE && !GED_PLUGIN_ONLY */


/* =======================================================================================
 * Canonical command list helpers (token-based entries)
 *
 * Background / Goals
 * ------------------
 * LIBGED supports two deployment models:
 *
 *   1) Static-core (built into libged):
 *      - A scanner-generated file (ged_force_static_registration) calls
 *        ged_register_command(...) on a set of symbols.
 *      - For this to work reliably across platforms and linkers, each TU must
 *        expose stable symbols that the scanner can reference and that the
 *        linker will not discard.
 *
 *   2) Dynamic plugins (shared libraries):
 *      - A plugin exports a bu_plugin_manifest (and its bu_plugin_cmd[] table)
 *        which the host locates via dlsym/GetProcAddress and loads at runtime.
 *
 * This macro system lets you define a command list ONCE per TU and reuse it to
 * drive both mechanisms with minimal duplication.
 *
 *
 * Command List Definition
 * -----------------------
 * Define a list macro in a TU with the signature:
 *
 *   #define GED_<group>_COMMANDS(X, XID) \
 *       X(token, fn, opts) \
 *       XID(symbol, "cmdname", fn, opts) \
 *       ...
 *
 * and then invoke:
 *
 *   GED_DECLARE_COMMAND_SET(GED_<group>_COMMANDS)
 *   GED_DECLARE_PLUGIN_MANIFEST("libged_<group>", 1, GED_<group>_COMMANDS)
 *
 * GED_DECLARE_COMMAND_SET expands to:
 *   - metadata declarations always
 *   - static registration anchors only when LIBGED_STATIC_CORE is enabled and
 *     GED_PLUGIN_ONLY is NOT defined
 *
 * GED_DECLARE_PLUGIN_MANIFEST only emits plugin export code when GED_PLUGIN is
 * defined.
 *
 *
 * Entry Forms
 * -----------
 * 1) X(token, fn, opts)
 *    - token: C identifier representing the command string (e.g., bot_split)
 *    - fn:    ged_func_ptr-compatible function pointer (implementation)
 *    - opts:  GED_CMD_* flags
 *
 *    Derived names:
 *      command string:  "token"
 *      base symbol:     token##_cmd
 *      impl variable:   token##_cmd_impl
 *
 *    Example:
 *      #define GED_EXAMPLE_COMMANDS(X, XID) \
 *          X(bot, ged_bot_core, GED_CMD_DEFAULT)
 *
 *    Expands (conceptually) to:
 *      struct ged_cmd_impl bot_cmd_impl = { "bot", ged_bot_core, GED_CMD_DEFAULT, NULL, NULL };
 *      REGISTER_GED_COMMAND(bot_cmd);   // static-core only
 *
 * 2) XID(symbol, "cmdname", fn, opts)
 *    Use when the command string cannot be expressed as a simple token or you
 *    need an explicit symbol name.
 *
 *    Example (odd characters):
 *      #define GED_HELP_COMMANDS(X, XID) \
 *          XID(questionmark_cmd, "?", ged_help_core, GED_CMD_DEFAULT)
 *
 *    Expands (conceptually) to:
 *      struct ged_cmd_impl questionmark_cmd_impl = { "?", ged_help_core, GED_CMD_DEFAULT, NULL, NULL };
 *      REGISTER_GED_COMMAND(questionmark_cmd);  // static-core only
 *
 *
 * When to use alternative styles
 * ------------------------------
 * A) Aliases / multiple names for the same implementation
 *    Prefer multiple X/XID entries mapping to the same function:
 *
 *      #define GED_ALIAS_COMMANDS(X, XID) \
 *          X(foo, ged_foo_core, GED_CMD_DEFAULT) \
 *          X(foo_legacy, ged_foo_core, GED_CMD_DEFAULT) \
 *          XID(foo_dash_cmd, "foo-legacy", ged_foo_core, GED_CMD_DEFAULT)
 *
 * B) Configuration-dependent command sets
 *    If some commands only exist under certain compile-time flags, you can gate
 *    entries inside the list macro with #ifdef, or use helper macros that expand
 *    to either an entry or nothing.
 *
 * C) Full manual fallback (legacy style)
 *    If a TU cannot cleanly express its registration as a (name, fn, opts) list,
 *    you may manually write:
 *      - struct ged_cmd_impl <sym>_impl definitions
 *      - REGISTER_GED_COMMAND(<sym>) anchors (static-core)
 *      - bu_plugin_cmd[] + bu_plugin_manifest export (plugin)
 *
 * ======================================================================================= */

/* Stringize helper */
#define GED_STR_IMPL(x) #x
#define GED_STR(x) GED_STR_IMPL(x)

/* Token concatenation helper */
#define GED_CAT2_IMPL(a,b) a##b
#define GED_CAT2(a,b) GED_CAT2_IMPL(a,b)

/* token -> token_cmd */
#define GED_CMD_SYM(token) GED_CAT2(token,_cmd)

/* Wrapper to force expansion before REGISTER_GED_COMMAND applies ## internally */
#define GED_REGISTER_SYM(sym) REGISTER_GED_COMMAND(sym)

/* ---------------------------------------------------------------------------------------
 * 1) Metadata emission (always)
 *
 * These macros ONLY define the struct ged_cmd_impl objects.  They do not create
 * any registration anchors.  This separation keeps the command description
 * stable and reusable across different build modes.
 * --------------------------------------------------------------------------------------- */
#define GED__DECL_META_X(token, fn, opts)                                            \
    struct ged_cmd_impl GED_CAT2(GED_CMD_SYM(token), _impl) = { GED_STR(token), fn, opts, NULL, NULL };

#define GED__DECL_META_XID(sym, cmdstr, fn, opts)                                    \
    struct ged_cmd_impl GED_CAT2(sym, _impl) = { cmdstr, fn, opts, NULL, NULL };

#define GED__IGNORE_SCHEMA_X(token, fn, opts, schema)
#define GED__IGNORE_SCHEMA_XID(sym, cmdstr, fn, opts, schema)

#define GED__DECL_META_NATIVE_SCHEMA_X(token, fn, opts, schema)                      \
    struct ged_cmd_impl GED_CAT2(GED_CMD_SYM(token), _impl) = { GED_STR(token), fn, opts, schema, NULL };

#define GED__DECL_META_NATIVE_SCHEMA_XID(sym, cmdstr, fn, opts, schema)              \
    struct ged_cmd_impl GED_CAT2(sym, _impl) = { cmdstr, fn, opts, schema, NULL };

#define GED__DECL_META_GRAMMAR_X(token, fn, opts, grammar)                            \
    struct ged_cmd_impl GED_CAT2(GED_CMD_SYM(token), _impl) = { GED_STR(token), fn, opts, NULL, grammar };

#define GED__DECL_META_GRAMMAR_XID(sym, cmdstr, fn, opts, grammar)                    \
    struct ged_cmd_impl GED_CAT2(sym, _impl) = { cmdstr, fn, opts, NULL, grammar };

#define GED_DECLARE_COMMAND_METADATA(LIST_MACRO)                                     \
    LIST_MACRO(GED__DECL_META_X, GED__DECL_META_XID)

#define GED_DECLARE_COMMAND_METADATA_WITH_NATIVE_SCHEMA(LIST_MACRO)                  \
    LIST_MACRO(GED__DECL_META_NATIVE_SCHEMA_X, GED__DECL_META_NATIVE_SCHEMA_XID)

#define GED_DECLARE_COMMAND_METADATA_WITH_GRAMMAR(LIST_MACRO)                        \
    LIST_MACRO(GED__DECL_META_GRAMMAR_X, GED__DECL_META_GRAMMAR_XID)

/* A mixed list takes six macro parameters.  Its first pair is retained only
 * for list source compatibility and must be unused; native-schema and grammar
 * entries use the second and third pairs respectively. */
#define GED_DECLARE_COMMAND_METADATA_WITH_MIXED_SCHEMA(LIST_MACRO)                   \
	LIST_MACRO(GED__IGNORE_SCHEMA_X, GED__IGNORE_SCHEMA_XID,                         \
	GED__DECL_META_NATIVE_SCHEMA_X, GED__DECL_META_NATIVE_SCHEMA_XID,             \
	GED__DECL_META_GRAMMAR_X, GED__DECL_META_GRAMMAR_XID)

/* ---------------------------------------------------------------------------------------
 * 2) Static registration anchors (only when using LIBGED static-core)
 *
 * These macros create the stable symbols consumed by the scanner-generated
 * ged_force_static_registration() implementation.
 *
 * In static-core mode, REGISTER_GED_COMMAND(sym) creates:
 *   - sym##_rcmd : canonical struct ged_cmd wrapper
 *   - __ged_cmd_ptr_##sym : stable anchor pointer that the scanner references
 *
 * The anchors depend on having sym##_impl already defined (by metadata emission).
 * --------------------------------------------------------------------------------------- */
#define GED__DECL_STATIC_ANCHOR_X(token, fn, opts)                                   \
    GED_REGISTER_SYM(GED_CMD_SYM(token));

#define GED__DECL_STATIC_ANCHOR_XID(sym, cmdstr, fn, opts)                           \
    GED_REGISTER_SYM(sym);

#define GED__DECL_STATIC_ANCHOR_NATIVE_SCHEMA_X(token, fn, opts, schema)             \
    GED_REGISTER_SYM(GED_CMD_SYM(token));

#define GED__DECL_STATIC_ANCHOR_NATIVE_SCHEMA_XID(sym, cmdstr, fn, opts, schema)     \
    GED_REGISTER_SYM(sym);

#define GED__DECL_STATIC_ANCHOR_GRAMMAR_X(token, fn, opts, grammar)                   \
    GED_REGISTER_SYM(GED_CMD_SYM(token));

#define GED__DECL_STATIC_ANCHOR_GRAMMAR_XID(sym, cmdstr, fn, opts, grammar)           \
    GED_REGISTER_SYM(sym);

#if defined(LIBGED_STATIC_CORE) && !defined(GED_PLUGIN_ONLY)
#  define GED_DECLARE_STATIC_REGISTRATION(LIST_MACRO)                                \
      LIST_MACRO(GED__DECL_STATIC_ANCHOR_X, GED__DECL_STATIC_ANCHOR_XID)
#  define GED_DECLARE_STATIC_REGISTRATION_WITH_NATIVE_SCHEMA(LIST_MACRO)             \
      LIST_MACRO(GED__DECL_STATIC_ANCHOR_NATIVE_SCHEMA_X, GED__DECL_STATIC_ANCHOR_NATIVE_SCHEMA_XID)
#  define GED_DECLARE_STATIC_REGISTRATION_WITH_GRAMMAR(LIST_MACRO)                   \
      LIST_MACRO(GED__DECL_STATIC_ANCHOR_GRAMMAR_X, GED__DECL_STATIC_ANCHOR_GRAMMAR_XID)
#  define GED_DECLARE_STATIC_REGISTRATION_WITH_MIXED_SCHEMA(LIST_MACRO)              \
	  LIST_MACRO(GED__IGNORE_SCHEMA_X, GED__IGNORE_SCHEMA_XID,                      \
	  GED__DECL_STATIC_ANCHOR_NATIVE_SCHEMA_X, GED__DECL_STATIC_ANCHOR_NATIVE_SCHEMA_XID, \
	  GED__DECL_STATIC_ANCHOR_GRAMMAR_X, GED__DECL_STATIC_ANCHOR_GRAMMAR_XID)
#else
/* File-scope safe no-op */
#  define GED_DECLARE_STATIC_REGISTRATION(LIST_MACRO)                                \
      /* static registration disabled */
#  define GED_DECLARE_STATIC_REGISTRATION_WITH_NATIVE_SCHEMA(LIST_MACRO)             \
      /* static registration disabled */
#  define GED_DECLARE_STATIC_REGISTRATION_WITH_GRAMMAR(LIST_MACRO)                   \
      /* static registration disabled */
#  define GED_DECLARE_STATIC_REGISTRATION_WITH_MIXED_SCHEMA(LIST_MACRO)              \
      /* static registration disabled */
#endif

/* ---------------------------------------------------------------------------------------
 * 3) Convenience: declare a command set (metadata + optional static anchors)
 * --------------------------------------------------------------------------------------- */
#define GED_DECLARE_COMMAND_SET(LIST_MACRO)                                          \
    GED_DECLARE_COMMAND_METADATA(LIST_MACRO)                                         \
    GED_DECLARE_STATIC_REGISTRATION(LIST_MACRO)

#define GED_DECLARE_COMMAND_SET_WITH_NATIVE_SCHEMA(LIST_MACRO)                       \
    GED_DECLARE_COMMAND_METADATA_WITH_NATIVE_SCHEMA(LIST_MACRO)                      \
    GED_DECLARE_STATIC_REGISTRATION_WITH_NATIVE_SCHEMA(LIST_MACRO)

#define GED_DECLARE_COMMAND_SET_WITH_GRAMMAR(LIST_MACRO)                             \
    GED_DECLARE_COMMAND_METADATA_WITH_GRAMMAR(LIST_MACRO)                            \
    GED_DECLARE_STATIC_REGISTRATION_WITH_GRAMMAR(LIST_MACRO)

#define GED_DECLARE_COMMAND_SET_WITH_MIXED_SCHEMA(LIST_MACRO)                        \
    GED_DECLARE_COMMAND_METADATA_WITH_MIXED_SCHEMA(LIST_MACRO)                       \
    GED_DECLARE_STATIC_REGISTRATION_WITH_MIXED_SCHEMA(LIST_MACRO)

/* ---------------------------------------------------------------------------------------
 * 4) Plugin manifest export (only when building a plugin)
 * --------------------------------------------------------------------------------------- */
#define GED__DECL_PCMD_X(token, fn, opts) { GED_STR(token), fn },
#define GED__DECL_PCMD_XID(sym, cmdstr, fn, opts) { cmdstr, fn },
#define GED__IGNORE_PCMD_SCHEMA_X(token, fn, opts, schema)
#define GED__IGNORE_PCMD_SCHEMA_XID(sym, cmdstr, fn, opts, schema)
#define GED__DECL_PCMD_NATIVE_SCHEMA_X(token, fn, opts, schema) { GED_STR(token), fn },
#define GED__DECL_PCMD_NATIVE_SCHEMA_XID(sym, cmdstr, fn, opts, schema) { cmdstr, fn },
#define GED__DECL_NATIVE_SCHEMA_X(token, fn, opts, schema) { GED_STR(token), schema, NULL },
#define GED__DECL_NATIVE_SCHEMA_XID(sym, cmdstr, fn, opts, schema) { cmdstr, schema, NULL },
#define GED__DECL_PCMD_GRAMMAR_X(token, fn, opts, grammar) { GED_STR(token), fn },
#define GED__DECL_PCMD_GRAMMAR_XID(sym, cmdstr, fn, opts, grammar) { cmdstr, fn },
#define GED__DECL_GRAMMAR_X(token, fn, opts, grammar) { GED_STR(token), NULL, grammar },
#define GED__DECL_GRAMMAR_XID(sym, cmdstr, fn, opts, grammar) { cmdstr, NULL, grammar },

#ifdef __cplusplus
#  define GED_SCHEMA_EXTERN_C extern "C"
#else
#  define GED_SCHEMA_EXTERN_C
#endif

#ifdef GED_PLUGIN
#  define GED_DECLARE_PLUGIN_MANIFEST(plugin_name_str, plugin_version_u32, LIST_MACRO) \
    static bu_plugin_cmd pcommands[] = {                                               \
        LIST_MACRO(GED__DECL_PCMD_X, GED__DECL_PCMD_XID)                               \
    };                                                                                 \
    static bu_plugin_manifest pinfo = {                                                \
        plugin_name_str,                                                               \
        (unsigned int)(plugin_version_u32),                                            \
        (unsigned int)(sizeof(pcommands)/sizeof(pcommands[0])),                        \
        pcommands,                                                                     \
        BU_PLUGIN_ABI_VERSION,                                                         \
        sizeof(bu_plugin_manifest)                                                     \
    };                                                                                 \
    BU_PLUGIN_DECLARE_MANIFEST(pinfo)
#  define GED_DECLARE_PLUGIN_MANIFEST_WITH_NATIVE_SCHEMA(plugin_name_str, plugin_version_u32, LIST_MACRO) \
    static bu_plugin_cmd pcommands[] = {                                               \
        LIST_MACRO(GED__DECL_PCMD_NATIVE_SCHEMA_X, GED__DECL_PCMD_NATIVE_SCHEMA_XID)   \
    };                                                                                 \
    static const struct ged_cmd_schema pschemas[] = {                                  \
        LIST_MACRO(GED__DECL_NATIVE_SCHEMA_X, GED__DECL_NATIVE_SCHEMA_XID)             \
    };                                                                                 \
    static bu_plugin_manifest pinfo = {                                                \
        plugin_name_str,                                                               \
        (unsigned int)(plugin_version_u32),                                            \
        (unsigned int)(sizeof(pcommands)/sizeof(pcommands[0])),                        \
        pcommands,                                                                     \
        BU_PLUGIN_ABI_VERSION,                                                         \
        sizeof(bu_plugin_manifest)                                                     \
    };                                                                                 \
    static const struct ged_plugin_schema_manifest pschema_info = {                    \
        plugin_name_str,                                                               \
        (unsigned int)(plugin_version_u32),                                            \
        (unsigned int)(sizeof(pschemas)/sizeof(pschemas[0])),                          \
        pschemas,                                                                      \
        GED_PLUGIN_SCHEMA_ABI_VERSION,                                                 \
        sizeof(struct ged_plugin_schema_manifest)                                      \
    };                                                                                 \
    BU_PLUGIN_DECLARE_MANIFEST(pinfo)                                                  \
    GED_SCHEMA_EXTERN_C BU_PLUGIN_EXPORT const struct ged_plugin_schema_manifest *ged_plugin_schema_info(void) { \
        return &pschema_info;                                                          \
    }
#  define GED_DECLARE_PLUGIN_MANIFEST_WITH_GRAMMAR(plugin_name_str, plugin_version_u32, LIST_MACRO) \
    static bu_plugin_cmd pcommands[] = {                                               \
        LIST_MACRO(GED__DECL_PCMD_GRAMMAR_X, GED__DECL_PCMD_GRAMMAR_XID)               \
    };                                                                                 \
    static const struct ged_cmd_schema pschemas[] = {                                  \
        LIST_MACRO(GED__DECL_GRAMMAR_X, GED__DECL_GRAMMAR_XID)                         \
    };                                                                                 \
    static bu_plugin_manifest pinfo = {                                                \
        plugin_name_str,                                                               \
        (unsigned int)(plugin_version_u32),                                            \
        (unsigned int)(sizeof(pcommands)/sizeof(pcommands[0])),                        \
        pcommands,                                                                     \
        BU_PLUGIN_ABI_VERSION,                                                         \
        sizeof(bu_plugin_manifest)                                                     \
    };                                                                                 \
    static const struct ged_plugin_schema_manifest pschema_info = {                    \
        plugin_name_str,                                                               \
        (unsigned int)(plugin_version_u32),                                            \
        (unsigned int)(sizeof(pschemas)/sizeof(pschemas[0])),                          \
        pschemas,                                                                      \
        GED_PLUGIN_SCHEMA_ABI_VERSION,                                                 \
        sizeof(struct ged_plugin_schema_manifest)                                      \
    };                                                                                 \
    BU_PLUGIN_DECLARE_MANIFEST(pinfo)                                                  \
    GED_SCHEMA_EXTERN_C BU_PLUGIN_EXPORT const struct ged_plugin_schema_manifest *ged_plugin_schema_info(void) { \
        return &pschema_info;                                                          \
    }
#  define GED_DECLARE_PLUGIN_MANIFEST_WITH_MIXED_SCHEMA(plugin_name_str, plugin_version_u32, LIST_MACRO) \
    static bu_plugin_cmd pcommands[] = {                                               \
	LIST_MACRO(GED__IGNORE_PCMD_SCHEMA_X, GED__IGNORE_PCMD_SCHEMA_XID,             \
	    GED__DECL_PCMD_NATIVE_SCHEMA_X, GED__DECL_PCMD_NATIVE_SCHEMA_XID,         \
	    GED__DECL_PCMD_GRAMMAR_X, GED__DECL_PCMD_GRAMMAR_XID)                       \
    };                                                                                 \
    static const struct ged_cmd_schema pschemas[] = {                                  \
	LIST_MACRO(GED__IGNORE_SCHEMA_X, GED__IGNORE_SCHEMA_XID,                        \
	    GED__DECL_NATIVE_SCHEMA_X, GED__DECL_NATIVE_SCHEMA_XID,                     \
	    GED__DECL_GRAMMAR_X, GED__DECL_GRAMMAR_XID)                                 \
    };                                                                                 \
    static bu_plugin_manifest pinfo = {                                                \
        plugin_name_str,                                                               \
        (unsigned int)(plugin_version_u32),                                            \
        (unsigned int)(sizeof(pcommands)/sizeof(pcommands[0])),                        \
        pcommands,                                                                     \
        BU_PLUGIN_ABI_VERSION,                                                         \
        sizeof(bu_plugin_manifest)                                                     \
    };                                                                                 \
    static const struct ged_plugin_schema_manifest pschema_info = {                    \
        plugin_name_str,                                                               \
        (unsigned int)(plugin_version_u32),                                            \
        (unsigned int)(sizeof(pschemas)/sizeof(pschemas[0])),                          \
        pschemas,                                                                      \
        GED_PLUGIN_SCHEMA_ABI_VERSION,                                                 \
        sizeof(struct ged_plugin_schema_manifest)                                      \
    };                                                                                 \
    BU_PLUGIN_DECLARE_MANIFEST(pinfo)                                                  \
    GED_SCHEMA_EXTERN_C BU_PLUGIN_EXPORT const struct ged_plugin_schema_manifest *ged_plugin_schema_info(void) { \
        return &pschema_info;                                                          \
    }
#else
/* File-scope safe no-op (no variables, no statements) */
#  define GED_DECLARE_PLUGIN_MANIFEST(plugin_name_str, plugin_version_u32, LIST_MACRO) \
      /* not building plugin */
#  define GED_DECLARE_PLUGIN_MANIFEST_WITH_NATIVE_SCHEMA(plugin_name_str, plugin_version_u32, LIST_MACRO) \
      /* not building plugin */
#  define GED_DECLARE_PLUGIN_MANIFEST_WITH_GRAMMAR(plugin_name_str, plugin_version_u32, LIST_MACRO) \
      /* not building plugin */
#  define GED_DECLARE_PLUGIN_MANIFEST_WITH_MIXED_SCHEMA(plugin_name_str, plugin_version_u32, LIST_MACRO) \
      /* not building plugin */
#endif

#endif /* LIBGED_PLUGIN_H */

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8 cino=N-s
 */
