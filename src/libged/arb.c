/*                         A R B . C
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
/** @file libged/arb.c
 *
 * The arb command.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include "bio.h"

#include "rtgeom.h"

#include "./ged_private.h"


int
ged_arb(struct ged *gedp, int argc, const char *argv[])
{
    struct directory *dp;
    struct rt_db_internal internal;
    struct rt_arb_internal *arb;
    int i, j;
    double rota, fb;
    vect_t norm1, norm2, norm3;
    static const char *usage = "name rot fb";

    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_READ_ONLY(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    if (argc != 4) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    GED_CHECK_EXISTS(gedp, argv[1], LOOKUP_QUIET, GED_ERROR);

    /* get rotation angle */
    if (sscanf(argv[2], "%lf", &rota) != 1) {
	bu_vls_printf(gedp->ged_result_str, "%s: bad rotation angle - %s", argv[0], argv[2]);
	return GED_ERROR;
    }

    /* get fallback angle */
    if (sscanf(argv[3], "%lf", &fb) != 1) {
	bu_vls_printf(gedp->ged_result_str, "%s: bad fallback angle - %s", argv[0], argv[3]);
	return GED_ERROR;
    }

    rota *= DEG2RAD;
    fb *= DEG2RAD;

    BU_ALLOC(arb, struct rt_arb_internal);
    RT_DB_INTERNAL_INIT(&internal);
    internal.idb_major_type = DB5_MAJORTYPE_BRLCAD;
    internal.idb_type = ID_ARB8;
    internal.idb_meth = &OBJ[ID_ARB8];
    internal.idb_ptr = (genptr_t)arb;
    arb->magic = RT_ARB_INTERNAL_MAGIC;

    /* FIXME: we should be creating the arb at the center of the
     * screen.  Extract bounding box code from both autoview.c and
     * get_autoview.c into a general bounding rpp function, and use
     * accordingly.  Should combin autoview.c with get_autoview.c
     * (perhaps as a flag).
     */
    VSET(arb->pt[0], 0.0, 0.0, 0.0);

    /* calculate normal vector defined by rot, fb */
    norm1[0] = cos(fb) * cos(rota);
    norm1[1] = cos(fb) * sin(rota);
    norm1[2] = sin(fb);

    /* find two perpendicular vectors which are perpendicular to norm */
    j = 0;
    for (i = 0; i < 3; i++) {
	if (fabs(norm1[i]) < fabs(norm1[j]))
	    j = i;
    }
    VSET(norm2, 0.0, 0.0, 0.0);
    norm2[j] = 1.0;
    VCROSS(norm3, norm2, norm1);
    VCROSS(norm2, norm3, norm1);

    /* create new rpp 20x20x2 */
    /* the 20x20 faces are in rot, fb plane */
    VUNITIZE(norm2);
    VUNITIZE(norm3);
    VJOIN1(arb->pt[1], arb->pt[0], 508.0, norm2);
    VJOIN1(arb->pt[3], arb->pt[0], -508.0, norm3);
    VJOIN2(arb->pt[2], arb->pt[0], 508.0, norm2, -508.0, norm3);
    for (i = 0; i < 4; i++)
	VJOIN1(arb->pt[i+4], arb->pt[i], -50.8, norm1);

    GED_DB_DIRADD(gedp, dp, argv[1], RT_DIR_PHONY_ADDR, 0, RT_DIR_SOLID, (genptr_t)&internal.idb_type, GED_ERROR);
    GED_DB_PUT_INTERNAL(gedp, dp, &internal, &rt_uniresource, GED_ERROR);

    return GED_OK;
}


/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
