/*                       G C V _ A P I . H
 * BRL-CAD
 *
 * Copyright (c) 2008-2014 United States Government as represented by
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
    unsigned verbosity_level; /* 0 -- quiet, print only errors */
    unsigned max_cpus;

    struct bn_tol calculational_tolerance;
    struct rt_tess_tol tessellation_tolerance;
    enum gcv_tessellation_algorithm tessellation_algorithm;

    fastf_t scale_factor; /* units */
    const char *default_name; /* default name for nameless objects */

    size_t num_objects; /* Number of elements in object_names. If zero, convert all top-level objects */
    const char * const *object_names; /* objects to convert */
};


void gcv_opts_default(struct gcv_opts *gcv_options);


/**
 * Input/Output/Translation filter.
 */
struct gcv_filter {
    const char * const name;
    const enum gcv_filter_type filter_type;
    const mime_model_t mime_type;


    /* PRIVATE */

    void (* const create_opts_fn)(struct bu_opt_desc **options_desc, void **options_data); /* PRIVATE */

    void (* const free_opts_fn)(void *options_data); /* PRIVATE */

    int (* const filter_fn)(struct gcv_context *context, const struct gcv_opts *gcv_options, const void *options_data, const char *target); /* PRIVATE */
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
 *
 */
GCV_EXPORT int
gcv_execute(struct gcv_context *context, const struct gcv_filter *filter, const struct gcv_opts *gcv_options, size_t argc, const char * const *argv, const char *target);


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
