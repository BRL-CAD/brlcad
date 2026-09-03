/*                  V A L I D A T I O N . H
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 */

#ifndef LIBGED_FACETIZE_VALIDATION_H
#define LIBGED_FACETIZE_VALIDATION_H

#include "bu/hook.h"
#include "rt/db_instance.h"

void
facetize_log_hooks_silence(struct bu_hook_list *saved_hooks);

void
facetize_log_hooks_restore(struct bu_hook_list *saved_hooks);

long
facetize_csg_metrics(struct db_i *dbip, const char *object_name,
	double *surface_area, double *volume);

#endif /* LIBGED_FACETIZE_VALIDATION_H */
