/*                     G E D _ I N I T . C P P
 * BRL-CAD
 *
 * Copyright (c) 2019-2026 United States Government as represented by
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
/** @file init.c
 *
 * NOTE: as this init is global to ALL applications before main(), care must be
 * taken to not write to STDOUT or STDERR or app output may be corrupted,
 * signals can be raised, or worse.
 *
 * Static constructors (REGISTER_GED_COMMAND) call ged_register_command before libged_init()
 * ordering isn't guaranteed across TUs, so ged_register_command is fully lazy-initializing.
 *
 * libged_init() performs a one-time plugin scan unless GED_NO_PLUGIN_SCAN=1.
 */

#include "common.h"

#include <algorithm>
#include <cctype>
#include <map>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

#include "bu/app.h"
#include "bu/dylib.h"
#include "bu/file.h"
#include "bu/str.h"
#include "bu/vls.h"
#include "ged.h"

#define BU_PLUGIN_IMPLEMENTATION
#include "./include/plugin.h"

static struct bu_vls init_msgs = BU_VLS_INIT_ZERO;

/* Optional: thread safety (add bu_mutex if needed in future) */
static std::map<std::string, const struct bu_cmd_schema *> native_schema_registry;
static std::map<std::string, const struct ged_cmd_grammar *> grammar_registry;

/* -------------------------------------------------------------------------- */
/* Public Registry API                                                        */
/* -------------------------------------------------------------------------- */

extern "C" int
ged_register_command(const struct ged_cmd *cmd)
{
    if (!cmd || !cmd->i || !cmd->i->cname || !cmd->i->cmd) return -1;

    std::string key(cmd->i->cname);

    /* First-wins: ignore duplicate */
    if (bu_plugin_cmd_exists(key.c_str())) {
	return 1;
    }

    /* Register name and historical _mged_ alias in generalized registry */
    (void)bu_plugin_cmd_register(key.c_str(), cmd->i->cmd);
    std::string mged_key = std::string("_mged_") + key;
    (void)bu_plugin_cmd_register(mged_key.c_str(), cmd->i->cmd);

    if (cmd->i->native_schema)
	(void)ged_register_command_native_schema(key.c_str(), cmd->i->native_schema);
    if (cmd->i->grammar)
	(void)ged_register_command_grammar(key.c_str(), cmd->i->grammar);

    return 0;
}

extern "C" int
ged_register_command_native_schema(const char *name, const struct bu_cmd_schema *schema)
{
    if (!name || !schema)
	return -1;

    std::string key(name);
    if (key.empty())
	return -1;

    if (key.compare(0, 6, "_mged_") == 0)
	key.erase(0, 6);

    if (native_schema_registry.find(key) != native_schema_registry.end())
	return 1;

    native_schema_registry[key] = schema;
    return 0;
}

extern "C" int
ged_register_command_grammar(const char *name, const struct ged_cmd_grammar *grammar)
{
    if (!name || !grammar)
	return -1;

    std::string key(name);
    if (key.empty())
	return -1;

    if (key.compare(0, 6, "_mged_") == 0)
	key.erase(0, 6);

    if (grammar_registry.find(key) != grammar_registry.end())
	return 1;

    grammar_registry[key] = grammar;
    return 0;
}

extern "C" int
ged_command_exists(const char *name)
{
    if (!name)
	return 0;

    ged_ensure_initialized();
    return bu_plugin_cmd_exists(name);
}

extern "C" size_t
ged_registered_count(void)
{
    ged_ensure_initialized();
    return bu_plugin_cmd_count();
}

extern "C" void
ged_list_command_names(struct bu_vls *out_csv)
{
    ged_ensure_initialized();
    if (!out_csv) return;
    bu_vls_trunc(out_csv, 0);
    auto cb = [](const char *name, bu_plugin_cmd_impl impl, void *ud) -> int {
	(void)impl;
	struct bu_vls *v = (struct bu_vls *)ud;
	if (!name) return 0;
	if (bu_strncmp(name, "_mged_", 6) == 0) return 0; /* skip synthetic aliases */
	if (bu_vls_strlen(v) > 0) bu_vls_printf(v, ",");
	bu_vls_printf(v, "%s", name);
	return 0;
    };
    /* Iterate sorted names via generalized registry */
    bu_plugin_cmd_foreach(cb, (void *)out_csv);
}

