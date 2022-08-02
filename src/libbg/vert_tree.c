/*                     V E R T _ T R E E . C
 * BRL-CAD
 *
 * Copyright (c) 2002-2022 United States Government as represented by
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
/** @addtogroup vtree */
/** @{ */
/** @file libbg/vert_tree.c
 *
 * @brief
 * Routines to manage a binary search tree of vertices.
 *
 * The actual vertices are stored in an array
 * for convenient use by routines such as "mk_bot".
 * The binary search tree stores indices into the array.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>

#include "vmath.h"
#include "bu/exit.h"
#include "bu/malloc.h"
#include "bg/vert_tree.h"


#define VERT_BLOCK 512 /**< @brief number of vertices to malloc per call when building the array */


/**
 * Structure to make vertex searching fast.
 *
 * Each leaf represents a vertex, and has an index into
 *   the vertices array ("the_array")
 *
 * Each node is a cutting plane at the "cut_val" on
 *   the "coord" (0, 1, or 2) axis.
 *
 * All vertices with "coord" value less than the "cut_val" are in the "lower"
 * subtree, others are in the "higher".
 */
union vert_tree {
    char type;		/* type - leaf or node */
    struct vert_leaf {
	char type;
	size_t index;	/* index into the array */
    } vleaf;
    struct vert_node {
	char type;
	double cut_val; /* cutting value */
	size_t coord;	/* cutting coordinate */
	union vert_tree *higher, *lower;	/* subtrees */
    } vnode;
};

/* types for the above "vert_tree" */
#define VERT_LEAF	'l'
#define VERT_NODE	'n'


struct bg_vert_tree *
bg_vert_tree_create(void)
{
    struct bg_vert_tree *tree;

    BU_ALLOC(tree, struct bg_vert_tree);
    tree->magic = BN_VERT_TREE_MAGIC;
    tree->tree_type = BN_VERT_TREE_TYPE_VERTS;
    tree->the_tree = (union vert_tree *)NULL;
    tree->curr_vert = 0;
    tree->max_vert = VERT_BLOCK;
    tree->the_array = (fastf_t *)bu_malloc( tree->max_vert * 3 * sizeof( fastf_t ), "vert tree array" );

    return tree;
}

struct bg_vert_tree *
bg_vert_tree_create_w_norms(void)
{
    struct bg_vert_tree *tree;

    BU_ALLOC(tree, struct bg_vert_tree);
    tree->magic = BN_VERT_TREE_MAGIC;
    tree->tree_type = BN_VERT_TREE_TYPE_VERTS_AND_NORMS;
    tree->the_tree = (union vert_tree *)NULL;
    tree->curr_vert = 0;
    tree->max_vert = VERT_BLOCK;
    tree->the_array = (fastf_t *)bu_malloc( tree->max_vert * 6 * sizeof( fastf_t ), "vert tree array" );

    return tree;
}


/**
 * static recursion routine used by "clean_vert_tree"
 */
static void
clean_vert_tree_recurse( union vert_tree *ptr )
{
    if ( ptr->type == VERT_NODE ) {
	clean_vert_tree_recurse( ptr->vnode.higher );
	clean_vert_tree_recurse( ptr->vnode.lower );
    }

    bu_free( (char *)ptr, "vert_tree" );

}

void
bg_vert_tree_clean( struct bg_vert_tree *tree )
{
    BN_CK_VERT_TREE( tree );

    if ( !tree->the_tree ) return;

    clean_vert_tree_recurse( tree->the_tree );
    tree->the_tree = (union vert_tree *)NULL;
    tree->curr_vert = 0;
}

/**
 * static recursive routine used by "free_vert_tree"
 */
static void
free_vert_tree_recurse( union vert_tree *ptr )
{
    if ( ptr->type == VERT_NODE ) {
	free_vert_tree_recurse( ptr->vnode.higher );
	free_vert_tree_recurse( ptr->vnode.lower );
    }

    bu_free( (char *)ptr, "vert_tree" );

}

void
bg_vert_tree_destroy( struct bg_vert_tree *tree )
{
    union vert_tree *ptr;

    if ( !tree )
	return;

    BN_CK_VERT_TREE( tree );

    ptr = tree->the_tree;
    if ( !ptr )
	return;

    free_vert_tree_recurse( ptr );

    bu_free( (char *)tree->the_array, "vertex array" );

    tree->the_tree = (union vert_tree *)NULL;
    tree->the_array = (fastf_t *)NULL;
    tree->curr_vert = 0;
    tree->max_vert = 0;
}

