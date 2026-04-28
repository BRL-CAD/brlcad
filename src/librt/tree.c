/*                          T R E E . C
 * BRL-CAD
 *
 * Copyright (c) 1995-2026 United States Government as represented by
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


#include "common.h"

#include <stddef.h>
#include <math.h>
#include <string.h>
#include "bio.h"

#include "bu/parallel.h"
#include "vmath.h"
#include "bn.h"
#include "rt/db4.h"
#include "raytrace.h"

#include "./cache.h"
#include "librt_private.h"


#define ACQUIRE_SEMAPHORE_TREE(_hash) switch ((_hash)&03) {	\
	case 0:							\
	    bu_semaphore_acquire(RT_SEM_TREE0);			\
	    break;						\
	case 1:							\
	    bu_semaphore_acquire(RT_SEM_TREE1);			\
	    break;						\
	case 2:							\
	    bu_semaphore_acquire(RT_SEM_TREE2);			\
	    break;						\
	default:						\
	    bu_semaphore_acquire(RT_SEM_TREE3);			\
	    break;						\
    }

#define RELEASE_SEMAPHORE_TREE(_hash) switch ((_hash)&03) {	\
	case 0:							\
	    bu_semaphore_release(RT_SEM_TREE0);			\
	    break;						\
	case 1:							\
	    bu_semaphore_release(RT_SEM_TREE1);			\
	    break;						\
	case 2:							\
	    bu_semaphore_release(RT_SEM_TREE2);			\
	    break;						\
	default:						\
	    bu_semaphore_release(RT_SEM_TREE3);			\
	    break;						\
    }


static void
_rt_tree_region_assign(union tree *tp, const struct region *regionp)
{
    RT_CK_TREE(tp);
    RT_CK_REGION(regionp);
    switch (tp->tr_op) {
	case OP_NOP:
	    return;

	case OP_SOLID:
	    tp->tr_a.tu_regionp = (struct region *)regionp;
	    return;

	case OP_NOT:
	case OP_GUARD:
	case OP_XNOP:
	    tp->tr_b.tb_regionp = (struct region *)regionp;
	    _rt_tree_region_assign(tp->tr_b.tb_left, regionp);
	    return;

	case OP_UNION:
	case OP_INTERSECT:
	case OP_SUBTRACT:
	case OP_XOR:
	    tp->tr_b.tb_regionp = (struct region *)regionp;
	    _rt_tree_region_assign(tp->tr_b.tb_left, regionp);
	    _rt_tree_region_assign(tp->tr_b.tb_right, regionp);
	    return;

	default:
	    bu_bomb("_rt_tree_region_assign: bad op\n");
    }
}


/**
 * This routine must be prepared to run in parallel.
 */
static int
_rt_gettree_region_start(struct db_tree_state *tsp, const struct db_full_path *pathp, const struct rt_comb_internal *combp, void *UNUSED(client_data))
{
    if (tsp) {
	RT_CK_RTI(tsp->ts_rtip);
	if (pathp) RT_CK_FULL_PATH(pathp);
	if (combp) RT_CHECK_COMB(combp);

	/* Ignore "air" regions unless wanted */
	if (tsp->ts_rtip->useair == 0 &&  tsp->ts_aircode != 0) {
	    tsp->ts_rtip->i->rti_air_discards++;
	    return -1;	/* drop this region */
	}
    }
    return 0;
}


struct gettree_data
{
    struct rt_cache *cache;
};


/**
 * This routine will be called by db_walk_tree() once all the solids
 * in this region have been visited.
 *
 * This routine must be prepared to run in parallel.  As a result,
 * note that the details of the solids pointed to by the soltab
 * pointers in the tree may not be filled in when this routine is
 * called (due to the way multiple instances of solids are handled).
 * Therefore, everything which referred to the tree has been moved out
 * into the serial section.  (_rt_tree_region_assign, rt_bound_tree)
 */
static union tree *
_rt_gettree_region_end(struct db_tree_state *tsp, const struct db_full_path *pathp, union tree *curtree, void *UNUSED(client_data))
{
    struct region *rp;
    struct directory *dp = NULL;
    size_t shader_len=0;
    struct rt_i *rtip;
    struct bu_attribute_value_set avs;
    struct bu_attribute_value_pair *avpp;

    RT_CK_DBI(tsp->ts_dbip);
    RT_CK_FULL_PATH(pathp);
    RT_CK_TREE(curtree);
    rtip =  tsp->ts_rtip;
    RT_CK_RTI(rtip);

    if (curtree->tr_op == OP_NOP) {
	/* Ignore empty regions */
	return curtree;
    }

    BU_ALLOC(rp, struct region);
    rp->l.magic = RT_REGION_MAGIC;
    rp->reg_regionid = tsp->ts_regionid;
    rp->reg_is_fastgen = tsp->ts_is_fastgen;
    rp->reg_aircode = tsp->ts_aircode;
    rp->reg_gmater = tsp->ts_gmater;
    rp->reg_los = tsp->ts_los;

    dp = (struct directory *)DB_FULL_PATH_CUR_DIR(pathp);
    if (!dp)
	return TREE_NULL;

    bu_avs_init_empty(&avs);
    if (db5_get_attributes(tsp->ts_dbip, &avs, dp) == 0) {
	/* copy avs */
	bu_avs_init_empty(&(rp->attr_values));
	for (BU_AVS_FOR(avpp, &(tsp->ts_attrs))) {
	    bu_avs_add(&(rp->attr_values), avpp->name, bu_avs_get(&avs, avpp->name));
	}
    }
    bu_avs_free(&avs);

    rp->reg_mater = tsp->ts_mater; /* struct copy */
    if (tsp->ts_mater.ma_shader)
	shader_len = strlen(tsp->ts_mater.ma_shader);
    if (shader_len) {
	rp->reg_mater.ma_shader = bu_strdup(tsp->ts_mater.ma_shader);
    } else
	rp->reg_mater.ma_shader = (char *)NULL;

    rp->reg_name = db_path_to_string(pathp);

    if (RT_G_DEBUG&RT_DEBUG_TREEWALK) {
	bu_log("_rt_gettree_region_end() %s\n", rp->reg_name);
	rt_pr_tree(curtree, 0);
    }

    rp->reg_treetop = curtree;
    rp->reg_all_unions = db_is_tree_all_unions(curtree);

    /* Determine material properties */
    rp->reg_mfuncs = (char *)0;
    rp->reg_udata = (char *)0;
    if (rp->reg_mater.ma_color_valid == 0)
	db_mater_color_region(tsp->ts_dbip, rp);

    /* enter critical section */
    bu_semaphore_acquire(RT_SEM_RESULTS);

    rp->reg_instnum = dp->d_uses++;

    /*
     * Add the region to the linked list of regions.
     * Positions in the region bit vector are established at this time.
     */
    BU_LIST_INSERT(&(rtip->HeadRegion), &rp->l);

    /* Assign bit vector pos. */
    rp->reg_bit = rtip->stats.nregions++;

    /* leave critical section */
    bu_semaphore_release(RT_SEM_RESULTS);

    /* If caller wants to do additional processing, now's the time */
    if (rtip->rti_gettrees_clbk)
	(*rtip->rti_gettrees_clbk)(rtip, tsp, rp);

    if (RT_G_DEBUG & RT_DEBUG_REGIONS) {
	bu_log("Add Region %s instnum %ld\n",
	       rp->reg_name, rp->reg_instnum);
    }

    /* Indicate that we have swiped 'curtree' */
    return TREE_NULL;
}


