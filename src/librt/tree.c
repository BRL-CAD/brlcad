/*                          T R E E . C
 * BRL-CAD
 *
 * Copyright (c) 1995-2011 United States Government as represented by
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
/** @addtogroup librt */
/** @{ */
/** @file librt/tree.c
 *
 * Ray Tracing library database tree walker.
 *
 * Collect and prepare regions and solids for subsequent ray-tracing.
 *
 */
/** @} */

#include "common.h"

#include <stddef.h>
#include <stdio.h>
#include <math.h>
#include <string.h>
#include "bio.h"

#include "bu.h"
#include "vmath.h"
#include "bn.h"
#include "db.h"
#include "raytrace.h"
#include "mater.h"


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


HIDDEN void
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
 * _ R T _ G E T T R E E _ R E G I O N _ S T A R T
 *
 * This routine must be prepared to run in parallel.
 */
HIDDEN int
_rt_gettree_region_start(struct db_tree_state *tsp, const struct db_full_path *pathp, const struct rt_comb_internal *combp, genptr_t UNUSED(client_data))
{
    if (tsp) {
	RT_CK_RTI(tsp->ts_rtip);
	RT_CK_RESOURCE(tsp->ts_resp);
    }
    if (pathp) RT_CK_FULL_PATH(pathp);
    if (combp) RT_CHECK_COMB(combp);

    /* Ignore "air" regions unless wanted */
    if (tsp->ts_rtip->useair == 0 &&  tsp->ts_aircode != 0) {
	tsp->ts_rtip->rti_air_discards++;
	return -1;	/* drop this region */
    }
    return 0;
}


/**
 * _ R T _ G E T T R E E _ R E G I O N _ E N D
 *
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
HIDDEN union tree *
_rt_gettree_region_end(struct db_tree_state *tsp, const struct db_full_path *pathp, union tree *curtree, genptr_t client_data)
{
    struct region *rp;
    struct directory *dp = NULL;
    size_t shader_len=0;
    struct rt_i *rtip;
    Tcl_HashTable *tbl = (Tcl_HashTable *)client_data;
    Tcl_HashEntry *entry;
    matp_t inv_mat;
    struct bu_attribute_value_set avs;
    struct bu_attribute_value_pair *avpp;

    RT_CK_DBI(tsp->ts_dbip);
    RT_CK_FULL_PATH(pathp);
    RT_CK_TREE(curtree);
    rtip =  tsp->ts_rtip;
    RT_CK_RTI(rtip);
    RT_CK_RESOURCE(tsp->ts_resp);

    if (curtree->tr_op == OP_NOP) {
	/* Ignore empty regions */
	return curtree;
    }

    BU_GETSTRUCT(rp, region);
    rp->l.magic = RT_REGION_MAGIC;
    rp->reg_regionid = tsp->ts_regionid;
    rp->reg_is_fastgen = tsp->ts_is_fastgen;
    rp->reg_aircode = tsp->ts_aircode;
    rp->reg_gmater = tsp->ts_gmater;
    rp->reg_los = tsp->ts_los;

    dp = (struct directory *)DB_FULL_PATH_CUR_DIR(pathp);
    bu_avs_init_empty(&avs); 
    if (!db5_get_attributes(tsp->ts_dbip, &avs, dp)) {
	bu_avs_init_empty(&(rp->attr_values));
	for(BU_AVS_FOR(avpp, &(tsp->ts_attrs))) {
	    bu_avs_add(&(rp->attr_values), avpp->name, bu_avs_get(&avs, avpp->name));
	}
    }

    rp->reg_mater = tsp->ts_mater; /* struct copy */
    if (tsp->ts_mater.ma_shader)
	shader_len = strlen(tsp->ts_mater.ma_shader);
    if (shader_len) {
	rp->reg_mater.ma_shader = bu_strdup(tsp->ts_mater.ma_shader);
    } else
	rp->reg_mater.ma_shader = (char *)NULL;

    rp->reg_name = db_path_to_string(pathp);


    if (RT_G_DEBUG&DEBUG_TREEWALK) {
	bu_log("_rt_gettree_region_end() %s\n", rp->reg_name);
	rt_pr_tree(curtree, 0);
    }

    rp->reg_treetop = curtree;
    rp->reg_all_unions = db_is_tree_all_unions(curtree);

    /* Determine material properties */
    rp->reg_mfuncs = (char *)0;
    rp->reg_udata = (char *)0;
    if (rp->reg_mater.ma_color_valid == 0)
	rt_region_color_map(rp);

    /* enter critical section */
    bu_semaphore_acquire(RT_SEM_RESULTS);

    rp->reg_instnum = dp->d_uses++;

    /*
     * Add the region to the linked list of regions.
     * Positions in the region bit vector are established at this time.
     */
    BU_LIST_INSERT(&(rtip->HeadRegion), &rp->l);

    /* Assign bit vector pos. */
    rp->reg_bit = rtip->nregions++;

    /* leave critical section */
    bu_semaphore_release(RT_SEM_RESULTS);

    if (tbl && bu_avs_get(&tsp->ts_attrs, "ORCA_Comp")) {
	int newentry;
	long int reg_bit = rp->reg_bit;
	const char *key = (char *)reg_bit;

	inv_mat = (matp_t)bu_calloc(16, sizeof(fastf_t), "inv_mat");
	bn_mat_inv(inv_mat, tsp->ts_mat);

	/* enter critical section */
	bu_semaphore_acquire(RT_SEM_RESULTS);

	entry = Tcl_CreateHashEntry(tbl, key, &newentry);
	Tcl_SetHashValue(entry, (ClientData)inv_mat);

	/* leave critical section */
	bu_semaphore_release(RT_SEM_RESULTS);
    }

    if (RT_G_DEBUG & DEBUG_REGIONS) {
	bu_log("Add Region %s instnum %d\n",
	       rp->reg_name, rp->reg_instnum);
    }

    /* Indicate that we have swiped 'curtree' */
    return TREE_NULL;
}


