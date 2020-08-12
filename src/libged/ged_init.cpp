/*                     G E D _ I N I T . C P P
 * BRL-CAD
 *
 * Copyright (c) 2019-2020 United States Government as represented by
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
 * NOTE: as this init is global to ALL applications before main(),
 * care must be taken to not write to STDOUT or STDERR or app output
 * may be corrupted, signals can be raised, or worse.
 *
 */

#include "common.h"

#include <algorithm>
#include <cctype>
#include <map>
#include <set>
#include <string>

#include "bu/defines.h"
#include "bu/app.h"
#include "bu/dylib.h"
#include "bu/file.h"
#include "bu/str.h"
#include "bu/vls.h"
#include "ged.h"

#include "./include/plugin.h"

static std::map<std::string, const struct ged_cmd *> ged_cmd_map;
static size_t cmd_list_len = 0;
static char **cmd_list = NULL;
void *ged_cmds;

static std::set<void *> ged_handles;
struct bu_vls *ged_init_msg_str;

const char *
ged_init_msgs()
{
    return bu_vls_cstr(ged_init_msg_str);
}

/* If func is NULL, just see if the string has a ged_cmd_map entry.
 * If func is defined, see if a) func and cmd have ged_cmd_map entries and
 * b) if they both do, whether they map to the same function. */
int
ged_cmd_valid(const char *cmd, const char *func)
{
    if (!cmd) {
	return 1;
    }
    int cmd_invalid = 1;
    std::string scmd(cmd);
    std::map<std::string, const struct ged_cmd *>::iterator cmd_it = ged_cmd_map.find(scmd);
    if (cmd_it != ged_cmd_map.end()) {
	cmd_invalid = 0;
    }
    if (cmd_invalid) {
	return cmd_invalid;
    }
    ged_func_ptr c1 = cmd_it->second->i->cmd;
    std::map<std::string, const struct ged_cmd *>::iterator func_it = ged_cmd_map.find(std::string(func));
    if (func_it == ged_cmd_map.end()) {
	// func not in table, nothing to validate against - return invalid
	return 1;
    }
    ged_func_ptr c2 = func_it->second->i->cmd;
    int mismatched_functions = 2;
    if (c1 == c2) {
	mismatched_functions = 0;
    }
    return mismatched_functions;
}

size_t
ged_cmd_list(const char * const **cl)
{
    if (!cmd_list) {
	bu_argv_free(cmd_list_len, (char **)cmd_list);
	cmd_list_len = 0;
    }
    cmd_list = (char **)bu_calloc(ged_cmd_map.size(), sizeof(char *), "ged cmd argv");
    std::map<std::string, const struct ged_cmd *>::iterator m_it;
    for (m_it = ged_cmd_map.begin(); m_it != ged_cmd_map.end(); m_it++) {
	const char *str = m_it->first.c_str();
	cmd_list[cmd_list_len] = bu_strdup(str);
	cmd_list_len++;
    }
    (*cl) = (const char * const *)cmd_list;
    return cmd_list_len;
}

static void
libged_init(void)
{

    BU_GET(ged_init_msg_str, struct bu_vls);
    bu_vls_init(ged_init_msg_str);

    const char *ppath = bu_dir(NULL, 0, BU_DIR_LIBEXEC, "ged", NULL);
    char **filenames;
    struct bu_vls plugin_pattern = BU_VLS_INIT_ZERO;
    bu_vls_sprintf(&plugin_pattern, "*%s", GED_PLUGIN_SUFFIX);
    size_t nfiles = bu_file_list(ppath, bu_vls_cstr(&plugin_pattern), &filenames);
    for (size_t i = 0; i < nfiles; i++) {
	char pfile[MAXPATHLEN] = {0};
	bu_dir(pfile, MAXPATHLEN, BU_DIR_LIBEXEC, "ged", filenames[i], NULL);
	void *dl_handle;
	dl_handle = bu_dlopen(pfile, BU_RTLD_NOW);
	if (!dl_handle) {
	    const char * const error_msg = bu_dlerror();
	    if (error_msg)
		bu_vls_printf(ged_init_msg_str, "%s\n", error_msg);

	    bu_vls_printf(ged_init_msg_str, "Unable to dynamically load '%s' (skipping)\n", pfile);
	    continue;
	}
	{
	    const char *psymbol = "ged_plugin_info";
	    void *info_val = bu_dlsym(dl_handle, psymbol);
	    const struct ged_plugin *(*plugin_info)() = (const struct ged_plugin *(*)())(intptr_t)info_val;
	    if (!plugin_info) {
		const char * const error_msg = bu_dlerror();

		if (error_msg)
		    bu_vls_printf(ged_init_msg_str, "%s\n", error_msg);

		bu_vls_printf(ged_init_msg_str, "Unable to load symbols from '%s' (skipping)\n", pfile);
		bu_vls_printf(ged_init_msg_str, "Could not find '%s' symbol in plugin\n", psymbol);
		bu_dlclose(dl_handle);
		continue;
	    }

	    const struct ged_plugin *plugin = plugin_info();

	    if (!plugin || !plugin->cmds) {
		bu_vls_printf(ged_init_msg_str, "Invalid plugin encountered from '%s' (skipping)\n", pfile);
		bu_dlclose(dl_handle);
		continue;
	    }

	    if (!plugin->cmd_cnt) {
		bu_vls_printf(ged_init_msg_str, "Plugin '%s' contains no commands, (skipping)\n", pfile);
		bu_dlclose(dl_handle);
		continue;
	    }

	    const struct ged_cmd **cmds = plugin->cmds;
	    for (int c = 0; c < plugin->cmd_cnt; c++) {
		const struct ged_cmd *cmd = cmds[c];
		std::string key(cmd->i->cname);
		if (ged_cmd_map.find(key) != ged_cmd_map.end()) {
		    bu_vls_printf(ged_init_msg_str, "Warning - plugin '%s' provides command '%s' but that command has already been loaded, skipping\n", pfile, cmd->i->cname);
		    continue;
		}
		ged_cmd_map[key] = cmd;

		// MGED calls many of these commands with an _mged_ prefix - allow for that
		std::string mged_key = std::string("_mged_") + key;
		ged_cmd_map[mged_key] = cmd;
	    }
	    ged_handles.insert(dl_handle);
	}
    }

    ged_cmds = (void *)&ged_cmd_map;
}


static void
libged_clear(void)
{
    ged_cmd_map.clear();
    std::set<void *>::iterator h_it;
    for (h_it = ged_handles.begin(); h_it != ged_handles.end(); h_it++) {
	void *handle = *h_it;
	bu_dlclose(handle);
    }
    ged_handles.clear();

    bu_vls_free(ged_init_msg_str);
    BU_PUT(ged_init_msg_str, struct bu_vls);
}


struct libged_initializer {
    /* constructor */
    libged_initializer() {
	libged_init();
    }
    /* destructor */
    ~libged_initializer() {
	libged_clear();
    }
};

static libged_initializer LIBGED;



// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
