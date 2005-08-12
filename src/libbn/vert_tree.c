/*                     V E R T _ T R E E . C
 * BRL-CAD
 *
 * Copyright (C) 2002-2005 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */

/** \addtogroup libbn */
/*@{*/
/** @file vert_tree.c
 * Routines to manage a binary search tree of vertices. The actual vertices are stored in an array
 * for convenient use by routines such as "mk_bot". The binary search tree stores indices into
 * the array.
 *
 *  Author -
 *	John R. Anderson
 *
 *  Source -
 *	The U. S. Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5068  USA
 */
/*@}*/

#ifndef lint
static const char bn_RCSvert_tree[] = "@(#)$Header$ (ARL)";
#endif

#include "common.h"



#include <stdio.h>
#include <math.h>
#ifdef HAVE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif
#include <ctype.h>
#include <errno.h>

#include "machine.h"
#include "vmath.h"
#include "raytrace.h"


/* structure to make vertex searching fast
 * Each leaf represents a vertex, and has an index into the vertices array ("the_array")
 * Each node is a cutting plane at the "cut_val" on the "coord" (0, 1, or 2) axis.
 * All vertices with "coord" value less than the "cut_val" are in the "lower"
 * subtree, others are in the "higher".
 */
union vert_tree {
	char type;		/* type - leaf or node */
	struct vert_leaf {
		char type;
		int index;	/* index into the array */
	} vleaf;
	struct vert_node {
		char type;
		double cut_val; /* cutting value */
		int coord;	/* cutting coordinate */
		union vert_tree *higher, *lower;	/* subtrees */
	} vnode;
};

/* types for the above "vert_tree" */
#define VERT_LEAF	'l'
#define VERT_NODE	'n'


/*		C R E A T E _ V E R T _ T R E E
 *
 *	routine to create a vertex tree.
 *	Possible refinements include specifying an initial size
 */
struct vert_root *
create_vert_tree()
{
	struct vert_root *tree;

	tree = (struct vert_root *)bu_calloc( 1, sizeof( struct vert_root ), "vert_tree_root" );
	tree->magic = VERT_TREE_MAGIC;
	tree->tree_type = TREE_TYPE_VERTS;
	tree->the_tree = (union vert_tree *)NULL;
	tree->curr_vert = 0;
	tree->max_vert = VERT_BLOCK;
	tree->the_array = (fastf_t *)bu_malloc( tree->max_vert * 3 * sizeof( fastf_t ), "vert tree array" );

	return( tree );
}

/*		C R E A T E _ V E R T _ T R E E _ W _ N O R M S
 *
 *	routine to create a vertex tree.
 *	Possible refinements include specifying an initial size
 */
struct vert_root *
create_vert_tree_w_norms()
{
	struct vert_root *tree;

	tree = (struct vert_root *)bu_calloc( 1, sizeof( struct vert_root ), "vert_tree_root" );
	tree->magic = VERT_TREE_MAGIC;
	tree->tree_type = TREE_TYPE_VERTS_AND_NORMS;
	tree->the_tree = (union vert_tree *)NULL;
	tree->curr_vert = 0;
	tree->max_vert = VERT_BLOCK;
	tree->the_array = (fastf_t *)bu_malloc( tree->max_vert * 6 * sizeof( fastf_t ), "vert tree array" );

	return( tree );
}

/*		C L E A N _ V E R T_ T R E E _ R E C U R S E
 *
 *	static recursion routine used by "clean_vert_tree" 
 */
static void
clean_vert_tree_recurse( union vert_tree *ptr )
{
	if( ptr->type == VERT_NODE ) {
		clean_vert_tree_recurse( ptr->vnode.higher );
		clean_vert_tree_recurse( ptr->vnode.lower );
	}

	bu_free( (char *)ptr, "vert_tree" );

}

/*		C L E A N _ V E R T _ T R E E
 *
 *	Routine to free the binary search tree and reset the current number of vertices.
 *	The vertex array is left untouched, for re-use later.
 */
