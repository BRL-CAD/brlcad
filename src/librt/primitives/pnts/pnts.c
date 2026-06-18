/*                          P N T S . C
 * BRL-CAD
 *
 * Copyright (c) 2008-2026 United States Government as represented by
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
#include <math.h>
#include "bnetwork.h"

/* common headers */
#include "bu/cv.h"
#include "bu/opt.h"
#include "bn.h"

#include "raytrace.h"
#include "rt/geom.h"
#include "vmath.h"

#define PNT_FMT "%.17g"    /* format precision for printf */
#define PNT_FMT_3ARGS PNT_FMT " " PNT_FMT " " PNT_FMT

/**
 * Per-soltab data computed at prep time, used to ray-trace the point
 * cloud as a collection of spheres.  Values are copied out of the
 * rt_db_internal so they remain valid after the internal is freed.
 */
struct pnts_specific {
    size_t count;		/* number of renderable spheres */
    point_t *centers;		/* [count] sphere centers (model space) */
    fastf_t *radii;		/* [count] sphere radii */
};


__BEGIN_DECLS
extern int rt_ell_plot(struct bu_list *, struct rt_db_internal *, const struct bg_tess_tol *, const struct bn_tol *, const struct bview *);
__END_DECLS

static unsigned char *
pnts_pack_double(unsigned char *buf, unsigned char *data, unsigned int count)
{
    bu_cv_htond(buf, data, count);
    buf += count * SIZEOF_NETWORK_DOUBLE;
    return buf;
}


static unsigned char *
pnts_unpack_double(unsigned char *buf, unsigned char *data, unsigned int count)
{
    bu_cv_ntohd(data, buf, count);
    buf += count * SIZEOF_NETWORK_DOUBLE;
    return buf;
}

static void
_pnts_calc_bbox(point_t *min, point_t *max, point_t *v, fastf_t scale)
{
    point_t sph_min, sph_max;
    if (!min || !max || !v) return;
    if (scale > 0) {
	/* we're making spheres out of these, so the bbox
	 * has to take that into account */
	sph_min[X] = (*v)[X] - scale;
	sph_max[X] = (*v)[X] + scale;
	sph_min[Y] = (*v)[Y] - scale;
	sph_max[Y] = (*v)[Y] + scale;
	sph_min[Z] = (*v)[Z] - scale;
	sph_max[Z] = (*v)[Z] + scale;
	VMINMAX((*min), (*max), sph_min);
	VMINMAX((*min), (*max), sph_max);

    } else {
	VMINMAX((*min), (*max), (*v));
    }
}

/**
 * True if 'type' is one of the recognized point types.  Used to keep
 * the unknown-type error paths intact now that the per-type switches
 * have been collapsed into generic, flag-driven loops.
 */
static int
pnts_type_is_valid(rt_pnt_type type)
{
    return (type >= RT_PNT_TYPE_PNT && type <= RT_PNT_TYPE_COL_SCA_NRM);
}

/**
 * Return the expected data size depending on point type
 */
static int
pnts_data_size(rt_pnt_type type) {
    int dataSize = 3;	    /* v [x y z] */

    if (type & RT_PNT_TYPE_COL) dataSize += 3; /* c [r g b]  */
    if (type & RT_PNT_TYPE_SCA) dataSize += 1; /* s [double] */
    if (type & RT_PNT_TYPE_NRM) dataSize += 3; /* n [i j k]  */

    return dataSize;
}

/**
 * Allocate a zeroed point node of the storage type appropriate for the
 * given pnts type.
 * Returns void* assuming caller is dispatching indiscriminatory to the type
 */
static void*
pnts_node_alloc(rt_pnt_type type)
{
    void *node = NULL;

    switch (type) {
	case RT_PNT_TYPE_PNT: {
	    struct pnt *p; BU_ALLOC(p, struct pnt); node = p; break;
	}
	case RT_PNT_TYPE_COL: {
	    struct pnt_color *p; BU_ALLOC(p, struct pnt_color); node = p; break;
	}
	case RT_PNT_TYPE_SCA: {
	    struct pnt_scale *p; BU_ALLOC(p, struct pnt_scale); node = p; break;
	}
	case RT_PNT_TYPE_NRM: {
	    struct pnt_normal *p; BU_ALLOC(p, struct pnt_normal); node = p; break;
	}
	case RT_PNT_TYPE_COL_SCA: {
	    struct pnt_color_scale *p; BU_ALLOC(p, struct pnt_color_scale); node = p; break;
	}
	case RT_PNT_TYPE_COL_NRM: {
	    struct pnt_color_normal *p; BU_ALLOC(p, struct pnt_color_normal); node = p; break;
	}
	case RT_PNT_TYPE_SCA_NRM: {
	    struct pnt_scale_normal *p; BU_ALLOC(p, struct pnt_scale_normal); node = p; break;
	}
	case RT_PNT_TYPE_COL_SCA_NRM: {
	    struct pnt_color_scale_normal *p; BU_ALLOC(p, struct pnt_color_scale_normal); node = p; break;
	}
	default:
	    break;
    }

    return node;
}

