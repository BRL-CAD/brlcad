/*                C H E C K _ M O M E N T S . C
 * BRL-CAD
 *
 * Copyright (c) 2018-2026 United States Government as represented by
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

#include "common.h"

#include "ged.h"

#include "../ged_private.h"
#include "./check_private.h"

int check_format_moments(struct ged *gedp,
			 struct analyze_results *res,
			 struct check_parameters *options)
{
    size_t i;
    mat_t moments;
    struct bu_vls title = BU_VLS_INIT_ZERO;

    (void)options; /* units not applied to moment tensor */

    for (i = 0; i < res->n_objects; i++) {
	MAT_COPY(moments, res->objects[i].moments_of_inertia);
	bu_vls_sprintf(&title, "Moments and Products of Inertia For %s",
		       res->objects[i].name);
	bn_mat_print_vls(bu_vls_addr(&title), moments, gedp->ged_result_str);
    }
    bu_vls_free(&title);

    MAT_COPY(moments, res->moments_of_inertia);
    bn_mat_print_vls("For the Moments and Products of Inertia For\n\tAll Specified Objects",
		     moments, gedp->ged_result_str);

    return BRLCAD_OK;
}

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