/**
 * See if solid "dp" as transformed by "mat" already exists in the
 * soltab list.  If it does, return the matching stp, otherwise,
 * create a new soltab structure, enroll it in the list, and return a
 * pointer to that.
 *
 * "mat" will be a null pointer when an identity matrix is signified.
 * This greatly speeds the comparison process.
 *
 * The two cases can be distinguished by the fact that stp->st_id will
 * be 0 for a new soltab structure, and non-zero for an existing one.
 *
 * This routine will run in parallel.
 *
 * In order to avoid a race between searching the soltab list and
 * adding new solids to it, the new solid to be added *must* be
 * enrolled in the list before exiting the critical section.
 *
 * To limit the size of the list to be searched, there are many lists.
 * The selection of which list is determined by the hash value
 * computed from the solid's name.  This is the same optimization used
 * in searching the directory lists.
 *
 * This subroutine is the critical bottleneck in parallel tree walking.
 *
 * It is safe, and much faster, to use several different critical
 * sections when searching different lists.
 *
 * There are only 4 dedicated semaphores defined, TREE0 through TREE3.
 * This unfortunately limits the code to having only 4 CPUs doing list
 * searching at any one time.  Hopefully, this is enough parallelism
 * to keep the rest of the CPUs doing I/O and actual solid prepping.
 *
 * Since the algorithm has been reduced from an O((nsolid/128)**2)
 * search on the entire rti_solidheads[hash] list to an O(ninstance)
 * search on the dp->d_use_head list for this one solid, the critical
 * section should be relatively short-lived.  Having the 3-way split
 * should provide ample opportunity for parallelism through here,
 * while still ensuring that the necessary variables are protected.
 *
 * There are two critical variables which *both* need to be protected:
 * the specific rti_solidhead[hash] list head, and the specific
 * dp->d_use_hd list head.  Fortunately, since the selection of
 * critical section is based upon db_dirhash(dp->d_namep), any other
 * processor that wants to search this same 'dp' will get the same
 * hash as the current thread, and will thus wait for the appropriate
 * semaphore to be released.  Similarly, any other thread that wants
 * to search the same rti_solidhead[hash] list as the current thread
 * will be using the same hash, and will thus wait for the proper
 * semaphore.
 */