/**
 * Collapse (color / scale / normal) access for the pnt types
 * Returns the pointer to the type-specific field (or NULL)
 */
static struct bu_color *
pnts_node_color(void *node, rt_pnt_type type)
{
    switch (type) {
	case RT_PNT_TYPE_COL:		return &((struct pnt_color *)node)->c;
	case RT_PNT_TYPE_COL_SCA:	return &((struct pnt_color_scale *)node)->c;
	case RT_PNT_TYPE_COL_NRM:	return &((struct pnt_color_normal *)node)->c;
	case RT_PNT_TYPE_COL_SCA_NRM:	return &((struct pnt_color_scale_normal *)node)->c;
	default:			return NULL;
    }
}

static fastf_t *
pnts_node_scale(void *node, rt_pnt_type type)
{
    switch (type) {
	case RT_PNT_TYPE_SCA:		return &((struct pnt_scale *)node)->s;
	case RT_PNT_TYPE_COL_SCA:	return &((struct pnt_color_scale *)node)->s;
	case RT_PNT_TYPE_SCA_NRM:	return &((struct pnt_scale_normal *)node)->s;
	case RT_PNT_TYPE_COL_SCA_NRM:	return &((struct pnt_color_scale_normal *)node)->s;
	default:			return NULL;
    }
}

static fastf_t *
pnts_node_normal(void *node, rt_pnt_type type)
{
    switch (type) {
	case RT_PNT_TYPE_NRM:		return ((struct pnt_normal *)node)->n;
	case RT_PNT_TYPE_COL_NRM:	return ((struct pnt_color_normal *)node)->n;
	case RT_PNT_TYPE_SCA_NRM:	return ((struct pnt_scale_normal *)node)->n;
	case RT_PNT_TYPE_COL_SCA_NRM:	return ((struct pnt_color_scale_normal *)node)->n;
	default:			return NULL;
    }
}

/**
 * Populate a point node's fields from the decomposed values.  Only the
 * fields the node's type actually carries are written.
 */
static void
pnts_node_set(void *node, rt_pnt_type type, const point_t v, fastf_t *cf, fastf_t s, const vect_t n)
{
    struct bu_color *c = pnts_node_color(node, type);
    fastf_t *sp = pnts_node_scale(node, type);
    fastf_t *np = pnts_node_normal(node, type);

    VMOVE(((struct pnt *)node)->v, v);
    if (c)  bu_color_from_rgb_floats(c, cf);
    if (sp) *sp = s;
    if (np) VMOVE(np, n);
}

/**
 * Free any existing point list (head sentinel and all nodes) held by a
 * pnts collection, leaving pnts->point NULL.
 */
static void
pnts_data_free(struct rt_pnts_internal *pnts)
{
    struct bu_list *bp;

    if (!pnts->point)
	return;

    /* since each point type has a bu_list as the first struct
     * element, we can treat them all as 'pnt' structs in order to
     * iterate over the bu_list and free them.
     */
    while (BU_LIST_WHILE(bp, bu_list, &(((struct pnt *)pnts->point)->l))) {
	BU_LIST_DEQUEUE(bp);
	bu_free(bp, "pnts point");
    }

    /* free the head point */
    bu_free(pnts->point, "pnts head point");
    pnts->point = NULL; /* sanity */
}


/**
 * Calculate a bounding box for a set of points
 */
C_DECL int
rt_pnts_bbox(struct rt_db_internal *ip, point_t *min, point_t *max, const struct bn_tol *UNUSED(tol)) {
    struct rt_pnts_internal *pnts;
    struct pnt *point;
    struct bu_list *head;

    RT_CK_DB_INTERNAL(ip);
    pnts = (struct rt_pnts_internal *)ip->idb_ptr;
    RT_PNTS_CK_MAGIC(pnts);

    if (pnts->count <= 0) {
	return 0;
    }
    if (!pnts_type_is_valid(pnts->type)) {
	return 0;
    }

    VSETALL((*min), INFINITY);
    VSETALL((*max), -INFINITY);

    /* every node type shares the leading 'l' and 'v' members, so walk
     * the list generically as 'struct pnt' regardless of type.  'head'
     * must be a stable pointer to the sentinel: BU_LIST_FOR reassigns
     * 'point', so it can't double as the head expression. */
    point = (struct pnt *)pnts->point;
    head = &point->l;
    for (BU_LIST_FOR(point, pnt, head)) {
	_pnts_calc_bbox(min, max, &(point->v), pnts->scale);
    }

    return 0;
}

