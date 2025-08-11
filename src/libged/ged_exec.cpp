/*                  G E D _ E X E C . C P P
 * BRL-CAD
 *
 * Copyright (c) 2020-2025 United States Government as represented by
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
/** @file ged_exec.cpp
 *
 * Executable to support allowing LIBGED commands to launch processes
 * to execute functionality that might have to be paused or terminated
 * without interruption of the parent process.
 *
 */

#include "common.h"

#include <map>
#include <string>

#include "bu/app.h"
#include "bu/dylib.h"
#include "bu/path.h"
#include "bu/process.h"
#include "bu/vls.h"
#include "ged.h"
#include "./include/plugin.h"

extern "C" int
main(int argc, const char *argv[])
{
    bu_setprogname(argv[0]);
    argc--; argv++;

    if (argc < 1 || !argv) {
	return BRLCAD_ERROR;
    }

    char pfile[MAXPATHLEN] = {0};
    bu_dir(pfile, MAXPATHLEN, BU_DIR_LIBEXEC, "ged_exec", argv[0], BU_DIR_LIBEXT, NULL);

    if (!bu_file_exists(pfile, NULL))
	return BRLCAD_ERROR;

    void *dl_handle = bu_dlopen(pfile, BU_RTLD_NOW);
    if (!dl_handle) {
	const char * const error_msg = bu_dlerror();
	bu_log("Unable to dynamically load '%s'\n", pfile);
	if (error_msg)
	    bu_log("%s\n", error_msg);
	return BRLCAD_ERROR;
    }

    const char *psymbol = "ged_process_info";
    void *info_val = bu_dlsym(dl_handle, psymbol);
    const struct ged_process_plugin *(*plugin_info)() = (const struct ged_process_plugin *(*)())(intptr_t)info_val;
    if (!plugin_info) {
	const char * const error_msg = bu_dlerror();
	if (error_msg)
	    bu_log("%s\n", error_msg);
	bu_log("Unable to load symbols from '%s' (skipping)\n", pfile);
	bu_log("Could not find '%s' symbol in plugin\n", psymbol);
	bu_dlclose(dl_handle);
	return BRLCAD_ERROR;
    }

    const struct ged_process_plugin *plugin = plugin_info();
    if (!plugin) {
	bu_log("Invalid plugin file '%s' encountered\n", pfile);
	bu_dlclose(dl_handle);
	return BRLCAD_ERROR;
    }

    if (*((const uint32_t *)(plugin)) != (uint32_t)  (GED_API)) {
	bu_log("Plugin version %d of '%s' differs from %d\n", *((const uint32_t *)(plugin)), pfile, GED_API);
	bu_dlclose(dl_handle);
	return BRLCAD_ERROR;
    }

    if (!plugin->p) {
	bu_log("Invalid plugin file '%s' encountered\n", pfile);
	bu_dlclose(dl_handle);
	return BRLCAD_ERROR;
    }

    const struct ged_cmd_process *p = plugin->p;
    int ret =  p->i->func(argc, argv);
    bu_dlclose(dl_handle);
    return ret;
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
