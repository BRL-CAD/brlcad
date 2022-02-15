/*                     G E D _ I N I T . C P P
 * BRL-CAD
 *
 * Copyright (c) 2019-2022 United States Government as represented by
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

static char **cmd_list = NULL;
static size_t cmd_list_len = 0;
static std::map<std::string, const struct ged_cmd *> ged_cmd_map;
static std::set<void *> ged_cmd_funcs;
static struct bu_vls init_msgs = BU_VLS_INIT_ZERO;
void *ged_cmds;

extern "C" void libged_init(void);


const char *
ged_init_msgs()
{
    return bu_vls_cstr(&init_msgs);
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

    // On OpenBSD, if the executable was launched in a way that requires
    // bu_setprogname to find the BRL-CAD root directory the iniital libged
    // initialization would have failed.  If we have no ged_cmds at all this is
    // probably what happened, so call libged_init again here.  By the time we
    // are calling ged_cmd_valid bu_setprogname should be set and we should be
    // ready to actually find the commands.
    if (!ged_cmd_map.size()) {
	libged_init();
    }

    std::string scmd(cmd);
    std::map<std::string, const struct ged_cmd *>::iterator cmd_it = ged_cmd_map.find(scmd);
    if (cmd_it != ged_cmd_map.end()) {
	cmd_invalid = 0;
    }
    if (cmd_invalid) {
	return cmd_invalid;
    }

    if (func) {
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

    return 0;
}


/* Use bu_editdist to see if there is a command name similar to cmd
 * defined.  Return the closest match and the edit distance.  (0 indicates
 * an exact match, -1 an error) */
int
ged_cmd_lookup(const char **ncmd, const char *cmd)
{
    if (!cmd || !ncmd) {
	return -1;
    }
    unsigned long min_dist = LONG_MAX;

    // On OpenBSD, if the executable was launched in a way that requires
    // bu_setprogname to find the BRL-CAD root directory the iniital libged
    // initialization would have failed.  If we have no ged_cmds at all this is
    // probably what happened, so call libged_init again here.  By the time we
    // are calling ged_cmd_valid bu_setprogname should be set and we should be
    // ready to actually find the commands.
    if (!ged_cmd_map.size()) {
	libged_init();
    }

    const char *ccmd = NULL;
    std::string scmd(cmd);
    std::map<std::string, const struct ged_cmd *>::iterator cmd_it;
    for (cmd_it = ged_cmd_map.begin(); cmd_it != ged_cmd_map.end(); cmd_it++) {
	unsigned long edist = bu_editdist(cmd, cmd_it->first.c_str());
	if (edist < min_dist) {
	    ccmd = (*cmd_it).first.c_str();
	    min_dist = edist;
	}
    }

    (*ncmd) = ccmd;
    return (int)min_dist;
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


extern "C" void
libged_init(void)
{
    const char *ppath = bu_dir(NULL, 0, BU_DIR_LIBEXEC, "ged", NULL);
    char **ged_filenames;
    struct bu_vls plugin_pattern = BU_VLS_INIT_ZERO;
    bu_vls_sprintf(&plugin_pattern, "*%s", GED_PLUGIN_SUFFIX);
    size_t ged_nfiles = bu_file_list(ppath, bu_vls_cstr(&plugin_pattern), &ged_filenames);
    for (size_t i = 0; i < ged_nfiles; i++) {
	char pfile[MAXPATHLEN] = {0};
	bu_dir(pfile, MAXPATHLEN, BU_DIR_LIBEXEC, "ged", ged_filenames[i], NULL);
	void *dl_handle;
	dl_handle = bu_dlopen(pfile, BU_RTLD_NOW);
	if (!dl_handle) {
	    const char * const error_msg = bu_dlerror();
	    if (error_msg)
		bu_vls_printf(&init_msgs, "%s\n", error_msg);

	    bu_vls_printf(&init_msgs, "Unable to dynamically load '%s' (skipping)\n", pfile);
	    continue;
	}
	{
	    const char *psymbol = "ged_plugin_info";
	    void *info_val = bu_dlsym(dl_handle, psymbol);
	    const struct ged_plugin *(*plugin_info)() = (const struct ged_plugin *(*)())(intptr_t)info_val;
	    if (!plugin_info) {
		const char * const error_msg = bu_dlerror();

		if (error_msg)
		    bu_vls_printf(&init_msgs, "%s\n", error_msg);

		bu_vls_printf(&init_msgs, "Unable to load symbols from '%s' (skipping)\n", pfile);
		bu_vls_printf(&init_msgs, "Could not find '%s' symbol in plugin\n", psymbol);
		bu_dlclose(dl_handle);
		continue;
	    }

	    const struct ged_plugin *plugin = plugin_info();

	    if (!plugin) {
		bu_vls_printf(&init_msgs, "Invalid plugin file '%s' encountered (skipping)\n", pfile);
		bu_dlclose(dl_handle);
		continue;
	    }

	    if (*((const uint32_t *)(plugin)) != (uint32_t)  (GED_API)) {
		bu_vls_printf(&init_msgs, "Plugin version %d of '%s' differs from %d (skipping)\n", *((const uint32_t *)(plugin)), pfile, GED_API);
		bu_dlclose(dl_handle);
		continue;
	    }

	    if (!plugin->cmds) {
		bu_vls_printf(&init_msgs, "Invalid plugin file '%s' encountered (skipping)\n", pfile);
		bu_dlclose(dl_handle);
		continue;
	    }

	    if (!plugin->cmd_cnt) {
		bu_vls_printf(&init_msgs, "Plugin '%s' contains no commands, (skipping)\n", pfile);
		bu_dlclose(dl_handle);
		continue;
	    }

	    const struct ged_cmd **cmds = plugin->cmds;
	    for (int c = 0; c < plugin->cmd_cnt; c++) {
		const struct ged_cmd *cmd = cmds[c];
		std::string key(cmd->i->cname);
		if (ged_cmd_map.find(key) != ged_cmd_map.end()) {
		    bu_vls_printf(&init_msgs, "Warning - plugin '%s' provides command '%s' but that command has already been loaded, skipping\n", pfile, cmd->i->cname);
		    continue;
		}
		ged_cmd_map[key] = cmd;

		// MGED calls many of these commands with an _mged_ prefix - allow for that
		std::string mged_key = std::string("_mged_") + key;
		ged_cmd_map[mged_key] = cmd;
	    }
	    ged_cmd_funcs.insert(dl_handle);
	}
    }
    bu_argv_free(ged_nfiles, ged_filenames);
    bu_vls_free(&plugin_pattern);

    ged_cmds = (void *)&ged_cmd_map;
}


static void
libged_clear(void)
{
    ged_cmd_map.clear();
    std::set<void *>::iterator h_it;
#ifdef __APPLE__
    /* unload in reverse in case symbols are referential */
    for (h_it = ged_cmd_funcs.end(); h_it != ged_cmd_funcs.begin(); h_it--) {
#else
    /* doing unloading in the above order crashes on Ubuntu Linux */
    for (h_it = ged_cmd_funcs.begin(); h_it != ged_cmd_funcs.end(); h_it++) {
#endif
	void *handle = *h_it;
	bu_dlclose(handle);
    }
    ged_cmd_funcs.clear();
}


struct libged_initializer {
    /* constructor */
    libged_initializer() {
	libged_init();
    }
    /* destructor */
    ~libged_initializer() {
	libged_clear();
	bu_vls_free(&init_msgs);
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