extern "C" void
ged_list_command_array(const char * const **cl, size_t *cnt)
{
    ged_ensure_initialized();
    if (!cl || !cnt) return;
    std::vector<std::string> names;
    auto cb = [](const char *name, bu_plugin_cmd_impl impl, void *ud) -> int {
	(void)impl;
	if (!name) return 0;
	if (bu_strncmp(name, "_mged_", 6) == 0) return 0;
	std::vector<std::string> *v = (std::vector<std::string> *)ud;
	v->push_back(std::string(name));
	return 0;
    };
    bu_plugin_cmd_foreach(cb, (void *)&names);
    char **alist = (char **)bu_calloc(names.size(), sizeof(char *), "ged cmd argv");
    size_t len = 0;
    for (auto &n : names) {
	alist[len++] = bu_strdup(n.c_str());
    }
    *cl = (const char * const *)alist;
    *cnt = len;
}

/* -------------------------------------------------------------------------- */
/* Plugin Loading                                                             */
/* -------------------------------------------------------------------------- */

static void
scan_plugins(void)
{
    const char *env_block = getenv("GED_NO_PLUGIN_SCAN");
    if (env_block && BU_STR_EQUAL(env_block, "1")) {
	return;
    }

    const char *ppath = bu_dir(NULL, 0, BU_DIR_LIBEXEC, "ged", NULL);
    if (!ppath) {
	return;
    }

    struct bu_vls pattern = BU_VLS_INIT_ZERO;
    bu_vls_sprintf(&pattern, "*%s", GED_PLUGIN_SUFFIX);
    char **ged_filenames = NULL;
    size_t ged_nfiles = bu_file_list(ppath, bu_vls_cstr(&pattern), &ged_filenames);

    for (size_t i = 0; i < ged_nfiles; i++) {
	char pfile[MAXPATHLEN] = {0};
	bu_dir(pfile, MAXPATHLEN, BU_DIR_LIBEXEC, "ged", ged_filenames[i], NULL);

	/* Phase 4: load plugins exclusively via generalized loader */
	(void)bu_plugin_load(pfile);

	void *handle = bu_dlopen(pfile, BU_RTLD_LAZY);
	if (handle) {
	    void *info_val = bu_dlsym(handle, "ged_plugin_schema_info");
	    if (info_val) {
		typedef const struct ged_plugin_schema_manifest *(*schema_info_fn)(void);
		schema_info_fn schema_info = (schema_info_fn)(intptr_t)info_val;
		const struct ged_plugin_schema_manifest *manifest = schema_info();
		if (manifest &&
			manifest->struct_size >= sizeof(struct ged_plugin_schema_manifest) &&
			manifest->schema_count < 8192U && manifest->schemas) {
		    if (manifest->abi_version == GED_PLUGIN_SCHEMA_ABI_VERSION) {
			for (unsigned int si = 0; si < manifest->schema_count; si++) {
			    const struct ged_cmd_schema *schema = &manifest->schemas[si];
			    if (schema->cname && schema->native_schema)
				(void)ged_register_command_native_schema(schema->cname, schema->native_schema);
			    if (schema->cname && schema->grammar)
				(void)ged_register_command_grammar(schema->cname, schema->grammar);
			}
		    }
		}
	    }
	    (void)bu_dlclose(handle);
	}
    }

    bu_vls_free(&pattern);
    bu_argv_free(ged_nfiles, ged_filenames);
}

/* -------------------------------------------------------------------------- */
/* Initialization & Shutdown                                                  */
/* -------------------------------------------------------------------------- */
extern "C" void ged_force_static_registration(void);

/* Phase 0: logger to funnel bu_plugin messages into init_msgs (avoid stdout/stderr) */
static void _ged_plugin_logger(int level, const char *msg)
{
    (void)level;
    if (msg) bu_vls_printf(&init_msgs, "%s\n", msg);
}

static void
libged_init(void)
{
    /* Bootstrap generalized registry and set logger */
    bu_plugin_set_logger(_ged_plugin_logger);
    (void)bu_plugin_init();

#if defined(LIBGED_STATIC_CORE)
    ged_force_static_registration();
#endif

    /* At this point, static constructors may or may not have run for all TUs.
     * Any that have will have already populated the registry through ged_register_command.
     * We proceed to scan plugins once to add plugin-only and user-provided commands.
     */
    scan_plugins();
}

