/*                       G E D _ U T I L . C
 * BRL-CAD
 *
 * Copyright (c) 2000-2011 United States Government as represented by
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
/** @addtogroup libged */
/** @{ */
/** @file libged/ged_util.c
 *
 * Utility routines for common operations in libged.
 *
 */
/** @} */

#include "common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "bio.h"

#include "bu.h"
#include "vmath.h"
#include "bn.h"
#include "rtgeom.h"
#include "raytrace.h"
#include "plot3.h"

#include "./ged_private.h"

int
_ged_results_append_str(struct ged *gedp, char *result_string)
{
    bu_vls_printf(gedp->ged_result_str, "%s", result_string);
    return GED_OK;
}


int
_ged_results_append_vls(struct ged *gedp, struct bu_vls *result_vls)
{
    bu_vls_printf(gedp->ged_result_str, "%s", bu_vls_addr(result_vls));
    return GED_OK;
}


int
_ged_results_clear(struct ged *gedp)
{
    bu_vls_trunc(gedp->ged_result_str, 0);
    return GED_OK;
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
