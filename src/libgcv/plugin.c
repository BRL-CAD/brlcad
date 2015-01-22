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
#include "gcv_private.h"

#include <string.h>


struct gcv_plugin {
    struct bu_list l;

    const struct gcv_plugin_info *plugin_info;
};


static inline struct bu_list *
gcv_get_plugin_list(void)
{
    static struct bu_list plugin_list = BU_LIST_INIT_ZERO;

    if (!BU_LIST_IS_INITIALIZED(&plugin_list))
	BU_LIST_INIT(&plugin_list);

    return &plugin_list;
}


void
gcv_plugin_register(const struct gcv_plugin_info *plugin_info)
{
    struct bu_list * const plugin_list = gcv_get_plugin_list();

    struct gcv_plugin *plugin;

    BU_GET(plugin, struct gcv_plugin);
    BU_LIST_PUSH(plugin_list, &plugin->l);
    plugin->plugin_info = plugin_info;
}


HIDDEN void
gcv_plugin_free(struct gcv_plugin *entry)
{
    BU_LIST_DEQUEUE(&entry->l);
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


const struct gcv_converter *
gcv_converter_find(const char *path, enum gcv_conversion_type type)
{
    const struct bu_list * const plugin_list = gcv_get_plugin_list();

    const struct gcv_plugin *entry;

    for (BU_LIST_FOR(entry, gcv_plugin, plugin_list)) {
	const struct gcv_converter *converter = entry->plugin_info->converters;

	while (converter->file_extensions) {
	    if (gcv_extension_match(path, converter->file_extensions)) {
		if (type == GCV_CONVERSION_READ && converter->reader_fn)
		    return converter;
		else if (type == GCV_CONVERSION_WRITE && converter->writer_fn)
		    return converter;
	    }

	    ++converter;
	}
    }

    return NULL;
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
