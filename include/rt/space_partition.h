/*                 S P A C E _ P A R T I T I O N . H
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
/** @file space_partition.h
 *
 */

#ifndef RT_SPACE_PARTITION_H
#define RT_SPACE_PARTITION_H

#include "common.h"
#include "vmath.h"
#include "bn/tol.h"
#include "rt/defines.h"
#include "rt/soltab.h"

__BEGIN_DECLS

struct rt_piecelist;  /* forward declaration */

/**
 * Structures for space subdivision.
 *
 * cut_type is an integer for efficiency of access in rt_shootray() on
 * non-word addressing machines.
 *
 * If a solid has 'pieces', it will be listed either in bn_list
 * (initially), or in bn_piecelist, but not both.
 */
struct cutnode {
    int         cn_type;
    int         cn_axis;                /**< @brief 0, 1, 2 = cut along X, Y, Z */
    fastf_t             cn_point;       /**< @brief cut through axis==point */
    union cutter *      cn_l;           /**< @brief val < point */
    union cutter *      cn_r;           /**< @brief val >= point */
};

struct boxnode {
    int         bn_type;
    fastf_t             bn_min[3];
    fastf_t             bn_max[3];
    struct soltab **bn_list;            /**< @brief bn_list[bn_len] */
    size_t              bn_len;         /**< @brief # of solids in list */
    size_t              bn_maxlen;      /**< @brief # of ptrs allocated to list */
    struct rt_piecelist *bn_piecelist;  /**< @brief [] solids with pieces */
    size_t              bn_piecelen;    /**< @brief # of piecelists used */
    size_t              bn_maxpiecelen; /**< @brief # of piecelists allocated */
};


#define CUT_CUTNODE     1
#define CUT_BOXNODE     2
#define CUT_MAXIMUM     2
union cutter {
    int cut_type;
    union cutter *cut_forw;     /**< @brief Freelist forward link */
    struct cutnode cn;
    struct boxnode bn;
};


#define CUTTER_NULL     ((union cutter *)0)

/**
 * Print out a cut tree.
 *
 * lvl is recursion level.
 */
RT_EXPORT extern void rt_pr_cut(const union cutter *cutp,
				int lvl);

struct rt_i;     /*forward declaration */
struct resource; /*forward declaration */
struct soltab;   /*forward declaration */
RT_EXPORT extern void rt_pr_cut_info(const struct rt_i  *rtip,
				     const char         *str);
RT_EXPORT extern void remove_from_bsp(struct soltab *stp,
				      union cutter *cutp,
				      struct bn_tol *tol);
RT_EXPORT extern void insert_in_bsp(struct soltab *stp,
				    union cutter *cutp);
RT_EXPORT extern void fill_out_bsp(struct rt_i *rtip,
				   union cutter *cutp,
				   struct resource *resp,
				   fastf_t bb[6]);

struct bvh_build_node; /*forward declaration */
RT_EXPORT extern struct bvh_build_node *
hlbvh_create(long max_prims_in_node, struct bu_pool *pool, const fastf_t *centroids_prims,
	     const fastf_t *bounds_prims, long *total_nodes,
	     const long n_primitives, long **ordered_prims);


/**
 * Add a solid into a given boxnode, extending the lists there.  This
 * is used only for building the root node, which will then be
 * subdivided.
 *
 * Solids with pieces go onto a special list.
 */
RT_EXPORT extern void rt_cut_extend(union cutter *cutp,
				    struct soltab *stp,
				    const struct rt_i *rtip);

/**
 * Return pointer to cell 'n' along a given ray.  Used for debugging
 * of how space partitioning interacts with shootray.  Intended to
 * mirror the operation of rt_shootray().  The first cell is 0.
 */
RT_EXPORT extern const union cutter *rt_cell_n_on_ray(struct application *ap,
						      int n);
/*
 * The rtip->rti_CutFree list can not be freed directly because is
 * bulk allocated.  Fortunately, we have a list of all the
 * bu_malloc()'ed blocks.  This routine may be called before the first
 * frame is done, so it must be prepared for uninitialized items.
 */
RT_EXPORT extern void rt_cut_clean(struct rt_i *rtip);


#ifdef USE_OPENCL
struct clt_bvh_bounds {
    cl_double p_min[3], p_max[3];
};

struct clt_linear_bvh_node {
    struct clt_bvh_bounds bounds;
    union {
	cl_int primitives_offset;	/* leaf */
	cl_int second_child_offset;	/* interior */
    } u;
    cl_ushort n_primitives;		/* 0 -> interior node */
    cl_uchar axis;			/* interior node: xyz */
    cl_uchar pad[1];			/* ensure 32 byte total size */
};


RT_EXPORT extern void
clt_linear_bvh_create(long n_primitives, struct clt_linear_bvh_node **nodes_p,
		      long **ordered_prims, const fastf_t *centroids_prims,
		      const fastf_t *bounds_prims, cl_int *total_nodes);
#endif


__END_DECLS

#endif /* RT_SPACE_PARTITION_H */

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
