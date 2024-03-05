/*                          J O I N T . C
 * BRL-CAD
 *
 * Copyright (c) 1985-2024 United States Government as represented by
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
/** @addtogroup primitives */
/** @{ */
/** @file primitives/grip/grip.c
 *
 * Intersect a ray with a "joint" and return nothing.
 *
 * A JOINT is defined by a point location, two vectors and a
 * joint value.  The location is the point at which the vectors
 * are constrained. The value is currently the angle between
 * the vectors.
 *
 * All Ray intersections return "missed"
 *
 * The bounding box for a grip is empty.
 *
 */

#include "common.h"

#include <stddef.h>
#include <math.h>
#include "bio.h"

#include "bu/cv.h"
#include "vmath.h"
#include "bn.h"
#include "rt/geom.h"
#include "raytrace.h"
#include "nmg.h"
#include "rt/db4.h"
#include "brep.h"

#include "../../librt_private.h"


struct joint_specific {
    uint32_t joint_magic;
    point_t joint_location;
    char *reference_path_1;
    char *reference_path_2;
    vect_t joint_vector1;
    vect_t joint_vector2;
    fastf_t joint_value; /* currently angle or cross product value */
};
#define JOINT_NULL ((struct joint_specific *)0)
#define JOINT_FLOAT_SIZE 10