C_DECL int
rt_pnts_prep(struct soltab *stp, struct rt_db_internal *ip, struct rt_i *rtip)
{
    struct rt_pnts_internal *pnts_ip;
    struct pnts_specific *spec;

    RT_CK_DB_INTERNAL(ip);
    pnts_ip = (struct rt_pnts_internal *)ip->idb_ptr;
    RT_PNTS_CK_MAGIC(pnts_ip);

    if (rt_pnts_bbox(ip, &(stp->st_min), &(stp->st_max), &(rtip->rti_tol))) return 1;

    /* Compute bounding sphere which contains the bounding RPP.*/
    {
	vect_t work;
	register fastf_t f;

	VADD2SCALE(stp->st_center, stp->st_min, stp->st_max, 0.5);
	VSUB2SCALE(work, stp->st_max, stp->st_min, 0.5);

	f = work[X];
	if (work[Y] > f) f = work[Y];
	if (work[Z] > f) f = work[Z];
	stp->st_aradius = f;
	stp->st_bradius = MAGNITUDE(work);
    }

    /* Build the renderable sphere list.  Each point with a positive
     * radius (per-point scale where available, otherwise the global
     * scale) becomes a sphere.  We copy values out of ip since the RT
     * pipeline frees the internal after prep.
     */
    BU_GET(spec, struct pnts_specific);
    spec->count = 0;
    spec->centers = NULL;
    spec->radii = NULL;

    if (pnts_ip->count > 0 && pnts_type_is_valid(pnts_ip->type)) {
	struct pnt *point;
	struct bu_list *head;

	spec->centers = (point_t *)bu_malloc(pnts_ip->count * sizeof(point_t), "pnts centers");
	spec->radii = (fastf_t *)bu_malloc(pnts_ip->count * sizeof(fastf_t), "pnts radii");

	/* each point becomes a sphere using its per-point scale where the
	 * type carries one, otherwise the global scale; zero/negative
	 * radii are not renderable and are skipped. */
	point = (struct pnt *)pnts_ip->point;
	head = &point->l;
	for (BU_LIST_FOR(point, pnt, head)) {
	    fastf_t *sp = pnts_node_scale(point, pnts_ip->type);
	    fastf_t r = sp ? *sp : pnts_ip->scale;
	    if (r > 0) {
		VMOVE(spec->centers[spec->count], point->v);
		spec->radii[spec->count] = r;
		spec->count++;
	    }
	}
    }

    stp->st_specific = (void *)spec;

    return 0;
}


/**
 * Intersect a ray with the point cloud, treating each scaled point as
 * a sphere.  Tests every sphere against the ray (O(n) per ray).
 *
 * Returns -
 * 0 MISS
 * >0 HIT (number of segments)
 */
C_DECL int
rt_pnts_shot(struct soltab *stp, struct xray *rp, struct application *ap, struct seg *seghead)
{
    struct pnts_specific *spec = (struct pnts_specific *)stp->st_specific;
    struct seg *segp;
    size_t i;
    int nseg = 0;

    if (!spec || spec->count == 0) return 0;

    for (i = 0; i < spec->count; i++) {
	vect_t oc;		/* ray origin to sphere center */
	fastf_t b, c, disc, s;
	fastf_t t_in, t_out;

	VSUB2(oc, rp->r_pt, spec->centers[i]);
	b = VDOT(oc, rp->r_dir);
	c = VDOT(oc, oc) - spec->radii[i] * spec->radii[i];
	disc = b * b - c;
	if (disc < 0) continue;

	s = sqrt(disc);
	t_in = -b - s;
	t_out = -b + s;
	if (t_out < 1.0e-6) continue;

	RT_GET_SEG(segp, ap->a_resource);
	segp->seg_stp = stp;
	segp->seg_in.hit_dist = t_in;
	segp->seg_in.hit_surfno = (int)i;
	segp->seg_out.hit_dist = t_out;
	segp->seg_out.hit_surfno = (int)i;
	BU_LIST_INSERT(&(seghead->l), &(segp->l));
	nseg++;
    }

    return nseg;
}


/**
 * Given ONE ray distance, return the normal and entry/exit point for
 * the sphere that was hit.
 */
C_DECL void
rt_pnts_norm(struct hit *hitp, struct soltab *stp, struct xray *rp)
{
    struct pnts_specific *spec = (struct pnts_specific *)stp->st_specific;
    size_t i = (size_t)hitp->hit_surfno;

    VJOIN1(hitp->hit_point, rp->r_pt, hitp->hit_dist, rp->r_dir);
    VSUB2(hitp->hit_normal, hitp->hit_point, spec->centers[i]);
    VUNITIZE(hitp->hit_normal);
}


/**
 * Trivial UV mapping for the point cloud.
 */
C_DECL void
rt_pnts_uv(struct application *UNUSED(ap), struct soltab *UNUSED(stp), struct hit *UNUSED(hitp), struct uvcoord *uvp)
{
    uvp->uv_u = 0.0;
    uvp->uv_v = 0.0;
    uvp->uv_du = 0.0;
    uvp->uv_dv = 0.0;
}


/**
 * Free the prep-time sphere list.
 */
C_DECL void
rt_pnts_free(struct soltab *stp)
{
    struct pnts_specific *spec = (struct pnts_specific *)stp->st_specific;

    if (spec) {
	if (spec->centers) bu_free(spec->centers, "pnts centers");
	if (spec->radii) bu_free(spec->radii, "pnts radii");
	BU_PUT(spec, struct pnts_specific);
    }
}

