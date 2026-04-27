/*                    R T _ I N S T A N C E . H
 * BRL-CAD
 *
 * Copyright (c) 1993-2026 United States Government as represented by
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
/** @addtogroup rt_instance
 * @brief The "raytrace instance" structure contains definitions for
 * librt which are specific to the particular model being processed.
 */
/** @{ */
/** @file rt/rt_instance.h */

#ifndef RT_RT_INSTANCE_H
#define RT_RT_INSTANCE_H

#include "common.h"
#include "vmath.h"
#include "bu/list.h"
#include "bu/ptbl.h"
#include "bn/tol.h"
#include "rt/defines.h"
#include "rt/db_instance.h"
#include "rt/region.h"
#include "rt/resource.h"
#include "rt/space_partition.h" /* cutter */
#include "rt/soltab.h"
#include "rt/tol.h"

__BEGIN_DECLS

// libbu's callback type isn't quite right for this case, so we might as well
// be specific.
typedef void(*rti_clbk_t)(struct rt_i *rtip, struct db_tree_state *tsp, struct region *r);

/**
 * Callback type for rt_iterate_regions().  Return 0 to continue
 * iteration; return non-zero to stop early.
 */
typedef int (*rt_region_callback_t)(struct region *regp, void *udata);

struct rt_i_internal; /* forward declaration for private state */

/**
 * Read-only statistics counters for a ray-trace instance.
 *
 * This sub-struct groups all scalar performance and geometry counters
 * that are maintained by librt during prep and ray-shooting.
 * Applications may freely read any field; all writes are performed
 * internally by librt.
 *
 * Access pattern:  rtip->stats.nregions,  rtip->stats.rti_nrays, etc.
 */
struct rt_i_stats {
    /* Geometry counts (set during rt_prep) */
    size_t  nregions;       /**< @brief  total # of regions participating */
    size_t  nsolids;        /**< @brief  total # of solids participating */

    /* Ray-shooting counters (accumulated during rt_shootray / rt_shootrays) */
    size_t  rti_nrays;      /**< @brief  # calls to rt_shootray() */
    size_t  nmiss_model;    /**< @brief  rays missed model RPP */
    size_t  nshots;         /**< @brief  # of calls to ft_shot() */
    size_t  nmiss;          /**< @brief  solid ft_shot() returned a miss */
    size_t  nhits;          /**< @brief  solid ft_shot() returned a hit */
    size_t  nmiss_tree;     /**< @brief  shots missed sub-tree RPP */
    size_t  nmiss_solid;    /**< @brief  shots missed solid RPP */
    size_t  ndup;           /**< @brief  duplicate shots at a given solid */
    size_t  nempty_cells;   /**< @brief  number of empty spatial partition cells passed through */

    /* Space-partition (cut tree) statistics (set during rt_cut_it) */
    size_t  rti_cut_maxlen;             /**< @brief  max len RPP list in 1 cut bin */
    size_t  rti_ncut_by_type[CUT_MAXIMUM+1]; /**< @brief  number of cuts by type */
    size_t  rti_cut_totobj;             /**< @brief  # objs in all bins, total */
    size_t  rti_cut_maxdepth;           /**< @brief  max depth of cut tree */
};

/**
 * This structure keeps track of almost everything for ray-tracing
 * support: Regions, primitives, model bounding box, statistics.
 *
 * Definitions for librt which are specific to the particular model
 * being processed, one copy for each model.  Initially, a pointer to
 * this is returned from rt_dirbuild().
 *
 * During gettree processing, the most time consuming step is
 * searching the list of existing solids to see if a new solid is
 * actually an identical instance of a previous solid.  Therefore, the
 * list has been divided into several lists.  The same macros & hash
 * value that accesses the dbi_Head[] array are used here.  The hash
 * value is computed by db_dirhash().
 */
