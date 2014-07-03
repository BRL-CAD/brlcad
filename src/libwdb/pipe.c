/*                          P I P E . C
 * BRL-CAD
 *
 * Copyright (c) 1990-2014 United States Government as represented by
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

/** @file libwdb/pipe.c
 *
 * Support for particles and pipes.  Library for writing geometry
 * databases from arbitrary procedures.
 *
 * Note that routines which are passed point_t or vect_t or mat_t
 * parameters (which are call-by-address) must be VERY careful to
 * leave those parameters unmodified (e.g., by scaling), so that the
 * calling routine is not surprised.
 *
 * Return codes of 0 are OK, -1 signal an error.
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
mk_particle(struct rt_wdb *fp, const char *name, fastf_t *vertex, fastf_t *height, double vradius, double hradius)
{
    struct rt_part_internal *part;

    BU_ALLOC(part, struct rt_part_internal);
    part->part_magic = RT_PART_INTERNAL_MAGIC;
    VMOVE(part->part_V, vertex);
    VMOVE(part->part_H, height);
    part->part_vrad = vradius;
    part->part_hrad = hradius;
    part->part_type = 0;		/* sanity, unused */

    return wdb_export(fp, name, (void *)part, ID_PARTICLE, mk_conv2mm);
}


int
mk_pipe(struct rt_wdb *fp, const char *name, struct bu_list *headp)
{
    struct rt_pipe_internal *pipep;

    if (rt_pipe_ck(headp)) {
	bu_log("mk_pipe: BAD PIPE SOLID (%s)\n", name);
	return 1;
    }

    BU_ALLOC(pipep, struct rt_pipe_internal);
    pipep->pipe_magic = RT_PIPE_INTERNAL_MAGIC;
    BU_LIST_INIT(&pipep->pipe_segs_head);
    /* linked list from caller */
    BU_LIST_APPEND_LIST(&pipep->pipe_segs_head, headp);

    return wdb_export(fp, name, (void *)pipep, ID_PIPE, mk_conv2mm);
}


void
mk_pipe_free(struct bu_list *headp)
{
    struct wdb_pipept *wp;

    while (BU_LIST_WHILE(wp, wdb_pipept, headp)) {
	BU_LIST_DEQUEUE(&wp->l);
	bu_free((char *)wp, "mk_pipe_free");
    }
}


void
mk_add_pipe_pt(
    struct bu_list *headp,
    const point_t coord,
    double od,
    double id,
    double bendradius)
{
    struct wdb_pipept *newpp;

    BU_CKMAG(headp, WDB_PIPESEG_MAGIC, "pipe point");

    BU_ALLOC(newpp, struct wdb_pipept);
    newpp->l.magic = WDB_PIPESEG_MAGIC;
    newpp->pp_od = od;
    newpp->pp_id = id;
    newpp->pp_bendradius = bendradius;
    VMOVE(newpp->pp_coord, coord);
    BU_LIST_INSERT(headp, &newpp->l);
}


void
mk_pipe_init(struct bu_list *headp)
{
    BU_LIST_INIT(headp);
    headp->magic = WDB_PIPESEG_MAGIC;
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
