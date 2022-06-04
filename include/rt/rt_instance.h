/*                    R T _ I N S T A N C E . H
 * BRL-CAD
 *
 * Copyright (c) 1993-2022 United States Government as represented by
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
#include "bu/hist.h"
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
    int                 rti_nlights;    /**< @brief  number of light sources */
    int                 rti_prismtrace; /**< @brief  add support for pixel prism trace */
    char *              rti_region_fix_file; /**< @brief  rt_regionfix() file or NULL */
    int                 rti_space_partition;  /**< @brief  space partitioning method */
    struct bn_tol       rti_tol;        /**< @brief  Math tolerances for this model */
    struct bg_tess_tol  rti_ttol;       /**< @brief  Tessellation tolerance defaults */
    fastf_t             rti_max_beam_radius; /**< @brief  Max threat radius for FASTGEN cline solid */
    /* THESE ITEMS ARE AVAILABLE FOR APPLICATIONS TO READ */
    point_t             mdl_min;        /**< @brief  min corner of model bounding RPP */
    point_t             mdl_max;        /**< @brief  max corner of model bounding RPP */
    point_t             rti_pmin;       /**< @brief  for plotting, min RPP */
    point_t             rti_pmax;       /**< @brief  for plotting, max RPP */
    double              rti_radius;     /**< @brief  radius of model bounding sphere */
    struct db_i *       rti_dbip;       /**< @brief  prt to Database instance struct */
    /* THESE ITEMS SHOULD BE CONSIDERED OPAQUE, AND SUBJECT TO CHANGE */
    int                 needprep;       /**< @brief  needs rt_prep */
    struct region **    Regions;        /**< @brief  ptrs to regions [reg_bit] */
    struct bu_list      HeadRegion;     /**< @brief  ptr of list of regions in model */
    void *              Orca_hash_tbl;  /**< @brief  Hash table in matrices for ORCA */
    struct bu_ptbl      delete_regs;    /**< @brief  list of region pointers to delete after light_init() */
    /* Ray-tracing statistics */
    size_t              nregions;       /**< @brief  total # of regions participating */
    size_t              nsolids;        /**< @brief  total # of solids participating */
    size_t              rti_nrays;      /**< @brief  # calls to rt_shootray() */
    size_t              nmiss_model;    /**< @brief  rays missed model RPP */
    size_t              nshots;         /**< @brief  # of calls to ft_shot() */
    size_t              nmiss;          /**< @brief  solid ft_shot() returned a miss */
    size_t              nhits;          /**< @brief  solid ft_shot() returned a hit */
    size_t              nmiss_tree;     /**< @brief  shots missed sub-tree RPP */
    size_t              nmiss_solid;    /**< @brief  shots missed solid RPP */
    size_t              ndup;           /**< @brief  duplicate shots at a given solid */
    size_t              nempty_cells;   /**< @brief  number of empty spatial partition cells passed through */
    union cutter        rti_CutHead;    /**< @brief  Head of cut tree */
    union cutter        rti_inf_box;    /**< @brief  List of infinite solids */
    union cutter *      rti_CutFree;    /**< @brief  cut Freelist */
    struct bu_ptbl      rti_busy_cutter_nodes; /**< @brief  List of "cutter" mallocs */
    struct bu_ptbl      rti_cuts_waiting;
    int                 rti_cut_maxlen; /**< @brief  max len RPP list in 1 cut bin */
    int                 rti_ncut_by_type[CUT_MAXIMUM+1];        /**< @brief  number of cuts by type */
    int                 rti_cut_totobj; /**< @brief  # objs in all bins, total */
    int                 rti_cut_maxdepth;/**< @brief  max depth of cut tree */
    struct soltab **    rti_sol_by_type[ID_MAX_SOLID+1];
    int                 rti_nsol_by_type[ID_MAX_SOLID+1];
    int                 rti_maxsol_by_type;
    int                 rti_air_discards;/**< @brief  # of air regions discarded */
    struct bu_hist      rti_hist_cellsize; /**< @brief  occupancy of cut cells */
    struct bu_hist      rti_hist_cell_pieces; /**< @brief  solid pieces per cell */
    struct bu_hist      rti_hist_cutdepth; /**< @brief  depth of cut tree */
    struct soltab **    rti_Solids;     /**< @brief  ptrs to soltab [st_bit] */
    struct bu_list      rti_solidheads[RT_DBNHASH]; /**< @brief  active solid lists */
    struct bu_ptbl      rti_resources;  /**< @brief  list of 'struct resource's encountered */
    size_t              rti_cutlen;     /**< @brief  goal for # solids per boxnode */
    size_t              rti_cutdepth;   /**< @brief  goal for depth of NUBSPT cut tree */
    /* Parameters required for rt_submodel */
    char *              rti_treetop;    /**< @brief  bu_strduped, for rt_submodel rti's only */
    size_t              rti_uses;       /**< @brief  for rt_submodel */
    /* Parameters for accelerating "pieces" of solids */
    size_t              rti_nsolids_with_pieces; /**< @brief  # solids using pieces */
    /* Parameters for dynamic geometry */
    int                 rti_add_to_new_solids_list;
    struct bu_ptbl      rti_new_solids;
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
    register struct bu_list *_head = &((_rti)->rti_solidheads[0]); \
    for (; _head < &((_rti)->rti_solidheads[RT_DBNHASH]); _head++) \
	for (BU_LIST_FOR(_s, soltab, _head)) {

#define RT_VISIT_ALL_SOLTABS_END        } }

/**************************/
/* Applications interface */
/**************************/

/* Prepare for raytracing */

RT_EXPORT extern struct rt_i *rt_new_rti(struct db_i *dbip);
RT_EXPORT extern void rt_free_rti(struct rt_i *rtip);
RT_EXPORT extern void rt_prep(struct rt_i *rtip);
RT_EXPORT extern void rt_prep_parallel(struct rt_i *rtip,
				       int ncpu);


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
 * User-called function to add a set of tree hierarchies to the active
 * set. Includes getting the indicated list of attributes and a
 * bu_hash_tbl for use with the ORCA man regions. (stashed in the
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
RT_EXPORT extern int rt_gettrees_and_attrs(struct rt_i *rtip,
				           const char **attrs,
				           int argc,
				           const char **argv,
				           int ncpus);

DEPRECATED RT_EXPORT extern int rt_load_attrs(struct rt_i *rtip,
					      char **attrs);


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
RT_EXPORT extern void rt_clean_resource_complete(struct rt_i *rtip,
						 struct resource *resp);


/* Plot a solid */
int rt_plot_solid(
    FILE                *fp,
    struct rt_i         *rtip,
    const struct soltab *stp,
    struct resource     *resp);

/* Release storage assoc with rt_i */
RT_EXPORT extern void rt_clean(struct rt_i *rtip);
RT_EXPORT extern int rt_del_regtree(struct rt_i *rtip,
				    struct region *delregp,
				    struct resource *resp);
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
