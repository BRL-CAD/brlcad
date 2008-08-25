/*                          P N T S . C
 * BRL-CAD
 *
 * Copyright (c) 2008 United States Government as represented by
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
/** @file pnts.c
 *
 * Collection of points.
 *
 */

#include "common.h"

#include "bn.h"
#include "bu.h"
#include "raytrace.h"
#include "rtgeom.h"
#include "vmath.h"

/* length of axes segments plotted for points with zero weight */
#define SEG_LENGTH .1

/**
 *                      R T _ P N T S _ E X P O R T 5
 *
 * Export a pnts collection from the internal structure
 * to the database format: numPoints, weight, points
 */
int
rt_pnts_export5(struct bu_external *external, const struct rt_db_internal *internal,
		double local2mm, const struct db_i *db)
{
    int i, numPointsBytes, weightBytes, pointBytes;
    unsigned long numPoints;
    struct rt_pnts_internal *pnts;
    register struct pnt *point;
    struct bu_list *head;
    fastf_t *points;

    /* acquire internal pnts structure */
    RT_CK_DB_INTERNAL(internal);
    BU_CK_EXTERNAL(external);

    pnts = (struct rt_pnts_internal *) internal->idb_ptr;
    RT_PNTS_CK_MAGIC(pnts);
   
    numPoints = pnts->numPoints;


    /* allocate enough space in buffer for the external format:
     * unsigned long numPoints, double weight, point doubles
     */
    numPointsBytes = sizeof(long);
    weightBytes = SIZEOF_NETWORK_DOUBLE;
    pointBytes = pnts->numPoints * ELEMENTS_PER_PT * SIZEOF_NETWORK_DOUBLE;

    external->ext_nbytes = numPointsBytes + weightBytes + pointBytes;
    external->ext_buf = (genptr_t) bu_malloc(external->ext_nbytes, "pnts external");

    /* place numPoints and weight at beginning of buffer */
    (void) bu_plong((unsigned char *) external->ext_buf, numPoints);

    htond((unsigned char *)external->ext_buf + numPointsBytes, (unsigned char *)&pnts->weight, 1);


    if (numPoints > 0) {
	head = &pnts->vList->l;

	points = (fastf_t *) bu_malloc(pointBytes, "rt_pnts_export5: points");

	/* scale points and store in memory */
	for (i = 0, BU_LIST_FOR(point, pnt, head), i += 3) {
	    VSCALE(&points[i], point->v, local2mm);
	}

	/* place scaled points after numPoints and weight in the buffer */
	htond((unsigned char *) external->ext_buf + numPointsBytes + weightBytes,
	      (unsigned char *) points, ELEMENTS_PER_PT * numPoints);

	bu_free((genptr_t) points, "rt_pnts_export5: points");
    }

    return 0;
}

/**
 *                     R T _ P N T S _ I M P O R T 5
 *
 * Import a pnts collection from the database format to
 * the internal structure and apply modeling transformations.
 */
int
rt_pnts_import5(struct rt_db_internal *internal, const struct bu_external *external,
		register const fastf_t *mat, const struct db_i *db)
{
    int i, numPointsBytes, weightBytes, pointBytes;
    unsigned long numPoints;
    struct rt_pnts_internal*pnts;
    struct pnt *point;
    fastf_t *pt;

    RT_CK_DB_INTERNAL(internal);
    BU_CK_EXTERNAL(external);

    /* initialize database structure */
    internal->idb_major_type = DB5_MAJORTYPE_BRLCAD;
    internal->idb_type = ID_PNTS;
    internal->idb_meth = &rt_functab[ID_PNTS];
    internal->idb_ptr = bu_malloc(sizeof(struct rt_pnts_internal), "rt_pnts_internal");

    /* initialize internal structure */
    pnts = (struct rt_pnts_internal *) internal->idb_ptr;
    pnts->magic = RT_PNTS_INTERNAL_MAGIC;
    BU_GETSTRUCT(pnts->vList, pnt);
    BU_LIST_INIT(&pnts->vList->l);

    /* pull internal members from buffer */
    numPointsBytes = sizeof(long);
    weightBytes = SIZEOF_NETWORK_DOUBLE;

    numPoints = pnts->numPoints = bu_glong((unsigned char *) external->ext_buf);
    pointBytes = numPoints * ELEMENTS_PER_PT * SIZEOF_NETWORK_DOUBLE;

    ntohd((unsigned char *)&pnts->weight, (unsigned char *)external->ext_buf + numPointsBytes, 1);


    if (numPoints > 0) {
	pt = (fastf_t *) bu_malloc(pointBytes, "rt_pnts_import5: pt");

	/* pull points from buffer */
	ntohd((unsigned char *) pt, (unsigned char *) external->ext_buf + numPointsBytes +
	      weightBytes, ELEMENTS_PER_PT * numPoints);


	if (mat == NULL) {
	    mat = bn_mat_identity;
	}

	/* make point_t's from doubles and place in bu_list */
	for (i = 0; i < numPoints * ELEMENTS_PER_PT; i += 3) {
	    BU_GETSTRUCT(point, pnt);
	    
	    MAT4X3PNT(point->v, mat, &pt[i]);

	    BU_LIST_PUSH(&(pnts->vList->l), &point->l);
	}

	bu_free((genptr_t) pt, "rt_pnts_import5: pt");
    }

    return 0;
}