/**
 * Export a pnts collection from the internal structure to the
 * database format
 */
C_DECL int
rt_pnts_export5(struct bu_external *external, const struct rt_db_internal *internal, double local2mm, const struct db_i *dbip)
{
    struct rt_pnts_internal *pnts = NULL;
    unsigned long pointDataSize;
    unsigned char *buf = NULL;

    /* must be double for import and export */
    double scan;

    if (dbip) RT_CK_DBI(dbip);

    /* acquire internal pnts structure */
    RT_CK_DB_INTERNAL(internal);
    BU_CK_EXTERNAL(external);
    external->ext_nbytes = 0;

    pnts = (struct rt_pnts_internal *) internal->idb_ptr;
    RT_PNTS_CK_MAGIC(pnts);

    if (!pnts_type_is_valid(pnts->type)) {
	bu_log("ERROR: unknown points primitive type (type=%d)\n", pnts->type);
	return 0;
    }

    /* allocate enough for the header (scale + type + count) */
    external->ext_nbytes = SIZEOF_NETWORK_DOUBLE + SIZEOF_NETWORK_SHORT + SIZEOF_NETWORK_LONG;
    external->ext_buf = (uint8_t *) bu_calloc(sizeof(unsigned char), external->ext_nbytes, "pnts external");
    buf = (unsigned char *)external->ext_buf;

    scan = pnts->scale; /* convert fastf_t to double */
    bu_cv_htond(buf, (unsigned char *)&scan, 1);
    buf += SIZEOF_NETWORK_DOUBLE;
    *(uint16_t *)buf = htons((unsigned short)pnts->type);
    buf += SIZEOF_NETWORK_SHORT;
    *(uint32_t *)buf = htonl(pnts->count);

    if (pnts->count <= 0) {
	/* no points to stash, we're done */
	return 0;
    }

    /* figure out how much data there is for each point */
    pointDataSize = pnts_data_size(pnts->type);

    /* convert number of doubles to number of network bytes required to store doubles */
    pointDataSize = pointDataSize * SIZEOF_NETWORK_DOUBLE;

    external->ext_buf = (uint8_t *)bu_realloc(external->ext_buf, external->ext_nbytes + (pnts->count * pointDataSize), "pnts external realloc");
    buf = (unsigned char *)external->ext_buf + external->ext_nbytes;
    external->ext_nbytes = external->ext_nbytes + (pnts->count * pointDataSize);

    /* get busy, serialize the point data depending on what type of point it is */
    {
	struct pnt *point = (struct pnt *)pnts->point;
	struct bu_list *head = &point->l;

	for (BU_LIST_FOR(point, pnt, head)) {
	    struct bu_color *col = pnts_node_color(point, pnts->type);
	    fastf_t *sp = pnts_node_scale(point, pnts->type);
	    fastf_t *np = pnts_node_normal(point, pnts->type);
	    double v[3];

	    /* pack v */
	    VSCALE(v, point->v, local2mm);
	    buf = pnts_pack_double(buf, (unsigned char *)v, ELEMENTS_PER_POINT);

	    /* pack c */
	    if (col) {
		double c[3];
		fastf_t cf[3];
		bu_color_to_rgb_floats(col, cf);
		VMOVE(c, cf);
		buf = pnts_pack_double(buf, (unsigned char *)c, 3);
	    }

	    /* pack s */
	    if (sp) {
		double s[1];
		s[0] = *sp * local2mm;
		buf = pnts_pack_double(buf, (unsigned char *)s, 1);
	    }

	    /* pack n */
	    if (np) {
		double n[3];
		VSCALE(n, np, local2mm);
		buf = pnts_pack_double(buf, (unsigned char *)n, ELEMENTS_PER_VECT);
	    }
	}
    }

    return 0;
}

C_DECL int
rt_pnts_mat(struct rt_db_internal *rop, const mat_t mat, const struct rt_db_internal *ip)
{
    if (!rop || !mat)
	return BRLCAD_OK;

    // For the moment, we only support applying a mat to a pnts primitive in place - the
    // input and output must be the same.
    if (ip && rop != ip) {
	bu_log("rt_pnts_mat:  alignment of points between multiple pnts objects is unsupported - input pnts must be the same as the output pnts.\n");
	return BRLCAD_ERROR;
    }

    struct rt_pnts_internal *pnts = (struct rt_pnts_internal *)rop->idb_ptr;
    RT_PNTS_CK_MAGIC(pnts);

    if (!pnts_type_is_valid(pnts->type)) {
	bu_log("rt_pnts_mat: unknown points primitive type (type=%d)\n", pnts->type);
	return BRLCAD_ERROR;
    }

    /* every node type shares the leading 'l' and 'v' members; transform
     * v for all, and n only for the types that carry a normal. */
    struct pnt *point = (struct pnt *)pnts->point;
    struct bu_list *head = &point->l;
    for (BU_LIST_FOR(point, pnt, head)) {
	point_t v;
	fastf_t *np = pnts_node_normal(point, pnts->type);

	VMOVE(v, point->v);
	MAT4X3PNT(point->v, mat, v);

	if (np) {
	    vect_t n;
	    VMOVE(n, np);
	    MAT4X3PNT(np, mat, n);
	}
    }

    return BRLCAD_OK;
}

