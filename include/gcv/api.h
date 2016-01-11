/*                       G C V _ A P I . H
 * BRL-CAD
 *
 * Copyright (c) 2008-2016 United States Government as represented by
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
/** @file gcv_api.h
 *
 * Main public API of the LIBGCV geometry conversion library.
 *
 */

#ifndef GCV_API_H
#define GCV_API_H

#include "common.h"

#include "gcv/defines.h"

#include "bu/opt.h"
#include "raytrace.h"

__BEGIN_DECLS


/**
 * The big kahuna.
 */
struct gcv_context
{
    struct db_i *dbip;
    bu_avs_t messages;
};


/**
 * Initialize a conversion context.
 */
GCV_EXPORT void
gcv_context_init(struct gcv_context *cxt);


/**
 * Release a conversion context, freeing all memory internally
 * allocated by the library.  Caller is responsible for freeing the
 * context pointer itself, if necessary.
 */
GCV_EXPORT void
gcv_context_destroy(struct gcv_context *cxt);


enum gcv_tessellation_algorithm {
    GCV_TESS_DEFAULT = 0,
    GCV_TESS_BOTTESS,
    GCV_TESS_MARCHING_CUBES
};


enum gcv_filter_type {
    GCV_FILTER_FILTER,
    GCV_FILTER_READ,
    GCV_FILTER_WRITE
};


/**
 * Conversion options.
 */
struct gcv_opts
{
    /* Debug flags, applied via bitwise-OR. Original flags are restored after conversion. */
    int bu_debug_flag;
    uint32_t rt_debug_flag;
    uint32_t nmg_debug_flag;

    unsigned debug_mode; /* Print debugging info if debug_mode == 1 */
    unsigned verbosity_level; /* 0 is quiet, printing only errors */
    unsigned max_cpus; /* max cpus; 0 for automatic */

    struct bn_tol calculational_tolerance;
    struct rt_tess_tol tessellation_tolerance;
    enum gcv_tessellation_algorithm tessellation_algorithm;

    fastf_t scale_factor; /* conversion to units */
    const char *default_name; /* default name for nameless objects */

    size_t num_objects; /* Number of elements in object_names. If 0, convert all top-level objects */
    const char * const *object_names; /* objects to convert */
};


/**
 * Assign default option values.
 */
void gcv_opts_default(struct gcv_opts *gcv_options);


/**
 * Input/Output/Translation filter.
 */
struct gcv_filter {
    const char * const name; /* name allowing unique identification by users */
    const enum gcv_filter_type filter_type; /* operation type */
    const mime_model_t mime_type; /* MIME_MODEL_UNKNOWN if 'filter_type' is GCV_FILTER_FILTER */


    /* PRIVATE to libgcv and the associated filter plugin */

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


/**
 * Append a filter for reading objects
 */
GCV_EXPORT void
gcv_reader(struct gcv_filter *reader, const char *source, const struct gcv_opts *UNUSED(opts));


/**
 * Append a filter for writing objects.
 */
GCV_EXPORT void
gcv_writer(struct gcv_filter *writer, const char *target, const struct gcv_opts *UNUSED(opts));


/**
 * Perform a filtering operation on a gcv_context.
 *
 * If 'gcv_options' is NULL, defaults will be used as set by gcv_opts_default().
 * The parameters 'argc' and 'argv' are used for option processing as specified by
 * the filter.
 *
 * Returns 1 on success and 0 on failure.
 */
GCV_EXPORT int
gcv_execute(struct gcv_context *context, const struct gcv_filter *filter, const struct gcv_opts *gcv_options, size_t argc, const char * const *argv, const char *target);



/*
 * Return a pointer to a bu_ptbl listing all registered filters as
 * const struct gcv_filter pointers.
 */
GCV_EXPORT const struct bu_ptbl *gcv_list_filters(void);


/**
 * Convert objects from one file to another, based on the conversion
 * criteria specified (defaults to maximum data preservation).
 *
 * This provides a simplified conversion interface for applications
 * desiring maximum simplicity for reading and writing geometry data.
 * This is a shorthand for manually calling gcv_read() and gcv_write().
 *
 * Returns negative on fatal error or number of objects written
 */
GCV_EXPORT int
gcv_convert(const char *in_file, const struct gcv_opts *in_opts, const char *out_file, const struct gcv_opts *out_opts);


__END_DECLS

#endif /* GCV_API_H */

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
