#include "common.h"
#include <string.h>

#include "bu/app.h"
#include "bu/dylib.h"
#include "bu/file.h"
#include "bu/log.h"
#include "bu/ptbl.h"
#include "bu/vls.h"

#include "dm.h"
#include "./include/private.h"

int
dm_load_backends(struct bu_ptbl *plugins, struct bu_ptbl *handles)
{
    const char *ppath = bu_dir(NULL, 0, BU_DIR_LIBEXEC, "dm", NULL);
    char **filenames;
    const char *psymbol = "dm_plugin_info";
    struct bu_vls plugin_pattern = BU_VLS_INIT_ZERO;
    bu_vls_sprintf(&plugin_pattern, "*%s", DM_PLUGIN_SUFFIX);
    size_t nfiles = bu_file_list(ppath, bu_vls_cstr(&plugin_pattern), &filenames);
    for (size_t i = 0; i < nfiles; i++) {
	char pfile[MAXPATHLEN] = {0};
	bu_dir(pfile, MAXPATHLEN, BU_DIR_LIBEXEC, "dm", filenames[i], NULL);
	void *dl_handle, *info_val;
	if (!(dl_handle = bu_dlopen(pfile, BU_RTLD_NOW))) {
	    const char * const error_msg = bu_dlerror();
	    if (error_msg)
		bu_log("%s\n", error_msg);

	    bu_log("Unable to dynamically load '%s' (skipping)\n", pfile);
	    continue;
	}
	if (handles) {
	    bu_ptbl_ins(handles, (long *)dl_handle);
	}
	info_val = bu_dlsym(dl_handle, psymbol);
	const struct dm_plugin *(*plugin_info)() = (const struct dm_plugin *(*)())(intptr_t)info_val;
	if (!plugin_info) {
	    const char * const error_msg = bu_dlerror();

	    if (error_msg)
		bu_log("%s\n", error_msg);

	    bu_log("Unable to load symbols from '%s' (skipping)\n", pfile);
	    bu_log("Could not find '%s' symbol in plugin\n", psymbol);
	    continue;
	}

	const struct dm_plugin *plugin = plugin_info();

	if (!plugin || !plugin->p) {
	    bu_log("Invalid plugin encountered from '%s' (skipping)\n", pfile);
	    continue;
	}

	const struct dm *d = plugin->p;
	bu_ptbl_ins(plugins, (long *)d);
    }

    return BU_PTBL_LEN(plugins);
}

int
dm_close_backends(struct bu_ptbl *handles)
{
    int ret = 0;
    for (size_t i = 0; i < BU_PTBL_LEN(handles); i++) {
	if (bu_dlclose((void *)BU_PTBL_GET(handles, i))) {
	    ret = 1;
	}
    }

    return ret;
}


struct dm *
dm_popen(void *interp, const char *type, int argc, const char *argv[])
{
    struct dm *dmp = DM_NULL;

    struct bu_ptbl plugins = BU_PTBL_INIT_ZERO;
    struct bu_ptbl handles = BU_PTBL_INIT_ZERO;
    int dm_cnt = dm_load_backends(&plugins, &handles);
    if (!dm_cnt) {
	bu_log("No display manager implementations found!\n");
	return DM_NULL;
    }

    for (size_t i = 0; i < BU_PTBL_LEN(&plugins); i++) {
	const struct dm *d = (const struct dm *)BU_PTBL_GET(&plugins, i);
	if (BU_STR_EQUIV(type, dm_get_name(d))) {
	    dmp = d->i->dm_open(interp, argc, argv);
	    break;
	}
    }
    bu_ptbl_free(&plugins);

    if (dm_close_backends(&handles)) {
	bu_log("bu_dlclose failed to unload plugins.\n");
    }
    bu_ptbl_free(&handles);

    return dmp;
}


