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
#include <condition_variable>
#include <map>
#include <mutex>
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
static bool ged_initialized = false;
static bool ged_shutting_down = false;
static size_t ged_active_references = 0;
static thread_local size_t ged_thread_active_references = 0;

/* Command implementations and their metadata are one logical registry.  The
 * generalized command table has its own lock, but a libged transaction must
 * span that table, the metadata maps, compatibility aliases, diagnostics, and
 * retained plugin handles. */
static std::recursive_mutex &
ged_registry_mutex()
{
    static std::recursive_mutex mutex;
    return mutex;
}


static std::condition_variable_any &
ged_registry_condition()
{
    static std::condition_variable_any condition;
    return condition;
}


static std::vector<void *> &
ged_plugin_handles()
{
    static std::vector<void *> handles;
    return handles;
}

struct ged_metadata_registry {
    std::mutex mutex;
    std::map<std::string, const struct bu_arg_schema *> native_schemas;
    std::map<std::string, const struct ged_cmd_grammar *> grammars;
};


static struct ged_metadata_registry &
ged_metadata()
{
    /* REGISTER_GED_COMMAND constructors in other translation units may arrive
     * before any other libged initialization.  A function-local registry is
     * initialized on first use and therefore has no cross-TU ordering hazard. */
    static struct ged_metadata_registry registry;
    return registry;
}


static size_t
ged_metadata_count_locked(const std::string &key)
{
    struct ged_metadata_registry &registry = ged_metadata();
    std::lock_guard<std::mutex> guard(registry.mutex);
    return (registry.native_schemas.find(key) != registry.native_schemas.end() ? 1 : 0) +
	(registry.grammars.find(key) != registry.grammars.end() ? 1 : 0);
}


static void
ged_metadata_erase_locked(const std::string &key)
{
    struct ged_metadata_registry &registry = ged_metadata();
    std::lock_guard<std::mutex> guard(registry.mutex);
    registry.native_schemas.erase(key);
    registry.grammars.erase(key);
}


static bool
ged_command_key_valid(const std::string &key)
{
    if (key.empty())
	return false;
    for (unsigned char c : key)
	if (std::isspace(c))
	    return false;
    return true;
}

static int ged_native_schema_lint_node(const char *, const struct bu_arg_schema *,
	struct bu_vls *);
static int ged_grammar_lint_declaration(const char *, const struct ged_cmd_grammar *,
	struct bu_vls *);

/* -------------------------------------------------------------------------- */
/* Public Registry API                                                        */
/* -------------------------------------------------------------------------- */

extern "C" GED_EXPORT int
ged_register_command(const struct ged_cmd *cmd)
{
    int ret;

    if (!cmd || !cmd->i || !cmd->i->cname || !cmd->i->cmd) return -1;

    std::lock_guard<std::recursive_mutex> transaction(ged_registry_mutex());

    std::string key(cmd->i->cname);

    if (ged_shutting_down || !ged_command_key_valid(key) ||
	key.compare(0, 6, "_mged_") == 0)
	return -1;

    size_t metadata_kinds = (cmd->i->native_schema ? 1 : 0) +
	(cmd->i->grammar ? 1 : 0);
    if (key.empty() || metadata_kinds > 1) {
	bu_vls_printf(&init_msgs,
	    "command '%s': at most one metadata representation may be declared\n",
	    key.empty() ? "(unnamed)" : key.c_str());
	return -1;
    }

    std::string mged_key = std::string("_mged_") + key;

    /* The implementation, compatibility alias, and metadata participate in
     * the same first-wins decision. */
    if (bu_plugin_cmd_exists(key.c_str()) ||
	bu_plugin_cmd_exists(mged_key.c_str()) || ged_metadata_count_locked(key)) {
	return 1;
    }

    /* Validate and publish metadata before making the command executable.
     * This prevents a malformed declaration from silently producing a command
     * with no completion, validation, or schema description. */
    if (cmd->i->native_schema) {
	ret = ged_register_command_native_schema(key.c_str(), cmd->i->native_schema);
	if (ret != 0) {
	    bu_vls_printf(&init_msgs, "command '%s': native schema registration failed\n",
		key.c_str());
	    return -1;
	}
    }
    if (cmd->i->grammar) {
	ret = ged_register_command_grammar(key.c_str(), cmd->i->grammar);
	if (ret != 0) {
	    bu_vls_printf(&init_msgs, "command '%s': grammar registration failed\n",
		key.c_str());
	    return -1;
	}
    }
    /* Register the public spelling and compatibility alias as one batch.  If
     * the generalized registry rejects the batch, remove the metadata which
     * was staged while this transaction lock excluded libged readers. */
    const bu_plugin_cmd commands[] = {
	{key.c_str(), cmd->i->cmd},
	{mged_key.c_str(), cmd->i->cmd}
    };
    ret = bu_plugin_cmd_register_batch(commands, 2);
    if (ret != 0) {
	ged_metadata_erase_locked(key);
	bu_vls_printf(&init_msgs, "command '%s': command registration failed\n",
	    key.c_str());
	return ret;
    }

    return 0;
}


