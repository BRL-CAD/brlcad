/*                        S A M P L E . C
 * BRL-CAD
 *
 * Copyright (c) 1997-2026 United States Government as represented by
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
/** @addtogroup librt */
/** @{ */
/** @file librt/primitives/sample.c
 *
 *  Pseudo-randomly ray sample an object to generate a point
 *  cloud and generate geometry for visualization.
 */
/** @} */

#include "common.h"

#include <stdlib.h>
#include <string.h>

#include "vmath.h"
#include "rt/defines.h"
#include "rt/geom.h"
#include "rt/db_internal.h"
#include "rt/func.h"
#include "rt/wdb.h"

#include "../librt_private.h"


static void
rt_sample_pnts_internal_free(struct rt_pnts_internal *pnts)
{
    if (!pnts)
	return;

    if (pnts->point) {
	struct pnt_normal *head = (struct pnt_normal *)pnts->point;
	struct pnt_normal *entry;
	while (BU_LIST_WHILE(entry, pnt_normal, &(head->l))) {
	    BU_LIST_DEQUEUE(&(entry->l));
	    BU_PUT(entry, struct pnt_normal);
	}
	BU_PUT(head, struct pnt_normal);
	pnts->point = NULL;
    }

    BU_PUT(pnts, struct rt_pnts_internal);
}


int
rt_obj_sampled_face_set(struct rt_primitive_indexed_face_set *face_set,
			struct rt_db_internal *ip)
{
    point_t *points = NULL;
    vect_t *normals = NULL;
    int *indices = NULL;
    size_t sample_count = 0;
    int ret = BRLCAD_OK;

    if (face_set)
	memset(face_set, 0, sizeof(*face_set));

    if (!face_set || !ip)
	return BRLCAD_ERROR;

    /* If we were given a comb, use its internal info to set up for shooting.
     * Otherwise, we will need to provide an inmem database. */
    const char *oname = NULL;
    struct db_i* dbip = NULL;
    if (ip->idb_type == ID_COMBINATION) {
	// A comb can't be raytraced without its leaves, so we need to get the
	// actual parent dbip and obj info from the comb internal (which is
	// stored for cases exactly like this.)
	struct rt_comb_internal *comb = (struct rt_comb_internal *)ip->idb_ptr;
	RT_CK_COMB(comb);
	RT_CK_DBI(comb->src_dbip);
	dbip = (struct db_i *)comb->src_dbip;
	oname = comb->src_objname;
    } else {
	/* Set up our non-comb object in an in-mem database so we can shoot it.
	 * (This lets us use the raytracer without having to know anything
	 * about ip's parent database.)*/
	oname = "tmp.s";
	dbip = db_open_inmem();
	if (!dbip)
	    return BRLCAD_ERROR;
	struct rt_wdb *wdbp = wdb_dbopen(dbip, RT_WDB_TYPE_DB_INMEM);
	if (wdb_export(wdbp, oname, ip->idb_ptr, ip->idb_type, 1.0) < 0) {
	    db_close(dbip);
	    return BRLCAD_ERROR;
	}
    }

    const char *oav[2] = {oname, NULL};
    point_t rpp_min, rpp_max;
    point_t obj_min, obj_max;
    VSETALL(rpp_min, INFINITY);
    VSETALL(rpp_max, -INFINITY);
    if (rt_obj_bounds(NULL, dbip, 1, oav, 0, obj_min, obj_max) == BRLCAD_ERROR) {
	if (ip->idb_type != ID_COMBINATION)
	    db_close(dbip);
	return BRLCAD_ERROR;
    }
    VMINMAX(rpp_min, rpp_max, (double *)obj_min);
    VMINMAX(rpp_min, rpp_max, (double *)obj_max);
    struct bn_tol btol = BN_TOL_INIT_TOL;
    btol.dist = DIST_PNT_PNT(rpp_max, rpp_min) * 0.01;

    struct rt_db_internal internal;
    RT_DB_INTERNAL_INIT(&internal);
    internal.idb_major_type = DB5_MAJORTYPE_BRLCAD;
    internal.idb_type = ID_PNTS;
    internal.idb_meth = &OBJ[ID_PNTS];
    BU_ALLOC(internal.idb_ptr, struct rt_pnts_internal);
    struct rt_pnts_internal *pnts = (struct rt_pnts_internal *) internal.idb_ptr;
    pnts->magic = RT_PNTS_INTERNAL_MAGIC;
    pnts->scale = 0.0;
    pnts->type = RT_PNT_TYPE_NRM;