/**
 * R T _ F I N D _ I D E N T I C A L _ S O L I D
 *
 * See if solid "dp" as transformed by "mat" already exists in the
 * soltab list.  If it does, return the matching stp, otherwise,
 * create a new soltab structure, enrole it in the list, and return a
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
HIDDEN struct soltab *
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
	    if (RT_G_DEBUG & DEBUG_SOLIDS) {
		bu_log(mat ?
		       "%s re-referenced %d\n" :
		       "%s re-referenced %d (identity mat)\n",
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
    BU_GETSTRUCT(stp, soltab);
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
    BU_LIST_INSERT(&(rtip->rti_solidheads[hash]), &(stp->l));

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
    bu_semaphore_acquire(RT_SEM_STATS);
    stp->st_bit = rtip->nsolids++;
    bu_semaphore_release(RT_SEM_STATS);

    /*
     * Fill in the last little bit of the structure in full parallel
     * mode, outside of any critical section.
     */

    /* Init tables of regions using this solid.  Usually small. */
    bu_ptbl_init(&stp->st_regions, 7, "st_regions ptbl");

    return stp;
}


/**
 * R T _ G E T T R E E _ L E A F
 *
 * This routine must be prepared to run in parallel.
 */
HIDDEN union tree *
_rt_gettree_leaf(struct db_tree_state *tsp, const struct db_full_path *pathp, struct rt_db_internal *ip, genptr_t UNUSED(client_data))
{
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
    RT_CK_RESOURCE(tsp->ts_resp);
    dp = DB_FULL_PATH_CUR_DIR(pathp);

    /* Determine if this matrix is an identity matrix */

    if (!bn_mat_is_equal(tsp->ts_mat, bn_mat_identity, &rtip->rti_tol)) {
	/* Not identity matrix */
	mat = (matp_t)tsp->ts_mat;
    } else {
	/* Identity matrix */
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

    if (rtip->rti_add_to_new_solids_list) {
	bu_ptbl_ins(&rtip->rti_new_solids, (long *)stp);
    }

    stp->st_id = ip->idb_type;
    stp->st_meth = &rt_functab[ip->idb_type];
    if (mat) {
	mat = stp->st_matp;
    } else {
	mat = (matp_t)bn_mat_identity;
    }

    RT_CK_DB_INTERNAL(ip);

    /* init solid's maxima and minima */
    VSETALL(stp->st_max, -INFINITY);
    VSETALL(stp->st_min,  INFINITY);

    /*
     * If the ft_prep routine wants to keep the internal structure,
     * that is OK, as long as idb_ptr is set to null.  Note that the
     * prep routine may have changed st_id.
     */
    ret = -1;
    if (stp->st_meth->ft_prep) {
	ret = stp->st_meth->ft_prep(stp, ip, rtip);
    }
    if (ret != 0) {
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
    if (RT_G_DEBUG&DEBUG_TREEWALK && stp->st_path.magic == DB_FULL_PATH_MAGIC) {
	char *sofar = db_path_to_string(&stp->st_path);
	bu_log("_rt_gettree_leaf() st_path=%s\n", sofar);
	bu_free(sofar, "path string");
    }

    if (RT_G_DEBUG&DEBUG_SOLIDS) {
	struct bu_vls str;
	bu_log("\n---Primitive %d: %s\n", stp->st_bit, dp->d_namep);
	bu_vls_init(&str);

	/* verbose=1, mm2local=1.0 */
	ret = -1;
	if (stp->st_meth->ft_describe) {
	    ret = stp->st_meth->ft_describe(&str, ip, 1, 1.0, tsp->ts_resp, tsp->ts_dbip);
	}
	if (ret < 0) {
	    bu_log("_rt_gettree_leaf(%s):  solid describe failure\n",
		   dp->d_namep);
	}
	bu_log("%s:  %s", dp->d_namep, bu_vls_addr(&str));
	bu_vls_free(&str);
    }

found_it:
    RT_GET_TREE(curtree, tsp->ts_resp);
    curtree->tr_op = OP_SOLID;
    curtree->tr_a.tu_stp = stp;
    /* regionp will be filled in later by _rt_tree_region_assign() */
    curtree->tr_a.tu_regionp = (struct region *)0;

    if (RT_G_DEBUG&DEBUG_TREEWALK) {
	char *sofar = db_path_to_string(pathp);
	bu_log("_rt_gettree_leaf() %s\n", sofar);
	bu_free(sofar, "path string");
    }

    return curtree;
}


/**
 * R T _ F R E E _ S O L T A B
 *
 * Decrement use count on soltab structure.  If no longer needed,
 * release associated storage, and free the structure.
 *
 * This routine semaphore protects against other copies of itself
 * running in parallel, and against other routines (such as
 * _rt_find_identical_solid()) which might also be modifying the
 * linked list heads.
 *
 * Called by -
 * db_free_tree()
 * rt_clean()
 * rt_gettrees()
 * rt_kill_deal_solid_refs()
 */
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
 * R T _ T R E E _ K I L L _ D E A D _ S O L I D _ R E F S
 *
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
		    if (RT_G_DEBUG&DEBUG_TREEWALK)bu_log("encountered dead solid '%s' stp=x%x, tp=x%x\n",
							 stp->st_dp->d_namep, stp, tp);
		    rt_free_soltab(stp);
		    tp->tr_a.tu_stp = SOLTAB_NULL;
		    tp->tr_op = OP_NOP;	/* Convert to NOP */
		}
		return;
	    }

	default:
	    bu_log("_rt_tree_kill_dead_solid_refs(x%x): unknown op=x%x\n",
		   tp, tp->tr_op);
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
 * R T _ G E T T R E E S _ M U V E S
 *
 * User-called function to add a set of tree hierarchies to the active
 * set. Includes getting the indicated list of attributes and a
 * Tcl_HashTable for use with the ORCA man regions. (stashed in the
 * rt_i structure).
 *
 * This function may run in parallel, but is not multiply re-entrant
 * itself, because db_walk_tree() isn't multiply re-entrant.
 *
 * Semaphores used for critical sections in parallel mode:
 * RT_SEM_TREE ====> protects rti_solidheads[] lists, d_uses(solids)
 * RT_SEM_RESULTS => protects HeadRegion, mdl_min/max, d_uses(reg), nregions
 * RT_SEM_WORKER ==> (db_walk_dispatcher, from db_walk_tree)
 * RT_SEM_STATS ===> nsolids
 *
 * INPUTS:
 *
 * rtip - RT instance pointer
 *
 * attrs - attribute value set
 * 
 * argc - number of trees to get
 *
 * argv - array of char pointers to the names of the tree tops
 *
 * ncpus - number of cpus to use
 *
 * Returns -
 * 0 Ordinarily
 * -1 On major error
 */