/**
 * Import a pnts collection from the database format to the internal
 * structure and apply modeling transformations.
 */
C_DECL int
rt_pnts_import5(struct rt_db_internal *internal, const struct bu_external *external, const fastf_t *mat, const struct db_i *dbip)
{
    struct rt_pnts_internal *pnts = NULL;
    struct bu_list *head = NULL;
    unsigned char *buf = NULL;
    unsigned long i;
    uint16_t type;

    /* must be double for import and export */
    double scan;

    if (dbip) RT_CK_DBI(dbip);

    RT_CK_DB_INTERNAL(internal);
    BU_CK_EXTERNAL(external);
    buf = (unsigned char *)external->ext_buf;

    /* initialize database structure */
    internal->idb_major_type = DB5_MAJORTYPE_BRLCAD;
    internal->idb_type = ID_PNTS;
    internal->idb_meth = &OBJ[ID_PNTS];
    BU_ALLOC(internal->idb_ptr, struct rt_pnts_internal);

    /* initialize internal structure */
    pnts = (struct rt_pnts_internal *) internal->idb_ptr;
    pnts->magic = RT_PNTS_INTERNAL_MAGIC;
    pnts->point = NULL;

    /* unpack the header */
    bu_cv_ntohd((unsigned char *)&scan, buf, 1);
    pnts->scale = scan; /* convert double to fastf_t */
    buf += SIZEOF_NETWORK_DOUBLE;
    type = ntohs(*(uint16_t *)buf);
    pnts->type = (rt_pnt_type)type; /* intentional enum coercion */
    buf += SIZEOF_NETWORK_SHORT;
    pnts->count = ntohl(*(uint32_t *)buf);
    buf += SIZEOF_NETWORK_LONG;

    if (pnts->count <= 0) {
	/* no points to read, we're done */
	return 0;
    }


    if (!pnts_type_is_valid(pnts->type)) {
	bu_log("ERROR: unknown points primitive type (type=%d)\n", pnts->type);
	return 0;
    }

    /* get busy, deserialize the point data.  Every node type shares the
     * leading 'l' and 'v' members, so allocate the head/sentinel then
     * pull out v and whatever optional c/s/n the type carries (matching
     * the order written by rt_pnts_export5). */
    {
	void *point = pnts_node_alloc(pnts->type);

	head = &((struct pnt *)point)->l;
	BU_LIST_INIT(head);
	pnts->point = point;

	for (i = 0; i < pnts->count; i++) {
	    double v[3];
	    point_t pv = VINIT_ZERO;
	    fastf_t cf[3] = {0.0, 0.0, 0.0};
	    fastf_t s = 0.0;
	    vect_t nrm = VINIT_ZERO;

	    point = pnts_node_alloc(pnts->type);

	    /* unpack v */
	    buf = pnts_unpack_double(buf, (unsigned char *)v, ELEMENTS_PER_POINT);
	    VMOVE(pv, v);

	    /* unpack c */
	    if (pnts->type & RT_PNT_TYPE_COL) {
		double c[3];
		buf = pnts_unpack_double(buf, (unsigned char *)c, 3);
		VMOVE(cf, c);
	    }

	    /* unpack s */
	    if (pnts->type & RT_PNT_TYPE_SCA) {
		double sd[1];
		buf = pnts_unpack_double(buf, (unsigned char *)sd, 1);
		s = sd[0];
	    }

	    /* unpack n */
	    if (pnts->type & RT_PNT_TYPE_NRM) {
		double n[3];
		buf = pnts_unpack_double(buf, (unsigned char *)n, ELEMENTS_PER_VECT);
		VMOVE(nrm, n);
	    }

	    pnts_node_set(point, pnts->type, pv, cf, s, nrm);
	    BU_LIST_INSERT(head, &((struct pnt *)point)->l);
	}
    }

    /* Apply transform */
    if (mat == NULL) mat = bn_mat_identity;
    return rt_pnts_mat(internal, mat, NULL);
}


/**
 * Free the storage associated with the rt_db_internal version of the
 * collection.  This uses type aliasing to iterate over the list of
 * points as a bu_list instead of calling up a switching table for
 * each point type.
 */
C_DECL void
rt_pnts_ifree(struct rt_db_internal *internal)
{
    struct rt_pnts_internal *pnts;
    register struct bu_list *point;

    RT_CK_DB_INTERNAL(internal);

    pnts = ((struct rt_pnts_internal *)(internal->idb_ptr));
    RT_PNTS_CK_MAGIC(pnts);

    if (pnts->count == 0) {
	return;
    }

    pnts_data_free(pnts);

    /* free the internal container */
    bu_free(internal->idb_ptr, "pnts ifree");
    internal->idb_ptr = ((void *)0); /* sanity */
}


