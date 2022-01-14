/*                     D M _ I N I T . C P P
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
#include "bu/malloc.h"
#include "bu/str.h"
#include "bu/vls.h"

#include "./include/private.h"

static std::map<std::string, const struct dm *> dm_map;
static std::map<std::string, const struct fb *> fb_map;
void *dm_backends;
void *fb_backends;

static std::set<void *> dm_handles;
struct bu_vls *dm_init_msg_str;

const char *
dm_init_msgs()
{
    return bu_vls_cstr(dm_init_msg_str);
}


static void
libdm_init(void)
{

    BU_GET(dm_init_msg_str, struct bu_vls);
    bu_vls_init(dm_init_msg_str);

    const char *ppath = bu_dir(NULL, 0, BU_DIR_LIBEXEC, "dm", NULL);
    char **dm_filenames;
    struct bu_vls plugin_pattern = BU_VLS_INIT_ZERO;
    bu_vls_sprintf(&plugin_pattern, "*%s", DM_PLUGIN_SUFFIX);
    size_t dm_nfiles = bu_file_list(ppath, bu_vls_cstr(&plugin_pattern), &dm_filenames);
    for (size_t i = 0; i < dm_nfiles; i++) {
	char pfile[MAXPATHLEN] = {0};
	bu_dir(pfile, MAXPATHLEN, BU_DIR_LIBEXEC, "dm", dm_filenames[i], NULL);
	void *dl_handle;
	if (!(dl_handle = bu_dlopen(pfile, BU_RTLD_NOW))) {
	    const char * const error_msg = bu_dlerror();
	    if (error_msg)
		bu_vls_printf(dm_init_msg_str, "%s\n", error_msg);

	    bu_vls_printf(dm_init_msg_str, "Unable to dynamically load '%s' (skipping)\n", pfile);
	    continue;
	}
	{
	    const char *psymbol = "dm_plugin_info";
	    void *info_val = bu_dlsym(dl_handle, psymbol);
	    const struct dm_plugin *(*plugin_info)() = (const struct dm_plugin *(*)())(intptr_t)info_val;
	    if (!plugin_info) {
		const char * const error_msg = bu_dlerror();

		if (error_msg)
		    bu_vls_printf(dm_init_msg_str, "%s\n", error_msg);

		bu_vls_printf(dm_init_msg_str, "Unable to load symbols from '%s' (skipping)\n", pfile);
		bu_vls_printf(dm_init_msg_str, "Could not find '%s' symbol in plugin\n", psymbol);
		bu_dlclose(dl_handle);
		continue;
	    }

	    const struct dm_plugin *plugin = plugin_info();

	    if (!plugin) {
		bu_vls_printf(dm_init_msg_str, "Invalid plugin encountered from '%s' (skipping)\n", pfile);
		bu_dlclose(dl_handle);
		continue;
	    }

	    if (*((const uint32_t *)(plugin)) != (uint32_t)(DM_API)) {
		bu_vls_printf(dm_init_msg_str, "Plugin version %d of '%s' differs from %d (skipping)\n", *((const uint32_t *)(plugin)), pfile, DM_API);
		bu_dlclose(dl_handle);
		continue;
	    }

	    if (!plugin->p) {
		bu_vls_printf(dm_init_msg_str, "Invalid plugin encountered from '%s' (skipping)\n", pfile);
		bu_dlclose(dl_handle);
		continue;
	    }

	    const struct dm *d = plugin->p;
	    const char *dname = dm_get_name(d);
	    if (!dname) {
		bu_vls_printf(dm_init_msg_str, "Warning - file '%s' does not provide a display manager name (?), skipping\n", pfile);
		bu_dlclose(dl_handle);
		continue;
	    }
	    std::string key(dname);
	    std::transform(key.begin(), key.end(), key.begin(), [](unsigned char c){ return std::tolower(c); });
	    if (dm_map.find(key) != dm_map.end()) {
		bu_vls_printf(dm_init_msg_str, "Warning - file '%s' provides backend '%s' but that backend has already been loaded, skipping\n", pfile, dname);
		bu_dlclose(dl_handle);
		continue;
	    }
	    dm_handles.insert(dl_handle);
	    dm_map[key] = d;
	}

	// If we've gotten this far, we have a dm - see if we also have a framebuffer */
	{
	    const char *psymbol = "fb_plugin_info";
	    void *info_val = bu_dlsym(dl_handle, psymbol);
	    const struct fb_plugin *(*plugin_info)() = (const struct fb_plugin *(*)())(intptr_t)info_val;
	    if (!plugin_info) {
		const char * const error_msg = bu_dlerror();

		if (error_msg)
		    bu_vls_printf(dm_init_msg_str, "%s\n", error_msg);

		bu_vls_printf(dm_init_msg_str, "Unable to load symbols from '%s' (skipping)\n", pfile);
		bu_vls_printf(dm_init_msg_str, "Could not find '%s' symbol in plugin\n", psymbol);
		continue;
	    }

	    const struct fb_plugin *plugin = plugin_info();

	    if (!plugin || !plugin->p) {
		bu_vls_printf(dm_init_msg_str, "Invalid plugin encountered from '%s' (skipping)\n", pfile);
		continue;
	    }

	    const struct fb *f = plugin->p;
	    const char *fname = fb_get_name(f);
	    std::string key(fname);
	    if (fb_map.find(key) != fb_map.end()) {
		bu_vls_printf(dm_init_msg_str, "Warning - file '%s' provides framebuffer backend '%s' but that backend has already been loaded, skipping\n", pfile, fname);
		continue;
	    }
	    fb_map[key] = f;
	}


    }
    bu_argv_free(dm_nfiles, dm_filenames);
    bu_vls_free(&plugin_pattern);

    dm_backends = (void *)&dm_map;

    // Populate the built-in if_* backends
    std::string nullkey("/dev/null");
    fb_map[nullkey] = &fb_null_interface;
    std::string debugkey("/dev/debug");
    fb_map[debugkey] = &debug_interface;
    std::string memkey("/dev/mem");
    fb_map[memkey] = &memory_interface;
    std::string stackkey("/dev/stack");
    fb_map[stackkey] = &stk_interface;

    fb_backends = (void *)&fb_map;
}


static void
libdm_clear(void)
{
    dm_map.clear();
    std::set<void *>::iterator h_it;
    for (h_it = dm_handles.begin(); h_it != dm_handles.end(); h_it++) {
	void *handle = *h_it;
	bu_dlclose(handle);
    }
    dm_handles.clear();

    bu_vls_free(dm_init_msg_str);
    BU_PUT(dm_init_msg_str, struct bu_vls);
}


struct libdm_initializer {
    /* constructor */
    libdm_initializer() {
	libdm_init();
    }
    /* destructor */
    ~libdm_initializer() {
	libdm_clear();
    }
};

static libdm_initializer LIBDM;



// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
