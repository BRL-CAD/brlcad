/*                          U G - G . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2006 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this file; see the file named COPYING for more
 * information.
 */
/** @file ug-g.c
 *
 */

#include "common.h"

#define DO_SUPPRESSIONS 0

#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <time.h>
#include <libexc.h>
#include <math.h>
#ifdef HAVE_STRING_H
#  include <string.h>
#else
#  include <strings.h>
#endif

#include "machine.h"
#include "db.h"
#include "vmath.h"
#include "nmg.h"
#include "rtgeom.h"
#include "raytrace.h"
#include "wdb.h"

#include <unistd.h>
#include <uf.h>
#include <uf_ui.h>
#include <uf_disp.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <uf_assem.h>
#include <uf_part.h>
#include <uf_facet.h>
#include <uf_obj.h>
#include <uf_object_types.h>
#include <uf_facet.h>
#include <uf_modl.h>
#include <uf_attr.h>
#include <uf_eval.h>
#include <uf_sket.h>
#include <uf_error.h>
#include <malloc.h>

#include "./conv.h"
#include "./ug_misc.h"

#define MAX_LINE_LEN		256
#define REFSET_NAME_LEN		37
#define INSTANCE_NAME_LEN	37
#define PART_NAME_LEN		257
#define MIN_RADIUS		1.0e-7		/* BRL-CAD does not allow tgc's with zero radius */

#define SIMPLE_HOLE_TYPE	1
#define COUNTER_BORE_HOLE_TYPE	2
#define COUNTER_SINK_HOLE_TYPE	3

ug_tol ugtol = {
    6.350,	/* distance = .25in */
    6.350	/* radius = .25in */
};

static time_t start_time;

int debug = 0;
int show_all_features = 0;

/* declarations to support use of getopt() system call */
char *options = "hsufd:o:i:t:a:n:c:r:R:";
extern char *optarg;
extern int optind, opterr, getopt();

char *progname = "(noname)";

/* count of how parts were converted */
static int parts_facetized=0;
static int parts_bool=0;
static int parts_brep=0;

static char *output_file=NULL;		/* name of BRL-CAD output database */
static struct rt_wdb *wdb_fd=NULL;	/* rt_wdb structure for output database */
static char *use_refset_name=NULL;	/* if set, always use this reference set name */
static int only_facetize=0;		/* if set, do not attempt any feature processing, just facetize everything */
static char **subparts=NULL;		/* list of subparts to be converted */
static int curr_level=0;
static int use_normals=0;		/* if set, obtain normals from database */

static int *part_tris=NULL;		/* list of triangles for current part */
static int max_tri=0;			/* number of triangles currently malloced */
static int curr_tri=0;			/* number of triangles currently being used */

#define TRI_BLOCK 512			/* number of triangles to malloc per call */

static int *part_norms=NULL;		/* list of normals for current part */
static int max_norm=0;			/* number of normals currently malloced */
static int curr_norm=0;			/* number of normals currently being used */

#define NORM_BLOCK 512			/* number of normals to malloc per call */

static int prim_no=0;			/* count of BRL-CAD primitive objects, used to build names */

static int ident=1000;			/* ident number for BRL-CAD regions */

static double tol_dist=0.005;	/* (mm) minimum distance between two distinct vertices */
static double tol_dist_sq;
static double surf_tol=3.175;	/* (mm) allowable surface tesselation tolerance (default is 1/8 inch) */
static double ang_tol=0.0;
static double min_chamfer=0.0;	/* (mm) chamfers smaller than this are ignored */
static double min_round=0.0;	/* (mm) rounds smaller than this are ignored */
static uf_list_t *suppress_list;	/* list of features to be suppressed */

static	Tcl_HashTable htbl;
static	int use_part_name_hash=0;
static char *part_name_file=NULL;
static struct bn_tol tol;

static char *feat_sign[]={
	"Nullsign",
	"Positive",
	"Negative",
	"Unsigned",
	"No Boolean",
	"Top Target",
	"Unite",
	"Subtract",
	"Intersect",
	"Deform Positive",
	"Deform Negative"};

struct refset_list {
	char *name;
	tag_t tag;
	struct refset_list *next;
};

struct obj_list {
	char *name;
	struct obj_list *next;
};

static struct obj_list *brlcad_objs_root=NULL;
#if 0
/* structure to make vertex searching fast
 * Each leaf represents a vertex, and has an index into the
 * part_verts array.
 * Each node is a cutting plane at the "cut_val" on the "coord" (0, 1, or 2) axis.
 * All vertices with "coord" value less than the "cut_val" are in the "lower"
 * subtree, others are in the "higher".
 */
union vert_tree {
	char type;
	struct vert_leaf {
		char type;
		int index;
	} vleaf;
	struct vert_node {
		char type;
		double cut_val;
		int coord;
		union vert_tree *higher, *lower;
	} vnode;
} *vert_root=NULL;

/* types for the above "vert_tree" */
#define VERT_LEAF	'l'
#define VERT_NODE	'n'
#else
static struct vert_root *vert_tree_root;
#endif
static struct vert_root *norm_tree_root;

static int indent_delta=4;

#define DO_INDENT { \
	int _i; \
	for( _i=0 ; _i<curr_level*indent_delta ; _i++ ) { \
		bu_log( " " ); \
	} \
}

char *process_part( tag_t node, const mat_t curr_xform, char *p_name, char *ref_set, char *inst_name );
char *convert_entire_part( tag_t node, char *p_name, char *refset_name, char *inst_name, const mat_t curr_xform, double units_conv );
char *facetize( tag_t solid_tag, char *part_name, char *refset_name, char *inst_name, const mat_t curr_xform, double units_conv, int make_region );
char *conv_features( tag_t solid_tag, char *part_name, char *refset_name, char *inst_name, const mat_t curr_xform,
		     double units_conv, int make_region );
static void do_suppressions( tag_t node );

void
add_to_obj_list( char *name )
{
	struct obj_list *ptr;

	ptr = (struct obj_list *)bu_malloc( sizeof( struct obj_list ), "obj_list" );
	fprintf( stderr, "In add_to_obj_list(%s), &ptr = x%x, name=x%x, brlcad_objs_root = x%x, ptr = x%x\n",
		name, &ptr, name, brlcad_objs_root, ptr );
	ptr->next = brlcad_objs_root;
	brlcad_objs_root = ptr;

	ptr->name = name;
}

double get_ug_double(char *p)
{
    char *lhs, *rhs;
    tag_t exp_tag;
    double value;

    DO_INDENT;
    bu_log( "get_ug_double( %s )\n", p );
    UF_func(UF_MODL_dissect_exp_string(p, &lhs, &rhs, &exp_tag));
    DO_INDENT;
    bu_log( "\texpression tag = %d, lhs = %s, rhs = %s\n", exp_tag, lhs, rhs );
    UF_func(UF_MODL_ask_exp_tag_value(exp_tag, &value));

    return value;
}

void
lower_case( char *name )
{
	unsigned char *c;

	c = (unsigned char *)name;
	while( *c ) {
		if( *c == '/' ) {
			*c = '_';
		} else {
			(*c) = tolower( *c );
		}
		c++;
	}
}

void
create_name_hash( FILE *fd )
{
	char line[MAX_LINE_LEN];
	Tcl_HashEntry *hash_entry=NULL;
	int new_entry=0;

	Tcl_InitHashTable( &htbl, TCL_STRING_KEYS );

	while( fgets( line, MAX_LINE_LEN, fd ) ) {
		char *part_no, *desc, *ptr;

		ptr = strtok( line, " \t\n" );
		if( !ptr ) {
			bu_log( "*****Error processing part name file at line:\n" );
			bu_log( "\t%s\n", line );
			exit( 1 );
		}
		part_no = bu_strdup( ptr );
		lower_case( part_no );
		ptr = strtok( (char *)NULL, " \t\n" );
		if( !ptr ) {
			bu_log( "*****Error processing part name file at line:\n" );
			bu_log( "\t%s\n", line );
			exit( 1 );
		}
		desc = bu_strdup( ptr );
		lower_case( desc );

		hash_entry = Tcl_CreateHashEntry( &htbl, part_no, &new_entry );
		if( new_entry ) {
			Tcl_SetHashValue( hash_entry, desc );
		} else {
			bu_free( (char *)part_no, "part_no" );
			bu_free( (char *)desc, "desc" );
		}
	}
}

/* routine to check for bad triangles
 * only checks for triangles with duplicate vertices
 */
int
bad_triangle( int v1, int v2, int v3 )
{
	double dist;
	double coord;
	int i;

	if( v1 == v2 || v2 == v3 || v1 == v3 )
		return( 1 );

	dist = 0;
	for( i=0 ; i<3 ; i++ ) {
		coord = vert_tree_root->the_array[v1*3 + i] - vert_tree_root->the_array[v2*3 + i];
		dist += coord * coord;
	}
	dist = sqrt( dist );
	if( dist < tol_dist ) {
		return( 1 );
	}

	dist = 0;
	for( i=0 ; i<3 ; i++ ) {
		coord = vert_tree_root->the_array[v2*3 + i] - vert_tree_root->the_array[v3*3 + i];
		dist += coord * coord;
	}
	dist = sqrt( dist );
	if( dist < tol_dist ) {
		return( 1 );
	}

	dist = 0;
	for( i=0 ; i<3 ; i++ ) {
		coord = vert_tree_root->the_array[v1*3 + i] - vert_tree_root->the_array[v3*3 + i];
		dist += coord * coord;
	}
	dist = sqrt( dist );
	if( dist < tol_dist ) {
		return( 1 );
	}

	return( 0 );
}

#if 0
/* routine to free the "vert_tree"
 * called after each part is output
 */
void
free_vert_tree( union vert_tree *ptr )
{
	if( !ptr )
		return;

	if( ptr->type == VERT_NODE ) {
		free_vert_tree( ptr->vnode.higher );
		free_vert_tree( ptr->vnode.lower );
	}

	free( (char *)ptr );
}

