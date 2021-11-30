/*                           X X X . C
 * BRL-CAD
 *
 * Copyright (c) 1990-2021 United States Government as represented by
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
/** @file primitives/xxx/xxx.c
 *
 * Intersect a ray with an 'xxx' primitive object.
 *
 */
/** @} */

/*****************************************
 * HOW TO ADD A NEW GEOMETRIC OBJECT TYPE:
 *
 * design a (nameless) in-memory data structure
 *
 * edit rt/defines.h add ID_XXX, update ID_MAXIMUM and ID_MAX_SOLID
 * edit rt/db5.h add DB5_MINORTYPE_BRLCAD_XXX define
 * edit rt/geom.h to add rt_xxx_internal
 *   struct with parameters for this new object
 * edit bu/magic.h to add RT_XXX_INTERNAL_MAGIC
 * edit src/librt/primitives/xxx/xxx.c
 *   minimally, implement callback code for:
 *     export5/import5 == .g writing/reading
 *     make == 'make' default constructor command
 *     describe == 'l' command
 *     ifree == db memory management
 *     free == raytrace memory management
 *     plot == wireframe visualization
 *     xform == matrix editing
 *     prep/shot/norm == ray tracing
 *   ideally, also implement callback code for:
 *     get/adjust == g2asc, get/put/adjust commands
 *     form == 'form' command
 *     bbox == bounding box, 'bb' command
 *     volume/surf_area/centroid == 'analyze' command
 *     brep == conversion to NURBS
 *     adaptive_plot == LoD wireframe
 *     uv/curve == texture mapping, visualization
 * edit src/librt/primitives/xxx/xxx.h,
 *   define xxx_specific: raytracing form with precomputed terms
 *   define rt_xxx_parse: bu_structparse for db get, adjust, ...
 * edit table.c:
 *   RT_DECLARE_INTERFACE()
 *   struct rt_functab entry
 * edit src/librt/CMakeLists.txt to add xxx.c to compile
 * edit src/libwdb/xxx.c and create an mk_xxx() routine
 * edit src/libwdb/CMakeLists.txt to add xxx.c to compile
 * edit src/libged/make.c and add 'make' support for xxx
 * edit src/libged/typein.c and add 'in' support for xxx
 * edit src/tclscripts/mged to add to mged GUI
 * edit src/tclscripts/archer to add to archer GUI
 *
 */

#include "common.h"

#include <stdio.h>
#include <math.h>

#include "bu/cv.h"
#include "vmath.h"
#include "rt/db4.h"
#include "nmg.h"
#include "rt/geom.h"
#include "raytrace.h"

/* local interface header */
#include "./xxx.h"


/**
 * Given a pointer to a GED database record, and a transformation
 * matrix, determine if this is a valid XXX, and if so, precompute
 * various terms of the formula.
 *
 * Returns -
 * 0 XXX is OK
 * !0 Error in description
 *
 * Implicit return -
 * A struct xxx_specific is created, and its address is stored in
 * stp->st_specific for use by xxx_shot().
 */
int
rt_xxx_prep(struct soltab *stp, struct rt_db_internal *ip, struct rt_i *rtip)
{
    struct rt_xxx_internal *xxx_ip;
    struct xxx_specific *xxx;

    if (!stp || !ip)
	return -1;

    RT_CK_SOLTAB(stp);
    RT_CK_DB_INTERNAL(ip);
    if (rtip) RT_CK_RTI(rtip);

    xxx_ip = (struct rt_xxx_internal *)ip->idb_ptr;
    RT_XXX_CK_MAGIC(xxx_ip);

    BU_GET(xxx, struct xxx_specific);
    stp->st_specific = (void *)xxx;

    /* fill in xxx_specific here */
    VSETALL(xxx->xxx_V, 0.0);

    return 0;
}


void
rt_xxx_print(const struct soltab *stp)
{
    const struct xxx_specific *xxx;

    if (!stp) return;
    xxx = (struct xxx_specific *)stp->st_specific;
    if (!xxx) return;
    RT_CK_SOLTAB(stp);
}


/**
 * Intersect a ray with a xxx.  If an intersection occurs, a struct
 * seg will be acquired and filled in.
 *
 * Returns -
 * 0 MISS
 * >0 HIT
 */
