/*                         D X F - G . C
 * BRL-CAD
 *
 * Copyright (C) 2004-2005 United States Government as represented by
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
 *
 */
/** @file dxf-g.c
 *
 */

#include "common.h"

/* system headers */
#include <stdio.h>
#include <math.h>
#ifdef HAVE_STRING_H
#  include <string.h>
#else
#  include <strings.h>
#endif
#if defined(HAVE_UNISTD_H)
#  include <unistd.h>
#else
#  if defined(HAVE_SYS_UNISTD_H)
#    include <sys/unistd.h>
#  endif
#endif
#include <ctype.h>

/* interface headers */
#include "machine.h"
#include "bu.h"
#include "vmath.h"
#include "bn.h"
#include "raytrace.h"
#include "wdb.h"


struct insert_data {
	fastf_t scale[3];
	fastf_t rotation;
	point_t insert_pt;
	vect_t extrude_dir;
};

struct state_data {
	struct bu_list l;
	struct block_list *curr_block;
	long file_offset;
	int state;
	int sub_state;
	mat_t xform;
};

static struct bu_list state_stack;
static struct state_data *curr_state;
static int curr_color=7;
static int ignore_colors=0;
static char *curr_layer_name;
static int color_by_layer=0;		/* flag, if set, colors are set by layer */

struct layer {
	char *name;			/* layer name */
	int color_number;		/* color */
	struct vert_root *vert_tree_root; /* root of vertex tree */
	int *part_tris;			/* list of triangles for current part */
	int max_tri;			/* number of triangles currently malloced */
	int curr_tri;			/* number of triangles currently being used */
	int line_count;
	int point_count;
	struct bu_ptbl solids;
	struct model *m;
	struct shell *s;
};

struct block_list {
	struct bu_list l;
	char *block_name;
	long offset;
	char handle[17];
	point_t base;
};

static struct bu_list block_head;
static struct block_list *curr_block=NULL;

static struct layer **layers=NULL;
static int max_layers;
static int next_layer;
static int curr_layer;

/* SECTIONS (states) */
#define UNKNOWN_SECTION		0
#define HEADER_SECTION		1
#define CLASSES_SECTION		2
#define TABLES_SECTION		3
#define BLOCKS_SECTION		4
#define ENTITIES_SECTION	5
#define OBJECTS_SECTION		6
#define THUMBNAILIMAGE_SECTION	7
#define NUM_SECTIONS		8

/* states for the ENTITIES section */
#define UNKNOWN_ENTITY_STATE		0
#define POLYLINE_ENTITY_STATE		1
#define POLYLINE_VERTEX_ENTITY_STATE	2
#define FACE3D_ENTITY_STATE		3
#define LINE_ENTITY_STATE		4
#define INSERT_ENTITY_STATE		5
#define POINT_ENTITY_STATE		6
#define CIRCLE_ENTITY_STATE		7
#define	ARC_ENTITY_STATE		8
#define DIMENSION_ENTITY_STATE		9
#define NUM_ENTITY_STATES		10

/* POLYLINE flags */
static int polyline_flag=0;
#define POLY_CLOSED		1
#define POLY_CURVE_FIT		2
#define POLY_SPLINE_FIT		4
#define	POLY_3D			8
#define	POLY_3D_MESH		16
#define	POLY_CLOSED_MESH	32
#define POLY_FACE_MESH		64
#define	POLY_PATTERN		128

/* POLYLINE VERTEX flags */
#define POLY_VERTEX_EXTRA	1
#define POLY_VERTEX_CURVE	2
#define POLY_VERTEX_SPLINE_V	8
#define POLY_VERTEX_SPLINE_C	16
#define POLY_VERTEX_3D_V	32
#define POLY_VERTEX_3D_M	64
#define POLY_VERTEX_FACE	128

/* states for the TABLES section */
#define UNKNOWN_TABLE_STATE	0
#define LAYER_TABLE_STATE	1
#define NUM_TABLE_STATES	2

static fastf_t *polyline_verts=NULL;
static int polyline_vertex_count=0;
static int polyline_vertex_max=0;
static int mesh_m_count=0;
static int mesh_n_count=0;
static int *polyline_vert_indices=NULL;
static int polyline_vert_indices_count=0;
static int polyline_vert_indices_max=0;
#define PVINDEX( _i, _j )	((_i)*mesh_n_count + (_j))
#define POLYLINE_VERTEX_BLOCK	10

static point_t pts[4];

#define UNKNOWN_ENTITY	0
#define POLYLINE_VERTEX		1

static int invisible=0;

#define ERROR_FLAG	-999
#define EOF_FLAG	-998

#define TOL_SQ	0.00001
#define LINELEN	2050
char line[LINELEN];

static char *usage="dxf-g [-c] [-d] [-v] [-t tolerance] [-s scale_factor] input_dxf_file output_file.g\n";

static FILE *dxf;
static struct rt_wdb *out_fp;
static char *output_file;
static char *dxf_file;
static int verbose=0;
static fastf_t tol=0.01;
static fastf_t tol_sq;
static char *base_name;
static char tmp_name[256];
static int segs_per_circle=32;
static fastf_t sin_delta, cos_delta;
static fastf_t delta_angle;
static point_t *circle_pts;
static fastf_t scale_factor;

#define TRI_BLOCK 512			/* number of triangles to malloc per call */

static int (*process_code[NUM_SECTIONS])( int code );
static int (*process_entities_code[NUM_ENTITY_STATES])( int code );
static int (*process_tables_sub_code[NUM_TABLE_STATES])( int code );

static int *int_ptr=NULL;
static int units=0;
static fastf_t units_conv[]={
	/* 0 */	1.0,
	/* 1 */	25.4,
	/* 2 */	304.8,
	/* 3 */	1609344.0,
	/* 4 */	1.0,
	/* 5 */	10.0,
	/* 6 */	1000.0,
	/* 7 */	1000000.0,
	/* 8 */	0.0000254,
	/* 9 */	0.0254,
	/* 10 */ 914.4,
	/* 11 */ 1.0e-7,
	/* 12 */ 1.0e-6,
	/* 13 */ 1.0e-3,
	/* 14 */ 100.0,
	/* 15 */ 10000.0,
	/* 16 */ 100000.0,
	/* 17 */ 1.0e+12,
	/* 18 */ 1.495979e+14,
	/* 19 */ 9.460730e+18,
	/* 20 */ 3.085678e+19
};