static struct soltab *
_rt_find_identical_solid(const matp_t mat, struct directory *dp, struct rt_i *rtip)
{
    struct soltab *stp = RT_SOLTAB_NULL;
    int hash;

    RT_CK_DIR(dp);
    RT_CK_RTI(rtip);

    hash = db_dirhash(dp->d_namep);

    /* Enter the appropriate dual critical-section */
    ACQUIRE_SEMAPHORE_TREE(hash);

    /*
     * If solid has not been referenced yet, the search can be
     * skipped.  If solid is being referenced a _lot_, it certainly
     * isn't all going to be in the same place, so don't bother
     * searching.  Consider the case of a million instances of the
     * same tree submodel solid.
     */
    if (dp->d_uses > 0 && dp->d_uses < 100 &&
	rtip->rti_dont_instance == 0
	) {
	struct bu_list *mid;

	/* Search dp->d_use_hd list for other instances */
	for (BU_LIST_FOR(mid, bu_list, &dp->d_use_hd)) {

	    stp = BU_LIST_MAIN_PTR(soltab, mid, l2);
	    RT_CK_SOLTAB(stp);

	    if (stp->st_matp == (matp_t)0) {
		if (mat == (matp_t)0) {
		    /* Both have identity matrix */
		    goto more_checks;
		}
		continue;
	    }
	    if (mat == (matp_t)0) continue;	/* doesn't match */

	    if (!bn_mat_is_equal(mat, stp->st_matp, &rtip->rti_tol))
		continue;

	more_checks:
	    /* Don't instance this solid from some other model
	     * instance.  As this is nearly always equal, check it
	     * last
	     */
	    if (stp->st_rtip != rtip) continue;

	    /*
	     * stp now points to re-referenced solid.  stp->st_id is
	     * non-zero, indicating pre-existing solid.
	     */
	    RT_CK_SOLTAB(stp);		/* sanity */

	    /* Only increment use counter for non-dead solids. */
	    if (!(stp->st_aradius <= -1))
		stp->st_uses++;
	    /* dp->d_uses is NOT incremented, because number of
	     * soltab's using it has not gone up.
	     */
	    if (RT_G_DEBUG & RT_DEBUG_SOLIDS) {
		bu_log(mat ?
		       "%s re-referenced %ld\n" :
		       "%s re-referenced %ld (identity mat)\n",
		       dp->d_namep, stp->st_uses);
	    }

	    /* Leave the appropriate dual critical-section */
	    RELEASE_SEMAPHORE_TREE(hash);
	    return stp;
	}
    }

    /*
     * Create and link a new solid into the list.
     *
     * Ensure the search keys "dp", "st_mat" and "st_rtip" are stored
     * now, while still inside the critical section, because they are
     * searched on, above.
     */
    BU_ALLOC(stp, struct soltab);
    stp->l.magic = RT_SOLTAB_MAGIC;
    stp->l2.magic = RT_SOLTAB2_MAGIC;
    stp->st_rtip = rtip;
    stp->st_dp = dp;
    dp->d_uses++;
    stp->st_uses = 1;
    /* stp->st_id is intentionally left zero here, as a flag */

    if (mat) {
	stp->st_matp = (matp_t)bu_malloc(sizeof(mat_t), "st_matp");
	MAT_COPY(stp->st_matp, mat);
    } else {
	stp->st_matp = (matp_t)0;
    }

    /* Add to the appropriate soltab list head */
    /* PARALLEL NOTE:  Uses critical section on rt_solidheads element */
    BU_LIST_INSERT(&(rtip->i->rti_solidheads[hash]), &(stp->l));

    /* Also add to the directory structure list head */
    /* PARALLEL NOTE:  Uses critical section on this 'dp' */
    BU_LIST_INSERT(&dp->d_use_hd, &(stp->l2));

    /*
     * Leave the 4-way critical-section protecting dp and [hash]
     */
    RELEASE_SEMAPHORE_TREE(hash);

    /* Enter an exclusive critical section to protect nsolids.
     * nsolids++ needs to be locked to a SINGLE thread
     */
    bu_semaphore_acquire(BU_SEM_GENERAL);
    stp->st_bit = rtip->stats.nsolids++;
    bu_semaphore_release(BU_SEM_GENERAL);

    /*
     * Fill in the last little bit of the structure in full parallel
     * mode, outside of any critical section.
     */

    /* Init tables of regions using this solid.  Usually small. */
    bu_ptbl_init(&stp->st_regions, 7, "st_regions ptbl");

    return stp;
}


/**
 * This routine must be prepared to run in parallel.
 */
static union tree *
_rt_gettree_leaf(struct db_tree_state *tsp, const struct db_full_path *pathp, struct rt_db_internal *ip, void *client_data)
{
    struct gettree_data *data;
    struct soltab *stp;
    struct directory *dp;
    matp_t mat;
    union tree *curtree;
    struct rt_i *rtip;
    int ret;
    int i;

    RT_CK_DBTS(tsp);
    RT_CK_DBI(tsp->ts_dbip);
    RT_CK_FULL_PATH(pathp);
    RT_CK_DB_INTERNAL(ip);
    rtip = tsp->ts_rtip;
    RT_CK_RTI(rtip);
    dp = DB_FULL_PATH_CUR_DIR(pathp);

    data = (struct gettree_data *)client_data;

    if (!data)
	bu_bomb("missing argument");

    if (!dp)
	return TREE_NULL;

    /* Determine if this matrix is an identity matrix */

    if (!bn_mat_is_equal(tsp->ts_mat, bn_mat_identity, &rtip->rti_tol)) {
	/* Not identity matrix */
	mat = (matp_t)tsp->ts_mat;
    } else {
	/* 0 == identity matrix */
	mat = (matp_t)0;
    }

    /*
     * Check to see if this exact solid has already been processed.
     * Match on leaf name and matrix.  Note that there is a race here
     * between having st_id filled in a few lines below (which is
     * necessary for calling ft_prep), and ft_prep filling in
     * st_aradius.  Fortunately, st_aradius starts out as zero, and
     * will never go down to -1 unless this soltab structure has
     * become a dead solid, so by testing against -1 (instead of <= 0,
     * like before, oops), it isn't a problem.
     */
    stp = _rt_find_identical_solid(mat, dp, rtip);
    if (stp->st_id != 0) {
	/* stp is an instance of a pre-existing solid */
	if (stp->st_aradius <= -1) {
	    /* It's dead, Jim.  st_uses was not incremented. */
	    return TREE_NULL;	/* BAD: instance of dead solid */
	}
	goto found_it;
    }

    if (rtip->i->rti_add_to_new_solids_list) {
	bu_ptbl_ins(&rtip->i->rti_new_solids, (long *)stp);
    }

    stp->st_id = ip->idb_type;
    stp->st_meth = &OBJ[ip->idb_type];

    RT_CK_DB_INTERNAL(ip);

    /* init solid's maxima and minima */
    VSETALL(stp->st_max, -INFINITY);
    VSETALL(stp->st_min,  INFINITY);

    /*
     * If prep wants to keep the internal structure, that is OK, as
     * long as idb_ptr is set to null.  Note that the prep routine may
     * have changed st_id.
     */
    if (rtip->rti_dbip->i->dbi_version > 4) {
	ret = rt_cache_prep(data->cache, stp, ip);
    } else {
	ret = rt_obj_prep(stp, ip, stp->st_rtip);
    }
    if (ret) {
	int hash;
	/* Error, solid no good */
	bu_log("_rt_gettree_leaf(%s):  prep failure\n", dp->d_namep);
	/* Too late to delete soltab entry; mark it as "dead" */
	hash = db_dirhash(dp->d_namep);
	ACQUIRE_SEMAPHORE_TREE(hash);
	stp->st_aradius = -1;
	stp->st_uses--;
	RELEASE_SEMAPHORE_TREE(hash);
	return TREE_NULL;		/* BAD */
    }

    if (rtip->rti_dont_instance) {
	/*
	 * If instanced solid refs are not being compressed, then
	 * memory isn't an issue, and the application (such as
	 * solids_on_ray) probably cares about the full path of this
	 * solid, from root to leaf.  So make it available here.
	 * (stp->st_dp->d_uses could be the way to discriminate
	 * references uniquely, if the path isn't enough.  To locate
	 * given dp and d_uses, search dp->d_use_hd list.  Question
	 * is, where to stash current val of d_uses?)
	 */
	db_full_path_init(&stp->st_path);
	db_dup_full_path(&stp->st_path, pathp);
    } else {
	/*
	 * If there is more than just a direct reference to this leaf
	 * from its containing region, copy that below-region path
	 * into st_path.  Otherwise, leave st_path's magic number 0.
	 *
	 * XXX nothing depends on this behavior yet, and this whole
	 * XXX 'else' clause might well be deleted. -Mike
	 */
	i = pathp->fp_len-1;
	if (i > 0 && !(pathp->fp_names[i-1]->d_flags & RT_DIR_REGION)) {
	    /* Search backwards for region.  If no region, use whole path */
	    for (--i; i > 0; i--) {
		if (pathp->fp_names[i-1]->d_flags & RT_DIR_REGION) break;
	    }
	    if (i < 0) i = 0;
	    db_full_path_init(&stp->st_path);
	    db_dup_path_tail(&stp->st_path, pathp, i);
	}
    }
    if (RT_G_DEBUG&RT_DEBUG_TREEWALK && stp->st_path.magic == DB_FULL_PATH_MAGIC) {
	char *sofar = db_path_to_string(&stp->st_path);
	bu_log("_rt_gettree_leaf() st_path=%s\n", sofar);
	bu_free(sofar, "path string");
    }

    if (RT_G_DEBUG&RT_DEBUG_SOLIDS) {
	struct bu_vls str = BU_VLS_INIT_ZERO;
	bu_log("\n---Primitive %ld: %s\n", stp->st_bit, dp->d_namep);

	/* verbose=1, mm2local=1.0 */
	ret = -1;
	if (stp->st_meth->ft_describe) {
	    ret = stp->st_meth->ft_describe(&str, ip, 1, 1.0);
	}
	if (ret < 0) {
	    bu_log("_rt_gettree_leaf(%s):  solid describe failure\n",
		   dp->d_namep);
	}
	bu_log("%s:  %s", dp->d_namep, bu_vls_addr(&str));
	bu_vls_free(&str);
    }

found_it:
    BU_GET(curtree, union tree);
    RT_TREE_INIT(curtree);
    curtree->tr_op = OP_SOLID;
    curtree->tr_a.tu_stp = stp;
    /* regionp will be filled in later by _rt_tree_region_assign() */
    curtree->tr_a.tu_regionp = (struct region *)0;

    if (RT_G_DEBUG&RT_DEBUG_TREEWALK) {
	char *sofar = db_path_to_string(pathp);
	bu_log("_rt_gettree_leaf() %s\n", sofar);
	bu_free(sofar, "path string");
    }

    return curtree;
}