int
rt_xxx_shot(struct soltab *stp, struct xray *rp, struct application *ap, struct seg *seghead)
{
    struct xxx_specific *xxx;

    if (!stp) return -1;
    RT_CK_SOLTAB(stp);
    xxx = (struct xxx_specific *)stp->st_specific;
    if (!xxx) return -1;
    if (rp) RT_CK_RAY(rp);
    if (ap) RT_CK_APPLICATION(ap);
    if (!seghead) return -1;

/* the EXAMPLE_NEW_SEGMENT block shows how one might add a new result
 * if the ray did hit the primitive.  the segment values would need to
 * be adjusted accordingly to match real values instead of -1.
 */
#ifdef EXAMPLE_NEW_SEGMENT
    /* allocate a segment */
    RT_GET_SEG(segp, ap->a_resource);
    segp->seg_stp = stp; /* stash a pointer to the primitive */

    segp->seg_in.hit_dist = -1; /* XXX set to real distance to entry point */
    segp->seg_out.hit_dist = -1; /* XXX set to real distance to exit point */
    segp->seg_in.hit_surfno = -1; /* XXX set to a non-negative ID for entry surface */
    segp->seg_out.hit_surfno = -1;  /* XXX set to a non-negative ID for exit surface */

    /* add segment to list of those encountered for this primitive */
    BU_LIST_INSERT(&(seghead->l), &(segp->l));

    return 2; /* num surface intersections == in + out == 2 */
#endif

    return 0;			/* MISS */
}


/**
 * Given ONE ray distance, return the normal and entry/exit point.
 */
void
rt_xxx_norm(struct hit *hitp, struct soltab *stp, struct xray *rp)
{
    struct xxx_specific *xxx;

    if (!stp) return;
    xxx = (struct xxx_specific *)stp->st_specific;
    if (!xxx) return;
    RT_CK_SOLTAB(stp);

    VJOIN1(hitp->hit_point, rp->r_pt, hitp->hit_dist, rp->r_dir);
}


/**
 * Return the curvature of the xxx.
 */
void
rt_xxx_curve(struct curvature *cvp, struct hit *hitp, struct soltab *stp)
{
    struct xxx_specific *xxx;

    if (!stp) return;
    xxx = (struct xxx_specific *)stp->st_specific;
    if (!xxx) return;
    RT_CK_SOLTAB(stp);

    cvp->crv_c1 = cvp->crv_c2 = 0;

    /* any tangent direction */
    bn_vec_ortho(cvp->crv_pdir, hitp->hit_normal);
}


/**
 * For a hit on the surface of an xxx, return the (u, v) coordinates
 * of the hit point, 0 <= u, v <= 1.

 * u = azimuth,  v = elevation
 */
void
rt_xxx_uv(struct application *ap, struct soltab *stp, struct hit *hitp, struct uvcoord *uvp)
{
    struct xxx_specific *xxx;

    if (ap) RT_CK_APPLICATION(ap);
    if (!stp || !uvp) return;
    RT_CK_SOLTAB(stp);
    if (hitp) RT_CK_HIT(hitp);

    xxx = (struct xxx_specific *)stp->st_specific;
    if (!xxx) return;
}


void
rt_xxx_free(struct soltab *stp)
{
    struct xxx_specific *xxx;

    if (!stp) return;
    RT_CK_SOLTAB(stp);
    xxx = (struct xxx_specific *)stp->st_specific;
    if (!xxx) return;

    /* release xxx_specific memory, however allocated in _prep() */
    BU_PUT(xxx, struct xxx_specific);

    return;
}


int
rt_xxx_plot(struct bu_list *vhead, struct rt_db_internal *ip, const struct bg_tess_tol *UNUSED(ttol), const struct bn_tol *UNUSED(tol), const struct bview *UNUSED(info))
{
    struct rt_xxx_internal *xxx_ip;

    BU_CK_LIST_HEAD(vhead);
    RT_CK_DB_INTERNAL(ip);
    xxx_ip = (struct rt_xxx_internal *)ip->idb_ptr;
    RT_XXX_CK_MAGIC(xxx_ip);

    return -1;
}


/**
 * Returns -
 * -1 failure
 * 0 OK.  *r points to nmgregion that holds this tessellation.
 */
int
rt_xxx_tess(struct nmgregion **r, struct model *m, struct rt_db_internal *ip, const struct bg_tess_tol *UNUSED(ttol), const struct bn_tol *UNUSED(tol))
{
    struct rt_xxx_internal *xxx_ip;

    if (r) NMG_CK_REGION(*r);
    if (m) NMG_CK_MODEL(m);
    RT_CK_DB_INTERNAL(ip);
    xxx_ip = (struct rt_xxx_internal *)ip->idb_ptr;
    RT_XXX_CK_MAGIC(xxx_ip);

    return -1;
}


/**
 * Export an XXX from internal form to external format.  Note that
 * this means converting all integers to Big-Endian format and
 * floating point data to IEEE double.
 *
 * Apply the transformation to mm units as well.
 */