static unsigned char rgb[]={
	0, 0, 0,
	255, 0, 0,
	255, 255, 0,
	0, 255, 0,
	0, 255, 255,
	0, 0, 255,
	255, 0, 255,
	255, 255, 255,
	65, 65, 65,
	128, 128, 128,
	255, 0, 0,
	255, 128, 128,
	166, 0, 0,
	166, 83, 83,
	128, 0, 0,
	128, 64, 64,
	77, 0, 0,
	77, 38, 38,
	38, 0, 0,
	38, 19, 19,
	255, 64, 0,
	255, 159, 128,
	166, 41, 0,
	166, 104, 83,
	128, 32, 0,
	128, 80, 64,
	77, 19, 0,
	77, 48, 38,
	38, 10, 0,
	38, 24, 19,
	255, 128, 0,
	255, 191, 128,
	166, 83, 0,
	166, 124, 83,
	128, 64, 0,
	128, 96, 64,
	77, 38, 0,
	77, 57, 38,
	38, 19, 0,
	38, 29, 19,
	255, 191, 0,
	255, 223, 128,
	166, 124, 0,
	166, 145, 83,
	128, 96, 0,
	128, 112, 64,
	77, 57, 0,
	77, 67, 38,
	38, 29, 0,
	38, 33, 19,
	255, 255, 0,
	255, 255, 128,
	166, 166, 0,
	166, 166, 83,
	128, 128, 0,
	128, 128, 64,
	77, 77, 0,
	77, 77, 38,
	38, 38, 0,
	38, 38, 19,
	191, 255, 0,
	223, 255, 128,
	124, 166, 0,
	145, 166, 83,
	96, 128, 0,
	112, 128, 64,
	57, 77, 0,
	67, 77, 38,
	29, 38, 0,
	33, 38, 19,
	128, 255, 0,
	191, 255, 128,
	83, 166, 0,
	124, 166, 83,
	64, 128, 0,
	96, 128, 64,
	38, 77, 0,
	57, 77, 38,
	19, 38, 0,
	29, 38, 19,
	64, 255, 0,
	159, 255, 128,
	41, 166, 0,
	104, 166, 83,
	32, 128, 0,
	80, 128, 64,
	19, 77, 0,
	48, 77, 38,
	10, 38, 0,
	24, 38, 19,
	0, 255, 0,
	128, 255, 128,
	0, 166, 0,
	83, 166, 83,
	0, 128, 0,
	64, 128, 64,
	0, 77, 0,
	38, 77, 38,
	0, 38, 0,
	19, 38, 19,
	0, 255, 64,
	128, 255, 159,
	0, 166, 41,
	83, 166, 104,
	0, 128, 32,
	64, 128, 80,
	0, 77, 19,
	38, 77, 48,
	0, 38, 10,
	19, 38, 24,
	0, 255, 128,
	128, 255, 191,
	0, 166, 83,
	83, 166, 124,
	0, 128, 64,
	64, 128, 96,
	0, 77, 38,
	38, 77, 57,
	0, 38, 19,
	19, 38, 29,
	0, 255, 191,
	128, 255, 223,
	0, 166, 124,
	83, 166, 145,
	0, 128, 96,
	64, 128, 112,
	0, 77, 57,
	38, 77, 67,
	0, 38, 29,
	19, 38, 33,
	0, 255, 255,
	128, 255, 255,
	0, 166, 166,
	83, 166, 166,
	0, 128, 128,
	64, 128, 128,
	0, 77, 77,
	38, 77, 77,
	0, 38, 38,
	19, 38, 38,
	0, 191, 255,
	128, 223, 255,
	0, 124, 166,
	83, 145, 166,
	0, 96, 128,
	64, 112, 128,
	0, 57, 77,
	38, 67, 77,
	0, 29, 38,
	19, 33, 38,
	0, 128, 255,
	128, 191, 255,
	0, 83, 166,
	83, 124, 166,
	0, 64, 128,
	64, 96, 128,
	0, 38, 77,
	38, 57, 77,
	0, 19, 38,
	19, 29, 38,
	0, 64, 255,
	128, 159, 255,
	0, 41, 166,
	83, 104, 166,
	0, 32, 128,
	64, 80, 128,
	0, 19, 77,
	38, 48, 77,
	0, 10, 38,
	19, 24, 38,
	0, 0, 255,
	128, 128, 255,
	0, 0, 166,
	83, 83, 166,
	0, 0, 128,
	64, 64, 128,
	0, 0, 77,
	38, 38, 77,
	0, 0, 38,
	19, 19, 38,
	64, 0, 255,
	159, 128, 255,
	41, 0, 166,
	104, 83, 166,
	32, 0, 128,
	80, 64, 128,
	19, 0, 77,
	48, 38, 77,
	10, 0, 38,
	24, 19, 38,
	128, 0, 255,
	191, 128, 255,
	83, 0, 166,
	124, 83, 166,
	64, 0, 128,
	96, 64, 128,
	38, 0, 77,
	57, 38, 77,
	19, 0, 38,
	29, 19, 38,
	191, 0, 255,
	223, 128, 255,
	124, 0, 166,
	145, 83, 166,
	96, 0, 128,
	112, 64, 128,
	57, 0, 77,
	67, 38, 77,
	29, 0, 38,
	33, 19, 38,
	255, 0, 255,
	255, 128, 255,
	166, 0, 166,
	166, 83, 166,
	128, 0, 128,
	128, 64, 128,
	77, 0, 77,
	77, 38, 77,
	38, 0, 38,
	38, 19, 38,
	255, 0, 191,
	255, 128, 223,
	166, 0, 124,
	166, 83, 145,
	128, 0, 96,
	128, 64, 112,
	77, 0, 57,
	77, 38, 67,
	38, 0, 29,
	38, 19, 33,
	255, 0, 128,
	255, 128, 191,
	166, 0, 83,
	166, 83, 124,
	128, 0, 64,
	128, 64, 96,
	77, 0, 38,
	77, 38, 57,
	38, 0, 19,
	38, 19, 29,
	255, 0, 64,
	255, 128, 159,
	166, 0, 41,
	166, 83, 104,
	128, 0, 32,
	128, 64, 80,
	77, 0, 19,
	77, 38, 48,
	38, 0, 10,
	38, 19, 24,
	84, 84, 84,
	118, 118, 118,
	152, 152, 152,
	187, 187, 187,
	221, 221, 221,
	255, 255, 255 };


static char *
make_brlcad_name( char *line )
{
	char *name;
	char *c;

	name = bu_strdup( line );

	c = name;
	while( *c != '\0' ) {
		if( *c == '/' || *c == '[' || *c == ']' || *c == '*' || isspace( *c ) ) {
			*c = '_';
		}
		c++;
	}

	return( name );
}

static void
get_layer()
{
	int i;
	int old_layer=curr_layer;

	if( verbose ) {
		bu_log( "get_layer(): state = %d, substate = %d\n", curr_state->state, curr_state->sub_state );
	}
	/* do we already have a layer by this name and color */
	curr_layer = -1;
	for( i = 1 ; i < next_layer ; i++ ) {
		if( !color_by_layer && !ignore_colors && curr_color != 256 ) {
			if( layers[i]->color_number == curr_color && !strcmp( curr_layer_name, layers[i]->name ) ) {
				curr_layer = i;
				break;
			}
		} else {
			if( !strcmp( curr_layer_name, layers[i]->name ) ) {
				curr_layer = i;
				break;
			}
		}
	}

	if( curr_layer == -1 ) {
		/* add a new layer */
		if( next_layer >= max_layers ) {
			if( verbose ) {
				bu_log( "Creating new block of layers\n" );
			}
			max_layers += 5;
			layers = (struct layer **)bu_realloc( layers,
					    max_layers*sizeof( struct layer *), "layers" );
			for( i=0 ; i<5 ; i++ ) {
				BU_GETSTRUCT( layers[max_layers-i-1], layer );
			}
		}
		curr_layer = next_layer++;
		if( verbose ) {
			bu_log( "New layer: %s, color number: %d", line, curr_color );
		}
		layers[curr_layer]->name = bu_strdup( curr_layer_name );
		if( curr_state->state == ENTITIES_SECTION && 
		    (curr_state->sub_state == POLYLINE_ENTITY_STATE ||
		     curr_state->sub_state == POLYLINE_VERTEX_ENTITY_STATE) ) {
			layers[curr_layer]->vert_tree_root = layers[old_layer]->vert_tree_root;
		} else {
			layers[curr_layer]->vert_tree_root = create_vert_tree();
		}
		layers[curr_layer]->color_number = curr_color;
		bu_ptbl_init( &layers[curr_layer]->solids, 8, "layers[curr_layer]->solids" );
		if( verbose ) {
			bu_log( "\tNew layer name: %s\n", layers[curr_layer]->name );
		}
	}

	if( verbose && curr_layer != old_layer ) {
		bu_log( "changed to layer #%d, (m = x%x, s=x%x)\n", curr_layer, layers[curr_layer]->m, layers[curr_layer]->s );
	}
}

static void
create_nmg()
{
	struct model *m;
	struct nmgregion *r;

	m = nmg_mm();
	r = nmg_mrsv( m );
	layers[curr_layer]->s = BU_LIST_FIRST( shell, &r->s_hd );
	layers[curr_layer]->m = m;
}

/* routine to add a new triangle to the current part */
void
add_triangle( int v1, int v2, int v3, int layer )
{
	if( verbose ) {
		bu_log( "Adding triangle %d %d %d, to layer %s\n", v1, v2, v3, layers[layer]->name );
	}
	if( v1 == v2 || v2 == v3 || v3 == v1 ) {
		if( verbose ) {
			bu_log( "\tSkipping degenerate triangle\n" );
		}
		return;
	}
	if( layers[layer]->curr_tri >= layers[layer]->max_tri ) {
		/* allocate more memory for triangles */
		layers[layer]->max_tri += TRI_BLOCK;
		layers[layer]->part_tris = (int *)realloc( layers[layer]->part_tris,
							  sizeof( int ) * layers[layer]->max_tri * 3 );
		if( !layers[layer]->part_tris ) {
			bu_log( "ERROR: Failed to allocate memory for part triangles on layer %s\n",
				 layers[layer]->name);
			exit( 1 );
		}
	}

	/* fill in triangle info */
	layers[layer]->part_tris[layers[layer]->curr_tri*3 + 0] = v1;
	layers[layer]->part_tris[layers[layer]->curr_tri*3 + 1] = v2;
	layers[layer]->part_tris[layers[layer]->curr_tri*3 + 2] = v3;

	/* increment count */
	layers[layer]->curr_tri++;
}

