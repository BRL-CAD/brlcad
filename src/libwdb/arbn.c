/*                          A R B N . C
 * BRL-CAD
 *
 * Copyright (c) 1989-2014 United States Government as represented by
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
/** @file libwdb/arbn.c
 *
 * libwdb support for writing an ARBN.
 *
 */

#include "common.h"

#include <stdio.h>
#include <math.h>
#include "bio.h"

#include "bu.h"
#include "vmath.h"
#include "bn.h"
#include "rtgeom.h"
#include "raytrace.h"
#include "wdb.h"


int
mk_arbn(struct rt_wdb *filep, const char *name, size_t neqn, const plane_t *eqn)
{
    struct rt_arbn_internal *arbn;
    plane_t *equations = NULL;
    size_t i;

    if (neqn <= 0)
	return -1;

    equations = (plane_t *)bu_malloc(neqn*sizeof(plane_t), "equations");
    for (i=0; i<neqn; i++) {
	HMOVE(equations[i], eqn[i]);
    }

    BU_ALLOC(arbn, struct rt_arbn_internal);
    arbn->magic = RT_ARBN_INTERNAL_MAGIC;
    arbn->neqn = neqn;
    arbn->eqn = equations;

    return wdb_export(filep, name, (void *)arbn, ID_ARBN, mk_conv2mm);
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