extern "C" GED_EXPORT int
ged_register_command_native_schema(const char *name, const struct bu_arg_schema *schema)
{
    struct bu_vls lint_msgs = BU_VLS_INIT_ZERO;
    int lint_failures;

    if (!name || !schema)
	return -1;
    std::lock_guard<std::recursive_mutex> transaction(ged_registry_mutex());
    if (ged_shutting_down)
	return -1;
    std::string key(name);
    if (!ged_command_key_valid(key) || key.compare(0, 6, "_mged_") == 0)
	return -1;

    lint_failures = ged_native_schema_lint_node(name, schema, &lint_msgs);
    if (lint_failures) {
	bu_vls_printf(&init_msgs, "native command schema '%s' rejected:\n%s", name,
	    bu_vls_addr(&lint_msgs));
	bu_vls_free(&lint_msgs);
	return -1;
    }
	bu_vls_free(&lint_msgs);
    struct ged_metadata_registry &registry = ged_metadata();
    std::lock_guard<std::mutex> guard(registry.mutex);
    if (registry.native_schemas.find(key) != registry.native_schemas.end() ||
	registry.grammars.find(key) != registry.grammars.end())
	return 1;

    registry.native_schemas[key] = schema;
    return 0;
}

extern "C" int
ged_register_command_grammar(const char *name, const struct ged_cmd_grammar *grammar)
{
    struct bu_vls lint_msgs = BU_VLS_INIT_ZERO;
    int lint_failures;

    if (!name || !grammar)
	return -1;
    std::lock_guard<std::recursive_mutex> transaction(ged_registry_mutex());
    if (ged_shutting_down)
	return -1;
    std::string key(name);
    if (!ged_command_key_valid(key) || key.compare(0, 6, "_mged_") == 0)
	return -1;

    /* Registration can occur while the static registry is still being
     * assembled.  Check the declaration itself here, but defer the optional
     * command-owned lint callback until the completed registry is queried: a
     * callback may legitimately inspect sibling commands. */
    lint_failures = ged_grammar_lint_declaration(name, grammar, &lint_msgs);
    if (lint_failures) {
	bu_vls_printf(&init_msgs, "command grammar '%s' rejected:\n%s", name,
	    bu_vls_addr(&lint_msgs));
	bu_vls_free(&lint_msgs);
	return -1;
    }
    bu_vls_free(&lint_msgs);

    struct ged_metadata_registry &registry = ged_metadata();
    std::lock_guard<std::mutex> guard(registry.mutex);
    if (registry.grammars.find(key) != registry.grammars.end() ||
	registry.native_schemas.find(key) != registry.native_schemas.end())
	return 1;

    registry.grammars[key] = grammar;
    return 0;
}

extern "C" int
ged_command_exists(const char *name)
{
    if (!name)
	return 0;

    ged_ensure_initialized();
    std::lock_guard<std::recursive_mutex> transaction(ged_registry_mutex());
    return bu_plugin_cmd_exists(name);
}

extern "C" size_t
ged_registered_count(void)
{
    ged_ensure_initialized();
    std::lock_guard<std::recursive_mutex> transaction(ged_registry_mutex());
    return bu_plugin_cmd_count();
}

