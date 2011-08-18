/*                          P N T S . C
 * BRL-CAD
 *
 * Copyright (c) 2008-2011 United States Government as represented by
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
/** @file primitives/pnts/pnts.c
 *
 * Collection of points.
 *
 */

#include "common.h"

/* system headers */
#include "bin.h"

/* common headers */
#include "bn.h"
#include "bu.h"
#include "raytrace.h"
#include "rtgeom.h"
#include "vmath.h"


extern int rt_ell_plot(struct bu_list *, struct rt_db_internal *, const struct rt_tess_tol *, const struct bn_tol *);


HIDDEN unsigned char *
pnts_pack_double(unsigned char *buf, unsigned char *data, unsigned int count)
{
    htond(buf, data, count);
    buf += count * SIZEOF_NETWORK_DOUBLE;
    return buf;
}


HIDDEN unsigned char *
pnts_unpack_double(unsigned char *buf, unsigned char *data, unsigned int count)
{
    ntohd(data, buf, count);
    buf += count * SIZEOF_NETWORK_DOUBLE;
    return buf;
}

/**
 * R T _ P N T S _ B B O X
 *
 * Calculate a bounding box for a set of points
 */
int
rt_pnts_bbox(struct rt_db_internal *ip, point_t *min, point_t *max) {
    struct rt_pnts_internal *pnts;
    struct bu_list *head;
    struct pnt *point;
    point_t sph_min, sph_max;

    RT_CK_DB_INTERNAL(ip);
    pnts = (struct rt_pnts_internal *)ip->idb_ptr;
    RT_PNTS_CK_MAGIC(pnts);

    if (pnts->count > 0) {
	point = (struct pnt *)pnts->point;
	head = &point->l;
    } else {
	return 0;
    }

    if (pnts->scale > 0) {
	/* we're making spheres out of these, so the bbox
	 * has to take that into account */
	for (BU_LIST_FOR(point, pnt, head)) {
	    sph_min[X] = point->v[X] - pnts->scale;
	    sph_max[X] = point->v[X] + pnts->scale;
	    sph_min[Y] = point->v[Y] - pnts->scale;
	    sph_max[Y] = point->v[Y] + pnts->scale;
	    sph_min[Z] = point->v[Z] - pnts->scale;
	    sph_max[Z] = point->v[Z] + pnts->scale;
	    VMINMAX((*min), (*max), sph_min);
	    VMINMAX((*min), (*max), sph_max);
	}
    } else {
	for (BU_LIST_FOR(point, pnt, head)) {
	    VMINMAX((*min), (*max), point->v);
	}
    }
    return 0;
}

/**
 * R T _ P N T S _ E X P O R T 5
 *
 * Export a pnts collection from the internal structure to the
 * database format
 */