static int
process_unknown_code( int code )
{
	switch( code ) {
	case 999:	/* comment */
		printf( "%s\n", line );
		break;
	case 0:		/* text string */
		if( !strncmp( line, "SECTION", 7 ) ) {
			curr_state->state = UNKNOWN_SECTION;
			break;
		} else if( !strncmp( line, "ENDSEC", 6 ) ) {
			curr_state->state = UNKNOWN_SECTION;
			break;
		}
		break;
	case 2:		/* name */
		if( !strncmp( line, "HEADER", 6 ) ) {
			curr_state->state = HEADER_SECTION;
			if( verbose ) {
				bu_log( "Change state to %d\n", curr_state->state );
			}
			break;
		} else if( !strncmp( line, "CLASSES", 7 ) ) {
			curr_state->state = CLASSES_SECTION;
			if( verbose ) {
				bu_log( "Change state to %d\n", curr_state->state );
			}
			break;
		} else if( !strncmp( line, "TABLES", 6 ) ) {
			curr_state->state = TABLES_SECTION;
			curr_state->sub_state = UNKNOWN_TABLE_STATE;
			if( verbose ) {
				bu_log( "Change state to %d\n", curr_state->state );
			}
			break;
		} else if( !strncmp( line, "BLOCKS", 6 ) ) {
			curr_state->state = BLOCKS_SECTION;
			if( verbose ) {
				bu_log( "Change state to %d\n", curr_state->state );
			}
			break;
		} else if( !strncmp( line, "ENTITIES", 8 ) ) {
			curr_state->state = ENTITIES_SECTION;
			curr_state->sub_state =UNKNOWN_ENTITY_STATE; 
			if( verbose ) {
				bu_log( "Change state to %d\n", curr_state->state );
			}
			break;
		} else if( !strncmp( line, "OBJECTS", 7 ) ) {
			curr_state->state = OBJECTS_SECTION;
			if( verbose ) {
				bu_log( "Change state to %d\n", curr_state->state );
			}
			break;
		} else if( !strncmp( line, "THUMBNAILIMAGE", 14 ) ) {
			curr_state->state = THUMBNAILIMAGE_SECTION;
			if( verbose ) {
				bu_log( "Change state to %d\n", curr_state->state );
			}
			break;
		}
		break;
	case 62:	/* color number */
		curr_color = atoi( line );
		break;
		}
	return( 0 );
}

static int
process_header_code( int code )
{
	switch( code ) {
	case 999:	/* comment */
		printf( "%s\n", line );
		break;
	case 0:		/* text string */
		if( !strncmp( line, "SECTION", 7 ) ) {
			curr_state->state = UNKNOWN_SECTION;
			break;
		} else if( !strncmp( line, "ENDSEC", 6 ) ) {
			curr_state->state = UNKNOWN_SECTION;
			break;
		}
		break;
	case 9:		/* variable name */
		if( !strncmp( line, "$INSUNITS", 9 ) ) {
			int_ptr = &units;
		} else if( !strcmp( line, "$CECOLOR" ) ) {
			int_ptr = &color_by_layer;
		}
		break;
	case 70:
	case 62:
		if( int_ptr ) {
			(*int_ptr) = atoi( line );
		}
		int_ptr = NULL;
		break;
	}
		
	return( 0 );
}

static int
process_classes_code( int code )
{
	switch( code ) {
	case 999:	/* comment */
		printf( "%s\n", line );
		break;
	case 0:		/* text string */
		if( !strncmp( line, "SECTION", 7 ) ) {
			curr_state->state = UNKNOWN_SECTION;
			break;
		} else if( !strncmp( line, "ENDSEC", 6 ) ) {
			curr_state->state = UNKNOWN_SECTION;
			break;
		}
		break;
	}
		
	return( 0 );
}

static int
process_tables_unknown_code( int code )
{

	switch( code ) {
	case 999:	/* comment */
		printf( "%s\n", line );
		break;
	case 0:		/* text string */
		if( !strcmp( line, "LAYER" ) ) {
			if( curr_layer_name ) {
				bu_free( curr_layer_name, "cur_layer_name" );
				curr_layer_name = NULL;
			}
			curr_color = 0;
			curr_state->sub_state = LAYER_TABLE_STATE;
			break;
		} else if( !strcmp( line, "ENDTAB" ) ) {
			if( curr_layer_name ) {
				bu_free( curr_layer_name, "cur_layer_name" );
				curr_layer_name = NULL;
			}
			curr_color = 0;
			curr_state->sub_state = UNKNOWN_TABLE_STATE;
			break;
		} else if( !strncmp( line, "SECTION", 7 ) ) {
			curr_state->state = UNKNOWN_SECTION;
			break;
		} else if( !strncmp( line, "ENDSEC", 6 ) ) {
			curr_state->state = UNKNOWN_SECTION;
			break;
		}
		break;
	}
		
	return( 0 );
}

static int
process_tables_layer_code( int code )
{
	switch( code ) {
	case 999:	/* comment */
		printf( "%s\n", line );
		break;
	case 2:		/* layer name */
		if( curr_layer_name ) {
			bu_free( curr_layer_name, "curr_layer_name" );
		}
		curr_layer_name = make_brlcad_name( line );
		if( verbose) {
		    bu_log( "In LAYER in TABLES, layer name = %s\n", curr_layer_name );
		}
		break;
	case 62:	/* layer color */
		curr_color = atoi( line );
		if( verbose) {
		    bu_log( "In LAYER in TABLES, layer color = %d\n", curr_color );
		}
		break;
	case 0:		/* text string */
		if( curr_layer_name && curr_color ) {
			get_layer();
		}

		if( curr_layer_name ) {
			bu_free( curr_layer_name, "cur_layer_name" );
			curr_layer_name = NULL;
		}
		curr_color = 0;
		curr_state->sub_state = UNKNOWN_TABLE_STATE;
		return( process_tables_unknown_code( code ) );
	}
		
	return( 0 );
}

static int
process_tables_code( int code )
{
	return( process_tables_sub_code[curr_state->sub_state]( code ) );
}

static int
process_blocks_code( int code )
{
	int len;
	int coord;

	switch( code ) {
	case 999:	/* comment */
		printf( "%s\n", line );
		break;
	case 0:		/* text string */
		if( !strncmp( line, "SECTION", 7 ) ) {
			curr_state->state = UNKNOWN_SECTION;
			break;
		} else if( !strncmp( line, "ENDSEC", 6 ) ) {
			curr_state->state = UNKNOWN_SECTION;
			break;
		} else if( !strcmp( line, "ENDBLK" ) ) {
			curr_block = NULL;
			break;
		} else if( !strncmp( line, "BLOCK", 5 ) ) {
			/* start of a new block */
			BU_GETSTRUCT( curr_block, block_list );
			curr_block->offset = ftell( dxf );
			BU_LIST_INSERT( &(block_head), &(curr_block->l) );
			break;
		}
		break;
	case 2:		/* block name */
		if( curr_block ) {
			curr_block->block_name = bu_strdup( line );
			if( verbose ) {
				bu_log( "BLOCK %s begins at %ld\n",
					curr_block->block_name,
					curr_block->offset );
			}
		}
		break;
	case 5:		/* block handle */
		if( curr_block ) {
			len = strlen( line );
			if( len > 16 ) {
				len = 16;
			}
			strncpy( curr_block->handle, line, len );
			curr_block->handle[len] = '\0';
		}
		break;
	case 10:
	case 20:
	case 30:
		if( curr_block ) {
			coord = code / 10 - 1;
			curr_block->base[coord] = atof( line ) * units_conv[units] * scale_factor;
		}
		break;
	}
		
	return( 0 );
}

void
add_polyface_mesh_triangle( int v1, int v2, int v3 )
{
}