extern "C" void
ged_list_command_names(struct bu_vls *out_csv)
{
    ged_ensure_initialized();
    if (!out_csv) return;
    std::lock_guard<std::recursive_mutex> transaction(ged_registry_mutex());
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
    std::lock_guard<std::recursive_mutex> transaction(ged_registry_mutex());
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
    char **alist = (char **)bu_calloc(names.size() + 1, sizeof(char *),
	"ged cmd argv");
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

	if (!bu_plugin_path_allowed(pfile)) {
	    bu_vls_printf(&init_msgs, "plugin '%s': path rejected by host policy\n", pfile);
	    continue;
	}

	/* Open once and retain this exact handle only after both manifests and all
	 * command/schema pairs have registered successfully. */
	void *handle = bu_dlopen(pfile, BU_RTLD_NOW | BU_RTLD_LOCAL);
	if (!handle) {
	    const char *loader_error = bu_dlerror();
	    bu_vls_printf(&init_msgs, "plugin '%s': unable to load: %s\n", pfile,
		loader_error ? loader_error : "unknown dynamic loader error");
	    continue;
	}

	typedef const bu_plugin_manifest *(*plugin_info_fn)(void);
	void *plugin_info_val = bu_dlsym(handle, BU_PLUGIN_MANIFEST_SYM);
	plugin_info_fn plugin_info = (plugin_info_fn)(intptr_t)plugin_info_val;
	const bu_plugin_manifest *plugin_manifest = plugin_info ? plugin_info() : NULL;
	if (!plugin_manifest || plugin_manifest->abi_version != BU_PLUGIN_ABI_VERSION ||
		plugin_manifest->struct_size < sizeof(bu_plugin_manifest) ||
		plugin_manifest->cmd_count > 8192U ||
		(plugin_manifest->cmd_count && !plugin_manifest->commands)) {
	    bu_vls_printf(&init_msgs, "plugin '%s': invalid generalized manifest\n", pfile);
	    (void)bu_dlclose(handle);
	    continue;
	}

	std::unordered_map<std::string, const struct ged_cmd_schema *> schemas;
	bool manifest_valid = true;
	void *schema_info_val = bu_dlsym(handle, "ged_plugin_schema_info");
	if (schema_info_val) {
	    typedef const struct ged_plugin_schema_manifest *(*schema_info_fn)(void);
	    schema_info_fn schema_info = (schema_info_fn)(intptr_t)schema_info_val;
	    const struct ged_plugin_schema_manifest *schema_manifest = schema_info();
	    manifest_valid = schema_manifest &&
		schema_manifest->struct_size >= sizeof(struct ged_plugin_schema_manifest) &&
		schema_manifest->abi_version == GED_PLUGIN_SCHEMA_ABI_VERSION &&
		schema_manifest->schema_count < 8192U &&
		(!schema_manifest->schema_count || schema_manifest->schemas);
	    for (unsigned int si = 0; manifest_valid &&
		    si < schema_manifest->schema_count; si++) {
		const struct ged_cmd_schema *schema = &schema_manifest->schemas[si];
		size_t kinds = (schema->native_schema ? 1 : 0) +
		    (schema->grammar ? 1 : 0);
		if (BU_STR_EMPTY(schema->cname) || kinds > 1 ||
			!schemas.emplace(schema->cname, schema).second)
		    manifest_valid = false;
	    }
	}
	if (!manifest_valid) {
	    bu_vls_printf(&init_msgs, "plugin '%s': invalid schema manifest\n", pfile);
	    (void)bu_dlclose(handle);
	    continue;
	}

	std::vector<std::pair<std::string, ged_func_ptr>> registered;
	std::unordered_set<std::string> command_names;
	for (unsigned int ci = 0; manifest_valid && ci < plugin_manifest->cmd_count; ci++) {
	    const bu_plugin_cmd *command = &plugin_manifest->commands[ci];
	    if (BU_STR_EMPTY(command->name) || !command->impl ||
		    !command_names.insert(command->name).second) {
		manifest_valid = false;
		break;
	    }
	    auto schema_it = schemas.find(command->name);
	    const struct ged_cmd_schema *schema = schema_it == schemas.end() ?
		NULL : schema_it->second;
	    struct ged_cmd_impl impl = {
		command->name, command->impl, GED_CMD_DEFAULT,
		schema ? schema->native_schema : NULL,
		schema ? schema->grammar : NULL
	    };
	    const struct ged_cmd command_record = {&impl};
	    int command_ret = ged_register_command(&command_record);
	    if (command_ret < 0) {
		manifest_valid = false;
		break;
	    }
	    if (!command_ret)
		registered.emplace_back(command->name, command->impl);
	    if (schema_it != schemas.end())
		schemas.erase(schema_it);
	}
	if (!schemas.empty())
	    manifest_valid = false;

	if (!manifest_valid) {
	    for (auto it = registered.rbegin(); it != registered.rend(); ++it) {
		std::string alias = std::string("_mged_") + it->first;
		const bu_plugin_cmd rollback[] = {
		    {it->first.c_str(), it->second}, {alias.c_str(), it->second}
		};
		(void)bu_plugin_cmd_unregister_batch(rollback, 2);
		ged_metadata_erase_locked(it->first);
	    }
	    bu_vls_printf(&init_msgs, "plugin '%s': registration transaction rejected\n", pfile);
	    (void)bu_dlclose(handle);
	    continue;
	}

	if (!registered.empty())
	    ged_plugin_handles().push_back(handle);
	else
	    (void)bu_dlclose(handle);
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
    std::lock_guard<std::recursive_mutex> transaction(ged_registry_mutex());
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

extern "C" void ged_ensure_initialized()
{
    std::lock_guard<std::recursive_mutex> transaction(ged_registry_mutex());
    if (ged_initialized)
	return;
    ged_shutting_down = false;
    libged_init();
    ged_initialized = true;
}


static void
ged_registry_reference_add_locked(void)
{
    ged_active_references++;
    ged_thread_active_references++;
}


extern "C" int
_ged_registry_access_acquire(void)
{
    std::lock_guard<std::recursive_mutex> transaction(ged_registry_mutex());
    if (ged_shutting_down && !ged_thread_active_references)
	return -1;
    ged_registry_reference_add_locked();
    return 0;
}


extern "C" int
_ged_registry_mutation_acquire(void)
{
    std::lock_guard<std::recursive_mutex> transaction(ged_registry_mutex());
    if (ged_shutting_down)
	return -1;
    ged_registry_reference_add_locked();
    return 0;
}


extern "C" void
_ged_registry_access_release(void)
{
    std::lock_guard<std::recursive_mutex> transaction(ged_registry_mutex());
    if (!ged_active_references || !ged_thread_active_references)
	return;
    ged_active_references--;
    ged_thread_active_references--;
    if (!ged_active_references)
	ged_registry_condition().notify_all();
}


extern "C" size_t
_ged_registry_access_depth(void)
{
    return ged_thread_active_references;
}


namespace {

class ged_registry_reference {
    public:
	explicit ged_registry_reference(bool mutation = false) : held(
	    (mutation ? _ged_registry_mutation_acquire() :
	     _ged_registry_access_acquire()) == 0) {}
	~ged_registry_reference()
	{
	    if (held)
		_ged_registry_access_release();
	}
	bool acquired() const { return held; }

    private:
	bool held;
};

}

extern "C" ged_func_ptr
_ged_cmd_func(const char *name)
{
    if (BU_STR_EMPTY(name))
	return GED_FUNC_PTR_NULL;
    ged_ensure_initialized();
    std::lock_guard<std::recursive_mutex> transaction(ged_registry_mutex());
    if (ged_shutting_down && !ged_thread_active_references)
	return GED_FUNC_PTR_NULL;
    return (ged_func_ptr)bu_plugin_cmd_get(name);
}

extern "C" ged_func_ptr
_ged_cmd_acquire(const char *name)
{
    if (BU_STR_EMPTY(name))
	return GED_FUNC_PTR_NULL;
    ged_ensure_initialized();
    std::lock_guard<std::recursive_mutex> transaction(ged_registry_mutex());
    if (ged_shutting_down && !ged_thread_active_references)
	return GED_FUNC_PTR_NULL;
    ged_func_ptr func = (ged_func_ptr)bu_plugin_cmd_get(name);
    if (func)
	ged_registry_reference_add_locked();
    return func;
}

extern "C" void
_ged_cmd_release(void)
{
    _ged_registry_access_release();
}

extern "C" void
libged_shutdown(void)
{
    std::unique_lock<std::recursive_mutex> transaction(ged_registry_mutex());
    if (ged_thread_active_references) {
	bu_log("libged_shutdown: refusing shutdown from an active libged operation\n");
	return;
    }
    ged_shutting_down = true;
    ged_registry_condition().wait(transaction, [] { return !ged_active_references; });

    /* Metadata and provider records contain pointers into plugin modules, so
     * tear them down before unloading retained handles. */
    _ged_cmd_semantic_provider_registry_reset();

    {
	struct ged_metadata_registry &registry = ged_metadata();
	std::lock_guard<std::mutex> guard(registry.mutex);
	registry.native_schemas.clear();
	registry.grammars.clear();
    }

    bu_plugin_shutdown();
    std::vector<void *> &handles = ged_plugin_handles();
    for (auto it = handles.rbegin(); it != handles.rend(); ++it)
	if (*it) (void)bu_dlclose(*it);
    handles.clear();
    bu_vls_free(&init_msgs);
    ged_initialized = false;
    ged_shutting_down = false;
}

/* Provide existing init message accessor */
extern "C" const char *
ged_init_msgs()
{
    ged_ensure_initialized();
    std::lock_guard<std::recursive_mutex> transaction(ged_registry_mutex());
    static thread_local std::string snapshot;
    snapshot = bu_vls_cstr(&init_msgs);
    return snapshot.c_str();
}

/* Backwards compatibility wrappers for old APIs (if needed) */

extern "C" int
ged_cmd_exists(const char *cmd)
{
    ged_ensure_initialized();
    std::lock_guard<std::recursive_mutex> transaction(ged_registry_mutex());
    return bu_plugin_cmd_exists(cmd);
}

extern "C" int
ged_cmd_schema_exists(const char *cmd)
{
    if (!cmd)
	return 0;

    ged_ensure_initialized();
    ged_registry_reference reference;
    if (!reference.acquired())
	return 0;
    return (_ged_cmd_native_schema(cmd) != NULL || _ged_cmd_grammar(cmd) != NULL) ? 1 : 0;
}

extern "C" char *
ged_cmd_schema_json(const char *cmd)
{
    if (!cmd)
	return NULL;

    ged_ensure_initialized();
    ged_registry_reference reference;
    if (!reference.acquired())
	return NULL;
    const struct ged_cmd_grammar *grammar = _ged_cmd_grammar(cmd);
    if (grammar && grammar->describe_json)
	return grammar->describe_json();
    const struct bu_arg_schema *native_schema = _ged_cmd_native_schema(cmd);
    if (native_schema)
	return bu_arg_schema_describe_json(native_schema);
    return NULL;
}

extern "C" char *
ged_cmd_help(const char *cmd, const char *invocation)
{
    if (!cmd)
	return NULL;

    ged_ensure_initialized();
    ged_registry_reference reference;
    if (!reference.acquired())
	return NULL;
    const struct ged_cmd_grammar *grammar = _ged_cmd_grammar(cmd);
    if (grammar && grammar->describe_help)
	return grammar->describe_help(invocation);
    const struct bu_arg_schema *native_schema = _ged_cmd_native_schema(cmd);
    return native_schema ? bu_arg_schema_help(native_schema, invocation) : NULL;
}

extern "C" int
ged_cmd_help_append(struct bu_vls *output, const char *cmd, const char *invocation)
{
    if (!output)
	return -1;

    char *help = ged_cmd_help(cmd, invocation);
    if (!help)
	return -1;

    bu_vls_strcat(output, help);
    bu_free(help, "GED command help");
    return 0;
}

extern "C" const struct bu_arg_schema *
_ged_cmd_native_schema(const char *cmd)
{
    if (!cmd)
	return NULL;
    std::lock_guard<std::recursive_mutex> transaction(ged_registry_mutex());

    const char *lookup = cmd;
    if (bu_strncmp(lookup, "_mged_", 6) == 0)
	lookup += 6;

    struct ged_metadata_registry &registry = ged_metadata();
    std::lock_guard<std::mutex> guard(registry.mutex);
    auto it = registry.native_schemas.find(std::string(lookup));
    if (it == registry.native_schemas.end())
	return NULL;

    return it->second;
}

extern "C" const struct ged_cmd_grammar *
_ged_cmd_grammar(const char *cmd)
{
    if (!cmd)
	return NULL;
    std::lock_guard<std::recursive_mutex> transaction(ged_registry_mutex());

    const char *lookup = cmd;
    if (bu_strncmp(lookup, "_mged_", 6) == 0)
	lookup += 6;

    struct ged_metadata_registry &registry = ged_metadata();
    std::lock_guard<std::mutex> guard(registry.mutex);
    auto it = registry.grammars.find(std::string(lookup));
    if (it == registry.grammars.end())
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
ged_native_schema_lint_node(const char *path, const struct bu_arg_schema *schema, struct bu_vls *msgs)
{
    int failures;

    if (!schema) {
	return bu_arg_schema_lint(schema, msgs);
    }
    failures = bu_arg_schema_lint(schema, msgs);
    if (schema->options) {
	for (size_t i = 0; bu_arg_option_is_valid(&schema->options[i]); i++) {
	    const struct bu_arg_option *option = &schema->options[i];
	    failures += ged_schema_lint_provider(path, "native option", option->semantic_provider, msgs);
	}
    }
    if (schema->operands) {
	for (size_t i = 0; schema->operands[i].name; i++) {
	    const struct bu_arg_operand *operand = &schema->operands[i];
	    failures += ged_schema_lint_provider(path, "native operand", operand->semantic_provider, msgs);
	}
    }

    return failures;
}

static int
ged_grammar_lint_declaration(const char *path, const struct ged_cmd_grammar *grammar,
	struct bu_vls *msgs)
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
    if (!grammar->validate || !grammar->analyze || !grammar->describe_json ||
	    !grammar->describe_help) {
	if (msgs)
	    bu_vls_printf(msgs,
		"%s: grammar adapter is missing required validation, analysis, JSON, or help hooks\n",
		path);
	failures++;
    }
    return failures;
}


static int
ged_grammar_lint_node(const char *path, const struct ged_cmd_grammar *grammar,
	struct bu_vls *msgs)
{
    int failures = ged_grammar_lint_declaration(path, grammar, msgs);
    if (grammar && grammar->lint)
	failures += grammar->lint(msgs);
    return failures;
}

extern "C" int
ged_cmd_schema_lint(const char *cmd, struct bu_vls *msgs)
{
    int failures = 0;

    ged_ensure_initialized();
    ged_registry_reference reference;
    if (!reference.acquired())
	return -1;

    if (cmd) {
	const struct bu_arg_schema *native_schema = _ged_cmd_native_schema(cmd);
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

    std::map<std::string, const struct bu_arg_schema *> native_schemas;
    std::map<std::string, const struct ged_cmd_grammar *> grammars;
    {
	struct ged_metadata_registry &registry = ged_metadata();
	std::lock_guard<std::mutex> guard(registry.mutex);
	native_schemas = registry.native_schemas;
	grammars = registry.grammars;
    }
    for (const auto &entry : native_schemas) {
	failures += ged_native_schema_lint_node(entry.first.c_str(), entry.second, msgs);
    }
    for (const auto &entry : grammars) {
	failures += ged_grammar_lint_node(entry.first.c_str(), entry.second, msgs);
    }

    return failures;
}

extern "C" int
ged_cmd_same(const char *cmd1, const char *cmd2)
{
    ged_ensure_initialized();
    std::lock_guard<std::recursive_mutex> transaction(ged_registry_mutex());
    bu_plugin_cmd_impl c1 = bu_plugin_cmd_get(cmd1);
    bu_plugin_cmd_impl c2 = bu_plugin_cmd_get(cmd2);
    if (!c1 || !c2) return 0;
    return (c1 == c2) ? 1 : 0;
}


extern "C" int
ged_cmd_valid(const char *cmd, const char *func)
{
    if (!cmd || !ged_cmd_exists(cmd)) return 1;
    if (!func) return 0;
    if (!ged_cmd_exists(func)) return 1;

    return ged_cmd_same(cmd, func) ? 0 : 2;
}


/* Edit distance lookup retained for help suggestions */
extern "C" int
ged_cmd_lookup(const char **ncmd, const char *cmd)
{
    if (!ncmd) return -1;
    *ncmd = NULL;
    if (!cmd) return -1;
    ged_ensure_initialized();
    std::lock_guard<std::recursive_mutex> transaction(ged_registry_mutex());
    size_t min_dist = (size_t)LONG_MAX;
    static thread_local std::string closest;
    closest.clear();
    auto cb = [](const char *name, bu_plugin_cmd_impl impl, void *ud) -> int {
	(void)impl;
	struct {
	    const char *target;
	    size_t *min_dist;
	    std::string *closest;
	} *ctx = (decltype(ctx))ud;
	if (!name || bu_strncmp(name, "_mged_", 6) == 0) return 0;
	size_t edist = bu_editdist(ctx->target, name);
	if (edist < *(ctx->min_dist)) {
	    *(ctx->min_dist) = edist;
	    *(ctx->closest) = name;
	}
	return 0;
    };
    struct { const char *target; size_t *min_dist; std::string *closest; } ctx = {
	cmd, &min_dist, &closest
    };
    bu_plugin_cmd_foreach(cb, (void *)&ctx);
    if (closest.empty())
	return -1;
    *ncmd = closest.c_str();
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