int
rt_pnts_export5(struct bu_external *external, const struct rt_db_internal *internal, double local2mm, const struct db_i *dbip)
{
    struct rt_pnts_internal *pnts = NULL;
    unsigned long pointDataSize;
    unsigned char *buf = NULL;

    if (dbip) RT_CK_DBI(dbip);

    /* acquire internal pnts structure */
    RT_CK_DB_INTERNAL(internal);
    BU_CK_EXTERNAL(external);
    external->ext_nbytes = 0;

    pnts = (struct rt_pnts_internal *) internal->idb_ptr;
    RT_PNTS_CK_MAGIC(pnts);

    /* allocate enough for the header (scale + type + count) */
    external->ext_nbytes = SIZEOF_NETWORK_DOUBLE + SIZEOF_NETWORK_SHORT + SIZEOF_NETWORK_LONG;
    external->ext_buf = (genptr_t) bu_calloc(sizeof(unsigned char), external->ext_nbytes, "pnts external");
    buf = (unsigned char *)external->ext_buf;

    htond(buf, (unsigned char *)&pnts->scale, 1);
    buf += SIZEOF_NETWORK_DOUBLE;
    *(uint16_t *)buf = htons((unsigned short)pnts->type);
    buf += SIZEOF_NETWORK_SHORT;
    *(uint32_t *)buf = htonl(pnts->count);
    buf += SIZEOF_NETWORK_LONG;

    if (pnts->count <= 0) {
	/* no points to stash, we're done */
	return 0;
    }

    /* figure out how much data there is for each point */
    pointDataSize = 3; /* v */
    if (pnts->type & RT_PNT_TYPE_COL) {
	pointDataSize += 3; /* c */
    }
    if (pnts->type & RT_PNT_TYPE_SCA) {
	pointDataSize += 1; /* s */
    }
    if (pnts->type & RT_PNT_TYPE_NRM) {
	pointDataSize += 3; /* n */
    }

    /* convert number of doubles to number of network bytes required to store doubles */
    pointDataSize = pointDataSize * SIZEOF_NETWORK_DOUBLE;

    external->ext_buf = (genptr_t)bu_realloc(external->ext_buf, external->ext_nbytes + (pnts->count * pointDataSize), "pnts external realloc");
    buf = (unsigned char *)external->ext_buf + external->ext_nbytes;
    external->ext_nbytes = external->ext_nbytes + (pnts->count * pointDataSize);

    /* get busy, serialize the point data depending on what type of point it is */
    switch (pnts->type) {
	case RT_PNT_TYPE_PNT: {
	    register struct pnt *point;

	    for (BU_LIST_FOR(point, pnt, &(((struct pnt *)pnts->point)->l))) {
		point_t v;

		/* pack v */
		VSCALE(v, point->v, local2mm);
		buf = pnts_pack_double(buf, (unsigned char *)v, ELEMENTS_PER_POINT);
	    }

	    break;
	}
	case RT_PNT_TYPE_COL: {
	    register struct pnt_color *point;

	    for (BU_LIST_FOR(point, pnt_color, &(((struct pnt_color *)pnts->point)->l))) {
		point_t v;
		double c[3];

		/* pack v */
		VSCALE(v, point->v, local2mm);
		buf = pnts_pack_double(buf, (unsigned char *)v, ELEMENTS_PER_POINT);

		/* pack c */
		bu_color_to_rgb_floats(&point->c, c);
		buf = pnts_pack_double(buf, (unsigned char *)c, 3);
	    }

	    break;
	}
	case RT_PNT_TYPE_SCA: {
	    register struct pnt_scale *point;

	    for (BU_LIST_FOR(point, pnt_scale, &(((struct pnt_scale *)pnts->point)->l))) {
		point_t v;
		double s[1];

		/* pack v */
		VSCALE(v, point->v, local2mm);
		buf = pnts_pack_double(buf, (unsigned char *)v, ELEMENTS_PER_POINT);

		/* pack s */
		s[0] = point->s * local2mm;
		buf = pnts_pack_double(buf, (unsigned char *)s, 1);
	    }

	    break;
	}
	case RT_PNT_TYPE_NRM: {
	    register struct pnt_normal *point;

	    for (BU_LIST_FOR(point, pnt_normal, &(((struct pnt_normal *)pnts->point)->l))) {
		point_t v;
		vect_t n;

		/* pack v */
		VSCALE(v, point->v, local2mm);
		buf = pnts_pack_double(buf, (unsigned char *)v, ELEMENTS_PER_POINT);

		/* pack n */
		VSCALE(n, point->n, local2mm);
		buf = pnts_pack_double(buf, (unsigned char *)n, ELEMENTS_PER_VECT);
	    }

	    break;
	}
	case RT_PNT_TYPE_COL_SCA: {
	    register struct pnt_color_scale *point;

	    for (BU_LIST_FOR(point, pnt_color_scale, &(((struct pnt_color_scale *)pnts->point)->l))) {
		point_t v;
		double c[3];
		double s[1];

		/* pack v */
		VSCALE(v, point->v, local2mm);
		buf = pnts_pack_double(buf, (unsigned char *)v, ELEMENTS_PER_POINT);

		/* pack c */
		bu_color_to_rgb_floats(&point->c, c);
		buf = pnts_pack_double(buf, (unsigned char *)c, 3);

		/* pack s */
		s[0] = point->s * local2mm;
		buf = pnts_pack_double(buf, (unsigned char *)s, 1);
	    }

	    break;
	}
	case RT_PNT_TYPE_COL_NRM: {
	    register struct pnt_color_normal *point;

	    for (BU_LIST_FOR(point, pnt_color_normal, &(((struct pnt_color_normal *)pnts->point)->l))) {
		point_t v;
		double c[3];
		vect_t n;

		/* pack v */
		VSCALE(v, point->v, local2mm);
		buf = pnts_pack_double(buf, (unsigned char *)v, ELEMENTS_PER_POINT);

		/* pack c */
		bu_color_to_rgb_floats(&point->c, c);
		buf = pnts_pack_double(buf, (unsigned char *)c, 3);

		/* pack n */
		VSCALE(n, point->n, local2mm);
		buf = pnts_pack_double(buf, (unsigned char *)n, ELEMENTS_PER_VECT);
	    }

	    break;
	}
	case RT_PNT_TYPE_SCA_NRM: {
	    register struct pnt_scale_normal *point;

	    for (BU_LIST_FOR(point, pnt_scale_normal, &(((struct pnt_scale_normal *)pnts->point)->l))) {
		point_t v;
		double s[1];
		vect_t n;

		/* pack v */
		VSCALE(v, point->v, local2mm);
		buf = pnts_pack_double(buf, (unsigned char *)v, ELEMENTS_PER_POINT);

		/* pack s */
		s[0] = point->s * local2mm;
		buf = pnts_pack_double(buf, (unsigned char *)s, 1);

		/* pack n */
		VSCALE(n, point->n, local2mm);
		buf = pnts_pack_double(buf, (unsigned char *)n, ELEMENTS_PER_VECT);
	    }

	    break;
	}
	case RT_PNT_TYPE_COL_SCA_NRM: {
	    register struct pnt_color_scale_normal *point;

	    for (BU_LIST_FOR(point, pnt_color_scale_normal, &(((struct pnt_color_scale_normal *)pnts->point)->l))) {
		point_t v;
		double c[3];
		double s[1];
		vect_t n;

		/* pack v */
		VSCALE(v, point->v, local2mm);
		buf = pnts_pack_double(buf, (unsigned char *)v, ELEMENTS_PER_POINT);

		/* pack c */
		bu_color_to_rgb_floats(&point->c, c);
		buf = pnts_pack_double(buf, (unsigned char *)c, 3);

		/* pack s */
		s[0] = point->s * local2mm;
		buf = pnts_pack_double(buf, (unsigned char *)s, 1);

		/* pack n */
		VSCALE(n, point->n, local2mm);
		buf = pnts_pack_double(buf, (unsigned char *)n, ELEMENTS_PER_VECT);
	    }

	    break;
	}
	default:
	    bu_log("ERROR: unknown points primitive type (type=%d)\n", pnts->type);
	    return 0;
    }

    return 0;
}