void
rt_free_soltab(struct soltab *stp)
{
    int hash = 0;

    RT_CK_SOLTAB(stp);
    if (stp->st_id < 0)
	bu_bomb("rt_free_soltab:  bad st_id");

    if (stp->st_dp && stp->st_dp->d_namep)
	hash = db_dirhash(stp->st_dp->d_namep);

    ACQUIRE_SEMAPHORE_TREE(hash);	/* start critical section */
    if (--(stp->st_uses) > 0) {
	RELEASE_SEMAPHORE_TREE(hash);
	return;
    }
    BU_LIST_DEQUEUE(&(stp->l2));	/* remove from st_dp->d_use_hd list */
    BU_LIST_DEQUEUE(&(stp->l));		/* uses rti_solidheads[] */

    RELEASE_SEMAPHORE_TREE(hash);	/* end critical section */

    if (stp->st_aradius > 0) {
	if (stp->st_meth->ft_free)
	    stp->st_meth->ft_free(stp);
	stp->st_aradius = 0;
    }
    if (stp->st_matp) bu_free((char *)stp->st_matp, "st_matp");
    stp->st_matp = (matp_t)0;	/* Sanity */

    bu_ptbl_free(&stp->st_regions);

    stp->st_dp = RT_DIR_NULL;	/* Sanity */

    if (stp->st_path.magic) {
	RT_CK_FULL_PATH(&stp->st_path);
	db_free_full_path(&stp->st_path);
    }

    bu_free((char *)stp, "struct soltab");
}


/**
 * Convert any references to "dead" solids into NOP nodes.
 */
void
_rt_tree_kill_dead_solid_refs(union tree *tp)
{

    RT_CK_TREE(tp);

    switch (tp->tr_op) {

	case OP_SOLID:
	    {
		struct soltab *stp;

		stp = tp->tr_a.tu_stp;
		RT_CK_SOLTAB(stp);
		if (stp->st_aradius <= 0) {
		    if (RT_G_DEBUG&RT_DEBUG_TREEWALK)
			bu_log("encountered dead solid '%s' stp=%p, tp=%p\n",
			       stp->st_dp->d_namep, (void *)stp, (void *)tp);
		    rt_free_soltab(stp);
		    tp->tr_a.tu_stp = SOLTAB_NULL;
		    tp->tr_op = OP_NOP;	/* Convert to NOP */
		}
		return;
	    }

	default:
	    bu_log("_rt_tree_kill_dead_solid_refs(%p): unknown op=%x\n",
		   (void *)tp, tp->tr_op);
	    return;

	case OP_XOR:
	case OP_UNION:
	case OP_INTERSECT:
	case OP_SUBTRACT:
	    /* BINARY */
	    _rt_tree_kill_dead_solid_refs(tp->tr_b.tb_left);
	    _rt_tree_kill_dead_solid_refs(tp->tr_b.tb_right);
	    break;
	case OP_NOT:
	case OP_GUARD:
	case OP_XNOP:
	    /* UNARY tree -- for completeness only, should never be seen */
	    _rt_tree_kill_dead_solid_refs(tp->tr_b.tb_left);
	    break;
	case OP_NOP:
	    /* This sub-tree has nothing further in it */
	    return;
    }
    return;
}


