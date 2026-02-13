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

#define BU_PLUGIN_IMPLEMENTATION
#include "./include/plugin.h"

static struct bu_vls init_msgs = BU_VLS_INIT_ZERO;

/* Optional: thread safety (add bu_mutex if needed in future) */

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
