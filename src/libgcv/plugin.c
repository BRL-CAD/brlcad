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


HIDDEN inline int
_gcv_mime_is_valid(mime_model_t mime_type)
{
    return mime_type != MIME_MODEL_AUTO && mime_type != MIME_MODEL_UNKNOWN;
}


HIDDEN void
_gcv_converter_insert(struct bu_ptbl *table,
		      const struct gcv_converter *converter)
{
    if (!converter)
	bu_bomb("null converter");

    if (!_gcv_mime_is_valid(converter->mime_type))
	bu_bomb("invalid mime_type");

    if (!converter->create_opts_fn != !converter->free_opts_fn)
	bu_bomb("must have either both or none of create_opts_fn and free_opts_fn");

    if (!converter->conversion_fn)
	bu_bomb("null conversion_fn");

    if (bu_ptbl_ins_unique(table, (long *)converter) != -1)
	bu_bomb("converter already registered");
}


const struct bu_ptbl *
gcv_get_converters(void)
{
    static struct bu_ptbl converter_table = BU_PTBL_INIT_ZERO;
    static int registered_static = 0;

    if (!registered_static) {
	registered_static = 1;

#define PLUGIN(name) \
    do { \
	extern const struct gcv_converter name; \
	_gcv_converter_insert(&converter_table, &name); \
    } while (0)

	PLUGIN(gcv_conv_brlcad_read);
	PLUGIN(gcv_conv_brlcad_write);
	PLUGIN(gcv_conv_fastgen4_read);
	PLUGIN(gcv_conv_fastgen4_write);
	PLUGIN(gcv_conv_obj_read);
	PLUGIN(gcv_conv_obj_write);
	PLUGIN(gcv_conv_stl_read);
	PLUGIN(gcv_conv_stl_write);
	PLUGIN(gcv_conv_vrml_write);

#undef PLUGIN
    }

    return &converter_table;
}


struct bu_ptbl
gcv_find_converters(mime_model_t mime_type,
		    enum gcv_conversion_type conversion_type)
{
    struct bu_ptbl result;
    struct gcv_converter **entry;
    const struct bu_ptbl *converters;

    if (!_gcv_mime_is_valid(mime_type))
	bu_bomb("invalid mime_type");

    converters = gcv_get_converters();

    bu_ptbl_init(&result, 8, "result");

    for (BU_PTBL_FOR(entry, (struct gcv_converter **), converters))
	if ((*entry)->mime_type == mime_type)
	    if ((*entry)->conversion_type == conversion_type)
		bu_ptbl_ins(&result, (long *)*entry);

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
