/*                      S U B M O D E L . C
 * BRL-CAD
 *
 * Copyright (c) 2000-2011 United States Government as represented by
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
/** @file primitives/submodel/submodel.c
 *
 * Intersect a ray with an entire subspace full of geometry, possibly
 * included from another .g file, with a subordinate instance of
 * LIBRT.
 *
 * This solid is particularly useful when instancing millions of
 * copies of a given complex object, such as a detailed tree.
 *
 */
/** @} */

#include "common.h"

#include <stddef.h>
#include <stdio.h>
#include <math.h>
#include <string.h>
#include "bio.h"

#include "vmath.h"
#include "db.h"
#include "nmg.h"
#include "rtgeom.h"
#include "raytrace.h"

#define RT_SUBMODEL_O(m) bu_offsetof(struct rt_submodel_internal, m)

const struct bu_structparse rt_submodel_parse[] = {
    {"%V", 1, "file", RT_SUBMODEL_O(file), BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    {"%V", 1, "treetop", RT_SUBMODEL_O(treetop), BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    {"%d", 1, "meth", RT_SUBMODEL_O(meth), BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    {"", 0, (char *)0, 0, BU_STRUCTPARSE_FUNC_NULL, NULL, NULL }
};


/* ray tracing form of solid, including precomputed terms */
struct submodel_specific {
    uint32_t magic;
    mat_t subm2m;		/* To transform normals back out to model coords */
    mat_t m2subm;		/* To transform rays into local coord sys */
    struct rt_i *rtip;		/* sub model */
};
#define RT_SUBMODEL_SPECIFIC_MAGIC 0x73756253	/* subS */
#define RT_CK_SUBMODEL_SPECIFIC(_p) BU_CKMAG(_p, RT_SUBMODEL_SPECIFIC_MAGIC, "submodel_specific")


/**
 * R T _ S U B M O D E L _ P R E P
 *
 * Given a pointer to a GED database record, and a transformation matrix,
 * determine if this is a valid SUBMODEL, and if so, precompute various
 * terms of the formula.
 *
 * Returns -
 * 0 SUBMODEL is OK
 * !0 Error in description
 *
 * Implicit return -
 * A struct submodel_specific is created, and its address is stored in
 * stp->st_specific for use by submodel_shot().
 */
int
rt_submodel_prep(struct soltab *stp, struct rt_db_internal *ip, struct rt_i *rtip)
{
    struct rt_submodel_internal *sip;
    struct submodel_specific *submodel;
    struct rt_i *sub_rtip;
    struct db_i *sub_dbip;
    struct resource *resp;
    vect_t radvec;
    vect_t diam;
    char *argv[2];
    struct rt_i **rtipp;

    RT_CK_DB_INTERNAL(ip);
    sip = (struct rt_submodel_internal *)ip->idb_ptr;
    RT_SUBMODEL_CK_MAGIC(sip);

    bu_semaphore_acquire(RT_SEM_MODEL);
    /*
     * This code must be prepared to run in parallel
     * without tripping over itself.
     */
    if (bu_vls_strlen(&sip->file) == 0) {
	/* No .g file name given, tree is in current .g file */
	sub_dbip = rtip->rti_dbip;
    } else {
	/* db_open will cache dbip's via bu_open_mapped_file() */
	if ((sub_dbip = db_open(bu_vls_addr(&sip->file), "r")) == DBI_NULL)
	    return -1;

	/* Save the overhead of stat() calls on subsequent opens */
	if (sub_dbip->dbi_mf) sub_dbip->dbi_mf->dont_restat = 1;

	if (!db_is_directory_non_empty(sub_dbip)) {
	    /* This is first open of db, build directory */
	    if (db_dirbuild(sub_dbip) < 0) {
		db_close(sub_dbip);
		bu_semaphore_release(RT_SEM_MODEL);
		return -1;
	    }
	}
    }

    /*
     * Search for a previous exact use of this file and treetop,
     * so as to obtain storage efficiency from re-using it.
     * Search dbi_client list for a re-use of an rt_i.
     * rtip's are registered there by db_clone_dbi().
     */
    for (BU_PTBL_FOR(rtipp, (struct rt_i **), &sub_dbip->dbi_clients)) {
	char *ttp;
	RT_CK_RTI(*rtipp);
	ttp = (*rtipp)->rti_treetop;
	if (ttp && BU_STR_EQUAL(ttp, bu_vls_addr(&sip->treetop))) {
	    /* Re-cycle an already prepped rti */
	    sub_rtip = *rtipp;
	    sub_rtip->rti_uses++;

	    bu_semaphore_release(RT_SEM_MODEL);

	    if (RT_G_DEBUG & (DEBUG_DB|DEBUG_SOLIDS)) {
		bu_log("rt_submodel_prep(%s): Re-used already prepped database %s, rtip=%p\n",
		       stp->st_dp->d_namep,
		       sub_dbip->dbi_filename,
		       (void *)sub_rtip);
	    }
	    goto done;
	}
    }

    sub_rtip = rt_new_rti(sub_dbip);	/* does db_clone_dbi() */
    RT_CK_RTI(sub_rtip);

    /* Set search term before leaving critical section */
    sub_rtip->rti_treetop = bu_vls_strdup(&sip->treetop);

    bu_semaphore_release(RT_SEM_MODEL);

    if (RT_G_DEBUG & (DEBUG_DB|DEBUG_SOLIDS)) {
	bu_log("rt_submodel_prep(%s): Opened database %s\n",
	       stp->st_dp->d_namep, sub_dbip->dbi_filename);
    }

    /*
     * Initialize per-processor resources for the submodel.
     * We treewalk here with only one processor (CPU 0).
     * db_walk_tree() as called from
     * rt_gettrees() will pluck the 0th resource out of the rtip table.
     * rt_submodel_shot() will get additional resources as needed.
     */
    BU_GETSTRUCT(resp, resource);
    BU_PTBL_SET(&sub_rtip->rti_resources, 0, resp);
    rt_init_resource(resp, 0, sub_rtip);

    /* Propagate some important settings downward */
    sub_rtip->useair = rtip->useair;
    sub_rtip->rti_dont_instance = rtip->rti_dont_instance;
    sub_rtip->rti_hasty_prep = rtip->rti_hasty_prep;
    sub_rtip->rti_tol = rtip->rti_tol;	/* struct copy */
    sub_rtip->rti_ttol = rtip->rti_ttol;	/* struct copy */

    if (sip->meth) {
	sub_rtip->rti_space_partition = sip->meth;
    } else {
	sub_rtip->rti_space_partition = rtip->rti_space_partition;
    }

    argv[0] = bu_vls_addr(&sip->treetop);
    argv[1] = NULL;
    if (rt_gettrees(sub_rtip, 1, (const char **)argv, 1) < 0) {
	bu_log("submodel(%s) rt_gettrees(%s) failed\n", stp->st_name, argv[0]);
	/* Can't call rt_free_rti(sub_rtip) because it may have
	 * already been instanced!
	 */
	return -2;
    }

    if (sub_rtip->nsolids <= 0) {
	bu_log("rt_submodel_prep(%s): %s No primitives found\n",
	       stp->st_dp->d_namep, bu_vls_addr(&sip->file));
	/* Can't call rt_free_rti(sub_rtip) because it may have
	 * already been instanced!
	 */
	return -3;
    }

    /* OK, it's going to work.  Prep the submodel. */
    /* Stay on 1 CPU because we're already multi-threaded at this point. */
    rt_prep_parallel(sub_rtip, 1);

    /* Ensure bu_ptbl rti_resources is full size.  Ptrs will be null */
    if (BU_PTBL_LEN(&sub_rtip->rti_resources) < sub_rtip->rti_resources.blen) {
	BU_PTBL_END(&sub_rtip->rti_resources) = sub_rtip->rti_resources.blen;
    }

    if (RT_G_DEBUG) rt_pr_cut_info(sub_rtip, stp->st_name);

 done:
    BU_GETSTRUCT(submodel, submodel_specific);
    submodel->magic = RT_SUBMODEL_SPECIFIC_MAGIC;
    stp->st_specific = (genptr_t)submodel;

    MAT_COPY(submodel->subm2m, sip->root2leaf);
    bn_mat_inv(submodel->m2subm, sip->root2leaf);
    submodel->rtip = sub_rtip;

    /* Propagage submodel bounding box back upwards, rotated&scaled. */
    bn_rotate_bbox(stp->st_min, stp->st_max,
		   submodel->subm2m,
		   sub_rtip->mdl_min, sub_rtip->mdl_max);

    VSUB2(diam, stp->st_max, stp->st_min);
    VADD2SCALE(stp->st_center, stp->st_min, stp->st_max, 0.5);
    VSCALE(radvec, diam, 0.5);
    stp->st_aradius = stp->st_bradius = MAGNITUDE(radvec);

    if (RT_G_DEBUG & (DEBUG_DB|DEBUG_SOLIDS)) {
	bu_log("rt_submodel_prep(%s): finished loading database %s\n",
	       stp->st_dp->d_namep, sub_dbip->dbi_filename);
    }

    return 0;		/* OK */
}


/**
 * R T _ S U B M O D E L _ P R I N T
 */
void
rt_submodel_print(const struct soltab *stp)
{
    const struct submodel_specific *submodel =
	(struct submodel_specific *)stp->st_specific;

    RT_CK_SUBMODEL_SPECIFIC(submodel);

    bn_mat_print("subm2m", submodel->subm2m);
    bn_mat_print("m2subm", submodel->m2subm);

    bu_log_indent_delta(4);
    bu_log("submodel->rtip=x%x\n", submodel->rtip);

    /* Loop through submodel's solid table printing them too. */
    RT_VISIT_ALL_SOLTABS_START(stp, submodel->rtip) {
	rt_pr_soltab(stp);
    } RT_VISIT_ALL_SOLTABS_END
	  bu_log_indent_delta(-4);
}


/**
 * R T _ S U B M O D E L _ A _ M I S S
 */
int
rt_submodel_a_miss(struct application *ap)
{
    if (ap) RT_CK_APPLICATION(ap);

    return 0;
}


struct submodel_gobetween {
    struct application *up_ap;
    struct seg *up_seghead;
    struct soltab *up_stp;
    fastf_t delta;		/* distance offset */
};


/**
 * R T _ S U B M O D E L _ A _ H I T
 */
int
rt_submodel_a_hit(struct application *ap, struct partition *PartHeadp, struct seg *UNUSED(segHeadp))
{
    struct partition *pp;
    struct application *up_ap;
    struct soltab *up_stp;
    struct region *up_reg;
    struct submodel_gobetween *gp;
    struct submodel_specific *submodel;
    int count = 0;

    RT_AP_CHECK(ap);
    RT_CK_PT_HD(PartHeadp);
    gp = (struct submodel_gobetween *)ap->a_uptr;
    up_ap = gp->up_ap;
    RT_AP_CHECK(up_ap);		/* original ap, in containing */
    RT_CK_RTI(up_ap->a_rt_i);
    up_stp = gp->up_stp;
    RT_CK_SOLTAB(up_stp);
    submodel = (struct submodel_specific *)up_stp->st_specific;
    RT_CK_SUBMODEL_SPECIFIC(submodel);

    /* Take the first containing region */
    up_reg = (struct region *)BU_PTBL_GET(&(up_stp->st_regions), 0);
    RT_CK_REGION(up_reg);

    /* Need to tackle this honestly --
     * build a totally new segment list from the partition list,
     * with normals, uv, and curvature already computed,
     * and converted back into up_model coordinate system,
     * so the lazy-evaluation routines don't have to do anything.
     * This is probably almost as cheap as the coordinate
     * re-mapping in the special hook routines.
     * Then the submodel can be stacked arbitrarily deep.
     */
    for (BU_LIST_FOR(pp, partition, (struct bu_list *)PartHeadp)) {
	struct seg *up_segp;
	struct seg *inseg;
	struct seg *outseg;

	RT_CK_PT(pp);
	inseg = pp->pt_inseg;
	outseg = pp->pt_outseg;
	RT_CK_SEG(inseg);
	RT_CK_SEG(outseg);

	/*
	 * Construct a completely new segment
	 * build seg_in, seg_out, and seg_stp.
	 * Take seg_in and seg_out literally, to track surfno, etc.,
	 * then update specific values.
	 */
	RT_GET_SEG(up_segp, up_ap->a_resource);
	up_segp->seg_in = inseg->seg_in;		/* struct copy */
	up_segp->seg_out = outseg->seg_out;	/* struct copy */
	up_segp->seg_stp = up_stp;

	/* Adjust for scale difference */
	MAT4XSCALOR(up_segp->seg_in.hit_dist, submodel->subm2m, inseg->seg_in.hit_dist);
	up_segp->seg_in.hit_dist -= gp->delta;
	MAT4XSCALOR(up_segp->seg_out.hit_dist, submodel->subm2m, outseg->seg_out.hit_dist);
	up_segp->seg_out.hit_dist -= gp->delta;

	BU_ASSERT_DOUBLE(up_segp->seg_in.hit_dist, <=, up_segp->seg_out.hit_dist);

	/* Link to ray in upper model, not submodel */
	up_segp->seg_in.hit_rayp = &up_ap->a_ray;
	up_segp->seg_out.hit_rayp = &up_ap->a_ray;

	/* Pre-calculate what would have been "lazy evaluation" */
	VJOIN1(up_segp->seg_in.hit_point,  up_ap->a_ray.r_pt,
	       up_segp->seg_in.hit_dist, up_ap->a_ray.r_dir);
	VJOIN1(up_segp->seg_out.hit_point,  up_ap->a_ray.r_pt,
	       up_segp->seg_out.hit_dist, up_ap->a_ray.r_dir);

	/* RT_HIT_NORMAL */
	inseg->seg_stp->st_meth->ft_norm(&inseg->seg_in, inseg->seg_stp, inseg->seg_in.hit_rayp);
	outseg->seg_stp->st_meth->ft_norm(&outseg->seg_out, outseg->seg_stp, outseg->seg_out.hit_rayp);

	/* XXX error checking */
	{
	    fastf_t cosine = fabs(VDOT(ap->a_ray.r_dir, inseg->seg_in.hit_normal));
	    if (cosine > 1.00001) {
		bu_log("rt_submodel_a_hit() cos=1+%g, %s surfno=%d\n",
		       cosine-1,
		       inseg->seg_stp->st_dp->d_namep,
		       inseg->seg_in.hit_surfno);
		VPRINT("inseg->seg_in.hit_normal", inseg->seg_in.hit_normal);
	    }
	}
	MAT3X3VEC(up_segp->seg_in.hit_normal, submodel->subm2m,
		  inseg->seg_in.hit_normal);
/* XXX error checking */
	{
	    fastf_t cosine = fabs(VDOT(up_ap->a_ray.r_dir, up_segp->seg_in.hit_normal));
	    if (cosine > 1.00001) {
		bu_log("rt_submodel_a_hit() cos=1+%g, %s surfno=%d\n",
		       cosine-1,
		       inseg->seg_stp->st_dp->d_namep,
		       inseg->seg_in.hit_surfno);
		VPRINT("up_segp->seg_in.hit_normal", up_segp->seg_in.hit_normal);
	    }
	}
	MAT3X3VEC(up_segp->seg_out.hit_normal, submodel->subm2m,
		  outseg->seg_out.hit_normal);

	/* RT_HIT_UV */
	{
	    struct uvcoord uv = {0.0, 0.0, 0.0, 0.0};
	    RT_HIT_UVCOORD(ap, inseg->seg_stp, &inseg->seg_in, &uv);
	    up_segp->seg_in.hit_vpriv[X] = uv.uv_u;
	    up_segp->seg_in.hit_vpriv[Y] = uv.uv_v;
	    if (uv.uv_du >= uv.uv_dv)
		up_segp->seg_in.hit_vpriv[Z] = uv.uv_du;
	    else
		up_segp->seg_in.hit_vpriv[Z] = uv.uv_dv;

	    RT_HIT_UVCOORD(ap, outseg->seg_stp, &outseg->seg_out, &uv);
	    up_segp->seg_out.hit_vpriv[X] = uv.uv_u;
	    up_segp->seg_out.hit_vpriv[Y] = uv.uv_v;
	    if (uv.uv_du >= uv.uv_dv)
		up_segp->seg_out.hit_vpriv[Z] = uv.uv_du;
	    else
		up_segp->seg_out.hit_vpriv[Z] = uv.uv_dv;
	}

	/* RT_HIT_CURVATURE */
	/* no place to stash curvature data! */

	/*
	 * Here, the surfno reported upwards is the solid's
	 * index (bit) number in the submodel.
	 * This can be used as subscript to rti_Solids[]
	 * to retrieve the identity of the solid that was hit.
	 */
	up_segp->seg_in.hit_surfno = inseg->seg_stp->st_bit;
	up_segp->seg_out.hit_surfno = outseg->seg_stp->st_bit;

	/* Put this segment on caller's shot routine seglist */
	BU_LIST_INSERT(&(gp->up_seghead->l), &(up_segp->l));
	count++;
    }
    return count;
}


/**
 * R T _ S U B M O D E L _ S H O T
 *
 * Intersect a ray with a submodel.
 * If an intersection occurs, a struct seg will be acquired
 * and filled in.
 *
 * Returns -
 * 0 MISS
 * >0 HIT
 */
int
rt_submodel_shot(struct soltab *stp, struct xray *rp, struct application *ap, struct seg *seghead)
{
    struct submodel_specific *submodel =
	(struct submodel_specific *)stp->st_specific;
    struct application sub_ap;
    struct submodel_gobetween gb;
    vect_t vdiff;
    int code;
    struct bu_ptbl *restbl;
    struct resource *resp;
    int cpu;

    RT_CK_SOLTAB(stp);
    RT_CK_RTI(ap->a_rt_i);
    RT_CK_SUBMODEL_SPECIFIC(submodel);

    gb.up_ap = ap;
    gb.up_seghead = seghead;
    gb.up_stp = stp;

    sub_ap = *ap;		/* struct copy */
    sub_ap.a_rt_i = submodel->rtip;
    sub_ap.a_hit = rt_submodel_a_hit;
    sub_ap.a_miss = rt_submodel_a_miss;
    sub_ap.a_uptr = (genptr_t)&gb;
    sub_ap.a_purpose = "rt_submodel_shot";

    /* Ensure even # of accurate hits, for building good partitions */
    if (sub_ap.a_onehit < 0) {
	if (sub_ap.a_onehit&1) sub_ap.a_onehit--;
    } else {
	if (sub_ap.a_onehit&1) sub_ap.a_onehit++;
    }

    /*
     * Obtain the resource structure for this CPU.
     * No need to semaphore because there is one pointer per cpu already.
     */
    restbl = &submodel->rtip->rti_resources;	/* a ptbl */
    cpu = ap->a_resource->re_cpu;
    BU_ASSERT_LONG(cpu, <, BU_PTBL_END(restbl));
    if ((resp = (struct resource *)BU_PTBL_GET(restbl, cpu)) == NULL) {
	/* First ray for this cpu for this submodel, alloc up */
	BU_GETSTRUCT(resp, resource);
	BU_PTBL_SET(restbl, cpu, resp);
	rt_init_resource(resp, cpu, submodel->rtip);
    }
    RT_CK_RESOURCE(resp);
    sub_ap.a_resource = resp;

    /* shootray already computed a_ray.r_min & r_max for us */
    /* Construct the ray in submodel coords. */
    /* Do this in a repeatable way */
    /* Distances differ only by a scale factor of m[15] */
    MAT4X3PNT(sub_ap.a_ray.r_pt, submodel->m2subm, ap->a_ray.r_pt);
    MAT3X3VEC(sub_ap.a_ray.r_dir, submodel->m2subm, ap->a_ray.r_dir);

    /* NOTE: ap->a_ray.r_pt is not the same as rp->r_pt! */
    /* This changes the distances */
    VSUB2(vdiff, rp->r_pt, ap->a_ray.r_pt);
    gb.delta = VDOT(vdiff, ap->a_ray.r_dir);

    code = rt_shootray(&sub_ap);

    if (code <= 0) return 0;	/* MISS */

    /* All the real (sneaky) work is done in the hit routine */
    /* a_hit routine will have added the segs to seghead */

    return 1;		/* HIT */
}


/**
 * R T _ S U B M O D E L _ N O R M
 *
 * Given ONE ray distance, return the normal and entry/exit point.
 */
void
rt_submodel_norm(struct hit *hitp, struct soltab *stp, struct xray *rp)
{
    RT_CK_HIT(hitp);

    /* hitp->hit_point is already valid */
    /* hitp->hit_normal is already valid */
/* XXX error checking */
    {
	fastf_t cosine = fabs(VDOT(rp->r_dir, hitp->hit_normal));
	if (cosine > 1.00001) {
	    bu_log("rt_submodel_norm() cos=1+%g, %s surfno=%d\n",
		   cosine-1,
		   stp->st_dp->d_namep,
		   hitp->hit_surfno);
	}
    }
}


/**
 * R T _ S U B M O D E L _ C U R V E
 *
 * Return the curvature of the submodel.
 */
void
rt_submodel_curve(struct curvature *cvp, struct hit *hitp, struct soltab *stp)
{
    if (!cvp || !hitp)
	return;
    RT_CK_HIT(hitp);
    if (stp) RT_CK_SOLTAB(stp);

    cvp->crv_c1 = cvp->crv_c2 = 0;

    /* any tangent direction */
    bn_vec_ortho(cvp->crv_pdir, hitp->hit_normal);

    /* XXX This will never be called */
    bu_log("rt_submodel_curve() not implemented, need extra fields in 'struct hit'\n");
}


/**
 * R T _ S U B M O D E L _ U V
 *
 * For a hit on the surface of an submodel, return the (u, v) coordinates
 * of the hit point, 0 <= u, v <= 1.
 * u = azimuth
 * v = elevation
 */
void
rt_submodel_uv(struct application *ap, struct soltab *stp, struct hit *hitp, struct uvcoord *uvp)
{
    if (ap) RT_CK_APPLICATION(ap);
    if (stp) RT_CK_SOLTAB(stp);
    if (!hitp || !uvp)
	return;
    RT_CK_HIT(hitp);

    uvp->uv_u = hitp->hit_vpriv[X];
    uvp->uv_v = hitp->hit_vpriv[Y];
    uvp->uv_du = uvp->uv_dv = hitp->hit_vpriv[Z];
}


/**
 * R T _ S U B M O D E L _ F R E E
 */
void
rt_submodel_free(struct soltab *stp)
{
    struct submodel_specific *submodel =
	(struct submodel_specific *)stp->st_specific;
    struct resource **rpp;
    struct rt_i *rtip;

    RT_CK_SUBMODEL_SPECIFIC(submodel);
    rtip = submodel->rtip;
    RT_CK_RTI(rtip);

    /* Specificially free resource structures here */
    BU_CK_PTBL(&rtip->rti_resources);
    for (BU_PTBL_FOR(rpp, (struct resource **), &rtip->rti_resources)) {
	if (*rpp == NULL) continue;
	if (*rpp == &rt_uniresource) continue;
	RT_CK_RESOURCE(*rpp);
	/* Cleans but does not free the resource struct */
	rt_clean_resource(rtip, *rpp);
	bu_free(*rpp, "struct resource (submodel)");
	/* Forget remembered ptr */
	*rpp = NULL;
    }
    /* Keep the ptbl allocated. */

    rt_free_rti(submodel->rtip);

    bu_free((genptr_t)submodel, "submodel_specific");
}


/**
 * R T _ S U B M O D E L _ C L A S S
 */
int
rt_submodel_class(const struct soltab *stp, const fastf_t *min, const fastf_t *max, const struct bn_tol *tol)
{
    if (stp) RT_CK_SOLTAB(stp);
    if (tol) BN_CK_TOL(tol);
    if (!min) return 0;
    if (!max) return 0;

    return 0;
}


struct goodies {
    struct db_i *dbip;
    struct bu_list *vheadp;
};


/**
 * R T _ S U B M O D E L _ W I R E F R A M E _ L E A F
 *
 * This routine must be prepared to run in parallel.
 * This routine should be generally exported for other uses.
 */
HIDDEN union tree *
rt_submodel_wireframe_leaf(struct db_tree_state *tsp, const struct db_full_path *pathp, struct rt_db_internal *ip, genptr_t UNUSED(client_data))
{
    union tree *curtree;
    struct goodies *gp;
    int ret;

    RT_CK_TESS_TOL(tsp->ts_ttol);
    BN_CK_TOL(tsp->ts_tol);
    RT_CK_DB_INTERNAL(ip);
    RT_CK_RESOURCE(tsp->ts_resp);

    gp = (struct goodies *)tsp->ts_m;	/* hack */
    if (gp) RT_CK_DBI(gp->dbip);

    /* NON-PARALLEL access to vlist pointed to by vheadp is not semaphored */
    if (bu_is_parallel()) bu_bomb("rt_submodel_wireframe_leaf() non-parallel code\n");

    if (RT_G_DEBUG&DEBUG_TREEWALK) {
	char *sofar = db_path_to_string(pathp);

	bu_log("rt_submodel_wireframe_leaf(%s) path=%s\n",
	       ip->idb_meth->ft_name, sofar);
	bu_free((genptr_t)sofar, "path string");
    }

    ret = -1;
    if (ip->idb_meth->ft_plot) {
	ret = ip->idb_meth->ft_plot(gp->vheadp, ip, tsp->ts_ttol, tsp->ts_tol);
    }
    if (ret < 0) {
	bu_log("rt_submodel_wireframe_leaf(%s): %s plot failure\n",
	       ip->idb_meth->ft_name,
	       DB_FULL_PATH_CUR_DIR(pathp)->d_namep);
	return TREE_NULL;		/* ERROR */
    }

    /* Indicate success by returning something other than TREE_NULL */
    RT_GET_TREE(curtree, tsp->ts_resp);
    curtree->tr_op = OP_NOP;

    return curtree;
}


/**
 * R T _ S U B M O D E L _ P L O T
 *
 * Not unlike mged/dodraw.c drawtrees()
 *
 * Note:  The submodel will be drawn entirely in one color
 * (for mged, typically this is red),
 * because we can't return a vlblock, only one vlist,
 * which by definition, is all one color.
 */
int
rt_submodel_plot(struct bu_list *vhead, struct rt_db_internal *ip, const struct rt_tess_tol *ttol, const struct bn_tol *tol)
{
    struct rt_submodel_internal *sip;
    struct db_tree_state state;
    int ret;
    char *argv[2];
    struct goodies good;

    BU_CK_LIST_HEAD(vhead);
    RT_CK_DB_INTERNAL(ip);
    sip = (struct rt_submodel_internal *)ip->idb_ptr;
    RT_SUBMODEL_CK_MAGIC(sip);

    /* BU_LIST_INIT(vhead); */

    state = rt_initial_tree_state;	/* struct copy */
    state.ts_ttol = ttol;
    state.ts_tol = tol;
    MAT_COPY(state.ts_mat, sip->root2leaf);

    state.ts_m = (struct model **)&good;	/* hack -- passthrough to rt_submodel_wireframe_leaf() */
    good.vheadp = vhead;

    if (bu_vls_strlen(&sip->file) != 0) {
	/* db_open will cache dbip's via bu_open_mapped_file() */
	if ((good.dbip = db_open(bu_vls_addr(&sip->file), "r")) == DBI_NULL) {
	    bu_log("rt_submodel_plot() db_open(%s) failure\n", bu_vls_addr(&sip->file));
	    return -1;
	}
	if (!db_is_directory_non_empty(good.dbip)) {
	    /* This is first open of this database, build directory */
	    if (db_dirbuild(good.dbip) < 0) {
		bu_log("rt_submodel_plot() db_dirbuild() failure\n");
		db_close(good.dbip);
		return -1;
	    }
	}
    } else {
	/* stash a pointer to the current database instance
	 * (should the dbi be cloned?)
	 */
	RT_CK_DBI(sip->dbip);
	good.dbip = (struct db_i *)sip->dbip;	/* un-const-cast */
    }

    argv[0] = bu_vls_addr(&sip->treetop);
    argv[1] = NULL;
    ret = db_walk_tree(good.dbip, 1, (const char **)argv,
		       1,
		       &state,
		       0,			/* take all regions */
		       NULL,			/* rt_submodel_wireframe_region_end */
		       rt_submodel_wireframe_leaf,
		       (genptr_t)NULL);

    if (ret < 0) bu_log("rt_submodel_plot() db_walk_tree(%s) failure\n", bu_vls_addr(&sip->treetop));
    if (bu_vls_strlen(&sip->file) != 0)
	db_close(good.dbip);
    return ret;
}


/**
 * R T _ S U B M O D E L _ T E S S
 *
 * Returns -
 * -1 failure
 * 0 OK.  *r points to nmgregion that holds this tessellation.
 */
int
rt_submodel_tess(struct nmgregion **r, struct model *m, struct rt_db_internal *ip, const struct rt_tess_tol *UNUSED(ttol), const struct bn_tol *UNUSED(tol))
{
    struct rt_submodel_internal *sip;

    if (r) NMG_CK_REGION(*r);
    if (m) NMG_CK_MODEL(m);
    RT_CK_DB_INTERNAL(ip);

    sip = (struct rt_submodel_internal *)ip->idb_ptr;
    RT_SUBMODEL_CK_MAGIC(sip);

    return -1;
}


/**
 * R T _ S U B M O D E L _ I M P O R T
 *
 * Import an SUBMODEL from the database format to the internal format.
 * Apply modeling transformations as well.
 */
int
rt_submodel_import4(struct rt_db_internal *ip, const struct bu_external *ep, const fastf_t *mat, const struct db_i *dbip)
{
    struct rt_submodel_internal *sip;
    union record *rp;
    struct bu_vls str;

    BU_CK_EXTERNAL(ep);
    RT_CK_DBI(dbip);

    rp = (union record *)ep->ext_buf;
    /* Check record type */
    if (rp->u_id != DBID_STRSOL) {
	bu_log("rt_submodel_import4: defective strsol record\n");
	return -1;
    }

    RT_CK_DB_INTERNAL(ip);
    ip->idb_major_type = DB5_MAJORTYPE_BRLCAD;
    ip->idb_type = ID_SUBMODEL;
    ip->idb_meth = &rt_functab[ID_SUBMODEL];
    ip->idb_ptr = bu_malloc(sizeof(struct rt_submodel_internal), "rt_submodel_internal");
    sip = (struct rt_submodel_internal *)ip->idb_ptr;
    sip->magic = RT_SUBMODEL_INTERNAL_MAGIC;
    sip->dbip = dbip;

    if (mat == NULL) mat = bn_mat_identity;
    MAT_COPY(sip->root2leaf, mat);

    bu_vls_init(&str);
    bu_vls_strcpy(&str, rp->ss.ss_args);

    if (bu_struct_parse(&str, rt_submodel_parse, (char *)sip) < 0) {
	bu_vls_free(&str);
    fail:
	bu_free((char *)sip, "rt_submodel_import4: sip");
	ip->idb_type = ID_NULL;
	ip->idb_ptr = (genptr_t)NULL;
	return -2;
    }
    bu_vls_free(&str);

    /* Check for reasonable values */
    if (bu_vls_strlen(&sip->treetop) == 0) {
	bu_log("rt_submodel_import4() treetop= must be specified\n");
	goto fail;
    }

    return 0;			/* OK */
}


/**
 * R T _ S U B M O D E L _ E X P O R T
 *
 * The name is added by the caller, in the usual place.
 */
int
rt_submodel_export4(struct bu_external *ep, const struct rt_db_internal *ip, double local2mm, const struct db_i *dbip)
{
    struct rt_submodel_internal *sip;
    union record *rec;
    struct bu_vls str;

    if (dbip) RT_CK_DBI(dbip);

    RT_CK_DB_INTERNAL(ip);
    if (ip->idb_type != ID_SUBMODEL) return -1;
    sip = (struct rt_submodel_internal *)ip->idb_ptr;
    RT_SUBMODEL_CK_MAGIC(sip);

    /* Ignores scale factor */
    BU_ASSERT(ZERO(local2mm - 1.0));

    BU_CK_EXTERNAL(ep);
    ep->ext_nbytes = sizeof(union record)*DB_SS_NGRAN;
    ep->ext_buf = bu_calloc(1, ep->ext_nbytes, "submodel external");
    rec = (union record *)ep->ext_buf;

    bu_vls_init(&str);
    bu_vls_struct_print(&str, rt_submodel_parse, (char *)sip);

    rec->ss.ss_id = DBID_STRSOL;
    bu_strlcpy(rec->ss.ss_keyword, "submodel", sizeof(rec->ss.ss_keyword));
    bu_strlcpy(rec->ss.ss_args, bu_vls_addr(&str), DB_SS_LEN);
    bu_vls_free(&str);

    return 0;
}


/**
 * R T _ S U B M O D E L _ I M P O R T 5
 *
 * Import an SUBMODEL from the database format to the internal format.
 * Apply modeling transformations as well.
 */
int
rt_submodel_import5(struct rt_db_internal *ip, const struct bu_external *ep, const fastf_t *mat, const struct db_i *dbip)
{
    struct rt_submodel_internal *sip;
    struct bu_vls str;

    if (dbip) RT_CK_DBI(dbip);

    BU_CK_EXTERNAL(ep);
    RT_CK_DBI(dbip);

    RT_CK_DB_INTERNAL(ip);
    ip->idb_major_type = DB5_MAJORTYPE_BRLCAD;
    ip->idb_type = ID_SUBMODEL;
    ip->idb_meth = &rt_functab[ID_SUBMODEL];
    ip->idb_ptr = bu_malloc(sizeof(struct rt_submodel_internal), "rt_submodel_internal");
    sip = (struct rt_submodel_internal *)ip->idb_ptr;
    sip->magic = RT_SUBMODEL_INTERNAL_MAGIC;
    sip->dbip = dbip;

    if (mat == NULL) mat = bn_mat_identity;
    MAT_COPY(sip->root2leaf, mat);

    bu_vls_init(&str);
    bu_vls_strncpy(&str, (const char *)ep->ext_buf, ep->ext_nbytes);

    if (bu_struct_parse(&str, rt_submodel_parse, (char *)sip) < 0) {
	bu_vls_free(&str);
    fail:
	bu_free((char *)sip, "rt_submodel_import4: sip");
	ip->idb_type = ID_NULL;
	ip->idb_ptr = (genptr_t)NULL;
	return -2;
    }
    bu_vls_free(&str);

    /* Check for reasonable values */
    if (bu_vls_strlen(&sip->treetop) == 0) {
	bu_log("rt_submodel_import4() treetop= must be specified\n");
	goto fail;
    }

    return 0;			/* OK */
}


/**
 * R T _ S U B M O D E L _ E X P O R T 5
 *
 * The name is added by the caller, in the usual place.
 */
int
rt_submodel_export5(struct bu_external *ep, const struct rt_db_internal *ip, double local2mm, const struct db_i *dbip)
{
    struct rt_submodel_internal *sip;
    struct bu_vls str;

    if (dbip) RT_CK_DBI(dbip);

    RT_CK_DB_INTERNAL(ip);
    if (ip->idb_type != ID_SUBMODEL) return -1;
    sip = (struct rt_submodel_internal *)ip->idb_ptr;
    RT_SUBMODEL_CK_MAGIC(sip);

    /* Ignores scale factor */
    BU_ASSERT(ZERO(local2mm - 1.0));
    BU_CK_EXTERNAL(ep);

    bu_vls_init(&str);
    bu_vls_struct_print(&str, rt_submodel_parse, (char *)sip);
    ep->ext_nbytes = bu_vls_strlen(&str);
    ep->ext_buf = bu_calloc(1, ep->ext_nbytes, "submodel external");

    bu_strlcpy((char *)ep->ext_buf, bu_vls_addr(&str), ep->ext_nbytes);
    bu_vls_free(&str);

    return 0;
}


/**
 * R T _ S U B M O D E L _ D E S C R I B E
 *
 * Make human-readable formatted presentation of this solid.
 * First line describes type of solid.
 * Additional lines are indented one tab, and give parameter values.
 */
int
rt_submodel_describe(struct bu_vls *str, const struct rt_db_internal *ip, int verbose, double UNUSED(mm2local))
{
    struct rt_submodel_internal *sip = (struct rt_submodel_internal *)ip->idb_ptr;

    RT_SUBMODEL_CK_MAGIC(sip);
    bu_vls_strcat(str, "instanced submodel (SUBMODEL)\n");

    if (!verbose)
	return 0;

    bu_vls_printf(str, "\tfile='%s', treetop='%s', meth=%d\n",
		  bu_vls_addr(&sip->file),
		  bu_vls_addr(&sip->treetop),
		  sip->meth);

    return 0;
}


/**
 * R T _ S U B M O D E L _ I F R E E
 *
 * Free the storage associated with the rt_db_internal version of this solid.
 */
void
rt_submodel_ifree(struct rt_db_internal *ip)
{
    struct rt_submodel_internal *sip;

    RT_CK_DB_INTERNAL(ip);

    sip = (struct rt_submodel_internal *)ip->idb_ptr;
    RT_SUBMODEL_CK_MAGIC(sip);
    sip->magic = 0;			/* sanity */

    bu_free((genptr_t)sip, "submodel ifree");
    ip->idb_ptr = GENPTR_NULL;	/* sanity */
}


/**
 * R T _ S U B M O D E L _ P A R A M S
 *
 */
int
rt_submodel_params(struct pc_pc_set *UNUSED(ps), const struct rt_db_internal *ip)
{
    if (ip) RT_CK_DB_INTERNAL(ip);

    return 0;			/* OK */
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