struct rt_i {
    uint32_t            rti_magic;      /**< @brief  magic # for integrity check */
    /* THESE ITEMS ARE AVAILABLE FOR APPLICATIONS TO READ & MODIFY */
    int                 useair;         /**< @brief  1="air" regions are retained while prepping */
    int                 rti_save_overlaps; /**< @brief  1=fill in pt_overlap_reg, change boolweave behavior */
    int                 rti_dont_instance; /**< @brief  1=Don't compress instances of solids into 1 while prepping */
    int                 rti_hasty_prep; /**< @brief  1=hasty prep, slower ray-trace */
    size_t              rti_nlights;    /**< @brief  number of light sources */
    int                 rti_prismtrace; /**< @brief  add support for pixel prism trace */
    char *              rti_region_fix_file; /**< @brief  rt_regionfix() file or NULL */
    int                 rti_space_partition;  /**< @brief  space partitioning method */
    struct bn_tol       rti_tol;        /**< @brief  Math tolerances for this model */
    struct bg_tess_tol  rti_ttol;       /**< @brief  Tessellation tolerance defaults */
    fastf_t             rti_max_beam_radius; /**< @brief  Max threat radius for FASTGEN cline solid */
    rti_clbk_t          rti_gettrees_clbk;  /**< @brief  Optional user clbk function called during rt_gettrees_and_attrs */
    void *              rti_udata;      /**< @brief  ptr for user data. */
    /* THESE ITEMS ARE AVAILABLE FOR APPLICATIONS TO READ */
    point_t             mdl_min;        /**< @brief  min corner of model bounding RPP */
    point_t             mdl_max;        /**< @brief  max corner of model bounding RPP */
    point_t             rti_pmin;       /**< @brief  for plotting, min RPP */
    point_t             rti_pmax;       /**< @brief  for plotting, max RPP */
    double              rti_radius;     /**< @brief  radius of model bounding sphere */
    struct db_i *       rti_dbip;       /**< @brief  prt to Database instance struct */
    int                 needprep;       /**< @brief  needs rt_prep */
    struct rt_i_stats   stats;          /**< @brief  geometry counts and ray-shooting counters */

    /* THESE ITEMS SHOULD BE CONSIDERED OPAQUE, AND SUBJECT TO CHANGE */
    struct bu_list      HeadRegion;     /**< @brief  ptr of list of regions in model */
    struct bu_ptbl      rti_resources;  /**< @brief  list of 'struct resource's encountered */

    /* PRIVATE librt-internal state; see src/librt/librt_private.h */
    struct rt_i_internal *i;
};


#define RTI_NULL        ((struct rt_i *)0)

#define RT_CHECK_RTI(_p) BU_CKMAG(_p, RTI_MAGIC, "struct rt_i")
#define RT_CK_RTI(_p) RT_CHECK_RTI(_p)

/**
 * Macros to painlessly visit all the active solids.  Serving suggestion:
 *
 * RT_VISIT_ALL_SOLTABS_START(stp, rtip) {
 *      rt_pr_soltab(stp);
 * } RT_VISIT_ALL_SOLTABS_END
 */