size_t
bg_vert_tree_add( struct bg_vert_tree *tree, double x, double y, double z, fastf_t local_tol_sq )
{
    union vert_tree *ptr, *prev=NULL, *new_leaf, *new_node;
    vect_t diff = VINIT_ZERO;
    vect_t vertex;

    BN_CK_VERT_TREE( tree );

    if ( tree->tree_type != BN_VERT_TREE_TYPE_VERTS ) {
	bu_bomb( "Error: bg_vert_tree_add() called for a tree containing vertices and normals\n" );
    }

    VSET( vertex, x, y, z );

    /* look for this vertex already in the list */
    ptr = tree->the_tree;
    while ( ptr ) {
	if ( ptr->type == VERT_NODE ) {
	    prev = ptr;
	    if ( vertex[ptr->vnode.coord] >= ptr->vnode.cut_val ) {
		ptr = ptr->vnode.higher;
	    } else {
		ptr = ptr->vnode.lower;
	    }
	} else {
	    size_t ij;

	    ij = ptr->vleaf.index*3;
	    diff[0] = fabs( vertex[0] - tree->the_array[ij] );
	    diff[1] = fabs( vertex[1] - tree->the_array[ij+1] );
	    diff[2] = fabs( vertex[2] - tree->the_array[ij+2] );
	    if ( (diff[0]*diff[0] + diff[1]*diff[1] + diff[2]*diff[2]) <= local_tol_sq ) {
		/* close enough, use this vertex again */
		return ptr->vleaf.index;
	    }
	    break;
	}
    }

    /* add this vertex to the list */
    if ( tree->curr_vert >= tree->max_vert ) {
	/* allocate more memory for vertices */
	tree->max_vert += VERT_BLOCK;

	tree->the_array = (fastf_t *)bu_realloc( tree->the_array, sizeof( fastf_t ) * tree->max_vert * 3,
						 "tree->the_array" );
    }

    VMOVE( &tree->the_array[tree->curr_vert*3], vertex );

    /* add to the tree also */
    BU_ALLOC(new_leaf, union vert_tree);
    new_leaf->vleaf.type = VERT_LEAF;
    new_leaf->vleaf.index = tree->curr_vert++;
    if ( !tree->the_tree ) {
	/* first vertex, it becomes the root */
	tree->the_tree = new_leaf;
    } else if ( ptr && ptr->type == VERT_LEAF ) {
	/* search above ended at a leaf, need to add a node above this leaf and the new leaf */
	BU_ALLOC(new_node, union vert_tree);
	new_node->vnode.type = VERT_NODE;

	/* select the cutting coord based on the biggest difference */
	if ( diff[0] >= diff[1] && diff[0] >= diff[2] ) {
	    new_node->vnode.coord = 0;
	} else if ( diff[1] >= diff[2] && diff[1] >= diff[0] ) {
	    new_node->vnode.coord = 1;
	} else if ( diff[2] >= diff[1] && diff[2] >= diff[0] ) {
	    new_node->vnode.coord = 2;
	}

	/* set the cut value to the mid value between the two vertices */
	new_node->vnode.cut_val = (vertex[new_node->vnode.coord] +
				   tree->the_array[ptr->vleaf.index * 3 + new_node->vnode.coord]) * 0.5;

	/* set the node "lower" and "higher" pointers */
	if ( vertex[new_node->vnode.coord] >=
	     tree->the_array[ptr->vleaf.index * 3 + new_node->vnode.coord] ) {
	    new_node->vnode.higher = new_leaf;
	    new_node->vnode.lower = ptr;
	} else {
	    new_node->vnode.higher = ptr;
	    new_node->vnode.lower = new_leaf;
	}

	if ( ptr == tree->the_tree ) {
	    /* if the above search ended at the root, redefine the root */
	    tree->the_tree =  new_node;
	} else {
	    /* set the previous node to point to our new one */
	    if ( prev->vnode.higher == ptr ) {
		prev->vnode.higher = new_node;
	    } else {
		prev->vnode.lower = new_node;
	    }
	}
    } else if ( ptr && ptr->type == VERT_NODE ) {
	/* above search ended at a node, just add the new leaf */
	prev = ptr;
	if ( vertex[prev->vnode.coord] >= prev->vnode.cut_val ) {
	    if ( prev->vnode.higher ) {
		bu_bomb("higher vertex node already exists in bg_vert_tree_add()?\n");
	    }
	    prev->vnode.higher = new_leaf;
	} else {
	    if ( prev->vnode.lower ) {
		bu_bomb("lower vertex node already exists in bg_vert_tree_add()?\n");
	    }
	    prev->vnode.lower = new_leaf;
	}
    } else {
	fprintf( stderr, "*********ERROR********\n" );
    }

    /* return the index into the vertex array */
    return new_leaf->vleaf.index;
}

