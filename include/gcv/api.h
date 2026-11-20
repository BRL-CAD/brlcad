/*                       G C V _ A P I . H
 * BRL-CAD
 *
 * Copyright (c) 2008-2026 United States Government as represented by
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
/** @file gcv/api.h
 *
 * Main public API of the LIBGCV geometry conversion library.
 *
 */

#ifndef GCV_API_H
#define GCV_API_H

#include "gcv/runtime.h"
#include "bu/mime.h"

__BEGIN_DECLS

/**
 * Input/Output/Translation filter.
 */
struct gcv_filter {
    /* name allowing unique identification by users */
    const char * const name;

    /* operation type */
    const enum gcv_filter_type filter_type;

    /** DEPRECATED (7.48): Declare supported MIME names
     * in gcv_handler.formats. Retained for the legacy plugin ABI;
     * modern handlers ignore this field. */
    const bu_mime_model_t mime_type;

    /** DEPRECATED (7.48): Use a bounded gcv_handler.probe
     * for recognition and MIME capabilities for handler selection. */
    int (* const data_supported)(const char *data);

    /**
     * PRIVATE
     *
     * Allocate and initialize a bu_opt_desc block and associated memory for
     * storing the option data.
     *
     * Must set *options_desc and *options_data.
     *
     * May be NULL.
     */
    void (* const create_opts_fn)(struct bu_opt_desc **options_desc, void **options_data);

    /**
     * PRIVATE
     *
     * Free the filter-specific opaque pointer returned by create_opts_fn().
     * NULL if and only if 'create_opts_fn' is NULL.
     */
    void (* const free_opts_fn)(void *options_data);

    /*
     * PRIVATE
     *
     * Perform the filter operation.
     *
     * For filters of type GCV_FILTER_WRITE, context->dbip is marked read-only
     * (the pointer is to a non-const struct db_i due to in-memory data that
     * may be modified). Filters of type GCV_FILTER_FILTER receive a NULL target.
     * For other filters, 'target' is the input or output file path.
     *
     * 'gcv_options' will always be a pointer to a valid struct gcv_opts.
     * 'options_data' is NULL if and only if 'create_opts_fn' is NULL.
     */
    int (* const filter_fn)(struct gcv_context *context, const struct gcv_opts *gcv_options, const void *options_data, const char *target);

};


/** DEPRECATED (7.48): Export gcv_plugin_info_v2 returning
 * gcv_plugin_v2 instead of the legacy gcv_plugin_info descriptor. */
struct gcv_plugin {
    const struct gcv_filter * const * const filters;
};

/** DEPRECATED (7.48): Use gcv_select_filter for MIME names
 * or gcv_resolve_filter for explicit formats and content recognition.
 * Return NULL if no matching filter is available. */
 GCV_EXPORT const struct gcv_filter*
 find_filter(enum gcv_filter_type filter_type, bu_mime_model_t mime_type, const char *data, struct gcv_context *context);
 

__END_DECLS

#endif
