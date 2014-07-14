/*                          J O I N T . C
 * BRL-CAD
 *
 * Copyright (c) 1985-2014 United States Government as represented by
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
#include <stdio.h>
#include <math.h>
#include "bio.h"

#include "bu/cv.h"
#include "vmath.h"
#include "bn.h"
#include "rtgeom.h"
#include "raytrace.h"
#include "nmg.h"
#include "db.h"

#include "../../librt_private.h"


struct joint_specific {
    uint32_t joint_magic;
    point_t joint_location;
    vect_t joint_vector1;
    vect_t joint_vector2;
    fastf_t joint_value; /* currently angle or cross product value */
};
#define JOINT_NULL ((struct joint_specific *)0)
#define JOINT_FLOAT_SIZE 10

const struct bu_structparse rt_joint_parse[] = {
    { "%f", 3, "L", bu_offsetofarray(struct rt_joint_internal, location, fastf_t, X), BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
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
int
rt_joint_plot(struct bu_list *vhead, struct rt_db_internal *ip, const struct rt_tess_tol *UNUSED(ttol), const struct bn_tol *UNUSED(tol), const struct rt_view_info *UNUSED(info))
{
    struct rt_joint_internal *jip;
    point_t a,b;

    BU_CK_LIST_HEAD(vhead);
    RT_CK_DB_INTERNAL(ip);
    jip = (struct rt_joint_internal *)ip->idb_ptr;
    RT_JOINT_CK_MAGIC(jip);


    VADD2(a, jip->location, jip->vector1);
    VADD2(b, jip->location, jip->vector2);

    RT_ADD_VLIST(vhead, jip->location, BN_VLIST_LINE_MOVE); /* the base */
    RT_ADD_VLIST(vhead, a, BN_VLIST_LINE_DRAW);
    RT_ADD_VLIST(vhead, jip->location, BN_VLIST_LINE_MOVE); /* the base */
    RT_ADD_VLIST(vhead, b, BN_VLIST_LINE_DRAW);

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

    bu_bomb("rt_joint_import4: nobody should be asking for import of a joint from this format.\n");

    return 0;			/* OK */
}


int
rt_joint_export4(struct bu_external *ep, const struct rt_db_internal *ip, double UNUSED(local2mm), const struct db_i *dbip)
{
    struct rt_joint_internal *jip;
    union record *rec;

    if (dbip) RT_CK_DBI(dbip);

    RT_CK_DB_INTERNAL(ip);
    if (ip->idb_type != ID_JOINT) return -1;
    jip = (struct rt_joint_internal *)ip->idb_ptr;
    RT_JOINT_CK_MAGIC(jip);

    BU_CK_EXTERNAL(ep);
    ep->ext_nbytes = sizeof(union record);
    ep->ext_buf = (uint8_t *)bu_calloc(1, ep->ext_nbytes, "joint external");
    rec = (union record *)ep->ext_buf;

    rec->s.s_id = ID_SOLID;
    rec->s.s_type = JOINT;
#if 0
    VMOVE(&rec->s.s_joint_N, jip->normal);
    VMOVE(&rec->s.s_joint_C, jip->center);
    rec->s.s_joint_m = jip->mag;
#endif
    return 0;
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

    BU_ASSERT_LONG(ep->ext_nbytes, ==, SIZEOF_NETWORK_DOUBLE * JOINT_FLOAT_SIZE);

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

    /* must be double for import and export */
    double vec[JOINT_FLOAT_SIZE];

    if (dbip) RT_CK_DBI(dbip);

    RT_CK_DB_INTERNAL(ip);
    if (ip->idb_type != ID_JOINT) return -1;
    jip = (struct rt_joint_internal *)ip->idb_ptr;
    RT_JOINT_CK_MAGIC(jip);

    BU_CK_EXTERNAL(ep);
    ep->ext_nbytes = SIZEOF_NETWORK_DOUBLE * JOINT_FLOAT_SIZE;
    ep->ext_buf = (uint8_t *)bu_malloc(ep->ext_nbytes, "joint external");

    VSCALE(&vec[0], jip->location, local2mm);
    VMOVE(&vec[3], jip->vector1);
    VMOVE(&vec[6], jip->vector2);
    vec[9] = jip->value;

    /* Convert from internal (host) to database (network) format */
    bu_cv_htond(ep->ext_buf, (unsigned char *)vec, JOINT_FLOAT_SIZE);


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

    sprintf(buf, "\tL (%g, %g, %g)\n",
	    V3INTCLAMPARGS(jip->location));		/* should have unit length */

    bu_vls_strcat(str, buf);

    if (!verbose)
	return 0;

    sprintf(buf, "\tV1 (%g %g %g) V2 (%g %g %g) value=%g\n",
	    INTCLAMP(jip->vector1[0]*mm2local),
	    INTCLAMP(jip->vector1[1]*mm2local),
	    INTCLAMP(jip->vector1[2]*mm2local),
	    INTCLAMP(jip->vector2[0]*mm2local),
	    INTCLAMP(jip->vector2[1]*mm2local),
	    INTCLAMP(jip->vector2[2]*mm2local),
	    INTCLAMP(jip->value*mm2local));
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
rt_joint_tess(struct nmgregion **r, struct model *m, struct rt_db_internal *ip, const struct rt_tess_tol *UNUSED(ttol), const struct bn_tol *UNUSED(tol))
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


int
rt_joint_params(struct pc_pc_set *UNUSED(ps), const struct rt_db_internal *ip)
{
    if (ip) RT_CK_DB_INTERNAL(ip);

    return 0;			/* OK */
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