void
add_polyline_vertex( fastf_t x, fastf_t y, fastf_t z )
{
	if( !polyline_verts ) {
		polyline_verts = (fastf_t *)bu_malloc( POLYLINE_VERTEX_BLOCK*3*sizeof( fastf_t ), "polyline_verts" );
		polyline_vertex_count = 0;
		polyline_vertex_max = POLYLINE_VERTEX_BLOCK;
	} else if( polyline_vertex_count >= polyline_vertex_max ) {
		polyline_vertex_max += POLYLINE_VERTEX_BLOCK;
		polyline_verts = (fastf_t *)bu_realloc( polyline_verts, polyline_vertex_max * 3 * sizeof( fastf_t ), "polyline_verts" );
	}

	VSET( &polyline_verts[polyline_vertex_count*3], x, y, z );
	polyline_vertex_count++;

	if( verbose ) {
		bu_log( "Added polyline vertex (%g %g %g) #%d\n", x, y, z, polyline_vertex_count );
	}
}

static int
process_point_entities_code( int code )
{
	static point_t pt;
	int coord;

	switch( code ) {
	case 8:		/* layer name */
		if( curr_layer_name ) {
			bu_free( curr_layer_name, "curr_layer_name" );
		}
		curr_layer_name = make_brlcad_name( line );
		break;
	case 10:
	case 20:
	case 30:
		coord = code / 10 - 1;
		pt[coord] = atof( line ) * units_conv[units] * scale_factor;
		break;
	case 62:	/* color number */
		curr_color = atoi( line );
		break;
	case 0:
		get_layer();
		layers[curr_layer]->point_count++;
		sprintf( tmp_name, "point.%d", layers[curr_layer]->point_count );
		(void)mk_sph( out_fp, tmp_name, pt, 0.1 );
		(void)bu_ptbl_ins( &(layers[curr_layer]->solids), (long *)bu_strdup( tmp_name ) );
		curr_state->sub_state = UNKNOWN_ENTITY_STATE;
		process_entities_code[curr_state->sub_state]( code );
		break;
	}

	return( 0 );
}

static int
process_entities_polyline_vertex_code( int code )
{
	static fastf_t x, y, z;
	static int face[4];
	static int vertex_flag;
	int coord;

	switch( code ) {
	case -1:	/* initialize */
		face[0] = 0;
		face[1] = 0;
		face[2] = 0;
		face[3] = 0;
		vertex_flag = 0;
		return( 0 );
	case 8:		/* layer name */
		if( curr_layer_name ) {
			bu_free( curr_layer_name, "curr_layer_name" );
		}
		curr_layer_name = make_brlcad_name( line );
		break;
	case 70:	/* vertex flag */
		vertex_flag = atoi( line );
		break;
	case 71:
	case 72:
	case 73:
	case 74:
		coord = (code % 70) - 1;
		face[coord] = abs( atoi( line ) );
		break;
	case 0:
		get_layer();
		if( vertex_flag == POLY_VERTEX_FACE ) {
			add_triangle( polyline_vert_indices[face[0]-1],
				      polyline_vert_indices[face[1]-1],
				      polyline_vert_indices[face[2]-1],
				      curr_layer );
			if( face[3] > 0 ) {
			add_triangle( polyline_vert_indices[face[2]-1],
				      polyline_vert_indices[face[3]-1],
				      polyline_vert_indices[face[0]-1],
				      curr_layer );
			}
		} else if( vertex_flag & POLY_VERTEX_3D_M) {
			if( polyline_vert_indices_count >= polyline_vert_indices_max ) {
				polyline_vert_indices_max += POLYLINE_VERTEX_BLOCK;
				polyline_vert_indices = (int *)bu_realloc( polyline_vert_indices,
									   polyline_vert_indices_max * sizeof( int ),
									   "polyline_vert_indices" );
			}
			polyline_vert_indices[polyline_vert_indices_count++] = Add_vert( x, y, z,
									     layers[curr_layer]->vert_tree_root,
									     tol_sq );
			if( verbose) {
				bu_log( "Added 3D mesh vertex (%g %g %g) index = %d, number = %d\n",
					x, y, z, polyline_vert_indices[polyline_vert_indices_count-1],
					polyline_vert_indices_count-1 );
			}
		} else {
			add_polyline_vertex( x, y, z );
		}
		curr_state->sub_state = POLYLINE_ENTITY_STATE;
		if( verbose ) {
			bu_log( "sub_state changed to %d\n", curr_state->sub_state );
		}
		return( process_entities_code[curr_state->sub_state]( code ) );
	case 999:	/* comment */
		printf( "%s\n", line );
		break;
	case 10:
		x = atof( line ) * units_conv[units] * scale_factor;
		break;
	case 20:
		y = atof( line ) * units_conv[units] * scale_factor;
		break;
	case 30:
		z = atof( line ) * units_conv[units] * scale_factor;
		break;
	case 62:	/* color number */
		curr_color = atoi( line );
		break;
	}
		
	return( 0 );
}

static int
process_entities_polyline_code( int code )
{

	switch( code ) {
	case 999:	/* comment */
		printf( "%s\n", line );
		break;
	case 0:		/* text string */
		get_layer();
		if( !strncmp( line, "SEQEND", 6 ) ) {
			/* build any polyline meshes here */
			if( polyline_flag & POLY_3D_MESH ) {
				if( polyline_vert_indices_count == 0 ) {
					return( 0 );
				} else if( polyline_vert_indices_count != mesh_m_count * mesh_n_count ) {
					bu_log( "Incorrect number of vertices for polygon mesh!!!\n" );
					polyline_vert_indices_count = 0;
				} else {
					int i, j;

					if( polyline_vert_indices_count >= polyline_vert_indices_max ) {
						polyline_vert_indices_max = ((polyline_vert_indices_count % POLYLINE_VERTEX_BLOCK) + 1) *
							POLYLINE_VERTEX_BLOCK;
						polyline_vert_indices = (int *)bu_realloc( polyline_vert_indices,
											   polyline_vert_indices_max * sizeof( int ),
											   "polyline_vert_indices" );
					}

					if( mesh_m_count < 2 ) {
						if( mesh_n_count > 4 ) {
							bu_log( "Cannot handle polyline meshes with m<2 and n>4\n");
							polyline_vert_indices_count=0;
							polyline_vert_indices_count = 0;
							break;
						}
						if( mesh_n_count < 3 ) {
							polyline_vert_indices_count=0;
							polyline_vert_indices_count = 0;
							break;
						}
						add_triangle( polyline_vert_indices[0],
							      polyline_vert_indices[1],
							      polyline_vert_indices[2],
							      curr_layer );
						if( mesh_n_count == 4 ) {
							add_triangle( polyline_vert_indices[2],
								      polyline_vert_indices[3],
								      polyline_vert_indices[0],
								      curr_layer );
						}
					}

					for( j=1 ; j<mesh_n_count ; j++ ) {
						for( i=1 ; i<mesh_m_count ; i++ ) {
							add_triangle( polyline_vert_indices[PVINDEX(i-1,j-1)],
								      polyline_vert_indices[PVINDEX(i-1,j)],
								      polyline_vert_indices[PVINDEX(i,j-1)],
								      curr_layer );
							add_triangle( polyline_vert_indices[PVINDEX(i-1,j-1)],
								      polyline_vert_indices[PVINDEX(i,j-1)],
								      polyline_vert_indices[PVINDEX(i,j)],
								      curr_layer );
						}
					}
					polyline_vert_indices_count=0;
					polyline_vertex_count = 0;
				}
			} else {
				int i;
				struct edgeuse *eu;
				struct vertex *v0=NULL, *v1=NULL, *v2=NULL;

				if( polyline_vertex_count > 1 ) {
					if( !layers[curr_layer]->m ) {
						create_nmg();
					}

					for( i=0 ; i<polyline_vertex_count-1 ; i++ ) {
						eu = nmg_me( v1, v2, layers[curr_layer]->s );
						if( i == 0 ) {
							v1 = eu->vu_p->v_p;
							nmg_vertex_gv( v1, polyline_verts );
							v0 = v1;
						}
						v2 = eu->eumate_p->vu_p->v_p;
						nmg_vertex_gv( v2, &polyline_verts[(i+1)*3] );
						if( verbose ) {
							bu_log( "Wire edge (polyline): (%g %g %g) <-> (%g %g %g)\n",
								V3ARGS( v1->vg_p->coord ),
								V3ARGS( v2->vg_p->coord ) );
						}
						v1 = v2;
						v2 = NULL;
					}

					if( polyline_flag & POLY_CLOSED ) {
						v2 = v0;
						(void)nmg_me( v1, v2, layers[curr_layer]->s );
						if( verbose ) {
							bu_log( "Wire edge (closing polyline): (%g %g %g) <-> (%g %g %g)\n",
								V3ARGS( v1->vg_p->coord ),
								V3ARGS( v2->vg_p->coord ) );
						}
					}
				}
				polyline_vert_indices_count=0;	
				polyline_vertex_count = 0;
			}

			curr_state->state = ENTITIES_SECTION;
			curr_state->sub_state = UNKNOWN_ENTITY_STATE;
			if( verbose ) {
				bu_log( "sub_state changed to %d\n", curr_state->sub_state );
			}
			break;
		} else if( !strncmp( line, "VERTEX", 6 ) ) {
			if( verbose)
				bu_log( "Found a POLYLINE VERTEX\n" );
			curr_state->sub_state = POLYLINE_VERTEX_ENTITY_STATE;
			process_entities_code[POLYLINE_VERTEX_ENTITY_STATE]( -1 );
			break;
		} else {
			if( verbose ) {
				bu_log( "Unrecognized text string while in polyline entity: %s\n", line );
			}
			break;
		}
	case 70:	/* polyline flag */
		polyline_flag = atoi( line );
		break;
	case 71:
		mesh_m_count = atoi( line );
		break;
	case 72:
		mesh_n_count = atoi( line );
		break;
	case 62:	/* color number */
		curr_color = atoi( line );
		break;
	case 60:
		invisible = atoi( line );
		break;
	case 8:		/* layer name */
		if( curr_layer_name ) {
			bu_free( curr_layer_name, "curr_layer_name" );
		}
		curr_layer_name = make_brlcad_name( line );
		break;
	}
		
	return( 0 );
}

