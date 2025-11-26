/*                     G E D _ I N I T . C P P
 * BRL-CAD
 *
 * Copyright (c) 2019-2025 United States Government as represented by
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

#include "./include/plugin.h"

/* -------------------------------------------------------------------------- */
/* Registry Data                                                              */
/* -------------------------------------------------------------------------- */

struct ged_registry {
    std::unordered_map<std::string, const struct ged_cmd *> by_name;
    std::vector<std::string> ordered_names; /* rebuilt lazily if invalidated */
    bool ordered_valid = false;
    std::set<void *> plugin_handles;        /* track loaded plugin dl handles */
    bool plugins_scanned = false;
    bool shutdown = false;
};

// Meyers singleton
static ged_registry &get_registry() {
    static ged_registry G;
    return G;
}

static struct bu_vls init_msgs = BU_VLS_INIT_ZERO;

/* Optional: thread safety (add bu_mutex if needed in future) */

/* -------------------------------------------------------------------------- */
/* Internal Helpers                                                           */
/* -------------------------------------------------------------------------- */

static void registry_invalidate_ordered()
{
    ged_registry &G = get_registry();
    G.ordered_valid = false;
}

static void registry_rebuild_ordered()
{
    ged_registry &G = get_registry();
    if (G.ordered_valid) return;
    G.ordered_names.clear();
    G.ordered_names.reserve(G.by_name.size());
    for (auto &p : G.by_name) {
	G.ordered_names.push_back(p.first);
    }
    std::sort(G.ordered_names.begin(), G.ordered_names.end());
    G.ordered_valid = true;
}

/* -------------------------------------------------------------------------- */
/* Public Registry API                                                        */
/* -------------------------------------------------------------------------- */

extern "C" int
ged_register_command(const struct ged_cmd *cmd)
{
    if (!cmd || !cmd->i || !cmd->i->cname || !cmd->i->cmd) return -1;

    ged_registry &G = get_registry();

    if (G.shutdown) return -2;

    std::string key(cmd->i->cname);
    if (G.by_name.find(key) != G.by_name.end()) {
	/* First-wins: ignore duplicate */
	return 1;
    }

    G.by_name[key] = cmd;

    /* MGED historical _mged_ prefix support */
    std::string mged_key = std::string("_mged_") + key;
    G.by_name[mged_key] = cmd;

    registry_invalidate_ordered();
    return 0;
}

extern "C" int
ged_command_exists(const char *name)
{
    if (!name)
	return 0;

    ged_registry &G = get_registry();

    return (G.by_name.find(std::string(name)) != G.by_name.end()) ? 1 : 0;
}

extern "C" const struct ged_cmd *
ged_get_command(const char *name)
{
    if (!name) return NULL;

    ged_registry &G = get_registry();

    auto it = G.by_name.find(std::string(name));
    if (it == G.by_name.end()) return NULL;
    return it->second;
}

extern "C" size_t
ged_registered_count(void)
{
    ged_registry &G = get_registry();
    return G.by_name.size();
}

extern "C" void
ged_list_command_names(struct bu_vls *out_csv)
{
    if (!out_csv) return;
    registry_rebuild_ordered();
    bu_vls_trunc(out_csv, 0);
    bool first = true;
    ged_registry &G = get_registry();
    for (auto &n : G.ordered_names) {
	if (n.rfind("_mged_", 0) == 0) continue; /* skip synthetic aliases */
	if (!first) bu_vls_printf(out_csv, ",");
	bu_vls_printf(out_csv, "%s", n.c_str());
	first = false;
    }
}

extern "C" void
ged_list_command_array(const char * const **cl, size_t *cnt)
{
    if (!cl || !cnt) return;
    registry_rebuild_ordered();
    ged_registry &G = get_registry();
    char **alist = (char **)bu_calloc(G.ordered_names.size(), sizeof(char *), "ged cmd argv");
    size_t len = 0;
    for (auto &n : G.ordered_names) {
	if (n.rfind("_mged_", 0) == 0) continue;
	alist[len++] = bu_strdup(n.c_str());
    }
    *cl = (const char * const *)alist;
    *cnt = len;
}

/* -------------------------------------------------------------------------- */
/* Plugin Loading                                                             */
/* -------------------------------------------------------------------------- */

