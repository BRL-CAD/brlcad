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


/* length of axes segments plotted for points with zero scale */
#define SEG_LENGTH .1


/**
 * R T _ P N T S _ E X P O R T 5
 *
 * Export a pnts collection from the internal structure
 * to the database format
 */
int
rt_pnts_export5(struct bu_external *external, const struct rt_db_internal *internal,
		double local2mm, const struct db_i *db)
{
    struct rt_pnts_internal *pnts = NULL;
    struct bu_list *head = NULL;
    unsigned long pointDataSize;
    unsigned char *buf = NULL;
    int i;

    /* acquire internal pnts structure */
    RT_CK_DB_INTERNAL(internal);
    BU_CK_EXTERNAL(external);
    external->ext_nbytes = 0;

    pnts = (struct rt_pnts_internal *) internal->idb_ptr;
    RT_PNTS_CK_MAGIC(pnts);

    /* allocate enough for the header (magic + scale + type + count) */
    external->ext_nbytes = sizeof(long) + SIZEOF_NETWORK_DOUBLE + sizeof(unsigned short) + sizeof (unsigned long);
    external->ext_buf = (genptr_t) bu_calloc(sizeof(unsigned char), external->ext_nbytes, "pnts external");
    buf = (unsigned char *)external->ext_buf;

    buf = bu_plong(buf, pnts->magic);
    htond(buf, (unsigned char *)&pnts->scale, 1);
    buf += SIZEOF_NETWORK_DOUBLE;
    buf = bu_pshort(buf, (unsigned short)pnts->type);
    buf = bu_plong(buf, pnts->count);

    if (pnts->count <= 0) {
	/* no points to stash, we're done */
	return 0;
    }

    /* figure out how much data there is for each point */
    pointDataSize = ELEMENTS_PER_POINT * SIZEOF_NETWORK_DOUBLE;
    if (pnts->type & RT_PNT_TYPE_COL)
	pointDataSize += 3 * SIZEOF_NETWORK_DOUBLE;
    if (pnts->type & RT_PNT_TYPE_SCA)
	pointDataSize += 1 * SIZEOF_NETWORK_DOUBLE;
    if (pnts->type & RT_PNT_TYPE_NRM)
	pointDataSize += ELEMENTS_PER_VECT;

    external->ext_buf = (genptr_t)bu_realloc(external->ext_buf, external->ext_nbytes + (pnts->count * pointDataSize), "pnts external realloc");
    buf = (unsigned char *)external->ext_buf + external->ext_nbytes;

    switch (pnts->type) {
	case RT_PNT_TYPE_PNT: {
	    register struct pnt *point = (struct pnt *)pnts->point;
	    head = &point->l;
    
	    for (BU_LIST_FOR(point, pnt, head)) {
		point_t v;

		/* pack v */
		VSCALE(v, point->v, local2mm);
		htond((unsigned char *)buf, (unsigned char *)v, ELEMENTS_PER_POINT);
		buf += ELEMENTS_PER_POINT;
	    }

	    break;
	}
	case RT_PNT_TYPE_COL: {
	    register struct pnt_color *point = (struct pnt_color *)pnts->point;
	    head = &point->l;
    
	    for (BU_LIST_FOR(point, pnt_color, head)) {
		point_t v;
		double c[3];

		/* pack v */
		VSCALE(v, point->v, local2mm);
		htond((unsigned char *)buf, (unsigned char *)v, ELEMENTS_PER_POINT);
		buf += ELEMENTS_PER_POINT * SIZEOF_NETWORK_DOUBLE;
		
		/* pack c */
		bu_color_to_rgb_floats(&point->c, c);
		htond((unsigned char *)buf, (unsigned char *)c, 3);
		buf += 3 * SIZEOF_NETWORK_DOUBLE;
	    }

	    break;
	}
	case RT_PNT_TYPE_SCA: {
	    register struct pnt_scale *point = (struct pnt_scale *)pnts->point;
	    head = &point->l;
    
	    for (BU_LIST_FOR(point, pnt_scale, head)) {
		point_t v;
		double s[1];

		/* pack v */
		VSCALE(v, point->v, local2mm);
		htond((unsigned char *)buf, (unsigned char *)v, ELEMENTS_PER_POINT);
		buf += ELEMENTS_PER_POINT * SIZEOF_NETWORK_DOUBLE;
		
		/* pack s */
		s[0] = point->s * local2mm;
		htond((unsigned char *)buf, (unsigned char *)s, SIZEOF_NETWORK_DOUBLE);
		buf += 1 * SIZEOF_NETWORK_DOUBLE;
	    }

	    break;
	}
	case RT_PNT_TYPE_NRM: {
	    register struct pnt_normal *point = (struct pnt_normal *)pnts->point;
	    head = &point->l;
    
	    for (BU_LIST_FOR(point, pnt_normal, head)) {
		point_t v;
		vect_t n;

		/* pack v */
		VSCALE(v, point->v, local2mm);
		htond((unsigned char *)buf, (unsigned char *)v, ELEMENTS_PER_POINT);
		buf += ELEMENTS_PER_POINT * SIZEOF_NETWORK_DOUBLE;
		
		/* pack n */
		VSCALE(n, point->n, local2mm);
		htond((unsigned char *)buf, (unsigned char *)n, ELEMENTS_PER_VECT);
		buf += ELEMENTS_PER_VECT * SIZEOF_NETWORK_DOUBLE;
	    }

	    break;
	}
	case RT_PNT_TYPE_COL_SCA: {
	    register struct pnt_color_scale *point = (struct pnt_color_scale *)pnts->point;
	    head = &point->l;
    
	    for (BU_LIST_FOR(point, pnt_color_scale, head)) {
		point_t v;
		double c[3];
		double s[1];

		/* pack v */
		VSCALE(v, point->v, local2mm);
		htond((unsigned char *)buf, (unsigned char *)v, ELEMENTS_PER_POINT);
		buf += ELEMENTS_PER_POINT * SIZEOF_NETWORK_DOUBLE;
		
		/* pack c */
		bu_color_to_rgb_floats(&point->c, c);
		htond((unsigned char *)buf, (unsigned char *)c, 3);
		buf += 3 * SIZEOF_NETWORK_DOUBLE;

		/* pack s */
		s[0] = point->s * local2mm;
		htond((unsigned char *)buf, (unsigned char *)s, SIZEOF_NETWORK_DOUBLE);
		buf += 1 * SIZEOF_NETWORK_DOUBLE;
	    }

	    break;
	}
	case RT_PNT_TYPE_COL_NRM: {
	    register struct pnt_color_normal *point = (struct pnt_color_normal *)pnts->point;
	    head = &point->l;
    
	    for (BU_LIST_FOR(point, pnt_color_normal, head)) {
		point_t v;
		double c[3];
		vect_t n;

		/* pack v */
		VSCALE(v, point->v, local2mm);
		htond((unsigned char *)buf, (unsigned char *)v, ELEMENTS_PER_POINT);
		buf += ELEMENTS_PER_POINT * SIZEOF_NETWORK_DOUBLE;
		
		/* pack c */
		bu_color_to_rgb_floats(&point->c, c);
		htond((unsigned char *)buf, (unsigned char *)c, 3);
		buf += 3 * SIZEOF_NETWORK_DOUBLE;

		/* pack n */
		VSCALE(n, point->n, local2mm);
		htond((unsigned char *)buf, (unsigned char *)n, ELEMENTS_PER_VECT);
		buf += ELEMENTS_PER_VECT * SIZEOF_NETWORK_DOUBLE;
	    }

	    break;
	}
	case RT_PNT_TYPE_SCA_NRM: {
	    register struct pnt_scale_normal *point = (struct pnt_scale_normal *)pnts->point;
	    head = &point->l;
    
	    for (BU_LIST_FOR(point, pnt_scale_normal, head)) {
		point_t v;
		double s[1];
		vect_t n;

		/* pack v */
		VSCALE(v, point->v, local2mm);
		htond((unsigned char *)buf, (unsigned char *)v, ELEMENTS_PER_POINT);
		buf += ELEMENTS_PER_POINT * SIZEOF_NETWORK_DOUBLE;
		
		/* pack s */
		s[0] = point->s * local2mm;
		htond((unsigned char *)buf, (unsigned char *)s, SIZEOF_NETWORK_DOUBLE);
		buf += 1 * SIZEOF_NETWORK_DOUBLE;

		/* pack n */
		VSCALE(n, point->n, local2mm);
		htond((unsigned char *)buf, (unsigned char *)n, ELEMENTS_PER_VECT);
		buf += ELEMENTS_PER_VECT * SIZEOF_NETWORK_DOUBLE;
	    }

	    break;
	}
	case RT_PNT_TYPE_COL_SCA_NRM: {
	    register struct pnt_color_scale_normal *point = (struct pnt_color_scale_normal *)pnts->point;
	    head = &point->l;
    
	    for (BU_LIST_FOR(point, pnt_color_scale_normal, head)) {
		point_t v;
		double c[3];
		double s[1];
		vect_t n;

		/* pack v */
		VSCALE(v, point->v, local2mm);
		htond((unsigned char *)buf, (unsigned char *)v, ELEMENTS_PER_POINT);
		buf += ELEMENTS_PER_POINT * SIZEOF_NETWORK_DOUBLE;
		
		/* pack c */
		bu_color_to_rgb_floats(&point->c, c);
		htond((unsigned char *)buf, (unsigned char *)c, 3);
		buf += 3 * SIZEOF_NETWORK_DOUBLE;

		/* pack s */
		s[0] = point->s * local2mm;
		htond((unsigned char *)buf, (unsigned char *)s, SIZEOF_NETWORK_DOUBLE);
		buf += 1 * SIZEOF_NETWORK_DOUBLE;

		/* pack n */
		VSCALE(n, point->n, local2mm);
		htond((unsigned char *)buf, (unsigned char *)n, ELEMENTS_PER_VECT);
		buf += ELEMENTS_PER_VECT * SIZEOF_NETWORK_DOUBLE;
	    }

	    break;
	}
	default:
	    bu_log("ERROR: unknown points primitive type\n");
	    return 0;
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
    int i, numPointsBytes, scaleBytes, pointBytes;
    unsigned long numPoints;
    struct rt_pnts_internal*pnts;
    struct pnt *point;
    struct pnt *headPoint;
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
    BU_GETSTRUCT(pnts->point, pnt);
    headPoint = (struct pnt *)pnts->point;
    BU_LIST_INIT(&headPoint->l);

    /* pull internal members from buffer */
    numPointsBytes = sizeof(long);
    scaleBytes = SIZEOF_NETWORK_DOUBLE;

    numPoints = pnts->count = bu_glong((unsigned char *) external->ext_buf);
    pointBytes = numPoints * ELEMENTS_PER_POINT * SIZEOF_NETWORK_DOUBLE;

    ntohd((unsigned char *)&pnts->scale, (unsigned char *)external->ext_buf + numPointsBytes, 1);


    if (numPoints > 0) {
	pt = (fastf_t *) bu_malloc(pointBytes, "rt_pnts_import5: pt");

	/* pull points from buffer */
	ntohd((unsigned char *) pt, (unsigned char *) external->ext_buf + numPointsBytes +
	      scaleBytes, ELEMENTS_PER_POINT * numPoints);


	if (mat == NULL) {
	    mat = bn_mat_identity;
	}

	/* make point_t's from doubles and place in bu_list */
	for (i = 0; i < numPoints * ELEMENTS_PER_POINT; i += 3) {

	    BU_GETSTRUCT(point, pnt);
	    
	    MAT4X3PNT(point->v, mat, &pt[i]);

	    BU_LIST_PUSH(&(headPoint->l), &point->l);
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
    double scale, vCoord, hCoord;
    point_t a, b;

    RT_CK_DB_INTERNAL(internal);

    pnts = (struct rt_pnts_internal *) internal->idb_ptr;
    RT_PNTS_CK_MAGIC(pnts);

    if (pnts->count > 0) {
	point = (struct pnt *)pnts->point;
	head = &point->l;
	scale = pnts->scale;
    } else {
	return 0;
    }

    if (scale > 0) {
	/* set local database */
	db.idb_magic = RT_DB_INTERNAL_MAGIC;
	db.idb_major_type = ID_ELL;
	db.idb_ptr = &ell;

	/* set local ell for the pnts collection */
	ell.magic = RT_ELL_INTERNAL_MAGIC;

	VSET(ell.a, scale, 0, 0);
	VSET(ell.b, 0, scale, 0);
	VSET(ell.c, 0, 0, scale);

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
