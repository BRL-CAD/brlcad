/*                     R E S O U R C E . H
 * BRL-CAD
 *
 * Copyright (c) 1993-2021 United States Government as represented by
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
/** @addtogroup rt_resource
 * @brief
 * Per-CPU statistics and resources.
 */
/** @{ */
/** @file resource.h */

#ifndef RT_RESOURCE_H
#define RT_RESOURCE_H

#include "common.h"
#include "vmath.h"
#include "bu/list.h"
#include "bu/ptbl.h"
#include "rt/defines.h"
#include "rt/tree.h"
#include "rt/directory.h"

__BEGIN_DECLS

/**
 * One of these structures is needed per thread of execution, usually
 * with calling applications creating an array with at least MAX_PSW
 * elements.  To prevent excessive competition for free structures,
 * memory is now allocated on a per-processor basis.  The application
 * structure a_resource element specifies the resource structure to be
 * used; if uniprocessing, a null a_resource pointer results in using
 * the internal global structure (&rt_uniresource), making initial
 * application development simpler.
 *
 * Note that if multiple models are being used, the partition and bitv
 * structures (which are variable length) will require there to be
 * ncpus * nmodels resource structures, the selection of which will be
 * the responsibility of the application.
 *
 * Applications are responsible for calling rt_init_resource() on each
 * resource structure before letting LIBRT use them.
 *
 * Per-processor statistics are initially collected in here, and then
 * posted to rt_i by rt_add_res_stats().
 */
struct resource {
    uint32_t            re_magic;       /**< @brief  Magic number */
    int                 re_cpu;         /**< @brief  processor number, for ID */
    struct bu_list      re_seg;         /**< @brief  Head of segment freelist */
    struct bu_ptbl      re_seg_blocks;  /**< @brief  Table of malloc'ed blocks of segs */
    long                re_seglen;
    long                re_segget;
    long                re_segfree;
    struct bu_list      re_parthead;    /**< @brief  Head of freelist */
    long                re_partlen;
    long                re_partget;
    long                re_partfree;
    struct bu_list      re_solid_bitv;  /**< @brief  head of freelist */
    struct bu_list      re_region_ptbl; /**< @brief  head of freelist */
    struct bu_list      re_nmgfree;     /**< @brief  head of NMG hitmiss freelist */
    union tree **       re_boolstack;   /**< @brief  Stack for rt_booleval() */
    long                re_boolslen;    /**< @brief  # elements in re_boolstack[] */
    float *             re_randptr;     /**< @brief  ptr into random number table */
    /* Statistics.  Only for examination by rt_add_res_stats() */
    long                re_nshootray;   /**< @brief  Calls to rt_shootray() */
    long                re_nmiss_model; /**< @brief  Rays pruned by model RPP */
    /* Solid nshots = shot_hit + shot_miss */
    long                re_shots;       /**< @brief  # calls to ft_shot() */
    long                re_shot_hit;    /**< @brief  ft_shot() returned a miss */
    long                re_shot_miss;   /**< @brief  ft_shot() returned a hit */
    /* Optimizations.  Rays not shot at solids */
    long                re_prune_solrpp;/**< @brief  shot missed solid RPP, ft_shot skipped */
    long                re_ndup;        /**< @brief  ft_shot() calls skipped for already-ft_shot() solids */
    long                re_nempty_cells;        /**< @brief  number of empty spatial partitioning cells passed through */
    /* Data for accelerating "pieces" of solids */
    struct rt_piecestate *re_pieces;    /**< @brief  array [rti_nsolids_with_pieces] */
    long                re_piece_ndup;  /**< @brief  ft_piece_shot() calls skipped for already-ft_shot() solids */
    long                re_piece_shots; /**< @brief  # calls to ft_piece_shot() */
    long                re_piece_shot_hit;      /**< @brief  ft_piece_shot() returned a miss */
    long                re_piece_shot_miss;     /**< @brief  ft_piece_shot() returned a hit */
    struct bu_ptbl      re_pieces_pending;      /**< @brief  pieces with an odd hit pending */
    /* Per-processor cache of tree unions, to accelerate "tops" and treewalk */
    union tree *        re_tree_hd;     /**< @brief  Head of free trees */
    long                re_tree_get;
    long                re_tree_malloc;
    long                re_tree_free;
    struct directory *  re_directory_hd;
    struct bu_ptbl      re_directory_blocks;    /**< @brief  Table of malloc'ed blocks */
};

/**
 * Resources for uniprocessor
 */
RT_EXPORT extern struct resource rt_uniresource;        /**< @brief  default.  Defined in librt/globals.c */
#define RESOURCE_NULL   ((struct resource *)0)
#define RT_CK_RESOURCE(_p) BU_CKMAG(_p, RESOURCE_MAGIC, "struct resource")
#define RT_RESOURCE_INIT_ZERO { RESOURCE_MAGIC, 0, BU_LIST_INIT_ZERO, BU_PTBL_INIT_ZERO, 0, 0, 0, BU_LIST_INIT_ZERO, 0, 0, 0, BU_LIST_INIT_ZERO, BU_LIST_INIT_ZERO, BU_LIST_INIT_ZERO, NULL, 0, NULL, 0, 0, 0, 0, 0, 0, 0, 0, NULL, 0, 0, 0, 0, BU_PTBL_INIT_ZERO, NULL, 0, 0, 0, NULL, BU_PTBL_INIT_ZERO }

/**
 * Definition of global parallel-processing semaphores.
 *
 * res_syscall is now   BU_SEM_SYSCALL
 */
RT_EXPORT extern int RT_SEM_WORKER;
RT_EXPORT extern int RT_SEM_MODEL;
RT_EXPORT extern int RT_SEM_RESULTS;
RT_EXPORT extern int RT_SEM_TREE0;
RT_EXPORT extern int RT_SEM_TREE1;
RT_EXPORT extern int RT_SEM_TREE2;
RT_EXPORT extern int RT_SEM_TREE3;


__END_DECLS

#endif /* RT_RESOURCE_H */
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