C_DECL void
rt_pnts_print(register const struct soltab *stp)
{
    register const struct pnts_specific *spec =
	(struct pnts_specific *)stp->st_specific;

    if (!spec) return;

    bu_log("pnts: %zu renderable sphere(s)\n", spec->count);
}


/**
 * Plot pnts collection as axes or spheres.
 */
C_DECL int
rt_pnts_plot(struct bu_list *vhead, struct rt_db_internal *internal, const struct bg_tess_tol *ttol, const struct bn_tol *tol, const struct bview *UNUSED(info))
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

    struct bu_list *vlfree = &rt_vlfree;
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
	    rt_ell_plot(vhead, &db, ttol, tol, NULL);
	}
    } else {
	double vCoord, hCoord;
	vCoord = hCoord = 1;

	for (BU_LIST_FOR(point, pnt, head)) {
	    /* draw first horizontal segment for this point */
	    VSET(a, point->v[X] - hCoord, point->v[Y], point->v[Z]);
	    VSET(b, point->v[X] + hCoord, point->v[Y], point->v[Z]);
	    BV_ADD_VLIST(vlfree, vhead, a, BV_VLIST_LINE_MOVE);
	    BV_ADD_VLIST(vlfree, vhead, b, BV_VLIST_LINE_DRAW);

	    /* draw perpendicular horizontal segment */
	    VSET(a, point->v[X], point->v[Y] - hCoord, point->v[Z]);
	    VSET(b, point->v[X], point->v[Y] + hCoord, point->v[Z]);
	    BV_ADD_VLIST(vlfree, vhead, a, BV_VLIST_LINE_MOVE);
	    BV_ADD_VLIST(vlfree, vhead, b, BV_VLIST_LINE_DRAW);

	    /* draw vertical segment */
	    VSET(a, point->v[X], point->v[Y], point->v[Z] - vCoord);
	    VSET(b, point->v[X], point->v[Y], point->v[Z] + vCoord);
	    BV_ADD_VLIST(vlfree, vhead, a, BV_VLIST_LINE_MOVE);
	    BV_ADD_VLIST(vlfree, vhead, b, BV_VLIST_LINE_DRAW);
	}
    }

    return 0;
}


/**
 * Make human-readable formatted presentation of this primitive.  First
 * line describes type of solid.  Additional lines are indented one
 * tab, and give parameter values.
 */
C_DECL int
rt_pnts_describe(struct bu_vls *str, const struct rt_db_internal *intern, int verbose, double mm2local)
{
    const struct rt_pnts_internal *pnts;
    struct pnt *point;
    struct bu_list *head;
    double defaultSize = 0.0;
    unsigned long numPoints = 0;
    unsigned long loop_counter = 0;

    /* retrieve head record values */
    pnts = (struct rt_pnts_internal *) intern->idb_ptr;
    RT_PNTS_CK_MAGIC(pnts);

    defaultSize = pnts->scale;
    numPoints = pnts->count;

    bu_vls_strcat(str, "Point Cloud (PNTS)\n");

    /* NOTE: this value is not arbitrary, but is dependent on the init in libged/list/list.c */
    if (verbose <= 99) {
	bu_vls_strcat(str, "    use -v (verbose) for all data\n");
	return 1;
    }

    bu_vls_printf(str, "Total number of points: %lu\nDefault scale: %f\n", numPoints, defaultSize);

    if (pnts->count == 0) {
	return 0;
    }
    if (!pnts_type_is_valid(pnts->type)) {
	bu_log("ERROR: unknown points primitive type (type=%d)\n", pnts->type);
	return 1;
    }

    /* column header describing the per-point fields for this type */
    bu_vls_strcat(str, "point#, (point)");
    if (pnts->type & RT_PNT_TYPE_COL) bu_vls_strcat(str, ", (color)");
    if (pnts->type & RT_PNT_TYPE_SCA) bu_vls_strcat(str, ", (scale)");
    if (pnts->type & RT_PNT_TYPE_NRM) bu_vls_strcat(str, ", (normal)");
    bu_vls_strcat(str, "\n");

    /* print c/s/n values if we have them */
    loop_counter = 1;
    point = (struct pnt *)pnts->point;
    head = &point->l;
    for (BU_LIST_FOR(point, pnt, head)) {
	struct bu_color *col = pnts_node_color(point, pnts->type);
	fastf_t *sp = pnts_node_scale(point, pnts->type);
	fastf_t *np = pnts_node_normal(point, pnts->type);

	bu_vls_printf(str, "%lu, \t (%f %f %f)", (long unsigned)loop_counter,
		      point->v[X] * mm2local, point->v[Y] * mm2local, point->v[Z] * mm2local);
	if (col)
	    bu_vls_printf(str, ", (%f %f %f)", col->buc_rgb[0], col->buc_rgb[1], col->buc_rgb[2]);
	if (sp)
	    bu_vls_printf(str, ", (%f)", *sp);
	if (np)
	    bu_vls_printf(str, ", (%f %f %f)", np[X], np[Y], np[Z]);
	bu_vls_strcat(str, "\n");
	loop_counter++;
    }

    return 0;
}


