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


struct gcv_plugin {
    struct bu_list l;

    const struct gcv_plugin_info *plugin_info;
};


static struct bu_list plugin_list = {BU_LIST_HEAD_MAGIC, &plugin_list, &plugin_list};


HIDDEN void
gcv_plugin_register(const struct gcv_plugin_info *plugin_info)
{
    struct gcv_plugin *plugin;

    BU_GET(plugin, struct gcv_plugin);
    BU_LIST_PUSH(&plugin_list, &plugin->l);
    plugin->plugin_info = plugin_info;
}


HIDDEN void
gcv_register_static(void)
{
    static int registered_static = 0;

    if (registered_static)
	return;
    else
	registered_static = 1;

#define PLUGIN(name) \
    do { \
	extern struct gcv_plugin_info name; \
	gcv_plugin_register(&name); \
    } while (0)

    PLUGIN(gcv_plugin_conv_brlcad);
    PLUGIN(gcv_plugin_conv_fastgen4_read);
    PLUGIN(gcv_plugin_conv_fastgen4_write);
    PLUGIN(gcv_plugin_conv_obj_read);
    PLUGIN(gcv_plugin_conv_obj_write);
    PLUGIN(gcv_plugin_conv_stl_read);
    PLUGIN(gcv_plugin_conv_stl_write);
    PLUGIN(gcv_plugin_conv_vrml_write);

#undef PLUGIN
}


const struct gcv_converter *
gcv_converter_find(mime_model_t mime_type, enum gcv_conversion_type type)
{
    const struct gcv_plugin *entry;

    gcv_register_static();

    for (BU_LIST_FOR(entry, gcv_plugin, &plugin_list)) {
	const struct gcv_converter *current = entry->plugin_info->converters;

	if (!current) continue;

	for (; current->mime_type != MIME_MODEL_UNKNOWN; ++current)
	    if (current->mime_type == mime_type) {
		if (type == GCV_CONVERSION_READ && current->reader_fn)
		    return current;
		else if (type == GCV_CONVERSION_WRITE && current->writer_fn)
		    return current;
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
