/*                    P O I N T S _ E V A L . C
 * BRL-CAD
 *
 * Copyright (c) 1997-2022 United States Government as represented by
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
/** @file libged/draw/points_eval.c
 *
 *  Given a .g object, raytrace that object pseudo-randomly to generate a point
 *  cloud for visualization
 *
 */
/** @} */

#include "common.h"

#include <stdlib.h>
#include <string.h>

#include "vmath.h"
#include "rt/geom.h"
#include "raytrace.h"
#include "analyze.h"

#include "./ged_private.h"

int
draw_points(struct bv_scene_obj *s)
{
    struct draw_update_data_t *d = (struct draw_update_data_t *)s->s_i_data;
    struct bn_tol btol = BG_TOL_INIT;
    struct db_i *dbip = d->dbip;
    struct db_full_path *fp = &d->fp;
    struct directory *dp = DB_FULL_PATH_CUR_DIR(fp);

    const char *oav[2] = {NULL};
    oav[0] = dp->d_namep;
    point_t rpp_min, rpp_max;
    point_t obj_min, obj_max;
    VSETALL(rpp_min, INFINITY);
    VSETALL(rpp_max, -INFINITY);
    if (rt_obj_bounds(NULL, dbip, 1, oav, 0, obj_min, obj_max) == BRLCAD_ERROR)
	return BRLCAD_ERROR;
    VMINMAX(rpp_min, rpp_max, (double *)obj_min);
    VMINMAX(rpp_min, rpp_max, (double *)obj_max);
    btol.dist = DIST_PNT_PNT(rpp_max, rpp_min) * 0.01;

    struct rt_db_internal internal;
    RT_DB_INTERNAL_INIT(&internal);
    internal.idb_major_type = DB5_MAJORTYPE_BRLCAD;
    internal.idb_type = ID_PNTS;
    internal.idb_meth = &OBJ[ID_PNTS];
    BU_GET(internal.idb_ptr, struct rt_pnts_internal);
    struct rt_pnts_internal *pnts = (struct rt_pnts_internal *) internal.idb_ptr;
    pnts->magic = RT_PNTS_INTERNAL_MAGIC;
    pnts->scale = 0.0;
    pnts->type = RT_PNT_TYPE_NRM;

    int max_pnts = 10000;
    int max_time = 2;

    double avg_thickness = 0.0;

    int flags = 0;
    flags |= ANALYZE_OBJ_TO_PNTS_RAND;

    if (analyze_obj_to_pnts(pnts, &avg_thickness, dbip, dp->d_namep, &btol, flags, max_pnts, max_time, 2))
	return BRLCAD_ERROR;

    // TODO - apply matrix from full path to points

    // Make triangles based on the point vertex and normal
    double scale = btol.dist * 0.1;
    fastf_t ty1 =  0.57735026918962573 * scale; /* tan(PI/6) */
    fastf_t ty2 = -0.28867513459481287 * scale; /* 0.5 * tan(PI/6) */
    fastf_t tx1 = 0.5 * scale;
    point_t v1, v2, v3;
    vect_t n1;
    vect_t v1pp, v2pp, v3pp = {0.0, 0.0, 0.0};
    vect_t v1fp, v2fp, v3fp = {0.0, 0.0, 0.0};
    mat_t rot;
    VSET(n1, 0, 0, 1);
    VSET(v1, 0, ty1, 0);
    VSET(v2, -1*tx1, ty2, 0);
    VSET(v3, tx1, ty2, 0);
    struct pnt_normal *point = (struct pnt_normal *)pnts->point;
    struct bu_list *head = &point->l;
    struct bu_list *vlfree = s->s_v->vlfree;
    struct bu_list *vhead = &s->s_vlist;
    for (BU_LIST_FOR(point, pnt_normal, head)) {
	VMOVE(v1pp, v1);
	VMOVE(v2pp, v2);
	VMOVE(v3pp, v3);
	bn_mat_fromto(rot, n1, point->n, &btol);
	MAT4X3VEC(v1fp, rot, v1pp);
	MAT4X3VEC(v2fp, rot, v2pp);
	MAT4X3VEC(v3fp, rot, v3pp);
	VADD2(v1pp, v1fp, point->v);
	VADD2(v2pp, v2fp, point->v);
	VADD2(v3pp, v3fp, point->v);
	BV_ADD_VLIST(vlfree, vhead, v1pp, BV_VLIST_LINE_MOVE);
	BV_ADD_VLIST(vlfree, vhead, v2pp, BV_VLIST_LINE_DRAW);
	BV_ADD_VLIST(vlfree, vhead, v3pp, BV_VLIST_LINE_DRAW);
	BV_ADD_VLIST(vlfree, vhead, v1pp, BV_VLIST_LINE_DRAW);
    }

    BU_PUT(internal.idb_ptr, struct rt_pnts_internal);

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