/**
 * Prune a subtractor (right-hand side of OP_SUBTRACT) tree by
 * eliminating branches whose AABB is entirely outside the constraint
 * box [cmin, cmax].
 *
 * Descends into OP_UNION and OP_XOR nodes to prune individual members.
 * All other operators (OP_SUBTRACT, OP_INTERSECT, etc.) are treated
 * opaquely: if the whole subtree bbox is disjoint it is pruned in one
 * shot, otherwise it is left unchanged (conservative).
 *
 * Returns the pruned tree pointer, or TREE_NULL when the entire
 * subtree has been freed.  The caller MUST store the return value back
 * into the parent's child slot.
 *
 * Memory contract: pruned nodes are freed via db_free_tree() which
 * also decrements soltab use-counts; un-pruned nodes are returned
 * unchanged.
 */
static union tree *
rt_tree_prune_subtractor(union tree *tp,
			 const vect_t cmin, const vect_t cmax)
{
    vect_t tp_min, tp_max;

    if (!tp)
	return TREE_NULL;
    RT_CK_TREE(tp);

    /* Compute the bounding box of this subtree.
     * Initialise with reversed infinities – the standard BRL-CAD VMIN/VMAX
     * accumulation pattern used by rt_bound_tree throughout bbox.c. */
    VSETALL(tp_min, INFINITY);
    VSETALL(tp_max, -INFINITY);
    if (rt_bound_tree(tp, tp_min, tp_max) < 0)
	return tp;		/* bound failed – be conservative */

    /* Never prune subtrees with infinite bounds (halfspaces, etc.). */
    if (tp_max[X] >= INFINITY || tp_max[Y] >= INFINITY || tp_max[Z] >= INFINITY)
	return tp;

    /* If this subtree bbox is entirely outside the constraint box,
     * the subtractor cannot contribute anything here – prune it. */
    if (tp_min[X] >= cmax[X] || tp_max[X] <= cmin[X] ||
	tp_min[Y] >= cmax[Y] || tp_max[Y] <= cmin[Y] ||
	tp_min[Z] >= cmax[Z] || tp_max[Z] <= cmin[Z]) {
	if (RT_G_DEBUG & RT_DEBUG_TREEWALK)
	    bu_log("rt_tree_prune_subtractor: pruned disjoint subtractor branch\n");
	db_free_tree(tp);
	return TREE_NULL;
    }

    /* Subtree bbox overlaps constraint; descend into UNION/XOR to find
     * individual members that can still be pruned. */
    switch (tp->tr_op) {

	case OP_SOLID:
	case OP_NOP:
	    return tp;		/* leaf that overlaps – keep it */

	case OP_UNION:
	case OP_XOR: {
	    union tree *left, *right;
	    left  = rt_tree_prune_subtractor(tp->tr_b.tb_left,  cmin, cmax);
	    right = rt_tree_prune_subtractor(tp->tr_b.tb_right, cmin, cmax);
	    if (!left && !right) {
		BU_PUT(tp, union tree);
		return TREE_NULL;
	    } else if (!left) {
		BU_PUT(tp, union tree);
		return right;
	    } else if (!right) {
		BU_PUT(tp, union tree);
		return left;
	    }
	    tp->tr_b.tb_left  = left;
	    tp->tr_b.tb_right = right;
	    return tp;
	}

	default:
	    /* OP_SUBTRACT, OP_INTERSECT, OP_NOT, etc. inside a subtractor:
	     * the whole-subtree bbox check above already handles the
	     * completely-disjoint case; leave complex sub-expressions alone. */
	    return tp;
    }
}


/**
 * Prep-time CSG tree shaker.
 *
 * Walks the boolean tree and, for each OP_SUBTRACT node, computes the
 * bounding box of the minuend (left child) and prunes subtractor (right
 * child) branches that are provably outside that box via AABB disjointness.
 *
 * The pass is conservative:
 *  - Minuend bboxes that are infinite or degenerate (min > max) are
 *    skipped entirely.
 *  - Only subtractor branches that are AABB-disjoint from the minuend
 *    are removed; uncertain cases are left unchanged.
 *  - When an entire subtractor is pruned, the OP_SUBTRACT node is
 *    replaced in-place by its left child (SUBTRACT(L, 0) == L).
 *
 * Returns the (possibly simplified) tree pointer that the caller MUST
 * store back into the parent's child slot or into regp->reg_treetop.
 */