static std::once_flag g_init_once;

extern "C" void ged_ensure_initialized()
{
    std::call_once(g_init_once, [](){
        // Safe early init + full init path
        libged_init();  // sets logger, initializes plugin core, does static reg (if enabled) and scans plugins once
    });
}

extern "C" void
libged_shutdown(void)
{
    /* Generalized registry teardown */
    bu_plugin_shutdown();

    native_schema_registry.clear();
    grammar_registry.clear();
    bu_vls_free(&init_msgs);
}

/* Provide existing init message accessor */
extern "C" const char *
ged_init_msgs()
{
    ged_ensure_initialized();
    return bu_vls_cstr(&init_msgs);
}

/* Backwards compatibility wrappers for old APIs (if needed) */

extern "C" int
ged_cmd_exists(const char *cmd)
{
    ged_ensure_initialized();
    return bu_plugin_cmd_exists(cmd);
}

extern "C" int
ged_cmd_schema_exists(const char *cmd)
{
    if (!cmd)
	return 0;

    ged_ensure_initialized();
    return (_ged_cmd_native_schema(cmd) != NULL || _ged_cmd_grammar(cmd) != NULL) ? 1 : 0;
}

extern "C" char *
ged_cmd_schema_json(const char *cmd)
{
    if (!cmd)
	return NULL;

    ged_ensure_initialized();
    const struct ged_cmd_grammar *grammar = _ged_cmd_grammar(cmd);
    if (grammar && grammar->describe_json)
	return grammar->describe_json();
    const struct bu_cmd_schema *native_schema = _ged_cmd_native_schema(cmd);
    if (native_schema)
	return bu_cmd_schema_describe_json(native_schema);
    return NULL;
}

extern "C" const struct bu_cmd_schema *
_ged_cmd_native_schema(const char *cmd)
{
    if (!cmd)
	return NULL;

    const char *lookup = cmd;
    if (bu_strncmp(lookup, "_mged_", 6) == 0)
	lookup += 6;

    auto it = native_schema_registry.find(std::string(lookup));
    if (it == native_schema_registry.end())
	return NULL;

    return it->second;
}

extern "C" const struct ged_cmd_grammar *
_ged_cmd_grammar(const char *cmd)
{
    if (!cmd)
	return NULL;

    const char *lookup = cmd;
    if (bu_strncmp(lookup, "_mged_", 6) == 0)
	lookup += 6;

    auto it = grammar_registry.find(std::string(lookup));
    if (it == grammar_registry.end())
	return NULL;

    return it->second;
}

static int
ged_schema_lint_provider(const char *path, const char *role, const char *provider, struct bu_vls *msgs)
{
    if (BU_STR_EMPTY(provider))
	return 0;
    if (ged_cmd_semantic_provider_exists(provider))
	return 0;
    if (msgs)
	bu_vls_printf(msgs, "%s: unresolved %s semantic provider \"%s\"\n", path, role, provider);
    return 1;
}


static int
ged_native_schema_lint_keyword_values(const char *path, const char *role, const char *name,
	const char * const *keywords, const struct bu_cmd_value_keyword *keyword_values,
	struct bu_vls *msgs)
{
    int failures = 0;
    std::set<std::string> spellings;

    if (keywords && keyword_values) {
	if (msgs)
	    bu_vls_printf(msgs, "%s: native %s \"%s\" declares both simple and rich keyword values\n",
		path, role, name);
	return 1;
    }
    if (keywords) {
	for (size_t i = 0; keywords[i]; i++) {
	    if (BU_STR_EMPTY(keywords[i]) || !spellings.insert(std::string(keywords[i])).second) {
		if (msgs)
		    bu_vls_printf(msgs, "%s: native %s \"%s\" has an empty or duplicate keyword \"%s\"\n",
			path, role, name, keywords[i] ? keywords[i] : "");
		failures++;
	    }
	}
	return failures;
    }
    if (!keyword_values)
	return 0;