#define RT_VISIT_ALL_SOLTABS_START(_s, _rti) { \
    int _i; \
    for (_i = 0; _i < RT_DBNHASH; _i++) { \
	struct bu_list *_head = rt_solidhead_ptr((_rti), _i); \
	for (BU_LIST_FOR(_s, soltab, _head)) {

#define RT_VISIT_ALL_SOLTABS_END        } } }

/**************************/
/* Applications interface */
/**************************/

/* Prepare for raytracing */

RT_EXPORT extern struct rt_i *rt_new_rti(struct db_i *dbip);
RT_EXPORT extern void rt_free_rti(struct rt_i *rtip);
RT_EXPORT extern void rt_prep(struct rt_i *rtip);
RT_EXPORT extern void rt_prep_parallel(struct rt_i *rtip,
				       int ncpu);

/**
 * Return a pointer to the idx-th active-solid list head in rtip.
 * Used by the RT_VISIT_ALL_SOLTABS_START macro and any code that
 * needs to iterate over all prepared solids without accessing the
 * private rt_i_internal struct directly.
 */
RT_EXPORT extern struct bu_list *rt_solidhead_ptr(struct rt_i *rtip, int idx);


/* Get expr tree for object */
/**
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
RT_EXPORT extern int rt_gettree(struct rt_i *rtip,
				const char *node);
RT_EXPORT extern int rt_gettrees(struct rt_i *rtip,
				 int argc,
				 const char **argv, int ncpus);

/**
 * User-called function to add a set of tree hierarchies to the active set.
 * Includes getting the indicated list of attributes and an optional
 * user-supplied rti_gettrees_clbk callback function to collect additional
 * information in rti_udata. (stashed in the rt_i structure).
 *
 * This function may run in parallel, but is not multiply re-entrant itself,
 * because db_walk_tree() isn't multiply re-entrant.  Note that callback
 * implementations should protect any data writes to a shared structure with
 * the RT_SEM_RESULTS semaphore.
 *
 * Semaphores used for critical sections in parallel mode:
 * RT_SEM_TREE ====> protects rtip->i->rti_solidheads[] lists, d_uses(solids)
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
RT_EXPORT extern int rt_gettrees_and_attrs(struct rt_i *rtip,
				           const char **attrs,
				           int argc,
				           const char **argv,
				           int ncpus);

/* Print the partitions */
RT_EXPORT extern void rt_pr_partitions(const struct rt_i *rtip,
				       const struct partition *phead,
				       const char *title);

/**
 * @brief
 * Find solid by leaf name
 *
 * Given the (leaf) name of a solid, find the first occurrence of it
 * in the solid list.  Used mostly to find the light source.  Returns
 * soltab pointer, or RT_SOLTAB_NULL.
 */
RT_EXPORT extern struct soltab *rt_find_solid(const struct rt_i *rtip,
					      const char *name);

/**
 * initialize a memory resource structure for use during ray tracing.
 *
 * a given resource structure is prepared for use and marked as the
 * resource for a given thread of execution (indicated by 'cpu_num').
 * if an 'rtip' ray tracing instance pointer is provided, the resource
 * structure will be stored within so that it's available to threads
 * of execution during parallel ray tracing.
 *
 * This routine should initialize all the same resources that
 * rt_clean_resource() releases.  It shouldn't (but currently does for
 * ptbl) allocate any dynamic memory, just init pointers & lists.
 */

struct rt_i; /* forward declaration */

RT_EXPORT extern void rt_init_resource(struct resource *resp, int cpu_num, struct rt_i *rtip);


RT_EXPORT extern void rt_clean_resource_basic(struct rt_i *rtip,
					struct resource *resp);
RT_EXPORT extern void rt_clean_resource(struct rt_i *rtip,
					struct resource *resp);
/* Deprecated - use rt_clean_resource_basic */
DEPRECATED RT_EXPORT extern void rt_clean_resource_complete(struct rt_i *rtip,
						 struct resource *resp);


/* Plot a solid */
int rt_plot_solid(
    FILE                *fp,
    struct rt_i         *rtip,
    const struct soltab *stp);

/* Release storage assoc with rt_i */
RT_EXPORT extern void rt_clean(struct rt_i *rtip);
RT_EXPORT extern int rt_del_regtree(struct rt_i *rtip,
				    struct region *delregp);

/**
 * Iterate over all regions in the rt_i, calling @p callback for each one.
 * Iteration stops early if @p callback returns non-zero.
 * This API hides the internal bu_list representation of the region list.
 */
RT_EXPORT extern void rt_iterate_regions(struct rt_i *rtip,
					 rt_region_callback_t callback,
					 void *udata);

/**
 * Mark a region for deletion after light_init() completes.
 * This hides the internal delete_regs bu_ptbl storage.
 */
RT_EXPORT extern void rt_mark_region_deleted(struct rt_i *rtip,
					     struct region *regp);

/**
 * Report count of delete_regs (used by src/rt.view.c)
 */
RT_EXPORT extern size_t rt_deleted_regions_cnt(struct rt_i *rtip);

/**
 * Get deleted region n
 */
RT_EXPORT extern struct region * rt_deleted_region_get(struct rt_i *rtip, size_t n);


/* Check in-memory data structures */
RT_EXPORT extern void rt_ck(struct rt_i *rtip);

/* Print value of tree for a partition */
RT_EXPORT extern void rt_pr_tree_val(const union tree *tp,
				     const struct partition *partp,
				     int pr_name, int lvl);
/* Print a partition */
RT_EXPORT extern void rt_pr_partition(const struct rt_i *rtip,
				      const struct partition *pp);
RT_EXPORT extern void rt_pr_partition_vls(struct bu_vls *v,
				   const struct rt_i *rtip,
				   const struct partition *pp);


/**
 * Go through all the solids in the model, given the model mins and
 * maxes, and generate a cutting tree.  A strategy better than
 * incrementally cutting each solid is to build a box node which
 * contains everything in the model, and optimize it.
 *
 * This is the main entry point into space partitioning from
 * rt_prep().
 */
RT_EXPORT extern void rt_cut_it(struct rt_i *rtip,
				int ncpu);

/* free a cut tree */
/**
 * Free a whole cut tree below the indicated node.  The strategy we
 * use here is to free everything BELOW the given node, so as not to
 * clobber rti_CutHead !
 */
RT_EXPORT extern void rt_fr_cut(struct rt_i *rtip,
				union cutter *cutp);

/**
 * Apply any deltas to reg_regionid values to allow old applications
 * that use the reg_regionid number to distinguish between different
 * instances of the same prototype region.
 *
 * Called once, from rt_prep(), before raytracing begins.
 */
RT_EXPORT extern void rt_regionfix(struct rt_i *rtip);


#ifdef USE_OPENCL
RT_EXPORT extern void clt_init(void);

RT_EXPORT extern void
clt_db_store(size_t count, struct soltab *solids[]);

RT_EXPORT extern void
clt_db_store_bvh(size_t count, struct clt_linear_bvh_node *nodes);

RT_EXPORT extern void
clt_db_store_regions(size_t sz_btree_array, struct bit_tree *btp, size_t nregions, struct cl_bool_region *regions, struct cl_region *mtls);

RT_EXPORT extern void
clt_db_store_regions_table(cl_uint *regions_table, size_t regions_table_size);

RT_EXPORT extern void clt_db_release(void);


RT_EXPORT void clt_prep(struct rt_i *rtip);
#endif


__END_DECLS

#endif /* RT_RT_INSTANCE_H */
/** @} */
/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