static int load_plugin_handle(void *dl_handle, const char *pfile)
{
    const char *psymbol = "ged_plugin_info";
    void *info_val = bu_dlsym(dl_handle, psymbol);
    const struct ged_plugin *(*plugin_info)() = (const struct ged_plugin *(*)())(intptr_t)info_val;
    if (!plugin_info) {
	const char *error_msg = bu_dlerror();
	if (error_msg) bu_vls_printf(&init_msgs, "%s\n", error_msg);
	bu_vls_printf(&init_msgs, "Unable to load symbols from '%s' (skipping: missing %s)\n", pfile, psymbol);
	return -1;
    }

    const struct ged_plugin *plugin = plugin_info();
    if (!plugin || !plugin->cmds || !plugin->cmd_cnt) {
	bu_vls_printf(&init_msgs, "Invalid or empty plugin '%s' (skipping)\n", pfile);
	return -2;
    }

    if (*((const uint32_t *)(plugin)) != (uint32_t)(GED_API)) {
	bu_vls_printf(&init_msgs, "Plugin version mismatch for '%s' (skipping)\n", pfile);
	return -3;
    }

    const struct ged_cmd **cmds = plugin->cmds;
    for (int c = 0; c < plugin->cmd_cnt; c++) {
	const struct ged_cmd *cmd = cmds[c];
	if (!cmd) break;
	(void)ged_register_command(cmd);
    }

    return 0;
}

extern "C" int
ged_load_plugin_file(const char *fullpath)
{
    ged_registry &G = get_registry();
    if (!fullpath || G.shutdown) return -1;
    void *dl_handle = bu_dlopen(fullpath, BU_RTLD_NOW);
    if (!dl_handle) {
	const char *error_msg = bu_dlerror();
	if (error_msg) bu_vls_printf(&init_msgs, "%s\n", error_msg);
	bu_vls_printf(&init_msgs, "Failed to load custom plugin '%s'\n", fullpath);
	return -2;
    }
    int ret = load_plugin_handle(dl_handle, fullpath);
    if (ret != 0) {
	bu_dlclose(dl_handle);
	return ret;
    }
    G.plugin_handles.insert(dl_handle);
    return 0;
}

#if defined(LIBGED_STATIC_CORE)
    // Linker set section enumeration for ELF
#if defined(__GNUC__)
    // GCC/Clang: linker offers __start_*/__stop_* symbols.
    extern "C" {
	extern const struct ged_cmd *__start_ged_cmd_set[];
	extern const struct ged_cmd *__stop_ged_cmd_set[];
    }
    static void ged_static_register_linkerset()
    {
	const struct ged_cmd **p = __start_ged_cmd_set;
	while (p < __stop_ged_cmd_set) {
	    ged_register_command(*p);
	    ++p;
	}
    }
#elif defined(_MSC_VER)
    // MSVC: pragma allocate sections and enumerate using section address from map.
    // For maximal correctness and portability: use __pragma(section) to get start/end.
    // This relies on VC++ initialization order but is robust if all .obj files are linked.
    // The code below will work _if_ the linker includes at least one symbol from each TU.
    // For edge cases, see 'force all objects' below.
    extern "C" {
	// __ged_cmd_ptrs_start & end: defined using special MSVC initializers
	__declspec(selectany) GED_CMD_LINKER_SET const struct ged_cmd * __ged_cmd_ptrs_start = 0;
	__declspec(selectany) GED_CMD_LINKER_SET const struct ged_cmd * __ged_cmd_ptrs_end = 0;
    }
    static void ged_static_register_linkerset()
    {
	const struct ged_cmd **start = &__ged_cmd_ptrs_start + 1;
	const struct ged_cmd **end = &__ged_cmd_ptrs_end;
	for (const struct ged_cmd **p = start; p < end; ++p)
	    if (*p) ged_register_command(*p);
    }
#else
    static void ged_static_register_linkerset() { /* fallback no-op */ }
#endif
#endif