const struct bu_structparse rt_joint_parse[] = {
    { "%f", 3, "V", bu_offsetofarray(struct rt_joint_internal, location, fastf_t, X), BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    { "%V", 1, "RP1", bu_offsetof(struct rt_joint_internal, reference_path_1), BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    { "%V", 1, "RP2", bu_offsetof(struct rt_joint_internal, reference_path_2), BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    { "%f", 3, "V1", bu_offsetofarray(struct rt_joint_internal, vector1, fastf_t, X), BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    { "%f", 3, "V2", bu_offsetofarray(struct rt_joint_internal, vector2, fastf_t, X), BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    { "%f", 1, "A", bu_offsetof(struct rt_joint_internal, value), BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    { {'\0', '\0', '\0', '\0'}, 0, (char *)NULL, 0, BU_STRUCTPARSE_FUNC_NULL, NULL, NULL }
};


int
rt_joint_prep(struct soltab *stp, struct rt_db_internal *ip, struct rt_i *rtip)
{
    struct rt_joint_internal *jip;
    struct joint_specific *jointp;

    if (rtip) RT_CK_RTI(rtip);

    jip = (struct rt_joint_internal *)ip->idb_ptr;
    RT_JOINT_CK_MAGIC(jip);

    BU_GET(jointp, struct joint_specific);
    stp->st_specific = (void *)jointp;

    VMOVE(jointp->joint_location, jip->location);
    VMOVE(jointp->joint_vector1, jip->vector1);
    VMOVE(jointp->joint_vector2, jip->vector2);
    jointp->reference_path_1 = bu_vls_addr(&jip->reference_path_1);
    jointp->reference_path_2 = bu_vls_addr(&jip->reference_path_2);
    jointp->joint_value = jip->value;


    /* No bounding sphere or bounding RPP is possible */
    VSETALL(stp->st_min, 0.0);
    VSETALL(stp->st_max, 0.0);

    stp->st_aradius = 0.0;
    stp->st_bradius = 0.0;
    return 0;		/* OK */
}


void
rt_joint_print(const struct soltab *stp)
{
    const struct joint_specific *jointp = (struct joint_specific *)stp->st_specific;

    if (jointp == JOINT_NULL) {
	bu_log("joint(%s):  no data?\n", stp->st_name);
	return;
    }
    VPRINT("Location", jointp->joint_location);
    VPRINT("Vector1", jointp->joint_vector1);
    VPRINT("Vector2", jointp->joint_vector2);
    bu_log("reference_path_1 = %s\n", jointp->reference_path_1);
    bu_log("reference_path_2 = %s\n", jointp->reference_path_1);
    bu_log("Value = %f\n", jointp->joint_value);
}


/**
 * Function -
 * Shoot a ray at a joint
 *
 * Algorithm -
 * The intersection distance is computed.
 *
 * Returns -
 * 0 MISS
 * >0 HIT
 */
int
rt_joint_shot(struct soltab *stp, struct xray *rp, struct application *ap, struct seg *UNUSED(seghead))
{
    if (stp) RT_CK_SOLTAB(stp);
    if (rp) RT_CK_RAY(rp);
    if (ap) RT_CK_APPLICATION(ap);

    return 0;	/* this has got to be the easiest.. no hit, no surfno,
		 * no nada.
		 */
}


/**
 * Given ONE ray distance, return the normal and entry/exit point.
 * The normal is already filled in.
 */
void
rt_joint_norm(struct hit *hitp, struct soltab *stp, struct xray *rp)
{
    if (hitp) RT_CK_HIT(hitp);
    if (stp) RT_CK_SOLTAB(stp);
    if (rp) RT_CK_RAY(rp);

    bu_bomb("rt_grp_norm: joints should never be hit.\n");
}


/**
 * Return the "curvature" of the joint.
 */
void
rt_joint_curve(struct curvature *cvp, struct hit *hitp, struct soltab *stp)
{
    if (!cvp) return;
    if (hitp) RT_CK_HIT(hitp);
    if (stp) RT_CK_SOLTAB(stp);

    bu_bomb("rt_grp_curve: nobody should be asking for curve of a joint.\n");
}


/**
 * For a hit on a face of an HALF, return the (u, v) coordinates of
 * the hit point.  0 <= u, v <= 1.  u extends along the Xbase
 * direction.  v extends along the "Ybase" direction.  Note that a
 * "toroidal" map is established, varying each from 0 up to 1 and then
 * back down to 0 again.
 */
void
rt_joint_uv(struct application *ap, struct soltab *stp, struct hit *hitp, struct uvcoord *uvp)
{
    if (ap) RT_CK_APPLICATION(ap);
    if (stp) RT_CK_SOLTAB(stp);
    if (hitp) RT_CK_HIT(hitp);
    if (!uvp) return;

    bu_bomb("rt_grp_uv: nobody should be asking for UV of a joint.\n");
}


void
rt_joint_free(struct soltab *stp)
{
    struct joint_specific *jointp = (struct joint_specific *)stp->st_specific;

    BU_PUT(jointp, struct joint_specific);
}


/**
 * We represent a joint as a pyramid.  The center describes where the
 * center of the base is.  The normal describes which direction the
 * tip of the pyramid is.  Mag describes the distance from the center
 * to the tip. 1/4 of the width is the length of a base side.
 *
 */

/**
 * Plot a joint by tracing out joint vectors This draws each
 * edge only once.
 *
 * XXX No checking for degenerate faces is done, but probably should
 * be.
 */
#define LOCATION_RADIUS 5
int
rt_joint_plot(struct bu_list *vhead, struct rt_db_internal *ip, const struct bg_tess_tol *UNUSED(ttol), const struct bn_tol *UNUSED(tol), const struct bview *UNUSED(info))
{
    struct rt_joint_internal *jip;
    point_t a = {LOCATION_RADIUS, 0, 0};
    point_t b = {0, LOCATION_RADIUS, 0};
    point_t c = {0, 0, LOCATION_RADIUS};
    fastf_t top[16*3];
    fastf_t middle[16*3];
    fastf_t bottom[16*3];
    int i;

    BU_CK_LIST_HEAD(vhead);
    RT_CK_DB_INTERNAL(ip);
    struct bu_list *vlfree = &RTG.rtg_vlfree;
    jip = (struct rt_joint_internal *)ip->idb_ptr;
    RT_JOINT_CK_MAGIC(jip);

    rt_ell_16pnts(top, jip->location, a, b);
    rt_ell_16pnts(bottom, jip->location, b, c);
    rt_ell_16pnts(middle, jip->location, a, c);

    BV_ADD_VLIST(vlfree, vhead, &top[15*ELEMENTS_PER_VECT], BV_VLIST_LINE_MOVE);
    for (i = 0; i < 16; i++) {
	BV_ADD_VLIST(vlfree, vhead, &top[i*ELEMENTS_PER_VECT], BV_VLIST_LINE_DRAW);
    }

    BV_ADD_VLIST(vlfree, vhead, &bottom[15*ELEMENTS_PER_VECT], BV_VLIST_LINE_MOVE);
    for (i = 0; i < 16; i++) {
	BV_ADD_VLIST(vlfree, vhead, &bottom[i*ELEMENTS_PER_VECT], BV_VLIST_LINE_DRAW);
    }

    BV_ADD_VLIST(vlfree, vhead, &middle[15*ELEMENTS_PER_VECT], BV_VLIST_LINE_MOVE);
    for (i = 0; i < 16; i++) {
	BV_ADD_VLIST(vlfree, vhead, &middle[i*ELEMENTS_PER_VECT], BV_VLIST_LINE_DRAW);
    }


    return 0;
}


/**
 * Returns -
 * -1 failure
 * 0 success
 */
int
rt_joint_import4(struct rt_db_internal *ip, const struct bu_external *ep, const fastf_t *UNUSED(mat), const struct db_i *dbip)
{
    if (ip) RT_CK_DB_INTERNAL(ip);
    if (ep) BU_CK_EXTERNAL(ep);
    if (dbip) RT_CK_DBI(dbip);

    return -1;
}


/**
 * Returns -
 * -1 failure
 * 0 success
 */
int
rt_joint_export4(struct bu_external *ep, const struct rt_db_internal *ip, double UNUSED(local2mm), const struct db_i *dbip)
{
    if (ep) BU_CK_EXTERNAL(ep);
    if (ip) RT_CK_DB_INTERNAL(ip);
    if (dbip) RT_CK_DBI(dbip);

    return -1;
}


int
rt_joint_import5(struct rt_db_internal *ip, const struct bu_external *ep, const fastf_t *mat, const struct db_i *dbip)
{
    struct rt_joint_internal *jip;
    double f, t;

    /* must be double for import and export */
    double vec[JOINT_FLOAT_SIZE];

    if (dbip) RT_CK_DBI(dbip);
    RT_CK_DB_INTERNAL(ip);
    BU_CK_EXTERNAL(ep);

    BU_ASSERT(ep->ext_nbytes >= SIZEOF_NETWORK_DOUBLE * JOINT_FLOAT_SIZE + 1);

    ip->idb_major_type = DB5_MAJORTYPE_BRLCAD;
    ip->idb_type = ID_JOINT;
    ip->idb_meth = &OBJ[ID_JOINT];
    BU_ALLOC(ip->idb_ptr, struct rt_joint_internal);

    jip = (struct rt_joint_internal *)ip->idb_ptr;
    jip->magic = RT_JOINT_INTERNAL_MAGIC;

    /* Convert from database (network) to internal (host) format */
    bu_cv_ntohd((unsigned char *)vec, ep->ext_buf, JOINT_FLOAT_SIZE);

    /* Transform the point, and the normal */
    if (mat == NULL) mat = bn_mat_identity;


    MAT4X3PNT(jip->location, mat, &vec[0]);
    MAT4X3VEC(jip->vector1, mat, &vec[3]);
    MAT4X3VEC(jip->vector2, mat, &vec[6]);
    if (NEAR_ZERO(mat[15], 0.001)) {
	bu_bomb("rt_joint_import5, scale factor near zero.");
    }
    jip->value = vec[9];

    /* convert name of reference_path_1 & 2*/
    bu_vls_init(&jip->reference_path_1);
    bu_vls_strcpy(&jip->reference_path_1, (char *)ep->ext_buf + JOINT_FLOAT_SIZE * SIZEOF_NETWORK_DOUBLE);
    bu_vls_init(&jip->reference_path_2);
    bu_vls_strcpy(&jip->reference_path_2, (char *)ep->ext_buf + JOINT_FLOAT_SIZE * SIZEOF_NETWORK_DOUBLE + bu_vls_strlen(&jip->reference_path_1) + 1);

    /* Verify that vector1 has unit length */
    f = MAGNITUDE(jip->vector1);
    if (f <= SMALL) {
	bu_log("rt_joint_import5:  bad vector1, len=%g\n", f);
	return -1;		/* BAD */
    }
    t = f - 1.0;
    if (!NEAR_ZERO(t, 0.001)) {
	/* Restore normal to unit length */
	f = 1/f;
	VSCALE(jip->vector1, jip->vector1, f);
    }
    /* Verify that vector2 has unit length */
    f = MAGNITUDE(jip->vector2);
    if (f <= SMALL) {
	bu_log("rt_joint_import5:  bad vector2, len=%g\n", f);
	return -1;		/* BAD */
    }
    t = f - 1.0;
    if (!NEAR_ZERO(t, 0.001)) {
	/* Restore normal to unit length */
	f = 1/f;
	VSCALE(jip->vector2, jip->vector2, f);
    }


    return 0;		/* OK */
}


int
rt_joint_export5(struct bu_external *ep, const struct rt_db_internal *ip, double local2mm, const struct db_i *dbip)
{
    struct rt_joint_internal *jip;
    unsigned char *ptr;

    /* must be double for import and export */
    double vec[JOINT_FLOAT_SIZE];

    if (dbip) RT_CK_DBI(dbip);

    RT_CK_DB_INTERNAL(ip);
    if (ip->idb_type != ID_JOINT) return -1;
    jip = (struct rt_joint_internal *)ip->idb_ptr;
    RT_JOINT_CK_MAGIC(jip);

    BU_CK_EXTERNAL(ep);
    ep->ext_nbytes = SIZEOF_NETWORK_DOUBLE * JOINT_FLOAT_SIZE +
		bu_vls_strlen(&jip->reference_path_1) + 1 +
		+ bu_vls_strlen(&jip->reference_path_2) + 1;
    ep->ext_buf = (uint8_t *)bu_malloc(ep->ext_nbytes, "joint external");
    ptr = (unsigned char *)ep->ext_buf;

    VSCALE(&vec[0], jip->location, local2mm);
    VMOVE(&vec[3], jip->vector1);
    VMOVE(&vec[6], jip->vector2);
    vec[9] = jip->value;

    /* Convert from internal (host) to database (network) format */
    bu_cv_htond(ep->ext_buf, (unsigned char *)vec, JOINT_FLOAT_SIZE);

    ptr += JOINT_FLOAT_SIZE * SIZEOF_NETWORK_DOUBLE;
    bu_strlcpy((char *)ptr, bu_vls_addr(&jip->reference_path_1), bu_vls_strlen(&jip->reference_path_1) + 1);

    ptr += bu_vls_strlen(&jip->reference_path_1) + 1;
    bu_strlcpy((char *)ptr, bu_vls_addr(&jip->reference_path_2), bu_vls_strlen(&jip->reference_path_2) + 1);

    return 0;
}


/**
 * Make human-readable formatted presentation of this solid.  First
 * line describes type of solid.  Additional lines are indented one
 * tab, and give parameter values.
 */
int
rt_joint_describe(struct bu_vls *str, const struct rt_db_internal *ip, int verbose, double mm2local)
{
    struct rt_joint_internal *jip = (struct rt_joint_internal *)ip->idb_ptr;
    char buf[256];

    RT_JOINT_CK_MAGIC(jip);
    bu_vls_strcat(str, "joint\n");

    sprintf(buf, "\tV (%g, %g, %g)\n",
	    V3INTCLAMPARGS(jip->location));		/* should have unit length */

    bu_vls_strcat(str, buf);

    if (!verbose)
	return 0;

    sprintf(buf, "\tV1 (%g %g %g) V2 (%g %g %g) A=%g RP1=%s RP2=%s\n",
	    INTCLAMP(jip->vector1[0]*mm2local),
	    INTCLAMP(jip->vector1[1]*mm2local),
	    INTCLAMP(jip->vector1[2]*mm2local),
	    INTCLAMP(jip->vector2[0]*mm2local),
	    INTCLAMP(jip->vector2[1]*mm2local),
	    INTCLAMP(jip->vector2[2]*mm2local),
	    INTCLAMP(jip->value*mm2local),
	    bu_vls_addr(&jip->reference_path_1),
	    bu_vls_addr(&jip->reference_path_2));
    bu_vls_strcat(str, buf);

    return 0;
}


/**
 * Free the storage associated with the rt_db_internal version of this
 * solid.
 */
void
rt_joint_ifree(struct rt_db_internal *ip)
{
    RT_CK_DB_INTERNAL(ip);

    bu_free(ip->idb_ptr, "joint ifree");
    ip->idb_ptr = ((void *)0);
}


int
rt_joint_tess(struct nmgregion **r, struct model *m, struct rt_db_internal *ip, const struct bg_tess_tol *UNUSED(ttol), const struct bn_tol *UNUSED(tol))
{
    struct rt_joint_internal *jip;

    if (r) *r = NULL;
    if (m) NMG_CK_MODEL(m);

    RT_CK_DB_INTERNAL(ip);
    jip = (struct rt_joint_internal *)ip->idb_ptr;
    RT_JOINT_CK_MAGIC(jip);

    /* XXX tess routine needed */
    return -1;
}

void
rt_joint_make(const struct rt_functab *ftp, struct rt_db_internal *intern)
{
    struct rt_joint_internal* ip;

    intern->idb_type = ID_JOINT;
    intern->idb_major_type = DB5_MAJORTYPE_BRLCAD;

    BU_ASSERT(&OBJ[intern->idb_type] == ftp);
    intern->idb_meth = ftp;

    BU_ALLOC(ip, struct rt_joint_internal);
    intern->idb_ptr = (void *)ip;

    ip->magic = RT_JOINT_INTERNAL_MAGIC;
    struct bu_vls empty = BU_VLS_INIT_ZERO;
    ip->reference_path_1 = empty;
    ip->reference_path_2 = empty;
    VSET(ip->vector1, 0.0, 1.0, 0.0);
    VSET(ip->vector2, 0.0, 1.0, 0.0);
}


int
rt_joint_params(struct pc_pc_set *UNUSED(ps), const struct rt_db_internal *ip)
{
    if (ip) RT_CK_DB_INTERNAL(ip);

    return 0;			/* OK */
}

/* TODO: should be dynamic */
#define JOINT_SELECT_VMAX_DISTSQ 100.0

enum {
    JOINT_SELECT_LOC,
    JOINT_SELECT_V1,
    JOINT_SELECT_V2,
    JOINT_SELECT_PATH1,
    JOINT_SELECT_PATH2
};

struct joint_selection {
    int what;
    vect_t start;
};

static void
joint_free_selection(struct rt_selection *s)
{
    struct joint_selection *js = (struct joint_selection *)s->obj;
    BU_PUT(js, struct joint_selection);
    BU_FREE(s, struct rt_selection);
}

struct rt_selection_set *
rt_joint_find_selections(
    const struct rt_db_internal *ip,
    const struct rt_selection_query *query)
{
    struct rt_joint_internal *jip;
    fastf_t dist[3];
    point_t qline_pt;
    vect_t qstart;
    point_t qdir;
    struct joint_selection *joint_selection = NULL;
    struct rt_selection *selection = NULL;
    struct rt_selection_set *selection_set = NULL;
    struct bn_tol tol;

#if 0
    fastf_t dist_sq1, dist_sq2;
    point_t joint_v1pt;
#endif

    RT_CK_DB_INTERNAL(ip);
    jip = (struct rt_joint_internal *)ip->idb_ptr;
    RT_JOINT_CK_MAGIC(jip);

    /* select location, or vector1, or vector2 */
    /* vectors are plotted location to location+vector */
    /* distinguish between close to location and close to vector */
    /* shortest distance between query vector and vector1, 2 */
    /* if vector 1, 2 point is within radius of location, select
     * location, otherwise select vector with shorter distance */
    VMOVE(qstart, query->start);
    VMOVE(qdir, query->dir);

    BU_GET(joint_selection, struct joint_selection);
    joint_selection->what = JOINT_SELECT_V1;

    /* closest point on query line to location (same as the
     * intersection between the query line and the plane parallel to
     * the view plane that intersects the location)
     */
    BN_TOL_INIT(&tol);
    (void)bg_dist_pnt3_line3(dist, qline_pt, qstart, qdir, jip->location, &tol);
    VJOIN1(joint_selection->start, qline_pt, -1.0, jip->location);

#if 0
    ret = bg_distsq_line3_line3(dist, qstart, qdir, jip->location,
	    jip->vector1, qline_pt, joint_v1pt);
    dist_sq1 = dist[2];

    if (ret == 1) {
	/* query ray is parallel to vector 1 */
	BU_GET(joint_selection, struct joint_selection);
	joint_selection->what = JOINT_SELECT_V1;
    } else {
	point_t joint_v2pt;
	fastf_t distsq_to_loc;
	fastf_t max_vdist;

	ret = bg_distsq_line3_line3(dist, qstart, qdir, jip->location,
		jip->vector2, qline_pt, joint_v2pt);
	dist_sq2 = dist[2];

	if (dist_sq1 > JOINT_SELECT_VMAX_DISTSQ &&
	    dist_sq2 > JOINT_SELECT_VMAX_DISTSQ)
	{
	    /* query isn't close enough */
	    bu_log("selected nothing.\n");
	    return NULL;
	}

	BU_GET(joint_selection, struct joint_selection);
	if (dist_sq1 <= dist_sq2) {
	    /* closer to v1 than v2 */
	    distsq_to_loc = DIST_PNT_PNT_SQ(joint_v1pt, jip->location);
	    max_vdist = .1 * MAGSQ(jip->vector1);
	    joint_selection->what = JOINT_SELECT_V1;
	} else {
	    /* closer to v2 than v1 */
	    distsq_to_loc = DIST_PNT_PNT_SQ(joint_v2pt, jip->location);
	    max_vdist = .1 * MAGSQ(jip->vector2);
	    joint_selection->what = JOINT_SELECT_V2;
	}

	if (distsq_to_loc < max_vdist) {
	    /* closer to location than v1 */
	    joint_selection->what = JOINT_SELECT_LOC;
	}
    }

    if (!joint_selection) {
	bu_log("selected nothing.\n");
	return NULL;
    }

    switch (joint_selection->what) {
	case JOINT_SELECT_LOC:
	    bu_log("selected location.\n");
	    break;
	case JOINT_SELECT_V1:
	    bu_log("selected vector1.\n");
	    break;
	case JOINT_SELECT_V2:
	    bu_log("selected vector2.\n");
    }
#endif

    /* build and return list of selections */
    BU_ALLOC(selection_set, struct rt_selection_set);
    BU_PTBL_INIT(&selection_set->selections);

    BU_GET(selection, struct rt_selection);
    selection->obj = (void *)joint_selection;
    bu_ptbl_ins(&selection_set->selections, (long *)selection);
    selection_set->free_selection = joint_free_selection;

    return selection_set;
}

static void
db_path_to_inverse_mat(struct db_i *dbip, const struct db_full_path *fpath, mat_t inverse_mat)
{
    size_t i;

    MAT_IDN(inverse_mat);

    for (i = 1; i <= fpath->fp_len; ++i) {
	struct db_full_path sub_path = *fpath;
	mat_t sub_mat, new_mat, new_mat_inv, comp;

	sub_path.fp_len = i;
	db_path_to_mat(dbip, &sub_path, sub_mat, 0, NULL);

	/* isolate to just the mat of the leaf directory */
	bn_mat_mul(new_mat, inverse_mat, sub_mat);

	/* apply inverse to previous inverse mat */
	bn_mat_inverse(new_mat_inv, new_mat);
	bn_mat_mul(comp, new_mat_inv, inverse_mat);
	MAT_COPY(inverse_mat, comp);
    }
}

int
rt_joint_process_selection(
    struct rt_db_internal *ip,
    struct db_i *dbip,
    const struct rt_selection *selection,
    const struct rt_selection_operation *op)
{
    struct joint_selection *js;
    struct rt_joint_internal *jip;
    mat_t rmat;
    mat_t path_inv_mat;
    vect_t center, delta, end, cross, tmp;
    fastf_t angle;
    struct rt_db_internal path_ip;
    struct directory *dp;
    struct db_full_path fpath;
    struct db_full_path mat_fpath;
    /*int ret;*/

    if (op->type == RT_SELECTION_NOP) {
	return 0;
    }

    if (op->type != RT_SELECTION_TRANSLATION) {
	return -1;
    }

    RT_CK_DB_INTERNAL(ip);
    jip = (struct rt_joint_internal *)ip->idb_ptr;
    RT_JOINT_CK_MAGIC(jip);

    js = (struct joint_selection *)selection->obj;
    if (!js) {
	return -1;
    }

    delta[X] = op->parameters.tran.dx;
    delta[Y] = op->parameters.tran.dy;
    delta[Z] = op->parameters.tran.dz;

    if (VNEAR_ZERO(delta, BN_TOL_DIST)) {
	return 0;
    }

    /* get solid or parent comb directory */
    /*ret =*/ (void)db_string_to_path(&fpath, dbip, bu_vls_cstr(&jip->reference_path_1));
    if (fpath.fp_len < 1) {
	dp = NULL;
    } else if (fpath.fp_len == 1) {
	dp = db_lookup(dbip, bu_vls_cstr(&jip->reference_path_1), LOOKUP_QUIET);
    } else {
	dp = DB_FULL_PATH_GET(&fpath, fpath.fp_len - 2);
    }
    if (dp == RT_DIR_NULL) {
	bu_log("lookup failed\n");
	db_free_full_path(&fpath);
	return 0;
    }

    /* get mat to reverse the path transformation */
    mat_fpath = fpath;
    /*DB_FULL_PATH_POP(&mat_fpath);*/
    db_path_to_inverse_mat(dbip, &mat_fpath, path_inv_mat);

    /* transform joint position to be relative to target mat */
    /*MAT4X3MAT(center, jip->location, path_inv_mat);*/
    VMOVE(center, jip->location);

    /* we'll rotate about the vector perpendicular to the delta vector, again,
     * made relative to target mat
     */
    VADD2(end, js->start, delta);
    VCROSS(tmp, js->start, end);

    MAT4X3VEC(cross, path_inv_mat, tmp);
    VUNITIZE(cross);

    /* find the angle of rotation */
    angle = VDOT(js->start, end);
    angle /= MAGNITUDE(js->start) * MAGNITUDE(end);
    angle = acos(angle);

    /* we're calculating the primitive/member matrix, so we need the
     * rotation from the perspective of the primitive
     */
    bn_mat_arb_rot(rmat, center, cross, angle);

    /* modify solid matrix or parent comb's member matrix */
    if (fpath.fp_len > 1) {
	char *member_name = DB_FULL_PATH_CUR_DIR(&fpath)->d_namep;
	struct rt_comb_internal *comb_ip;
	union tree *comb_tree, *member;
	mat_t combined_mat;

	rt_db_get_internal(&path_ip, dp, dbip, NULL, NULL);
	comb_ip = (struct rt_comb_internal *)path_ip.idb_ptr;
	comb_tree = comb_ip->tree;

	member = NULL;
	member = db_find_named_leaf(comb_tree, member_name);

	if (!member) {
	    bu_log("couldn't lookup member to edit matrix\n");
	    rt_db_free_internal(&path_ip);
	    return 0;
	}
	if (!member->tr_l.tl_mat) {
	    mat_t *new_mat;
	    BU_ALLOC(new_mat, mat_t);
	    MAT_IDN(*new_mat);
	    member->tr_l.tl_mat = (matp_t)new_mat;
	}
	bn_mat_mul(combined_mat, member->tr_l.tl_mat, rmat);
	MAT_COPY(member->tr_l.tl_mat, combined_mat);
    } else {
	rt_db_get_internal(&path_ip, dp, dbip, rmat, NULL);
    }

    /* write changes */
    rt_db_put_internal(dp, dbip, &path_ip, NULL);

    VMOVE(js->start, end);
    db_free_full_path(&fpath);

    return 0;
}

/** @} */
/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */

