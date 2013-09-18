/*                   C O M B _ T R E E . C P P
 * BRL-CAD
 *
 * Copyright (c) 2013 United States Government as represented by
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
/** @file Comb_Tree.cpp
 *
 * File for writing out a combination and its tree contentents 
 * into the STEPcode containers
 *
 */

#include "STEPWrapper.h"

STEPentity *
Comb_Tree_to_STEP(struct directory *dp, struct rt_db_internal *intern)
{
    STEPentity *toplevel_comb = NULL;

    struct rt_comb_internal *comb = (struct rt_comb_internal *)(intern->idb_ptr);

    /* Find all combs, make instances of them, insert them, and stick in *dp to STEPentity* map */
    /* Find all solids, make instances of them, insert them, and stick in *dp to STEPentity* map */

    /* TODO - id combs whos name is the name of a single solid under them plus .c - those combs
     * are auto-created by the brep routine and don't need to be created here.*/

    /* For each comb, get list of immediate children and call Assembly Product,
     * which will define the relationships between the comb and its children using
     * the appropriate step foo and the pointers in the map.*/

    /* TODO - need to figure out how to pull matricies, translate them into STEP, and
     * where to associate them.*/

    //union tree *curr_node = db_find_named_leaf(comb->tree, curr_dp->d_namep)
    //matp_t curr_matrix = curr_node->tr_l.tl_mat;
    //bn_mat_print(curr_dp->d_namep, curr_matrix);

    return toplevel_comb;
}


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