/**
 * R T _ P N T S _ I M P O R T 5
 *
 * Import a pnts collection from the database format to the internal
 * structure and apply modeling transformations.
 */
int
rt_pnts_import5(struct rt_db_internal *internal, const struct bu_external *external, const fastf_t *mat, const struct db_i *dbip)
{
    struct rt_pnts_internal *pnts = NULL;
    struct bu_list *head = NULL;
    unsigned char *buf = NULL;
    unsigned long i;

    if (dbip) RT_CK_DBI(dbip);

    RT_CK_DB_INTERNAL(internal);
    BU_CK_EXTERNAL(external);
    buf = (unsigned char *)external->ext_buf;

    /* initialize database structure */
    internal->idb_major_type = DB5_MAJORTYPE_BRLCAD;
    internal->idb_type = ID_PNTS;
    internal->idb_meth = &rt_functab[ID_PNTS];
    internal->idb_ptr = bu_malloc(sizeof(struct rt_pnts_internal), "rt_pnts_internal");

    /* initialize internal structure */
    pnts = (struct rt_pnts_internal *) internal->idb_ptr;
    pnts->magic = RT_PNTS_INTERNAL_MAGIC;
    pnts->point = NULL;

    /* unpack the header */
    ntohd((unsigned char *)&pnts->scale, buf, 1);
    buf += SIZEOF_NETWORK_DOUBLE;
    pnts->type = ntohs(*(uint16_t *)buf);
    buf += SIZEOF_NETWORK_SHORT;
    pnts->count = ntohl(*(uint32_t *)buf);
    buf += SIZEOF_NETWORK_LONG;

    if (pnts->count <= 0) {
	/* no points to read, we're done */
	return 0;
    }

    if (mat == NULL) {
	mat = bn_mat_identity;
    }

    /* get busy, deserialize the point data depending on what type of point it is */
    switch (pnts->type) {
	case RT_PNT_TYPE_PNT: {
	    register struct pnt *point;

	    BU_GETSTRUCT(point, pnt);
	    head = &point->l;
	    BU_LIST_INIT(head);
	    pnts->point = point;

	    for (i = 0; i < pnts->count; i++) {
		point_t v;

		BU_GETSTRUCT(point, pnt);

		/* unpack v */
		buf = pnts_unpack_double(buf, (unsigned char *)v, ELEMENTS_PER_POINT);
		MAT4X3PNT(point->v, mat, v);

		BU_LIST_PUSH(head, &point->l);
	    }

	    break;
	}
	case RT_PNT_TYPE_COL: {
	    register struct pnt_color *point;

	    BU_GETSTRUCT(point, pnt_color);
	    head = &point->l;
	    BU_LIST_INIT(head);
	    pnts->point = point;

	    for (i = 0; i < pnts->count; i++) {
		point_t v;
		double c[3];

		BU_GETSTRUCT(point, pnt_color);

		/* unpack v */
		buf = pnts_unpack_double(buf, (unsigned char *)v, ELEMENTS_PER_POINT);
		MAT4X3PNT(point->v, mat, v);

		/* unpack c */
		buf = pnts_unpack_double(buf, (unsigned char *)c, 3);
		bu_color_from_rgb_floats(&point->c, c);

		BU_LIST_PUSH(head, &point->l);
	    }

	    break;
	}
	case RT_PNT_TYPE_SCA: {
	    register struct pnt_scale *point;

	    BU_GETSTRUCT(point, pnt_scale);
	    head = &point->l;
	    BU_LIST_INIT(head);
	    pnts->point = point;

	    for (i = 0; i < pnts->count; i++) {
		point_t v;
		double s[1];

		BU_GETSTRUCT(point, pnt_scale);

		/* unpack v */
		buf = pnts_unpack_double(buf, (unsigned char *)v, ELEMENTS_PER_POINT);
		MAT4X3PNT(point->v, mat, v);

		/* unpack s */
		buf = pnts_unpack_double(buf, (unsigned char *)s, 1);
		point->s = s[0];

		BU_LIST_PUSH(head, &point->l);
	    }

	    break;
	}
	case RT_PNT_TYPE_NRM: {
	    register struct pnt_normal *point;

	    BU_GETSTRUCT(point, pnt_normal);
	    head = &point->l;
	    BU_LIST_INIT(head);
	    pnts->point = point;

	    for (i = 0; i < pnts->count; i++) {
		point_t v;
		vect_t n;

		BU_GETSTRUCT(point, pnt_normal);

		/* unpack v */
		buf = pnts_unpack_double(buf, (unsigned char *)v, ELEMENTS_PER_POINT);
		MAT4X3PNT(point->v, mat, v);

		/* unpack n */
		buf = pnts_unpack_double(buf, (unsigned char *)n, ELEMENTS_PER_VECT);
		MAT4X3PNT(point->n, mat, n);

		BU_LIST_PUSH(head, &point->l);
	    }

	    break;
	}
	case RT_PNT_TYPE_COL_SCA: {
	    register struct pnt_color_scale *point;

	    BU_GETSTRUCT(point, pnt_color_scale);
	    head = &point->l;
	    BU_LIST_INIT(head);
	    pnts->point = point;

	    for (i = 0; i < pnts->count; i++) {
		point_t v;
		double c[3];
		double s[1];

		BU_GETSTRUCT(point, pnt_color_scale);

		/* unpack v */
		buf = pnts_unpack_double(buf, (unsigned char *)v, ELEMENTS_PER_POINT);
		MAT4X3PNT(point->v, mat, v);

		/* unpack c */
		buf = pnts_unpack_double(buf, (unsigned char *)c, 3);
		bu_color_from_rgb_floats(&point->c, c);

		/* unpack s */
		buf = pnts_unpack_double(buf, (unsigned char *)s, 1);
		point->s = s[0];

		BU_LIST_PUSH(head, &point->l);
	    }

	    break;
	}
	case RT_PNT_TYPE_COL_NRM: {
	    register struct pnt_color_normal *point;

	    BU_GETSTRUCT(point, pnt_color_normal);
	    head = &point->l;
	    BU_LIST_INIT(head);
	    pnts->point = point;

	    for (i = 0; i < pnts->count; i++) {
		point_t v;
		double c[3];
		vect_t n;

		BU_GETSTRUCT(point, pnt_color_normal);

		/* unpack v */
		buf = pnts_unpack_double(buf, (unsigned char *)v, ELEMENTS_PER_POINT);
		MAT4X3PNT(point->v, mat, v);

		/* unpack c */
		buf = pnts_unpack_double(buf, (unsigned char *)c, 3);
		bu_color_from_rgb_floats(&point->c, c);

		/* unpack n */
		buf = pnts_unpack_double(buf, (unsigned char *)n, ELEMENTS_PER_VECT);
		MAT4X3PNT(point->n, mat, n);

		BU_LIST_PUSH(head, &point->l);
	    }

	    break;
	}
	case RT_PNT_TYPE_SCA_NRM: {
	    register struct pnt_scale_normal *point;

	    BU_GETSTRUCT(point, pnt_scale_normal);
	    head = &point->l;
	    BU_LIST_INIT(head);
	    pnts->point = point;

	    for (i = 0; i < pnts->count; i++) {
		point_t v;
		double s[1];
		vect_t n;

		BU_GETSTRUCT(point, pnt_scale_normal);

		/* unpack v */
		buf = pnts_unpack_double(buf, (unsigned char *)v, ELEMENTS_PER_POINT);
		MAT4X3PNT(point->v, mat, v);

		/* unpack s */
		buf = pnts_unpack_double(buf, (unsigned char *)s, 1);
		point->s = s[0];

		/* unpack n */
		buf = pnts_unpack_double(buf, (unsigned char *)n, ELEMENTS_PER_VECT);
		MAT4X3PNT(point->n, mat, n);

		BU_LIST_PUSH(head, &point->l);
	    }

	    break;
	}
	case RT_PNT_TYPE_COL_SCA_NRM: {
	    register struct pnt_color_scale_normal *point;

	    BU_GETSTRUCT(point, pnt_color_scale_normal);
	    head = &point->l;
	    BU_LIST_INIT(head);
	    pnts->point = point;

	    for (i = 0; i < pnts->count; i++) {
		point_t v;
		double c[3];
		double s[1];
		vect_t n;

		BU_GETSTRUCT(point, pnt_color_scale_normal);

		/* unpack v */
		buf = pnts_unpack_double(buf, (unsigned char *)v, ELEMENTS_PER_POINT);
		MAT4X3PNT(point->v, mat, v);

		/* unpack c */
		buf = pnts_unpack_double(buf, (unsigned char *)c, 3);
		bu_color_from_rgb_floats(&point->c, c);

		/* unpack s */
		buf = pnts_unpack_double(buf, (unsigned char *)s, 1);
		point->s = s[0];

		/* unpack n */
		buf = pnts_unpack_double(buf, (unsigned char *)n, ELEMENTS_PER_VECT);
		MAT4X3PNT(point->n, mat, n);

		BU_LIST_PUSH(head, &point->l);
	    }

	    break;
	}
	default:
	    bu_log("ERROR: unknown points primitive type (type=%d)\n", pnts->type);
	    return 0;
    }

    return 0;
}