static int
process_entities_unknown_code( int code )
{
	struct state_data *tmp_state;

	invisible = 0;

	switch( code ) {
	case 999:	/* comment */
		printf( "%s\n", line );
		break;
	case 0:		/* text string */
		if( !strncmp( line, "SECTION", 7 ) ) {
			curr_state->state = UNKNOWN_SECTION;
			break;
		} else if( !strncmp( line, "ENDSEC", 6 ) ) {
			curr_state->state = UNKNOWN_SECTION;
			break;
		} else if( !strncmp( line, "POLYLINE", 8 ) ) {
			if( verbose)
				bu_log( "Found a POLYLINE\n" );
			curr_state->sub_state = POLYLINE_ENTITY_STATE;
			if( verbose ) {
				bu_log( "sub_state changed to %d\n", curr_state->sub_state );
			}
			break;
		} else if( !strncmp( line, "3DFACE", 6 ) ) {
			curr_state->sub_state = FACE3D_ENTITY_STATE;
			if( verbose ) {
				bu_log( "sub_state changed to %d\n", curr_state->sub_state );
			}
			break;
		} else if( !strcmp( line, "CIRCLE" ) ) {
			curr_state->sub_state = CIRCLE_ENTITY_STATE;
			if( verbose ) {
				bu_log( "sub_state changed to %d\n", curr_state->sub_state );
			}
			break;
		} else if( !strcmp( line, "ARC" ) ) {
			curr_state->sub_state = ARC_ENTITY_STATE;
			if( verbose ) {
				bu_log( "sub_state changed to %d\n", curr_state->sub_state );
			}
			break;
		} else if( !strcmp( line, "DIMENSION" ) ) {
			curr_state->sub_state = DIMENSION_ENTITY_STATE;
			if( verbose ) {
				bu_log( "sub_state changed to %d\n", curr_state->sub_state );
			}
			break;
		} else if( !strncmp( line, "LINE", 4 ) ) {
			curr_state->sub_state = LINE_ENTITY_STATE;
			if( verbose ) {
				bu_log( "sub_state changed to %d\n", curr_state->sub_state );
			}
			break;
		} else if( !strcmp( line, "POINT" ) ) {
			curr_state->sub_state = POINT_ENTITY_STATE;
			if( verbose ) {
				bu_log( "sub_state changed to %d\n", curr_state->sub_state );
			}
			break;
		} else if( !strncmp( line, "INSERT", 6 ) ) {
			curr_state->sub_state = INSERT_ENTITY_STATE;
			if( verbose ) {
				bu_log( "sub_state changed to %d\n", curr_state->sub_state );
			}
			break;
		} else if( !strcmp( line, "ENDBLK" ) ) {
			/* found end of an inserted block, pop the state stack */
			tmp_state = curr_state;
			BU_LIST_POP( state_data, &state_stack, curr_state );
			if( !curr_state ) {
				bu_log( "ERROR: end of block encountered while not inserting!!!\n" );
				curr_state = tmp_state;
				break;
			}
			bu_free( (char *)tmp_state, "curr_state" );
			fseek( dxf, curr_state->file_offset, SEEK_SET );
			curr_state->sub_state = UNKNOWN_ENTITY_STATE;
			if( verbose ) {
				bu_log( "Popped state at end of inserted block (seeked to %ld)\n", curr_state->file_offset );
			}
			break;
		} else {
			if( verbose ) {
				bu_log( "Unrecognized text string while in unknown entities state: %s\n",
					line );
			}
			break;
		}
	}
		
	return( 0 );
}

static void
insert_init( struct insert_data *ins )
{
	VSETALL( ins->scale, 1.0 );
	ins->rotation = 0.0;
	VSETALL( ins->insert_pt, 0.0 );
	VSET( ins->extrude_dir, 0, 0, 1 );
}

static int
process_insert_entities_code( int code )
{
	static struct insert_data ins;
	static struct state_data *new_state=NULL;
	struct block_list *blk;
	int coord;

	if( !new_state ) {
		insert_init( &ins );
		BU_GETSTRUCT( new_state, state_data );
		*new_state = *curr_state;
		if( verbose ) {
			bu_log( "Created a new state for INSERT\n" );
		}
	}

	switch( code ) {
	case 8:		/* layer name */
		if( curr_layer_name ) {
			bu_free( curr_layer_name, "curr_layer_name" );
		}
		curr_layer_name = make_brlcad_name( line );
		break;
	case 2:		/* block name */
		for( BU_LIST_FOR( blk, block_list, &block_head ) ) {
			if( !strcmp( blk->block_name, line ) ) {
				break;
			}
		}
		if( BU_LIST_IS_HEAD( blk, &block_head ) ) {
			bu_log( "ERROR: INSERT references non-existent block (%s)\n", line );
			bu_log( "\tignoring missing block\n" );
			blk = NULL;
		}
		new_state->curr_block = blk;
		if( verbose && blk ) {
			bu_log( "Inserting block %s\n", blk->block_name );
		}
		break;
	case 10:
	case 20:
	case 30:
		coord = (code / 10) - 1;
		ins.insert_pt[coord] = atof( line );
		break;
	case 41:
	case 42:
	case 43:
		coord = (code % 40) - 1;
		ins.scale[coord] = atof( line );
		break;
	case 50:
		ins.rotation = atof( line );
		break;
	case 62:	/* color number */
		curr_color = atoi( line );
		break;
	case 70:
	case 71:
		if( atof( line ) != 1 ) {
			bu_log( "Cannot yet handle insertion of a pattern\n\tignoring\n" );
		}
		break;
	case 44:
	case 45:
		break;
	case 210:
	case 220:
	case 230:
		coord = ((code / 10) % 20) - 1;
		ins.extrude_dir[coord] = atof( line );
		break;
	case 0:		/* end of this insert */
		if( new_state->curr_block ) {
			BU_LIST_PUSH( &state_stack, &(curr_state->l) );
			curr_state = new_state;
			new_state = NULL;
			fseek( dxf, curr_state->curr_block->offset, SEEK_SET );
			curr_state->state = ENTITIES_SECTION;
			curr_state->sub_state = UNKNOWN_ENTITY_STATE;
			if( verbose ) {
				bu_log( "Changing state for INSERT\n" );
				bu_log( "seeked to %ld\n", curr_state->curr_block->offset );
			}
		}
		break;
	}

	return( 0 );
}