/* routine to add a vertex to the current list of part vertices */
int
Add_vert( fastf_t *vertex )
{
	union vert_tree *ptr, *prev=NULL, *new_leaf, *new_node;
	point_t diff;

	/* look for this vertex already in the list */
	ptr = vert_tree_root;
	while( ptr ) {
		if( ptr->type == VERT_NODE ) {
			prev = ptr;
			if( vertex[ptr->vnode.coord] >= ptr->vnode.cut_val ) {
				ptr = ptr->vnode.higher;
			} else {
				ptr = ptr->vnode.lower;
			}
		} else {
			diff[0] = fabs( vertex[0] - part_verts[ptr->vleaf.index*3 + 0] );
			diff[1] = fabs( vertex[1] - part_verts[ptr->vleaf.index*3 + 1] );
			diff[2] = fabs( vertex[2] - part_verts[ptr->vleaf.index*3 + 2] );
			if( (diff[0]*diff[0] + diff[1]*diff[1] + diff[2]*diff[2]) <= tol_dist_sq ) {
				/* close enough, use this vertex again */
				return( ptr->vleaf.index );
			}
			break;
		}
	}

	/* add this vertex to the list */
	if( curr_vert >= max_vert ) {
		/* allocate more memory for vertices */
		max_vert += VERT_BLOCK;

		part_verts = (fastf_t *)realloc( part_verts, sizeof( fastf_t ) * max_vert * 3 );
		if( !part_verts ) {
			fprintf( stderr, "ERROR: Failed to allocate memory for part vertices\n" );
			exit( 1 );
		}
	}

	part_verts[curr_vert*3 + 0] = vertex[0];
	part_verts[curr_vert*3 + 1] = vertex[1];
	part_verts[curr_vert*3 + 2] = vertex[2];

	/* add to the tree also */
	new_leaf = (union vert_tree *)malloc( sizeof( union vert_tree ) );
	new_leaf->vleaf.type = VERT_LEAF;
	new_leaf->vleaf.index = curr_vert++;
	if( !vert_tree_root ) {
		/* first vertex, it becomes the root */
		vert_tree_root = new_leaf;
	} else if( ptr && ptr->type == VERT_LEAF ) {
		/* search above ended at a leaf, need to add a node above this leaf and the new leaf */
		new_node = (union vert_tree *)malloc( sizeof( union vert_tree ) );
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
					   part_verts[ptr->vleaf.index*3 + new_node->vnode.coord]) * 0.5;

		/* set the node "lower" and "higher" pointers */
		if( vertex[new_node->vnode.coord] >= part_verts[ptr->vleaf.index*3 + new_node->vnode.coord] ) {
			new_node->vnode.higher = new_leaf;
			new_node->vnode.lower = ptr;
		} else {
			new_node->vnode.higher = ptr;
			new_node->vnode.lower = new_leaf;
		}

		if( ptr == vert_tree_root ) {
			/* if the above search ended at the root, redefine the root */
			vert_tree_root =  new_node;
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
#endif

/* routine to add a new triangle to the current part */
void
add_triangle( int v1, int v2, int v3 )
{
	if( curr_tri >= max_tri ) {
		/* allocate more memory for triangles */
		max_tri += TRI_BLOCK;
		part_tris = (int *)realloc( part_tris, sizeof( int ) * max_tri * 3 );
		if( !part_tris ) {
			fprintf( stderr, "ERROR: Failed to allocate memory for part triangles\n" );
			exit( 1 );
		}
	}

	/* fill in triangle info */
	part_tris[curr_tri*3 + 0] = v1;
	part_tris[curr_tri*3 + 1] = v2;
	part_tris[curr_tri*3 + 2] = v3;

	/* increment count */
	curr_tri++;
}

/* routine to add a new triangle to the current part */
void
add_face_normals( int v1, int v2, int v3 )
{
	if( curr_norm >= max_norm ) {
		/* allocate more memory for triangles */
		max_norm += NORM_BLOCK;
		part_norms = (int *)realloc( part_norms, sizeof( int ) * max_norm * 3 );
		if( !part_norms ) {
			fprintf( stderr, "ERROR: Failed to allocate memory for part triangles\n" );
			exit( 1 );
		}
	}

	/* fill in triangle info */
	part_norms[curr_norm*3 + 0] = v1;
	part_norms[curr_norm*3 + 1] = v2;
	part_norms[curr_norm*3 + 2] = v3;

	/* increment count */
	curr_norm++;
}


void
get_part_name( struct bu_vls *name )
{
	struct bu_vls vls;
	char *ptr;
	Tcl_HashEntry *hash_entry=NULL;

	if( !use_part_name_hash )
		return;

	lower_case( bu_vls_addr( name ) );

	bu_vls_init( &vls );

	hash_entry = Tcl_FindHashEntry( &htbl, bu_vls_addr( name ) );
	if( !hash_entry ) {
		/* try without final name extension after a _ */
		if( (ptr=strrchr( bu_vls_addr( name ), '_' )) != NULL ) {
			bu_vls_trunc( &vls, 0 );
			bu_vls_vlscat( &vls, name );
			bu_vls_trunc( &vls, ptr - bu_vls_addr( name ) );
			hash_entry = Tcl_FindHashEntry( &htbl, bu_vls_addr( &vls ) );
		}
	}

	if( !hash_entry ) {
		/* try without any name extension after a _ */
		if( (ptr=strchr( bu_vls_addr( name ), '_' )) != NULL ) {
			bu_vls_trunc( &vls, 0 );
			bu_vls_vlscat( &vls, name );
			bu_vls_trunc( &vls, ptr - bu_vls_addr( name ) );
			hash_entry = Tcl_FindHashEntry( &htbl, bu_vls_addr( &vls ) );
		}
	}

	if( !hash_entry ) {
		/* try without final name extension after a - */
		if( (ptr=strrchr( bu_vls_addr( name ), '-' )) != NULL ) {
			bu_vls_trunc( &vls, 0 );
			bu_vls_vlscat( &vls, name );
			bu_vls_trunc( &vls, ptr - bu_vls_addr( name ) );
			hash_entry = Tcl_FindHashEntry( &htbl, bu_vls_addr( &vls ) );
		}
	}

	if( !hash_entry ) {
		/* try without any name extension after a - */
		if( (ptr=strchr( bu_vls_addr( name ), '-' )) != NULL ) {
			bu_vls_trunc( &vls, 0 );
			bu_vls_vlscat( &vls, name );
			bu_vls_trunc( &vls, ptr - bu_vls_addr( name ) );
			hash_entry = Tcl_FindHashEntry( &htbl, bu_vls_addr( &vls ) );
		}
	}

	if( !hash_entry ) {
		/* try adding "-011" */
		bu_vls_trunc( &vls, 0 );
		bu_vls_vlscat( &vls, name );
		if( (ptr=strchr( bu_vls_addr( name ), '-' ))  != NULL ) {
			bu_vls_trunc( &vls, ptr - bu_vls_addr( name ) );
		}
		bu_vls_strcat( &vls, "-011" );
		hash_entry = Tcl_FindHashEntry( &htbl, bu_vls_addr( &vls ) );
	}

	if( !hash_entry ) {
		/* try adding "-001" */
		bu_vls_trunc( &vls, 0 );
		bu_vls_vlscat( &vls, name );
		if( (ptr=strchr( bu_vls_addr( name ), '-' ))  != NULL ) {
			bu_vls_trunc( &vls, ptr - bu_vls_addr( name ) );
		}
		bu_vls_strcat( &vls, "-001" );
		hash_entry = Tcl_FindHashEntry( &htbl, bu_vls_addr( &vls ) );
	}

	bu_vls_free( &vls );
	if( hash_entry ) {
		DO_INDENT;
		bu_log( "---part number = %s, name = %s\n", bu_vls_addr( name ), (char *)Tcl_GetHashValue( hash_entry ) );
		bu_vls_strcpy( name, (char *)Tcl_GetHashValue( hash_entry ) );
	} else {
		DO_INDENT;
		bu_log( "---no name found for part number %s\n", bu_vls_addr( name ) );
	}
	ptr = bu_vls_addr( name );
	while( *ptr != '\0' ) {
		if( !(isalnum( *ptr ) || *ptr == '/')) {
			*ptr = '_';
		}
		ptr++;
	}
}

char *
create_unique_brlcad_name( struct bu_vls *name_vls )
{
	struct bu_vls tmp_vls;
	int count=0;
	int len=0;

	bu_vls_init( &tmp_vls );
	bu_vls_vlscat( &tmp_vls, name_vls );
	len = bu_vls_strlen( &tmp_vls );
	while( db_lookup( wdb_fd->dbip, bu_vls_addr( &tmp_vls ), LOOKUP_QUIET ) != DIR_NULL ) {
		count++;
		bu_vls_trunc( &tmp_vls, len );
		bu_vls_printf( &tmp_vls, ".%d", count );
	}

	/* bu_vls_strgrab() does the equivalent of a bu_vls_free() */
	return( bu_vls_strgrab( &tmp_vls ) );
}

char *
create_unique_brlcad_solid_name()
{
	struct bu_vls solid_name_vls;
	char *solid_name;

	bu_vls_init( &solid_name_vls );
	prim_no++;
	bu_vls_printf( &solid_name_vls, "s.%d", prim_no );
	solid_name = create_unique_brlcad_name( &solid_name_vls );
	bu_vls_free( &solid_name_vls );

	return( solid_name );
}

char *
create_unique_brlcad_combination_name()
{
	struct bu_vls solid_name_vls;
	char *solid_name;

	bu_vls_init( &solid_name_vls );
	prim_no++;
	bu_vls_printf( &solid_name_vls, "c.%d", prim_no );
	solid_name = create_unique_brlcad_name( &solid_name_vls );
	bu_vls_free( &solid_name_vls );

	return( solid_name );
}

char *
build_region( struct wmember *head, char *part_name, char *refset_name, char *inst_name, unsigned char *rgb )
{
	struct bu_vls region_name_vls;
	char *region_name;

	/* make the region */
	bu_vls_init( &region_name_vls );
	if( inst_name ) {
		bu_vls_strcat( &region_name_vls, inst_name );
	} else {
		char *ptr;

		ptr = strrchr( part_name, '/' );
		if( ptr ) {
			ptr++;
			bu_vls_strcat( &region_name_vls, ptr );
		} else {
			bu_vls_strcat( &region_name_vls, part_name );
		}
	}

	get_part_name( &region_name_vls );

	if( refset_name && strcmp( refset_name, "None" ) ) {
		bu_vls_strcat( &region_name_vls, "_" );
		bu_vls_strcat( &region_name_vls, refset_name );
	}
	bu_vls_strcat( &region_name_vls, ".r" );
	region_name = create_unique_brlcad_name( &region_name_vls );
	bu_vls_free( &region_name_vls );

	(void)mk_comb( wdb_fd, region_name, &head->l, 1, NULL, NULL, rgb, ident++, 0, 1, 100, 0, 0, 0 );

	return( region_name );
}


struct pt_list {
	struct bu_list l;
	double t;
	double pt[3];
};

#define INITIAL_APPROX_PTS	5

int
make_curve_particles( tag_t guide_curve, fastf_t outer_diam, fastf_t inner_diam, int is_start, int is_end,
		      struct wmember *outer_head, struct wmember *inner_head, const mat_t curr_xform, double units_conv )
{
	UF_EVAL_p_t evaluator;
	double limits[2];
	fastf_t outer_radius=outer_diam/2.0;
	fastf_t inner_radius=inner_diam/2.0;
	struct bu_vls name_vls;
	char *outer_solid_name;
	char *inner_solid_name;
	struct pt_list pt_head;
	struct pt_list *prev, *pt, *next;
	double tmp_pt[3];
	double t;
	int i;
	int done=0;


	UF_func( UF_EVAL_initialize( guide_curve, &evaluator ) );
	UF_func( UF_EVAL_ask_limits( evaluator, limits ) );

	BU_LIST_INIT( &pt_head.l );

	/* Create initial approximation */
	for( i=0 ; i<=INITIAL_APPROX_PTS ; i++ ) {
		if( i == INITIAL_APPROX_PTS ) {
			t = limits[1];
		} else {
			t = limits[0] + ((limits[1] - limits[0])/(double)(INITIAL_APPROX_PTS)) * (double)i;
		}

		UF_func( UF_EVAL_evaluate( evaluator, 0, t, tmp_pt, NULL ) );

		VSCALE( tmp_pt, tmp_pt, units_conv );
		pt = (struct pt_list *)bu_malloc( sizeof( struct pt_list ), "struct pt_list" );
		BU_LIST_INIT( &pt->l );
		pt->t = t;
		MAT4X3PNT( pt->pt, curr_xform, tmp_pt );

		BU_LIST_INSERT( &pt_head.l, &pt->l );
	}

	/* eliminate collinear points */
	prev = BU_LIST_FIRST( pt_list, &pt_head.l );
	pt = BU_LIST_NEXT( pt_list, &prev->l );
	next = BU_LIST_NEXT( pt_list, &pt->l );

	if( BU_LIST_NOT_HEAD( &prev->l, &pt_head.l ) &&
	    BU_LIST_NOT_HEAD( &pt->l, &pt_head.l ) ) {

		while( BU_LIST_NOT_HEAD( &next->l, &pt_head.l ) ) {

			/* check for collinearity */
			if( bn_3pts_collinear( prev->pt, pt->pt, next->pt, &tol ) ) {
				/* remove middle point */
				BU_LIST_DEQUEUE( &pt->l );
				bu_free( (char *)pt, "pt_list" );
				pt = BU_LIST_NEXT( pt_list, &prev->l );
			} else {
				prev = pt;
				pt = next;
			}
			next = BU_LIST_NEXT( pt_list, &pt->l );
		}
	}

	/* refine approximation */
	while( !done ) {
		struct pt_list *cur, *new;
		double this_pt[3];

		done = 1;
		for( BU_LIST_FOR( cur, pt_list, &pt_head.l ) ) {
			if( BU_LIST_NEXT_IS_HEAD( cur, &pt_head.l ) )
				break;

			next = BU_LIST_NEXT( pt_list, &cur->l );

			/* check midpoint of span between "cur" and "next" points */
			t = 0.5 * (cur->t + next->t);

			UF_func( UF_EVAL_evaluate( evaluator, 0, t, tmp_pt, NULL ) );
			VSCALE( tmp_pt, tmp_pt, units_conv );
			MAT4X3PNT( this_pt, curr_xform, tmp_pt );

			if( bn_3pts_collinear( cur->pt, this_pt, next->pt, &tol ) ) {
				continue;
			}

			new = (struct pt_list *)bu_malloc( sizeof( struct pt_list), "struct pt_list" );
			BU_LIST_INIT( &new->l );
			new->t = t;
			VMOVE( new->pt, this_pt );
			BU_LIST_APPEND( &cur->l, &new->l );

			done = 0;
		}
	}

	/* now build primitives */
	bu_vls_init( &name_vls );
	for( BU_LIST_FOR( pt, pt_list, &pt_head.l ) ) {
		vect_t height;

		next = BU_LIST_NEXT( pt_list, &pt->l );

		if( BU_LIST_IS_HEAD( &next->l, &pt_head.l ) ) {
			break;
		}

		VSUB2( height, next->pt, pt->pt );

		prim_no++;
		bu_vls_printf( &name_vls, "s.%d", prim_no );

		outer_solid_name = create_unique_brlcad_name( &name_vls );

		if( inner_radius > 0.0 ) {
			bu_vls_strcat( &name_vls, ".i" );
			inner_solid_name = create_unique_brlcad_name( &name_vls );
		} else {
			inner_solid_name = (char *)NULL;
		}
		bu_vls_trunc( &name_vls, 0 );

		if( (is_start && BU_LIST_PREV_IS_HEAD( &pt->l, &pt_head.l )) ||
		    (is_end && BU_LIST_NEXT_IS_HEAD( &pt->l, &pt_head.l )) ) {
			if( mk_rcc( wdb_fd, outer_solid_name, pt->pt, height, outer_radius ) ) {
				UF_EVAL_free( evaluator );
				bu_log( "Failed to make RCC primitive!!!\n" );
				bu_free( outer_solid_name, "outer_solid_name" );
				if( inner_solid_name ) {
					bu_free( inner_solid_name, "inner_solid_name" );
				}
				return( 1 );
			}
			add_to_obj_list( outer_solid_name );
			if( inner_solid_name ) {
				if( mk_rcc( wdb_fd, inner_solid_name, pt->pt, height, inner_radius ) ) {
					UF_EVAL_free( evaluator );
					bu_log( "Failed to make RCC primitive!!!\n" );
					bu_free( outer_solid_name, "outer_solid_name" );
					bu_free( inner_solid_name, "inner_solid_name" );
					return( 1 );
				}
				add_to_obj_list( inner_solid_name );
			}
		} else {
			if( mk_particle( wdb_fd, outer_solid_name, pt->pt, height, outer_radius, outer_radius ) ) {
				UF_EVAL_free( evaluator );
				bu_log( "Failed to make particle primitive!!!\n" );
				bu_free( outer_solid_name, "outer_solid_name" );
				if( inner_solid_name ) {
					bu_free( inner_solid_name, "inner_solid_name" );
				}
				return( 1 );
			}
			add_to_obj_list( outer_solid_name );
			if( inner_solid_name ) {
				if( mk_particle( wdb_fd, inner_solid_name, pt->pt, height, inner_radius, inner_radius ) ) {
					UF_EVAL_free( evaluator );
					bu_log( "Failed to make particle primitive!!!\n" );
					bu_free( outer_solid_name, "outer_solid_name" );
					bu_free( inner_solid_name, "inner_solid_name" );
					return( 1 );
				}
				add_to_obj_list( inner_solid_name );
			}
		}


		(void)mk_addmember( outer_solid_name, &outer_head->l, NULL, WMOP_UNION );
		if( inner_solid_name ) {
			(void)mk_addmember( inner_solid_name, &inner_head->l, NULL, WMOP_UNION );
		}
	}

	bu_vls_free( &name_vls );
	UF_EVAL_free( evaluator );

	return( 0 );
}

int
make_linear_particle( tag_t guide_curve, fastf_t outer_diam, fastf_t inner_diam, int is_start, int is_end,
		      struct wmember *outer_head, struct wmember *inner_head, const mat_t curr_xform, double units_conv )
{
	UF_EVAL_p_t evaluator;
	double limits[2];
	double start[3], end[3];
	point_t start_f, end_f;
	vect_t height;
	fastf_t outer_radius=outer_diam/2.0;
	fastf_t inner_radius=inner_diam/2.0;
	struct bu_vls name_vls;
	char *outer_solid_name;
	char *inner_solid_name;

	UF_func( UF_EVAL_initialize( guide_curve, &evaluator ) );
	UF_func( UF_EVAL_ask_limits( evaluator, limits ) );

	UF_func( UF_EVAL_evaluate( evaluator, 0, limits[0], start, NULL ) );
	UF_func( UF_EVAL_evaluate( evaluator, 0, limits[1], end, NULL ) );

	VSCALE( start, start, units_conv );
	VSCALE( end, end, units_conv );

	MAT4X3PNT( start_f, curr_xform, start );
	MAT4X3PNT( end_f, curr_xform, end );
	VSUB2( height, end_f, start_f );

	prim_no++;
	bu_vls_init( &name_vls );
	bu_vls_printf( &name_vls, "s.%d", prim_no );

	outer_solid_name = create_unique_brlcad_name( &name_vls );

	if( inner_radius > 0.0 ) {
		bu_vls_strcat( &name_vls, ".i" );
		inner_solid_name = create_unique_brlcad_name( &name_vls );
	} else {
		inner_solid_name = (char *)NULL;
	}
	bu_vls_free( &name_vls );

	if( is_start || is_end ) {
		if( mk_rcc( wdb_fd, outer_solid_name, start_f, height, outer_radius ) ) {
			UF_EVAL_free( evaluator );
			bu_log( "Failed to make RCC primitive!!!\n" );
			bu_free( outer_solid_name, "outer_solid_name" );
			if( inner_solid_name ) {
				bu_free( inner_solid_name, "inner_solid_name" );
			}
			return( 1 );
		}
		add_to_obj_list( outer_solid_name );
		if( inner_solid_name ) {
			if( mk_rcc( wdb_fd, inner_solid_name, start_f, height, inner_radius ) ) {
				UF_EVAL_free( evaluator );
				bu_log( "Failed to make RCC primitive!!!\n" );
				bu_free( outer_solid_name, "outer_solid_name" );
				bu_free( inner_solid_name, "inner_solid_name" );
				return( 1 );
			}
			add_to_obj_list( inner_solid_name );
		}
	} else {
		if( mk_particle( wdb_fd, outer_solid_name, start_f, height, outer_radius, outer_radius ) ) {
			UF_EVAL_free( evaluator );
			bu_log( "Failed to make particle primitive!!!\n" );
			bu_free( outer_solid_name, "outer_solid_name" );
			if( inner_solid_name ) {
				bu_free( inner_solid_name, "inner_solid_name" );
			}
			return( 1 );
		}
		add_to_obj_list( outer_solid_name );
		if( inner_solid_name ) {
			if( mk_particle( wdb_fd, inner_solid_name, start_f, height, inner_radius, inner_radius ) ) {
				UF_EVAL_free( evaluator );
				bu_log( "Failed to make particle primitive!!!\n" );
				bu_free( outer_solid_name, "outer_solid_name" );
				bu_free( inner_solid_name, "inner_solid_name" );
				return( 1 );
			}
			add_to_obj_list( inner_solid_name );
		}
	}

	UF_EVAL_free( evaluator );

	(void)mk_addmember( outer_solid_name, &outer_head->l, NULL, WMOP_UNION );

	if( inner_solid_name ) {
		(void)mk_addmember( inner_solid_name, &inner_head->l, NULL, WMOP_UNION );
	}

	return( 0 );
}

static int
get_exp_value( char *want, int n_exps, tag_t *exps, char **descs, double *value )
{
	int i;

	for( i=0 ; i<n_exps ; i++ ) {
		if( !strcmp( want, descs[i] ) ) {
			/* found the wanted expression */
			UF_func( UF_MODL_ask_exp_tag_value( exps[i], value ) );
			return( 0 );
		}
	}

	return( 1 );
}


static void
get_chamfer_offsets( tag_t feat_tag, char *part_name, double units_conv, double *offset1, double *offset2 )
{
	int n_exps;
	tag_t *exps;
	char **descs;
	double tmp;

	UF_func( UF_MODL_ask_exp_desc_of_feat(feat_tag, &n_exps, &descs, &exps ) );

	if( !get_exp_value( "Offset 1", n_exps, exps, descs, &tmp ) ) {
		*offset1= tmp * units_conv;
	}

	if( !get_exp_value( "Offset 2", n_exps, exps, descs, &tmp ) ) {
		*offset2 = tmp * units_conv;
	} else {
		if( n_exps > 1 ) {
			/* must be an offset and angle chamfer
			 * we do not handle this, so fake a value to avoid suppression
			 */
			*offset2 = min_chamfer + 1.0;
		} else {
			*offset2 = *offset1;
		}
	}

	UF_free( exps );
	UF_free( descs );
}

static void
get_blend_radius( tag_t feat_tag, char *part_name, double units_conv, double *blend_radius )
{
	int n_exps;
	tag_t *exps;
	char **descs;
	double tmp;

	UF_func( UF_MODL_ask_exp_desc_of_feat(feat_tag, &n_exps, &descs, &exps ) );
	if( get_exp_value( "Default Radius", n_exps, exps, descs, &tmp ) ) {
		bu_log( "Failed to get radius for blend in part %s\n", part_name );
		UF_free( exps );
		UF_free( descs );
		bu_bomb( "Failed to get radius for blend\n" );
	}
	*blend_radius = tmp * units_conv;

	UF_free( exps );
	UF_free( descs );

}

static int
get_cylinder_data( tag_t feat_tag, int n_exps, tag_t *exps, char **descs,
		   double units_conv, const mat_t curr_xform, point_t base,
		   vect_t height, fastf_t *radius )
{
	double diam, len;
	fastf_t length;
	double location[3];
	double dir1[3], dir2[3];

	if( get_exp_value( "Diameter", n_exps, exps, descs, &diam ) ) {
		bu_log( "Failed to get diameter for Cylinder.\n" );
		return( 1 );
	}
	if( get_exp_value( "Height", n_exps, exps, descs, &len ) ) {
		bu_log( "Failed to get height for Cylinder.\n" );
		return( 1 );
	}
	*radius = diam * units_conv / 2.0;
	length = len * units_conv;

	UF_func( UF_MODL_ask_feat_location( feat_tag, location ) );
	VSCALE( location, location, units_conv );
	MAT4X3PNT( base, curr_xform, location );

	UF_func( UF_MODL_ask_feat_direction( feat_tag, dir1, dir2 ) );

	VSCALE( dir1, dir1, length );
	MAT4X3VEC( height, curr_xform, dir1 );

	return( 0 );
}

static int
get_block_data( tag_t feat_tag, int n_exps, tag_t *exps, char **descs,
		double units_conv, const mat_t curr_xform, fastf_t pts[24] )
{
	double location[3], xdir[3], ydir[3], zdir[3], tmp_x[3], tmp_y[3];
	double size[3];
	double length, width, height;

	UF_func( UF_MODL_ask_feat_location( feat_tag, location ) );
	VSCALE( location, location, units_conv );
	MAT4X3PNT( pts, curr_xform, location );

	UF_func( UF_MODL_ask_feat_direction( feat_tag, tmp_x, tmp_y ) );
	MAT4X3VEC( xdir, curr_xform, tmp_x );
	MAT4X3VEC( ydir, curr_xform, tmp_y );

	VCROSS( zdir, xdir, ydir );

	if( get_exp_value( "Size X", n_exps, exps, descs, &size[0] ) ) {
		bu_log( "Failed to get size for block.\n" );
		return( 1 );
	}
	if( get_exp_value( "Size Y", n_exps, exps, descs, &size[1] ) ) {
		bu_log( "Failed to get size for block.\n" );
		return( 1 );
	}
	if( get_exp_value( "Size Z", n_exps, exps, descs, &size[2] ) ) {
		bu_log( "Failed to get size for block.\n" );
		return( 1 );
	}

	length = size[0] * units_conv;
	width = size[1] * units_conv;
	height = size[2] * units_conv;

	VJOIN1( &pts[3], pts, length, xdir );
	VJOIN1( &pts[6], &pts[3], height, zdir );
	VJOIN1( &pts[9], pts, height, zdir );
	VJOIN1( &pts[12], pts, width, ydir );
	VJOIN1( &pts[15], &pts[12], length, xdir );
	VJOIN1( &pts[18], &pts[15], height, zdir );
	VJOIN1( &pts[21], &pts[12], height, zdir );

	return( 0 );
}

static int
get_sphere_data( tag_t feat_tag, int n_exps, tag_t *exps, char **descs,
		 double units_conv, const mat_t curr_xform, point_t center, fastf_t *radius )
{
	double location[3];
	double diameter;

	if( get_exp_value( "Diameter", n_exps, exps, descs, &diameter ) ) {
		bu_log( "Failed to get diameter for sphere\n" );
		return( 1 );
	}

	UF_func( UF_MODL_ask_feat_location( feat_tag, location ) );
	VSCALE( location, location, units_conv );
	MAT4X3PNT( center, curr_xform, location );


	*radius = diameter * units_conv / 2.0;

	return( 0 );
}


static int
get_cone_data( tag_t feat_tag, int n_exps, tag_t *exps, char **descs, double units_conv,
	       const mat_t curr_xform, point_t base, vect_t dirv, fastf_t *height,
	       fastf_t *radbase, fastf_t *radtop)
{

	double location[3];
	double dir1[3], dir2[3];
	double base_diam=-1.0, top_diam=-1.0, half_angle=-1.0, ht=-1.0;

	(void)get_exp_value( "Base Diameter", n_exps, exps, descs, &base_diam );
	(void)get_exp_value( "Top Diameter", n_exps, exps, descs, &top_diam );
	(void)get_exp_value( "Half Angle", n_exps, exps, descs, &half_angle );
	(void)get_exp_value( "Height", n_exps, exps, descs, &ht );

	if( half_angle > 0.0 ) {
		half_angle *= M_PI / 180.0;
	} else {
		half_angle = atan2( (base_diam - top_diam)/2.0, ht );
	}

	if( top_diam < 0.0 ) {
		top_diam = base_diam - 2.0 * ht * tan( half_angle );
	}

	if( base_diam < 0.0 ) {
		base_diam = top_diam + 2.0 * ht * tan( half_angle );
	}

	if( ht < 0.0 ) {
		ht = (base_diam - top_diam)/(2.0 * tan( half_angle ) );
	}

	if( base_diam < 0.0 || top_diam < 0.0 || ht < 0.0 ) {
		return( 1 );
	}

	*radbase = base_diam * units_conv / 2.0;
	if( *radbase < RT_LEN_TOL ) {
		*radbase = RT_LEN_TOL;
	}

	*radtop = top_diam * units_conv / 2.0;
	if( *radtop < RT_LEN_TOL ) {
		*radtop = RT_LEN_TOL;
	}

	*height = ht * units_conv;

	UF_func( UF_MODL_ask_feat_location( feat_tag, location ) );
	VSCALE( location, location, units_conv );
	MAT4X3PNT( base, curr_xform, location );

	UF_func( UF_MODL_ask_feat_direction( feat_tag, dir1, dir2 ) );
	MAT4X3VEC( dirv, curr_xform, dir1 );

	return( 0 );

}

#define VERT_ALLOC_BLOCK 32

static int
add_sketch_vert( double pt[3], struct rt_sketch_internal *skt, int *verts_alloced, double tol_sq )
{
	int i;
	point2d_t diff;

	for( i=0 ; i<skt->vert_count ; i++ ) {

		V2SUB2( diff, pt, skt->verts[i] );
		if( MAG2SQ( diff ) < tol_sq ) {
			return( i );
		}
	}

	if( skt->vert_count >= *verts_alloced ) {
		*verts_alloced += VERT_ALLOC_BLOCK;
		bu_log( "Allocating %d vertices\n", *verts_alloced );
		skt->verts = (point2d_t *)bu_realloc( skt->verts, *verts_alloced*sizeof(point2d_t ), "skt->verts" );
	}
	V2MOVE( skt->verts[skt->vert_count], pt );
	bu_log( "new vertex #%d is (%g %g)\n", skt->vert_count, V2ARGS( skt->verts[skt->vert_count] ) );
	skt->vert_count++;

	return( skt->vert_count - 1 );
}

static char *
conv_extrusion( tag_t feat_tag, char *part_name, char *refset_name, char *inst_name, unsigned char *rgb, const mat_t curr_xform,
		double units_conv, int make_region, int num_exp, tag_t *exps, char **descs,
	    int n_guide_curves, tag_t *guide_curves, int n_profile_curves, tag_t *profile_curves, double tol_dist )
{
	char *sketch_name;
	char *solid_name;
	int num_curves;
	tag_t *curves;
	UF_MODL_SWEEP_TRIM_object_p_t trim;
	char *ta, *limits[2], *offsets[2];
	double pos[3], dir[3];
	logical region_desired, solid_body;
	fastf_t taper_angle, dist1, dist2;
	struct rt_sketch_internal *skt;
	struct UF_CURVE_line_s line_data;
	struct UF_CURVE_arc_s arc_data;
	int verts_alloced=0;
	int i,j;
	struct rt_db_internal intern;
	uf_list_p_t sketch_list;
	int num_sketches=0;
	int type, subtype;
	int seg_count=0;
	double csys[12];
	UF_SKET_info_t skt_info;
	tag_t sketch_tag;
	vect_t extrude_dir;
	point_t extrude_base, tmp_pt;
	vect_t extrude_vect, extrude_uvec, extrude_vvec;
	double arc_angle_m_2pi;
	double tol_sq=tol_dist*tol_dist;
	double *z_coords;
	double tmp;
	char skt_name[31];
	struct bu_vls sketch_vls;
	char *c;
	int code;

	if( UF_MODL_ask_extrusion( feat_tag, &num_curves, &curves, &trim, &ta, limits, offsets,
				   pos, &region_desired, &solid_body, dir ) ) {
		bu_log( "This is probably not an extrusion\n" );
		return( (char *)NULL );
	}

	bu_log( "Extrusion: pos = (%g %g %g)\n", V3ARGS( pos ) );
	VSCALE( pos, pos, units_conv );
	MAT4X3PNT( tmp_pt, curr_xform, pos );
	bu_log( "\tafter conversion pos = (%g %g %g)\n", V3ARGS( tmp_pt ) );

	bu_log( "%d curves\n", num_curves );
	for( i=0 ; i<num_curves ; i++ ) {
		bu_log( "curve #%d is tag %d\n", i, curves[i] );
	}

	bu_log( "offsets = :%s: :%s:\n", offsets[0], offsets[1] );
	if( strcmp( offsets[0], "0.0" ) || strcmp( offsets[1], "0.0" ) ) {
		bu_log( "Cannot handle offset extrusions yet\n" );
		UF_free( ta );
		UF_free( limits[0] );
		UF_free( limits[1] );
		UF_free( offsets[0] );
		UF_free( offsets[1] );
		UF_free( curves );

		return( (char *)NULL );
	}

	UF_free( ta );
	UF_free( limits[0] );
	UF_free( limits[1] );
	UF_free( offsets[0] );
	UF_free( offsets[1] );

	/* these are not 'parameters', so cannot use 'get_ug_double' */
	if( get_exp_value( "Limit 1", num_exp, exps, descs, &tmp ) ) {
		bu_log( "Failed to get limit 1 for extrusion\n" );
		return( (char *)NULL );
	}
	dist1 = tmp * units_conv;

	if( get_exp_value( "Limit 2", num_exp, exps, descs, &tmp ) ) {
		bu_log( "Failed to get limit 2 for extrusion\n" );
		return( (char *)NULL );
	}
	dist2 = tmp * units_conv;

	if( get_exp_value( "Taper Angle", num_exp, exps, descs, &tmp ) ) {
		bu_log( "Failed to get taper angle for extrusion\n" );
		return( (char *)NULL );
	}
	taper_angle = tmp * M_PI / 180.0;

	if( taper_angle != 0.0 ) {
		bu_log( "Cannot handle tapered extrusions yet\n" );
		UF_free( curves );
		return( (char *)NULL );
	}

	UF_func( UF_SKET_ask_feature_sketches( feat_tag, &sketch_list ) );
	UF_func( UF_MODL_ask_list_count( sketch_list, &num_sketches ) );
	DO_INDENT;
	bu_log( "\t%d sketches\n", num_sketches );
	if( num_sketches != 1 ) {
		bu_log( "Extrusion (%s) has too many sketches (%d)\n", part_name, num_sketches );
		UF_MODL_delete_list( &sketch_list );
		return( (char *)NULL );
	}

	DO_INDENT;
	bu_log( "\t found a linear extrusion\n" );
	MAT4X3VEC( extrude_dir, curr_xform, dir );
	DO_INDENT;
	bu_log( "\t dir = (%g %g %g), dist1 = %g, dist2 = %g\n", V3ARGS( extrude_dir ), dist1, dist2 );
	UF_func( UF_MODL_ask_list_item( sketch_list, 0, &sketch_tag ) );
	if( sketch_tag < 1 ) {
		bu_log( "Illegal tag for sketch (%d)\n", sketch_tag );
		UF_MODL_delete_list( &sketch_list );
		return( (char *)NULL );
	}

	UF_func( UF_OBJ_ask_type_and_subtype( sketch_tag, &type, &subtype));
	DO_INDENT;
	bu_log( "sketch_tag = %d is type %s\n", sketch_tag, lookup_ug_type( type, NULL ) );
	code = UF_SKET_ask_sketch_info( sketch_tag, &skt_info );
	if( code ) {
		char message[133]="";

		if( code == 650004 ) {
			bu_log( "Got a %d error from UF_SKET_ask_sketch_info() sketch_name = %s (ignoring the error)\n", code, skt_info.name );
			for( i=0 ; i<12 ; i+= 3 ) {
				bu_log( "%g %g %g\n", V3ARGS( &skt_info.csys[i] ) );
			}

		} else {

			bu_log( "Apparrently, a sketch(%d) is not a sketch\n", sketch_tag );
			UF_free( curves );
			UF_MODL_delete_list( &sketch_list );

			if (UF_get_fail_message(code, message)) {
				fprintf(stderr, "UF_SKET_ask_sketch_info() failed with error code %d\n", code );
			} else {
				fprintf(stderr, "UF_SKET_ask_sketch_info() failed with error %s\n", message );
			}
			return( (char *)NULL );
		}
	}

	strcpy( skt_name, skt_info.name );

	seg_count = num_curves;

	skt = (struct rt_sketch_internal *)bu_calloc( 1, sizeof( struct rt_sketch_internal ), "sketch" );
	skt->magic = RT_SKETCH_INTERNAL_MAGIC;
	skt->skt_curve.seg_count = seg_count;
	skt->skt_curve.reverse = (int *)bu_calloc( seg_count, sizeof( int ), "sketch reverse flags" );
	skt->skt_curve.segments = (genptr_t *)bu_calloc( seg_count, sizeof( genptr_t ), "sketch segment pointers" );
	skt->vert_count = 0;
	skt->verts = (point2d_t *)bu_calloc( VERT_ALLOC_BLOCK, sizeof( point2d_t ), "skt->verts" );
	verts_alloced = VERT_ALLOC_BLOCK;

	DO_INDENT;
	bu_log( "Sketch (%d):\n", feat_tag );
	DO_INDENT;
	bu_log( "\tname = %s\n", skt_name );
	DO_INDENT;
	bu_log( "\tcsys:\n" );
	for( j=0 ; j<12 ; j += 3 ) {
		VMOVE( &csys[j],  &skt_info.csys[j] );
		DO_INDENT;
		bu_log( "\t\t%g\t%g\t%g\n", V3ARGS( &skt_info.csys[j] ) );
	}

	bn_mat_print( "curr_xform", curr_xform );

	MAT3X3VEC( skt->u_vec, curr_xform, &csys[0] );
	MAT3X3VEC( skt->v_vec, curr_xform, &csys[3] );
	VSCALE( tmp_pt, &csys[9], units_conv );
	MAT4X3PNT( skt->V, curr_xform, tmp_pt );

	z_coords = (double *)bu_calloc( num_curves, sizeof( double ), "z_coords" );
	for( j=0 ; j<num_curves ; j++ ) {
		struct line_seg *lsg;
		struct carc_seg *csg;
		point_t start, end;
		point_t pt;
		vect_t to_end, to_center, cross;
		double z1, z2;

		bu_log( "curve #%d is tag %d\n", j, curves[j] );
		if( curves[j] < 1 ) {
			bu_log( "Illegal tag for curve (%d)\n", curves[j] );
			bu_free( (char *)z_coords, "z_coords" );
			for( i=j+1 ; i<num_curves ; i++ ) {
				lsg = (struct line_seg *)bu_malloc( sizeof( struct line_seg ), "fake line seg" );
				lsg->magic = CURVE_LSEG_MAGIC;
				skt->skt_curve.segments[i] = (genptr_t)lsg;
			}
			intern.idb_magic = RT_DB_INTERNAL_MAGIC;
			intern.idb_major_type = DB5_MAJORTYPE_BRLCAD;
			intern.idb_minor_type = DB5_MINORTYPE_BRLCAD_SKETCH;
			intern.idb_meth = &rt_functab[ID_SKETCH];
			intern.idb_ptr = (genptr_t)skt;
			bu_avs_init_empty( &intern.idb_avs );
			rt_sketch_ifree( &intern );
			UF_MODL_delete_list( &sketch_list );
			UF_free( curves );
			return( (char *)NULL );
		}
		UF_func( UF_OBJ_ask_type_and_subtype( curves[j], &type, &subtype));
		switch( type ) {
			case UF_line_type:
				UF_func( UF_CURVE_ask_line_data( curves[j], &line_data ) );
				DO_INDENT;
				bu_log( "Line from (%g %g %g) to (%g %g %g)\n",
					V3ARGS( line_data.start_point ), V3ARGS( line_data.end_point ) );
				lsg = (struct line_seg *)bu_malloc( sizeof( struct line_seg ), "line seg" );
				skt->skt_curve.segments[j] = (genptr_t)lsg;
				lsg->magic = CURVE_LSEG_MAGIC;
				UF_MTX3_vec_multiply( line_data.start_point, csys, pt );
				VSCALE( pt, pt, units_conv );
				z1 = pt[Z];
				lsg->start = add_sketch_vert( pt, skt, &verts_alloced, tol_sq );
				UF_MTX3_vec_multiply( line_data.end_point, csys, pt );
				VSCALE( pt, pt, units_conv );
				z2 = pt[Z];
				lsg->end = add_sketch_vert( pt, skt, &verts_alloced, tol_sq );
				if( !NEAR_ZERO( fabs( z1 - z2 ), tol_dist ) ) {
					bu_log( "Sketch (%s) for part %s is not planar, cannot handle this",
						skt_name, part_name );
					/* for simplicity, malloc up the rest of a sketch internal object for freeing */
					bu_free( (char *)z_coords, "z_coords" );
					for( i=j+1 ; i<num_curves ; i++ ) {
						lsg = (struct line_seg *)bu_malloc( sizeof( struct line_seg ), "fake line seg" );
						lsg->magic = CURVE_LSEG_MAGIC;
						skt->skt_curve.segments[i] = (genptr_t)lsg;
					}
					intern.idb_magic = RT_DB_INTERNAL_MAGIC;
					intern.idb_major_type = DB5_MAJORTYPE_BRLCAD;
					intern.idb_minor_type = DB5_MINORTYPE_BRLCAD_SKETCH;
					intern.idb_meth = &rt_functab[ID_SKETCH];
					intern.idb_ptr = (genptr_t)skt;
					bu_avs_init_empty( &intern.idb_avs );
					rt_sketch_ifree( &intern );
					UF_MODL_delete_list( &sketch_list );
					UF_free( curves );
					return( (char *)NULL );
				}
				z_coords[j] = z1;
				break;
			case UF_circle_type:
				UF_func( UF_CURVE_ask_arc_data( curves[j], &arc_data ) );
				csg = (struct carc_seg *)bu_malloc( sizeof( struct carc_seg ), "carc seg" );
				DO_INDENT;
				bu_log( "Arc centered at (%g %g %g), start angle = %g end angle = %g, radius = %g\n",
					V3ARGS( arc_data.arc_center ), arc_data.start_angle*180.0/M_PI,
					arc_data.end_angle*180.0/M_PI, arc_data.radius );
				csg->magic = CURVE_CARC_MAGIC;
				csg->radius = arc_data.radius * units_conv;
				if( arc_data.end_angle > arc_data.start_angle ) {
					csg->orientation = 0;
				} else {
					csg->orientation = 1;
				}

				start[0] = arc_data.arc_center[0] + arc_data.radius * cos( arc_data.start_angle );
				start[1] = arc_data.arc_center[1] + arc_data.radius * sin( arc_data.start_angle );
				start[2] = arc_data.arc_center[2];
				arc_angle_m_2pi = fabs( arc_data.end_angle - arc_data.start_angle ) - 2.0 * M_PI;
				if( NEAR_ZERO( arc_angle_m_2pi, 0.0005)  ) {
					/* full circle */
					csg->radius = -csg->radius;
					csg->center_is_left = 1;
					VSCALE( start, start, units_conv );
					z1 = start[Z];
					csg->start = add_sketch_vert( start, skt, &verts_alloced, tol_sq );
					VSCALE( end, arc_data.arc_center, units_conv );
					z2 = end[Z];
					csg->end = add_sketch_vert( end, skt, &verts_alloced, tol_sq );
				} else {
					/* arc */
					end[0] = arc_data.arc_center[0] + arc_data.radius * cos( arc_data.end_angle );
					end[1] = arc_data.arc_center[1] + arc_data.radius * sin( arc_data.end_angle );
					end[2] = arc_data.arc_center[2];
					VSUB2( to_end, end, start );
					VSUB2( to_center, arc_data.arc_center, start );
					VCROSS( cross, to_end, to_center );
					if( cross[Z] > 0.0 ) {
						csg->center_is_left = 1;
					} else {
						csg->center_is_left = 0;
					}
					VSCALE( start, start, units_conv );
					z1 = start[Z];
					csg->start = add_sketch_vert( start, skt, &verts_alloced, tol_sq );
					VSCALE( end, end, units_conv );
					z2 = end[Z];
					csg->end = add_sketch_vert( end, skt, &verts_alloced, tol_sq );
				}
				skt->skt_curve.segments[j] = (genptr_t)csg;
				if( !NEAR_ZERO( fabs( z1 - z2 ), tol_dist ) ) {
					bu_log( "Sketch (%s) for part %s is not planar, cannot handle this",
						skt_name, part_name );
					/* for simplicity, malloc up the rest of a sketch internal object for freeing */
					bu_free( (char *)z_coords, "z_coords" );
					for( i=j+1 ; i<num_curves ; i++ ) {
						lsg = (struct line_seg *)bu_malloc( sizeof( struct line_seg ), "fake line seg" );
						lsg->magic = CURVE_LSEG_MAGIC;
						skt->skt_curve.segments[i] = (genptr_t)lsg;
					}
					intern.idb_magic = RT_DB_INTERNAL_MAGIC;
					intern.idb_major_type = DB5_MAJORTYPE_BRLCAD;
					intern.idb_minor_type = DB5_MINORTYPE_BRLCAD_SKETCH;
					intern.idb_meth = &rt_functab[ID_SKETCH];
					intern.idb_ptr = (genptr_t)skt;
					bu_avs_init_empty( &intern.idb_avs );
					rt_sketch_ifree( &intern );
					UF_MODL_delete_list( &sketch_list );
					UF_free( curves );
					return( (char *)NULL );
				}
				z_coords[j] = z1;
				break;
			default:
				bu_log( "Cannot yet handle curves of type %s\n", lookup_ug_type( type, NULL ) );
				/* for simplicity, malloc up the rest of a sketch internal object for freeing */
				bu_free( (char *)z_coords, "z_coords" );
				for( i=j ; i<num_curves ; i++ ) {
					lsg = (struct line_seg *)bu_malloc( sizeof( struct line_seg ), "fake line seg" );
					lsg->magic = CURVE_LSEG_MAGIC;
					skt->skt_curve.segments[i] = (genptr_t)lsg;
				}
				intern.idb_magic = RT_DB_INTERNAL_MAGIC;
				intern.idb_major_type = DB5_MAJORTYPE_BRLCAD;
				intern.idb_minor_type = DB5_MINORTYPE_BRLCAD_SKETCH;
				intern.idb_meth = &rt_functab[ID_SKETCH];
				intern.idb_ptr = (genptr_t)skt;
				bu_avs_init_empty( &intern.idb_avs );
				rt_sketch_ifree( &intern );
				UF_MODL_delete_list( &sketch_list );
				UF_free( curves );
				return( (char *)NULL );
		}
	}

	UF_free( curves );

	UF_MODL_delete_list( &sketch_list );

	for( j=1 ; j<num_curves ; j++ ) {
		struct line_seg *lsg;

		i = j - 1;

		if( !NEAR_ZERO( fabs( z_coords[i] - z_coords[j] ), tol_dist ) ) {
			bu_log( "Sketch (%s) for part %s is not planar, cannot handle this",
				skt_name, part_name );
			/* for simplicity, malloc up the rest of a sketch internal object for freeing */
			bu_free( (char *)z_coords, "z_coords" );
			for( i=j+1 ; i<num_curves ; i++ ) {
				lsg = (struct line_seg *)bu_malloc( sizeof( struct line_seg ), "fake line seg" );
				lsg->magic = CURVE_LSEG_MAGIC;
				skt->skt_curve.segments[i] = (genptr_t)lsg;
			}
			intern.idb_magic = RT_DB_INTERNAL_MAGIC;
			intern.idb_major_type = DB5_MAJORTYPE_BRLCAD;
			intern.idb_minor_type = DB5_MINORTYPE_BRLCAD_SKETCH;
			intern.idb_meth = &rt_functab[ID_SKETCH];
			intern.idb_ptr = (genptr_t)skt;
			bu_avs_init_empty( &intern.idb_avs );
			rt_sketch_ifree( &intern );
			return( (char *)NULL );
		}

	}

	if( z_coords[0] != 0.0 ) {
		point_t pt;

		bu_log( "z-coord = %g\n", z_coords[0] );
		/* move sketch base point */
		VJOIN1( pt, &csys[9], z_coords[0], &csys[6] );
		bu_log( "pt = (%g %g %g), new sketch V = (%g %g %g)\n", V3ARGS( pt ), V3ARGS( skt->V ) );
		MAT4X3PNT( skt->V, curr_xform, pt );
	}

	rt_curve_order_segments( &skt->skt_curve );

	bu_vls_init( &sketch_vls );
	bu_vls_strcat( &sketch_vls, skt_name );
	sketch_name = create_unique_brlcad_name( &sketch_vls );
	c = sketch_name;
	while( *c ) {
		if( isspace( *c ) || *c == '/' ) {
			*c = '_';
		}
		c++;
	}
	VMOVE( extrude_base, skt->V );
	VMOVE( extrude_uvec, skt->u_vec );
	VMOVE( extrude_vvec, skt->v_vec );

	/* mk_sketch() frees the "skt" */
	if( mk_sketch( wdb_fd, sketch_name, skt ) ) {
		bu_log( "Failed to create sketch for extrusion (%s)\n", part_name );
		bu_free( (char *)z_coords, "z_coords" );
		bu_free( ( char *)sketch_name, "sketch name" );
		intern.idb_magic = RT_DB_INTERNAL_MAGIC;
		intern.idb_major_type = DB5_MAJORTYPE_BRLCAD;
		intern.idb_minor_type = DB5_MINORTYPE_BRLCAD_SKETCH;
		intern.idb_meth = &rt_functab[ID_SKETCH];
		intern.idb_ptr = (genptr_t)skt;
		bu_avs_init_empty( &intern.idb_avs );
		rt_sketch_ifree( &intern );
		return( (char *)NULL );
	}
	add_to_obj_list( sketch_name );

	VJOIN1( extrude_base, extrude_base, dist1, extrude_dir );
	VSCALE( extrude_vect, extrude_dir, (dist2 - dist1 ) );
	solid_name = create_unique_brlcad_solid_name();
	if( mk_extrusion( wdb_fd, solid_name, sketch_name, extrude_base, extrude_vect, extrude_uvec, extrude_vvec, 0 ) ) {
		bu_free( (char *)z_coords, "z_coords" );
		bu_log( "Failed to create extrusion for part %s\n", part_name );
		bu_free( sketch_name, "sketch name" );
		bu_free( solid_name, "solid name" );
		return( (char *)NULL );
	}

	bu_free( (char *)z_coords, "z_coords" );
	return( solid_name );
}

static char *
conv_cable( char *part_name, char *refset_name, char *inst_name, unsigned char *rgb, const mat_t curr_xform,
	    double units_conv, int make_region, int num_exp, tag_t *exps,
	    int n_guide_curves, tag_t *guide_curves)
{
	int type, subtype;
	char *region_name, *outer_name, *inner_name;
	struct wmember head_outer, head_inner;
	fastf_t outer_diam=0.0;
	fastf_t inner_diam=0.0;
	struct bu_vls region_name_vls, outer_name_vls, inner_name_vls;
	int i;

	DO_INDENT;
	bu_log( "Found %d expressions\n", num_exp );
	UF_func( UF_MODL_ask_exp_tag_value( exps[0], &outer_diam ) );
	outer_diam *= units_conv;
	UF_func( UF_MODL_ask_exp_tag_value( exps[1], &inner_diam ) );
	inner_diam *= units_conv;

	DO_INDENT;
	bu_log( "Converted diameters; %g %g\n", outer_diam, inner_diam );

	BU_LIST_INIT( &head_outer.l );
	BU_LIST_INIT( &head_inner.l );
	/* build the primitives */
	for( i=0 ; i<n_guide_curves ; i++ ) {
		int start=0, end=0;

		if( i == 0 )
			start = 1;
		if( i == n_guide_curves-1 )
			end = 1;

		UF_func(UF_OBJ_ask_type_and_subtype(guide_curves[i], &type, &subtype));

		switch( type ) {
			case UF_line_type:
				if( make_linear_particle( guide_curves[i], outer_diam, inner_diam, start, end,
							  &head_outer, &head_inner, curr_xform, units_conv ) ) {
					return( (char *)NULL );
				}
				break;
			default:
				if( make_curve_particles( guide_curves[i], outer_diam, inner_diam, start, end,
							   &head_outer, &head_inner, curr_xform, units_conv ) ) {
					return( (char *)NULL );
				}
				break;
		}
	}

	bu_vls_init( &region_name_vls );
	bu_vls_init( &outer_name_vls );
	bu_vls_init( &inner_name_vls );
	if( inst_name ) {
		bu_vls_strcat( &region_name_vls, inst_name );
	} else {
		char *ptr;

		ptr = strrchr( part_name, '/' );
		if( ptr ) {
			ptr++;
			bu_vls_strcat( &region_name_vls, ptr );
		} else {
			bu_vls_strcat( &region_name_vls, part_name );
		}
	}

	get_part_name( &region_name_vls );
	if( inner_diam > 0.0 ) {
		bu_vls_vlscat( &outer_name_vls, &region_name_vls );
		bu_vls_strcat( &outer_name_vls, "_outer" );
		bu_vls_vlscat( &inner_name_vls, &region_name_vls );
		bu_vls_strcat( &inner_name_vls, "_inner" );
	}

	if( refset_name && strcmp( refset_name, "None" ) ) {
		bu_vls_strcat( &region_name_vls, "_" );
		bu_vls_strcat( &region_name_vls, refset_name );
	}
	bu_vls_strcat( &region_name_vls, ".r" );
	region_name = create_unique_brlcad_name( &region_name_vls );
	bu_vls_free( &region_name_vls );

	if( inner_diam > 0.0 ) {
		outer_name = create_unique_brlcad_name( &outer_name_vls );
		bu_vls_free( &outer_name_vls );
		inner_name = create_unique_brlcad_name( &inner_name_vls );
		bu_vls_free( &inner_name_vls );
	}

	if( inner_diam > 0.0 ) {
		struct wmember head_region;

		/* make outer and inner combinations, and do the subtraction in the region */
		(void)mk_comb( wdb_fd, outer_name, &head_outer.l, 0, NULL, NULL, NULL, 0, 0, 0, 0, 0, 0, 0 );
		add_to_obj_list( outer_name );
		(void)mk_comb( wdb_fd, inner_name, &head_inner.l, 0, NULL, NULL, NULL, 0, 0, 0, 0, 0, 0, 0 );
		add_to_obj_list( inner_name );

		BU_LIST_INIT( &head_region.l );
		(void)mk_addmember( outer_name, &head_region.l, NULL, WMOP_UNION );
		(void)mk_addmember( inner_name, &head_region.l, NULL, WMOP_SUBTRACT );
		if( make_region ) {
			(void)mk_comb( wdb_fd, region_name, &head_region.l, 1, NULL, NULL, rgb, ident++, 0, 1, 100, 0, 0, 0 );
		} else {
			(void)mk_comb( wdb_fd, region_name, &head_region.l, 0, NULL, NULL, NULL, 0, 0, 0, 0, 0, 0, 0 );
		}
	} else {
		/* cable is solid, no need for intermediate combinations */
		if( make_region ) {
			(void)mk_comb( wdb_fd, region_name, &head_outer.l, 1, NULL, NULL, rgb, ident++, 0, 1, 100, 0, 0, 0 );
		} else {
			(void)mk_comb( wdb_fd, region_name, &head_outer.l, 0, NULL, NULL, NULL, 0, 0, 0, 0, 0, 0, 0 );
		}
	}

	return( region_name );
}

static char *
convert_sweep( tag_t feat_tag, char *part_name, char *refset_name, char *inst_name, unsigned char *rgb,
	       const mat_t curr_xform, double units_conv, int make_region )
{
	int n_profile_curves, n_guide_curves;
	tag_t *profile_curves, *guide_curves;
	int num_exp;
	tag_t *exps;
	char **descs;
	char *solid_name=(char *)NULL;
	int i;

	/* this is a sweep */
	DO_INDENT;
	bu_log( "Found a sweep\n" );

	UF_func( UF_MODL_ask_sweep_curves( feat_tag, &n_profile_curves, &profile_curves,
					   &n_guide_curves, &guide_curves ) );

	UF_func( UF_MODL_ask_exp_desc_of_feat(feat_tag, &num_exp, &descs, &exps ) );

	bu_log( "sweep has %d profile curves, %d guide curves, and %d expressions\n", n_profile_curves, n_guide_curves, num_exp );

	for( i=0 ; i<num_exp ; i++ ) {
		bu_log( "sweep expression #%d - %s\n", i+1, descs[i] );
	}
	if( num_exp == 2 && n_profile_curves == 0 ) {
		/* this is a tube or cable */

		solid_name = conv_cable( part_name, refset_name, inst_name, rgb, curr_xform, units_conv, make_region,
					 num_exp, exps, n_guide_curves, guide_curves);
	} else if( n_guide_curves == 0 ) {
		/* this may be a linear extrusion */
#if 0
		solid_name = (char *)NULL;
#else
		solid_name = conv_extrusion( feat_tag, part_name, refset_name, inst_name, rgb, curr_xform, units_conv, make_region,
					  num_exp, exps, descs, n_guide_curves, guide_curves, n_profile_curves, profile_curves, tol_dist );
#endif
	}

	UF_free( exps );
	UF_free( descs );
	if( n_profile_curves )
		UF_free( profile_curves );
	if( n_guide_curves )
		UF_free( guide_curves );


	return( solid_name );

}

static fastf_t
get_thru_faces_length( tag_t feat_tag,
		       double base[3],
		       double dir[3] )
{
	int i;
	tag_t face1, face2;
	double bb[6];
	fastf_t max_len, min_len, length;
	fastf_t max_entr, min_exit;

	/* get thru face */
	UF_func( UF_MODL_ask_thru_faces( feat_tag, &face1, &face2 ) );

	/* get bounding box of thru face */
	if( UF_MODL_ask_bounding_box( face1, bb ) ) {
		bu_log( "Failed to get bounding box for face %d\n", face1 );
		return( -1.0 );
	}

	DO_INDENT;
	bu_log( "get_thru_faces_length(): base = (%g %g %g), dir = (%g %g %g)\n",
		V3ARGS( base ), V3ARGS( dir ) );
	DO_INDENT;
	bu_log( "\tface1 = %d, face2 = %d\n", face1, face2 );
	DO_INDENT;
	bu_log( "\tface1 bb = (%g %g %g) <-> (%g %g %g)\n", V3ARGS( bb ) , V3ARGS( &bb[3] ) );

	/* calculate length needed to reach furthest point of bounding box */
	min_len = MAX_FASTF;
	max_len = -min_len;
	min_exit = MAX_FASTF;
	max_entr = -min_exit;
	for( i=X ; i<=Z ; i++ ) {
		plane_t pl;
		int ret;
		fastf_t dist;

		VSETALLN( pl, 0.0, 4 );
		pl[i] = 1.0;
		pl[3] = bb[i+3];
		DO_INDENT;
		bu_log( "\tChecking plane (%g %g %g %g)\n", V4ARGS( pl ) );
		ret = bn_isect_line3_plane( &dist, base, dir, pl, &tol );
		DO_INDENT;
		bu_log( "ret = %d, dist = %g\n", ret, dist );
		/* 1 - exit, 2 - entrance, else miss */
		if( ret == 1 ) {
			if( dist < min_exit ) {
				min_exit = dist;
			}
		} else if( ret ==2 ) {
			if( dist > max_entr ) {
				max_entr = dist;
			}
		}

		VSETALLN( pl, 0.0, 4 );
		pl[i] = -1.0;
		pl[3] = -bb[i];
		DO_INDENT;
		bu_log( "\tChecking plane (%g %g %g %g)\n", V4ARGS( pl ) );
		ret = bn_isect_line3_plane( &dist, base, dir, pl, &tol );
		DO_INDENT;
		bu_log( "ret = %d, dist = %g\n", ret, dist );
		/* 1 - exit, 2 - entrance, else miss */
		if( ret == 1 ) {
			if( dist < min_exit ) {
				min_exit = dist;
			}
		} else if( ret ==2 ) {
			if( dist > max_entr ) {
				max_entr = dist;
			}
		}
	}

	if( min_exit < min_len ) {
		min_len = min_exit;
	}
	if( max_entr < min_len ) {
		min_len = max_entr;
	}

	if( min_exit > max_len ) {
		max_len = min_exit;
	}
	if( max_entr > max_len ) {
		max_len = max_entr;
	}

	if( face2 ) {
		if( UF_MODL_ask_bounding_box( face2, bb ) ) {
			bu_log( "Failed to get bounding box for face %d\n", face2 );
			return( -1.0 );
		}

		/* calculate length needed to reach furthest point of bounding box */
		min_exit = MAX_FASTF;
		max_entr = -min_exit;
		for( i=X ; i<=Z ; i++ ) {
			plane_t pl;
			int ret;
			fastf_t dist;

			VSETALLN( pl, 0.0, 4 );
			pl[i] = 1.0;
			pl[3] = bb[i+3];
			ret = bn_isect_line3_plane( &dist, base, dir, pl, &tol );
			/* 1 - exit, 2 - entrance, else miss */
			if( ret == 1 ) {
				if( dist < min_exit ) {
					min_exit = dist;
				}
			} else if( ret ==2 ) {
				if( dist > max_entr ) {
					max_entr = dist;
				}
			}

			VSETALLN( pl, 0.0, 4 );
			pl[i] = -1.0;
			pl[3] = -bb[i];
			ret = bn_isect_line3_plane( &dist, base, dir, pl, &tol );
			/* 1 - exit, 2 - entrance, else miss */
			if( ret == 1 ) {
				if( dist < min_exit ) {
					min_exit = dist;
				}
			} else if( ret ==2 ) {
				if( dist > max_entr ) {
					max_entr = dist;
				}
			}
		}

		if( min_exit < min_len ) {
			min_len = min_exit;
		}
		if( max_entr < min_len ) {
			min_len = max_entr;
		}

		if( min_exit > max_len ) {
			max_len = min_exit;
		}
		if( max_entr > max_len ) {
			max_len = max_entr;
		}
	}


	if( face2 ) {
		length = max_len - min_len;
		/* occaisionally UG places the "base" at an unreasonable position along the dir direction
		 * move it to the midpoint along the dir direction
		 */

		VJOIN1( base, base, (max_len + min_len)/2.0, dir );

		return( length );
	} else {
		return( max_len > min_len ? max_len : min_len );
	}
}

int
do_hole( int hole_type, tag_t feat_tag, int n_exps, tag_t *exps, char ** descs, double units_conv,
	 const mat_t curr_xform, struct wmember *head )
{
	char *diam, *depth, *angle, *cb_diam, *cb_depth, *cs_diam, *cs_angle;
	int thru_flag;
	double tmp;
	double loc_orig[3];
	double location[3], dir1[3], dir2[3];
	fastf_t Radius, Tip_angle, Depth;
	vect_t dir, height;
	point_t base;
	char *solid_name;
	fastf_t CB_depth, CB_radius;
	fastf_t CS_radius, CS_angle, CS_depth;

	UF_func( UF_MODL_ask_feat_location( feat_tag, loc_orig ) );
	VSCALE( location, loc_orig, units_conv );
	MAT4X3PNT( base, curr_xform, location );
	UF_func( UF_MODL_ask_feat_direction( feat_tag, dir1, dir2 ) );
	MAT4X3VEC( dir, curr_xform, dir1 );

	DO_INDENT;
	bu_log( "Hole is at %g %g %g, direction is %g %g %g\n", V3ARGS( base ), V3ARGS( dir ) );

	switch( hole_type ) {
		case SIMPLE_HOLE_TYPE:
			UF_func( UF_MODL_ask_simple_hole_parms( feat_tag, 0, &diam, &depth, &angle, &thru_flag ) );
			UF_free( diam );
			UF_free( depth );
			UF_free( angle );
			if( get_exp_value( "Diameter", n_exps, exps, descs, &tmp ) ) {
				bu_log( "Failed to get diameter for simple hole.\n" );
				return( 1 );
			}
			Radius = tmp * units_conv / 2.0;
			if( !thru_flag ) {
				if( get_exp_value( "Depth", n_exps, exps, descs, &tmp ) ) {
					bu_log( "Failed to get depth for simple hole.\n" );
					return( 1 );
				}
				Depth = tmp * units_conv;

				if( get_exp_value( "Tip Angle", n_exps, exps, descs, &tmp ) ) {
					bu_log( "Failed to get tip angle for simple hole.\n" );
					return( 1 );
				}
				Tip_angle = tmp * M_PI / 360.0;
			}
			break;
		case COUNTER_BORE_HOLE_TYPE:
			UF_func( UF_MODL_ask_c_bore_hole_parms( feat_tag, 0, &cb_diam, &diam, &cb_depth, &depth,
								&angle, &thru_flag ) );
			UF_free( cb_diam );
			UF_free( diam );
			UF_free( cb_depth );
			UF_free( depth );
			UF_free( angle );
			if( get_exp_value( "Hole Diameter", n_exps, exps, descs, &tmp ) ) {
				bu_log( "Failed to get diameter for counter-bore hole.\n" );
				return( 1 );
			}
			Radius = tmp * units_conv / 2.0;

			if( get_exp_value( "C-Bore Diameter", n_exps, exps, descs, &tmp ) ) {
				bu_log( "Failed to get counter bore diameter for counter-bore hole.\n" );
				return( 1 );
			}
			CB_radius = tmp * units_conv / 2.0;

			if( get_exp_value( "C-Bore Depth", n_exps, exps, descs, &tmp ) ) {
				bu_log( "Failed to get counter bore depth for counter-bore hole.\n" );
				return( 1 );
			}
			CB_depth = tmp * units_conv;

			if( !thru_flag  ) {
				if( get_exp_value( "Hole Depth", n_exps, exps, descs, &tmp ) ) {
					bu_log( "Failed to get depth for counter-bore hole.\n" );
					return( 1 );
				}
				Depth = tmp * units_conv;

				if( get_exp_value( "Tip Angle", n_exps, exps, descs, &tmp ) ) {
					bu_log( "Failed to get tip angle for counter-bore hole.\n" );
					return( 1 );
				}
				Tip_angle = tmp * M_PI / 360.0;
			}
			break;
		case COUNTER_SINK_HOLE_TYPE:
			UF_func( UF_MODL_ask_c_sunk_hole_parms( feat_tag, 0, &cs_diam, &diam, &depth, &cs_angle,
								&angle, &thru_flag ) );
			UF_free( cs_diam );
			UF_free( diam );
			UF_free( depth );
			UF_free( cs_angle );
			UF_free( angle );
			if( get_exp_value( "Hole Diameter", n_exps, exps, descs, &tmp ) ) {
				bu_log( "Failed to get diameter for counter-sink hole.\n" );
				return( 1 );
			}
			Radius = tmp * units_conv / 2.0;

			if( get_exp_value( "C-Sink Diameter", n_exps, exps, descs, &tmp ) ) {
				bu_log( "Failed to get counter bore diameter for counter-sink hole.\n" );
				return( 1 );
			}
			CS_radius = tmp * units_conv / 2.0;

			if( get_exp_value( "C-Sink Angle", n_exps, exps, descs, &tmp ) ) {
				bu_log( "Failed to get counter bore depth for counter-sink hole.\n" );
				return( 1 );
			}
			CS_angle = tmp * M_PI / 360.0;

			if( !thru_flag  ) {
				if( get_exp_value( "Hole Depth", n_exps, exps, descs, &tmp ) ) {
					bu_log( "Failed to get depth for counter-sink hole.\n" );
					return( 1 );
				}
				Depth = tmp * units_conv;

				if( get_exp_value( "Tip Angle", n_exps, exps, descs, &tmp ) ) {
					bu_log( "Failed to get tip angle for counter-sink hole.\n" );
					return( 1 );
				}
				Tip_angle = tmp * M_PI / 360.0;
			}
			break;
		default:
			bu_log( "do_hole(): Unrecognized hole type (%d)\n", hole_type );
			return( 1 );
	}

	DO_INDENT;
	bu_log( "Found a HOLE:\n" );
	DO_INDENT;
	bu_log( "\tradius = %g\n", Radius );
	if( thru_flag ) {
		Depth = get_thru_faces_length( feat_tag, loc_orig, dir1 ) * units_conv;
		if( Depth < SQRT_SMALL_FASTF ) {
			bu_log( "Failed to get hole depth\n" );
			return( 1 );
		}
		bu_log( "\t calulated depth = %g\n", Depth );
	}

	if( hole_type == COUNTER_BORE_HOLE_TYPE ) {

		VSCALE( height, dir, CB_depth );
		solid_name = create_unique_brlcad_solid_name();
		if( mk_rcc( wdb_fd, solid_name, base, height, CB_radius ) ) {
			bu_log( "Failed to make RCC for simple hole feature!!\n" );
			bu_free( solid_name, "solid_name" );
			return( 1 );
		}
		add_to_obj_list( solid_name );
		(void)mk_addmember( solid_name, &head->l, NULL, WMOP_SUBTRACT );
	}

	if( hole_type == COUNTER_SINK_HOLE_TYPE ) {

		CS_depth = (CS_radius - Radius) / tan( CS_angle );

		VSCALE( height, dir, CS_depth );
		solid_name = create_unique_brlcad_solid_name();
		if( mk_trc_h( wdb_fd, solid_name, base, height, CS_radius, Radius ) ) {
			bu_log( "Failed to make TRC for conter sink feature!!\n" );
			bu_free( solid_name, "solid_name" );
			return( 1 );
		}
		add_to_obj_list( solid_name );
		(void)mk_addmember( solid_name, &head->l, NULL, WMOP_SUBTRACT );

	}

	VSCALE( height, dir, Depth );
	solid_name = create_unique_brlcad_solid_name();
	if( mk_rcc( wdb_fd, solid_name, base, height, Radius ) ) {
		bu_log( "Failed to make RCC for simple hole feature!!\n" );
		bu_free( solid_name, "solid_name" );
		return( 1 );
	}
	add_to_obj_list( solid_name );
	(void)mk_addmember( solid_name, &head->l, NULL, WMOP_SUBTRACT );

	if( !thru_flag && Tip_angle > SQRT_SMALL_FASTF ) {
		fastf_t tip_depth;

		/* handle tip angle */
		tip_depth = (Radius - MIN_RADIUS) / tan( Tip_angle );
		VADD2( base, base, height );
		VSCALE( height, dir, tip_depth );
		solid_name = create_unique_brlcad_solid_name();
		DO_INDENT;
		if( mk_trc_h( wdb_fd, solid_name, base, height, Radius, MIN_RADIUS ) ) {
			bu_log( "Failed to make TRC for simple hole (tip) feature!!\n" );
			bu_free( solid_name, "solid_name" );
			return( 1 );
		}
		add_to_obj_list( solid_name );
		(void)mk_addmember( solid_name, &head->l, NULL, WMOP_SUBTRACT );
	}

	return( 0 );
}

static int
do_rect_pocket(
	      tag_t feat_tag,
	      int n_exps,
	      tag_t *exps,
	      char **descs,
	      double units_conv,
	      const mat_t curr_xform,
	      struct wmember *head )
{
	char *solid_name;
	double location[3], dir1[3], dir2[3];
	point_t base;
	vect_t dirx, diry, dirz;
	fastf_t depth, ylen, zlen, c_radius, f_radius, angle;
	fastf_t ylen_bottom, zlen_bottom, c_radius_bottom;
	fastf_t pts[24];
	point_t base_bottom, base_bottom2;
	vect_t part_height;
	point_t trc_base, trc_top;
	vect_t trc_height;
	double tmp;

	UF_func( UF_MODL_ask_feat_location( feat_tag, location ) );
	DO_INDENT;
	bu_log( "rect pocket location = (%g %g %g), units_conv = %g\n", V3ARGS( location ), units_conv );
	bn_mat_print( "curr_xform", curr_xform );

	VSCALE( location, location, units_conv );
	MAT4X3PNT( base, curr_xform, location );
	UF_func( UF_MODL_ask_feat_direction( feat_tag, dir1, dir2 ) );
	MAT4X3VEC( dirx, curr_xform, dir1 );
	MAT4X3VEC( diry, curr_xform, dir2 );
	VCROSS( dirz, dirx, diry );

	if( get_exp_value( "Length X", n_exps, exps, descs, &tmp ) ) {
		bu_log( "Failed to get X length for rectangular pocket.\n" );
		return( 1 );
	}
	ylen = tmp * units_conv;

	if( get_exp_value( "Length Y", n_exps, exps, descs, &tmp ) ) {
		bu_log( "Failed to get Y length for rectangular pocket.\n" );
		return( 1 );
	}
	zlen = tmp * units_conv;

	if( get_exp_value( "Length Z", n_exps, exps, descs, &tmp ) ) {
		bu_log( "Failed to get Z length for rectangular pocket.\n" );
		return( 1 );
	}
	depth = tmp * units_conv;

	if( get_exp_value( "Corner Radius", n_exps, exps, descs, &tmp ) ) {
		bu_log( "Failed to get corner radius for rectangular pocket.\n" );
		return( 1 );
	}
	c_radius = tmp * units_conv;

	if( get_exp_value( "Floor Radius", n_exps, exps, descs, &tmp ) ) {
		bu_log( "Failed to get floor radius for rectangular pocket.\n" );
		return( 1 );
	}
	f_radius = tmp * units_conv;

	if( get_exp_value( "Taper Angle", n_exps, exps, descs, &tmp ) ) {
		bu_log( "Failed to get taper angle for rectangular pocket.\n" );
		return( 1 );
	}
	angle = tmp * M_PI / 180.0;

	DO_INDENT;
	bu_log( "rect pocket: ylen = %g, zlen = %g, depth = %g, c_radius = %g, f_radius = %g, angle = %g\n",
		ylen, zlen, depth, c_radius, f_radius, angle );

	VJOIN1( base_bottom, base, depth, dirx );
	VJOIN1( base_bottom2, base, depth-f_radius, dirx );
	ylen_bottom = ylen - 2.0 * (depth-f_radius) * tan( angle );
	zlen_bottom = zlen - 2.0 * (depth-f_radius) * tan( angle );
	c_radius_bottom = c_radius - (depth-f_radius) * tan( angle );
	if( c_radius_bottom < 0.0 ) {
		c_radius_bottom = 0.0;
	}

	DO_INDENT;
	bu_log( "\tbase = (%g %g %g), base_bottom = (%g %g %g), ylen_bottom = %g, zlen_bottom = %g\n",
		V3ARGS( base ), V3ARGS( base_bottom ), ylen_bottom, zlen_bottom );
	DO_INDENT;
	bu_log( "\tdiry = (%g %g %g), dirz = (%g %g %g)\n", V3ARGS( diry ), V3ARGS( dirz ) );

	if( f_radius > 0.0 && c_radius > 0.0 ) {
		/* use 3 arbs to hollow out most of the pocket
		 * use particles and trcs to make round edges
		 */
		VJOIN2( &pts[0], base, -ylen/2.0+c_radius, diry, -zlen/2.0, dirz );
		VJOIN1( &pts[3], &pts[0], ylen-2.0*c_radius, diry );
		VJOIN1( &pts[6], &pts[3], zlen, dirz );
		VJOIN1( &pts[9], &pts[0], zlen, dirz );
		VJOIN2( &pts[12], base_bottom2, -ylen_bottom/2.0+c_radius_bottom, diry, -zlen_bottom/2.0, dirz );
		VJOIN1( &pts[15], &pts[12], ylen_bottom-2.0*c_radius_bottom, diry );
		VJOIN1( &pts[18], &pts[15], zlen_bottom, dirz );
		VJOIN1( &pts[21], &pts[12], zlen_bottom, dirz );

		solid_name = create_unique_brlcad_solid_name();
		if( mk_arb8( wdb_fd, solid_name, pts ) ) {
			bu_log( "Failed to make ARB8 for Rectangular Pocket feature!!\n" );
			bu_free( solid_name, "solid_name" );
			return( 1 );
		}
		add_to_obj_list( solid_name );
		(void)mk_addmember( solid_name, &head->l, NULL, WMOP_SUBTRACT );

		VJOIN2( &pts[0], base, -ylen/2.0, diry, -zlen/2.0+c_radius, dirz );
		VJOIN1( &pts[3], &pts[0], ylen, diry );
		VJOIN1( &pts[6], &pts[3], zlen-2.0*c_radius, dirz );
		VJOIN1( &pts[9], &pts[0], zlen-2.0*c_radius, dirz );
		VJOIN2( &pts[12], base_bottom2, -ylen_bottom/2.0, diry, -zlen_bottom/2.0+c_radius_bottom, dirz );
		VJOIN1( &pts[15], &pts[12], ylen_bottom, diry );
		VJOIN1( &pts[18], &pts[15], zlen_bottom-2.0*c_radius_bottom, dirz );
		VJOIN1( &pts[21], &pts[12], zlen_bottom-2.0*c_radius_bottom, dirz );

		solid_name = create_unique_brlcad_solid_name();
		if( mk_arb8( wdb_fd, solid_name, pts ) ) {
			bu_log( "Failed to make ARB8 for Rectangular Pocket feature!!\n" );
			bu_free( solid_name, "solid_name" );
			return( 1 );
		}
		add_to_obj_list( solid_name );
		(void)mk_addmember( solid_name, &head->l, NULL, WMOP_SUBTRACT );

		/* make rounded edges in depth direction */
		VJOIN2( trc_base, base, -ylen/2.0+c_radius, diry, -zlen/2.0+c_radius, dirz );
		VJOIN1( trc_top, trc_base, depth-f_radius, dirx );
		VSUB2( trc_height, trc_top, trc_base );
		solid_name = create_unique_brlcad_solid_name();
		if( mk_trc_h( wdb_fd, solid_name, trc_base, trc_height, c_radius, c_radius_bottom ) ) {
			bu_log( "Failed to make TRC for Rectangular Pocket feature!!\n" );
			bu_free( solid_name, "solid_name" );
			return( 1 );
		}
		add_to_obj_list( solid_name );
		(void)mk_addmember( solid_name, &head->l, NULL, WMOP_SUBTRACT );

		VJOIN2( trc_base, base, ylen/2.0-c_radius, diry, -zlen/2.0+c_radius, dirz );
		solid_name = create_unique_brlcad_solid_name();
		if( mk_trc_h( wdb_fd, solid_name, trc_base, trc_height, c_radius, c_radius_bottom ) ) {
			bu_log( "Failed to make TRC for Rectangular Pocket feature!!\n" );
			bu_free( solid_name, "solid_name" );
			return( 1 );
		}
		add_to_obj_list( solid_name );
		(void)mk_addmember( solid_name, &head->l, NULL, WMOP_SUBTRACT );

		VJOIN2( trc_base, base, -ylen/2.0+c_radius, diry, zlen/2.0-c_radius, dirz );
		solid_name = create_unique_brlcad_solid_name();
		if( mk_trc_h( wdb_fd, solid_name, trc_base, trc_height, c_radius, c_radius_bottom ) ) {
			bu_log( "Failed to make TRC for Rectangular Pocket feature!!\n" );
			bu_free( solid_name, "solid_name" );
			return( 1 );
		}
		add_to_obj_list( solid_name );
		(void)mk_addmember( solid_name, &head->l, NULL, WMOP_SUBTRACT );

		VJOIN2( trc_base, base, ylen/2.0-c_radius, diry, zlen/2.0-c_radius, dirz );
		solid_name = create_unique_brlcad_solid_name();
		if( mk_trc_h( wdb_fd, solid_name, trc_base, trc_height, c_radius, c_radius_bottom ) ) {
			bu_log( "Failed to make TRC for Rectangular Pocket feature!!\n" );
			bu_free( solid_name, "solid_name" );
			return( 1 );
		}
		add_to_obj_list( solid_name );
		(void)mk_addmember( solid_name, &head->l, NULL, WMOP_SUBTRACT );

		/* another arb8 for the bottom */
		VJOIN2( &pts[0], base_bottom2, -ylen_bottom/2.0 + f_radius, diry, -zlen_bottom/2.0 + f_radius, dirz );
		VJOIN1( &pts[3], &pts[0], ylen_bottom-2.0*f_radius, diry );
		VJOIN1( &pts[6], &pts[3], zlen_bottom-2.0*f_radius, dirz );
		VJOIN1( &pts[9], &pts[0], zlen_bottom-2.0*f_radius, dirz );
		VJOIN1( &pts[12], &pts[0], f_radius, dirx );
		VJOIN1( &pts[15], &pts[3], f_radius, dirx );
		VJOIN1( &pts[18], &pts[6], f_radius, dirx );
		VJOIN1( &pts[21], &pts[9], f_radius, dirx );

		solid_name = create_unique_brlcad_solid_name();
		if( mk_arb8( wdb_fd, solid_name, pts ) ) {
			bu_log( "Failed to make ARB8 for Rectangular Pocket feature!!\n" );
			bu_free( solid_name, "solid_name" );
			return( 1 );
		}
		add_to_obj_list( solid_name );
		(void)mk_addmember( solid_name, &head->l, NULL, WMOP_SUBTRACT );

		/* 4 particle solids for rounded floor edges */
		VSUB2( part_height, &pts[3], &pts[0] );
		solid_name = create_unique_brlcad_solid_name();
		if( mk_particle( wdb_fd, solid_name, &pts[0], part_height, f_radius, f_radius ) ) {
			bu_log( "Failed to make Particle for Rectangular Pocket feature!!\n" );
			bu_free( solid_name, "solid_name" );
			return( 1 );
		}
		add_to_obj_list( solid_name );
		(void)mk_addmember( solid_name, &head->l, NULL, WMOP_SUBTRACT );

		VSUB2( part_height, &pts[6], &pts[3] );
		solid_name = create_unique_brlcad_solid_name();
		if( mk_particle( wdb_fd, solid_name, &pts[3], part_height, f_radius, f_radius ) ) {
			bu_log( "Failed to make Particle for Rectangular Pocket feature!!\n" );
			bu_free( solid_name, "solid_name" );
			return( 1 );
		}
		add_to_obj_list( solid_name );
		(void)mk_addmember( solid_name, &head->l, NULL, WMOP_SUBTRACT );

		VSUB2( part_height, &pts[9], &pts[6] );
		solid_name = create_unique_brlcad_solid_name();
		if( mk_particle( wdb_fd, solid_name, &pts[6], part_height, f_radius, f_radius ) ) {
			bu_log( "Failed to make Particle for Rectangular Pocket feature!!\n" );
			bu_free( solid_name, "solid_name" );
			return( 1 );
		}
		add_to_obj_list( solid_name );
		(void)mk_addmember( solid_name, &head->l, NULL, WMOP_SUBTRACT );

		VSUB2( part_height, &pts[0], &pts[9] );
		solid_name = create_unique_brlcad_solid_name();
		if( mk_particle( wdb_fd, solid_name, &pts[9], part_height, f_radius, f_radius ) ) {
			bu_log( "Failed to make Particle for Rectangular Pocket feature!!\n" );
			bu_free( solid_name, "solid_name" );
			return( 1 );
		}
		add_to_obj_list( solid_name );
		(void)mk_addmember( solid_name, &head->l, NULL, WMOP_SUBTRACT );

	} else if( f_radius > 0.0 ) {
		char *rcc1, *rcc2, *rcc3, *rcc4;
		struct wmember corner;

		VJOIN2( &pts[0], base, -ylen/2.0, diry, -zlen/2.0, dirz );
		VJOIN1( &pts[3], &pts[0], ylen, diry );
		VJOIN1( &pts[6], &pts[3], zlen, dirz );
		VJOIN1( &pts[9], &pts[0], zlen, dirz );

		VJOIN2( &pts[12], base_bottom2, -ylen_bottom/2.0, diry, -zlen_bottom/2.0, dirz );
		VJOIN1( &pts[15], &pts[12], ylen_bottom, diry );
		VJOIN1( &pts[18], &pts[15], zlen_bottom, dirz );
		VJOIN1( &pts[21], &pts[12], zlen_bottom, dirz );

		solid_name = create_unique_brlcad_solid_name();
		if( mk_arb8( wdb_fd, solid_name, pts ) ) {
			bu_log( "Failed to make ARB8 for Rectangular Pocket feature!!\n" );
			bu_free( solid_name, "solid_name" );
			return( 1 );
		}
		add_to_obj_list( solid_name );
		(void)mk_addmember( solid_name, &head->l, NULL, WMOP_SUBTRACT );

		/* another arb8 for the bottom */
		VJOIN2( &pts[0], base_bottom2, -ylen_bottom/2.0 + f_radius, diry, -zlen_bottom/2.0 + f_radius, dirz );
		VJOIN1( &pts[3], &pts[0], ylen_bottom-2.0*f_radius, diry );
		VJOIN1( &pts[6], &pts[3], zlen_bottom-2.0*f_radius, dirz );
		VJOIN1( &pts[9], &pts[0], zlen_bottom-2.0*f_radius, dirz );
		VJOIN1( &pts[12], &pts[0], f_radius, dirx );
		VJOIN1( &pts[15], &pts[3], f_radius, dirx );
		VJOIN1( &pts[18], &pts[6], f_radius, dirx );
		VJOIN1( &pts[21], &pts[9], f_radius, dirx );

		solid_name = create_unique_brlcad_solid_name();
		if( mk_arb8( wdb_fd, solid_name, pts ) ) {
			bu_log( "Failed to make ARB8 for Rectangular Pocket feature!!\n" );
			bu_free( solid_name, "solid_name" );
			return( 1 );
		}
		add_to_obj_list( solid_name );
		(void)mk_addmember( solid_name, &head->l, NULL, WMOP_SUBTRACT );

		/* 4 particle solids for rounded floor edges */
		VSUB2( part_height, &pts[3], &pts[0] );
		solid_name = create_unique_brlcad_solid_name();
		if( mk_particle( wdb_fd, solid_name, &pts[0], part_height, f_radius, f_radius ) ) {
			bu_log( "Failed to make Particle for Rectangular Pocket feature!!\n" );
			bu_free( solid_name, "solid_name" );
			return( 1 );
		}
		add_to_obj_list( solid_name );
		(void)mk_addmember( solid_name, &head->l, NULL, WMOP_SUBTRACT );

		VSUB2( part_height, &pts[6], &pts[3] );
		solid_name = create_unique_brlcad_solid_name();
		if( mk_particle( wdb_fd, solid_name, &pts[3], part_height, f_radius, f_radius ) ) {
			bu_log( "Failed to make Particle for Rectangular Pocket feature!!\n" );
			bu_free( solid_name, "solid_name" );
			return( 1 );
		}
		add_to_obj_list( solid_name );
		(void)mk_addmember( solid_name, &head->l, NULL, WMOP_SUBTRACT );

		VSUB2( part_height, &pts[9], &pts[6] );
		solid_name = create_unique_brlcad_solid_name();
		if( mk_particle( wdb_fd, solid_name, &pts[6], part_height, f_radius, f_radius ) ) {
			bu_log( "Failed to make Particle for Rectangular Pocket feature!!\n" );
			bu_free( solid_name, "solid_name" );
			return( 1 );
		}
		add_to_obj_list( solid_name );
		(void)mk_addmember( solid_name, &head->l, NULL, WMOP_SUBTRACT );

		VSUB2( part_height, &pts[0], &pts[9] );
		solid_name = create_unique_brlcad_solid_name();
		if( mk_particle( wdb_fd, solid_name, &pts[9], part_height, f_radius, f_radius ) ) {
			bu_log( "Failed to make Particle for Rectangular Pocket feature!!\n" );
			bu_free( solid_name, "solid_name" );
			return( 1 );
		}
		add_to_obj_list( solid_name );
		(void)mk_addmember( solid_name, &head->l, NULL, WMOP_SUBTRACT );

		/* 4 RCC's and 4 combinations to get the flooe corners right */
		VJOIN1( trc_base, &pts[0], -f_radius, diry );
		VJOIN1( trc_top, &pts[3], f_radius, diry );
		VSUB2( trc_height, trc_top, trc_base );
		rcc1 = create_unique_brlcad_solid_name();
		if( mk_rcc( wdb_fd, rcc1, trc_base, trc_height, f_radius ) ) {
			bu_log( "Failed to make RCC for Rectangular Pocket feature!!\n" );
			bu_free( rcc1, "rcc1" );
			return( 1 );
		}
		add_to_obj_list( rcc1 );

		VJOIN1( trc_base, &pts[3], -f_radius, dirz );
		VJOIN1( trc_top, &pts[6], f_radius, dirz );
		VSUB2( trc_height, trc_top, trc_base );
		rcc2 = create_unique_brlcad_solid_name();
		if( mk_rcc( wdb_fd, rcc2, trc_base, trc_height, f_radius ) ) {
			bu_log( "Failed to make RCC for Rectangular Pocket feature!!\n" );
			bu_free( rcc2, "rcc2" );
			return( 1 );
		}
		add_to_obj_list( rcc2 );

		VJOIN1( trc_base, &pts[6], f_radius, diry );
		VJOIN1( trc_top, &pts[9], -f_radius, diry );
		VSUB2( trc_height, trc_top, trc_base );
		rcc3 = create_unique_brlcad_solid_name();
		if( mk_rcc( wdb_fd, rcc3, trc_base, trc_height, f_radius ) ) {
			bu_log( "Failed to make RCC for Rectangular Pocket feature!!\n" );
			bu_free( rcc3, "rcc3" );
			return( 1 );
		}
		add_to_obj_list( rcc3 );

		VJOIN1( trc_base, &pts[9], f_radius, dirz );
		VJOIN1( trc_top, &pts[0], -f_radius, dirz );
		VSUB2( trc_height, trc_top, trc_base );
		rcc4 = create_unique_brlcad_solid_name();
		if( mk_rcc( wdb_fd, rcc4, trc_base, trc_height, f_radius ) ) {
			bu_log( "Failed to make RCC for Rectangular Pocket feature!!\n" );
			bu_free( rcc4, "rcc4" );
			return( 1 );
		}
		add_to_obj_list( rcc4 );

		/* intersect these RCC's at the corners, and subtract those intersections to get the dorners right */
		solid_name = create_unique_brlcad_solid_name();
		BU_LIST_INIT( &corner.l );
		(void)mk_addmember( rcc1, &corner.l, NULL, WMOP_UNION );
		(void)mk_addmember( rcc2, &corner.l, NULL, WMOP_INTERSECT );
		mk_comb( wdb_fd, solid_name, &corner.l, 0, NULL, NULL, NULL, 0, 0, 0, 0, 0, 0, 0 );
		(void)mk_addmember( solid_name, &head->l, NULL, WMOP_SUBTRACT );
		add_to_obj_list( solid_name );

		solid_name = create_unique_brlcad_solid_name();
		BU_LIST_INIT( &corner.l );
		(void)mk_addmember( rcc2, &corner.l, NULL, WMOP_UNION );
		(void)mk_addmember( rcc3, &corner.l, NULL, WMOP_INTERSECT );
		mk_comb( wdb_fd, solid_name, &corner.l, 0, NULL, NULL, NULL, 0, 0, 0, 0, 0, 0, 0 );
		(void)mk_addmember( solid_name, &head->l, NULL, WMOP_SUBTRACT );
		add_to_obj_list( solid_name );

		solid_name = create_unique_brlcad_solid_name();
		BU_LIST_INIT( &corner.l );
		(void)mk_addmember( rcc3, &corner.l, NULL, WMOP_UNION );
		(void)mk_addmember( rcc4, &corner.l, NULL, WMOP_INTERSECT );
		mk_comb( wdb_fd, solid_name, &corner.l, 0, NULL, NULL, NULL, 0, 0, 0, 0, 0, 0, 0 );
		(void)mk_addmember( solid_name, &head->l, NULL, WMOP_SUBTRACT );
		add_to_obj_list( solid_name );

		solid_name = create_unique_brlcad_solid_name();
		BU_LIST_INIT( &corner.l );
		(void)mk_addmember( rcc4, &corner.l, NULL, WMOP_UNION );
		(void)mk_addmember( rcc1, &corner.l, NULL, WMOP_INTERSECT );
		mk_comb( wdb_fd, solid_name, &corner.l, 0, NULL, NULL, NULL, 0, 0, 0, 0, 0, 0, 0 );
		(void)mk_addmember( solid_name, &head->l, NULL, WMOP_SUBTRACT );
		add_to_obj_list( solid_name );

	} else if( c_radius > 0.0 ) {
		VJOIN2( &pts[0], base, -ylen/2.0+c_radius, diry, -zlen/2.0, dirz );
		VJOIN1( &pts[3], &pts[0], ylen-2.0*c_radius, diry );
		VJOIN1( &pts[6], &pts[3], zlen, dirz );
		VJOIN1( &pts[9], &pts[0], zlen, dirz );
		VJOIN2( &pts[12], base_bottom, -ylen_bottom/2.0+c_radius_bottom, diry, -zlen_bottom/2.0, dirz );
		VJOIN1( &pts[15], &pts[12], ylen_bottom-2.0*c_radius_bottom, diry );
		VJOIN1( &pts[18], &pts[15], zlen_bottom, dirz );
		VJOIN1( &pts[21], &pts[12], zlen_bottom, dirz );

		solid_name = create_unique_brlcad_solid_name();
		if( mk_arb8( wdb_fd, solid_name, pts ) ) {
			bu_log( "Failed to make ARB8 for Rectangular Pocket feature!!\n" );
			bu_free( solid_name, "solid_name" );
			return( 1 );
		}
		add_to_obj_list( solid_name );
		(void)mk_addmember( solid_name, &head->l, NULL, WMOP_SUBTRACT );

		VJOIN2( &pts[0], base, -ylen/2.0, diry, -zlen/2.0+c_radius, dirz );
		VJOIN1( &pts[3], &pts[0], ylen, diry );
		VJOIN1( &pts[6], &pts[3], zlen-2.0*c_radius, dirz );
		VJOIN1( &pts[9], &pts[0], zlen-2.0*c_radius, dirz );
		VJOIN2( &pts[12], base_bottom, -ylen_bottom/2.0, diry, -zlen_bottom/2.0+c_radius_bottom, dirz );
		VJOIN1( &pts[15], &pts[12], ylen_bottom, diry );
		VJOIN1( &pts[18], &pts[15], zlen_bottom-2.0*c_radius_bottom, dirz );
		VJOIN1( &pts[21], &pts[12], zlen_bottom-2.0*c_radius_bottom, dirz );

		solid_name = create_unique_brlcad_solid_name();
		if( mk_arb8( wdb_fd, solid_name, pts ) ) {
			bu_log( "Failed to make ARB8 for Rectangular Pocket feature!!\n" );
			bu_free( solid_name, "solid_name" );
			return( 1 );
		}
		add_to_obj_list( solid_name );
		(void)mk_addmember( solid_name, &head->l, NULL, WMOP_SUBTRACT );

		/* make rounded edges in depth direction */
		VJOIN2( trc_base, base, -ylen/2.0+c_radius, diry, -zlen/2.0+c_radius, dirz );
		VJOIN1( trc_top, trc_base, depth, dirx );
		VSUB2( trc_height, trc_top, trc_base );
		solid_name = create_unique_brlcad_solid_name();
		if( mk_trc_h( wdb_fd, solid_name, trc_base, trc_height, c_radius, c_radius_bottom ) ) {
			bu_log( "Failed to make TRC for Rectangular Pocket feature!!\n" );
			bu_free( solid_name, "solid_name" );
			return( 1 );
		}
		add_to_obj_list( solid_name );
		(void)mk_addmember( solid_name, &head->l, NULL, WMOP_SUBTRACT );

		VJOIN2( trc_base, base, ylen/2.0-c_radius, diry, -zlen/2.0+c_radius, dirz );
		solid_name = create_unique_brlcad_solid_name();
		if( mk_trc_h( wdb_fd, solid_name, trc_base, trc_height, c_radius, c_radius_bottom ) ) {
			bu_log( "Failed to make TRC for Rectangular Pocket feature!!\n" );
			bu_free( solid_name, "solid_name" );
			return( 1 );
		}
		add_to_obj_list( solid_name );
		(void)mk_addmember( solid_name, &head->l, NULL, WMOP_SUBTRACT );

		VJOIN2( trc_base, base, -ylen/2.0+c_radius, diry, zlen/2.0-c_radius, dirz );
		solid_name = create_unique_brlcad_solid_name();
		if( mk_trc_h( wdb_fd, solid_name, trc_base, trc_height, c_radius, c_radius_bottom ) ) {
			bu_log( "Failed to make TRC for Rectangular Pocket feature!!\n" );
			bu_free( solid_name, "solid_name" );
			return( 1 );
		}
		add_to_obj_list( solid_name );
		(void)mk_addmember( solid_name, &head->l, NULL, WMOP_SUBTRACT );

		VJOIN2( trc_base, base, ylen/2.0-c_radius, diry, zlen/2.0-c_radius, dirz );
		solid_name = create_unique_brlcad_solid_name();
		if( mk_trc_h( wdb_fd, solid_name, trc_base, trc_height, c_radius, c_radius_bottom ) ) {
			bu_log( "Failed to make TRC for Rectangular Pocket feature!!\n" );
			bu_free( solid_name, "solid_name" );
			return( 1 );
		}
		add_to_obj_list( solid_name );
		(void)mk_addmember( solid_name, &head->l, NULL, WMOP_SUBTRACT );
	} else {
		/* simple pocket, no rounding */
		VJOIN2( &pts[0], base, -ylen/2.0, diry, -zlen/2.0, dirz );
		VJOIN1( &pts[3], &pts[0], ylen, diry );
		VJOIN1( &pts[6], &pts[3], zlen, dirz );
		VJOIN1( &pts[9], &pts[0], zlen, dirz );

		VJOIN2( &pts[12], base_bottom, -ylen_bottom/2.0, diry, -zlen_bottom/2.0, dirz );
		VJOIN1( &pts[15], &pts[12], ylen_bottom, diry );
		VJOIN1( &pts[18], &pts[15], zlen_bottom, dirz );
		VJOIN1( &pts[21], &pts[12], zlen_bottom, dirz );

		solid_name = create_unique_brlcad_solid_name();
		if( mk_arb8( wdb_fd, solid_name, pts ) ) {
			bu_log( "Failed to make ARB8 for Rectangular Pocket feature!!\n" );
			bu_free( solid_name, "solid_name" );
			return( 1 );
		}
		add_to_obj_list( solid_name );
		(void)mk_addmember( solid_name, &head->l, NULL, WMOP_SUBTRACT );
	}

	return( 0 );
}

static int
do_cyl_pocket(
	      tag_t feat_tag,
	      int n_exps,
	      tag_t *exps,
	      char **descs,
	      double units_conv,
	      const mat_t curr_xform,
	      struct wmember *head )
{
	char *solid_name;
	double location[3], dir1[3], dir2[3];
	point_t base;
	vect_t height, dir;
	fastf_t radius1, radius2, radius3, round_rad, angle, ht;
	double tmp;

	UF_func( UF_MODL_ask_feat_location( feat_tag, location ) );
	VSCALE( location, location, units_conv );
	MAT4X3PNT( base, curr_xform, location );
	UF_func( UF_MODL_ask_feat_direction( feat_tag, dir1, dir2 ) );
	MAT4X3VEC( dir, curr_xform, dir1 );

	if( get_exp_value( "Depth", n_exps, exps, descs, &tmp ) ) {
		bu_log( "Failed to get Depth for cylindrical pocket.\n" );
		return( 1 );
	}
	ht = tmp * units_conv;

	if( get_exp_value( "Diameter", n_exps, exps, descs, &tmp ) ) {
		bu_log( "Failed to get Diameter for cylindrical pocket.\n" );
		return( 1 );
	}
	radius1 = tmp * units_conv / 2.0;

	if( get_exp_value( "Taper Angle", n_exps, exps, descs, &tmp ) ) {
		bu_log( "Failed to get taper angle for cylindrical pocket.\n" );
		return( 1 );
	}
	angle = tmp * M_PI / 180.0;
	radius2 = radius1 - ht * tan( angle );
	if( radius2 < MIN_RADIUS ) {
		radius2 = MIN_RADIUS;
	}

	if( get_exp_value( "Floor Radius", n_exps, exps, descs, &tmp ) ) {
		bu_log( "Failed to get floor radius for cylindrical pocket.\n" );
		return( 1 );
	}
	round_rad = tmp * units_conv;

	if( round_rad > 0.0 ) {
		/* use a TRC to create most of the pocket
		 * use a TOR to create rounded floor edge
		 * use an RCC to remove the middle of the torus
		 */
		fastf_t tmp_ht, radius4;
		point_t base2;

		if( round_rad > radius2 ) {
			round_rad = radius2;
		}

		tmp_ht = ht - round_rad * cos( angle );
		if( tmp_ht < SMALL_FASTF ) {
			tmp_ht = 0.0;
		}

		radius3 = radius1 - tmp_ht * tan( angle );
		if( radius3 < MIN_RADIUS ) {
			radius3 = MIN_RADIUS;
		}
		VSCALE( height, dir, tmp_ht );
		radius4 = radius3 - round_rad * cos( angle );
		if( NEAR_ZERO( radius2 - round_rad, SMALL_FASTF ) ) {
			/* bottom is spherical */
			point_t center;

			solid_name = create_unique_brlcad_solid_name();
			if( mk_trc_h( wdb_fd, solid_name, base, height, radius1, radius3 ) ) {
				bu_log( "Failed to make TRC for Cylindrical Pocket feature!!\n" );
				bu_free( solid_name, "solid_name" );
				return( 1 );
			}
			add_to_obj_list( solid_name );
			(void)mk_addmember( solid_name, &head->l, NULL, WMOP_SUBTRACT );

			/* use sphere to round bottom */
			VADD2( center, base, height );
			solid_name = create_unique_brlcad_solid_name();
			if( mk_sph( wdb_fd, solid_name, center, radius2 ) ) {
				bu_log( "Failed to make SPH for Cylindrical Pocket feature!!\n" );
				bu_free( solid_name, "solid_name" );
				return( 1 );
			}
			add_to_obj_list( solid_name );
			(void)mk_addmember( solid_name, &head->l, NULL, WMOP_SUBTRACT );
		} else {

			if( round_rad > radius4 ) {
				bu_log( "Bottom of cylindrical pocket will not be rounded\n" );
				VSCALE( height, dir, ht );
				radius3 = radius1 - ht * tan( angle );

				solid_name = create_unique_brlcad_solid_name();
				if( mk_trc_h( wdb_fd, solid_name, base, height, radius1, radius3 ) ) {
					bu_log( "Failed to make TRC for Cylindrical Pocket feature!!\n" );
					bu_free( solid_name, "solid_name" );
					return( 1 );
				}
				add_to_obj_list( solid_name );
				(void)mk_addmember( solid_name, &head->l, NULL, WMOP_SUBTRACT );
				return( 0 );
			}
			solid_name = create_unique_brlcad_solid_name();
			if( mk_trc_h( wdb_fd, solid_name, base, height, radius1, radius3 ) ) {
				bu_log( "Failed to make TRC for Cylindrical Pocket feature!!\n" );
				bu_free( solid_name, "solid_name" );
				return( 1 );
			}
			add_to_obj_list( solid_name );
			(void)mk_addmember( solid_name, &head->l, NULL, WMOP_SUBTRACT );

			/* use torus to round bottom edges */
			VJOIN1( base2, base, tmp_ht, dir );
			VSCALE( height, dir, ht - tmp_ht );
			solid_name = create_unique_brlcad_solid_name();
			if( mk_rcc( wdb_fd, solid_name, base2, height, radius4 ) ) {
				bu_log( "Failed to make RCC for cylinderical pocket feature!!\n" );
				bu_free( solid_name, "solid_name" );
				return( 1 );
			}
			add_to_obj_list( solid_name );
			(void)mk_addmember( solid_name, &head->l, NULL, WMOP_SUBTRACT );

			VJOIN1( base2, base, ht - round_rad, dir );
			solid_name = create_unique_brlcad_solid_name();
			if( mk_tor( wdb_fd, solid_name, base2, dir, radius4, round_rad ) ) {
				bu_log( "Failed to make TOR for cylinderical pocket feature!!\n" );
				bu_free( solid_name, "solid_name" );
				return( 1 );
			}
			add_to_obj_list( solid_name );
			(void)mk_addmember( solid_name, &head->l, NULL, WMOP_SUBTRACT );
		}
	} else {
		/* no rounding */
		radius3 = radius1 - ht * tan( angle );
		if( radius3 < MIN_RADIUS ) {
			radius3 = MIN_RADIUS;
		}
		VSCALE( height, dir, ht );
		solid_name = create_unique_brlcad_solid_name();
		if( mk_trc_h( wdb_fd, solid_name, base, height, radius1, radius3 ) ) {
			bu_log( "Failed to make TRC for Cylindrical Pocket feature!!\n" );
			bu_free( solid_name, "solid_name" );
			return( 1 );
		}
		add_to_obj_list( solid_name );
		(void)mk_addmember( solid_name, &head->l, NULL, WMOP_SUBTRACT );
	}

	return( 0 );
}

static int
do_rect_slot(
	    tag_t feat_tag,
	    int n_exps,
	    tag_t *exps,
	    char **descs,
	    double units_conv,
	    const mat_t curr_xform,
	    struct wmember *head )
{
	char *solid_name;
	double loc_orig[3];
	double location[3], dir1[3], dir2[3];
	point_t base;
	vect_t dirx, diry, dirz;
	char *w, *d, *len;
	int thru_flag;
	fastf_t width, depth, length;
	fastf_t pts[24];
	double tmp;

	UF_func( UF_MODL_ask_feat_location( feat_tag, loc_orig ) );
	bu_log( "Rectangulat Slot:\n" );
	UF_func( UF_MODL_ask_feat_direction( feat_tag, dir1, dir2 ) );
	MAT4X3VEC( dirx, curr_xform, dir1 );
	MAT4X3VEC( diry, curr_xform, dir2 );
	VCROSS( dirz, dirx, diry );

	UF_func( UF_MODL_ask_rect_slot_parms( feat_tag, 0, &w, &d, &len, &thru_flag ) );
	UF_free( w );
	UF_free( d );
	UF_free( len );

	if( get_exp_value( "Width", n_exps, exps, descs, &tmp ) ) {
		bu_log( "Failed to get width for rectangular slot.\n" );
		return( 1 );
	}
	width = tmp * units_conv;
	if( get_exp_value( "Depth", n_exps, exps, descs, &tmp ) ) {
		bu_log( "Failed to get depth for rectangular slot.\n" );
		return( 1 );
	}
	depth = tmp * units_conv;

	bu_log( "\twidth = %g, depth = %g\n", width, depth );
	if( !thru_flag ) {
		if( get_exp_value( "Length", n_exps, exps, descs, &tmp ) ) {
			bu_log( "Failed to get length for rectangular slot.\n" );
			return( 1 );
		}
		length = tmp * units_conv;
	} else {
		length = get_thru_faces_length( feat_tag, loc_orig, dir2 ) * units_conv;
		if( length < SQRT_SMALL_FASTF ) {
			bu_log( "Failed to get slot length\n" );
			return( 1 );
		}
	}
	VSCALE( location, loc_orig, units_conv );
	MAT4X3PNT( base, curr_xform, location );

	/* build an ARB8 for the slot */
	VJOIN2( &pts[0], base, -length/2.0, diry, -width/2.0, dirz );
	VJOIN1( &pts[3], &pts[0], length, diry );
	VJOIN1( &pts[6], &pts[3], width, dirz );
	VJOIN1( &pts[9], &pts[0], width, dirz );
	VJOIN1( &pts[12], &pts[0], -depth, dirx );
	VJOIN1( &pts[15], &pts[3], -depth, dirx );
	VJOIN1( &pts[18], &pts[6], -depth, dirx );
	VJOIN1( &pts[21], &pts[9], -depth, dirx );

	solid_name = create_unique_brlcad_solid_name();
	if( mk_arb8( wdb_fd, solid_name, pts ) ) {
		bu_log( "Failed to make ARB8 for Rectangular Slot feature!!\n" );
		bu_free( solid_name, "solid_name" );
		return( 1 );
	}
	add_to_obj_list( solid_name );
	(void)mk_addmember( solid_name, &head->l, NULL, WMOP_SUBTRACT );

	return( 0 );
}

static int
do_rect_pad(
	    tag_t feat_tag,
	    int n_exps,
	    tag_t *exps,
	    char **descs,
	    double units_conv,
	    const mat_t curr_xform,
	    struct wmember *head )
{
	char *solid_name;
	double location[3], dir1[3], dir2[3];
	point_t base;
	vect_t dirx, diry, dirz;
	fastf_t ylen, zlen, depth, c_radius, angle;
	fastf_t d, c_radius_end;
	fastf_t pts[24];
	point_t trc_base;
	vect_t height;
	double tmp;

	UF_func( UF_MODL_ask_feat_location( feat_tag, location ) );
	VSCALE( location, location, units_conv );
	MAT4X3PNT( base, curr_xform, location );
	UF_func( UF_MODL_ask_feat_direction( feat_tag, dir1, dir2 ) );
	MAT4X3VEC( dirx, curr_xform, dir1 );
	MAT4X3VEC( diry, curr_xform, dir2 );
	VCROSS( dirz, dirx, diry );

	if( get_exp_value( "X Length", n_exps, exps, descs, &tmp ) ) {
		bu_log( "Failed to get X length for rectangular pad.\n" );
		return( 1 );
	}
	ylen = tmp * units_conv;

	if( get_exp_value( "Y Length", n_exps, exps, descs, &tmp ) ) {
		bu_log( "Failed to get Y length for rectangular pad.\n" );
		return( 1 );
	}
	zlen = tmp * units_conv;

	if( get_exp_value( "Z Length", n_exps, exps, descs, &tmp ) ) {
		bu_log( "Failed to get Z length for rectangular pad.\n" );
		return( 1 );
	}
	depth = tmp * units_conv;

	if( get_exp_value( "Corner Radius", n_exps, exps, descs, &tmp ) ) {
		bu_log( "Failed to get corner radius for rectangular pad.\n" );
		return( 1 );
	}
	c_radius = tmp * units_conv;

	if( get_exp_value( "Taper Angle", n_exps, exps, descs, &tmp ) ) {
		bu_log( "Failed to get taper angle for rectangular pad.\n" );
		return( 1 );
	}
	angle = tmp * M_PI / 180.0;

	d = depth * tan( angle );
	c_radius_end = c_radius - d;
	if( c_radius_end < MIN_RADIUS ) {
		c_radius_end = MIN_RADIUS;
	}

	bu_log( "Rectangular Pad:\n" );
	bu_log( "\tlocation = (%g %g %g), ylen = %g, zlen = %g, depth = %g\n", V3ARGS( base ), ylen, zlen, depth );
	bu_log( "\tcorner radius = %g, taper angle = %g\n", c_radius, angle );

	if( c_radius <= 0.0 ) {
		/* no rounding, just make an arb */
		VJOIN2( &pts[0], base, -ylen/2.0, diry, -zlen/2.0, dirz );
		VJOIN1( &pts[3], &pts[0], ylen, diry );
		VJOIN1( &pts[6], &pts[3], zlen, dirz );
		VJOIN1( &pts[9], &pts[0], zlen, dirz );
		VJOIN3( &pts[12], &pts[0], depth, dirx, d, diry, d, dirz );
		VJOIN3( &pts[15], &pts[3], depth, dirx, -d, diry, d, dirz );
		VJOIN3( &pts[18], &pts[6], depth, dirx, -d, diry, -d, dirz );
		VJOIN3( &pts[21], &pts[9], depth, dirx, d, diry, -d, dirz );
		solid_name = create_unique_brlcad_solid_name();
		if( mk_arb8( wdb_fd, solid_name, pts ) ) {
			bu_log( "Failed to make ARB8 for Rectangular Pad feature!!\n" );
			bu_free( solid_name, "solid_name" );
			return( 1 );
		}
		add_to_obj_list( solid_name );
		(void)mk_addmember( solid_name, &head->l, NULL, WMOP_UNION );
	} else {
		/* make two arbs for main part of pad
		 * and four TRC's for the rounded edges
		 */
		VJOIN2( &pts[0], base, -ylen/2.0+c_radius, diry, -zlen/2.0, dirz );
		VJOIN1( &pts[3], &pts[0], ylen-2.0*c_radius, diry );
		VJOIN1( &pts[6], &pts[3], zlen, dirz );
		VJOIN1( &pts[9], &pts[0], zlen, dirz );
		VJOIN2( &pts[12], &pts[0], depth, dirx, d, dirz );
		VJOIN2( &pts[15], &pts[3], depth, dirx, d, dirz );
		VJOIN2( &pts[18], &pts[6], depth, dirx, -d, dirz );
		VJOIN2( &pts[21], &pts[9], depth, dirx, -d, dirz );
		solid_name = create_unique_brlcad_solid_name();
		if( mk_arb8( wdb_fd, solid_name, pts ) ) {
			bu_log( "Failed to make ARB8 for Rectangular Pad feature!!\n" );
			bu_free( solid_name, "solid_name" );
			return( 1 );
		}
		add_to_obj_list( solid_name );
		(void)mk_addmember( solid_name, &head->l, NULL, WMOP_UNION );

		VJOIN2( &pts[0], base, -ylen/2.0, diry, -zlen/2.0+c_radius, dirz );
		VJOIN1( &pts[3], &pts[0], ylen, diry );
		VJOIN1( &pts[6], &pts[3], zlen-2.0*c_radius, dirz );
		VJOIN1( &pts[9], &pts[0], zlen-2.0*c_radius, dirz );
		VJOIN2( &pts[12], &pts[0], depth, dirx, d, diry );
		VJOIN2( &pts[15], &pts[3], depth, dirx, -d, diry );
		VJOIN2( &pts[18], &pts[6], depth, dirx, -d, diry );
		VJOIN2( &pts[21], &pts[9], depth, dirx, d, diry );
		solid_name = create_unique_brlcad_solid_name();
		if( mk_arb8( wdb_fd, solid_name, pts ) ) {
			bu_log( "Failed to make ARB8 for Rectangular Pad feature!!\n" );
			bu_free( solid_name, "solid_name" );
			return( 1 );
		}
		add_to_obj_list( solid_name );
		(void)mk_addmember( solid_name, &head->l, NULL, WMOP_UNION );

		/* make four TRC's */
		VJOIN2( trc_base, base, -ylen/2.0 + c_radius, diry, -zlen/2.0 + c_radius, dirz );
		VSCALE( height, dirx, depth );
		solid_name = create_unique_brlcad_solid_name();
		if( mk_trc_h( wdb_fd, solid_name, trc_base, height, c_radius, c_radius_end ) ) {
			bu_log( "Failed to make TRC for Rectangular Pad feature!!\n" );
			bu_free( solid_name, "solid_name" );
			return( 1 );
		}
		add_to_obj_list( solid_name );
		(void)mk_addmember( solid_name, &head->l, NULL, WMOP_UNION );

		VJOIN2( trc_base, base, ylen/2.0 - c_radius, diry, -zlen/2.0 + c_radius, dirz );
		solid_name = create_unique_brlcad_solid_name();
		if( mk_trc_h( wdb_fd, solid_name, trc_base, height, c_radius, c_radius_end ) ) {
			bu_log( "Failed to make TRC for Rectangular Pad feature!!\n" );
			bu_free( solid_name, "solid_name" );
			return( 1 );
		}
		add_to_obj_list( solid_name );
		(void)mk_addmember( solid_name, &head->l, NULL, WMOP_UNION );

		VJOIN2( trc_base, base, ylen/2.0 - c_radius, diry, zlen/2.0 - c_radius, dirz );
		solid_name = create_unique_brlcad_solid_name();
		if( mk_trc_h( wdb_fd, solid_name, trc_base, height, c_radius, c_radius_end ) ) {
			bu_log( "Failed to make TRC for Rectangular Pad feature!!\n" );
			bu_free( solid_name, "solid_name" );
			return( 1 );
		}
		add_to_obj_list( solid_name );
		(void)mk_addmember( solid_name, &head->l, NULL, WMOP_UNION );

		VJOIN2( trc_base, base, -ylen/2.0 + c_radius, diry, zlen/2.0 - c_radius, dirz );
		solid_name = create_unique_brlcad_solid_name();
		if( mk_trc_h( wdb_fd, solid_name, trc_base, height, c_radius, c_radius_end ) ) {
			bu_log( "Failed to make TRC for Rectangular Pad feature!!\n" );
			bu_free( solid_name, "solid_name" );
			return( 1 );
		}
		add_to_obj_list( solid_name );
		(void)mk_addmember( solid_name, &head->l, NULL, WMOP_UNION );

	}

	return( 0 );
}

static int
do_ball_end_slot(
		 tag_t feat_tag,
		 int n_exps,
		 tag_t *exps,
		 char **descs,
		 double units_conv,
		 const mat_t curr_xform,
		 struct wmember *head )
{
	char *solid_name;
	double loc_orig[3];
	double location[3], dir1[3], dir2[3];
	point_t base;
	char *dia, *d, *l;
	int thru_flag;
	fastf_t radius, depth, length;
	vect_t dirx, diry, dirz;
	point_t part_base;
	vect_t part_height;
	double tmp;

	UF_func( UF_MODL_ask_feat_location( feat_tag, loc_orig ) );
	UF_func( UF_MODL_ask_feat_direction( feat_tag, dir1, dir2 ) );
	MAT4X3VEC( dirx, curr_xform, dir1 );
	MAT4X3VEC( diry, curr_xform, dir2 );
	VCROSS( dirz, dirx, diry );

	UF_func( UF_MODL_ask_ball_slot_parms( feat_tag, 0, &dia, &d, &l, &thru_flag ) );
	UF_free( dia );
	UF_free( d );
	UF_free( l );

	if( get_exp_value( "Ball Diameter", n_exps, exps, descs, &tmp ) ) {
		bu_log( "Failed to get Ball diameter for ball end slot.\n" );
		return( 1 );
	}
	radius = tmp * units_conv / 2.0;

	if( get_exp_value( "Depth", n_exps, exps, descs, &tmp ) ) {
		bu_log( "Failed to get depth for ball end slot.\n" );
		return( 1 );
	}
	depth = tmp * units_conv;

	if( !thru_flag ) {
		if( get_exp_value( "Length", n_exps, exps, descs, &tmp ) ) {
			bu_log( "Failed to get length for ball end slot.\n" );
			return( 1 );
		}
		length = tmp * units_conv;
	} else {
		length = get_thru_faces_length( feat_tag, loc_orig, dir2 ) * units_conv;
		if( length < SQRT_SMALL_FASTF ) {
			bu_log( "Failed to get slot length\n" );
			return( 1 );
		}
		length += 2.0 * radius;
	}
	VSCALE( location, loc_orig, units_conv );
	MAT4X3PNT( base, curr_xform, location );

	/* use one arb8, one partices and two RCC's */
	if( length > radius*2.0 ) {
		fastf_t pts[24];
		point_t rcc_base;
		vect_t rcc_height;

		/* build an ARB8 for the slot */
		VJOIN2( &pts[0], base, -length/2.0 + radius, diry, -radius, dirz );
		VJOIN1( &pts[3], &pts[0], length-radius*2.0, diry );
		VJOIN1( &pts[6], &pts[3], radius*2.0, dirz );
		VJOIN1( &pts[9], &pts[0], radius*2.0, dirz );
		VJOIN1( &pts[12], &pts[0], -depth+radius, dirx );
		VJOIN1( &pts[15], &pts[3], -depth+radius, dirx );
		VJOIN1( &pts[18], &pts[6], -depth+radius, dirx );
		VJOIN1( &pts[21], &pts[9], -depth+radius, dirx );

		solid_name = create_unique_brlcad_solid_name();
		if( mk_arb8( wdb_fd, solid_name, pts ) ) {
			bu_log( "Failed to make ARB8 for Ball End Slot feature!!\n" );
			bu_free( solid_name, "solid_name" );
			return( 1 );
		}
		add_to_obj_list( solid_name );
		(void)mk_addmember( solid_name, &head->l, NULL, WMOP_SUBTRACT );

		/* build the particle for the bottom */
		VJOIN2( part_base, base, -length/2.0 + radius, diry, -depth+radius, dirx );
		VSCALE( part_height, diry, length-radius*2.0 );
		solid_name = create_unique_brlcad_solid_name();
		if( mk_particle( wdb_fd, solid_name, part_base, part_height, radius, radius ) ) {
			bu_log( "Failed to make particle solid for Ball End Slot feature!!\n" );
			bu_free( solid_name, "solid_name" );
			return( 1 );
		}
		add_to_obj_list( solid_name );
		(void)mk_addmember( solid_name, &head->l, NULL, WMOP_SUBTRACT );

		/* now the RCC's for each end */
		VJOIN1( rcc_base, base, -length/2.0 + radius, diry );
		VSCALE( rcc_height, dirx, -depth+radius );
		solid_name = create_unique_brlcad_solid_name();
		bu_log( "Creating RCC (%s): base = (%g %g %g), height = (%g %g %g), radius = %g\n",
			solid_name, V3ARGS( rcc_base ), V3ARGS( rcc_height ), radius );
		if( mk_rcc( wdb_fd, solid_name, rcc_base, rcc_height, radius ) ) {
			bu_log( "Failed to make RCC for Ball End Slot feature!!\n" );
			bu_free( solid_name, "solid_name" );
			return( 1 );
		}
		add_to_obj_list( solid_name );
		(void)mk_addmember( solid_name, &head->l, NULL, WMOP_SUBTRACT );

		VJOIN1( rcc_base, base, length/2.0 - radius, diry );
		solid_name = create_unique_brlcad_solid_name();
		bu_log( "Creating RCC (%s): base = (%g %g %g), height = (%g %g %g), radius = %g\n",
			solid_name, V3ARGS( rcc_base ), V3ARGS( rcc_height ), radius );
		if( mk_rcc( wdb_fd, solid_name, rcc_base, rcc_height, radius ) ) {
			bu_log( "Failed to make RCC for Ball End Slot feature!!\n" );
			bu_free( solid_name, "solid_name" );
			return( 1 );
		}
		add_to_obj_list( solid_name );
		(void)mk_addmember( solid_name, &head->l, NULL, WMOP_SUBTRACT );
	} else {
		/* this is just a drill hole, use a single particle primitive */
		VSCALE( part_height, dirx, -depth+radius );
		solid_name = create_unique_brlcad_solid_name();
		if( mk_particle( wdb_fd, solid_name, base, part_height, radius, radius ) ) {
			bu_log( "Failed to make particle solid for Ball End Slot feature!!\n" );
			bu_free( solid_name, "solid_name" );
			return( 1 );
		}
		add_to_obj_list( solid_name );
		(void)mk_addmember( solid_name, &head->l, NULL, WMOP_SUBTRACT );

	}
	return( 0 );
}


static int
do_t_slot( tag_t feat_tag,
	   int n_exps,
	   tag_t *exps,
	   char **descs,
	   double units_conv,
	   const mat_t curr_xform,
	   struct wmember *head )
{
	char *solid_name;
	double loc_orig[3];
	double location[3], dir1[3], dir2[3];
	point_t base;
	vect_t dirx, diry, dirz;
	int thru_flag;
	char *tw, *td, *bw, *bd, *dist;
	fastf_t top_radius, top_depth, bottom_radius, bottom_depth, length=0.0;
	fastf_t pts[24];
	point_t rcc_base;
	vect_t rcc_height;
	double tmp;

	UF_func( UF_MODL_ask_feat_location( feat_tag, loc_orig ) );
	UF_func( UF_MODL_ask_feat_direction( feat_tag, dir1, dir2 ) );
	MAT4X3VEC( dirx, curr_xform, dir1 );
	MAT4X3VEC( diry, curr_xform, dir2 );
	VCROSS( dirz, dirx, diry );

	UF_func( UF_MODL_ask_t_slot_parms( feat_tag, 0, &tw, &td, &bw, &bd, &dist, &thru_flag ) );
	UF_free( tw );
	UF_free( td );
	UF_free( bw );
	UF_free( bd );
	UF_free( dist );

	if( get_exp_value( "Top Width", n_exps, exps, descs, &tmp ) ) {
		bu_log( "Failed to get top width for T slot.\n" );
		return( 1 );
	}
	top_radius = tmp * units_conv / 2.0;

	if( get_exp_value( "Top Depth", n_exps, exps, descs, &tmp ) ) {
		bu_log( "Failed to get top depth for T slot.\n" );
		return( 1 );
	}
	top_depth = tmp * units_conv;

	if( get_exp_value( "Bottom Width", n_exps, exps, descs, &tmp ) ) {
		bu_log( "Failed to get bottom width for T slot.\n" );
		return( 1 );
	}
	bottom_radius = tmp * units_conv / 2.0;

	if( get_exp_value( "Bottom Depth", n_exps, exps, descs, &tmp ) ) {
		bu_log( "Failed to get bottom depth for T slot.\n" );
		return( 1 );
	}
	bottom_depth = tmp * units_conv;

	if( !thru_flag ) {
		if( get_exp_value( "Length", n_exps, exps, descs, &tmp ) ) {
			bu_log( "Failed to get length for T slot.\n" );
			return( 1 );
		}
		length = tmp * units_conv;
	} else {
		length = get_thru_faces_length( feat_tag, loc_orig, dir2 ) * units_conv;
		if( length < SQRT_SMALL_FASTF ) {
			bu_log( "Failed to get slot length\n" );
			return( 1 );
		}
		length += 2.0 * bottom_radius;
	}
	VSCALE( location, loc_orig, units_conv );
	MAT4X3PNT( base, curr_xform, location );


	/* build a T-slot using 2 ARB8's and 4 RCC's */
	VJOIN2( &pts[0], base, -top_radius, dirz, -length/2.0+bottom_radius, diry );
	VJOIN1( &pts[3], &pts[0], top_radius*2.0, dirz );
	VJOIN1( &pts[6], &pts[3], length-bottom_radius*2.0, diry );
	VJOIN1( &pts[9], &pts[0], length-bottom_radius*2.0, diry );
	VJOIN1( &pts[12], &pts[0], -top_depth, dirx );
	VJOIN1( &pts[15], &pts[3], -top_depth, dirx );
	VJOIN1( &pts[18], &pts[6], -top_depth, dirx );
	VJOIN1( &pts[21], &pts[9], -top_depth, dirx );

	solid_name = create_unique_brlcad_solid_name();
	if( mk_arb8( wdb_fd, solid_name, pts ) ) {
		bu_log( "Failed to make ARB8 for T Slot feature!!\n" );
		bu_free( solid_name, "solid_name" );
		return( 1 );
	}
	add_to_obj_list( solid_name );
	(void)mk_addmember( solid_name, &head->l, NULL, WMOP_SUBTRACT );

	VJOIN3( &pts[0], base, -bottom_radius, dirz, -length/2.0+bottom_radius, diry, -top_depth, dirx );
	VJOIN1( &pts[3], &pts[0], bottom_radius*2.0, dirz );
	VJOIN1( &pts[6], &pts[3], length-bottom_radius*2.0, diry );
	VJOIN1( &pts[9], &pts[0], length-bottom_radius*2.0, diry );
	VJOIN1( &pts[12], &pts[0], -bottom_depth, dirx );
	VJOIN1( &pts[15], &pts[3], -bottom_depth, dirx );
	VJOIN1( &pts[18], &pts[6], -bottom_depth, dirx );
	VJOIN1( &pts[21], &pts[9], -bottom_depth, dirx );

	solid_name = create_unique_brlcad_solid_name();
	if( mk_arb8( wdb_fd, solid_name, pts ) ) {
		bu_log( "Failed to make ARB8 for T Slot feature!!\n" );
		bu_free( solid_name, "solid_name" );
		return( 1 );
	}
	add_to_obj_list( solid_name );
	(void)mk_addmember( solid_name, &head->l, NULL, WMOP_SUBTRACT );

	VJOIN1( rcc_base, base, length/2.0 - bottom_radius, diry );
	VSCALE( rcc_height, dirx, -top_depth );
	solid_name = create_unique_brlcad_solid_name();
	if( mk_rcc( wdb_fd, solid_name, rcc_base, rcc_height, top_radius ) ) {
		bu_log( "Failed to make RCC for T Slot feature!!\n" );
		bu_free( solid_name, "solid_name" );
		return( 1 );
	}
	add_to_obj_list( solid_name );
	(void)mk_addmember( solid_name, &head->l, NULL, WMOP_SUBTRACT );

	VJOIN1( rcc_base, base, -length/2.0 + bottom_radius, diry );
	solid_name = create_unique_brlcad_solid_name();
	if( mk_rcc( wdb_fd, solid_name, rcc_base, rcc_height, top_radius ) ) {
		bu_log( "Failed to make RCC for T Slot feature!!\n" );
		bu_free( solid_name, "solid_name" );
		return( 1 );
	}
	add_to_obj_list( solid_name );
	(void)mk_addmember( solid_name, &head->l, NULL, WMOP_SUBTRACT );

	VJOIN2( rcc_base, base, length/2.0 - bottom_radius, diry, -top_depth, dirx );
	VSCALE( rcc_height, dirx, -bottom_depth );
	solid_name = create_unique_brlcad_solid_name();
	if( mk_rcc( wdb_fd, solid_name, rcc_base, rcc_height, bottom_radius ) ) {
		bu_log( "Failed to make RCC for T Slot feature!!\n" );
		bu_free( solid_name, "solid_name" );
		return( 1 );
	}
	add_to_obj_list( solid_name );
	(void)mk_addmember( solid_name, &head->l, NULL, WMOP_SUBTRACT );

	VJOIN2( rcc_base, base, -length/2.0 + bottom_radius, diry, -top_depth, dirx );
	VSCALE( rcc_height, dirx, -bottom_depth );
	solid_name = create_unique_brlcad_solid_name();
	if( mk_rcc( wdb_fd, solid_name, rcc_base, rcc_height, bottom_radius ) ) {
		bu_log( "Failed to make RCC for T Slot feature!!\n" );
		bu_free( solid_name, "solid_name" );
		return( 1 );
	}
	add_to_obj_list( solid_name );
	(void)mk_addmember( solid_name, &head->l, NULL, WMOP_SUBTRACT );

	return( 0 );
}

static int
do_u_slot( tag_t feat_tag,
	   int n_exps,
	   tag_t *exps,
	   char **descs,
	   double units_conv,
	   const mat_t curr_xform,
	   struct wmember *head )
{
	char *solid_name;
	double loc_orig[3];
	double location[3], dir1[3], dir2[3];
	point_t base;
	vect_t dirx, diry, dirz;
	int thru_flag;
	char *w, *d, *crad, *dist;
	fastf_t radius, corner_radius, length, depth;
	fastf_t pts[24];
	point_t rcc_base;
	vect_t rcc_height;
	fastf_t tmp;


	UF_func( UF_MODL_ask_feat_location( feat_tag, loc_orig ) );
	UF_func( UF_MODL_ask_feat_direction( feat_tag, dir1, dir2 ) );
	MAT4X3VEC( dirx, curr_xform, dir1 );
	MAT4X3VEC( diry, curr_xform, dir2 );
	VCROSS( dirz, dirx, diry );

	UF_func( UF_MODL_ask_u_slot_parms( feat_tag, 0, &w, &d, &crad, &dist, &thru_flag ) );
	UF_free( w );
	UF_free( d );
	UF_free( crad );
	UF_free( dist );

	if( get_exp_value( "Width", n_exps, exps, descs, &tmp ) ) {
		bu_log( "Failed to get width for U slot.\n" );
		return( 1 );
	}
	radius = tmp * units_conv / 2.0;

	if( get_exp_value( "Depth", n_exps, exps, descs, &tmp ) ) {
		bu_log( "Failed to get depth for U slot.\n" );
		return( 1 );
	}
	depth = tmp * units_conv;

	if( get_exp_value( "Corner Radius", n_exps, exps, descs, &tmp ) ) {
		bu_log( "Failed to get corner radius for U slot.\n" );
		return( 1 );
	}
	corner_radius = tmp * units_conv;

	if( !thru_flag ) {
		if( get_exp_value( "Length", n_exps, exps, descs, &tmp ) ) {
			bu_log( "Failed to get length for U slot.\n" );
			return( 1 );
		}
		length = tmp * units_conv;
	} else {
		length = get_thru_faces_length( feat_tag, loc_orig, dir2 ) * units_conv;
		if( length < SQRT_SMALL_FASTF ) {
			bu_log( "failed to get slot length\n" );
			return( 1 );
		}
		length += 2.0 * radius;
	}
	VSCALE( location, loc_orig, units_conv );
	MAT4X3PNT( base, curr_xform, location );

	/* this is a fairly complicated shape:
	 *	Use an ARB8 for the center of the slot
	 *	Use two RCC's for the rounded ends of the slot
	 *	Use two more RCC's of smaller radius to get the ends of the bottom
	 *	Use two tori to get the rounded corners of the bottom
	 *	Use another ARB8 to get the center of the bottom
	 *	Use two RCC's to get the rounded straight edges of the bottom
	 */


	/* the two ARB8's */
	VJOIN2( &pts[0], base, -length/2.0 + radius, diry, -radius, dirz );
	VJOIN1( &pts[3], &pts[0], length-radius*2.0, diry );
	VJOIN1( &pts[6], &pts[3], radius*2.0, dirz );
	VJOIN1( &pts[9], &pts[0], radius*2.0, dirz );
	VJOIN1( &pts[12], &pts[0], -depth+corner_radius, dirx );
	VJOIN1( &pts[15], &pts[3], -depth+corner_radius, dirx );
	VJOIN1( &pts[18], &pts[6], -depth+corner_radius, dirx );
	VJOIN1( &pts[21], &pts[9], -depth+corner_radius, dirx );

	solid_name = create_unique_brlcad_solid_name();
	if( mk_arb8( wdb_fd, solid_name, pts ) ) {
		bu_log( "Failed to make ARB8 for U Slot feature!!\n" );
		bu_free( solid_name, "solid_name" );
		return( 1 );
	}
	add_to_obj_list( solid_name );
	(void)mk_addmember( solid_name, &head->l, NULL, WMOP_SUBTRACT );

	VJOIN3( &pts[0], base, -length/2.0 + radius, diry, -radius+corner_radius, dirz, -depth+corner_radius, dirx );
	VJOIN1( &pts[3], &pts[0], length-radius*2.0, diry );
	VJOIN1( &pts[6], &pts[3], radius*2.0 - corner_radius*2.0, dirz );
	VJOIN1( &pts[9], &pts[0], radius*2.0 - corner_radius*2.0, dirz );
	VJOIN1( &pts[12], &pts[0], -corner_radius, dirx );
	VJOIN1( &pts[15], &pts[3], -corner_radius, dirx );
	VJOIN1( &pts[18], &pts[6], -corner_radius, dirx );
	VJOIN1( &pts[21], &pts[9], -corner_radius, dirx );

	solid_name = create_unique_brlcad_solid_name();
	if( mk_arb8( wdb_fd, solid_name, pts ) ) {
		bu_log( "Failed to make ARB8 for U Slot feature!!\n" );
		bu_free( solid_name, "solid_name" );
		return( 1 );
	}
	add_to_obj_list( solid_name );
	(void)mk_addmember( solid_name, &head->l, NULL, WMOP_SUBTRACT );

	/* the two RCC's for the ends of the slot */
	VJOIN1( rcc_base, base, -length/2.0 + radius, diry );
	VSCALE( rcc_height, dirx, -depth+corner_radius );
	solid_name = create_unique_brlcad_solid_name();
	if( mk_rcc( wdb_fd, solid_name, rcc_base, rcc_height, radius ) ) {
		bu_log( "Failed to make RCC for U Slot feature!!\n" );
		bu_free( solid_name, "solid_name" );
		return( 1 );
	}
	add_to_obj_list( solid_name );
	(void)mk_addmember( solid_name, &head->l, NULL, WMOP_SUBTRACT );

	VJOIN1( rcc_base, base, +length/2.0 - radius, diry );
	solid_name = create_unique_brlcad_solid_name();
	if( mk_rcc( wdb_fd, solid_name, rcc_base, rcc_height, radius ) ) {
		bu_log( "Failed to make RCC for U Slot feature!!\n" );
		bu_free( solid_name, "solid_name" );
		return( 1 );
	}
	add_to_obj_list( solid_name );
	(void)mk_addmember( solid_name, &head->l, NULL, WMOP_SUBTRACT );

	/* two smaller RCC's for the end bottoms */
	VJOIN1( rcc_base, rcc_base, -depth+corner_radius, dirx );
	VSCALE( rcc_height, dirx, -corner_radius );
	solid_name = create_unique_brlcad_solid_name();
	if( mk_rcc( wdb_fd, solid_name, rcc_base, rcc_height, radius-corner_radius ) ) {
		bu_log( "Failed to make RCC for U Slot feature!!\n" );
		bu_free( solid_name, "solid_name" );
		return( 1 );
	}
	add_to_obj_list( solid_name );
	(void)mk_addmember( solid_name, &head->l, NULL, WMOP_SUBTRACT );

	VJOIN1( rcc_base, rcc_base, -length+radius*2.0, diry );
	solid_name = create_unique_brlcad_solid_name();
	if( mk_rcc( wdb_fd, solid_name, rcc_base, rcc_height, radius-corner_radius ) ) {
		bu_log( "Failed to make RCC for U Slot feature!!\n" );
		bu_free( solid_name, "solid_name" );
		return( 1 );
	}
	add_to_obj_list( solid_name );
	(void)mk_addmember( solid_name, &head->l, NULL, WMOP_SUBTRACT );

	/* Two torii for the rounded edges on the bottom ends */
	solid_name = create_unique_brlcad_solid_name();
	if( mk_tor( wdb_fd, solid_name, rcc_base, dirx, radius-corner_radius, corner_radius ) ) {
		bu_log( "Failed to make Torus for U Slot feature!!\n" );
		bu_free( solid_name, "solid_name" );
		return( 1 );
	}
	add_to_obj_list( solid_name );
	(void)mk_addmember( solid_name, &head->l, NULL, WMOP_SUBTRACT );

	VJOIN1( rcc_base, rcc_base, +length-radius*2.0, diry );
	solid_name = create_unique_brlcad_solid_name();
	if( mk_tor( wdb_fd, solid_name, rcc_base, dirx, radius-corner_radius, corner_radius ) ) {
		bu_log( "Failed to make Torus for U Slot feature!!\n" );
		bu_free( solid_name, "solid_name" );
		return( 1 );
	}
	add_to_obj_list( solid_name );
	(void)mk_addmember( solid_name, &head->l, NULL, WMOP_SUBTRACT );


	/* and two RCC's for the rounded straight edges of the bottom floor */
	VJOIN3( rcc_base, base, -radius+corner_radius, dirz, -length/2.0+radius, diry, -depth+corner_radius, dirx );
	VSCALE( rcc_height, diry, length - radius*2.0 );
	solid_name = create_unique_brlcad_solid_name();
	if( mk_rcc( wdb_fd, solid_name, rcc_base, rcc_height, corner_radius ) ) {
		bu_log( "Failed to make RCC for U Slot feature!!\n" );
		bu_free( solid_name, "solid_name" );
		return( 1 );
	}
	add_to_obj_list( solid_name );
	(void)mk_addmember( solid_name, &head->l, NULL, WMOP_SUBTRACT );

	VJOIN1( rcc_base, rcc_base, (radius-corner_radius)*2.0, dirz );
	solid_name = create_unique_brlcad_solid_name();
	if( mk_rcc( wdb_fd, solid_name, rcc_base, rcc_height, corner_radius ) ) {
		bu_log( "Failed to make RCC for U Slot feature!!\n" );
		bu_free( solid_name, "solid_name" );
		return( 1 );
	}
	add_to_obj_list( solid_name );
	(void)mk_addmember( solid_name, &head->l, NULL, WMOP_SUBTRACT );

	return( 0 );
}

static int
do_dove_tail_slot( tag_t feat_tag,
		   int n_exps,
		   tag_t *exps,
		   char **descs,
		   double units_conv,
		   const mat_t curr_xform,
		   struct wmember *head )
{
	char *solid_name;
	double loc_orig[3];
	double location[3], dir1[3], dir2[3];
	point_t base;
	vect_t dirx, diry, dirz;
	int thru_flag;
	char *w, *d, *a, *l;
	fastf_t top_radius, bottom_radius, depth, angle, length;
	fastf_t pts[24];
	point_t trc_base;
	vect_t trc_height;
	double tmp;

	UF_func( UF_MODL_ask_feat_location( feat_tag, loc_orig ) );
	UF_func( UF_MODL_ask_feat_direction( feat_tag, dir1, dir2 ) );
	MAT4X3VEC( dirx, curr_xform, dir1 );
	MAT4X3VEC( diry, curr_xform, dir2 );
	VCROSS( dirz, dirx, diry );

	UF_func( UF_MODL_ask_dovetail_slot_parms( feat_tag, 0, &w, &d, &a, &l, &thru_flag ) );
	UF_free( w );
	UF_free( d );
	UF_free( a );
	UF_free( l );

	if( get_exp_value( "Width", n_exps, exps, descs, &tmp ) ) {
		bu_log( "Failed to get width for dove-tail slot.\n" );
		return( 1 );
	}
	top_radius = tmp * units_conv / 2.0;

	if( get_exp_value( "Depth", n_exps, exps, descs, &tmp ) ) {
		bu_log( "Failed to get depth for dove-tail slot.\n" );
		return( 1 );
	}
	depth = tmp * units_conv;

	if( get_exp_value( "Angle", n_exps, exps, descs, &tmp ) ) {
		bu_log( "Failed to get angle for dove-tail slot.\n" );
		return( 1 );
	}
	angle = tmp * M_PI / 180.0;
	if( !thru_flag ) {
		if( get_exp_value( "Length", n_exps, exps, descs, &tmp ) ) {
			bu_log( "Failed to get length for dove-tail slot.\n" );
			return( 1 );
		}
		length = tmp * units_conv;
	} else {
		length = get_thru_faces_length( feat_tag, loc_orig, dir2 ) * units_conv;
		if( length < SQRT_SMALL_FASTF ) {
			bu_log( "failed to get slot length\n" );
			return( 1 );
		}
		length += 2.0 * top_radius;
	}
	VSCALE( location, loc_orig, units_conv );
	MAT4X3PNT( base, curr_xform, location );

	bottom_radius = top_radius + depth * tan( angle );

	/* build this feature using an ARB8 and two TRC's */

	VJOIN2( &pts[0], base, -length/2.0 + top_radius, diry, -top_radius, dirz );
	VJOIN1( &pts[3], &pts[0], top_radius*2.0, dirz );
	VJOIN1( &pts[6], &pts[3], length - top_radius*2.0, diry );
	VJOIN1( &pts[9], &pts[0], length - top_radius*2.0, diry );
	VJOIN2( &pts[12], &pts[0], -bottom_radius + top_radius, dirz, -depth, dirx );
	VJOIN1( &pts[15], &pts[12], bottom_radius*2.0, dirz );
	VJOIN1( &pts[18], &pts[15], length - top_radius*2.0, diry );
	VJOIN1( &pts[21], &pts[12], length - top_radius*2.0, diry );

	solid_name = create_unique_brlcad_solid_name();
	if( mk_arb8( wdb_fd, solid_name, pts ) ) {
		bu_log( "Failed to make ARB8 for Dovetail Slot feature!!\n" );
		bu_free( solid_name, "solid_name" );
		return( 1 );
	}
	add_to_obj_list( solid_name );
	(void)mk_addmember( solid_name, &head->l, NULL, WMOP_SUBTRACT );

	VJOIN1( trc_base, base, length/2.0-top_radius, diry );
	VSCALE( trc_height, dirx, -depth );
	solid_name = create_unique_brlcad_solid_name();
	if( mk_trc_h( wdb_fd, solid_name, trc_base, trc_height, top_radius, bottom_radius ) ) {
		bu_log( "Failed to make TRC for Dovetail Slot feature!!\n" );
		bu_free( solid_name, "solid_name" );
		return( 1 );
	}
	add_to_obj_list( solid_name );
	(void)mk_addmember( solid_name, &head->l, NULL, WMOP_SUBTRACT );

	VJOIN1( trc_base, base, -length/2.0+top_radius, diry );
	solid_name = create_unique_brlcad_solid_name();
	if( mk_trc_h( wdb_fd, solid_name, trc_base, trc_height, top_radius, bottom_radius ) ) {
		bu_log( "Failed to make TRC for Dovetail Slot feature!!\n" );
		bu_free( solid_name, "solid_name" );
		return( 1 );
	}
	add_to_obj_list( solid_name );
	(void)mk_addmember( solid_name, &head->l, NULL, WMOP_SUBTRACT );

	return( 0 );
}

static char *
build_rect_torus( point_t rcc_base,
		  vect_t rcc_height,
		  fastf_t inner_radius,
		  fastf_t outer_radius )
{
	char *solid_name;
	struct wmember head;


	BU_LIST_INIT( &head.l );

	bu_log( "In build_rect_torus():\n" );
	solid_name = create_unique_brlcad_solid_name();
	bu_log( "\tbuilding RCC (%s): base = (%g %g %g), height = (%g %g %g), radius = %g\n",
		solid_name, V3ARGS( rcc_base ), V3ARGS( rcc_height ), outer_radius );
	if( mk_rcc( wdb_fd, solid_name, rcc_base, rcc_height, outer_radius ) ) {
		bu_log( "Failed to make RCC for rectangular torus!!\n" );
		bu_free( solid_name, "solid_name" );
		return( (char *)NULL );
	}
	add_to_obj_list( solid_name );
	(void)mk_addmember( solid_name, &head.l, NULL, WMOP_UNION );

	solid_name = create_unique_brlcad_solid_name();
	bu_log( "\tbuilding RCC (%s): base = (%g %g %g), height = (%g %g %g), radius = %g\n",
		solid_name, V3ARGS( rcc_base ), V3ARGS( rcc_height ), inner_radius );
	if( mk_rcc( wdb_fd, solid_name, rcc_base, rcc_height, inner_radius ) ) {
		bu_log( "Failed to make RCC for rectangular torus!!\n" );
		bu_free( solid_name, "solid_name" );
		return( (char *)NULL );
	}
	add_to_obj_list( solid_name );
	(void)mk_addmember( solid_name, &head.l, NULL, WMOP_SUBTRACT );

	solid_name = create_unique_brlcad_combination_name();
	(void)mk_comb( wdb_fd, solid_name, &head.l, 0, NULL, NULL, NULL, 0, 0, 0, 0, 0, 0, 0 );

	return( solid_name );
}

#define RECT_GROOVE	1
#define BALL_END_GROOVE	2
#define U_GROOVE	3

static int
do_groove( int groove_type,
	   tag_t feat_tag,
	   int n_exps,
	   tag_t *exps,
	   char **descs,
	   double units_conv,
	   const mat_t curr_xform,
	   struct wmember *head )
{
	char *solid_name;
	double location[3], dir1[3], dir2[3];
	point_t base;
	vect_t dirx;
	fastf_t groove_width, groove_radius, ball_radius, corner_radius;
	int i,j;
	uf_list_p_t face_list;
	int num_faces;
	fastf_t inner_radius, outer_radius;
	point_t rcc_base;
	vect_t rcc_height;
	int internal_groove=0;
	double tmp;

	UF_func( UF_MODL_ask_feat_location( feat_tag, location ) );
	bu_log( "location = (%g %g %g\n", V3ARGS( location ) );
	VSCALE( location, location, units_conv );
	MAT4X3PNT( base, curr_xform, location );
	UF_func( UF_MODL_ask_feat_direction( feat_tag, dir1, dir2 ) );
	MAT4X3VEC( dirx, curr_xform, dir1 );

	switch( groove_type ) {
		case RECT_GROOVE:
			if( get_exp_value( "Groove Diameter", n_exps, exps, descs, &tmp ) ) {
				bu_log( "Failed to get groove diameter for rectangular groove.\n" );
				return( 1 );
			}
			groove_radius = tmp * units_conv / 2.0;

			if( get_exp_value( "Width", n_exps, exps, descs, &tmp ) ) {
				bu_log( "Failed to get width for rectangular groove.\n" );
				return( 1 );
			}
			groove_width = tmp * units_conv;

			break;
		case BALL_END_GROOVE:
			if( get_exp_value( "Groove Diameter", n_exps, exps, descs, &tmp ) ) {
				bu_log( "Failed to get groove diameter for ball end groove.\n" );
				return( 1 );
			}
			groove_radius = tmp * units_conv / 2.0;

			if( get_exp_value( "Ball Diameter", n_exps, exps, descs, &tmp ) ) {
				bu_log( "Failed to get ball diameter for ball end groove.\n" );
				return( 1 );
			}
			ball_radius = tmp * units_conv / 2.0;
			break;
		case U_GROOVE:
			if( get_exp_value( "Groove Diameter", n_exps, exps, descs, &tmp ) ) {
				bu_log( "Failed to get groove diameter for U groove.\n" );
				return( 1 );
			}
			groove_radius = tmp * units_conv / 2.0;

			if( get_exp_value( "Width", n_exps, exps, descs, &tmp ) ) {
				bu_log( "Failed to get width for U groove.\n" );
				return( 1 );
			}
			groove_width = tmp * units_conv;

			if( get_exp_value( "Corner Radius", n_exps, exps, descs, &tmp ) ) {
				bu_log( "Failed to get corner radius for U groove.\n" );
				return( 1 );
			}
			corner_radius = tmp * units_conv;
			break;
	}

	UF_func( UF_MODL_ask_feat_faces( feat_tag, &face_list ) );
	UF_func( UF_MODL_ask_list_count( face_list, &num_faces ) );

	inner_radius = MAX_FASTF;
	outer_radius = 0.0;
	for( i=0 ; i<num_faces ; i++ ) {
		tag_t face_tag, edge_tag;
		int face_type;
		uf_list_p_t edge_list;
		int num_edges;
		UF_EVAL_p_t eval;
		struct UF_EVAL_arc_s arc;
		logical is_arc;

		UF_func( UF_MODL_ask_list_item( face_list, i, &face_tag ) );
		UF_func( UF_MODL_ask_face_type( face_tag, &face_type ) );
		if( face_type ==  UF_MODL_PLANAR_FACE ) {
			UF_func( UF_MODL_ask_face_edges( face_tag, &edge_list ) );
			UF_func( UF_MODL_ask_list_count( edge_list, &num_edges ) );
			for( j=0 ; j<num_edges ; j++ ) {
				UF_func( UF_MODL_ask_list_item( edge_list, j, &edge_tag ) );
				UF_func( UF_EVAL_initialize( edge_tag, &eval ) );
				UF_func( UF_EVAL_is_arc( eval, &is_arc ) );
				if( is_arc ) {
					UF_func( UF_EVAL_ask_arc( eval, &arc ) );
					if( arc.radius > outer_radius ) {
						outer_radius = arc.radius;
					}
					if( arc.radius < inner_radius ) {
						inner_radius = arc.radius;
					}
				}
				UF_func( UF_EVAL_free( eval ) );
			}
			UF_MODL_delete_list( &edge_list );
		}
		if( face_type == UF_MODL_CYLINDRICAL_FACE |
		    (face_type == UF_MODL_TOROIDAL_FACE && groove_type == BALL_END_GROOVE ) ) {
			double uv_minmax[4], param[2];
			double pnt[3], u1[3], v1[3], u2[3], v2[3], norm[3], curvature[2];
			point_t pt_on_face;
			vect_t face_norm;
			vect_t to_face;

			/* get face normal to determine if this is an internal or external groove */
			UF_func( UF_MODL_ask_face_uv_minmax( face_tag, uv_minmax ) );
			param[0] = (uv_minmax[0] + uv_minmax[1])/2.0;
			param[1] = (uv_minmax[2] + uv_minmax[3])/2.0;

			UF_func( UF_MODL_ask_face_props( face_tag, param, pnt, u1, v1, u2, v2, norm, curvature ) );
			VSCALE( pnt, pnt, units_conv );
			MAT4X3PNT( pt_on_face, curr_xform, pnt );
			MAT4X3VEC( face_norm, curr_xform, norm );
			VSUB2( to_face, pt_on_face, base );
			if( VDOT( to_face, face_norm ) < 0.0 ) {
				bu_log( "This is an internal groove\n" );
				internal_groove = 1;
			}
		}
	}

	UF_MODL_delete_list( &face_list );

	if( outer_radius > 0.0 ) {
		outer_radius *= units_conv;
		inner_radius *= units_conv;
	}

	switch( groove_type ) {
		case RECT_GROOVE:
			if( outer_radius <= 0.0 ) {
				bu_log( "Unable to build rectangular groove!!\n" );
				return( 0 );
			}
			VJOIN1( rcc_base, base, -groove_width/2.0, dirx );
			VSCALE( rcc_height, dirx, groove_width );
			solid_name = build_rect_torus( rcc_base, rcc_height, groove_radius, outer_radius );
			add_to_obj_list( solid_name );
			(void)mk_addmember( solid_name, &head->l, NULL, WMOP_SUBTRACT );
			break;
		case BALL_END_GROOVE:
			if( outer_radius > 0.0 ) {
				VJOIN1( rcc_base, base, -ball_radius, dirx );
				VSCALE( rcc_height, dirx, ball_radius*2.0 );
				solid_name = build_rect_torus( rcc_base, rcc_height, inner_radius, outer_radius );
				add_to_obj_list( solid_name );
				(void)mk_addmember( solid_name, &head->l, NULL, WMOP_SUBTRACT );
			}
			solid_name = create_unique_brlcad_solid_name();
			if( internal_groove ) {
				if( mk_tor( wdb_fd, solid_name, base, dirx, groove_radius - ball_radius, ball_radius ) ) {
					bu_log( "Failed to make TOR for Ball End Groove feature!!\n" );
					bu_free( solid_name, "solid_name" );
					return( 1 );
				}
			} else {
				if( mk_tor( wdb_fd, solid_name, base, dirx, groove_radius + ball_radius, ball_radius ) ) {
					bu_log( "Failed to make TOR for Ball End Groove feature!!\n" );
					bu_free( solid_name, "solid_name" );
					return( 1 );
				}
			}
			add_to_obj_list( solid_name );
			(void)mk_addmember( solid_name, &head->l, NULL, WMOP_SUBTRACT );
			break;
		case U_GROOVE:
			if( outer_radius > 0.0 ) {
				VJOIN1( rcc_base, base, -groove_width/2.0, dirx );
				VSCALE( rcc_height, dirx, groove_width );
				solid_name = build_rect_torus( rcc_base, rcc_height, inner_radius, outer_radius );
				add_to_obj_list( solid_name );
				(void)mk_addmember( solid_name, &head->l, NULL, WMOP_SUBTRACT );
			}
			VJOIN1( rcc_base, base, -groove_width/2.0 + corner_radius, dirx );
			VSCALE( rcc_height, dirx, groove_width - 2.0*corner_radius );
			if( internal_groove ) {
				solid_name = build_rect_torus( rcc_base, rcc_height, groove_radius - corner_radius,
							       groove_radius );
			} else {
				solid_name = build_rect_torus( rcc_base, rcc_height, groove_radius,
							       groove_radius + corner_radius );
			}
			add_to_obj_list( solid_name );
			(void)mk_addmember( solid_name, &head->l, NULL, WMOP_SUBTRACT );

			VJOIN1( rcc_base, base, groove_width/2.0 - corner_radius, dirx );
			solid_name = create_unique_brlcad_solid_name();
			if( internal_groove ) {
				if( mk_tor( wdb_fd, solid_name, rcc_base, dirx, groove_radius-corner_radius, corner_radius ) ) {
					bu_log( "Failed to make TOR for U Groove feature!!\n" );
					bu_free( solid_name, "solid_name" );
					return( 1 );
				}
			} else {
				if( mk_tor( wdb_fd, solid_name, rcc_base, dirx, groove_radius+corner_radius, corner_radius ) ) {
					bu_log( "Failed to make TOR for U Groove feature!!\n" );
					bu_free( solid_name, "solid_name" );
					return( 1 );
				}
			}
			add_to_obj_list( solid_name );
			(void)mk_addmember( solid_name, &head->l, NULL, WMOP_SUBTRACT );

			VJOIN1( rcc_base, base, -groove_width/2.0 + corner_radius, dirx );
			solid_name = create_unique_brlcad_solid_name();
			if( internal_groove ) {
				if( mk_tor( wdb_fd, solid_name, rcc_base, dirx, groove_radius-corner_radius, corner_radius ) ) {
					bu_log( "Failed to make TOR for U Groove feature!!\n" );
					bu_free( solid_name, "solid_name" );
					return( 1 );
				}
			} else {
				if( mk_tor( wdb_fd, solid_name, rcc_base, dirx, groove_radius+corner_radius, corner_radius ) ) {
					bu_log( "Failed to make TOR for U Groove feature!!\n" );
					bu_free( solid_name, "solid_name" );
					return( 1 );
				}
			}
			add_to_obj_list( solid_name );
			(void)mk_addmember( solid_name, &head->l, NULL, WMOP_SUBTRACT );

			break;
	}

	return( 0 );
}

static int
convert_a_feature( tag_t feat_tag,
		   char *part_name,
		   char *refset_name,
		   char *inst_name,
		   double units_conv,
		   const mat_t curr_xform,
		   unsigned char *rgb,
		   struct wmember *head )
{
	int brlcad_op;
	int failed=0;
	char *feat_name;
	char *feat_type;
	char **descs;
	tag_t *exps;
	int n_exps;
	UF_FEATURE_SIGN sign;

	UF_func( UF_MODL_ask_feature_sign( feat_tag, &sign ) );
	switch( sign ) {
		case UF_NULLSIGN:
		case UF_POSITIVE:
		case UF_UNITE:
		case UF_TOP_TARGET:
			brlcad_op = WMOP_UNION;
			break;
		case UF_NEGATIVE:
		case UF_SUBTRACT:
			brlcad_op = WMOP_SUBTRACT;
			break;
		case UF_UNSIGNED:
		case UF_INTERSECT:
			brlcad_op = WMOP_INTERSECT;
			break;
		default:
			/* unhandled sign type */
			failed = 1;
			break;
	}
	if( failed ) {
		return( 1 );
	}

	if( UF_MODL_ask_exp_desc_of_feat(feat_tag, &n_exps, &descs, &exps ) ) {
		DO_INDENT;
		bu_log( "UF_MODL_ask_exp_desc_of_feat() failed!!!\n" );
		return( 1 );
	}

	UF_func( UF_MODL_ask_feat_name( feat_tag, &feat_name ) );
	UF_func( UF_MODL_ask_feat_type( feat_tag, &feat_type ) );
	DO_INDENT;
	bu_log( "Feature %s is type %s, sign is %s, starting prim number is %d\n",
		feat_name, feat_type, feat_sign[sign], prim_no );
#if 0
	if( !strncmp( feat_name, "UNITE", 5 ) ) {
		bu_log( "Cannot handle UNITE features yet!!\n" );
		UF_free( feat_name );
		UF_free( feat_type );
		return( 1 );
	}
#endif
	if( debug ) {
		int i;

		DO_INDENT;
		bu_log( "\t%d expressions:\n", n_exps );
		for( i=0 ; i<n_exps ; i++ ) {
			DO_INDENT;
			bu_log( "\t\t#%d - %s\n", i+1, descs[i] );
		}
	}
	if( !strcmp( feat_type, "CYLINDER" ) ) {
		fastf_t radius;
		point_t base;
		vect_t height;
		char *solid_name;

		/* make the solid */
		if( get_cylinder_data( feat_tag, n_exps, exps, descs, units_conv,
				       curr_xform, base, height, &radius ) ) {
			bu_log( "Failed to get data for cylinder feature %s in %s\n",
				feat_name, part_name );
			failed = 1;
			goto out;
		}
		solid_name = create_unique_brlcad_solid_name();
		if( mk_rcc( wdb_fd, solid_name, base, height, radius ) ) {
			bu_log( "Failed to make RCC for cylinder feature!!\n" );
			bu_free( solid_name, "solid_name" );
			failed = 1;
			goto out;
		}
		add_to_obj_list( solid_name );
		(void)mk_addmember( solid_name, &head->l, NULL, brlcad_op );
	} else if( !strcmp( feat_type, "BLOCK" ) ) {
		fastf_t pts[24];
		char *solid_name;

		if( get_block_data( feat_tag, n_exps, exps, descs, units_conv, curr_xform, pts ) ) {
			bu_log( "Failed to get data for block feature %s in %s\n",
				feat_name, part_name );
			failed = 1;
			goto out;
		}
		solid_name = create_unique_brlcad_solid_name();
		if( mk_arb8( wdb_fd, solid_name, pts ) ) {
			bu_log( "Failed to make ARB8 for block feature!!\n" );
			bu_free( solid_name, "solid_name" );
			failed = 1;
			goto out;
		}
		add_to_obj_list( solid_name );
		(void)mk_addmember( solid_name, &head->l, NULL, brlcad_op );
	} else if( !strcmp( feat_type, "SPHERE" ) ) {
		point_t center;
		fastf_t radius;
		char *solid_name;

		if( get_sphere_data( feat_tag, n_exps, exps, descs, units_conv,
				     curr_xform, center, &radius ) ) {
			bu_log( "Failed to get data for sphere feature %s in %s\n",
				feat_name, part_name );
			failed = 1;
			goto out;
		}
		solid_name = create_unique_brlcad_solid_name();
		if( mk_sph( wdb_fd, solid_name, center, radius ) ) {
			bu_log( "Failed to make SPHERE for sphere feature!!\n" );
			bu_free( solid_name, "solid_name" );
			failed = 1;
			goto out;
		}
		add_to_obj_list( solid_name );
		(void)mk_addmember( solid_name, &head->l, NULL, brlcad_op );
	} else if( !strcmp( feat_type, "CONE" ) ) {
		point_t base;
		vect_t dirv;
		fastf_t height, radbase, radtop;
		char *solid_name;

		if( get_cone_data( feat_tag, n_exps, exps, descs, units_conv,
				   curr_xform, base, dirv, &height, &radbase, &radtop) ) {
			bu_log( "Failed to get data for cone feature %s in %s\n",
				feat_name, part_name );
			failed = 1;
			goto out;
		}
		solid_name = create_unique_brlcad_solid_name();
		if( mk_cone( wdb_fd, solid_name, base, dirv, height, radbase, radtop ) ) {
			bu_log( "Failed to make TRC for cone feature!!\n" );
			bu_free( solid_name, "solid_name" );
			failed = 1;
			goto out;
		}
		add_to_obj_list( solid_name );
		(void)mk_addmember( solid_name, &head->l, NULL, brlcad_op );
	} else if( !strcmp( feat_type, "SWP104" ) ) {
		char *solid_name;

		solid_name = convert_sweep( feat_tag, part_name, refset_name, inst_name, rgb, curr_xform,
					    units_conv, 0 );
		if( !solid_name ) {
			failed = 1;
			goto out;
		}
		add_to_obj_list( solid_name );
		(void)mk_addmember( solid_name, &head->l, NULL, brlcad_op );
	} else if( !strcmp( feat_type, "SIMPLE HOLE" ) ) {
		if( do_hole( SIMPLE_HOLE_TYPE, feat_tag, n_exps, exps, descs, units_conv,
			     curr_xform, head ) ) {
			failed = 1;
		}
	} else if( !strcmp( feat_type, "CBORE_HOLE" ) ) {
		if( do_hole( COUNTER_BORE_HOLE_TYPE, feat_tag, n_exps, exps, descs, units_conv,
			     curr_xform, head ) ) {
			failed = 1;
		}
	} else if( !strcmp( feat_type, "CSUNK_HOLE" ) ) {
		if( do_hole( COUNTER_SINK_HOLE_TYPE, feat_tag, n_exps, exps, descs, units_conv,
			     curr_xform, head ) ) {
			failed = 1;
		}
	} else if( !strcmp( feat_type, "RECT_POCKET" ) ) {
		if( do_rect_pocket( feat_tag, n_exps, exps, descs, units_conv,
				    curr_xform, head ) ) {
			failed = 1;
		}
	} else if( !strcmp( feat_type, "CYL_POCKET" ) ) {
		if( do_cyl_pocket( feat_tag, n_exps, exps, descs, units_conv,
				   curr_xform, head ) ) {
			failed = 1;
		}
	} else if( !strcmp( feat_type, "RECT_PAD" ) ) {
		if( do_rect_pad( feat_tag, n_exps, exps, descs, units_conv,
				 curr_xform, head ) ) {
			failed = 1;
		}
	} else if( !strcmp( feat_type, "RECT_SLOT" ) ) {
		if( do_rect_slot( feat_tag, n_exps, exps, descs, units_conv,
				  curr_xform, head ) ) {
			failed = 1;
		}
	} else if( !strcmp( feat_type, "BALL_END_SLOT" ) ) {
		if( do_ball_end_slot( feat_tag, n_exps, exps, descs, units_conv,
				      curr_xform, head ) ) {
			failed = 1;
		}
	} else if( !strcmp( feat_type, "U_SLOT" ) ) {
		if( do_u_slot( feat_tag, n_exps, exps, descs, units_conv,
			       curr_xform, head ) ) {
			failed = 1;
		}
	} else if( !strcmp( feat_type, "T_SLOT" ) ) {
		if( do_t_slot( feat_tag, n_exps, exps, descs, units_conv,
			       curr_xform, head ) ) {
			failed = 1;
		}
	} else if( !strcmp( feat_type, "DOVE_TAIL_SLOT" ) ) {
		if( do_dove_tail_slot( feat_tag, n_exps, exps, descs, units_conv,
				       curr_xform, head ) ) {
			failed = 1;
		}
	} else if( !strcmp( feat_type, "RECT_GROOVE" ) ) {
		if( do_groove( RECT_GROOVE, feat_tag, n_exps, exps, descs, units_conv,
			       curr_xform, head ) ) {
			failed = 1;
		}
	} else if( !strcmp( feat_type, "BALL_END_GROOVE" ) ) {
		if( do_groove( BALL_END_GROOVE, feat_tag, n_exps, exps, descs, units_conv,
			       curr_xform, head ) ) {
			failed = 1;
		}
	} else if( !strcmp( feat_type, "U_GROOVE" ) ) {
		if( do_groove( U_GROOVE, feat_tag, n_exps, exps, descs, units_conv,
			       curr_xform, head ) ) {
			failed = 1;
		}
	} else if( !strcmp( feat_type, "MIRROR" ) ) {
#if 1

		int i;
		tag_t source_body, body_xform, datum_plane, datum_xform;

		bu_log( "MIRROR:\n" );
		for( i=0 ; i<n_exps ; i++ ) {
			bu_log( "\t%s\n", descs[i] );
		}

		UF_func( UF_WAVE_ask_link_mirror_data( feat_tag, 1, &source_body, &body_xform, &datum_plane, &datum_xform ) );
		bu_log( "UF_WAVE_ask_link_mirror_data says: source_body = %d, body_xform = %d, datum_plane = %d, datum_xform = %d\n",
			source_body, body_xform, datum_plane, datum_xform );
		failed = 1;
#else
		char *solid_name;
		tag_t *parents, *children;
		int num_parents, num_children;
		int num_datum_planes=0;
		int type, subtype;
		double plane_pt[3], plane_norm[3];
		double mirror_mtx[16];
		mat_t mirror_mat;
		int i,j;
		struct wmember mirror_head;

		UF_func( UF_MODL_ask_feat_relatives( feat_tag, &num_parents, &parents, &num_children, &children ) );

		DO_INDENT;
		bu_log( "Mirror has %d children and %d parents\n", num_children, num_parents );
		for( i=0 ; i<num_parents ; i++ ) {
			char *ftype1;

			UF_func( UF_OBJ_ask_type_and_subtype( parents[i], &type, &subtype));
			UF_func( UF_MODL_ask_feat_type( parents[i], &ftype1 ) );
			DO_INDENT;
			bu_log( "parent[%d] is feature %s, type %s\n", i, feat_name, ftype1 );
			if( !strcmp( ftype1, "DATUM_PLANE" ) ) {
				char *offset, *angle;
				UF_func( UF_MODL_ask_datum_plane_parms( parents[i], plane_pt, plane_norm, &offset, &angle ) );
				DO_INDENT;
				bu_log( "plane is at (%g %g %g), normal is (%g %g %g)\n", V3ARGS( plane_pt ), V3ARGS( plane_norm ) );
				UF_free( offset );
				UF_free( angle );
				UF_func( UF_MTX4_mirror( plane_pt, plane_norm, mirror_mtx ) );
				MAT_COPY( mirror_mat, mirror_mtx );
				num_datum_planes++;
			}
			UF_free( ftype1 );
		}

		if( num_datum_planes != 1 ) {
			UF_free( parents );
			UF_free( children );

			DO_INDENT;
			bu_log( "Failed to convert MIRROR BODY (%s) in part %s\n", feat_name, part_name );
			failed = 1;
			goto out;
		}

		BU_LIST_INIT( &mirror_head.l );
		for( i=0 ; i<num_parents ; i++ ) {
			char *ftype1;

			UF_func( UF_OBJ_ask_type_and_subtype( parents[i], &type, &subtype));
			UF_func( UF_MODL_ask_feat_type( parents[i], &ftype1 ) );
			if( strcmp( ftype1, "DATUM_PLANE" ) ) {
				uf_list_p_t feat_list;
				int feat_count=0;
				tag_t body_tag;
				tag_t body_feat;

				UF_func( UF_MODL_ask_feat_body( parents[i], &body_tag ) );
				UF_func( UF_MODL_ask_body_feats( body_tag, &feat_list ) );
				UF_func( UF_MODL_ask_list_count( feat_list, &feat_count ) );
				DO_INDENT;
				bu_log( "parent feature %s has %d features\n", ftype1, feat_count );
				for( j=0 ; j<feat_count ; j++ ) {
					UF_free( ftype1 );
					UF_func( UF_MODL_ask_list_item( feat_list, j, &body_feat ) );
					UF_func( UF_MODL_ask_feat_type( body_feat, &ftype1 ) );
					DO_INDENT;
					bu_log( "\t%s\n", ftype1 );
				}

				if( only_facetize ) {
					solid_name = (char *)NULL;
				} else {
					solid_name = conv_features( body_tag, part_name, NULL, NULL, curr_xform, units_conv, 0 );
				}

				if( !solid_name ) {
					parts_facetized++;
					if( !only_facetize ) {
						DO_INDENT;
						bu_log( "\tfailed to convert features for feature %s part %s, facetizing\n",
							feat_name, part_name );
					}
					solid_name = facetize( body_tag, part_name, NULL, NULL, curr_xform, units_conv, 0 );
				} else {
					parts_bool++;
				}
				if( solid_name ) {
					add_to_obj_list( solid_name );
					(void)mk_addmember( solid_name, &mirror_head.l, mirror_mat, brlcad_op );
				}
			}
			UF_free( ftype1 );
		}

		UF_free( parents );
		UF_free( children );

		if( BU_LIST_NON_EMPTY( &mirror_head.l ) ) {
			if( BU_LIST_IS_HEAD( BU_LIST_PNEXT_PNEXT( bu_list, &mirror_head.l ), &mirror_head.l ) ) {
				/* only one member in list, we don't need to build a combination */
				(void)mk_addmember( bu_strdup( solid_name ), &head->l, mirror_mat, brlcad_op );
				mk_freemembers( &mirror_head.l );
			} else {
				char *comb_name;

				/* we need to build a combination */
				comb_name = create_unique_brlcad_combination_name();
				(void)mk_comb( wdb_fd, comb_name, &mirror_head.l, 0, NULL, NULL, NULL, 0, 0, 0, 0, 0, 0 ,0 );
				add_to_obj_list( comb_name );
				(void)mk_addmember( comb_name, &head->l, NULL, brlcad_op );
			}
		}
#endif
	} else if( !strcmp( feat_type, "MIRROR_SET" ) ) {
		tag_t plane_tag;
		tag_t *mirror_features;
		int num_mirror_features;
		double plane_pt[3], plane_norm[3];

		UF_func( UF_MODL_ask_features_of_mirror_set( feat_tag, &mirror_features, &num_mirror_features ) );
		UF_func( UF_MODL_ask_plane_of_mirror_set( feat_tag, &plane_tag ) );
		UF_func( UF_MODL_ask_plane( plane_tag, plane_pt, plane_norm ) );

		/* XXX convert this list of features, add it to head list, with a mirror xform */

		UF_free( mirror_features );
		bu_log( "Failed to convert mirror set (%s) in part %s\n", feat_name, part_name );
		failed = 1;
	} else if( !strcmp( feat_type, "BOSS" ) ) {
		char *solid_name;
		double location[3], dir1[3], dir2[3], ht, ang;
		point_t base;
		vect_t dir;
		vect_t Height;
		fastf_t radius1, radius2;
		double tmp;

		UF_func( UF_MODL_ask_feat_location( feat_tag, location ) );
		VSCALE( location, location, units_conv );
		MAT4X3PNT( base, curr_xform, location );
		UF_func( UF_MODL_ask_feat_direction( feat_tag, dir1, dir2 ) );
		MAT4X3VEC( dir, curr_xform, dir1 );

		if( get_exp_value( "Height", n_exps, exps, descs, &tmp ) ) {
			bu_log( "Failed to get height for boss\n" );
			failed = 1;
			goto out;
		}
		ht = tmp * units_conv;
		VSCALE( Height, dir, ht );

		if( get_exp_value( "Diameter", n_exps, exps, descs, &tmp ) ) {
			bu_log( "Failed to get diameter for boss\n" );
			failed = 1;
			goto out;
		}
		radius1 = tmp * units_conv / 2.0;

		if( get_exp_value( "Taper Angle", n_exps, exps, descs, &tmp ) ) {
			bu_log( "Failed to get taper angle for boss\n" );
			failed = 1;
			goto out;
		}
		ang = tmp * M_PI / 180.0;
		radius2 = radius1 - ht * tan( ang );
		if( radius2 < MIN_RADIUS ) {
			radius2 = MIN_RADIUS;
		}

		solid_name = create_unique_brlcad_solid_name();
		if( mk_trc_h( wdb_fd, solid_name, base, Height, radius1, radius2 ) ) {
			bu_log( "Failed to make TRC for BOSS feature!!\n" );
			bu_free( solid_name, "solid_name" );
			failed = 1;
			goto out;
		}
		add_to_obj_list( solid_name );
		(void)mk_addmember( solid_name, &head->l, NULL, brlcad_op );
# if DO_SUPPRESSIONS
	} else if( !strcmp( feat_type, "CHAMFER" ) ) {
		failed = 1;
	} else if( !strcmp( feat_type, "BLEND" ) ) {
		failed = 1;
#else
	} else if( !strcmp( feat_type, "CHAMFER" ) ) {
		if( min_chamfer <= 0.0 ) {
			failed = 1;
		} else {
			double offset1, offset2;

			get_chamfer_offsets( feat_tag, part_name, units_conv, &offset1, &offset2 );
			if( offset1 >= min_chamfer || offset2 >= min_chamfer ) {
				DO_INDENT;
				bu_log( "chamfer offsets of %g or %g is greater than minimum of %g\n", offset1, offset2, min_chamfer );
				failed = 1;
			}
		}
	} else if( !strcmp( feat_type, "BLEND" ) ) {
		if( min_round <= 0.0 ) {
			failed = 1;
		} else {
			double blend_radius;

			get_blend_radius( feat_tag, part_name, units_conv, &blend_radius );
			if( blend_radius >= min_round ) {
				DO_INDENT;
				bu_log( "blend radius of %g is greater than minimum of %g\n", blend_radius, min_round );
				failed = 1;
			}
		}
#endif
	} else if( !strcmp( feat_type, "BREP" ) ) {
		parts_brep++;
		failed = 1;
	} else {
		DO_INDENT;
		bu_log( "Unrecognized feature type (%s)\n", feat_type );
		failed = 1;
	}

 out:
	UF_free( feat_name );
	UF_free( feat_type );
	UF_free( descs );
	UF_free( exps );

	return( failed );
}

char *
conv_features( tag_t solid_tag, char *part_name, char *refset_name, char *inst_name, const mat_t curr_xform,
	       double units_conv, int make_region )
{
	uf_list_p_t feat_list;
	int feat_count=0;
	tag_t feat_tag;
	struct UF_OBJ_disp_props_s disp_props;
	char *color_name;
	double rgb_double[3];
	int color_count;
	int is_suppressed;
	unsigned char *rgb;
	int feat_num;
	int i;
	struct wmember head;
	int failed=0;
	struct obj_list *ptr;

	UF_func( UF_OBJ_ask_display_properties( solid_tag, &disp_props ) );
	if( disp_props.blank_status == UF_OBJ_BLANKED ) {
		DO_INDENT;
		bu_log( "found blanked object in conv_features\n" );
		return( (char *)NULL );
	}
	DO_INDENT;
	bu_log( "Attempt to convert features for %s %s %s\n", part_name, refset_name, inst_name );

	/* Look at the features in this part */
	UF_func( UF_MODL_ask_body_feats( solid_tag, &feat_list ) );
	UF_func( UF_MODL_ask_list_count( feat_list, &feat_count ) );

	DO_INDENT;
	bu_log( "Object %s instance %s has %d features:\n", part_name, inst_name, feat_count );

	BU_LIST_INIT( &head.l );
	for( feat_num=0 ; feat_num<feat_count ; feat_num++ ) {

		UF_func( UF_MODL_ask_list_item( feat_list, feat_num, &feat_tag ) );
		UF_func(UF_MODL_ask_suppress_feature (feat_tag, &is_suppressed));
		if( is_suppressed ) {
			continue;
		}

		if( convert_a_feature( feat_tag, part_name, refset_name, inst_name, units_conv, curr_xform, rgb, &head ) ) {
			failed = 1;

			if( !show_all_features ) {
				break;
			}
		}
	}

	UF_func( UF_MODL_delete_list( &feat_list ) );

	ptr = brlcad_objs_root;
	fprintf( stderr, "freeing list of brlcad objects:\n" );
	while( ptr ) {
		struct obj_list *tmp;
		struct directory *dp;

		fprintf( stderr, "\t%s\n", ptr->name );
		tmp = ptr->next;
		if( failed ) {
			dp = db_lookup( wdb_fd->dbip, ptr->name, LOOKUP_QUIET );
			if( dp != DIR_NULL ) {
				db_delete( wdb_fd->dbip, dp );
				db_dirdelete( wdb_fd->dbip, dp );
			}
		}

		bu_free( ptr->name, "solid name" );
		bu_free( (char *)ptr, "object list" );
		ptr = tmp;
	}
	brlcad_objs_root = NULL;

	if( failed ) {
		return( (char *)NULL );
	}


	UF_func( UF_DISP_ask_color_count( &color_count ) );
	if( disp_props.color > 0 && disp_props.color < color_count ) {
		unsigned char color[3];

		UF_func( UF_DISP_ask_color( disp_props.color, UF_DISP_rgb_model, &color_name, rgb_double ) );
		UF_free( color_name );
		for( i=0 ; i<3 ; i++ ) {
			color[i] = (int)(255.0 * rgb_double[i]);
		}
		rgb = color;
	} else {
		rgb = NULL;
	}

	if( BU_LIST_NON_EMPTY( &head.l ) ) {
		if( make_region ) {
			return( build_region( &head, part_name, refset_name, inst_name, rgb ) );
		} else {
			char *comb_name;

			comb_name = create_unique_brlcad_combination_name();
			(void)mk_comb( wdb_fd, comb_name, &head.l, 0, NULL, NULL, NULL, 0, 0, 0, 0, 0, 0 ,0 );

			return( comb_name );
		}
	}

	return( (char *)NULL );
}

char *
facetize( tag_t solid_tag, char *part_name, char *refset_name, char *inst_name, const mat_t curr_xform, double units_conv, int make_region ) {
    int type, subtype;
    tag_t model;	/* The facetized solid */
    int facet_id;
    int max_verts;	/* Maximum number of verts per facet */
    int vn;		/* vertex number */
    int i;
    int vert_count;
    int face_id;	/* ID of face for a given facet */
    tag_t face_tag;	/* tag of face for a given facet */
    struct bu_vls name_vls;
#define MAX_VERT_PER_FACET 3
    double v[MAX_VERT_PER_FACET][3];	/* vertex list for one facet */
    double normals[MAX_VERT_PER_FACET][3]; /* list of normals (per vertex) for one facet */
    UF_FACET_parameters_t facet_params;
    char *solid_name;
    char *region_name;
    struct wmember head;
    struct UF_OBJ_disp_props_s disp_props;
    char *color_name;
    double rgb_double[3];
    int color_count;
    int status;

    UF_func( UF_DISP_ask_color_count( &color_count ) );
    UF_func( UF_OBJ_ask_display_properties( solid_tag, &disp_props ) );
    if( disp_props.blank_status == UF_OBJ_BLANKED ) {
	    DO_INDENT;
	    bu_log( "facetizer says this is blanked %s %s %s\n", part_name, refset_name, inst_name );
    } else {
	    DO_INDENT;
	    bu_log( "Facetizing %s %s %s\n", part_name, refset_name, inst_name );
	    DO_INDENT;
	    bu_log( "blank_status = %d\n", disp_props.blank_status );
    }

#if 0
    DO_INDENT;
    bu_log( "units_conv = %g\n", units_conv );
    DO_INDENT;
    bn_mat_print( "Facetizer using matrix", curr_xform );
#endif
    UF_func(UF_OBJ_ask_type_and_subtype(solid_tag, &type, &subtype));

    if( type == UF_faceted_model_type ) {
	    DO_INDENT;
	    bu_log( "Found an already facetted model:\n" );
	    model = solid_tag;
    } else {

	    /* check the limitations on facetization */
	    UF_func(UF_FACET_ask_default_parameters( &facet_params ));

	    /* tell UG we will take MAX_VERT_PER_FACET-sided polygons,
	     * and we want doubles
	     * XXX MAX_VERT_PER_FACET should be set to 3 for easy BOT construction.
	     */
	    if (facet_params.max_facet_edges != MAX_VERT_PER_FACET ||
		facet_params.number_storage_type != UF_FACET_TYPE_DOUBLE ||
		!facet_params.specify_surface_tolerance ||
		facet_params.surface_dist_tolerance != surf_tol ||
		facet_params.surface_angular_tolerance != ang_tol) {

		    facet_params.number_storage_type = UF_FACET_TYPE_DOUBLE;
		    facet_params.max_facet_edges = MAX_VERT_PER_FACET;
		    facet_params.specify_surface_tolerance = 1;
		    facet_params.surface_dist_tolerance = surf_tol;
		    facet_params.surface_angular_tolerance = ang_tol;
		    UF_func(UF_FACET_set_default_parameters (&facet_params));

		    UF_func(UF_FACET_ask_default_parameters( &facet_params ));
	    }


	    /* Facetize the solid body we were given */
	    status = UF_FACET_facet_solid(solid_tag, &facet_params, &model );

	    if( status ) {
		    char err_message[133];

		    DO_INDENT;
		    bu_log( "ERROR: failed to facetize part %s, reference set %s, instance name %s\n",
			    part_name, refset_name, inst_name );
		    DO_INDENT;
		    if( !UF_get_fail_message(status, err_message)) {
			    bu_log( "%s\n", err_message );
		    }
		    DO_INDENT;
		    bu_log( "Continueing without this part\n" );
		    return( (char *)NULL );
	    }
    }

    /* find out what the maximum number of vertecies per facet is */
    UF_func(UF_FACET_ask_max_facet_verts( model, &max_verts ));
    DO_INDENT;
    bu_log( "max_verts = %d\n", max_verts );
    if( max_verts != 3 ) {
	    bu_log( "ERROR: cannot handle non-triangular facets\n" );
	    return ( (char *)NULL );
    }

    /* foreach facet, print verts & normals */
    facet_id = UF_FACET_NULL_FACET_ID;
    for (UF_FACET_cycle_facets(model, &facet_id ) ;
	 facet_id != UF_FACET_NULL_FACET_ID ;
	 UF_FACET_cycle_facets (model, &facet_id ) ) {
	    int vindex[3], nindex[3];


	/* XXX Could we intuit something useful
	 * based upon the face_id of the facets??
	 */
	UF_FACET_ask_solid_face_of_facet (model, facet_id, &face_tag);
	UF_FACET_ask_face_id_of_facet (model, facet_id, &face_id );

	/* retrieve the verticies & normals for this facet */
	UF_func(UF_FACET_ask_vertices_of_facet(model, facet_id,
					       &vert_count, v));
	UF_func(UF_FACET_ask_normals_of_facet( model, facet_id,
					       &vert_count, normals));

	for (vn=0 ; vn < vert_count ; vn++) {
		point_t xformed_pt;

		if (units_conv != 1.0) {
			VSCALE(v[vn], v[vn], units_conv);
		}
		MAT4X3PNT( xformed_pt, curr_xform, v[vn] );
		vindex[vn] = Add_vert( xformed_pt[0], xformed_pt[1], xformed_pt[2], vert_tree_root, tol_dist_sq );

		if( use_normals ) {
			MAT4X3VEC( xformed_pt, curr_xform, normals[vn] );
			nindex[vn] = Add_vert( xformed_pt[0], xformed_pt[1], xformed_pt[2], norm_tree_root, tol_dist_sq );
		}
	}
	if( !bad_triangle( vindex[0], vindex[1], vindex[2] ) ) {
		add_triangle( vindex[0], vindex[1], vindex[2] );
		if( use_normals ) {
			add_face_normals( nindex[0], nindex[1], nindex[2] );
		}
	}
    }

    if( curr_tri > 0 && vert_tree_root->curr_vert > 0 ) {
	    prim_no++;
	    bu_vls_init( &name_vls );
	    bu_vls_printf( &name_vls, "s.%d", prim_no );

	    solid_name = create_unique_brlcad_name( &name_vls );
	    bu_vls_free( &name_vls );

	    if( use_normals && norm_tree_root->curr_vert > 0 ) {
		    if( mk_bot_w_normals( wdb_fd, solid_name, RT_BOT_SOLID, RT_BOT_UNORIENTED,
					  RT_BOT_HAS_SURFACE_NORMALS | RT_BOT_USE_NORMALS,
					  vert_tree_root->curr_vert, curr_tri, vert_tree_root->the_array, part_tris,
					  (fastf_t *)NULL, (struct bu_bitv *)NULL, norm_tree_root->curr_vert,
					  norm_tree_root->the_array, part_norms  ) ) {
			    bu_log( "ERROR: Failed to create BOT (%s)\n", solid_name );
		    }
	    } else {
		    if( mk_bot( wdb_fd, solid_name, RT_BOT_SOLID, RT_BOT_UNORIENTED, 0, vert_tree_root->curr_vert, curr_tri,
				vert_tree_root->the_array, part_tris, (fastf_t *)NULL, (struct bu_bitv *)NULL ) ) {
			    bu_log( "ERROR: Failed to create BOT (%s)\n", solid_name );
		    }
	    }

	    clean_vert_tree( vert_tree_root );
	    clean_vert_tree( norm_tree_root );
	    curr_tri = 0;
	    curr_norm = 0;

	    if( make_region ) {
		    bu_vls_init( &name_vls );
		    if( inst_name ) {
			    bu_vls_strcat( &name_vls, inst_name );
		    } else {
			    char *ptr;

			    ptr = strrchr( part_name, '/' );
			    if( ptr ) {
				    ptr++;
				    bu_vls_strcat( &name_vls, ptr );
			    } else {
				    bu_vls_strcat( &name_vls, part_name );
			    }
		    }

		    get_part_name( &name_vls );

		    if( refset_name && strcmp( refset_name, "None" ) ) {
			    bu_vls_strcat( &name_vls, "_" );
			    bu_vls_strcat( &name_vls, refset_name );
		    }
		    bu_vls_strcat( &name_vls, ".r" );
		    region_name = create_unique_brlcad_name( &name_vls );
		    bu_vls_free( &name_vls );

		    BU_LIST_INIT( &head.l );
		    (void)mk_addmember( solid_name, &head.l, NULL, WMOP_UNION );
		    if( disp_props.color > 0 && disp_props.color < color_count ) {
			    unsigned char rgb[3];

			    UF_func( UF_DISP_ask_color( disp_props.color, UF_DISP_rgb_model, &color_name, rgb_double ) );
			    UF_free( color_name );
			    for( i=0 ; i<3 ; i++ ) {
			    rgb[i] = (int)(255.0 * rgb_double[i]);
			    }
			    (void)mk_comb( wdb_fd, region_name, &head.l, 1, NULL, NULL, rgb, ident++, 0, 1, 100, 0, 0, 0 );
		    } else {
			    (void)mk_comb( wdb_fd, region_name, &head.l, 1, NULL, NULL, NULL, ident++, 0, 1, 100, 0, 0, 0 );
		    }
		    DO_INDENT;
		    bu_log( "wrote region: %s, solid %s\n", region_name, solid_name );
		    bu_free( solid_name, "solid_name" );

		    return( region_name );
	    } else {
		    DO_INDENT;
		    bu_log( "wrote solid %s\n", solid_name );
		    return( solid_name );
	    }
    } else {
	    clean_vert_tree( vert_tree_root );
	    clean_vert_tree( norm_tree_root );
	    curr_tri = 0;
	    curr_norm = 0;

	    return( (char *)NULL );
    }
}

char *
process_instance( tag_t comp_obj_tag, const mat_t curr_xform, double units_conv, char *part_name )
{
	tag_t child_tag;
	int i,j;
	mat_t tmp_xform;
	char refset_name[REFSET_NAME_LEN];
	char instance_name[INSTANCE_NAME_LEN];
	char subpart_name[PART_NAME_LEN];
	double origin[ 3 ];
	double csys_matrix[ 9 ];
	double transform[ 4 ][ 4 ];
	mat_t new_xform;
	char *comp_name=NULL;
	int error;
	logical is_suppressed;
	int type, subtype;

	/* get information about the component */
	if( comp_obj_tag == NULL_TAG ) {
		bu_log( "WARNING: An instance tag from part %s has a NULL tag!\n", part_name );
		return( (char *)NULL );
	}

	UF_func( UF_ASSEM_ask_suppress_state( comp_obj_tag, &is_suppressed ) );

	if( is_suppressed ) {
		DO_INDENT;
		bu_log( "component is suppressed\n" );
		return( (char *)NULL );
	}
#if 0
	/* this gives an error */
	UF_func( UF_OBJ_ask_display_properties( comp_obj_tag, &disp_props ) );
	if( disp_props.blank_status == UF_OBJ_BLANKED ) {
		bu_log( "Found a blanked instance\n" );
		return( (char *)NULL );
	}
#endif

	child_tag = UF_ASSEM_ask_child_of_instance(comp_obj_tag);
	if( child_tag == NULL_TAG ) {
		bu_log( "WARNING: The child tag of an instance tag from part %s has a NULL tag!\n", part_name );
		return( (char *)NULL );
	}

#if 0
	/* this gives an error */
	UF_func( UF_OBJ_ask_display_properties( child_tag, &disp_props ) );
	if( disp_props.blank_status == UF_OBJ_BLANKED ) {
		bu_log( "Found a blanked child of instance\n" );
		return( (char *)NULL );
	}
#endif

	UF_func(UF_OBJ_ask_type_and_subtype(comp_obj_tag, &type, &subtype));
	DO_INDENT;
	bu_log( "comp_obj_tag type is %s (subtype = %d)\n", lookup_ug_type( type, NULL ), subtype );
	UF_func(UF_OBJ_ask_type_and_subtype(child_tag, &type, &subtype));
	DO_INDENT;
	bu_log( "child_tag type is %s (subtype = %d)\n", lookup_ug_type( type, NULL ), subtype );

	bzero( refset_name, REFSET_NAME_LEN );
	bzero( instance_name, INSTANCE_NAME_LEN );
	bzero( subpart_name, PART_NAME_LEN );
	error = UF_ASSEM_ask_component_data(comp_obj_tag,
					    subpart_name, refset_name,
					    instance_name, origin,
					    csys_matrix, transform);

	if( error == 641074 ) {
		/* This error occurs when the instance points to a drawing */
		DO_INDENT;
		bu_log( "WARNING: Failed to get component data for an instance in %s\n", part_name );
		return( (char *)NULL );
	}

	if( error ) {
		report( "UF_ASSEM_ask_component_data()", __FILE__, __LINE__, error );
	}

	/* check if this subpart_name is on the list to be converted */
	if( subparts && curr_level == 0 ) {
		char *ptr;
		int do_this_one=0;

		bu_log( "Checking subparts list\n" );

		ptr = strrchr( subpart_name, '/' );
		if( !ptr ) {
			ptr = subpart_name;
		} else {
			ptr++;
		}

		i = -1;
		while( subparts[++i] ) {
			if( !strcmp( ptr, subparts[i] ) ) {
				do_this_one = 1;
				break;
			}
		}

		if( !do_this_one ) {
			bu_log( "Skipping %s\n", ptr );
			return( (char *)NULL );
		} else {
			bu_log( "processing %s\n", ptr );
		}
	}

	DO_INDENT;
	bu_log( "instance: reference set name = %s, instance name = %s\n", refset_name, instance_name );

	transform[0][3] *= units_conv;
	transform[1][3] *= units_conv;
	transform[2][3] *= units_conv;
	for( i=0 ; i<4 ; i++ ) {
		for( j=0 ; j<4 ; j++ ) {
			tmp_xform[i*4+j] = transform[i][j];
		}
	}
#if 0
	DO_INDENT;
	bn_mat_print( "Applying from instance", tmp_xform );
#endif
	bn_mat_mul( new_xform, curr_xform, tmp_xform );

	if( child_tag == NULL_TAG ) {
		bu_log( "Child is not loaded!!!\n" );
	} else {
		curr_level++;
		comp_name = process_part( child_tag, new_xform, subpart_name,
					  refset_name, instance_name );
		curr_level--;
	}

	return( comp_name );
}

char *
convert_entire_part( tag_t node, char *p_name, char *refset_name, char *inst_name, const mat_t curr_xform, double units_conv )
{
	tag_t solid_tag;
	tag_t comp_obj_tag=NULL_TAG;
	int type;
	int subtype;
	struct wmember head;
	char *member_name;
	char *assy_name=NULL;
	struct bu_vls name_vls;
	struct UF_OBJ_disp_props_s disp_props;

	/* no reference sets, convert all solid parts */
	BU_LIST_INIT( &head.l );
	solid_tag = NULL_TAG;
	for (UF_func(UF_OBJ_cycle_objs_in_part(node, UF_solid_type, &solid_tag));
	     solid_tag != NULL_TAG ;
	     UF_func(UF_OBJ_cycle_objs_in_part(node, UF_solid_type, &solid_tag))) {

		if (UF_ASSEM_is_occurrence(solid_tag)) {
			continue;
		}

		UF_func(UF_OBJ_ask_type_and_subtype(solid_tag, &type, &subtype));
		if (type != UF_solid_type || subtype != UF_solid_body_subtype)
			continue;

		UF_func( UF_OBJ_ask_display_properties( solid_tag, &disp_props ) );
		if( disp_props.blank_status == UF_OBJ_BLANKED ) {
			DO_INDENT;
			bu_log( "Found a blanked object in convert_geom\n" );
			continue;
		}

		if( only_facetize ) {
			member_name = (char *)NULL;
		} else {
			member_name = conv_features( solid_tag, p_name,
						     refset_name, inst_name, curr_xform, units_conv, 1 );
		}

		if( !member_name ) {
			parts_facetized++;
			if( !only_facetize ) {
				DO_INDENT;
				bu_log( "\tfailed to convert features, using facetization\n" );
			}
			member_name = facetize( solid_tag, p_name,
						refset_name, inst_name, curr_xform, units_conv, 1 );
		} else {
			parts_bool++;
		}

		if( member_name ) {
			(void)mk_addmember( member_name, &head.l, NULL, WMOP_UNION );
			bu_free( member_name, "member_name" );
		}
	}

	/* recurse to sub-levels (if any) */
	while (comp_obj_tag = UF_ASSEM_cycle_inst_of_part(node, comp_obj_tag) ) {

		if (comp_obj_tag == NULL_TAG) break;

		DO_INDENT;
		bu_log( "Checking instances in %s instance tag = %d\n", p_name, comp_obj_tag );
#if 0
		/* this gives an error */
		UF_func( UF_OBJ_ask_display_properties( comp_obj_tag, &disp_props ) );
		if( disp_props.blank_status == UF_OBJ_BLANKED ) {
			bu_log( "Found a blanked instance\n" );
			continue;
		}
#endif
		member_name = process_instance( comp_obj_tag, curr_xform, units_conv, p_name );
		if( member_name ) {
			(void)mk_addmember( member_name, &head.l, NULL, WMOP_UNION );
			bu_free( member_name, "member_name" );
		}
	}

	if( BU_LIST_NON_EMPTY( &head.l ) ) {
		bu_vls_init( &name_vls );
		if( inst_name ) {
			bu_vls_strcat( &name_vls, inst_name );
		} else {
			char *ptr;

			ptr = strrchr( p_name, '/' );
			if( ptr ) {
				ptr++;
				bu_vls_strcat( &name_vls, ptr );
			} else {
				bu_vls_strcat( &name_vls, p_name );
			}
		}

		get_part_name( &name_vls );

		if( refset_name && strcmp( refset_name, "None" ) ) {
			bu_vls_strcat( &name_vls, "_" );
			bu_vls_strcat( &name_vls, refset_name );
		}

		assy_name = create_unique_brlcad_name( &name_vls );
		bu_vls_free( &name_vls );

		mk_lcomb( wdb_fd , assy_name , &head , 0 ,
			(char *)NULL , (char *)NULL , (unsigned char *)NULL , 0 );
	}

	return( assy_name );
}

char *
convert_reference_set( tag_t node, char *p_name, char *refset_name, char *inst_name, const mat_t curr_xform, double units_conv )
{
	tag_t ref_tag;
	char ref_set_name[REFSET_NAME_LEN];
	double origin[3];
	double matrix[9];
	int num_members;
	tag_t *members;
	int i;
	int type;
	int subtype;
	struct wmember head;
	char *member_name;
	char *assy_name=NULL;
	struct bu_vls name_vls;
	tag_t tmp_tag;
	int found_refset=0;
	int num_refsets=0;
	char **def_ref_sets;
	struct refset_list *ref_root=NULL, *ref_ptr;
	struct UF_OBJ_disp_props_s disp_props;
	int do_entire_part=0;

	BU_LIST_INIT( &head.l );
	tmp_tag = NULL_TAG;
	for (UF_func(UF_OBJ_cycle_objs_in_part(node, UF_reference_set_type,
					       &tmp_tag));
	     tmp_tag != NULL_TAG ;
	     UF_func(UF_OBJ_cycle_objs_in_part(node, UF_reference_set_type,
					       &tmp_tag))) {

		bzero( ref_set_name, REFSET_NAME_LEN );

		UF_func(UF_ASSEM_ask_ref_set_data(tmp_tag,
						  ref_set_name,
						  origin,
						  matrix,
						  &num_members,
						  & members));

		UF_free(members);
		if( !ref_root ) {
			ref_root = (struct refset_list *)bu_malloc( sizeof( struct refset_list ), "ref_root" );
			ref_ptr = ref_root;
		} else {
			ref_ptr->next = (struct refset_list *)bu_malloc( sizeof( struct refset_list ), "ref_root" );
			ref_ptr = ref_ptr->next;
		}
		ref_ptr->next = NULL;
		ref_ptr->name = bu_strdup( ref_set_name );
		ref_ptr->tag = tmp_tag;

		if (refset_name &&  !strcmp(refset_name, ref_set_name) ) {
			DO_INDENT;
			bu_log("----found desired refset \"%s\"\n",
			       ref_set_name);
			found_refset = 1;
			ref_tag = tmp_tag;
		} else {
			continue;
		}
	}

	if( !found_refset ) {

		/* use default reference set names */
		UF_func( UF_ASSEM_ask_default_ref_sets(&num_refsets, &def_ref_sets) );
		for( i=0 ; i<num_refsets ; i++ ) {
			bu_log( "default ref set %d = %s\n", i, def_ref_sets[i] );
		}

		/* look for default reference set names in the actual list of reference sets */

		for( i=0 ; i<num_refsets ; i++ ) {

			if( !strcmp( def_ref_sets[i], "Entire Part" ) ) {
				/* convert entire part */
				DO_INDENT;
				bu_log( "Using reference set %s, since we cannot find %s\n",
					def_ref_sets[i], refset_name );
				do_entire_part = 1;
				break;
			}
			ref_ptr = ref_root;
			while( ref_ptr ) {
				if( !strcmp( ref_ptr->name, def_ref_sets[i] ) ) {
					found_refset = 1;
					ref_tag = ref_ptr->tag;
					break;
				}
				ref_ptr = ref_ptr->next;
			}
			if( found_refset ) {
				break;
			}
		}

		for( i=0 ; i<num_refsets ; i++ ) {
			UF_free( def_ref_sets[i] );
		}
		UF_free( def_ref_sets );
		ref_ptr = ref_root;
		while( ref_ptr ) {
			struct refset_list *tmp_ptr;

			tmp_ptr = ref_ptr->next;
			bu_free( ref_ptr->name, "refset name" );
			bu_free( (char *)ref_ptr, "ref_ptr" );
			ref_ptr = tmp_ptr;
		}
	}

	if( do_entire_part ) {
		return( convert_entire_part( node, p_name, refset_name,
					     inst_name, curr_xform, units_conv ) );
	}

	if( !found_refset ) {
		DO_INDENT;
		bu_log( "ERROR: failed to find a usable reference set for part %s, reference set desired = %s\n",
			p_name, refset_name );
	}

	UF_func(UF_ASSEM_ask_ref_set_data(ref_tag, ref_set_name, origin, matrix, &num_members, & members));
	UF_func( UF_OBJ_ask_display_properties( ref_tag, &disp_props ) );
	if( disp_props.blank_status == UF_OBJ_BLANKED ) {
		DO_INDENT;
		bu_log( "Found a blanked reference set in convert_geom\n" );
		return( (char *)NULL );
	}

	for (i=0 ; i < num_members ; i++ ) {
		UF_func(UF_OBJ_ask_type_and_subtype(members[i], &type, &subtype));
		if (type == UF_solid_type && subtype == UF_solid_body_subtype) {
			if( only_facetize ) {
				member_name = (char *)NULL;
			} else {
				member_name = conv_features( members[i], p_name,
							     refset_name, inst_name, curr_xform, units_conv, 1 );
			}

			if( !member_name ) {
				parts_facetized++;
				if( !only_facetize ) {
					DO_INDENT;
					bu_log( "\tfailed to convert features, using facetization\n" );
				}
				member_name = facetize( members[i], p_name,
							refset_name, inst_name, curr_xform, units_conv, 1 );
			} else {
				parts_bool++;
			}
		} else if( type == UF_occ_instance_type && subtype == UF_occ_instance_subtype ) {
			member_name = process_instance( members[i], curr_xform, units_conv, p_name );
		} else if( type == UF_faceted_model_type && subtype == UF_faceted_model_normal_subtype ) {
			member_name = facetize( members[i], p_name,
						refset_name, inst_name, curr_xform, units_conv, 1 );
		} else {
			DO_INDENT;
			bu_log( "\tSkipping refset member #%d (type = %s, subtype = %d)\n",
				i, lookup_ug_type( type, NULL ), subtype );
			continue;
		}
		if( member_name ) {
			(void)mk_addmember( member_name, &head.l, NULL, WMOP_UNION );
			bu_free( member_name, "member_name" );
		}
	}

	UF_free(members);

	if( BU_LIST_NON_EMPTY( &head.l ) ) {
		bu_vls_init( &name_vls );
		if( inst_name ) {
			bu_vls_strcat( &name_vls, inst_name );
		} else {
			char *ptr;

			ptr = strrchr( p_name, '/' );
			if( ptr ) {
				ptr++;
				bu_vls_strcat( &name_vls, ptr );
			} else {
				bu_vls_strcat( &name_vls, p_name );
			}
		}

		get_part_name( &name_vls );

		if( refset_name && strcmp( refset_name, "None" ) ) {
			bu_vls_strcat( &name_vls, "_" );
			bu_vls_strcat( &name_vls, refset_name );
		}

		assy_name = create_unique_brlcad_name( &name_vls );
		bu_vls_free( &name_vls );

		mk_lcomb( wdb_fd , assy_name , &head , 0 ,
			(char *)NULL , (char *)NULL , (unsigned char *)NULL , 0 );
	}

	return( assy_name );
}

char *
convert_geom( tag_t node, char *p_name, char *refset_name, char *inst_name, const mat_t curr_xform, double units_conv )
{
#if 0
	/* this gives an error */
	UF_func( UF_OBJ_ask_display_properties( node, &disp_props ) );
	if( disp_props.blank_status == UF_OBJ_BLANKED ) {
		bu_log( "Found a blanked object in convert_geom\n" );
		return( (char *)NULL );
	}
#endif

	DO_INDENT;
	bu_log( "Converting part %s (refset=%s)\n", p_name, refset_name );
	if( use_refset_name ) {
		DO_INDENT;
		bu_log( "Using user specified reference set name (%s) in place of (%s)\n",
			use_refset_name, refset_name );
		if( !strcmp( use_refset_name, "Entire Part" ) || !strcmp( use_refset_name, "None" ) ) {
			return( convert_entire_part( node, p_name, refset_name, inst_name,
						     curr_xform, units_conv ) );
		} else {
			return( convert_reference_set( node, p_name, use_refset_name, inst_name,
						       curr_xform, units_conv ) );
		}
	}
	if( refset_name && strcmp( refset_name, "None" ) ) {
		/* convert reference set */
		return( convert_reference_set( node, p_name, refset_name, inst_name,
					       curr_xform, units_conv ) );
	} else {
		/* convert entire part */
		return( convert_entire_part( node, p_name, refset_name, inst_name,
					       curr_xform, units_conv ) );
	}

}

char *
process_part( tag_t node, const mat_t curr_xform, char *p_name, char *ref_set, char *inst_name )
{
    char part_name[PART_NAME_LEN ];		/* name of current part */
    int units;
    double units_conv=1.0;
    char *assy_name=NULL;

    UF_func(UF_PART_ask_part_name(node, part_name));

    DO_INDENT
    bu_log( "********process_part( %s ), tag = %d\n", p_name, node );
    /* make sure the part for this object is loaded */
    switch (UF_PART_is_loaded (part_name)) {
    case 0: /* not loaded */
	fprintf(stderr, "ERROR: %s:%d part %s not loaded\n",
		__FILE__, __LINE__, part_name);
	break;
    case 1: /* fully loaded */
	break;
    case 2: /* partially loaded */
	node = Lee_open_part(part_name, 0);
	DO_INDENT
	bu_log("Loading %s\n", part_name);
	break;
    }

#if 0
    /* this gives an error */
    UF_func( UF_OBJ_ask_display_properties( node, &disp_props ) );
    if( disp_props.blank_status == UF_OBJ_BLANKED ) {
	    bu_log( "Found a blanked instance\n" );
	    return( (char *)NULL );
    }
#endif

    /* ensure that the part we are working on is the current part,
     * this eliminates some confusion about units.
     */
    UF_func( UF_PART_set_display_part( node ) );

    UF_func(UF_PART_ask_units (node, &units));
    if( units == UF_PART_ENGLISH ) {
	    DO_INDENT;
	    bu_log( "  Units are English\n" );
	    units_conv = 25.4;
    } else {
	    DO_INDENT;
	    bu_log( "  Units are Metric\n" );
	    units_conv = 1.0;
    }

    /* get any geometry at this level */
    assy_name = convert_geom( node, p_name, ref_set, inst_name, curr_xform, units_conv );

    UF_func( UF_PART_set_display_part( node ) );

    if( assy_name ) {
	    return( assy_name );
    }

    return( (char *)NULL );
}

static void
add_to_suppress_list( tag_t feat_tag )
{
	tag_t *parents, *children;
	int num_parents, num_children;
	int i, list_len;
	tag_t list_member;

	DO_INDENT;
	bu_log( "add_to_supress_list(%d)\n", feat_tag );
	UF_func( UF_MODL_ask_feat_relatives (feat_tag, &num_parents, &parents,
					     &num_children, &children));
	UF_free( parents );
	UF_free( children );

	/* do not suppress features that have other features depending on them */
	if( num_children ) {
		DO_INDENT;
		bu_log( "Feature %d has %d children, do not suppress\n", feat_tag, num_children );
		return;
	}

	/* make sure this feature is not already on the list */
	UF_func( UF_MODL_ask_list_count( suppress_list, &list_len ) );
	DO_INDENT;
	for( i=0 ; i<list_len ; i++ ) {
		UF_func( UF_MODL_ask_list_item( suppress_list, i, &list_member ) );
		DO_INDENT;
		if( list_member == feat_tag ) {
			DO_INDENT;
			return;
		}
	}

	/* add this feature to the list */
	UF_func( UF_MODL_put_list_item( suppress_list, feat_tag ) );
	DO_INDENT;
}

static void
check_features_for_suppression( tag_t solid_tag, char *part_name, double units_conv )
{
	uf_list_p_t feat_list;
	int feat_count=0;
	int feat_num;

	UF_func( UF_MODL_ask_body_feats( solid_tag, &feat_list ) );
	UF_func( UF_MODL_ask_list_count( feat_list, &feat_count ) );

	DO_INDENT;
	bu_log( "Checking %d features of part %s for suppression\n", feat_count, part_name );
	for( feat_num=0 ; feat_num < feat_count ; feat_num++ ) {
		tag_t feat_tag;
		int is_suppressed;
		char *feat_type;

		DO_INDENT;
		bu_log( "\tChecking feature number %d\n", feat_num );

		UF_func( UF_MODL_ask_list_item( feat_list, feat_num, &feat_tag ) );
		UF_func(UF_MODL_ask_suppress_feature (feat_tag, &is_suppressed));
		if( is_suppressed ) {
			continue;
		}

		UF_func( UF_MODL_ask_feat_type( feat_tag, &feat_type ) );

		if( !strcmp( feat_type, "BLEND" ) ) {
			double blend_radius;

			DO_INDENT;
			bu_log( "\tChecking a blend (%d) for suppression\n", feat_tag );
			get_blend_radius( feat_tag, part_name, units_conv, &blend_radius );
			if( blend_radius >= min_round ) {
				continue;
			}

			add_to_suppress_list( feat_tag );
		} else if( !strcmp( feat_type, "CHAMFER" ) ) {
			double offset1, offset2;

			DO_INDENT;
			bu_log( "\tChecking a chamfer (%d) for suppression\n", feat_tag );
			get_chamfer_offsets( feat_tag, part_name, units_conv, &offset1, &offset2 );

			if( offset1 >= min_chamfer || offset2 >= min_chamfer ) {
				continue;
			}

			add_to_suppress_list( feat_tag );
		}

		UF_free( feat_type );
	}
	UF_func( UF_MODL_delete_list( &feat_list ) );

	DO_INDENT;
	bu_log( "Finished checking features for part %s\n", part_name );
}

static void
do_suppressions_in_instance( tag_t comp_obj_tag )
{
	logical is_suppressed;
	tag_t child_tag;

	if( comp_obj_tag == NULL_TAG ) {
		return;
	}

	UF_func( UF_ASSEM_ask_suppress_state( comp_obj_tag, &is_suppressed ) );

	if( is_suppressed ) {
		return;
	}

	child_tag = UF_ASSEM_ask_child_of_instance(comp_obj_tag);
	if( child_tag == NULL_TAG ) {
		return;
	}

	/* ensure that the part we are working on is the current part,
	 * this eliminates some confusion about units.
	 */
	UF_func( UF_PART_set_display_part( child_tag ) );
	do_suppressions( child_tag );
	UF_func( UF_PART_set_display_part( child_tag ) );
}


static void
do_suppressions( tag_t node )
{
    char part_name[PART_NAME_LEN ];		/* name of current part */
    int units;
    double units_conv=1.0;
    tag_t solid_tag, comp_obj_tag;

    UF_func(UF_PART_ask_part_name(node, part_name));

    DO_INDENT;
    bu_log( "********do_suppressions( %s ), tag = %d\n", part_name, node );
    /* make sure the part for this object is loaded */
    switch (UF_PART_is_loaded (part_name)) {
    case 0: /* not loaded */
	fprintf(stderr, "ERROR: %s:%d part %s not loaded\n",
		__FILE__, __LINE__, part_name);
	break;
    case 1: /* fully loaded */
	break;
    case 2: /* partially loaded */
	node = Lee_open_part(part_name, 0);
	DO_INDENT
	bu_log("Loading %s\n", part_name);
	break;
    }

    UF_func(UF_PART_ask_units (node, &units));
    if( units == UF_PART_ENGLISH ) {
	    units_conv = 25.4;
    } else {
	    units_conv = 1.0;
    }

    solid_tag = NULL_TAG;
    UF_func( UF_OBJ_cycle_objs_in_part(node, UF_solid_type, &solid_tag ) );
    while( solid_tag != NULL_TAG ) {
	    int type, subtype;

	    UF_func(UF_OBJ_ask_type_and_subtype(solid_tag, &type, &subtype));
	    if ( !UF_ASSEM_is_occurrence(solid_tag) &&
		 (type == UF_solid_type) &&
		 (subtype == UF_solid_body_subtype) ) {
		    check_features_for_suppression( solid_tag, part_name, units_conv );
	    }

	    UF_func( UF_OBJ_cycle_objs_in_part(node, UF_solid_type, &solid_tag ) );
    }

    /* recurse to sub-levels (if any) */
    comp_obj_tag = NULL_TAG;
    while (comp_obj_tag = UF_ASSEM_cycle_inst_of_part(node, comp_obj_tag) ) {

	    if (comp_obj_tag == NULL_TAG) break;

	    DO_INDENT;
	    bu_log( "Checking for suppressable features in %s instance tag = %d\n", part_name, comp_obj_tag );

	    curr_level++;
	    do_suppressions_in_instance( comp_obj_tag );
	    curr_level--;
    }
}

static void
get_it_all_loaded( tag_t node )
{
    char part_name[PART_NAME_LEN ];		/* name of current part */
    tag_t comp_obj_tag, child_tag;
    logical is_suppressed;

    UF_func(UF_PART_ask_part_name(node, part_name));

    /* make sure the part for this object is loaded */
    switch (UF_PART_is_loaded (part_name)) {
    case 0: /* not loaded */
	fprintf(stderr, "ERROR: %s:%d part %s not loaded\n",
		__FILE__, __LINE__, part_name);
	break;
    case 1: /* fully loaded */
	break;
    case 2: /* partially loaded */
	node = Lee_open_part(part_name, 0);
	DO_INDENT
	bu_log("Loading %s\n", part_name);
	break;
    }

    comp_obj_tag = NULL_TAG;
    while (comp_obj_tag = UF_ASSEM_cycle_inst_of_part(node, comp_obj_tag) ) {

	    if (comp_obj_tag == NULL_TAG) break;

	    UF_func( UF_ASSEM_ask_suppress_state( comp_obj_tag, &is_suppressed ) );

	    if( is_suppressed ) {
		    continue;
	    }

	    child_tag = UF_ASSEM_ask_child_of_instance(comp_obj_tag);
	    if( child_tag == NULL_TAG ) {
		    continue;
	    }

	    curr_level++;
	    get_it_all_loaded( child_tag );
	    curr_level--;
    }
}

/*
 *	P A R S E _ A R G S --- Parse through command line flags
 */
int parse_args(int ac, char *av[])
{
	int  c;
	char *strrchr();

	if (  ! (progname=strrchr(*av, '/'))  )
		progname = *av;
	else
		++progname;

	/* Turn off getopt's error messages */
	opterr = 0;

	/* get all the option flags from the command line */
	while ((c=getopt(ac,av,options)) != EOF)
		switch (c) {
		case 'i'	: ident = atoi( optarg ); break;
		case 'o'	: output_file = strdup( optarg ); break;
		case 'd'	: debug = atoi(optarg); break;
		case 't'	: surf_tol = atof( optarg ); break;
		case 'a'	: ang_tol = atof( optarg ) * M_PI / 180.0; break;
		case 'n'	: part_name_file = optarg; use_part_name_hash = 1; break;
		case 'R'	: use_refset_name = optarg; break;
		case 'c'	: min_chamfer = atof( optarg ); break;
		case 'r'	: min_round = atof( optarg ); break;
		case 'f'	: only_facetize = 1; break;
		case 's'	: show_all_features = 1; break;
		case 'u'	: use_normals = 1; break;
		case '?'	:
		case 'h'	:
		default		: fprintf(stderr, "Bad or help flag specified\n"); break;
		}

	return(optind);
}

static char *usage="Usage: %s [-d level] [-i starting_ident_number] [-n part_no_to_part_name_mapping_file] [-t surface_tolerance] [-a surface_normal_tolerance] [-R use_refset_name] [-c min_chamfer] [-r min_round] [-f] [-s] [-u] -o output_file.g part_filename [subpart1 subpart2 ...]\n";

int
main(int ac, char *av[])
{
    tag_t displayed_part = NULL_TAG;
    char part_name[PART_NAME_LEN];		/* name of current part */
    tag_t ugpart;
    tag_t cset=0;
    int i, j;
    char *comp_name;
    FILE *fd_parts;
    UF_ASSEM_options_t assem_options;
    time_t elapsed_time, end_time;

    time( &start_time );

    tol_dist_sq = tol_dist * tol_dist;

    i = parse_args(ac, av);

    tol.magic = BN_TOL_MAGIC;
    tol.dist = surf_tol;
    tol.dist_sq = tol.dist * tol.dist;
    tol.perp = 0.00001;
    tol.para = 1.0 - tol.perp;

    if (i+1 > ac) {
	printf( usage, *av);
	return -1;
    }

    vert_tree_root = create_vert_tree();
    norm_tree_root = create_vert_tree();

    if( ac > i+1 ) {
	    subparts = (char **)bu_calloc( ac - i + 1, sizeof(char *), "subparts" );
	    for( j=i+1 ; j<ac ; j++ ) {
		    char *ptr;

		    ptr = strrchr( av[j], '/' );
		    if( !ptr ) {
			    ptr = av[j];
		    } else {
			    ptr++;
		    }

		    subparts[j-i-1] = bu_strdup( ptr );
		    bu_log( "subpart specified: %s\n", subparts[j-i-1] );
	    }
    }

    if( use_part_name_hash ) {
	    if( (fd_parts=fopen( part_name_file, "r" )) == NULL ) {
		    bu_log( "Cannot open part name file (%s)\n", part_name_file );
		    perror( av[0] );
		    exit( 1 );
	    }
	    create_name_hash( fd_parts );
    }

    if( !output_file ) {
	printf( "ERROR: Output file name is required!!\n" );
	printf( usage, *av);
	return -1;
    }

    /* open BRL-CAD database */
    if( (wdb_fd=wdb_fopen( output_file ) ) == NULL ) {
	    fprintf( stderr, "ERROR: Cannot open output file (%s)\n", output_file );
	    perror( *av );
	    return -1;
    }

    /* start up UG interface */
    UF_initialize();

    signal( SIGBUS, abort );

    /* process part listed on command line */
    printf("file %s\n", av[i]);

    /* open part */
    if( UF_PART_is_loaded( av[i] ) != 1 ) {
	    ugpart = open_part(av[i]);
	    bu_log( "opened: %s\n", av[i] );
    }

    uc6476(1); /* draw */
    bu_log( "drew: %s\n", av[i] );

    displayed_part = UF_PART_ask_display_part();
    UF_func(UF_PART_ask_part_name(displayed_part, part_name));

    UF_func( UF_ASSEM_ask_assem_options( &assem_options ) );
    assem_options.maintain_work_part = UF_ASSEM_dont_maintain_work_part;
    UF_func( UF_ASSEM_set_assem_options( &assem_options ) );

    if( only_facetize ) {
	    curr_level = 0;
	    comp_name = process_part( displayed_part, bn_mat_identity, part_name, NULL, NULL );
	    if( comp_name ) {
		    bu_free( comp_name, "comp_name" );
	    }

	    if (cset) unload_sub_parts(cset);
	    if (ugpart) UF_PART_close(ugpart, 1, 1);

	    cset = ugpart = 0;
	    dprintf("%s closed\n", av[i]);

	    UF_terminate();

	    bu_log( "\n\n\t%d parts facetized\n\t%d parts converted to Boolean form\n", parts_facetized, parts_bool );
	    return 0;
    }

#if 1
    if( !only_facetize ) {
	    curr_level = 0;
	    get_it_all_loaded( displayed_part );
    }
#else
    cset = load_sub_parts(displayed_part);
    bu_log( "loaded sub_parts\n" );
#endif

#if DO_SUPPRESSIONS
    if( min_chamfer > 0.0 || min_round > 0.0 ) {
	    int list_len=0;

	    /* create a list of features to be suppressed */
	    UF_func( UF_MODL_create_list( &suppress_list ) );

	    /* ensure that the part we are working on is the current part,
	     * this eliminates some confusion about units.
	     */
	    UF_func( UF_PART_set_display_part( displayed_part ) );
	    do_suppressions( displayed_part );
	    UF_func( UF_PART_set_display_part( displayed_part ) );

	    bu_log( "finished collecting suppressions\n" );
	    /* suppress them */
	    UF_func( UF_MODL_ask_list_count( suppress_list, &list_len ) );

	    if( list_len ) {
		    bu_log( "Suppressing %d features\n", list_len );
		    UF_func( UF_MODL_delete_feature( suppress_list ) );
	    }
    }
#endif

    curr_level = 0;
    comp_name = process_part( displayed_part, bn_mat_identity, part_name, NULL, NULL );
    if( comp_name ) {
	    bu_free( comp_name, "comp_name" );
    }

 bailout:
    if (cset) unload_sub_parts(cset);
    if (ugpart) UF_PART_close(ugpart, 1, 1);

    fprintf( stderr, "%s closed\n", av[i]);

    UF_terminate();

    bu_log( "\n\n\t%d parts facetized\n\t%d parts converted to Boolean form\n", parts_facetized, parts_bool );
    bu_log( "\t\t%d of the facetized parts were BREP models\n", parts_brep );

    elapsed_time = time( &end_time ) - start_time;
    bu_log( "Elapsed time: %02d:%02d:%02d\n", elapsed_time/3600, (elapsed_time%3600)/60, (elapsed_time%60) );

    return 0;

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