/**
 * Create a brace-deliminted vls list from point data.
 * Field order is "v [c] [s] [n]" - c,s,n optionally added depending on type
 */
static void
pnts_data_to_vls(struct bu_vls *logstr, const struct rt_pnts_internal *pnts)
{
    struct pnt *p;
    struct bu_list *head;

    if (pnts->count == 0 || !pnts->point)
	return;
    if (!pnts_type_is_valid(pnts->type))
	return;

    /* build up the format string
     * minimum is "{x y z}"
     * maximum is "{x y z r g b s i j k}"
     * with any combination of c/s/n depending on pnt type */
    p = (struct pnt *)pnts->point;
    head = &p->l;
    for (BU_LIST_FOR(p, pnt, head)) {
	struct bu_color *col = pnts_node_color(p, pnts->type);
	fastf_t *sp = pnts_node_scale(p, pnts->type);
	fastf_t *np = pnts_node_normal(p, pnts->type);

	bu_vls_printf(logstr, " {" PNT_FMT_3ARGS, V3ARGS(p->v));
	if (col) {
	    fastf_t cf[3];
	    bu_color_to_rgb_floats(col, cf);
	    bu_vls_printf(logstr, " " PNT_FMT_3ARGS, V3ARGS(cf));
	}
	if (sp)
	    bu_vls_printf(logstr, " " PNT_FMT, *sp);
	if (np)
	    bu_vls_printf(logstr, " " PNT_FMT_3ARGS, V3ARGS(np));
	bu_vls_strcat(logstr, "}");
    }
}


/**
 * Available attrs:
 * count -  number of points in set
 * type  -  point type (see geom.h:rt_pnt_type)
 * scale -  scale of point
 * data  -  data in "v [c] [s] [n]"
 * ---
 * NULL  -  All of the above
 */
int
rt_pnts_get(struct bu_vls *logstr, const struct rt_db_internal *intern, const char *attr)
{
    struct rt_pnts_internal *pnts = (struct rt_pnts_internal *)intern->idb_ptr;

    BU_CK_VLS(logstr);
    RT_PNTS_CK_MAGIC(pnts);

    if (attr == (char *)NULL) {
	/* full dump */
	bu_vls_strcpy(logstr, "pnts");
	bu_vls_printf(logstr, " count %lu", pnts->count);
	bu_vls_printf(logstr, " type %d", (int)pnts->type);
	bu_vls_printf(logstr, " scale " PNT_FMT, pnts->scale);
	bu_vls_strcat(logstr, " data {");
	pnts_data_to_vls(logstr, pnts);
	bu_vls_strcat(logstr, " }");
    } else if (BU_STR_EQUAL(attr, "count")) {
	bu_vls_printf(logstr, "%lu", pnts->count);
    } else if (BU_STR_EQUAL(attr, "type")) {
	bu_vls_printf(logstr, "%d", (int)pnts->type);
    } else if (BU_STR_EQUAL(attr, "scale")) {
	bu_vls_printf(logstr, PNT_FMT, pnts->scale);
    } else if (BU_STR_EQUAL(attr, "data")) {
	pnts_data_to_vls(logstr, pnts);
    } else {
	bu_vls_printf(logstr, "ERROR: Unknown attribute '%s', choices are count, type, scale, or data\n", attr);
	return BRLCAD_ERROR;
    }

    return BRLCAD_OK;
}


/**
 * Rebuild the point list from a Tcl list of per-point sublists, using
 * the already-established pnts->type to interpret each sublist.
 */