void
clean_vert_tree( struct vert_root *tree_root )
{
	BN_CK_VERT_TREE( tree_root );

	if( !tree_root->the_tree ) return;

	clean_vert_tree_recurse( tree_root->the_tree );
	tree_root->the_tree = (union vert_tree *)NULL;
	tree_root->curr_vert = 0;
}


/*		F R E E _ V E R T_ T R E E_ R E C U R S E
 *
 *	Static recursive routine used by "free_vert_tree"
 */
static void
free_vert_tree_recurse( union vert_tree *ptr )
{
	if( ptr->type == VERT_NODE ) {
		free_vert_tree_recurse( ptr->vnode.higher );
		free_vert_tree_recurse( ptr->vnode.lower );
	}

	bu_free( (char *)ptr, "vert_tree" );

}

/*		F R E E _ V E R T_ T R E E
 *
 *	Routine to free a vertex tree and all associated dynamic memory
 */
void
free_vert_tree( struct vert_root *vert_root )
{
	union vert_tree *ptr;

	if( !vert_root )
		return;

	BN_CK_VERT_TREE( vert_root );

	ptr = vert_root->the_tree;
	if( !ptr )
		return;

	free_vert_tree_recurse( ptr );

	if( vert_root->the_array ) {
		bu_free( (char *)vert_root->the_array, "vertex array" );
	}

	vert_root->the_tree = (union vert_tree *)NULL;
	vert_root->the_array = (fastf_t *)NULL;
	vert_root->curr_vert = 0;
	vert_root->max_vert = 0;
}

/*		A D D _ V E R T
 *
 *	Routine to add a vertex to the current list of part vertices.
 *	The array is re-alloc'd if needed.
 *	Returns index into the array of vertices where this vertex is stored
 */