    // TODO - make scene_obj setting parameters
    int max_pnts = 100000;
    int max_time = 0;

    double avg_thickness = 0.0;

    int flags = RT_GEN_OBJ_PNTS_RAND;
    if (rt_gen_obj_pnts(pnts, &avg_thickness, dbip, oname, &btol, flags, max_pnts, max_time, 2)) {
	if (ip->idb_type != ID_COMBINATION)
	    db_close(dbip);
	rt_sample_pnts_internal_free(pnts);
	return BRLCAD_ERROR;
    }

    // TODO - apply matrix from full path to points

    // Make triangles based on the point vertex and normal
    struct bn_tol btolt = BN_TOL_INIT_TOL;
    double scale = btol.dist;
    fastf_t ty1 =  0.57735026918962573 * scale; /* tan(PI/6) */
    fastf_t ty2 = -0.28867513459481287 * scale; /* 0.5 * tan(PI/6) */
    fastf_t tx1 = 0.5 * scale;
    vect_t n1;
    point_t v1, v2, v3;
    mat_t rot;
    VSET(n1, 0, 0, 1);
    VSET(v1, 0, ty1, 0);
    VSET(v2, -1*tx1, ty2, 0);
    VSET(v3, tx1, ty2, 0);

    struct pnt_normal *pl = (struct pnt_normal *)pnts->point;
    if (!pl)
	goto done;

    struct pnt_normal *pn = NULL;
    for (BU_LIST_FOR(pn, pnt_normal, &(pl->l)))
	sample_count++;

    if (!sample_count)
	goto done;

    face_set->point_count = sample_count * 3;
    face_set->normal_count = sample_count * 3;
    face_set->index_count = sample_count * 4;
    points = (point_t *)bu_calloc(face_set->point_count, sizeof(point_t),
	    "sample point face-set points");
    normals = (vect_t *)bu_calloc(face_set->normal_count, sizeof(vect_t),
	    "sample point face-set normals");
    indices = (int *)bu_calloc(face_set->index_count, sizeof(int),
	    "sample point face-set indices");

    size_t point_idx = 0;
    size_t index_idx = 0;
    size_t normal_idx = 0;
    for (BU_LIST_FOR(pn, pnt_normal, &(pl->l))) {
	vect_t v1pp, v2pp, v3pp = {0.0, 0.0, 0.0};
	vect_t v1fp, v2fp, v3fp = {0.0, 0.0, 0.0};
	VMOVE(v1pp, v1);
	VMOVE(v2pp, v2);
	VMOVE(v3pp, v3);
	bn_mat_fromto(rot, n1, pn->n, &btolt);
	MAT4X3VEC(v1fp, rot, v1pp);
	MAT4X3VEC(v2fp, rot, v2pp);
	MAT4X3VEC(v3fp, rot, v3pp);
	VADD2(v1pp, v1fp, pn->v);
	VADD2(v2pp, v2fp, pn->v);
	VADD2(v3pp, v3fp, pn->v);

	VMOVE(points[point_idx], v1pp);
	VMOVE(normals[normal_idx], pn->n);
	normal_idx++;
	indices[index_idx++] = (int)point_idx++;

	VMOVE(points[point_idx], v2pp);
	VMOVE(normals[normal_idx], pn->n);
	normal_idx++;
	indices[index_idx++] = (int)point_idx++;

	VMOVE(points[point_idx], v3pp);
	VMOVE(normals[normal_idx], pn->n);
	normal_idx++;
	indices[index_idx++] = (int)point_idx++;

	indices[index_idx++] = -1;
    }

    face_set->points = points;
    face_set->normals = normals;
    face_set->indices = indices;
    face_set->source_identity = (uint64_t)(uintptr_t)ip->idb_ptr;
    face_set->geometry_revision++;
    points = NULL;
    normals = NULL;
    indices = NULL;

done:
    if (points)
	bu_free(points, "sample point face-set points");
    if (normals)
	bu_free(normals, "sample point face-set normals");
    if (indices)
	bu_free(indices, "sample point face-set indices");
    rt_sample_pnts_internal_free(pnts);
    if (ip->idb_type != ID_COMBINATION)
	db_close(dbip);

    return ret;
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