static union tree *
rt_tree_shake_subs(union tree *tp)
{
    vect_t left_min, left_max;
    union tree *pruned;

    if (!tp)
	return TREE_NULL;
    RT_CK_TREE(tp);

    switch (tp->tr_op) {

	case OP_SOLID:
	case OP_NOP:
	    return tp;

	case OP_SUBTRACT:
	    /* Recursively shake children first. */
	    if (tp->tr_b.tb_left)
		tp->tr_b.tb_left  = rt_tree_shake_subs(tp->tr_b.tb_left);
	    if (tp->tr_b.tb_right)
		tp->tr_b.tb_right = rt_tree_shake_subs(tp->tr_b.tb_right);

	    if (!tp->tr_b.tb_left) {
		/* Left side vanished – whole subtraction is empty. */
		if (tp->tr_b.tb_right)
		    db_free_tree(tp->tr_b.tb_right);
		BU_PUT(tp, union tree);
		return TREE_NULL;
	    }
	    if (!tp->tr_b.tb_right) {
		/* Right side vanished: SUBTRACT(L, 0) = L. */
		union tree *keep = tp->tr_b.tb_left;
		BU_PUT(tp, union tree);
		return keep;
	    }

	    /* Compute the minuend (left child) bounding box.
	     * Reversed-infinity initialisation – standard BRL-CAD VMIN/VMAX
	     * accumulation pattern; a return of (min > max) means empty. */
	    VSETALL(left_min, INFINITY);
	    VSETALL(left_max, -INFINITY);
	    if (rt_bound_tree(tp->tr_b.tb_left, left_min, left_max) < 0)
		return tp;	/* bound failed – be conservative */

	    /* Skip if the minuend has infinite or degenerate bounds. */
	    if (left_max[X] >= INFINITY || left_max[Y] >= INFINITY || left_max[Z] >= INFINITY)
		return tp;
	    if (left_min[X] > left_max[X])
		return tp;	/* degenerate / empty minuend */

	    /* Prune the subtractor using the minuend bbox as constraint. */
	    pruned = rt_tree_prune_subtractor(
		tp->tr_b.tb_right, left_min, left_max);

	    if (!pruned) {
		/* Entire subtractor eliminated: SUBTRACT(L, 0) = L. */
		union tree *keep = tp->tr_b.tb_left;
		BU_PUT(tp, union tree);
		if (RT_G_DEBUG & RT_DEBUG_TREEWALK)
		    bu_log("rt_tree_shake_subs: subtractor fully pruned\n");
		return keep;
	    }
	    tp->tr_b.tb_right = pruned;
	    return tp;

	case OP_UNION:
	case OP_INTERSECT:
	case OP_XOR:
	    if (tp->tr_b.tb_left)
		tp->tr_b.tb_left  = rt_tree_shake_subs(tp->tr_b.tb_left);
	    if (tp->tr_b.tb_right)
		tp->tr_b.tb_right = rt_tree_shake_subs(tp->tr_b.tb_right);
	    return tp;

	case OP_NOT:
	case OP_GUARD:
	case OP_XNOP:
	    /* Unary operators. */
	    if (tp->tr_b.tb_left)
		tp->tr_b.tb_left  = rt_tree_shake_subs(tp->tr_b.tb_left);
	    return tp;

	default:
	    return tp;
    }
}


