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

#include "raytrace.h"

__BEGIN_DECLS


/**
 * The big kahuna.
 */
struct gcv_context;


/**
 * Conversion options.
 */
struct gcv_opts;


/**
 * Input/Output/Translation filter.
 */
struct gcv_filter;


/**
 * Initialize a conversion context.
 *
 * Returns 0 on success or non-zero on failure.
 */
GCV_EXPORT int
gcv_init(struct gcv_context *cxt);


/**
 * Release a conversion context, freeing all memory internally
 * allocated by the library.  Caller is responsible for freeing the
 * context pointer itself, if necessary.
 */
GCV_EXPORT int
gcv_destroy(struct gcv_context *cxt);


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
gcv_execute(struct gcv_context *cxt, const struct gcv_filter *filter);


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
