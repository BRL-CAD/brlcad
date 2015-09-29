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

#include "gcv/api.h"


__BEGIN_DECLS


struct gcv_converter {
    const mime_model_t mime_type;
    const enum gcv_conversion_type conversion_type;


    /**
     * Allocates and initializes data for bu_opt argument parsing.
     * create_opts_fn and free_opts_fn must either both be NULL or both be set.
     *
     * Must set *options_desc to point to a block of bu_opt_desc's suitable for freeing with bu_free().
     * Must set *options_data to point to converter-specific options suitable for freeing with free_opts_fn().
     */
    void (* const create_opts_fn)(struct bu_opt_desc **options_desc,
				  void **options_data);

    void (* const free_opts_fn)(void *options_data);


    /**
     * Perform the conversion operation.
     * options_data must be set if create_opts_fn is set, or NULL otherwise.
     */
    int (* const conversion_fn)(const char *path, struct db_i *dbip,
				const struct gcv_opts *gcv_options, const void *options_data);
};


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
