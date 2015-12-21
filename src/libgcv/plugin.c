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

    if (!BU_PTBL_LEN(&converter_table)) {
#define CONVERTER(name) \
	do { \
	    extern const struct gcv_converter (name); \
	    _gcv_converter_insert(&converter_table, &(name)); \
	} while (0)

	CONVERTER(gcv_conv_brlcad_read);
	CONVERTER(gcv_conv_brlcad_write);
	CONVERTER(gcv_conv_fastgen4_read);
	CONVERTER(gcv_conv_fastgen4_write);
	CONVERTER(gcv_conv_obj_read);
	CONVERTER(gcv_conv_obj_write);
	CONVERTER(gcv_conv_stl_read);
	CONVERTER(gcv_conv_stl_write);
	CONVERTER(gcv_conv_vrml_write);

#undef CONVERTER
    }

    return &converter_table;
}


struct bu_ptbl
gcv_find_converters(mime_model_t mime_type,
		    enum gcv_conversion_type conversion_type)
{
    struct bu_ptbl result;
    const struct gcv_converter * const *entry;
    const struct bu_ptbl *converters;

    if (!_gcv_mime_is_valid(mime_type))
	bu_bomb("invalid mime_type");

    converters = gcv_get_converters();

    bu_ptbl_init(&result, 8, "result");

    for (BU_PTBL_FOR(entry, (const struct gcv_converter * const *), converters))
	if ((*entry)->mime_type == mime_type)
	    if ((*entry)->conversion_type == conversion_type)
		bu_ptbl_ins(&result, (long *)*entry);

    return result;
}


void
gcv_converter_create_options(const struct gcv_converter *converter,
			     struct bu_opt_desc **options_desc, void **options_data)
{
    if (!converter || !options_desc || !options_data)
	bu_bomb("missing arguments");

    if (converter->create_opts_fn) {
	*options_desc = NULL;
	*options_data = NULL;

	converter->create_opts_fn(options_desc, options_data);

	if (!*options_desc || !*options_data)
	    bu_bomb("converter->create_opts_fn() set null result");
    } else {
	*options_desc = (struct bu_opt_desc *)bu_malloc(sizeof(struct bu_opt_desc),
							"options_desc");
	BU_OPT_NULL(**options_desc);
	*options_data = NULL;
    }
}


void
gcv_converter_free_options(const struct gcv_converter *converter,
			   void *options_data)
{
    if (!converter || (!converter->create_opts_fn != !options_data))
	bu_bomb("missing arguments");

    if (converter->create_opts_fn)
	converter->free_opts_fn(options_data);
}


int
gcv_converter_convert(const struct gcv_converter *converter, const char *path,
		      struct db_i *dbip, const struct gcv_opts *gcv_options, const void *options_data)
{
    if (!converter || !path || !dbip
	|| (!converter->create_opts_fn != !options_data))
	bu_bomb("missing arguments");

    if (gcv_options)
	bu_bomb("struct gcv_opts unimplemented");

    RT_CK_DBI(dbip);

    return converter->conversion_fn(path, dbip, gcv_options, options_data);
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