/**
 * R T _ P N T S _ I F R E E
 *
 * Free the storage associated with the rt_db_internal version of the
 * collection.  This uses type aliasing to iterate over the list of
 * points as a bu_list instead of calling up a switching table for
 * each point type.
 */
void
rt_pnts_ifree(struct rt_db_internal *internal)
{
    struct rt_pnts_internal *pnts;
    register struct bu_list *point;

    RT_CK_DB_INTERNAL(internal);

    pnts = ((struct rt_pnts_internal *)(internal->idb_ptr));
    RT_PNTS_CK_MAGIC(pnts);

    /* since each point type has a bu_list as the first struct
     * element, we can treat them all as 'pnt' structs in order to
     * iterate over the bu_list and free them.
     */
    while (BU_LIST_WHILE(point, bu_list, &(((struct pnt *)pnts->point)->l))) {
	BU_LIST_DEQUEUE(point);
	bu_free(point, "free point");
    }

    /* free the head point */
    bu_free(pnts->point, "free head point");
    pnts->point = NULL; /* sanity */

    /* free the internal container */
    bu_free(internal->idb_ptr, "pnts ifree");
    internal->idb_ptr = GENPTR_NULL; /* sanity */
}


/**
 * R T _ P N T S _ P R I N T
 *
 */