extern "C" void
ged_scan_plugins(void)
{
    ged_registry &G = get_registry();
    if (G.shutdown) return;
    if (G.plugins_scanned) return;
    const char *env_block = getenv("GED_NO_PLUGIN_SCAN");
    if (env_block && BU_STR_EQUAL(env_block, "1")) {
	G.plugins_scanned = true;
	return;
    }

    const char *ppath = bu_dir(NULL, 0, BU_DIR_LIBEXEC, "ged", NULL);
    if (!ppath) {
	G.plugins_scanned = true;
	return;
    }

    struct bu_vls pattern = BU_VLS_INIT_ZERO;
    bu_vls_sprintf(&pattern, "*%s", GED_PLUGIN_SUFFIX);
    char **ged_filenames = NULL;
    size_t ged_nfiles = bu_file_list(ppath, bu_vls_cstr(&pattern), &ged_filenames);

    for (size_t i = 0; i < ged_nfiles; i++) {
	char pfile[MAXPATHLEN] = {0};
	bu_dir(pfile, MAXPATHLEN, BU_DIR_LIBEXEC, "ged", ged_filenames[i], NULL);
	void *dl_handle = bu_dlopen(pfile, BU_RTLD_NOW);
	if (!dl_handle) {
	    const char *error_msg = bu_dlerror();
	    if (error_msg) bu_vls_printf(&init_msgs, "%s\n", error_msg);
	    bu_vls_printf(&init_msgs, "Unable to dynamically load '%s' (skipping)\n", pfile);
	    continue;
	}

	if (load_plugin_handle(dl_handle, pfile) != 0) {
	    bu_dlclose(dl_handle);
	    continue;
	}

	G.plugin_handles.insert(dl_handle);
    }

    bu_vls_free(&pattern);
    bu_argv_free(ged_nfiles, ged_filenames);
    G.plugins_scanned = true;
}

/* -------------------------------------------------------------------------- */
/* Initialization & Shutdown                                                  */
/* -------------------------------------------------------------------------- */
extern "C" void ged_force_static_registration(void);
extern "C" void
libged_init(void)
{
    ged_registry &G = get_registry();
    if (G.shutdown) return;

#if defined(LIBGED_STATIC_CORE)
    ged_force_static_registration();
    ged_static_register_linkerset();  // Enumerate and register all statically linked commands
#endif

    /* At this point, static constructors may or may not have run for all TUs.
     * Any that have will have already populated G.by_name via ged_register_command.
     * We proceed to scan plugins once to add plugin-only and user-provided commands.
     */
    ged_scan_plugins();
}

extern "C" void
libged_shutdown(void)
{
    ged_registry &G = get_registry();
    if (G.shutdown) return;
    /* Unload plugins in reverse order */
    for (auto it = G.plugin_handles.rbegin(); it != G.plugin_handles.rend(); ++it) {
	bu_dlclose(*it);
    }
    G.plugin_handles.clear();
    G.by_name.clear();
    G.ordered_names.clear();
    bu_vls_free(&init_msgs);
    G.shutdown = true;
}

/* Provide existing init message accessor */
extern "C" const char *
ged_init_msgs()
{
    return bu_vls_cstr(&init_msgs);
}

/* Backwards compatibility wrappers for old APIs (if needed) */

extern "C" int
ged_cmd_exists(const char *cmd)
{
    return ged_command_exists(cmd);
}

extern "C" int
ged_cmd_same(const char *cmd1, const char *cmd2)
{
    const struct ged_cmd *c1 = ged_get_command(cmd1);
    const struct ged_cmd *c2 = ged_get_command(cmd2);
    if (!c1 || !c2) return 0;
    return (c1->i->cmd == c2->i->cmd) ? 1 : 0;
}

/* Edit distance lookup retained for help suggestions */
extern "C" int
ged_cmd_lookup(const char **ncmd, const char *cmd)
{
    if (!ncmd || !cmd) return -1;
    registry_rebuild_ordered();
    size_t min_dist = (size_t)LONG_MAX;
    const char *closest = NULL;
    ged_registry &G = get_registry();
    for (auto &n : G.ordered_names) {
	size_t edist = bu_editdist(cmd, n.c_str());
	if (edist < min_dist) {
	    min_dist = edist;
	    closest = n.c_str();
	}
    }
    *ncmd = closest;
    return (int)min_dist;
}

extern "C" size_t
ged_cmd_list(const char * const **cl)
{
    size_t cnt = 0;
    ged_list_command_array(cl, &cnt);
    return cnt;
}

/* Legacy initializer object retained for compatibility */
struct libged_initializer {
    libged_initializer()  { libged_init(); }
    ~libged_initializer() { /* Do NOT auto-shutdown – keep commands alive for app lifetime */ }
};

/* Instantiate global initializer (startup scan still runs once) */
static libged_initializer LIBGED;

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