static int
pnts_data_from_str(struct bu_vls *logstr, struct rt_pnts_internal *pnts, const char *datastr)
{
    int npts = 0;
    const char **pts = NULL;
    int i;
    int expected;
    void *head = NULL;
    struct bu_list *headl = NULL;

    if (pnts->type == RT_PNT_UNKNOWN) {
	bu_vls_printf(logstr, "ERROR: pnts type must be set before point data\n");
	return BRLCAD_ERROR;
    }

    /* num of expected fields per point */
    expected = pnts_data_size(pnts->type);

    /* discard any pre-existing points before rebuilding; keep count
     * consistent with the (now empty) list so that any early error
     * return below leaves the internal in a coherent state. */
    pnts_data_free(pnts);
    pnts->count = 0;

    if (bu_argv_from_tcl_list(datastr, &npts, (const char ***)&pts) != 0) {
	bu_vls_printf(logstr, "ERROR: unable to parse pnts data list\n");
	return BRLCAD_ERROR;
    }

    /* allocate the head/sentinel node */
    head = pnts_node_alloc(pnts->type);
    if (!head) {
	bu_vls_printf(logstr, "ERROR: unknown pnts type %d\n", (int)pnts->type);
	if (pts)
	    bu_free((char *)pts, "pnts data argv");
	return BRLCAD_ERROR;
    }
    headl = &((struct pnt *)head)->l;
    BU_LIST_INIT(headl);
    pnts->point = head;

    for (i = 0; i < npts; i++) {
	fastf_t *vals = NULL;
	int nvals = 0;
	int idx = 3;
	point_t v = VINIT_ZERO;
	fastf_t cf[3] = {0.0, 0.0, 0.0};
	fastf_t s = 0.0;
	vect_t nrm = VINIT_ZERO;
	void *node;

	(void)_rt_tcl_list_to_fastf_array(pts[i], &vals, &nvals);
	if (nvals != expected) {
	    bu_vls_printf(logstr, "ERROR: point %d has %d values, expected %d for this pnts type\n",
			  i, nvals, expected);
	    if (vals)
		bu_free(vals, "pnts point vals");
	    bu_free((char *)pts, "pnts data argv");
	    /* free the partial list and leave an empty, coherent object */
	    pnts_data_free(pnts);
	    return BRLCAD_ERROR;
	}

	VSET(v, vals[0], vals[1], vals[2]);
	if (pnts->type & RT_PNT_TYPE_COL) {
	    VSET(cf, vals[idx], vals[idx+1], vals[idx+2]);
	    idx += 3;
	}
	if (pnts->type & RT_PNT_TYPE_SCA) {
	    s = vals[idx];
	    idx += 1;
	}
	if (pnts->type & RT_PNT_TYPE_NRM) {
	    VSET(nrm, vals[idx], vals[idx+1], vals[idx+2]);
	    idx += 3;
	}

	/* next node */
	node = pnts_node_alloc(pnts->type);
	pnts_node_set(node, pnts->type, v, cf, s, nrm);
	BU_LIST_INSERT(headl, &((struct pnt *)node)->l);

	bu_free(vals, "pnts point vals");
    }

    pnts->count = (unsigned long)npts;

    if (pts)
	bu_free((char *)pts, "pnts data argv");

    return BRLCAD_OK;
}


/**
 * Set pnts attributes from attribute/value pairs (the inverse of rt_pnts_get)
 * Recognized attributes are count, type, scale, and data 
 */
int
rt_pnts_adjust(struct bu_vls *logstr, struct rt_db_internal *intern, int argc, const char **argv)
{
    struct rt_pnts_internal *pnts;
    const char *datastr = NULL;

    RT_CK_DB_INTERNAL(intern);
    pnts = (struct rt_pnts_internal *)intern->idb_ptr;
    RT_PNTS_CK_MAGIC(pnts);

    /* First pass: scalar attributes.  'data' is deferred until after the
     * loop so that 'type' is known regardless of argument order. */
    while (argc >= 2) {
	if (BU_STR_EQUAL(argv[0], "count")) {
	    /* 'count' is advisory only - the actual count is always derived
	     * from the 'data' list (see pnts_data_from_str).  We deliberately
	     * do not store it here, so a 'count' supplied without 'data'
	     * cannot leave count > 0 with an empty point list. */
	    int cnt = 0;
	    (void)bu_opt_int(NULL, 1, &argv[1], (void *)&cnt);
	} else if (BU_STR_EQUAL(argv[0], "type")) {
	    int t = 0;
	    (void)bu_opt_int(NULL, 1, &argv[1], (void *)&t);
	    if (t < RT_PNT_TYPE_PNT || t > RT_PNT_TYPE_COL_SCA_NRM) {
		bu_vls_printf(logstr, "ERROR: invalid pnts type %d (valid range is %d-%d)\n",
			      t, RT_PNT_TYPE_PNT, RT_PNT_TYPE_COL_SCA_NRM);
		return BRLCAD_ERROR;
	    }
	    pnts->type = (rt_pnt_type)t;
	} else if (BU_STR_EQUAL(argv[0], "scale")) {
	    (void)bu_opt_fastf_t(NULL, 1, &argv[1], (void *)&pnts->scale);
	} else if (BU_STR_EQUAL(argv[0], "data")) {
	    datastr = argv[1];
	} else {
	    bu_vls_printf(logstr, "ERROR: Unknown attribute '%s', choices are count, type, scale, or data\n", argv[0]);
	    return BRLCAD_ERROR;
	}

	argc -= 2;
	argv += 2;
    }

    if (datastr) {
	/* count is authoritative from the data list itself */
	if (pnts_data_from_str(logstr, pnts, datastr) != BRLCAD_OK)
	    return BRLCAD_ERROR;
    }

    return BRLCAD_OK;
}


int
rt_pnts_form(struct bu_vls *logstr, const struct rt_functab *ftp)
{
    BU_CK_VLS(logstr);
    RT_CK_FUNCTAB(ftp);

    bu_vls_printf(logstr, "count %%lu type %%d scale %" PNT_FMT " data {{x y z [r g b] [s] [i j k]} ...}");

    return BRLCAD_OK;
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