int
rt_gettrees_and_attrs(struct rt_i *rtip, const char **attrs, int argc, const char **argv, int ncpus)
{
    struct soltab *stp;
    struct region *regp;

    size_t prev_sol_count;
    int ret = 0;
    int num_attrs=0;
    point_t region_min, region_max;

    RT_CHECK_RTI(rtip);
    RT_CK_DBI(rtip->rti_dbip);

    if (!rtip->needprep) {
	bu_log("ERROR: rt_gettree() called again after rt_prep!\n");
	return -1;		/* FAIL */
    }

    if (argc <= 0)
	return -1;	/* FAIL */

    prev_sol_count = rtip->stats.nsolids;

    {
	struct gettree_data data;
	struct db_tree_state tree_state;

	RT_DBTS_INIT(&tree_state);
	tree_state.ts_dbip = rtip->rti_dbip;
	tree_state.ts_rtip = rtip;

	if (attrs) {
	    if (db_version(rtip->rti_dbip) < 5) {
		bu_log("WARNING: requesting attributes from an old database version (ignored)\n");
		bu_avs_init_empty(&tree_state.ts_attrs);
	    } else {
		while (attrs[num_attrs]) {
		    num_attrs++;
		}
		if (num_attrs) {
		    bu_avs_init(&tree_state.ts_attrs,
				num_attrs,
				"avs in tree_state");
		    num_attrs = 0;
		    while (attrs[num_attrs]) {
			bu_avs_add(&tree_state.ts_attrs,
				   attrs[num_attrs],
				   NULL);
			num_attrs++;
		    }
		} else {
		    bu_avs_init_empty(&tree_state.ts_attrs);
		}
	    }
	} else {
	    bu_avs_init_empty(&tree_state.ts_attrs);
	}

	if (rtip->rti_dbip->i->dbi_version > 4) {
	    data.cache = rt_cache_open();
	}

	if (UNLIKELY(rtip->rti_dbip->i->dbi_use_comb_instance_ids)) {
	    struct bu_ptbl pos_paths = BU_PTBL_INIT_ZERO;
	    for (int i = 0; i < argc; i++) {
		struct db_full_path ifp;
		db_full_path_init(&ifp);
		db_string_to_path(&ifp, rtip->rti_dbip, argv[i]);
		if (db_fp_op(&ifp, rtip->rti_dbip, 0) == OP_UNION) {
		    bu_ptbl_ins(&pos_paths, (long *)argv[i]);
		}
		db_free_full_path(&ifp);
	    }
	    int ac = (int)BU_PTBL_LEN(&pos_paths);
	    const char **av = (const char **)bu_calloc(BU_PTBL_LEN(&pos_paths)+1, sizeof(const char *), "av");
	    for (size_t i = 0; i < BU_PTBL_LEN(&pos_paths); i++) {
		av[i] = (const char *)BU_PTBL_GET(&pos_paths, i);
	    }
	    bu_ptbl_free(&pos_paths);

	    ret = db_walk_tree(rtip->rti_dbip, ac, av, ncpus,
		    &tree_state,
		    _rt_gettree_region_start,
		    _rt_gettree_region_end,
		    _rt_gettree_leaf, (void *)&data);
	    bu_avs_free(&tree_state.ts_attrs);
	    bu_free(av, "av");
	} else {
	    ret = db_walk_tree(rtip->rti_dbip, argc, argv, ncpus,
		    &tree_state,
		    _rt_gettree_region_start,
		    _rt_gettree_region_end,
		    _rt_gettree_leaf, (void *)&data);
	    bu_avs_free(&tree_state.ts_attrs);
	}

	if (rtip->rti_dbip->i->dbi_version > 4) {
	    rt_cache_close(data.cache);
	}
    }

    /* DEBUG:  Ensure that all region trees are valid */
    for (BU_LIST_FOR(regp, region, &(rtip->HeadRegion))) {
	RT_CK_REGION(regp);
	db_ck_tree(regp->reg_treetop);
    }

    /*
     * Eliminate any "dead" solids that parallel code couldn't change.
     * First remove any references from the region tree, then remove
     * actual soltab structs from the soltab list.
     */
    for (BU_LIST_FOR(regp, region, &(rtip->HeadRegion))) {
	RT_CK_REGION(regp);
	_rt_tree_kill_dead_solid_refs(regp->reg_treetop);
	(void)rt_tree_elim_nops(regp->reg_treetop);
    }
again:
    RT_VISIT_ALL_SOLTABS_START(stp, rtip) {
	RT_CK_SOLTAB(stp);
	if (stp->st_aradius <= 0) {
	    bu_log("rt_gettrees() cleaning up dead solid '%s'\n",
		   stp->st_dp->d_namep);
	    rt_free_soltab(stp);
	    /* Can't do rtip->stats.nsolids--, that doubles as max bit number! */
	    /* The macro makes it hard to regain place, punt */
	    goto again;
	}
    } RT_VISIT_ALL_SOLTABS_END;

    /*
     * Another pass, no restarting.  Assign "piecestate" indices
     * for those solids which contain pieces.
     */
    RT_VISIT_ALL_SOLTABS_START(stp, rtip) {
	if (stp->st_npieces > 1) {
	    /* all pieces must be within model bounding box for pieces
	     * to work correctly.
	     */
	    VMINMAX(rtip->mdl_min, rtip->mdl_max, stp->st_min);
	    VMINMAX(rtip->mdl_min, rtip->mdl_max, stp->st_max);
	    stp->st_piecestate_num = rtip->i->rti_nsolids_with_pieces++;
	}
	if (RT_G_DEBUG&RT_DEBUG_SOLIDS)
	    rt_pr_soltab(stp);
    } RT_VISIT_ALL_SOLTABS_END;

    /*
     * Tree shaker: for each region, eliminate OP_SUBTRACT branches
     * whose subtractor bounding box is provably AABB-disjoint from
     * the minuend bounding box.  Runs in the serial section after
     * dead-solid cleanup (so all soltab bboxes are valid) and before
     * the region-assign / bounds pass (so the reduced trees are used
     * for computing model extents).
     */
    for (BU_LIST_FOR(regp, region, &(rtip->HeadRegion))) {
	RT_CK_REGION(regp);
	if (!regp->reg_treetop)
	    continue;
	/* Always store the result: if the shaker returns TREE_NULL
	 * (entire tree pruned, which shouldn't happen for well-formed
	 * regions but is handled defensively), we must update
	 * reg_treetop rather than leaving a dangling pointer. */
	regp->reg_treetop = rt_tree_shake_subs(regp->reg_treetop);
    }

    /* Handle finishing touches on the trees that needed soltab
     * structs that the parallel code couldn't look at yet.
     */
    for (BU_LIST_FOR(regp, region, &(rtip->HeadRegion))) {
	RT_CK_REGION(regp);

	/* Skip regions whose tree was entirely pruned (defensive). */
	if (!regp->reg_treetop)
	    continue;

	/* The region and the entire tree are cross-referenced */
	_rt_tree_region_assign(regp->reg_treetop, regp);

	/*
	 * Find region RPP, and update the model maxima and
	 * minima.
	 *
	 * Don't update min & max for halfspaces; instead, add
	 * them to the list of infinite solids, for special
	 * handling.
	 */
	if (rt_bound_tree(regp->reg_treetop, region_min, region_max) < 0) {
	    bu_log("rt_gettrees() %s\n", regp->reg_name);
	    bu_bomb("rt_gettrees(): rt_bound_tree() fail\n");
	}
	if (region_max[X] < INFINITY) {
	    /* infinite regions are exempted from this */
	    VMINMAX(rtip->mdl_min, rtip->mdl_max, region_min);
	    VMINMAX(rtip->mdl_min, rtip->mdl_max, region_max);
	}
    }

    /* DEBUG:  Ensure that all region trees are valid */
    for (BU_LIST_FOR(regp, region, &(rtip->HeadRegion))) {
	RT_CK_REGION(regp);
	db_ck_tree(regp->reg_treetop);
    }

    if (ret < 0)
	return ret;

    if (rtip->stats.nsolids <= prev_sol_count)
	bu_log("rt_gettrees(%s) warning:  no primitives found\n", argv[0]);
    return ret;	/* OK */
}

int
rt_gettree(struct rt_i *rtip, const char *node)
{
    int rv;
    const char *argv[2];

    argv[0] = node;
    argv[1] = (const char *)NULL;

    rv =  rt_gettrees_and_attrs(rtip, NULL, 1, argv, 1);

    if (rv == 0 || rv == -2) {
	return 0;
    } else {
	return -1;
    }
}


int
rt_gettrees(struct rt_i *rtip, int argc, const char **argv, int ncpus)
{
    int rv;
    rv = rt_gettrees_and_attrs(rtip, NULL, argc, argv, ncpus);

    if (rv == 0 || rv == -2) {
	return 0;
    } else {
	return -1;
    }
}