void
rt_pnts_print(register const struct soltab *stp)
{
    register struct rt_pnts_internal *pnts;

    pnts = (struct rt_pnts_internal *)stp->st_specific;
    RT_PNTS_CK_MAGIC(pnts);

    switch (pnts->type) {
	case RT_PNT_TYPE_PNT: {
	    register struct pnt *point;
	    for (BU_LIST_FOR(point, pnt, &(((struct pnt *)pnts->point)->l))) {
	    }
	}
	case RT_PNT_TYPE_COL: {
	    register struct pnt_color *point;
	    for (BU_LIST_FOR(point, pnt_color, &(((struct pnt_color *)pnts->point)->l))) {
	    }
	}
	case RT_PNT_TYPE_SCA: {
	    register struct pnt_scale *point;
	    for (BU_LIST_FOR(point, pnt_scale, &(((struct pnt_scale *)pnts->point)->l))) {
	    }
	}
	case RT_PNT_TYPE_NRM: {
	    register struct pnt_normal *point;
	    for (BU_LIST_FOR(point, pnt_normal, &(((struct pnt_normal *)pnts->point)->l))) {
	    }
	}
	case RT_PNT_TYPE_COL_SCA: {
	    register struct pnt_color_scale *point;
	    for (BU_LIST_FOR(point, pnt_color_scale, &(((struct pnt_color_scale *)pnts->point)->l))) {
	    }
	}
	case RT_PNT_TYPE_COL_NRM: {
	    register struct pnt_color_normal *point;
	    for (BU_LIST_FOR(point, pnt_color_normal, &(((struct pnt_color_normal *)pnts->point)->l))) {
	    }
	}
	case RT_PNT_TYPE_SCA_NRM: {
	    register struct pnt_scale_normal *point;
	    for (BU_LIST_FOR(point, pnt_scale_normal, &(((struct pnt_scale_normal *)pnts->point)->l))) {
	    }
	}
	case RT_PNT_TYPE_COL_SCA_NRM: {
	    register struct pnt_color_scale_normal *point;
	    for (BU_LIST_FOR(point, pnt_color_scale_normal, &(((struct pnt_color_scale_normal *)pnts->point)->l))) {
	    }
	}
	default:
	    bu_log("ERROR: unknown points primitive type (type=%d)\n", pnts->type);
    }
}