    for (size_t i = 0; keyword_values[i].canonical; i++) {
	const struct bu_cmd_value_keyword *keyword = &keyword_values[i];
	if (BU_STR_EMPTY(keyword->canonical) ||
		!spellings.insert(std::string(keyword->canonical)).second) {
	    if (msgs)
		bu_vls_printf(msgs, "%s: native %s \"%s\" has an empty or duplicate canonical keyword \"%s\"\n",
			path, role, name, keyword->canonical ? keyword->canonical : "");
	    failures++;
	}
	if (!keyword->aliases)
	    continue;
	for (size_t ai = 0; keyword->aliases[ai]; ai++) {
	    const char *alias = keyword->aliases[ai];
	    if (BU_STR_EMPTY(alias) || !spellings.insert(std::string(alias)).second) {
		if (msgs)
		    bu_vls_printf(msgs, "%s: native %s \"%s\" has an empty or duplicate keyword alias \"%s\"\n",
			path, role, name, alias ? alias : "");
		failures++;
	    }
	}
    }

    return failures;
}


static int
ged_native_schema_lint_node(const char *path, const struct bu_cmd_schema *schema, struct bu_vls *msgs)
{
    int failures = 0;
    std::set<std::string> short_opts;
    std::set<std::string> long_opts;

    if (!schema) {
	if (msgs)
	    bu_vls_printf(msgs, "%s: null native schema\n", path ? path : "(null)");
	return 1;
    }
    if (BU_STR_EMPTY(schema->name)) {
	if (msgs)
	    bu_vls_printf(msgs, "%s: native schema has no name\n", path ? path : "(null)");
	failures++;
    }
    if (schema->parse_policy < BU_CMD_PARSE_INTERSPERSED ||
	schema->parse_policy > BU_CMD_PARSE_STOP_AT_FIRST_OPERAND) {
	if (msgs)
	    bu_vls_printf(msgs, "%s: invalid native parse policy enum %d\n", path, (int)schema->parse_policy);
	failures++;
    }
    if (schema->options) {
	for (size_t i = 0; schema->options[i].canonical; i++) {
	    const struct bu_cmd_option *option = &schema->options[i];
	    if (BU_STR_EMPTY(option->canonical)) {
		if (msgs)
		    bu_vls_printf(msgs, "%s: native option %lu has no canonical name\n", path, (unsigned long)i);
		failures++;
	    }
	    if (option->shortopt && option->shortopt[0]) {
		std::string spelling(option->shortopt);
		if (!short_opts.insert(spelling).second) {
		    if (msgs)
			bu_vls_printf(msgs, "%s: duplicate native short option \"%s\"\n", path, option->shortopt);
		    failures++;
		}
	    }
	    if (option->longopt && option->longopt[0]) {
		std::string spelling(option->longopt);
		if (!long_opts.insert(spelling).second) {
		    if (msgs)
			bu_vls_printf(msgs, "%s: duplicate native long option \"%s\"\n", path, option->longopt);
		    failures++;
		}
	    }
	    if (option->alias_of && !option->alias_of[0]) {
		if (msgs)
		    bu_vls_printf(msgs, "%s: native alias has no target\n", path);
		failures++;
	    }
	    if (option->alias_of && option->alias_of[0]) {
		int target_found = 0;
		for (size_t ci = 0; schema->options[ci].canonical; ci++) {
		    const struct bu_cmd_option *target = &schema->options[ci];
		    if (!target->alias_of && BU_STR_EQUAL(target->canonical, option->alias_of)) {
			target_found = 1;
			break;
		    }
		}
		if (!target_found) {
		    if (msgs)
			bu_vls_printf(msgs, "%s: native alias \"%s\" targets unknown option \"%s\"\n",
			    path, option->canonical, option->alias_of);
		    failures++;
		}
	    }
	    if (option->value_type < BU_CMD_VALUE_FLAG || option->value_type > BU_CMD_VALUE_CUSTOM) {
		if (msgs)
		    bu_vls_printf(msgs, "%s: native option \"%s\" has invalid value type %d\n",
			path, option->canonical, (int)option->value_type);
		failures++;
	    }
	    if (option->arg_requirement < BU_CMD_ARG_REQUIRED || option->arg_requirement > BU_CMD_ARG_NONE) {
		if (msgs)
		    bu_vls_printf(msgs, "%s: native option \"%s\" has invalid argument requirement %d\n",
			path, option->canonical, (int)option->arg_requirement);
		failures++;
	    }
	/* Most typed values consume an argument.  The repeatable no-argument
	 * counter helpers are the intentional exception: their storage type is
	 * long (rather than int) even though selecting the option supplies the
	 * value. */
	if ((option->value_type == BU_CMD_VALUE_FLAG && option->arg_requirement != BU_CMD_ARG_NONE) ||
		(option->value_type != BU_CMD_VALUE_FLAG && option->value_type != BU_CMD_VALUE_CUSTOM &&
		 option->arg_requirement == BU_CMD_ARG_NONE && !option->alias_of && !option->repeat)) {
		if (msgs)
		    bu_vls_printf(msgs, "%s: native option \"%s\" has incompatible value and argument requirements\n",
			path, option->canonical);
		failures++;
	    }
	    if (option->arg_shape) {
		if (option->arg_shape->kind < BU_CMD_ARG_SHAPE_SCALAR ||
		    option->arg_shape->kind > BU_CMD_ARG_SHAPE_CUSTOM) {
		    if (msgs)
			bu_vls_printf(msgs, "%s: native option \"%s\" has invalid argument shape %d\n",
			    path, option->canonical, (int)option->arg_shape->kind);
		    failures++;
		}
		if (option->arg_shape->max_tokens != BU_CMD_COUNT_UNLIMITED &&
		    option->arg_shape->min_tokens > option->arg_shape->max_tokens) {
		    if (msgs)
			bu_vls_printf(msgs, "%s: native option \"%s\" has invalid argument token range %lu > %lu\n",
			    path, option->canonical, (unsigned long)option->arg_shape->min_tokens,
			    (unsigned long)option->arg_shape->max_tokens);
		    failures++;
		}
		if (option->arg_requirement == BU_CMD_ARG_NONE && option->arg_shape->max_tokens) {
		    if (msgs)
			bu_vls_printf(msgs, "%s: native flag \"%s\" declares an argument shape\n",
			    path, option->canonical);
		    failures++;
		}
		if (option->arg_shape->max_tokens != 1 && !option->consume) {
		    if (msgs)
			bu_vls_printf(msgs, "%s: native option \"%s\" needs an argument-shape consumer\n",
			    path, option->canonical);
		    failures++;
		}
	    }
    if (option->value_type == BU_CMD_VALUE_CUSTOM && !option->custom_parse &&
	!option->consume && !option->alias_of) {
	if (msgs)
	    bu_vls_printf(msgs, "%s: native custom option \"%s\" has no parser or consumer\n",
		path, option->canonical);
		failures++;
	    }
	    failures += ged_schema_lint_provider(path, "native option", option->semantic_provider, msgs);
	    failures += ged_native_schema_lint_keyword_values(path, "option", option->canonical,
		option->value_keywords, option->keyword_values, msgs);
	}
    }
    if (schema->operands) {
	for (size_t i = 0; schema->operands[i].name; i++) {
	    const struct bu_cmd_operand *operand = &schema->operands[i];
	    if (operand->max_count != BU_CMD_COUNT_UNLIMITED && operand->min_count > operand->max_count) {
		if (msgs)
		    bu_vls_printf(msgs, "%s: native operand \"%s\" has invalid count range %lu > %lu\n",
			path, operand->name, (unsigned long)operand->min_count, (unsigned long)operand->max_count);
		failures++;
	    }
	    if (operand->value_type < BU_CMD_VALUE_FLAG || operand->value_type > BU_CMD_VALUE_CUSTOM) {
		if (msgs)
		    bu_vls_printf(msgs, "%s: native operand \"%s\" has invalid value type %d\n",
			path, operand->name, (int)operand->value_type);
		failures++;
	    }
	    if (operand->value_type == BU_CMD_VALUE_CUSTOM) {
		if (msgs)
		    bu_vls_printf(msgs, "%s: native operand \"%s\" cannot use an unbound custom parser\n",
			path, operand->name);
		failures++;
	    }
	    if (operand->shape) {
		if (operand->shape->kind < BU_CMD_ARG_SHAPE_SCALAR ||
		    operand->shape->kind > BU_CMD_ARG_SHAPE_CUSTOM) {
		    if (msgs)
			bu_vls_printf(msgs, "%s: native operand \"%s\" has invalid shape %d\n",
			    path, operand->name, (int)operand->shape->kind);
		    failures++;
		}
		if (operand->shape->min_tokens != 1 || operand->shape->max_tokens != 1) {
		    if (msgs)
			bu_vls_printf(msgs, "%s: native operand \"%s\" shape must describe one token\n",
			    path, operand->name);
		    failures++;
		}
	    }
	    failures += ged_schema_lint_provider(path, "native operand", operand->semantic_provider, msgs);
	    failures += ged_native_schema_lint_keyword_values(path, "operand", operand->name,
		operand->value_keywords, operand->keyword_values, msgs);
	}
    }

    return failures;
}