int
rt_xxx_export5(struct bu_external *ep, const struct rt_db_internal *ip, double local2mm, const struct db_i *dbip)
{
    struct rt_xxx_internal *xxx_ip;

    /* must be double for import and export */
    double vec[ELEMENTS_PER_VECT];

    RT_CK_DB_INTERNAL(ip);
    if (ip->idb_type != ID_XXX) return -1;
    xxx_ip = (struct rt_xxx_internal *)ip->idb_ptr;
    RT_XXX_CK_MAGIC(xxx_ip);
    if (dbip) RT_CK_DBI(dbip);

    BU_CK_EXTERNAL(ep);
    ep->ext_nbytes = SIZEOF_NETWORK_DOUBLE * ELEMENTS_PER_VECT;
    ep->ext_buf = (uint8_t *)bu_calloc(1, ep->ext_nbytes, "xxx external");

    /* Since libwdb users may want to operate in units other than mm,
     * we offer the opportunity to scale the solid (to get it into mm)
     * on the way out.
     */
    VSCALE(vec, xxx_ip->v, local2mm);

    /* Convert from internal (host) to database (network) format */
    bu_cv_htond(ep->ext_buf, (unsigned char *)vec, ELEMENTS_PER_VECT);

    return 0;
}


/**
 * Import an XXX from the database format to the internal format.
 * Note that the data read will be in network order.  This means
 * Big-Endian integers and IEEE doubles for floating point.
 *
 * Apply modeling transformations as well.
 */
int
rt_xxx_import5(struct rt_db_internal *ip, const struct bu_external *ep, const mat_t mat, const struct db_i *dbip)
{
    struct rt_xxx_internal *xxx_ip;

    /* must be double for import and export */
    double vv[ELEMENTS_PER_VECT*1];

    RT_CK_DB_INTERNAL(ip);
    BU_CK_EXTERNAL(ep);
    if (dbip) RT_CK_DBI(dbip);

    BU_ASSERT(ep->ext_nbytes == SIZEOF_NETWORK_DOUBLE * 3*4);

    /* set up the internal structure */
    ip->idb_major_type = DB5_MAJORTYPE_BRLCAD;
    ip->idb_type = ID_XXX;
    ip->idb_meth = &OBJ[ID_XXX];
    BU_ALLOC(ip->idb_ptr, struct rt_xxx_internal);

    xxx_ip = (struct rt_xxx_internal *)ip->idb_ptr;
    xxx_ip->magic = RT_XXX_INTERNAL_MAGIC;

    /* Convert the data in ep->ext_buf into internal format.  Note the
     * conversion from network data (Big Endian ints, IEEE double
     * floating point) to host local data representations.
     */
    bu_cv_ntohd((unsigned char *)&vv, (unsigned char *)ep->ext_buf, ELEMENTS_PER_VECT*1);

    /* Apply the modeling transformation */
    if (mat == NULL) mat = bn_mat_identity;
    MAT4X3PNT(xxx_ip->v, mat, vv);

    return 0;			/* OK */
}


/**
 * Make human-readable formatted presentation of this solid.  First
 * line describes type of solid.  Additional lines are indented one
 * tab, and give parameter values.
 */
int
rt_xxx_describe(struct bu_vls *str, const struct rt_db_internal *ip, int verbose, double mm2local)
{
    struct rt_xxx_internal *xxx_ip =
	(struct rt_xxx_internal *)ip->idb_ptr;
    char buf[256];

    RT_XXX_CK_MAGIC(xxx_ip);
    bu_vls_strcat(str, "truncated general xxx (XXX)\n");

    if (!verbose)
	return 0;

    sprintf(buf, "\tV (%g, %g, %g)\n",
	    INTCLAMP(xxx_ip->v[X] * mm2local),
	    INTCLAMP(xxx_ip->v[Y] * mm2local),
	    INTCLAMP(xxx_ip->v[Z] * mm2local));
    bu_vls_strcat(str, buf);

    return 0;
}


/**
 * Free the storage associated with the rt_db_internal version of this
 * solid.
 */
void
rt_xxx_ifree(struct rt_db_internal *ip)
{
    struct rt_xxx_internal *xxx_ip;

    RT_CK_DB_INTERNAL(ip);

    xxx_ip = (struct rt_xxx_internal *)ip->idb_ptr;
    RT_XXX_CK_MAGIC(xxx_ip);
    xxx_ip->magic = 0;			/* sanity */

    bu_free((char *)xxx_ip, "xxx ifree");
    ip->idb_ptr = ((void *)0);	/* sanity */
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