/**
 * R T _ P N T S _ P L O T
 *
 * Plot pnts collection as axes or spheres.
 */
int
rt_pnts_plot(struct bu_list *vhead, struct rt_db_internal *internal, const struct rt_tess_tol *ttol, const struct bn_tol *tol)
{
    struct rt_pnts_internal *pnts;
    struct bu_list *head;
    struct rt_db_internal db;
    struct rt_ell_internal ell;
    struct pnt *point;
    double scale;
    point_t a, b;

    BU_CK_LIST_HEAD(vhead);
    RT_CK_DB_INTERNAL(internal);

    pnts = (struct rt_pnts_internal *)internal->idb_ptr;
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
	for (BU_LIST_FOR(point, pnt, head)) {
	    VMOVE(ell.v, point->v);
	    rt_ell_plot(vhead, &db, ttol, tol);
	}
    } else {
	double vCoord, hCoord;
	vCoord = hCoord = 1;

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


/**
 * R T _ P N T S _ D E S C R I B E
 *
 * Make human-readable formatted presentation of this primitive.  First
 * line describes type of solid.  Additional lines are indented one
 * tab, and give parameter values.
 */
int
rt_pnts_describe(struct bu_vls *str, const struct rt_db_internal *intern, int verbose, double mm2local)
{
    const struct rt_pnts_internal *pnts;
    double defaultSize = 0.0;
    unsigned long numPoints = 0;
    unsigned long loop_counter = 0;

    char buf[256]= {0};
    static const int BUF_SZ = 256;

    /* retrieve head record values */
    pnts = (struct rt_pnts_internal *) intern->idb_ptr;
    RT_PNTS_CK_MAGIC(pnts);

    defaultSize = pnts->scale;
    numPoints = pnts->count;

    bu_vls_strcat(str, "Point Cloud (PNTS)\n");

    if (!verbose) {
	return 1;
    }

    snprintf(buf, BUF_SZ, "Total number of points: %lu\nDefault scale: %f\n", numPoints, defaultSize);
    bu_vls_strcat(str, buf);

    loop_counter = 1;
    switch (pnts->type) {
	case RT_PNT_TYPE_PNT: {
	    register struct pnt *point;
	    bu_vls_strcat(str, "point#, (point)\n");
	    for (BU_LIST_FOR(point, pnt, &(((struct pnt *)pnts->point)->l))) {
		snprintf(buf, BUF_SZ, "%lu, \t (%f %f %f)\n",
			 (long unsigned)loop_counter,
			 point->v[X] * mm2local,
			 point->v[Y] * mm2local,
			 point->v[Z] * mm2local);
		bu_vls_strcat(str, buf);
		loop_counter++;
	    }
	    break;
	}
	case RT_PNT_TYPE_COL: {
	    register struct pnt_color *point;
	    bu_vls_strcat(str, "point#, (point), (color)\n");
	    for (BU_LIST_FOR(point, pnt_color, &(((struct pnt_color *)pnts->point)->l))) {
		snprintf(buf, BUF_SZ, "%lu, \t (%f %f %f), (%f %f %f)\n",
			 (long unsigned)loop_counter,
			 point->v[X] * mm2local,
			 point->v[Y] * mm2local,
			 point->v[Z] * mm2local,
			 point->c.buc_rgb[0],
			 point->c.buc_rgb[1],
			 point->c.buc_rgb[2]);
		bu_vls_strcat(str, buf);
		loop_counter++;
	    }
	    break;
	}
	case RT_PNT_TYPE_SCA: {
	    register struct pnt_scale *point;
	    bu_vls_strcat(str, "point#, (point), (scale)\n");
	    for (BU_LIST_FOR(point, pnt_scale, &(((struct pnt_scale *)pnts->point)->l))) {
		snprintf(buf, BUF_SZ, "%lu, \t (%f %f %f), (%f)\n",
			 (long unsigned)loop_counter,
			 point->v[X] * mm2local,
			 point->v[Y] * mm2local,
			 point->v[Z] * mm2local,
			 point->s);
		bu_vls_strcat(str, buf);
		loop_counter++;
	    }
	    break;
	}
	case RT_PNT_TYPE_NRM: {
	    register struct pnt_normal *point;
	    bu_vls_strcat(str, "point#, (point), (normal)\n");
	    for (BU_LIST_FOR(point, pnt_normal, &(((struct pnt_normal *)pnts->point)->l))) {
		snprintf(buf, BUF_SZ, "%lu, \t (%f %f %f), (%f %f %f)\n",
			 (long unsigned)loop_counter,
			 point->v[X] * mm2local,
			 point->v[Y] * mm2local,
			 point->v[Z] * mm2local,
			 point->n[X],
			 point->n[Y],
			 point->n[Z]);
		bu_vls_strcat(str, buf);
		loop_counter++;
	    }
	    break;
	}
	case RT_PNT_TYPE_COL_SCA: {
	    register struct pnt_color_scale *point;
	    bu_vls_strcat(str, "point#, (point), (color), (scale)\n");
	    for (BU_LIST_FOR(point, pnt_color_scale, &(((struct pnt_color_scale *)pnts->point)->l))) {
		snprintf(buf, BUF_SZ, "%lu, \t (%f %f %f), (%f %f %f), (%f)\n",
			 (long unsigned)loop_counter,
			 point->v[X] * mm2local,
			 point->v[Y] * mm2local,
			 point->v[Z] * mm2local,
			 point->c.buc_rgb[0],
			 point->c.buc_rgb[1],
			 point->c.buc_rgb[2],
			 point->s);
		bu_vls_strcat(str, buf);
		loop_counter++;
	    }
	    break;
	}
	case RT_PNT_TYPE_COL_NRM: {
	    register struct pnt_color_normal *point;
	    bu_vls_strcat(str, "point#, (point), (color), (normal)\n");
	    for (BU_LIST_FOR(point, pnt_color_normal, &(((struct pnt_color_normal *)pnts->point)->l))) {
		snprintf(buf, BUF_SZ, "%lu, \t (%f %f %f), (%f %f %f), (%f %f %f)\n",
			 (long unsigned)loop_counter,
			 point->v[X] * mm2local,
			 point->v[Y] * mm2local,
			 point->v[Z] * mm2local,
			 point->c.buc_rgb[0],
			 point->c.buc_rgb[1],
			 point->c.buc_rgb[2],
			 point->n[X],
			 point->n[Y],
			 point->n[Z]);
		bu_vls_strcat(str, buf);
		loop_counter++;
	    }
	    break;
	}
	case RT_PNT_TYPE_SCA_NRM: {
	    register struct pnt_scale_normal *point;
	    bu_vls_strcat(str, "point#, (point), (scale), (normal)\n");
	    for (BU_LIST_FOR(point, pnt_scale_normal, &(((struct pnt_scale_normal *)pnts->point)->l))) {
		snprintf(buf, BUF_SZ, "%lu, \t (%f %f %f), (%f), (%f %f %f)\n",
			 (long unsigned)loop_counter,
			 point->v[X] * mm2local,
			 point->v[Y] * mm2local,
			 point->v[Z] * mm2local,
			 point->s,
			 point->n[X],
			 point->n[Y],
			 point->n[Z]);
		bu_vls_strcat(str, buf);
		loop_counter++;
	    }
	    break;
	}
	case RT_PNT_TYPE_COL_SCA_NRM: {
	    register struct pnt_color_scale_normal *point;
	    bu_vls_strcat(str, "point#, (point), (color), (scale), (normal)\n");
	    for (BU_LIST_FOR(point, pnt_color_scale_normal, &(((struct pnt_color_scale_normal *)pnts->point)->l))) {
		snprintf(buf, BUF_SZ, "%lu, \t (%f %f %f), (%f %f %f), (%f), (%f %f %f)\n",
			 (long unsigned)loop_counter,
			 point->v[X] * mm2local,
			 point->v[Y] * mm2local,
			 point->v[Z] * mm2local,
			 point->c.buc_rgb[0],
			 point->c.buc_rgb[1],
			 point->c.buc_rgb[2],
			 point->s,
			 point->n[X],
			 point->n[Y],
			 point->n[Z]);
		bu_vls_strcat(str, buf);
		loop_counter++;
	    }
	    break;
	}
	default:
	    bu_log("ERROR: unknown points primitive type (type=%d)\n", pnts->type);
	    return 1;
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
