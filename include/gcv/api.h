/*                       G C V _ A P I . H
 * BRL-CAD
 *
 * Copyright (c) 2008-2022 United States Government as represented by
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

#include "common.h"
#include "vmath.h"

#include "bu/avs.h"
#include "bu/opt.h"
#include "bu/mime.h"
#include "bn/tol.h"
#include "rt/defines.h"
#include "rt/tol.h"
#include "gcv/defines.h"

__BEGIN_DECLS


/**
 * The big kahuna.
 */
struct gcv_context_internal;
struct gcv_context
{
    struct db_i *dbip;
    bu_avs_t messages;
    struct gcv_context_internal *i;  // Internal information
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
    /* debug flags, applied via bitwise-OR
     * original flags are restored after conversion */
    int bu_debug_flag;
    uint32_t rt_debug_flag;
    uint32_t nmg_debug_flag;

    /* print debugging info if debug_mode == 1 */
    unsigned debug_mode;

    /* 0 is quiet, printing only errors */
    unsigned verbosity_level;

    /* max cpus; 0 for automatic */
    unsigned max_cpus;

    struct bn_tol calculational_tolerance;
    struct bg_tess_tol tessellation_tolerance;
    enum gcv_tessellation_algorithm tessellation_algorithm;

    /* conversion to units */
    fastf_t scale_factor;

    /* default name for nameless objects */
    const char *default_name;

    /* number of elements in object_names
     * if 0, convert all top-level objects */
    size_t num_objects;

    /* objects to convert */
    const char * const *object_names;

    /* Apply color randomization */
    unsigned randomize_colors;
};


/**
 * Assign default option values.
 */
GCV_EXPORT void
gcv_opts_default(struct gcv_opts *gcv_options);


/**
 * Input/Output/Translation filter.
 */
struct gcv_filter {
    /* name allowing unique identification by users */
    const char * const name;

    /* operation type */
    const enum gcv_filter_type filter_type;

    /* If we have a specific model type this converter is known to
     * handle, call it out here.  Use BU_MIME_MODEL_UNKNOWN if
     * 'filter_type' is GCV_FILTER_FILTER or if the plugin is a
     * multi-format I/O plugin.
     *
     * FIXME: input/output plugins conceivably could be something
     * other than geometry (e.g., png input or csv output).
     */
    const bu_mime_model_t mime_type;

    /* For plugins supporting multiple file types, call this to
     * process them.
     *
     * Return 1 if the input data can be processed by this filter.
     * (for example: is the file readable by this plugin?  Return
     * 1 if yes, 0 if no.  Later we should incorporate some notion
     * of quality of support into this return value...) */
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


struct gcv_plugin {
    const struct gcv_filter * const * const filters;
};


/*
 * Return a pointer to a bu_ptbl listing all registered filters as
 * const struct gcv_filter pointers.
 */
GCV_EXPORT const struct bu_ptbl *gcv_list_filters(struct gcv_context *context);

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
