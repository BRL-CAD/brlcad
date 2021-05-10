/*                        V E R T _ T R E E . H
 * BRL-CAD
 *
 * Copyright (c) 2004-2021 United States Government as represented by
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

/*----------------------------------------------------------------------*/
/** @addtogroup bg_vert_tree
 *
 * @brief
 * Vertex tree support
 *
 * Routines to manage a binary search tree of vertices.
 *
 * The actual vertices are stored in an array
 * for convenient use by routines such as "mk_bot".
 * The binary search tree stores indices into the array.
 *
 */
/** @{ */
/** @file vert_tree.h */

#ifndef BN_VERT_TREE_H
#define BN_VERT_TREE_H

#include "common.h"

#include "vmath.h"

#include "bu/magic.h"
#include "bg/defines.h"

__BEGIN_DECLS

/**
 * packaging structure
 * holds all the required info for a single vertex tree
 */
struct bg_vert_tree {
    uint32_t magic;
    int tree_type;		/**< @brief vertices or vertices with normals */
    union vert_tree *the_tree;	/**< @brief the actual vertex tree */
    fastf_t *the_array;		/**< @brief the array of vertices */
    size_t curr_vert;		/**< @brief the number of vertices currently in the array */
    size_t max_vert;		/**< @brief the current maximum capacity of the array */
};

#define BN_VERT_TREE_TYPE_VERTS 1
#define BN_VERT_TREE_TYPE_VERTS_AND_NORMS 2

#define BN_CK_VERT_TREE(_p) BU_CKMAG(_p, BN_VERT_TREE_MAGIC, "vert_tree")


/**
 *@brief
 *	routine to create a vertex tree.
 *
 *	Possible refinements include specifying an initial size
 */
BG_EXPORT extern struct bg_vert_tree *bg_vert_tree_create(void);

/**
 *@brief
 *	routine to create a vertex tree.
 *
 *	Possible refinements include specifying an initial size
 */
BG_EXPORT extern struct bg_vert_tree *bg_vert_tree_create_w_norms(void);

/**
 *@brief
 *	Routine to free a vertex tree and all associated dynamic memory
 */
BG_EXPORT extern void bg_vert_tree_destroy(struct bg_vert_tree *tree);

/**
 *@brief
 *	Routine to add a vertex to the current list of part vertices.
 *	The array is re-alloc'd if needed.
 *	Returns index into the array of vertices where this vertex is stored
 */
BG_EXPORT extern size_t bg_vert_tree_add(struct bg_vert_tree *tree,
					 double x,
					 double y,
					 double z,
					 fastf_t local_tol_sq);

/**
 *@brief
 *	Routine to add a vertex and a normal to the current list of part vertices.
 *	The array is re-alloc'd if needed.
 *	Returns index into the array of vertices where this vertex and normal is stored
 */
BG_EXPORT extern size_t bg_vert_tree_add_w_norm(struct bg_vert_tree *tree,
						double x,
						double y,
						double z,
						double nx,
						double ny,
						double nz,
						fastf_t local_tol_sq);

/**
 *@brief
 *	Routine to free the binary search tree and reset the current number of vertices.
 *	The vertex array is left untouched, for re-use later.
 */
BG_EXPORT extern void bg_vert_tree_clean(struct bg_vert_tree *tree);


__END_DECLS

#endif  /* BN_VERT_TREE_H */
/** @} */
/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