static int
ged_grammar_lint_node(const char *path, const struct ged_cmd_grammar *grammar, struct bu_vls *msgs)
{
    int failures = 0;

    if (!grammar) {
	if (msgs)
	    bu_vls_printf(msgs, "%s: null grammar adapter\n", path ? path : "(null)");
	return 1;
    }
    if (BU_STR_EMPTY(grammar->name)) {
	if (msgs)
	    bu_vls_printf(msgs, "%s: grammar adapter has no name\n", path);
	failures++;
    }
    if (BU_STR_EMPTY(grammar->help)) {
	if (msgs)
	    bu_vls_printf(msgs, "%s: grammar adapter has no help text\n", path);
	failures++;
    }
    if (!grammar->validate || !grammar->analyze || !grammar->describe_json) {
	if (msgs)
	    bu_vls_printf(msgs, "%s: grammar adapter is missing required analysis or JSON hooks\n", path);
	failures++;
    }
    if (grammar->lint)
	failures += grammar->lint(msgs);

    return failures;
}

extern "C" int
ged_cmd_schema_lint(const char *cmd, struct bu_vls *msgs)
{
    int failures = 0;

    ged_ensure_initialized();

    if (cmd) {
	const struct bu_cmd_schema *native_schema = _ged_cmd_native_schema(cmd);
	const struct ged_cmd_grammar *grammar = _ged_cmd_grammar(cmd);
	if (!native_schema && !grammar) {
	    if (msgs)
		bu_vls_printf(msgs, "%s: schema metadata unavailable\n", cmd);
	    return 1;
	}
	if (native_schema)
	    failures += ged_native_schema_lint_node(cmd, native_schema, msgs);
	if (grammar)
	    failures += ged_grammar_lint_node(cmd, grammar, msgs);
	return failures;
    }

    for (std::map<std::string, const struct bu_cmd_schema *>::iterator it = native_schema_registry.begin(); it != native_schema_registry.end(); ++it) {
	failures += ged_native_schema_lint_node(it->first.c_str(), it->second, msgs);
    }
    for (std::map<std::string, const struct ged_cmd_grammar *>::iterator it = grammar_registry.begin(); it != grammar_registry.end(); ++it) {
	failures += ged_grammar_lint_node(it->first.c_str(), it->second, msgs);
    }

    return failures;
}