void
dm_list_backends(const char *separator)
{
    struct bu_vls *list;
    BU_GET(list, struct bu_vls);
    bu_vls_init(list);
    bu_vls_trunc(list, 0);

    struct bu_ptbl plugins = BU_PTBL_INIT_ZERO;
    struct bu_ptbl handles = BU_PTBL_INIT_ZERO;
    int dm_cnt = dm_load_backends(&plugins, &handles);
    if (!dm_cnt) {
	bu_log("No display manager implementations found!\n");
	return;
    }

    // We've got something, and may need a separator
    struct bu_vls sep = BU_VLS_INIT_ZERO;
    if (!separator) {
	bu_vls_sprintf(&sep, " ");
    } else {
	bu_vls_sprintf(&sep, "%s", separator);
    }

    for (size_t i = 0; i < BU_PTBL_LEN(&plugins); i++) {
	const struct dm *d = (const struct dm *)BU_PTBL_GET(&plugins, i);
	if (strlen(bu_vls_cstr(list)) > 0) bu_vls_printf(list, "%s", bu_vls_cstr(&sep));
	bu_vls_printf(list, "%s", dm_get_name(d));
    }
    bu_ptbl_free(&plugins);

    if (dm_close_backends(&handles)) {
	bu_log("bu_dlclose failed to unload plugins.\n");
    }
    bu_ptbl_free(&handles);

    bu_log("%s\n", bu_vls_cstr(list));
    bu_vls_free(list);
    BU_PUT(list, struct bu_vls);
}

int
dm_valid_type(const char *name, const char *dpy_string)
{
    struct bu_ptbl plugins = BU_PTBL_INIT_ZERO;
    struct bu_ptbl handles = BU_PTBL_INIT_ZERO;
    int dm_cnt = dm_load_backends(&plugins, &handles);
    if (!dm_cnt) {
	bu_log("No display manager implementations found!\n");
	return 0;
    }

    int is_valid = 0;

    for (size_t i = 0; i < BU_PTBL_LEN(&plugins); i++) {
	const struct dm *d = (const struct dm *)BU_PTBL_GET(&plugins, i);
	if (BU_STR_EQUIV(name, dm_get_name(d))) {
	    if (d->i->dm_viable(dpy_string) != 1) {
		bu_log("WARNING: found matching plugin %s, but viability test failed - skipping.\n", dm_get_name(d));
	    } else {
		is_valid = 1;
		break;
	    }
	}
    }
    bu_ptbl_free(&plugins);
    if (dm_close_backends(&handles)) {
	bu_log("bu_dlclose failed to unload plugins.\n");
    }
    bu_ptbl_free(&handles);

    return is_valid;
}

/** dm_recommend_type determines what mged will normally
  * use as the default display manager
  */
const char *
dm_recommend_type(const char *dpy_string)
{
    static const char *priority_list[] = {"osgl", "wgl", "ogl", "X", "tk", "nu"};

    struct bu_ptbl plugins = BU_PTBL_INIT_ZERO;
    struct bu_ptbl handles = BU_PTBL_INIT_ZERO;
    int dm_cnt = dm_load_backends(&plugins, &handles);
    if (!dm_cnt) {
	bu_log("No display manager implementations found!\n");
	return NULL;
    }

    const char *ret = NULL;

    int i = 0;
    const char *b = priority_list[i];
    while (!BU_STR_EQUAL(b, "nu")) {
	for (size_t j = 0; j < BU_PTBL_LEN(&plugins); j++) {
	    const struct dm *d = (const struct dm *)BU_PTBL_GET(&plugins, j);
	    if (BU_STR_EQUIV(b, dm_get_name(d))) {
		if (d->i->dm_viable(dpy_string) == 1) {
		    ret = b;
		    break;
		}
	    }
	}
	if (ret) {
	    break;
	}    
	i++;
	b = priority_list[i];
    }

    bu_ptbl_free(&plugins);
    if (dm_close_backends(&handles)) {
	bu_log("bu_dlclose failed to unload plugins.\n");
    }
    bu_ptbl_free(&handles);

    return (ret) ? ret : b;
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
