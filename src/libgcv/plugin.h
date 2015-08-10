/*                        P L U G I N . H
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
/** @file plugin.h
 *
 * Registration of GCV conversion plugins.
 *
 */


#ifndef PLUGIN_H
#define PLUGIN_H


#include "common.h"

#include "gcv.h"

#include "bu/opt.h"


__BEGIN_DECLS


enum gcv_conversion_type {GCV_CONVERSION_NONE, GCV_CONVERSION_READ, GCV_CONVERSION_WRITE};


struct gcv_converter {
    mime_model_t mime_type;
    enum gcv_conversion_type conversion_type;

    void (*create_opts_fn)(struct bu_opt_desc **options_desc, void **options_data);

    void (*free_opts_fn)(void *options_data);

    int (*conversion_fn)(const char *path, struct db_i *dbip,
			 const struct gcv_opts *gcv_options, const void *options_data);
};


GCV_EXPORT struct bu_ptbl gcv_converter_find(mime_model_t mime_type,
	enum gcv_conversion_type conversion_type);


__END_DECLS


#endif


/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