int
rt_tree_elim_nops(union tree *tp)
{
    union tree *left, *right;

top:
    RT_CK_TREE(tp);

    switch (tp->tr_op) {

	case OP_SOLID:
	    return 0;		/* Retain */

	default:
	    bu_log("rt_tree_elim_nops(%p): unknown op=%x\n",
		   (void *)tp, tp->tr_op);
	    return -1;

	case OP_XOR:
	case OP_UNION:
	    /* BINARY type -- rewrite tp as surviving side */
	    left = tp->tr_b.tb_left;
	    right = tp->tr_b.tb_right;
	    if (rt_tree_elim_nops(left) < 0) {
		*tp = *right;	/* struct copy */
		BU_PUT(left, union tree);
		BU_PUT(right, union tree);
		goto top;
	    }
	    if (rt_tree_elim_nops(right) < 0) {
		*tp = *left;	/* struct copy */
		BU_PUT(left, union tree);
		BU_PUT(right, union tree);
		goto top;
	    }
	    break;
	case OP_INTERSECT:
	    /* BINARY type -- if either side fails, nuke subtree */
	    left = tp->tr_b.tb_left;
	    right = tp->tr_b.tb_right;
	    if (rt_tree_elim_nops(left) < 0 ||
		rt_tree_elim_nops(right) < 0) {
		db_free_tree(left);
		db_free_tree(right);
		tp->tr_op = OP_NOP;
		return -1;	/* eliminate reference to tp */
	    }
	    break;
	case OP_SUBTRACT:
	    /* BINARY type -- if right fails, rewrite (X - 0 = X).
	     * If left fails, nuke entire subtree (0 - X = 0).
	     */
	    left = tp->tr_b.tb_left;
	    right = tp->tr_b.tb_right;
	    if (rt_tree_elim_nops(left) < 0) {
		db_free_tree(left);
		db_free_tree(right);
		tp->tr_op = OP_NOP;
		return -1;	/* eliminate reference to tp */
	    }
	    if (rt_tree_elim_nops(right) < 0) {
		*tp = *left;	/* struct copy */
		BU_PUT(left, union tree);
		BU_PUT(right, union tree);
		goto top;
	    }
	    break;
	case OP_NOT:
	case OP_GUARD:
	case OP_XNOP:
	    /* UNARY tree -- for completeness only, should never be seen */
	    left = tp->tr_b.tb_left;
	    if (rt_tree_elim_nops(left) < 0) {
		BU_PUT(left, union tree);
		tp->tr_op = OP_NOP;
		return -1;	/* Kill ref to unary op, too */
	    }
	    break;
	case OP_NOP:
	    /* Implies that this tree has nothing in it */
	    return -1;		/* Kill ref to this NOP */
    }
    return 0;
}


struct soltab *
rt_find_solid(const struct rt_i *rtip, const char *name)
{
    struct soltab *stp;
    struct directory *dp;

    RT_CHECK_RTI(rtip);
    dp = db_lookup((struct db_i *)rtip->rti_dbip, (char *)name, LOOKUP_QUIET);
    if (dp == RT_DIR_NULL)
	return RT_SOLTAB_NULL;

    RT_VISIT_ALL_SOLTABS_START(stp, (struct rt_i *)rtip) {
	if (stp->st_dp == dp) {
	    return stp;
	}
    } RT_VISIT_ALL_SOLTABS_END;

    return RT_SOLTAB_NULL;
}


void
rt_optim_tree(union tree *tp, struct resource *resp)
{
    union tree **sp;
    union tree *low;
    union tree **stackend;

    RT_CK_TREE(tp);
    while ((sp = resp->re_boolstack) == (union tree **)0)
	_bool_growstack(resp);
    stackend = &(resp->re_boolstack[resp->re_boolslen-1]);
    *sp++ = TREE_NULL;
    *sp++ = tp;
    while ((tp = *--sp) != TREE_NULL) {
	switch (tp->tr_op) {
	    case OP_NOP:
		/* XXX Consider eliminating nodes of form (A op NOP) */
		/* XXX Needs to be detected in previous iteration */
		break;
	    case OP_SOLID:
		break;
	    case OP_SUBTRACT:
		while ((low=tp->tr_b.tb_left)->tr_op == OP_SUBTRACT) {
		    /* Rewrite X - A - B as X - (A union B) */
		    tp->tr_b.tb_left = low->tr_b.tb_left;
		    low->tr_op = OP_UNION;
		    low->tr_b.tb_left = low->tr_b.tb_right;
		    low->tr_b.tb_right = tp->tr_b.tb_right;
		    tp->tr_b.tb_right = low;
		}
		/* push both nodes - search left first */
		*sp++ = tp->tr_b.tb_right;
		*sp++ = tp->tr_b.tb_left;
		if (sp >= stackend) {
		    int off = sp - resp->re_boolstack;
		    _bool_growstack(resp);
		    sp = &(resp->re_boolstack[off]);
		    stackend = &(resp->re_boolstack[resp->re_boolslen-1]);
		}
		break;
	    case OP_UNION:
	    case OP_INTERSECT:
	    case OP_XOR:
		/* Need to look at 3-level optimizations here, both sides */
		/* push both nodes - search left first */
		*sp++ = tp->tr_b.tb_right;
		*sp++ = tp->tr_b.tb_left;
		if (sp >= stackend) {
		    int off = sp - resp->re_boolstack;
		    _bool_growstack(resp);
		    sp = &(resp->re_boolstack[off]);
		    stackend = &(resp->re_boolstack[resp->re_boolslen-1]);
		}
		break;
	    default:
		bu_log("rt_optim_tree: bad op x%x\n", tp->tr_op);
		break;
	}
    }
}


#ifdef USE_OPENCL
void
rt_bit_tree(struct bit_tree *btp, const union tree *tp, size_t *len)
{
    int idx;
    unsigned st_bit, uop, rchild;

    if (tp == TREE_NULL)
	return;

    idx = (*len)++;
    switch (tp->tr_op) {
	case OP_SOLID:
	    /* Tree Leaf */
	    st_bit = tp->tr_a.tu_stp->st_bit;
	    if (btp)
		btp[idx].val = (st_bit << 3) | UOP_SOLID;
	    return;
	case OP_SUBTRACT:
	    uop = UOP_SUBTRACT;
	    break;
	case OP_UNION:
	    uop = UOP_UNION;
	    break;
	case OP_INTERSECT:
	    uop = UOP_INTERSECT;
	    break;
	case OP_XOR:
	    uop = UOP_XOR;
	    break;
	default:
	    bu_log("rt_bit_tree: bad op[%d]\n", tp->tr_op);
	    exit(1);
	    break;
    }

    rt_bit_tree(btp, tp->tr_b.tb_left, len);

    rchild = *len;
    if (btp)
	btp[idx].val = (rchild << 3) | uop;

    rt_bit_tree(btp, tp->tr_b.tb_right, len);
}
#endif


/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
