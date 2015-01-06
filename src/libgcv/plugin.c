/*                        P L U G I N . C
 * BRL-CAD
 *
 * Copyright (c) 2015 United States Government as represented by
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
/** @file plugin.c
 *
 * Loading and registration of GCV conversion plugins.
 *
 */


#include "common.h"

#include "plugin.h"

#include <string.h>

#include "gcv.h"


struct gcv_plugin {
    struct bu_list l;

    char *path;
    void *dl_handle;

    struct gcv_plugin_info info;
};


HIDDEN inline struct bu_list *
gcv_get_plugin_list(void)
{
    static struct bu_list plugin_list = BU_LIST_INIT_ZERO;

    if (!BU_LIST_IS_INITIALIZED(&plugin_list))
	BU_LIST_INIT(&plugin_list);

    return &plugin_list;
}


HIDDEN void
gcv_create_plugin(const char *path, void *dl_handle,
		  const struct gcv_plugin_info *info)
{
    struct bu_list * const plugin_list = gcv_get_plugin_list();

    struct gcv_plugin *plugin;

    BU_GET(plugin, struct gcv_plugin);
    BU_LIST_PUSH(plugin_list, &plugin->l);

    plugin->path = path ? bu_strdup(path) : NULL;
    plugin->dl_handle = dl_handle;
    plugin->info.file_extensions = bu_strdup(info->file_extensions);
    plugin->info.importer = info->importer;
    plugin->info.exporter = info->exporter;
}


HIDDEN void
gcv_free_plugin(struct gcv_plugin *entry)
{
    BU_LIST_DEQUEUE(&entry->l);

    if (entry->path)
	bu_free(entry->path, "path");

    if (entry->dl_handle && bu_dlclose(entry->dl_handle))
	bu_bomb(bu_dlerror());

    bu_free(entry->info.file_extensions, "file_extensions");
    BU_PUT(entry, struct gcv_plugin);
}


HIDDEN int
gcv_extension_match(const char *path, const char *extension)
{
    path = strrchr(path, '.');

    if (!path++) return 0;

    while (1) {
	const char * const next = strchr(extension, ';');

	if (!next) break;

	if (bu_strncasecmp(path, extension, next - 1 - extension) == 0)
	    return 1;

	extension = next + 1;
    }

    return bu_strcasecmp(path, extension) == 0;
}


HIDDEN const struct gcv_plugin *
gcv_plugin_find(const char *path, int importing)
{
    const struct bu_list * const plugin_list = gcv_get_plugin_list();

    struct gcv_plugin *entry;

    for (BU_LIST_FOR(entry, gcv_plugin, plugin_list))
	if (gcv_extension_match(path, entry->info.file_extensions))
	    if ((importing && entry->info.importer) || entry->info.exporter)
		return entry;

    return NULL;
}


int
gcv_load_plugin(const char *path)
{
    void *dl_handle = bu_dlopen(path, BU_RTLD_LAZY);
    const struct gcv_plugin_info *plugin_info;

    if (!dl_handle) return 0;

    plugin_info = (const struct gcv_plugin_info *)bu_dlsym(dl_handle,
		  "gcv_plugin_info");

    if (!plugin_info) {
	bu_log("failed loading gcv_plugin_info for '%s': %s\n", path, bu_dlerror());

	if (bu_dlclose(dl_handle))
	    bu_bomb(bu_dlerror());

	return 0;
    }

    gcv_create_plugin(path, dl_handle, plugin_info);

    return 1;
}


void
gcv_unload_plugin(const char *path)
{
    struct bu_list * const plugin_list = gcv_get_plugin_list();

    struct gcv_plugin *entry;

    for (BU_LIST_FOR(entry, gcv_plugin, plugin_list))
	if (bu_strcmp(entry->path, path) == 0) {
	    gcv_free_plugin(entry);
	    return;
	}
}


void
gcv_register_plugin(const struct gcv_plugin_info *info)
{
    gcv_create_plugin(NULL, NULL, info);
}


struct db_i *
gcv_import(const char *path, const struct gcv_conversion_configuration *config)
{
    const struct gcv_plugin *plugin = gcv_plugin_find(path, 1);

    struct db_i *dbip;
    struct rt_wdb *wdbp;


    if (!plugin) {
	bu_log("no plugin found to import '%s'\n", path);
	return NULL;
    }

    dbip = db_create_inmem();
    wdbp = wdb_dbopen(dbip, RT_WDB_TYPE_DB_INMEM_APPEND_ONLY);

    if (!plugin->info.importer(path, wdbp, config)) {
	wdb_close(wdbp);
	db_close(dbip);
	return NULL;
    }

    wdb_close(wdbp);
    return dbip;
}


int
gcv_export(const char *path, const struct db_i *dbip,
	   const struct gcv_conversion_configuration *config)
{
    const struct gcv_plugin *plugin = gcv_plugin_find(path, 0);

    if (!plugin) {
	bu_log("no plugin found to export '%s'\n", path);
	return 0;
    }

    return !!plugin->info.exporter(path, dbip, config);
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