extern "C" int
ged_cmd_same(const char *cmd1, const char *cmd2)
{
    ged_ensure_initialized();
    bu_plugin_cmd_impl c1 = bu_plugin_cmd_get(cmd1);
    bu_plugin_cmd_impl c2 = bu_plugin_cmd_get(cmd2);
    if (!c1 || !c2) return 0;
    return (c1 == c2) ? 1 : 0;
}

/* Edit distance lookup retained for help suggestions */
extern "C" int
ged_cmd_lookup(const char **ncmd, const char *cmd)
{
    if (!ncmd || !cmd) return -1;
    ged_ensure_initialized();
    size_t min_dist = (size_t)LONG_MAX;
    const char *closest = NULL;
    auto cb = [](const char *name, bu_plugin_cmd_impl impl, void *ud) -> int {
	(void)impl;
	struct {
	    const char *target;
	    size_t *min_dist;
	    const char **closest;
	} *ctx = (decltype(ctx))ud;
	if (!name) return 0;
	size_t edist = bu_editdist(ctx->target, name);
	if (edist < *(ctx->min_dist)) {
	    *(ctx->min_dist) = edist;
	    *(ctx->closest) = name;
	}
	return 0;
    };
    struct { const char *target; size_t *min_dist; const char **closest; } ctx = { cmd, &min_dist, &closest };
    bu_plugin_cmd_foreach(cb, (void *)&ctx);
    *ncmd = closest;
    return (int)min_dist;
}

extern "C" size_t
ged_cmd_list(const char * const **cl)
{
    ged_ensure_initialized();
    size_t cnt = 0;
    ged_list_command_array(cl, &cnt);
    return cnt;
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