static int
process_line_entities_code( int code )
{
	int vert_no;
	int coord;
	static point_t line_pt[2];
	struct edgeuse *eu;

	switch( code ) {
	case 8:
		if( curr_layer_name ) {
			bu_free( curr_layer_name, "curr_layer_name" );
		}
		curr_layer_name = make_brlcad_name( line );
		if( verbose ) {
		    bu_log( "LINE is in layer: %s\n", curr_layer_name );
		}
		break;
	case 10:
	case 20:
	case 30:
	case 11:
	case 21:
	case 31:
		vert_no = code % 10;
		coord = code / 10 - 1;
		line_pt[vert_no][coord] = atof( line ) * units_conv[units] * scale_factor;
		if( verbose ) {
			bu_log( "LINE vertex #%d coord #%d = %g\n", vert_no, coord, line_pt[vert_no][coord] );
		}
		break;
	case 62:	/* color number */
		curr_color = atoi( line );
		break;
	case 0:
		/* end of this line */
		get_layer();
		if( verbose ) {
			bu_log( "Found end of LINE\n" );
		}

		layers[curr_layer]->line_count++;

		if( !layers[curr_layer]->m ) {
			create_nmg();
		}

		/* create a wire edge in the NMG */
		eu = nmg_me( NULL, NULL, layers[curr_layer]->s );
		nmg_vertex_gv( eu->vu_p->v_p, line_pt[0] );
		nmg_vertex_gv( eu->eumate_p->vu_p->v_p, line_pt[1] );
		if( verbose ) {
			bu_log( "Wire edge (line): (%g %g %g) <-> (%g %g %g)\n",
				V3ARGS(eu->vu_p->v_p->vg_p->coord ),
				V3ARGS(eu->eumate_p->vu_p->v_p->vg_p->coord ) );
		}

		curr_state->sub_state = UNKNOWN_ENTITY_STATE;
		process_entities_code[curr_state->sub_state]( code );
		break;
	}
	
	return( 0 );
}

static int
process_circle_entities_code( int code )
{
	static point_t center;
	static fastf_t radius;
	int coord, i;
	struct vertex *v0=NULL, *v1=NULL, *v2=NULL;
	struct edgeuse *eu;

	switch( code ) {
	case 8:		/* layer name */
		if( curr_layer_name ) {
			bu_free( curr_layer_name, "curr_layer_name" );
		}
		curr_layer_name = make_brlcad_name( line );
		break;
	case 10:
	case 20:
	case 30:
		coord = code / 10 - 1;
		center[coord] = atof( line ) * units_conv[units] * scale_factor;
		if( verbose ) {
			bu_log( "CIRCLE center coord #%d = %g\n", coord, center[coord] );
		}
		break;
	case 40:
		radius = atof( line ) * units_conv[units] * scale_factor;
		break;
	case 62:	/* color number */
		curr_color = atoi( line );
		break;
	case 0:
		/* end of this circle entity
		 * make a series of wire edges in the NMG to approximate a circle
		 */

		get_layer();
		if( verbose ) {
			bu_log( "Found a circle\n" );
		}

		if( !layers[curr_layer]->m ) {
			create_nmg();
		}

		/* calculate circle at origin first */
		VSET( circle_pts[0], radius, 0.0, 0.0 );
		for( i=1 ; i<segs_per_circle ; i++ ) {
			circle_pts[i][X] = circle_pts[i-1][X]*cos_delta - circle_pts[i-1][Y]*sin_delta;
			circle_pts[i][Y] = circle_pts[i-1][Y]*cos_delta + circle_pts[i-1][X]*sin_delta;
		}

		/* move everything to the specified center */
		for( i=0 ; i<segs_per_circle ; i++ ) {
			VADD2( circle_pts[i], circle_pts[i], center );
		}

		/* make nmg wire edges */
		for( i=0 ; i<segs_per_circle ; i++ ) {
			if( i+1 == segs_per_circle ) {
				v2 = v0;
			}
			eu = nmg_me( v1, v2, layers[curr_layer]->s );
			if( i == 0 ) {
				v1 = eu->vu_p->v_p;
				v0 = v1;
				nmg_vertex_gv( v1, circle_pts[0] );
			}
			v2 = eu->eumate_p->vu_p->v_p;
			if( i+1 < segs_per_circle ) {
				nmg_vertex_gv( v2, circle_pts[i+1] );
			}
			if( verbose ) {
				bu_log( "Wire edge (circle): (%g %g %g) <-> (%g %g %g)\n",
					V3ARGS( v1->vg_p->coord ),
					V3ARGS( v2->vg_p->coord ) );
			}
			v1 = v2;
			v2 = NULL;
		}

		curr_state->sub_state = UNKNOWN_ENTITY_STATE;
		process_entities_code[curr_state->sub_state]( code );
		break;
	}

	return( 0 );
}

static int
process_dimension_entities_code( int code )
{
	static point_t def_pt={0.0, 0.0, 0.0};
	static char *block_name=NULL;
	static struct state_data *new_state=NULL;
	struct block_list *blk;
	int coord;

	switch( code ) {
	case 10:
	case 20:
	case 30:
		coord = (code / 10) - 1;
		def_pt[coord] = atof( line ) * units_conv[units] * scale_factor;
		break;
	case 8:		/* layer name */
		if( curr_layer_name ) {
			bu_free( curr_layer_name, "curr_layer_name" );
		}
		curr_layer_name = make_brlcad_name( line );
		break;
	case 2:	/* block name */
		block_name = bu_strdup( line );
		break;
	case 0:
	       if( block_name != NULL  ) {
		   /* insert this dimension block */
		   get_layer();
		   BU_GETSTRUCT( new_state, state_data );
		   *new_state = *curr_state;
		   if( verbose ) {
                       bu_log( "Created a new state for DIMENSION\n" );
		   }
		   for( BU_LIST_FOR( blk, block_list, &block_head ) ) {
		       if (block_name) {
			   if( !strcmp( blk->block_name, block_name ) ) {
			       break;
			   }
		       }
		   }
		   if( BU_LIST_IS_HEAD( blk, &block_head ) ) {
                       bu_log( "ERROR: DIMENSION references non-existent block (%s)\n", block_name );
                       bu_log( "\tignoring missing block\n" );
                       blk = NULL;
		   }
		   new_state->curr_block = blk;
		   if( verbose && blk ) {
                       bu_log( "Inserting block %s\n", blk->block_name );
		   }

		   if (block_name) {
		       bu_free( block_name, "block_name" );
		   }

		   if( new_state->curr_block ) {
                       BU_LIST_PUSH( &state_stack, &(curr_state->l) );
                       curr_state = new_state;
                       new_state = NULL;
                       fseek( dxf, curr_state->curr_block->offset, SEEK_SET );
                       curr_state->state = ENTITIES_SECTION;
                       curr_state->sub_state = UNKNOWN_ENTITY_STATE;
                       if( verbose ) {
			   bu_log( "Changing state for INSERT\n" );
			   bu_log( "seeked to %ld\n", curr_state->curr_block->offset );
                       }
		   }
	       }
	       else
	       {
		   curr_state->sub_state = UNKNOWN_ENTITY_STATE;
		   process_entities_code[curr_state->sub_state]( code );
	       }
	       break;
	}

	return( 0 );
}