int
Add_vert( double x, double y, double z, struct vert_root *vert_root, fastf_t local_tol_sq )
{
	union vert_tree *ptr, *prev=NULL, *new_leaf, *new_node;
	vect_t diff;
	vect_t vertex;

	BN_CK_VERT_TREE( vert_root );

	if( vert_root->tree_type != TREE_TYPE_VERTS ) {
		bu_bomb( "Error: Add_vert() called for a tree containing vertices and normals\n" );
	}

	VSET( vertex, x, y, z );

	/* look for this vertex already in the list */
	ptr = vert_root->the_tree;
	while( ptr ) {
		if( ptr->type == VERT_NODE ) {
			prev = ptr;
			if( vertex[ptr->vnode.coord] >= ptr->vnode.cut_val ) {
				ptr = ptr->vnode.higher;
			} else {
				ptr = ptr->vnode.lower;
			}
		} else {
			int ij;

			ij = ptr->vleaf.index*3;
			diff[0] = fabs( vertex[0] - vert_root->the_array[ij] ); 
			diff[1] = fabs( vertex[1] - vert_root->the_array[ij+1] ); 
			diff[2] = fabs( vertex[2] - vert_root->the_array[ij+2] ); 
			if( (diff[0]*diff[0] + diff[1]*diff[1] + diff[2]*diff[2]) <= local_tol_sq ) {
				/* close enough, use this vertex again */
				return( ptr->vleaf.index );
			}
			break;
		}
	}

	/* add this vertex to the list */
	if( vert_root->curr_vert >= vert_root->max_vert ) {
		/* allocate more memory for vertices */
		vert_root->max_vert += VERT_BLOCK;

		vert_root->the_array = (fastf_t *)bu_realloc( vert_root->the_array, sizeof( fastf_t ) * vert_root->max_vert * 3,
						    "vert_root->the_array" );
	}

	VMOVE( &vert_root->the_array[vert_root->curr_vert*3], vertex );

	/* add to the tree also */
	new_leaf = (union vert_tree *)bu_malloc( sizeof( union vert_tree ), "new_leaf" );
	new_leaf->vleaf.type = VERT_LEAF;
	new_leaf->vleaf.index = vert_root->curr_vert++;
	if( !vert_root->the_tree ) {
		/* first vertex, it becomes the root */
		vert_root->the_tree = new_leaf;
	} else if( ptr && ptr->type == VERT_LEAF ) {
		/* search above ended at a leaf, need to add a node above this leaf and the new leaf */
		new_node = (union vert_tree *)bu_malloc( sizeof( union vert_tree ), "new_node" );
		new_node->vnode.type = VERT_NODE;

		/* select the cutting coord based on the biggest difference */
		if( diff[0] >= diff[1] && diff[0] >= diff[2] ) {
			new_node->vnode.coord = 0;
		} else if( diff[1] >= diff[2] && diff[1] >= diff[0] ) {
			new_node->vnode.coord = 1;
		} else if( diff[2] >= diff[1] && diff[2] >= diff[0] ) {
			new_node->vnode.coord = 2;
		}

		/* set the cut value to the mid value between the two vertices */
		new_node->vnode.cut_val = (vertex[new_node->vnode.coord] +
					   vert_root->the_array[ptr->vleaf.index * 3 + new_node->vnode.coord]) * 0.5;

		/* set the node "lower" nad "higher" pointers */
		if( vertex[new_node->vnode.coord] >= 
		    vert_root->the_array[ptr->vleaf.index * 3 + new_node->vnode.coord] ) {
			new_node->vnode.higher = new_leaf;
			new_node->vnode.lower = ptr;
		} else {
			new_node->vnode.higher = ptr;
			new_node->vnode.lower = new_leaf;
		}

		if( ptr == vert_root->the_tree ) {
			/* if the above search ended at the root, redefine the root */
			vert_root->the_tree =  new_node;
		} else {
			/* set the previous node to point to our new one */
			if( prev->vnode.higher == ptr ) {
				prev->vnode.higher = new_node;
			} else {
				prev->vnode.lower = new_node;
			}
		}
	} else if( ptr && ptr->type == VERT_NODE ) {
		/* above search ended at a node, just add the new leaf */
		prev = ptr;
		if( vertex[prev->vnode.coord] >= prev->vnode.cut_val ) {
			if( prev->vnode.higher ) {
				exit(1);
			}
			prev->vnode.higher = new_leaf;
		} else {
			if( prev->vnode.lower ) {
				exit(1);
			}
			prev->vnode.lower = new_leaf;
		}
	} else {
		fprintf( stderr, "*********ERROR********\n" );
	}

	/* return the index into the vertex array */
	return( new_leaf->vleaf.index );
}

/*		A D D _ V E R T _ A N D _ N O R M
 *
 *	Routine to add a vertex and a normal to the current list of part vertices.
 *	The array is re-alloc'd if needed.
 *	Returns index into the array of vertices where this vertex and normal is stored
 */