int
rt_gettrees_muves(struct rt_i *rtip, const char **attrs, int argc, const char **argv, int ncpus)
{
    struct soltab *stp;
    struct region *regp;
    Tcl_HashTable *tbl;
    size_t prev_sol_count;
    int i;
    int num_attrs=0;
    point_t region_min, region_max;

    RT_CHECK_RTI(rtip);
    RT_CK_DBI(rtip->rti_dbip);

    if (!rtip->needprep) {
	bu_log("ERROR: rt_gettree() called again after rt_prep!\n");
	return -1;		/* FAIL */
    }

    if (argc <= 0) return -1;	/* FAIL */

    tbl = (Tcl_HashTable *)bu_malloc(sizeof(Tcl_HashTable), "rtip->Orca_hash_tbl");
    Tcl_InitHashTable(tbl, TCL_ONE_WORD_KEYS);
    rtip->Orca_hash_tbl = (genptr_t)tbl;

    prev_sol_count = rtip->nsolids;

    {
	struct db_tree_state tree_state;

	tree_state = rt_initial_tree_state;	/* struct copy */
	tree_state.ts_dbip = rtip->rti_dbip;
	tree_state.ts_rtip = rtip;
	tree_state.ts_resp = NULL;	/* sanity.  Needs to be updated */


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

	i = db_walk_tree(rtip->rti_dbip, argc, argv, ncpus,
			 &tree_state,
			 _rt_gettree_region_start,
			 _rt_gettree_region_end,
			 _rt_gettree_leaf, (genptr_t)tbl);
	bu_avs_free(&tree_state.ts_attrs);
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
	(void)rt_tree_elim_nops(regp->reg_treetop, &rt_uniresource);
    }
again:
    RT_VISIT_ALL_SOLTABS_START(stp, rtip) {
	RT_CK_SOLTAB(stp);
	if (stp->st_aradius <= 0) {
	    bu_log("rt_gettrees() cleaning up dead solid '%s'\n",
		   stp->st_dp->d_namep);
	    rt_free_soltab(stp);
	    /* Can't do rtip->nsolids--, that doubles as max bit number! */
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
	    stp->st_piecestate_num = rtip->rti_nsolids_with_pieces++;
	}
	if (RT_G_DEBUG&DEBUG_SOLIDS)
	    rt_pr_soltab(stp);
    } RT_VISIT_ALL_SOLTABS_END;

    /* Handle finishing touches on the trees that needed soltab
     * structs that the parallel code couldn't look at yet.
     */
    for (BU_LIST_FOR(regp, region, &(rtip->HeadRegion))) {
	RT_CK_REGION(regp);

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

    if (i < 0) return i;

    if (rtip->nsolids <= prev_sol_count)
	bu_log("rt_gettrees(%s) warning:  no primitives found\n", argv[0]);
    return 0;	/* OK */
}


/**
 * R T _ G E T T R E E S _ A N D _ A T T R S
 *
 * User-called function to add a set of tree hierarchies to the active
 * set.
 *
 * This function may run in parallel, but is not multiply re-entrant
 * itself, because db_walk_tree() isn't multiply re-entrant.
 *
 * Semaphores used for critical sections in parallel mode:
 * RT_SEM_TREE* protects rti_solidheads[] lists, d_uses(solids)
 * RT_SEM_RESULTS protects HeadRegion, mdl_min/max, d_uses(reg), nregions
 * RT_SEM_WORKER (db_walk_dispatcher, from db_walk_tree)
 * RT_SEM_STATS nsolids
 *
 * Returns -
 * 0 Ordinarily
 * -1 On major error
 * -2 If there were unresolved names
 */
int
rt_gettrees_and_attrs(struct rt_i *rtip, const char **attrs, int argc, const char **argv, int ncpus)
{
    return rt_gettrees_muves(rtip, attrs, argc, argv, ncpus);
}


/**
 * R T _ G E T T R E E
 *
 * User-called function to add a tree hierarchy to the displayed set.
 *
 * This function is not multiply re-entrant.
 *
 * Returns -
 * 0 Ordinarily
 * -1 On major error
 *
 * Note: -2 returns from rt_gettrees_and_attrs are filtered.
 */
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


/**
 * R T _ T R E E _ E L I M _ N O P S
 *
 * Eliminate any references to NOP nodes from the tree.  It is safe to
 * use db_free_tree() here, because there will not be any dead solids.
 * They will all have been converted to OP_NOP nodes by
 * _rt_tree_kill_dead_solid_refs(), previously, so there is no need to
 * worry about multiple db_free_tree()'s repeatedly trying to free one
 * solid that has been instanced multiple times.
 *
 * Returns -
 * 0 this node is OK.
 * -1 request caller to kill this node
 */
int
rt_tree_elim_nops(union tree *tp, struct resource *resp)
{
    union tree *left, *right;

    RT_CK_RESOURCE(resp);
top:
    RT_CK_TREE(tp);

    switch (tp->tr_op) {

	case OP_SOLID:
	    return 0;		/* Retain */

	default:
	    bu_log("rt_tree_elim_nops(x%x): unknown op=x%x\n",
		   tp, tp->tr_op);
	    return -1;

	case OP_XOR:
	case OP_UNION:
	    /* BINARY type -- rewrite tp as surviving side */
	    left = tp->tr_b.tb_left;
	    right = tp->tr_b.tb_right;
	    if (rt_tree_elim_nops(left, resp) < 0) {
		*tp = *right;	/* struct copy */
		RT_FREE_TREE(left, resp);
		RT_FREE_TREE(right, resp);
		goto top;
	    }
	    if (rt_tree_elim_nops(right, resp) < 0) {
		*tp = *left;	/* struct copy */
		RT_FREE_TREE(left, resp);
		RT_FREE_TREE(right, resp);
		goto top;
	    }
	    break;
	case OP_INTERSECT:
	    /* BINARY type -- if either side fails, nuke subtree */
	    left = tp->tr_b.tb_left;
	    right = tp->tr_b.tb_right;
	    if (rt_tree_elim_nops(left, resp) < 0 ||
		rt_tree_elim_nops(right, resp) < 0) {
		db_free_tree(left, resp);
		db_free_tree(right, resp);
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
	    if (rt_tree_elim_nops(left, resp) < 0) {
		db_free_tree(left, resp);
		db_free_tree(right, resp);
		tp->tr_op = OP_NOP;
		return -1;	/* eliminate reference to tp */
	    }
	    if (rt_tree_elim_nops(right, resp) < 0) {
		*tp = *left;	/* struct copy */
		RT_FREE_TREE(left, resp);
		RT_FREE_TREE(right, resp);
		goto top;
	    }
	    break;
	case OP_NOT:
	case OP_GUARD:
	case OP_XNOP:
	    /* UNARY tree -- for completeness only, should never be seen */
	    left = tp->tr_b.tb_left;
	    if (rt_tree_elim_nops(left, resp) < 0) {
		RT_FREE_TREE(left, resp);
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

/**
 * R T _ F I N D _ S O L I D
 *
 * Given the (leaf) name of a solid, find the first occurance of it in
 * the solid list.  Used mostly to find the light source.  Returns
 * soltab pointer, or RT_SOLTAB_NULL.
 */
struct soltab *
rt_find_solid(const struct rt_i *rtip, const char *name)
{
    struct soltab *stp;
    struct directory *dp;

    RT_CHECK_RTI(rtip);
    if ((dp = db_lookup((struct db_i *)rtip->rti_dbip, (char *)name,
			LOOKUP_QUIET)) == RT_DIR_NULL)
	return RT_SOLTAB_NULL;

    RT_VISIT_ALL_SOLTABS_START(stp, (struct rt_i *)rtip) {
	if (stp->st_dp == dp)
	    return stp;
    } RT_VISIT_ALL_SOLTABS_END
	  return RT_SOLTAB_NULL;
}


/**
 * R T _ O P T I M _ T R E E
 */
void
rt_optim_tree(union tree *tp, struct resource *resp)
{
    union tree **sp;
    union tree *low;
    union tree **stackend;

    RT_CK_TREE(tp);
    while ((sp = resp->re_boolstack) == (union tree **)0)
	rt_grow_boolstack(resp);
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
		    rt_grow_boolstack(resp);
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
		    rt_grow_boolstack(resp);
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


/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