static int
process_arc_entities_code( int code )
{
	static point_t center={0,0,0};
	static fastf_t radius;
	static fastf_t start_angle, end_angle;
	int num_segs;
	int coord, i;
	struct vertex *v0=NULL, *v1=NULL, *v2=NULL;
	struct edgeuse *eu;

	switch( code ) {
	case 8:		/* layer name */
		if( curr_layer_name ) {
			bu_free( curr_layer_name, "curr_layer_name" );
		}
		curr_layer_name = make_brlcad_name( line );
		break;
	case 10:
	case 20:
	case 30:
		coord = code / 10 - 1;
		center[coord] = atof( line ) * units_conv[units] * scale_factor;
		if( verbose ) {
			bu_log( "ARC center coord #%d = %g\n", coord, center[coord] );
		}
		break;
	case 40:
		radius = atof( line ) * units_conv[units] * scale_factor;
		if( verbose) {
			bu_log( "ARC radius = %g\n", radius );
		}
		break;
	case 50:
		start_angle = atof( line );
		if( verbose ) {
			bu_log( "ARC start angle = %g\n", start_angle );
		}
		break;
	case 51:
		end_angle = atof( line );
		if( verbose ) {
			bu_log( "ARC end angle = %g\n", end_angle );
		}
		break;
	case 62:	/* color number */
		curr_color = atoi( line );
		break;
	case 0:
		/* end of this arc entity
		 * make a series of wire edges in the NMG to approximate an arc
		 */

		get_layer();
		if( verbose ) {
			bu_log( "Found an arc\n" );
		}

		if( !layers[curr_layer]->m ) {
			create_nmg();
		}

		while( end_angle < start_angle ) {
			end_angle += 360.0;
		}

		/* calculate arc at origin first */
		num_segs = (end_angle - start_angle) / 360.0 * segs_per_circle;
		start_angle *= M_PI / 180.0;
		end_angle *= M_PI / 180.0;
		if( verbose ) {
			bu_log( "arc has %d segs\n", num_segs );
		}

		if( num_segs < 1 ) {
			num_segs = 1;
		}
		VSET( circle_pts[0], radius * cos( start_angle ), radius * sin( start_angle ), 0.0 );
		for( i=1 ; i<num_segs ; i++ ) {
			circle_pts[i][X] = circle_pts[i-1][X]*cos_delta - circle_pts[i-1][Y]*sin_delta;
			circle_pts[i][Y] = circle_pts[i-1][Y]*cos_delta + circle_pts[i-1][X]*sin_delta;
		}
		circle_pts[num_segs][X] = radius * cos( end_angle );
		circle_pts[num_segs][Y] = radius * sin( end_angle );
		num_segs++;

		if( verbose ) {
			bu_log( "ARC points calculated:\n" );
			for( i=0 ; i<num_segs ; i++ ) {
				bu_log( "\t point #%d: (%g %g %g)\n", i, V3ARGS( circle_pts[i] ) );
			}
		}

		/* move everything to the specified center */
		for( i=0 ; i<num_segs ; i++ ) {
			VADD2( circle_pts[i], circle_pts[i], center );
		}

		if( verbose ) {
			bu_log( "ARC points after move to center at (%g %g %g):\n", V3ARGS( center ) );
			for( i=0 ; i<num_segs ; i++ ) {
				bu_log( "\t point #%d: (%g %g %g)\n", i, V3ARGS( circle_pts[i] ) );
			}
		}

		/* make nmg wire edges */
		for( i=1 ; i<num_segs ; i++ ) {
			if( i == num_segs ) {
				v2 = v0;
			}
			eu = nmg_me( v1, v2, layers[curr_layer]->s );
			if( i == 1 ) {
				v1 = eu->vu_p->v_p;
				v0 = v1;
				nmg_vertex_gv( v1, circle_pts[0] );
			}
			v2 = eu->eumate_p->vu_p->v_p;
			if( i < segs_per_circle ) {
				nmg_vertex_gv( v2, circle_pts[i] );
			}
			if( verbose ) {
				bu_log( "Wire edge (arc) #%d (%g %g %g) <-> (%g %g %g)\n", i,
					V3ARGS( v1->vg_p->coord ),
					V3ARGS( v2->vg_p->coord ) );
			}
			v1 = v2;
			v2 = NULL;
		}

		VSETALL( center, 0.0 );
		for( i=0 ; i<segs_per_circle ; i++ ) {
			VSETALL( circle_pts[i], 0.0 );
		}

		curr_state->sub_state = UNKNOWN_ENTITY_STATE;
		process_entities_code[curr_state->sub_state]( code );
		break;
	}

	return( 0 );
}

static int
process_3dface_entities_code( int code )
{
	int vert_no;
	int coord;
	int face[5];

	switch( code ) {
	case 8:
		if( curr_layer_name ) {
			bu_free( curr_layer_name, "curr_layer_name" );
		}
		curr_layer_name = make_brlcad_name( line );
		break;
	case 10:
	case 20:
	case 30:
	case 11:
	case 21:
	case 31:
	case 12:
	case 22:
	case 32:
	case 13:
	case 23:
	case 33:
		vert_no = code % 10;
		coord = code / 10 - 1;
		pts[vert_no][coord] = atof( line ) * units_conv[units] * scale_factor;
		if( verbose ) {
			bu_log( "3dface vertex #%d coord #%d = %g\n", vert_no, coord, pts[vert_no][coord] );
		}
		if( vert_no == 2 ) {
			pts[3][coord] = pts[2][coord];
		}
		break;
	case 62:	/* color number */
		curr_color = atoi( line );
		break;
	case 0:
		/* end of this 3dface */
		get_layer();
		if( verbose ) {
			bu_log( "Found end of 3DFACE\n" );
		}
		if( verbose ) {
			bu_log( "\tmaking two triangles\n" );
		}
		for( vert_no=0 ; vert_no<4; vert_no++ ) {
			face[vert_no] = Add_vert( V3ARGS( pts[vert_no]),
						  layers[curr_layer]->vert_tree_root,
						  tol_sq );
		}
		add_triangle( face[0], face[1], face[2], curr_layer );
		add_triangle( face[2], face[3], face[0], curr_layer );
		if( verbose ) {
			bu_log( "finished face\n" );
		}

		curr_state->sub_state = UNKNOWN_ENTITY_STATE;
		process_entities_code[curr_state->sub_state]( code );
		break;
	}
	
	return( 0 );
}

static int
process_entity_code( int code )
{
	return( process_entities_code[curr_state->sub_state]( code ) );
}

static int
process_objects_code( int code )
{
	switch( code ) {
	case 999:	/* comment */
		printf( "%s\n", line );
		break;
	case 0:		/* text string */
		if( !strncmp( line, "SECTION", 7 ) ) {
			curr_state->state = UNKNOWN_SECTION;
			break;
		} else if( !strncmp( line, "ENDSEC", 6 ) ) {
			curr_state->state = UNKNOWN_SECTION;
			break;
		}
		break;
	}
		
	return( 0 );
}

static int
process_thumbnail_code( int code )
{
	switch( code ) {
	case 999:	/* comment */
		printf( "%s\n", line );
		break;
	case 0:		/* text string */
		if( !strncmp( line, "SECTION", 7 ) ) {
			curr_state->state = UNKNOWN_SECTION;
			break;
		} else if( !strncmp( line, "ENDSEC", 6 ) ) {
			curr_state->state = UNKNOWN_SECTION;
			break;
		}
		break;
	}
		
	return( 0 );
}

int
readcodes()
{
	int code;
	int line_len;
	static int line_num=0;

	curr_state->file_offset = ftell( dxf );

	if( fgets( line, LINELEN, dxf ) == NULL ) {
		return( ERROR_FLAG );
	} else {
		code = atoi( line );
	}

	if( fgets( line, LINELEN, dxf ) == NULL ) {
		return( ERROR_FLAG );
	}

	if( !strncmp( line, "EOF", 3 ) ) {
		return( EOF_FLAG );
	}

	line_len = strlen( line );
	if( line_len ) {
		line[line_len-1] = '\0';
		line_len--;
	}

	if( line_len && line[line_len-1] == '\r' ) {
		line[line_len-1] = '\0';
		line_len--;
	}

	if( verbose ) {
		line_num++;
		bu_log( "%d:\t%d\n", line_num, code );
		line_num++;
		bu_log( "%d:\t%s\n", line_num, line );
	}

	return( code );
}

