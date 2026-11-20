/* G C V _ P R I V A T E . H
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
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
#ifndef GCV_PRIVATE_H
#define GCV_PRIVATE_H

#include "common.h"
#include "gcv/api.h"

struct gcv_handler_registry;
struct gcv_context_internal {
    struct bu_ptbl *handles;
    struct bu_ptbl filters;
    struct bu_mime_registry *mime;
    struct gcv_handler_registry *handlers;
    int filters_loaded;
    const char *target_mime;
    const struct bu_mime_traits *target_traits;
    const struct bu_mime_traits *target_request_traits;
    struct gcv_conversion_result *active_result;
    unsigned int reject_losses;
};

__BEGIN_DECLS
struct gcv_handler_registry *gcv_handlers_create(void);
void gcv_handlers_destroy(struct gcv_handler_registry *registry);
void gcv_conversion_result_log(const struct gcv_conversion_result *result);
int gcv_invoke_filter(struct gcv_context *context, const struct gcv_filter *filter,
    const struct gcv_opts *options, const void *options_data, const char *target, struct bu_vls *messages);
enum gcv_conversion_status gcv_execute_internal(struct gcv_context *context, const struct gcv_filter *filter,
    const struct gcv_opts *options, size_t argc, const char *const *argv, const char *target,
    struct gcv_conversion_result *result);
__END_DECLS

#endif
