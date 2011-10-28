/*                           X X X . C
 * BRL-CAD
 *
 * Copyright (c) 1990-2011 United States Government as represented by
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
 * Adding a new solid type:
 *
 * Design disk record
 *
 * define rt_xxx_internal --- parameters for solid
 * define xxx_specific --- raytracing form, possibly w/precomuted terms
 * define rt_xxx_parse --- struct bu_structparse for "db get", "db adjust", ...
 *
 * code import/export4/describe/print/ifree/plot/prep/shot/curve/uv/tess
 *
 * edit db.h add solidrec s_type define
 * edit rtgeom.h to add rt_xxx_internal
 * edit magic.h to add RT_XXX_INTERNAL_MAGIC
 * edit table.c:
 *	RT_DECLARE_INTERFACE()
 *	struct rt_functab entry
 *	rt_id_solid()
 * edit raytrace.h to make ID_XXX, increment ID_MAXIMUM
 * edit db_scan.c to add the new solid to db_scan()
 * edit Makefile.am to add g_xxx.c to compile
 *
 * go to src/libwdb and create mk_xxx() routine
 * go to src/conv and edit g2asc.c and asc2g.c to support the new solid
 * go to src/librt and edit tcl.c to add the new solid to
 *	rt_solid_type_lookup[]
 *	also add the interface table and to rt_id_solid() in table.c
 * go to src/mged and create the edit support
 *
 */
/** @} */

#include "common.h"

#include <stdio.h>
#include <math.h>

#include "vmath.h"
#include "db.h"
#include "nmg.h"
#include "rtgeom.h"
#include "raytrace.h"

/* local interface header */
#include "./xxx.h"


/**
 * R T _ X X X _ P R E P
 *
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

    if (stp) RT_CK_SOLTAB(stp);
    RT_CK_DB_INTERNAL(ip);
    if (rtip) RT_CK_RTI(rtip);

    xxx_ip = (struct rt_xxx_internal *)ip->idb_ptr;
    RT_XXX_CK_MAGIC(xxx_ip);

    return 0;
}


/**
 * R T _ X X X _ P R I N T
 */
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
 * R T _ X X X _ S H O T
 *
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
 * R T _ X X X _ N O R M
 *
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
 * R T _ X X X _ C U R V E
 *
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
 * R T _ X X X _ U V
 *
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


/**
 * R T _ X X X _ F R E E
 */
void
rt_xxx_free(struct soltab *stp)
{
    struct xxx_specific *xxx;

    if (!stp) return;
    RT_CK_SOLTAB(stp);
    xxx = (struct xxx_specific *)stp->st_specific;
    if (!xxx) return;

    bu_free((char *)xxx, "xxx_specific");
}


/**
 * R T _ X X X _ P L O T
 */
int
rt_xxx_plot(struct bu_list *vhead, struct rt_db_internal *ip, const struct rt_tess_tol *UNUSED(ttol), const struct bn_tol *UNUSED(tol), const struct rt_view_info *UNUSED(info))
{
    struct rt_xxx_internal *xxx_ip;

    BU_CK_LIST_HEAD(vhead);
    RT_CK_DB_INTERNAL(ip);
    xxx_ip = (struct rt_xxx_internal *)ip->idb_ptr;
    RT_XXX_CK_MAGIC(xxx_ip);

    return -1;
}


/**
 * R T _ X X X _ T E S S
 *
 * Returns -
 * -1 failure
 * 0 OK.  *r points to nmgregion that holds this tessellation.
 */
int
rt_xxx_tess(struct nmgregion **r, struct model *m, struct rt_db_internal *ip, const struct rt_tess_tol *UNUSED(ttol), const struct bn_tol *UNUSED(tol))
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
 * R T _ X X X _ I M P O R T 5
 *
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
    fastf_t vv[ELEMENTS_PER_VECT*1];

    RT_CK_DB_INTERNAL(ip);
    BU_CK_EXTERNAL(ep);
    if (dbip) RT_CK_DBI(dbip);

    BU_ASSERT_LONG(ep->ext_nbytes, ==, SIZEOF_NETWORK_DOUBLE * 3*4);

    /* set up the internal structure */
    ip->idb_major_type = DB5_MAJORTYPE_BRLCAD;
    ip->idb_type = ID_XXX;
    ip->idb_meth = &rt_functab[ID_XXX];
    ip->idb_ptr = bu_malloc(sizeof(struct rt_xxx_internal), "rt_xxx_internal");
    xxx_ip = (struct rt_xxx_internal *)ip->idb_ptr;
    xxx_ip->magic = RT_XXX_INTERNAL_MAGIC;

    /* Convert the data in ep->ext_buf into internal format.  Note the
     * conversion from network data (Big Endian ints, IEEE double
     * floating point) to host local data representations.
     */
    ntohd((unsigned char *)&vv, (unsigned char *)ep->ext_buf, ELEMENTS_PER_VECT*1);

    /* Apply the modeling transformation */
    if (mat == NULL) mat = bn_mat_identity;
    MAT4X3PNT(xxx_ip->v, mat, vv);

    return 0;			/* OK */
}


/**
 * R T _ X X X _ E X P O R T 5
 *
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
    fastf_t vec[ELEMENTS_PER_VECT];

    RT_CK_DB_INTERNAL(ip);
    if (ip->idb_type != ID_XXX) return -1;
    xxx_ip = (struct rt_xxx_internal *)ip->idb_ptr;
    RT_XXX_CK_MAGIC(xxx_ip);
    if (dbip) RT_CK_DBI(dbip);

    BU_CK_EXTERNAL(ep);
    ep->ext_nbytes = SIZEOF_NETWORK_DOUBLE * ELEMENTS_PER_VECT;
    ep->ext_buf = (genptr_t)bu_calloc(1, ep->ext_nbytes, "xxx external");


    /* Since libwdb users may want to operate in units other than mm,
     * we offer the opportunity to scale the solid (to get it into mm)
     * on the way out.
     */
    VSCALE(vec, xxx_ip->v, local2mm);

    /* Convert from internal (host) to database (network) format */
    htond(ep->ext_buf, (unsigned char *)vec, ELEMENTS_PER_VECT*1);

    return 0;
}


/**
 * R T _ X X X _ D E S C R I B E
 *
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
 * R T _ X X X _ I F R E E
 *
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
    ip->idb_ptr = GENPTR_NULL;	/* sanity */
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