size_t
bg_vert_tree_add_w_norm( struct bg_vert_tree *tree, double x, double y, double z, double nx, double ny, double nz, fastf_t local_tol_sq )
{
    union vert_tree *ptr, *prev=NULL, *new_leaf, *new_node;
    fastf_t diff[6];
    fastf_t vertex[6];
    double d1_sq=0.0, d2_sq=0.0;

    BN_CK_VERT_TREE( tree );

    if ( tree->tree_type != BN_VERT_TREE_TYPE_VERTS_AND_NORMS ) {
	bu_bomb( "Error: bg_vert_tree_add_w_norm() called for a tree containing just vertices\n" );
    }

    VSET( vertex, x, y, z );
    VSET( &vertex[3], nx, ny, nz );

    /* look for this vertex and normal already in the list */
    ptr = tree->the_tree;
    while ( ptr ) {
	size_t i;

	if ( ptr->type == VERT_NODE ) {
	    prev = ptr;
	    if ( vertex[ptr->vnode.coord] >= ptr->vnode.cut_val ) {
		ptr = ptr->vnode.higher;
	    } else {
		ptr = ptr->vnode.lower;
	    }
	} else {
	    size_t ij;

	    ij = ptr->vleaf.index*6;
	    for ( i=0; i<6; i++ ) {
		diff[i] = fabs( vertex[i] - tree->the_array[ij+i] );
	    }
	    d1_sq = VDOT( diff, diff );
	    d2_sq = VDOT( &diff[3], &diff[3] );
	    if ( d1_sq <= local_tol_sq && d2_sq <= 0.0001 ) {
		/* close enough, use this vertex and normal again */
		return ptr->vleaf.index;
	    }
	    break;
	}
    }

    /* add this vertex and normal to the list */
    if ( tree->curr_vert >= tree->max_vert ) {
	/* allocate more memory for vertices */
	tree->max_vert += VERT_BLOCK;

	tree->the_array = (fastf_t *)bu_realloc( tree->the_array,
						 sizeof( fastf_t ) * tree->max_vert * 6,
						 "tree->the_array" );
    }

    VMOVE( &tree->the_array[tree->curr_vert*6], vertex );
    VMOVE( &tree->the_array[tree->curr_vert*6+3], &vertex[3] );

    /* add to the tree also */
    BU_ALLOC(new_leaf, union vert_tree);
    new_leaf->vleaf.type = VERT_LEAF;
    new_leaf->vleaf.index = tree->curr_vert++;
    if ( !tree->the_tree ) {
	/* first vertex, it becomes the root */
	tree->the_tree = new_leaf;
    } else if ( ptr && ptr->type == VERT_LEAF ) {
	fastf_t max;
	size_t i;

	/* search above ended at a leaf, need to add a node above this leaf and the new leaf */
	BU_ALLOC(new_node, union vert_tree);
	new_node->vnode.type = VERT_NODE;

	/* select the cutting coord based on the biggest difference */
	if ( d1_sq <= local_tol_sq ) {
	    /* cut based on normal */
	    new_node->vnode.coord = 3;
	} else {
	    new_node->vnode.coord = 0;
	}
	max = diff[new_node->vnode.coord];
	for ( i=new_node->vnode.coord+1; i<6; i++ ) {
	    if ( diff[i] > max ) {
		new_node->vnode.coord = i;
		max = diff[i];
	    }
	}

	/* set the cut value to the mid value between the two vertices or normals */
	new_node->vnode.cut_val = (vertex[new_node->vnode.coord] +
				   tree->the_array[ptr->vleaf.index * 3 + new_node->vnode.coord]) * 0.5;

	/* set the node "lower" and "higher" pointers */
	if ( vertex[new_node->vnode.coord] >=
	     tree->the_array[ptr->vleaf.index * 3 + new_node->vnode.coord] ) {
	    new_node->vnode.higher = new_leaf;
	    new_node->vnode.lower = ptr;
	} else {
	    new_node->vnode.higher = ptr;
	    new_node->vnode.lower = new_leaf;
	}

	if ( ptr == tree->the_tree ) {
	    /* if the above search ended at the root, redefine the root */
	    tree->the_tree =  new_node;
	} else {
	    /* set the previous node to point to our new one */
	    if ( prev->vnode.higher == ptr ) {
		prev->vnode.higher = new_node;
	    } else {
		prev->vnode.lower = new_node;
	    }
	}
    } else if ( ptr && ptr->type == VERT_NODE ) {
	/* above search ended at a node, just add the new leaf */
	prev = ptr;
	if ( vertex[prev->vnode.coord] >= prev->vnode.cut_val ) {
	    if ( prev->vnode.higher ) {
		bu_bomb("higher vertex node already exists in bg_vert_tree_add_w_norm()?\n");
	    }
	    prev->vnode.higher = new_leaf;
	} else {
	    if ( prev->vnode.lower ) {
		bu_bomb("lower vertex node already exists in bg_vert_tree_add_w_norm()?\n");
	    }
	    prev->vnode.lower = new_leaf;
	}
    } else {
	fprintf( stderr, "*********ERROR********\n" );
    }

    /* return the index into the vertex array */
    return new_leaf->vleaf.index;
}
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
