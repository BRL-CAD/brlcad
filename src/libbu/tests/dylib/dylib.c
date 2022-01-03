/*                         D Y L I B . C
 * BRL-CAD
 *
 * Published in 2020 by the United States Government.
 * This work is in the public domain.
 *
 */
/** @file dylib.c
 *
 * Mechanics of loading the plugins.
 *
 */

#include "common.h"
#include <string.h>

#include "bu/app.h"
#include "bu/dylib.h"
#include "bu/file.h"
#include "bu/log.h"
#include "bu/ptbl.h"
#include "bu/vls.h"

#include "dylib.h"

int
dylib_load_plugins(struct bu_ptbl *plugins, struct bu_ptbl *dl_handles)
{
    if (!plugins || !dl_handles)
	return -1;

    const char *ppath = bu_dir(NULL, 0, BU_DIR_LIBEXEC, "dylib", NULL);
    char **filenames;
    const char *psymbol = "dylib_plugin_info";
    struct bu_vls plugin_pattern = BU_VLS_INIT_ZERO;
    bu_vls_sprintf(&plugin_pattern, "*%s", DYLIB_PLUGIN_SUFFIX);
    size_t nfiles = bu_file_list(ppath, bu_vls_cstr(&plugin_pattern), &filenames);
    for (size_t i = 0; i < nfiles; i++) {
	char pfile[MAXPATHLEN] = {0};
	bu_dir(pfile, MAXPATHLEN, BU_DIR_LIBEXEC, "dylib", filenames[i], NULL);
	void *dl_handle, *info_val;
	if (!(dl_handle = bu_dlopen(pfile, BU_RTLD_NOW))) {
	    const char * const error_msg = bu_dlerror();
	    if (error_msg)
		bu_log("%s\n", error_msg);

	    bu_log("Unable to dynamically load '%s' (skipping)\n", pfile);
	    continue;
	}

	info_val = bu_dlsym(dl_handle, psymbol);
	const struct dylib_plugin *(*plugin_info)() = (const struct dylib_plugin *(*)())(intptr_t)info_val;
	if (!plugin_info) {
	    const char * const error_msg = bu_dlerror();

	    if (error_msg)
		bu_log("%s\n", error_msg);

	    bu_dlclose(dl_handle);
	    bu_log("Unable to load symbols from '%s' (skipping)\n", pfile);
	    bu_log("Could not find '%s' symbol in plugin\n", psymbol);
	    continue;
	}

	const struct dylib_plugin *plugin = plugin_info();

	if (!plugin || !plugin->i) {
	    bu_dlclose(dl_handle);
	    bu_log("Invalid plugin encountered from '%s' (skipping)\n", pfile);
	    continue;
	}

	bu_ptbl_ins(dl_handles, (long *)dl_handle);

	const struct dylib_contents *pcontents = plugin->i;
	bu_ptbl_ins(plugins, (long *)pcontents);
    }

    return BU_PTBL_LEN(plugins);
}

int
dylib_close_plugins(struct bu_ptbl *plugins)
{
    int ret = 0;
    for (size_t i = 0; i < BU_PTBL_LEN(plugins); i++) {
	if (bu_dlclose((void *)BU_PTBL_GET(plugins, i))) {
	    ret = 1;
	}
    }

    return ret;
}

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