/**
 *                       R T _ P N T S _ I F R E E
 *
 * Free the storage associated with the rt_db_internal version of the collection.
 */
int
rt_pnts_ifree(struct rt_db_internal *internal)
{
    RT_CK_DB_INTERNAL(internal);

    bu_free(internal->idb_ptr, "pnts ifree");

    internal->idb_ptr = GENPTR_NULL;
}

/**
 *                      R T _ P N T S _ P R I N T
 *
 */
void
rt_pnts_print(register const struct soltab *stp)
{
}

/**
 *                      R T _ P N T S _ P L O T
 *
 * Plot pnts collection as axes or spheres.
 */
int
rt_pnts_plot(struct bu_list *vhead, struct rt_db_internal *internal,
	     const struct rt_tess_tol *ttol, const struct bn_tol *tol)
{
    struct rt_pnts_internal *pnts;
    struct bu_list *head;
    struct rt_db_internal db;
    struct rt_ell_internal ell;
    struct pnt *point;
    double weight, vCoord, hCoord;
    point_t a, b;

    RT_CK_DB_INTERNAL(internal);

    pnts = (struct rt_pnts_internal *) internal->idb_ptr;
    RT_PNTS_CK_MAGIC(pnts);

    if (pnts->numPoints > 0) {
	head = &pnts->vList->l;
	weight = pnts->weight;
    } else {
	return 0;
    }

    if (weight > 0) {
	/* set local database */
	db.idb_magic = RT_DB_INTERNAL_MAGIC;
	db.idb_major_type = ID_ELL;
	db.idb_ptr = &ell;

	/* set local ell for the pnts collection */
	ell.magic = RT_ELL_INTERNAL_MAGIC;

	VSET(ell.a, weight, 0, 0);
	VSET(ell.b, 0, weight, 0);
	VSET(ell.c, 0, 0, weight);

	/* give rt_ell_plot a sphere representation of each point */
	for(BU_LIST_FOR(point, pnt, head)) {
	    VMOVE(ell.v, point->v);
	    rt_ell_plot(vhead, &db, ttol, tol);
	}
    } else {
	vCoord = hCoord = SEG_LENGTH / 2;

	for (BU_LIST_FOR(point, pnt, head)) {
	    /* draw first horizontal segment for this point */
	    VSET(a, point->v[X] - hCoord, point->v[Y], point->v[Z]);
	    VSET(b, point->v[X] + hCoord, point->v[Y], point->v[Z]);
	    RT_ADD_VLIST(vhead, a, BN_VLIST_LINE_MOVE);
	    RT_ADD_VLIST(vhead, b, BN_VLIST_LINE_DRAW);

	    /* draw perpendicular horizontal segment */
	    VSET(a, point->v[X], point->v[Y] - hCoord, point->v[Z]);
	    VSET(b, point->v[X], point->v[Y] + hCoord, point->v[Z]);
	    RT_ADD_VLIST(vhead, a, BN_VLIST_LINE_MOVE);
	    RT_ADD_VLIST(vhead, b, BN_VLIST_LINE_DRAW);

	    /* draw vertical segment */
	    VSET(a, point->v[X], point->v[Y], point->v[Z] - vCoord);
	    VSET(b, point->v[X], point->v[Y], point->v[Z] + vCoord);
	    RT_ADD_VLIST(vhead, a, BN_VLIST_LINE_MOVE);
	    RT_ADD_VLIST(vhead, b, BN_VLIST_LINE_DRAW);
	}
    }

    return 0;
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