int
Add_vert_and_norm( double x, double y, double z, double nx, double ny, double nz, struct vert_root *vert_root, fastf_t local_tol_sq )
{
	union vert_tree *ptr, *prev=NULL, *new_leaf, *new_node;
	fastf_t diff[6];
	fastf_t vertex[6];
	double d1_sq=0.0, d2_sq=0.0;

	BN_CK_VERT_TREE( vert_root );

	if( vert_root->tree_type != TREE_TYPE_VERTS_AND_NORMS ) {
		bu_bomb( "Error: Add_vert_and_norm() called for a tree containing just vertices\n" );
	}

	VSET( vertex, x, y, z );
	VSET( &vertex[3], nx, ny, nz );

	/* look for this vertex and normal already in the list */
	ptr = vert_root->the_tree;
	while( ptr ) {
		int i;

		if( ptr->type == VERT_NODE ) {
			prev = ptr;
			if( vertex[ptr->vnode.coord] >= ptr->vnode.cut_val ) {
				ptr = ptr->vnode.higher;
			} else {
				ptr = ptr->vnode.lower;
			}
		} else {
			int ij;

			ij = ptr->vleaf.index*6;
			for( i=0 ; i<6 ; i++ ) {
				diff[i] = fabs( vertex[i] - vert_root->the_array[ij+i] ); 
			}
			d1_sq = VDOT( diff, diff );
			d2_sq = VDOT( &diff[3], &diff[3] );
			if( d1_sq <= local_tol_sq && d2_sq <= 0.0001 ) {
				/* close enough, use this vertex and normal again */
				return( ptr->vleaf.index );
			}
			break;
		}
	}

	/* add this vertex and normal to the list */
	if( vert_root->curr_vert >= vert_root->max_vert ) {
		/* allocate more memory for vertices */
		vert_root->max_vert += VERT_BLOCK;

		vert_root->the_array = (fastf_t *)bu_realloc( vert_root->the_array,
					 sizeof( fastf_t ) * vert_root->max_vert * 6,
					 "vert_root->the_array" );
	}

	VMOVE( &vert_root->the_array[vert_root->curr_vert*6], vertex );
	VMOVE( &vert_root->the_array[vert_root->curr_vert*6+3], &vertex[3] );

	/* add to the tree also */
	new_leaf = (union vert_tree *)bu_malloc( sizeof( union vert_tree ), "new_leaf" );
	new_leaf->vleaf.type = VERT_LEAF;
	new_leaf->vleaf.index = vert_root->curr_vert++;
	if( !vert_root->the_tree ) {
		/* first vertex, it becomes the root */
		vert_root->the_tree = new_leaf;
	} else if( ptr && ptr->type == VERT_LEAF ) {
		fastf_t max;
		int i;

		/* search above ended at a leaf, need to add a node above this leaf and the new leaf */
		new_node = (union vert_tree *)bu_malloc( sizeof( union vert_tree ), "new_node" );
		new_node->vnode.type = VERT_NODE;

		/* select the cutting coord based on the biggest difference */
		if( d1_sq <= local_tol_sq ) {
			/* cut based on normal */
			new_node->vnode.coord = 3;
		} else {
			new_node->vnode.coord = 0;
		}
		max = diff[new_node->vnode.coord];
		for( i=new_node->vnode.coord+1 ; i<6 ; i++ ) {
			if( diff[i] > max ) {
				new_node->vnode.coord = i;
				max = diff[i];
			}
		}

		/* set the cut value to the mid value between the two vertices or normals */
		new_node->vnode.cut_val = (vertex[new_node->vnode.coord] +
					   vert_root->the_array[ptr->vleaf.index * 3 + new_node->vnode.coord]) * 0.5;

		/* set the node "lower" nad "higher" pointers */
		if( vertex[new_node->vnode.coord] >= 
		    vert_root->the_array[ptr->vleaf.index * 3 + new_node->vnode.coord] ) {
			new_node->vnode.higher = new_leaf;
			new_node->vnode.lower = ptr;
		} else {
			new_node->vnode.higher = ptr;
			new_node->vnode.lower = new_leaf;
		}

		if( ptr == vert_root->the_tree ) {
			/* if the above search ended at the root, redefine the root */
			vert_root->the_tree =  new_node;
		} else {
			/* set the previous node to point to our new one */
			if( prev->vnode.higher == ptr ) {
				prev->vnode.higher = new_node;
			} else {
				prev->vnode.lower = new_node;
			}
		}
	} else if( ptr && ptr->type == VERT_NODE ) {
		/* above search ended at a node, just add the new leaf */
		prev = ptr;
		if( vertex[prev->vnode.coord] >= prev->vnode.cut_val ) {
			if( prev->vnode.higher ) {
				exit(1);
			}
			prev->vnode.higher = new_leaf;
		} else {
			if( prev->vnode.lower ) {
				exit(1);
			}
			prev->vnode.lower = new_leaf;
		}
	} else {
		fprintf( stderr, "*********ERROR********\n" );
	}

	/* return the index into the vertex array */
	return( new_leaf->vleaf.index );
}

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
