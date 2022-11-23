/*                         P R E P . H
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
/** @file rt/prep.h
 *
 */

#ifndef RT_PREP_H
#define RT_PREP_H

#include "common.h"
#include "vmath.h"
#include "bu/ptbl.h"
#include "rt/defines.h"
#include "rt/tree.h"
#include "rt/resource.h"
#include "rt/rt_instance.h"

__BEGIN_DECLS

/**
 * Structure used by the "reprep" routines
 */
struct rt_reprep_obj_list {
    size_t ntopobjs;            /**< @brief  number of objects in the original call to gettrees */
    char **topobjs;             /**< @brief  list of the above object names */
    size_t nunprepped;          /**< @brief  number of objects to be unprepped and re-prepped */
    char **unprepped;           /**< @brief  list of the above objects */
    /* Above here must be filled in by application */
    /* Below here is used by dynamic geometry routines, should be zeroed by application before use */
    struct bu_ptbl paths;       /**< @brief  list of all paths from topobjs to unprepped objects */
    struct db_tree_state **tsp; /**< @brief  tree state used by tree walker in "reprep" routines */
    struct bu_ptbl unprep_regions;      /**< @brief  list of region structures that will be "unprepped" */
    size_t old_nsolids;         /**< @brief  rtip->nsolids before unprep */
    size_t old_nregions;        /**< @brief  rtip->nregions before unprep */
    size_t nsolids_unprepped;   /**< @brief  number of soltab structures eliminated by unprep */
    size_t nregions_unprepped;  /**< @brief  number of region structures eliminated by unprep */
};

/* prep.c */
RT_EXPORT extern int rt_unprep(struct rt_i *rtip,
			       struct rt_reprep_obj_list *objs,
			       struct resource *resp);
RT_EXPORT extern int rt_reprep(struct rt_i *rtip,
			       struct rt_reprep_obj_list *objs,
			       struct resource *resp);
RT_EXPORT extern int re_prep_solids(struct rt_i *rtip,
				    int num_solids,
				    char **solid_names,
				    struct resource *resp);


__END_DECLS

#endif /* RT_PREP_H */

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