int
main( int argc, char *argv[] )
{
	struct bu_list head_all;
	int name_len;
	char *ptr1, *ptr2;
	int code;
	int c;
	int i;

	tol_sq = tol * tol;

	delta_angle = 2.0 * M_PI / (fastf_t)segs_per_circle;
	sin_delta = sin( delta_angle );
	cos_delta = cos( delta_angle );

	/* get command line arguments */
	scale_factor = 1.0;
	while ((c = getopt(argc, argv, "cdvt:s:")) != EOF)
	{
		switch( c )
		{
		        case 's':	/* scale factor */
				scale_factor = atof( optarg );
				if( scale_factor < SQRT_SMALL_FASTF ) {
					bu_log( "scale factor too small\n" );
					bu_log( "%s", usage );
					exit( 1 );
				}
				break;
		        case 'c':	/* ignore colors */
				ignore_colors = 1;
				break;
		        case 'd':	/* debug */
				bu_debug = BU_DEBUG_COREDUMP;
				break;
			case 't':	/* tolerance */
				tol = atof( optarg );
				tol_sq = tol * tol;
				break;
			case 'v':	/* verbose */
				verbose = 1;
				break;
			default:
				bu_bomb( usage );
				break;
		}
	}

	if( argc - optind < 2 ) {
		bu_bomb( usage );
	}

	dxf_file = argv[optind++];
	output_file = argv[optind];

	if( (out_fp = wdb_fopen( output_file )) == NULL ) {
		bu_log( "Cannot open output file (%s)\n", output_file );
		perror( output_file );
		bu_bomb( "Cannot open output file\n" );
	}

	if( (dxf=fopen( dxf_file, "r")) == NULL ) {
		bu_log( "Cannot open DXF file (%s)\n", dxf_file );
		perror( dxf_file );
		bu_bomb( "Cannot open DXF file\n" );
	}

	ptr1 = strrchr( dxf_file , '/' );
	if( ptr1 == NULL )
		ptr1 = dxf_file;
	else
		ptr1++;
	ptr2 = strchr( ptr1 , '.' );

	if( ptr2 == NULL )
		name_len = strlen( ptr1 );
	else
		name_len = ptr2 - ptr1;

	base_name = (char *)bu_malloc( name_len + 1 , "base_name" );
	strncpy( base_name , ptr1 , name_len );

	mk_id( out_fp , base_name );

	BU_LIST_INIT( &block_head );

	process_code[UNKNOWN_SECTION] = process_unknown_code;
	process_code[HEADER_SECTION] = process_header_code;
	process_code[CLASSES_SECTION] = process_classes_code;
	process_code[TABLES_SECTION] = process_tables_code;
	process_code[BLOCKS_SECTION] = process_blocks_code;
	process_code[ENTITIES_SECTION] = process_entity_code;
	process_code[OBJECTS_SECTION] = process_objects_code;
	process_code[THUMBNAILIMAGE_SECTION] = process_thumbnail_code;

	process_entities_code[UNKNOWN_ENTITY_STATE] = process_entities_unknown_code;
	process_entities_code[POLYLINE_ENTITY_STATE] = process_entities_polyline_code;
	process_entities_code[POLYLINE_VERTEX_ENTITY_STATE] = process_entities_polyline_vertex_code;
	process_entities_code[FACE3D_ENTITY_STATE] = process_3dface_entities_code;
	process_entities_code[LINE_ENTITY_STATE] = process_line_entities_code;
	process_entities_code[INSERT_ENTITY_STATE] = process_insert_entities_code;
	process_entities_code[POINT_ENTITY_STATE] = process_point_entities_code;
	process_entities_code[CIRCLE_ENTITY_STATE] = process_circle_entities_code;
	process_entities_code[ARC_ENTITY_STATE] = process_arc_entities_code;
	process_entities_code[DIMENSION_ENTITY_STATE] = process_dimension_entities_code;

	process_tables_sub_code[UNKNOWN_TABLE_STATE] = process_tables_unknown_code;
	process_tables_sub_code[LAYER_TABLE_STATE] = process_tables_layer_code;

	/* create storage for circles */
	circle_pts = (point_t *)bu_calloc( segs_per_circle, sizeof( point_t ), "circle_pts" );
	for( i=0 ; i<segs_per_circle ; i++ ) {
		VSETALL( circle_pts[i], 0.0 );
	}

	/* initialize state stack */
	BU_LIST_INIT( &state_stack );

	/* create initial state */
	BU_GETSTRUCT( curr_state, state_data );
	curr_state->file_offset = 0;
	curr_state->state = UNKNOWN_SECTION;
	curr_state->sub_state = UNKNOWN_ENTITY_STATE;
	MAT_IDN( curr_state->xform );

	/* make space for 5 layers to start */
	max_layers = 5;
	next_layer = 1;
	curr_layer = 0;
	layers = (struct layer **)bu_calloc( 5, sizeof( struct layer *), "layers" );
	for( i=0 ; i<max_layers ; i++ ) {
		BU_GETSTRUCT( layers[i], layer );
	}
	layers[0]->name = bu_strdup( "noname" );
	layers[0]->color_number = 7;	/* default white */
	layers[0]->vert_tree_root = create_vert_tree();
	bu_ptbl_init( &layers[0]->solids, 8, "layers[curr_layer]->solids" );

	curr_color = layers[0]->color_number;
	curr_layer_name = bu_strdup( layers[0]->name );

	while( (code=readcodes()) > -900 ) {
		process_code[curr_state->state](code);
	}

	BU_LIST_INIT( &head_all );
	for( i=0 ; i<next_layer ; i++ ) {
		struct bu_list head;
		int j;

		BU_LIST_INIT( &head );

		if( layers[i]->curr_tri || BU_PTBL_END( &layers[i]->solids ) || layers[i]->m ) {
			bu_log( "LAYER: %s\n", layers[i]->name );
		}

		if( layers[i]->curr_tri && layers[i]->vert_tree_root->curr_vert > 2 ) {
			bu_log( "\t%d triangles\n", layers[i]->curr_tri );
			sprintf( tmp_name, "bot.s%d", i );
			if( mk_bot( out_fp, tmp_name, RT_BOT_SURFACE, RT_BOT_UNORIENTED,0,
				    layers[i]->vert_tree_root->curr_vert, layers[i]->curr_tri, layers[i]->vert_tree_root->the_array,
				    layers[i]->part_tris, (fastf_t *)NULL, (struct bu_bitv *)NULL ) ) {
				bu_log( "Failed to make Bot\n" );
			} else {
				(void)mk_addmember( tmp_name, &head, NULL, WMOP_UNION );
			}
		}

		if( BU_PTBL_END( &layers[i]->solids ) > 0 ) {
			bu_log( "\t%d points\n", BU_PTBL_END( &layers[i]->solids ) );
		}

		for( j=0 ; j<BU_PTBL_END( &layers[i]->solids ) ; j++ ) {
			(void)mk_addmember((char *)BU_PTBL_GET( &layers[i]->solids, j ), &head,
					    NULL, WMOP_UNION );
			bu_free( (char *)BU_PTBL_GET( &layers[i]->solids, j), "solid_name" );
		}

		if( layers[i]->m ) {
			char name[32];

			sprintf( name, "nmg.%d", i );
			mk_nmg( out_fp, name, layers[i]->m );
			(void)mk_addmember( name, &head, NULL, WMOP_UNION );
		}

		if( layers[i]->line_count ) {
			bu_log( "\t%d lines\n", layers[i]->line_count );
		}

		if( layers[i]->point_count ) {
			bu_log( "\t%d points\n", layers[i]->point_count );
		}

		if( BU_LIST_NON_EMPTY( &head ) ) {
			unsigned char *tmp_rgb;
			struct bu_vls comb_name;

			if( layers[i]->color_number < 0 ) {
				tmp_rgb = &rgb[7];
			} else {
				tmp_rgb = &rgb[layers[i]->color_number*3];
			}
			bu_vls_init( &comb_name );
			bu_vls_printf( &comb_name, "%s.c.%d", layers[i]->name, i );
			if( mk_comb( out_fp, bu_vls_addr( &comb_name ), &head, 1, NULL, NULL,
				     tmp_rgb, 1, 0, 1, 100, 0, 0, 0 ) ) {
				bu_log( "Failed to make region %s\n", layers[i]->name );
			} else {
				(void)mk_addmember( bu_vls_addr( &comb_name ), &head_all, NULL, WMOP_UNION );
			}
		}

	}


	if( BU_LIST_NON_EMPTY( &head_all ) ) {
		struct bu_vls top_name;
		int count=0;


		bu_vls_init(&top_name);
		bu_vls_strcpy( &top_name, "all" );
		while( db_lookup( out_fp->dbip, bu_vls_addr( &top_name ), LOOKUP_QUIET ) != DIR_NULL ) {
			count++;
			bu_vls_trunc( &top_name, 0 );
			bu_vls_printf( &top_name, "all.%d", count );
		}

		(void)mk_comb( out_fp, bu_vls_addr( &top_name ), &head_all, 0, NULL, NULL, NULL, 0, 0, 0, 0, 0, 0, 0 );
	}

	return( 0 );
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
