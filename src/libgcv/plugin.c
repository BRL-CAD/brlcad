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


struct gcv_converter_entry {
    struct bu_list l;

    struct gcv_converter converter;
};


static struct bu_list converter_list = {BU_LIST_HEAD_MAGIC, &converter_list, &converter_list};


HIDDEN void
gcv_plugin_register(const struct gcv_converter *converter)
{
    struct gcv_converter_entry *entry;

    if (converter->mime_type == MIME_MODEL_AUTO
	|| converter->mime_type == MIME_MODEL_UNKNOWN)
	bu_bomb("invalid mime_type");

    if (converter->conversion_type == GCV_CONVERSION_NONE)
	bu_bomb("invalid gcv_conversion_type");

    if (!converter->conversion_fn)
	bu_bomb("null conversion_fn");

    BU_GET(entry, struct gcv_converter_entry);
    BU_LIST_PUSH(&converter_list, &entry->l);
    entry->converter = *converter;
}


HIDDEN void
gcv_register_static(void)
{
    static int registered_static = 0;

    if (!registered_static)
	registered_static = 1;
    else
	return;

#define PLUGIN(name) \
    do { \
	extern const struct gcv_converter name; \
	gcv_plugin_register(&name); \
    } while (0)

    PLUGIN(gcv_conv_brlcad_write);
    PLUGIN(gcv_conv_brlcad_read);
    PLUGIN(gcv_conv_fastgen4_read);
    PLUGIN(gcv_conv_fastgen4_write);
    PLUGIN(gcv_conv_obj_read);
    PLUGIN(gcv_conv_obj_write);
    PLUGIN(gcv_conv_stl_read);
    PLUGIN(gcv_conv_stl_write);
    PLUGIN(gcv_conv_vrml_write);

#undef PLUGIN
}


struct bu_ptbl
gcv_converter_find(mime_model_t mime_type,
		   enum gcv_conversion_type conversion_type)
{
    struct bu_ptbl result;
    struct gcv_converter_entry *entry;

    gcv_register_static();

    bu_ptbl_init(&result, 8, "result");

    for (BU_LIST_FOR(entry, gcv_converter_entry, &converter_list)) {
	if (mime_type == MIME_MODEL_AUTO || entry->converter.mime_type == mime_type)
	    if (conversion_type == GCV_CONVERSION_NONE
		|| entry->converter.conversion_type == conversion_type)
		bu_ptbl_ins(&result, (long *)&entry->converter);
    }

    return result;
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
