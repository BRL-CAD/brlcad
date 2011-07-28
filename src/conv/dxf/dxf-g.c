/*                         D X F - G . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2011 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file dxf-g.c
 *
 * Program to convert from the DXF file format to BRL-CAD format
 *
 */

#include "common.h"

/* system headers */
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <string.h>
#include <ctype.h>
#include "bio.h"

/* interface headers */
#include "bu.h"
#include "vmath.h"
#include "bn.h"
#include "raytrace.h"
#include "wdb.h"
#include "nurb.h"

/* private headers */
#include "./dxf.h"


static int overstrikemode = 0;
static int underscoremode = 0;

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
    size_t max_tri;			/* number of triangles currently malloced */
    size_t curr_tri;			/* number of triangles currently being used */
    size_t line_count;
    size_t solid_count;
    size_t polyline_count;
    size_t lwpolyline_count;
    size_t ellipse_count;
    size_t circle_count;
    size_t spline_count;
    size_t arc_count;
    size_t text_count;
    size_t mtext_count;
    size_t attrib_count;
    size_t dimension_count;
    size_t leader_count;
    size_t face3d_count;
    size_t point_count;
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
#define TEXT_ENTITY_STATE		10
#define	SOLID_ENTITY_STATE		11
#define LWPOLYLINE_ENTITY_STATE		12
#define MTEXT_ENTITY_STATE		13
#define LEADER_ENTITY_STATE		14
#define ATTRIB_ENTITY_STATE		15
#define ATTDEF_ENTITY_STATE		16
#define ELLIPSE_ENTITY_STATE		17
#define SPLINE_ENTITY_STATE		18
#define NUM_ENTITY_STATES		19

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

/* SPLINE flags */
#define SPLINE_CLOSED		1
#define SPLINE_PERIODIC		2
#define SPLINE_RATIONAL		4
#define SPLINE_PLANAR		8
#define SPLINE_LINEAR		16

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
#define MAX_LINE_SIZE	2050
char line[MAX_LINE_SIZE];

static char *usage="dxf-g [-c] [-d] [-v] [-t tolerance] [-s scale_factor] input_file.dxf output_file.g\n";

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
static int splineSegs=16;
static fastf_t sin_delta, cos_delta;
static fastf_t delta_angle;
static point_t *circle_pts;
static fastf_t scale_factor;
static struct bu_list free_hd;

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


static char *
make_brlcad_name( const char *nameline )
{
    char *name;
    char *c;

    name = bu_strdup( nameline );

    c = name;
    while ( *c != '\0' ) {
	if ( *c == '/' || *c == '[' || *c == ']' || *c == '*' || isspace( *c ) ) {
	    *c = '_';
	}
	c++;
    }

    return name;
}

static void
get_layer()
{
    int i;
    int old_layer=curr_layer;

    if ( verbose ) {
	bu_log( "get_layer(): state = %d, substate = %d\n", curr_state->state, curr_state->sub_state );
    }
    /* do we already have a layer by this name and color */
    curr_layer = -1;
    for ( i = 1; i < next_layer; i++ ) {
	if ( !color_by_layer && !ignore_colors && curr_color != 256 ) {
	    if ( layers[i]->color_number == curr_color && BU_STR_EQUAL( curr_layer_name, layers[i]->name ) ) {
		curr_layer = i;
		break;
	    }
	} else {
	    if ( BU_STR_EQUAL( curr_layer_name, layers[i]->name ) ) {
		curr_layer = i;
		break;
	    }
	}
    }

    if ( curr_layer == -1 ) {
	/* add a new layer */
	if ( next_layer >= max_layers ) {
	    if ( verbose ) {
		bu_log( "Creating new block of layers\n" );
	    }
	    max_layers += 5;
	    layers = (struct layer **)bu_realloc( layers, max_layers*sizeof( struct layer *), "layers" );
	    for ( i=0; i<5; i++ ) {
		BU_GETSTRUCT( layers[max_layers-i-1], layer );
	    }
	}
	curr_layer = next_layer++;
	if ( verbose ) {
	    bu_log( "New layer: %s, color number: %d", line, curr_color );
	}
	layers[curr_layer]->name = bu_strdup( curr_layer_name );
	if ( curr_state->state == ENTITIES_SECTION &&
	     (curr_state->sub_state == POLYLINE_ENTITY_STATE ||
	      curr_state->sub_state == POLYLINE_VERTEX_ENTITY_STATE) ) {
	    layers[curr_layer]->vert_tree_root = layers[old_layer]->vert_tree_root;
	} else {
	    layers[curr_layer]->vert_tree_root = create_vert_tree();
	}
	layers[curr_layer]->color_number = curr_color;
	bu_ptbl_init( &layers[curr_layer]->solids, 8, "layers[curr_layer]->solids" );
	if ( verbose ) {
	    bu_log( "\tNew layer name: %s\n", layers[curr_layer]->name );
	}
    }

    if ( verbose && curr_layer != old_layer ) {
	bu_log( "changed to layer #%d, (m = %p, s=%p)\n",
		curr_layer,
		(void *)layers[curr_layer]->m,
		(void *)layers[curr_layer]->s );
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
    if ( verbose ) {
	bu_log( "Adding triangle %d %d %d, to layer %s\n", v1, v2, v3, layers[layer]->name );
    }
    if ( v1 == v2 || v2 == v3 || v3 == v1 ) {
	if ( verbose ) {
	    bu_log( "\tSkipping degenerate triangle\n" );
	}
	return;
    }
    if ( layers[layer]->curr_tri >= layers[layer]->max_tri ) {
	/* allocate more memory for triangles */
	layers[layer]->max_tri += TRI_BLOCK;
	layers[layer]->part_tris = (int *)bu_realloc( layers[layer]->part_tris, sizeof(int) * layers[layer]->max_tri * 3, "layers[layer]->part_tris" );
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
    switch ( code ) {
	case 999:	/* comment */
	    printf( "%s\n", line );
	    break;
	case 0:		/* text string */
	    if ( !strncmp( line, "SECTION", 7 ) ) {
		curr_state->state = UNKNOWN_SECTION;
		break;
	    } else if ( !strncmp( line, "ENDSEC", 6 ) ) {
		curr_state->state = UNKNOWN_SECTION;
		break;
	    }
	    break;
	case 2:		/* name */
	    if ( !strncmp( line, "HEADER", 6 ) ) {
		curr_state->state = HEADER_SECTION;
		if ( verbose ) {
		    bu_log( "Change state to %d\n", curr_state->state );
		}
		break;
	    } else if ( !strncmp( line, "CLASSES", 7 ) ) {
		curr_state->state = CLASSES_SECTION;
		if ( verbose ) {
		    bu_log( "Change state to %d\n", curr_state->state );
		}
		break;
	    } else if ( !strncmp( line, "TABLES", 6 ) ) {
		curr_state->state = TABLES_SECTION;
		curr_state->sub_state = UNKNOWN_TABLE_STATE;
		if ( verbose ) {
		    bu_log( "Change state to %d\n", curr_state->state );
		}
		break;
	    } else if ( !strncmp( line, "BLOCKS", 6 ) ) {
		curr_state->state = BLOCKS_SECTION;
		if ( verbose ) {
		    bu_log( "Change state to %d\n", curr_state->state );
		}
		break;
	    } else if ( !strncmp( line, "ENTITIES", 8 ) ) {
		curr_state->state = ENTITIES_SECTION;
		curr_state->sub_state =UNKNOWN_ENTITY_STATE;
		if ( verbose ) {
		    bu_log( "Change state to %d\n", curr_state->state );
		}
		break;
	    } else if ( !strncmp( line, "OBJECTS", 7 ) ) {
		curr_state->state = OBJECTS_SECTION;
		if ( verbose ) {
		    bu_log( "Change state to %d\n", curr_state->state );
		}
		break;
	    } else if ( !strncmp( line, "THUMBNAILIMAGE", 14 ) ) {
		curr_state->state = THUMBNAILIMAGE_SECTION;
		if ( verbose ) {
		    bu_log( "Change state to %d\n", curr_state->state );
		}
		break;
	    }
	    break;
	case 62:	/* color number */
	    curr_color = atoi( line );
	    break;
    }
    return 0;
}

static int
process_header_code( int code )
{
    switch ( code ) {
	case 999:	/* comment */
	    printf( "%s\n", line );
	    break;
	case 0:		/* text string */
	    if ( !strncmp( line, "SECTION", 7 ) ) {
		curr_state->state = UNKNOWN_SECTION;
		break;
	    } else if ( !strncmp( line, "ENDSEC", 6 ) ) {
		curr_state->state = UNKNOWN_SECTION;
		break;
	    }
	    break;
	case 9:		/* variable name */
	    if ( !strncmp( line, "$INSUNITS", 9 ) ) {
		int_ptr = &units;
	    } else if ( BU_STR_EQUAL( line, "$CECOLOR" ) ) {
		int_ptr = &color_by_layer;
	    } else if ( BU_STR_EQUAL( line, "$SPLINESEGS" ) ) {
		int_ptr = &splineSegs;
	    }
	    break;
	case 70:
	case 62:
	    if ( int_ptr ) {
		(*int_ptr) = atoi( line );
	    }
	    int_ptr = NULL;
	    break;
    }

    return 0;
}

static int
process_classes_code( int code )
{
    switch ( code ) {
	case 999:	/* comment */
	    printf( "%s\n", line );
	    break;
	case 0:		/* text string */
	    if ( !strncmp( line, "SECTION", 7 ) ) {
		curr_state->state = UNKNOWN_SECTION;
		break;
	    } else if ( !strncmp( line, "ENDSEC", 6 ) ) {
		curr_state->state = UNKNOWN_SECTION;
		break;
	    }
	    break;
    }

    return 0;
}

static int
process_tables_unknown_code( int code )
{

    switch ( code ) {
	case 999:	/* comment */
	    printf( "%s\n", line );
	    break;
	case 0:		/* text string */
	    if ( BU_STR_EQUAL( line, "LAYER" ) ) {
		if ( curr_layer_name ) {
		    bu_free( curr_layer_name, "cur_layer_name" );
		    curr_layer_name = NULL;
		}
		curr_color = 0;
		curr_state->sub_state = LAYER_TABLE_STATE;
		break;
	    } else if ( BU_STR_EQUAL( line, "ENDTAB" ) ) {
		if ( curr_layer_name ) {
		    bu_free( curr_layer_name, "cur_layer_name" );
		    curr_layer_name = NULL;
		}
		curr_color = 0;
		curr_state->sub_state = UNKNOWN_TABLE_STATE;
		break;
	    } else if ( !strncmp( line, "SECTION", 7 ) ) {
		curr_state->state = UNKNOWN_SECTION;
		break;
	    } else if ( !strncmp( line, "ENDSEC", 6 ) ) {
		curr_state->state = UNKNOWN_SECTION;
		break;
	    }
	    break;
    }

    return 0;
}

static int
process_tables_layer_code( int code )
{
    switch ( code ) {
	case 999:	/* comment */
	    printf( "%s\n", line );
	    break;
	case 2:		/* layer name */
	    if ( curr_layer_name ) {
		bu_free( curr_layer_name, "curr_layer_name" );
	    }
	    curr_layer_name = make_brlcad_name( line );
	    if ( verbose) {
		bu_log( "In LAYER in TABLES, layer name = %s\n", curr_layer_name );
	    }
	    break;
	case 62:	/* layer color */
	    curr_color = atoi( line );
	    if ( verbose) {
		bu_log( "In LAYER in TABLES, layer color = %d\n", curr_color );
	    }
	    break;
	case 0:		/* text string */
	    if ( curr_layer_name && curr_color ) {
		get_layer();
	    }

	    if ( curr_layer_name ) {
		bu_free( curr_layer_name, "cur_layer_name" );
		curr_layer_name = NULL;
	    }
	    curr_color = 0;
	    curr_state->sub_state = UNKNOWN_TABLE_STATE;
	    return process_tables_unknown_code( code );
    }

    return 0;
}

static int
process_tables_code( int code )
{
    return process_tables_sub_code[curr_state->sub_state]( code );
}

static int
process_blocks_code( int code )
{
    size_t len;
    int coord;

    switch ( code ) {
	case 999:	/* comment */
	    printf( "%s\n", line );
	    break;
	case 0:		/* text string */
	    if ( !strncmp( line, "SECTION", 7 ) ) {
		curr_state->state = UNKNOWN_SECTION;
		break;
	    } else if ( !strncmp( line, "ENDSEC", 6 ) ) {
		curr_state->state = UNKNOWN_SECTION;
		break;
	    } else if ( BU_STR_EQUAL( line, "ENDBLK" ) ) {
		curr_block = NULL;
		break;
	    } else if ( !strncmp( line, "BLOCK", 5 ) ) {
		/* start of a new block */
		BU_GETSTRUCT( curr_block, block_list );
		curr_block->offset = ftell( dxf );
		BU_LIST_INSERT( &(block_head), &(curr_block->l) );
		break;
	    }
	    break;
	case 2:		/* block name */
	    if ( curr_block && curr_block->block_name == NULL ) {
		curr_block->block_name = bu_strdup( line );
		if ( verbose ) {
		    bu_log( "BLOCK %s begins at %ld\n",
			    curr_block->block_name,
			    curr_block->offset );
		}
	    }
	    break;
	case 5:		/* block handle */
	    if ( curr_block && curr_block->handle == NULL ) {
		len = strlen( line );
		if ( len > 16 ) {
		    len = 16;
		}
		bu_strlcpy( curr_block->handle, line, len );
	    }
	    break;
	case 10:
	case 20:
	case 30:
	    if ( curr_block ) {
		coord = code / 10 - 1;
		curr_block->base[coord] = atof( line ) * units_conv[units] * scale_factor;
	    }
	    break;
    }

    return 0;
}

void
add_polyline_vertex( fastf_t x, fastf_t y, fastf_t z )
{
    if ( !polyline_verts ) {
	polyline_verts = (fastf_t *)bu_malloc( POLYLINE_VERTEX_BLOCK*3*sizeof( fastf_t ), "polyline_verts" );
	polyline_vertex_count = 0;
	polyline_vertex_max = POLYLINE_VERTEX_BLOCK;
    } else if ( polyline_vertex_count >= polyline_vertex_max ) {
	polyline_vertex_max += POLYLINE_VERTEX_BLOCK;
	polyline_verts = (fastf_t *)bu_realloc( polyline_verts, polyline_vertex_max * 3 * sizeof( fastf_t ), "polyline_verts" );
    }

    VSET( &polyline_verts[polyline_vertex_count*3], x, y, z );
    polyline_vertex_count++;

    if ( verbose ) {
	bu_log( "Added polyline vertex (%g %g %g) #%d\n", x, y, z, polyline_vertex_count );
    }
}

static int
process_point_entities_code( int code )
{
    static point_t pt;
    point_t tmp_pt;
    int coord;

    switch ( code ) {
	case 8:		/* layer name */
	    if ( curr_layer_name ) {
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
	    MAT4X3PNT( tmp_pt, curr_state->xform, pt );
	    sprintf( tmp_name, "point.%lu", (long unsigned int)layers[curr_layer]->point_count );
	    (void)mk_sph( out_fp, tmp_name, tmp_pt, 0.1 );
	    (void)bu_ptbl_ins( &(layers[curr_layer]->solids), (long *)bu_strdup( tmp_name ) );
	    curr_state->sub_state = UNKNOWN_ENTITY_STATE;
	    process_entities_code[curr_state->sub_state]( code );
	    break;
    }

    return 0;
}

static int
process_entities_polyline_vertex_code( int code )
{
    static fastf_t x, y, z;
    static int face[4];
    static int vertex_flag;
    int coord;

    switch ( code ) {
	case -1:	/* initialize */
	    face[0] = 0;
	    face[1] = 0;
	    face[2] = 0;
	    face[3] = 0;
	    vertex_flag = 0;
	    return 0;
	case 8:		/* layer name */
	    if ( curr_layer_name ) {
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
	    if ( vertex_flag == POLY_VERTEX_FACE ) {
		add_triangle( polyline_vert_indices[face[0]-1],
			      polyline_vert_indices[face[1]-1],
			      polyline_vert_indices[face[2]-1],
			      curr_layer );
		if ( face[3] > 0 ) {
		    add_triangle( polyline_vert_indices[face[2]-1],
				  polyline_vert_indices[face[3]-1],
				  polyline_vert_indices[face[0]-1],
				  curr_layer );
		}
	    } else if ( vertex_flag & POLY_VERTEX_3D_M) {
		point_t tmp_pt1, tmp_pt2;
		if ( polyline_vert_indices_count >= polyline_vert_indices_max ) {
		    polyline_vert_indices_max += POLYLINE_VERTEX_BLOCK;
		    polyline_vert_indices = (int *)bu_realloc( polyline_vert_indices,
							       polyline_vert_indices_max * sizeof( int ),
							       "polyline_vert_indices" );
		}
		VSET( tmp_pt1, x, y, z );
		MAT4X3PNT( tmp_pt2, curr_state->xform, tmp_pt1 );
		polyline_vert_indices[polyline_vert_indices_count++] = Add_vert( tmp_pt2[X], tmp_pt2[Y], tmp_pt2[Z],
										 layers[curr_layer]->vert_tree_root,
										 tol_sq );
		if ( verbose) {
		    bu_log( "Added 3D mesh vertex (%g %g %g) index = %d, number = %d\n",
			    x, y, z, polyline_vert_indices[polyline_vert_indices_count-1],
			    polyline_vert_indices_count-1 );
		}
	    } else {
		add_polyline_vertex( x, y, z );
	    }
	    curr_state->sub_state = POLYLINE_ENTITY_STATE;
	    if ( verbose ) {
		bu_log( "sub_state changed to %d\n", curr_state->sub_state );
	    }
	    return process_entities_code[curr_state->sub_state]( code );
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

    return 0;
}

static int
process_entities_polyline_code( int code )
{

    switch ( code ) {
	case 999:	/* comment */
	    printf( "%s\n", line );
	    break;
	case 0:		/* text string */
	    get_layer();
	    if ( !strncmp( line, "SEQEND", 6 ) ) {
		/* build any polyline meshes here */
		if ( polyline_flag & POLY_3D_MESH ) {
		    if ( polyline_vert_indices_count == 0 ) {
			return 0;
		    } else if ( polyline_vert_indices_count != mesh_m_count * mesh_n_count ) {
			bu_log( "Incorrect number of vertices for polygon mesh!!!\n" );
			polyline_vert_indices_count = 0;
		    } else {
			int i, j;

			if ( polyline_vert_indices_count >= polyline_vert_indices_max ) {
			    polyline_vert_indices_max = ((polyline_vert_indices_count % POLYLINE_VERTEX_BLOCK) + 1) *
				POLYLINE_VERTEX_BLOCK;
			    polyline_vert_indices = (int *)bu_realloc( polyline_vert_indices,
								       polyline_vert_indices_max * sizeof( int ),
								       "polyline_vert_indices" );
			}

			if ( mesh_m_count < 2 ) {
			    if ( mesh_n_count > 4 ) {
				bu_log( "Cannot handle polyline meshes with m<2 and n>4\n");
				polyline_vert_indices_count=0;
				polyline_vert_indices_count = 0;
				break;
			    }
			    if ( mesh_n_count < 3 ) {
				polyline_vert_indices_count=0;
				polyline_vert_indices_count = 0;
				break;
			    }
			    add_triangle( polyline_vert_indices[0],
					  polyline_vert_indices[1],
					  polyline_vert_indices[2],
					  curr_layer );
			    if ( mesh_n_count == 4 ) {
				add_triangle( polyline_vert_indices[2],
					      polyline_vert_indices[3],
					      polyline_vert_indices[0],
					      curr_layer );
			    }
			}

			for ( j=1; j<mesh_n_count; j++ ) {
			    for ( i=1; i<mesh_m_count; i++ ) {
				add_triangle( polyline_vert_indices[PVINDEX(i-1, j-1)],
					      polyline_vert_indices[PVINDEX(i-1, j)],
					      polyline_vert_indices[PVINDEX(i, j-1)],
					      curr_layer );
				add_triangle( polyline_vert_indices[PVINDEX(i-1, j-1)],
					      polyline_vert_indices[PVINDEX(i, j-1)],
					      polyline_vert_indices[PVINDEX(i, j)],
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

		    if ( polyline_vertex_count > 1 ) {
			if ( !layers[curr_layer]->m ) {
			    create_nmg();
			}

			for ( i=0; i<polyline_vertex_count-1; i++ ) {
			    eu = nmg_me( v1, v2, layers[curr_layer]->s );
			    if ( i == 0 ) {
				v1 = eu->vu_p->v_p;
				nmg_vertex_gv( v1, polyline_verts );
				v0 = v1;
			    }
			    v2 = eu->eumate_p->vu_p->v_p;
			    nmg_vertex_gv( v2, &polyline_verts[(i+1)*3] );
			    if ( verbose ) {
				bu_log( "Wire edge (polyline): (%g %g %g) <-> (%g %g %g)\n",
					V3ARGS( v1->vg_p->coord ),
					V3ARGS( v2->vg_p->coord ) );
			    }
			    v1 = v2;
			    v2 = NULL;
			}

			if ( polyline_flag & POLY_CLOSED ) {
			    v2 = v0;
			    (void)nmg_me( v1, v2, layers[curr_layer]->s );
			    if ( verbose ) {
				bu_log( "Wire edge (closing polyline): (%g %g %g) <-> (%g %g %g)\n",
					V3ARGS( v1->vg_p->coord ),
					V3ARGS( v2->vg_p->coord ) );
			    }
			}
		    }
		    polyline_vert_indices_count=0;
		    polyline_vertex_count = 0;
		}

		layers[curr_layer]->polyline_count++;
		curr_state->state = ENTITIES_SECTION;
		curr_state->sub_state = UNKNOWN_ENTITY_STATE;
		if ( verbose ) {
		    bu_log( "sub_state changed to %d\n", curr_state->sub_state );
		}
		break;
	    } else if ( !strncmp( line, "VERTEX", 6 ) ) {
		if ( verbose)
		    bu_log( "Found a POLYLINE VERTEX\n" );
		curr_state->sub_state = POLYLINE_VERTEX_ENTITY_STATE;
		process_entities_code[POLYLINE_VERTEX_ENTITY_STATE]( -1 );
		break;
	    } else {
		if ( verbose ) {
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
	    if ( curr_layer_name ) {
		bu_free( curr_layer_name, "curr_layer_name" );
	    }
	    curr_layer_name = make_brlcad_name( line );
	    break;
    }

    return 0;
}

static int
process_entities_unknown_code( int code )
{
    struct state_data *tmp_state;

    invisible = 0;

    switch ( code ) {
	case 999:	/* comment */
	    printf( "%s\n", line );
	    break;
	case 0:		/* text string */
	    if ( !strncmp( line, "SECTION", 7 ) ) {
		curr_state->state = UNKNOWN_SECTION;
		break;
	    } else if ( !strncmp( line, "ENDSEC", 6 ) ) {
		curr_state->state = UNKNOWN_SECTION;
		break;
	    } else if ( !strncmp( line, "POLYLINE", 8 ) ) {
		if ( verbose)
		    bu_log( "Found a POLYLINE\n" );
		curr_state->sub_state = POLYLINE_ENTITY_STATE;
		if ( verbose ) {
		    bu_log( "sub_state changed to %d\n", curr_state->sub_state );
		}
		break;
	    } else if ( !strncmp( line, "LWPOLYLINE", 10 ) ) {
		if ( verbose)
		    bu_log( "Found a LWPOLYLINE\n" );
		curr_state->sub_state = LWPOLYLINE_ENTITY_STATE;
		if ( verbose ) {
		    bu_log( "sub_state changed to %d\n", curr_state->sub_state );
		}
		break;
	    } else if ( !strncmp( line, "3DFACE", 6 ) ) {
		curr_state->sub_state = FACE3D_ENTITY_STATE;
		if ( verbose ) {
		    bu_log( "sub_state changed to %d\n", curr_state->sub_state );
		}
		break;
	    } else if ( BU_STR_EQUAL( line, "CIRCLE" ) ) {
		curr_state->sub_state = CIRCLE_ENTITY_STATE;
		if ( verbose ) {
		    bu_log( "sub_state changed to %d\n", curr_state->sub_state );
		}
		break;
	    } else if ( BU_STR_EQUAL( line, "ELLIPSE" ) ) {
		curr_state->sub_state = ELLIPSE_ENTITY_STATE;
		if ( verbose ) {
		    bu_log( "sub_state changed to %d\n", curr_state->sub_state );
		}
		break;
	    } else if ( BU_STR_EQUAL( line, "SPLINE" ) ) {
		curr_state->sub_state = SPLINE_ENTITY_STATE;
		if ( verbose ) {
		    bu_log( "sub_state changed to %d\n", curr_state->sub_state );
		}
		break;
	    } else if ( BU_STR_EQUAL( line, "ARC" ) ) {
		curr_state->sub_state = ARC_ENTITY_STATE;
		if ( verbose ) {
		    bu_log( "sub_state changed to %d\n", curr_state->sub_state );
		}
		break;
	    } else if ( BU_STR_EQUAL( line, "DIMENSION" ) ) {
		curr_state->sub_state = DIMENSION_ENTITY_STATE;
		if ( verbose ) {
		    bu_log( "sub_state changed to %d\n", curr_state->sub_state );
		}
		break;
	    } else if ( !strncmp( line, "LINE", 4 ) ) {
		curr_state->sub_state = LINE_ENTITY_STATE;
		if ( verbose ) {
		    bu_log( "sub_state changed to %d\n", curr_state->sub_state );
		}
		break;
	    } else if ( BU_STR_EQUAL( line, "POINT" ) ) {
		curr_state->sub_state = POINT_ENTITY_STATE;
		if ( verbose ) {
		    bu_log( "sub_state changed to %d\n", curr_state->sub_state );
		}
		break;
	    } else if ( BU_STR_EQUAL( line, "LEADER" ) ) {
		curr_state->sub_state = LEADER_ENTITY_STATE;
		if ( verbose ) {
		    bu_log( "sub_state changed to %d\n", curr_state->sub_state );
		}
		break;
	    } else if ( BU_STR_EQUAL( line, "MTEXT" ) ) {
		curr_state->sub_state = MTEXT_ENTITY_STATE;
		if ( verbose ) {
		    bu_log( "sub_state changed to %d\n", curr_state->sub_state );
		}
		break;
	    } else if ( BU_STR_EQUAL( line, "TEXT" ) ) {
		curr_state->sub_state = TEXT_ENTITY_STATE;
		if ( verbose ) {
		    bu_log( "sub_state changed to %d\n", curr_state->sub_state );
		}
		break;
	    } else if ( BU_STR_EQUAL( line, "ATTRIB" ) ) {
		curr_state->sub_state = ATTRIB_ENTITY_STATE;
		if ( verbose ) {
		    bu_log( "sub_state changed to %d\n", curr_state->sub_state );
		}
		break;
	    } else if ( BU_STR_EQUAL( line, "ATTDEF" ) ) {
		curr_state->sub_state = ATTDEF_ENTITY_STATE;
		if ( verbose ) {
		    bu_log( "sub_state changed to %d\n", curr_state->sub_state );
		}
		break;
	    } else if ( BU_STR_EQUAL( line, "SOLID" ) ) {
		curr_state->sub_state = SOLID_ENTITY_STATE;
		if ( verbose ) {
		    bu_log( "sub_state changed to %d\n", curr_state->sub_state );
		}
		break;
	    } else if ( !strncmp( line, "VIEWPORT", 8 ) ) {
		/* not a useful entity, just ignore it */
		break;
	    } else if ( !strncmp( line, "INSERT", 6 ) ) {
		curr_state->sub_state = INSERT_ENTITY_STATE;
		if ( verbose ) {
		    bu_log( "sub_state changed to %d\n", curr_state->sub_state );
		}
		break;
	    } else if ( BU_STR_EQUAL( line, "ENDBLK" ) ) {
		/* found end of an inserted block, pop the state stack */
		tmp_state = curr_state;
		BU_LIST_POP( state_data, &state_stack, curr_state );
		if ( !curr_state ) {
		    bu_log( "ERROR: end of block encountered while not inserting!!!\n" );
		    curr_state = tmp_state;
		    break;
		}
		bu_free( (char *)tmp_state, "curr_state" );
		fseek( dxf, curr_state->file_offset, SEEK_SET );
		curr_state->sub_state = UNKNOWN_ENTITY_STATE;
		if ( verbose ) {
		    bu_log( "Popped state at end of inserted block (seeked to %ld)\n", curr_state->file_offset );
		}
		break;
	    } else {
		bu_log( "Unrecognized entity type encountered (ignoring): %s\n",
			line );
		break;
	    }
    }

    return 0;
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

    if ( !new_state ) {
	insert_init( &ins );
	BU_GETSTRUCT( new_state, state_data );
	*new_state = *curr_state;
	if ( verbose ) {
	    bu_log( "Created a new state for INSERT\n" );
	}
    }

    switch ( code ) {
	case 8:		/* layer name */
	    if ( curr_layer_name ) {
		bu_free( curr_layer_name, "curr_layer_name" );
	    }
	    curr_layer_name = make_brlcad_name( line );
	    break;
	case 2:		/* block name */
	    for ( BU_LIST_FOR( blk, block_list, &block_head ) ) {
		if ( BU_STR_EQUAL( blk->block_name, line ) ) {
		    break;
		}
	    }
	    if ( BU_LIST_IS_HEAD( blk, &block_head ) ) {
		bu_log( "ERROR: INSERT references non-existent block (%s)\n", line );
		bu_log( "\tignoring missing block\n" );
		blk = NULL;
	    }
	    new_state->curr_block = blk;
	    if ( verbose && blk ) {
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
	    if ( atoi( line ) != 1 ) {
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
	    if ( new_state->curr_block ) {
		mat_t xlate, scale, rot, tmp1, tmp2;
		MAT_IDN( xlate );
		MAT_IDN( scale );
		MAT_SCALE_VEC( scale, ins.scale );
		MAT_DELTAS_VEC( xlate, ins.insert_pt );
		bn_mat_angles( rot, 0.0, 0.0, ins.rotation );
		bn_mat_mul( tmp1, rot, scale );
		bn_mat_mul( tmp2, xlate, tmp1 );
		bn_mat_mul( new_state->xform, tmp2, curr_state->xform );
		BU_LIST_PUSH( &state_stack, &(curr_state->l) );
		curr_state = new_state;
		new_state = NULL;
		fseek( dxf, curr_state->curr_block->offset, SEEK_SET );
		curr_state->state = ENTITIES_SECTION;
		curr_state->sub_state = UNKNOWN_ENTITY_STATE;
		if ( verbose ) {
		    bu_log( "Changing state for INSERT\n" );
		    bu_log( "seeked to %ld\n", curr_state->curr_block->offset );
		    bn_mat_print( "state xform", curr_state->xform );
		}
	    }
	    break;
    }

    return 0;
}

static int
process_solid_entities_code( int code )
{
    int vert_no;
    int coord;
    point_t tmp_pt;
    struct vertex *v0, *v1;
    static int last_vert_no = -1;
    static point_t solid_pt[4];

    switch ( code ) {
	case 8:
	    if ( curr_layer_name ) {
		bu_free( curr_layer_name, "curr_layer_name" );
	    }
	    curr_layer_name = make_brlcad_name( line );
	    if ( verbose ) {
		bu_log( "LINE is in layer: %s\n", curr_layer_name );
	    }
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
	    if ( vert_no > last_vert_no ) last_vert_no = vert_no;
	    coord = code / 10 - 1;
	    solid_pt[vert_no][coord] = atof( line ) * units_conv[units] * scale_factor;
	    if ( verbose ) {
		bu_log( "SOLID vertex #%d coord #%d = %g\n", vert_no, coord, solid_pt[vert_no][coord] );
	    }
	    break;
	case 62:	/* color number */
	    curr_color = atoi( line );
	    break;
	case 0:
	    /* end of this solid */
	    get_layer();
	    if ( verbose ) {
		bu_log( "Found end of SOLID\n" );
	    }

	    layers[curr_layer]->solid_count++;

	    if ( !layers[curr_layer]->m ) {
		create_nmg();
	    }

	    v0 = NULL;
	    v1 = NULL;
	    for ( vert_no = 0; vert_no <= last_vert_no; vert_no ++ ) {
		MAT4X3PNT( tmp_pt, curr_state->xform, solid_pt[vert_no] );
		VMOVE( solid_pt[vert_no], tmp_pt );

		if ( vert_no > 0 ) {
		    struct edgeuse *eu;
		    /* create a wire edge in the NMG */
		    eu = nmg_me( v1, NULL, layers[curr_layer]->s );
		    if ( v1 == NULL ) {
			nmg_vertex_gv( eu->vu_p->v_p, solid_pt[vert_no - 1] );
			v0 = eu->vu_p->v_p;
		    }
		    nmg_vertex_gv( eu->eumate_p->vu_p->v_p, solid_pt[vert_no] );
		    v1 = eu->eumate_p->vu_p->v_p;
		    if ( verbose ) {
			bu_log( "Wire edge (solid): (%g %g %g) <-> (%g %g %g)\n",
				V3ARGS(eu->vu_p->v_p->vg_p->coord ),
				V3ARGS(eu->eumate_p->vu_p->v_p->vg_p->coord ) );
		    }
		}
	    }

	    /* close the outline */
	    nmg_me( v1, v0, layers[curr_layer]->s );

	    last_vert_no = -1;
	    curr_state->sub_state = UNKNOWN_ENTITY_STATE;
	    process_entities_code[curr_state->sub_state]( code );
	    break;
    }

    return 0;
}

static int
process_lwpolyline_entities_code( int code )
{
    point_t tmp_pt;
    static int vert_no = 0;
    static fastf_t x, y;

    switch ( code ) {
	case 8:
	    if ( curr_layer_name ) {
		bu_free( curr_layer_name, "curr_layer_name" );
	    }
	    curr_layer_name = make_brlcad_name( line );
	    if ( verbose ) {
		bu_log( "LINE is in layer: %s\n", curr_layer_name );
	    }
	    break;
	case 90:
	    /* oops */
	    break;
	case 10:
	    x = atof( line ) * units_conv[units] * scale_factor;
	    if ( verbose ) {
		bu_log( "LWPolyLine vertex #%d (x) = %g\n", vert_no, x );
	    }
	    break;
	case 20:
	    y = atof( line ) * units_conv[units] * scale_factor;
	    if ( verbose ) {
		bu_log( "LWPolyLine vertex #%d (y) = %g\n", vert_no, y );
	    }
	    add_polyline_vertex( x, y, 0.0 );
	    break;
	case 62:	/* color number */
	    curr_color = atoi( line );
	    break;
	case 70:
	    polyline_flag = atoi( line );
	    break;
	case 0:
	    /* end of this line */
	    get_layer();
	    if ( verbose ) {
		bu_log( "Found end of LWPOLYLINE\n" );
	    }

	    layers[curr_layer]->lwpolyline_count++;

	    if ( !layers[curr_layer]->m ) {
		create_nmg();
	    }

	    if ( polyline_vertex_count > 1 ) {
		struct vertex *v0=NULL, *v1=NULL, *v2=NULL;
		int i;

		if ( !layers[curr_layer]->m ) {
		    create_nmg();
		}

		for ( i=0; i<polyline_vertex_count; i++ ) {
		    MAT4X3PNT( tmp_pt, curr_state->xform, &polyline_verts[i*3] );
		    VMOVE( &polyline_verts[i*3], tmp_pt );
		}

		for ( i=0; i<polyline_vertex_count-1; i++ ) {
		    struct edgeuse *eu;

		    eu = nmg_me( v1, v2, layers[curr_layer]->s );
		    if ( i == 0 ) {
			v1 = eu->vu_p->v_p;
			nmg_vertex_gv( v1, polyline_verts );
			v0 = v1;
		    }
		    v2 = eu->eumate_p->vu_p->v_p;
		    nmg_vertex_gv( v2, &polyline_verts[(i+1)*3] );
		    if ( verbose ) {
			bu_log( "Wire edge (lwpolyline): (%g %g %g) <-> (%g %g %g)\n",
				V3ARGS( v1->vg_p->coord ),
				V3ARGS( v2->vg_p->coord ) );
		    }
		    v1 = v2;
		    v2 = NULL;
		}

		if ( polyline_flag & POLY_CLOSED ) {
		    v2 = v0;
		    (void)nmg_me( v1, v2, layers[curr_layer]->s );
		    if ( verbose ) {
			bu_log( "Wire edge (closing lwpolyline): (%g %g %g) <-> (%g %g %g)\n",
				V3ARGS( v1->vg_p->coord ),
				V3ARGS( v2->vg_p->coord ) );
		    }
		}
	    }
	    polyline_vert_indices_count=0;
	    polyline_vertex_count = 0;
	    curr_state->sub_state = UNKNOWN_ENTITY_STATE;
	    process_entities_code[curr_state->sub_state]( code );
	    break;
    }

    return 0;
}

static int
process_line_entities_code( int code )
{
    int vert_no;
    int coord;
    static point_t line_pt[2];
    struct edgeuse *eu;
    point_t tmp_pt;

    switch ( code ) {
	case 8:
	    if ( curr_layer_name ) {
		bu_free( curr_layer_name, "curr_layer_name" );
	    }
	    curr_layer_name = make_brlcad_name( line );
	    if ( verbose ) {
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
	    if ( verbose ) {
		bu_log( "LINE vertex #%d coord #%d = %g\n", vert_no, coord, line_pt[vert_no][coord] );
	    }
	    break;
	case 62:	/* color number */
	    curr_color = atoi( line );
	    break;
	case 0:
	    /* end of this line */
	    get_layer();
	    if ( verbose ) {
		bu_log( "Found end of LINE\n" );
	    }

	    layers[curr_layer]->line_count++;

	    if ( !layers[curr_layer]->m ) {
		create_nmg();
	    }

	    MAT4X3PNT( tmp_pt, curr_state->xform, line_pt[0] );
	    VMOVE( line_pt[0], tmp_pt );
	    MAT4X3PNT( tmp_pt, curr_state->xform, line_pt[1] );
	    VMOVE( line_pt[1], tmp_pt );

	    /* create a wire edge in the NMG */
	    eu = nmg_me( NULL, NULL, layers[curr_layer]->s );
	    nmg_vertex_gv( eu->vu_p->v_p, line_pt[0] );
	    nmg_vertex_gv( eu->eumate_p->vu_p->v_p, line_pt[1] );
	    if ( verbose ) {
		bu_log( "Wire edge (line): (%g %g %g) <-> (%g %g %g)\n",
			V3ARGS(eu->vu_p->v_p->vg_p->coord ),
			V3ARGS(eu->eumate_p->vu_p->v_p->vg_p->coord ) );
	    }

	    curr_state->sub_state = UNKNOWN_ENTITY_STATE;
	    process_entities_code[curr_state->sub_state]( code );
	    break;
    }

    return 0;
}

static int
process_ellipse_entities_code( int code )
{
    static point_t center={0, 0, 0};
    static point_t majorAxis={1.0, 0, 0};
    static double ratio=1.0;
    static double startAngle=0.0;
    static double endAngle=M_PI*2.0;
    double angle, delta;
    double majorRadius, minorRadius;
    point_t tmp_pt;
    vect_t xdir, ydir, zdir;
    int coord;
    int fullCircle;
    int done;
    struct vertex *v0=NULL, *v1=NULL, *v2=NULL;
    struct edgeuse *eu;

    switch ( code ) {
	case 8:		/* layer name */
	    if ( curr_layer_name ) {
		bu_free( curr_layer_name, "curr_layer_name" );
	    }
	    curr_layer_name = make_brlcad_name( line );
	    break;
	case 10:
	case 20:
	case 30:
	    coord = code / 10 - 1;
	    center[coord] = atof( line ) * units_conv[units] * scale_factor;
	    break;
	case 11:
	case 21:
	case 31:
	    coord = code / 10 - 1;
	    majorAxis[coord] = atof( line ) * units_conv[units] * scale_factor;
	    break;
	case 40:
	    ratio = atof( line );
	    break;
	case 41:
	    startAngle = atof( line );
	    break;
	case 42:
	    endAngle = atof( line );
	    break;
	case 62:	/* color number */
	    curr_color = atoi( line );
	    break;
	case 0:
	    /* end of this ellipse entity
	     * make a series of wire edges in the NMG to approximate a circle
	     */

	    get_layer();
	    if ( verbose ) {
		bu_log( "Found an ellipse\n" );
	    }

	    if ( !layers[curr_layer]->m ) {
		create_nmg();
	    }

	    layers[curr_layer]->ellipse_count++;

	    MAT4X3PNT( tmp_pt, curr_state->xform, center );
	    VMOVE( center, tmp_pt );
	    MAT4X3PNT( tmp_pt, curr_state->xform, majorAxis );
	    VMOVE( majorAxis, tmp_pt );

	    majorRadius = MAGNITUDE( majorAxis );
	    minorRadius = ratio * majorRadius;

	    VMOVE( xdir, majorAxis );
	    VUNITIZE( xdir );
	    VSET( zdir, 0, 0, 1 );
	    VCROSS( ydir, zdir, xdir );

	    /* FIXME: arbitrary undefined tolerance */
	    if ( NEAR_EQUAL( endAngle, startAngle, 0.001 ) ) {
		fullCircle = 1;
	    } else {
		fullCircle = 0;
	    }

	    if ( verbose ) {
		bu_log( "Ellipse:\n" );
		bu_log( "\tcenter = (%g %g %g)\n", V3ARGS( center ) );
		bu_log( "\tmajorAxis = (%g %g %g)\n", V3ARGS( majorAxis ) );
		bu_log( "\txdir = (%g %g %g)\n", V3ARGS( xdir ) );
		bu_log( "\tydir = (%g %g %g)\n", V3ARGS( ydir ) );
		bu_log( "\tradii = %g %g\n", majorRadius, minorRadius );
		bu_log( "\tangles = %g %g\n", startAngle, endAngle );
		bu_log( "\tfull circle = %d\n", fullCircle );
	    }

	    /* make nmg wire edges */
	    angle = startAngle;
	    delta = M_PI / 15.0;
	    if ( (endAngle - startAngle)/delta < 4 ) {
		delta = (endAngle - startAngle) / 5.0;
	    }
	    done = 0;
	    while ( !done ) {
		point_t p0, p1;
		double r0, r1;

		if ( angle >= endAngle ) {
		    angle = endAngle;
		    done = 1;
		}

		r0 = majorRadius * cos( angle );
		r1 = minorRadius * sin( angle );
		VJOIN2( p1, center, r0, xdir, r1, ydir );
		if ( EQUAL(angle, startAngle) ) {
		    VMOVE( p0, p1 );
		    angle += delta;
		    continue;
		}
		if ( fullCircle && EQUAL(angle, endAngle) ) {
		    v2 = v0;
		}
		eu = nmg_me( v1, v2, layers[curr_layer]->s );
		v1 = eu->vu_p->v_p;
		if ( v0 == NULL ) {
		    v0 = v1;
		    nmg_vertex_gv( v0, p0 );
		}
		v2 = eu->eumate_p->vu_p->v_p;
		if ( v2 != v0 ) {
		    nmg_vertex_gv( v2, p1 );
		}
		if ( verbose ) {
		    bu_log( "Wire edge (ellipse): (%g %g %g) <-> (%g %g %g)\n",
			    V3ARGS( v1->vg_p->coord ),
			    V3ARGS( v2->vg_p->coord ) );
		}
		v1 = v2;
		v2 = NULL;

		angle += delta;
	    }

	    VSET( center, 0, 0, 0 );
	    VSET( majorAxis, 0, 0, 0 );
	    ratio = 1.0;
	    startAngle = 0.0;
	    endAngle = M_PI * 2.0;

	    curr_state->sub_state = UNKNOWN_ENTITY_STATE;
	    process_entities_code[curr_state->sub_state]( code );
	    break;
    }

    return 0;
}

static int
process_circle_entities_code( int code )
{
    static point_t center;
    static fastf_t radius;
    int coord, i;
    struct vertex *v0=NULL, *v1=NULL, *v2=NULL;
    struct edgeuse *eu;

    switch ( code ) {
	case 8:		/* layer name */
	    if ( curr_layer_name ) {
		bu_free( curr_layer_name, "curr_layer_name" );
	    }
	    curr_layer_name = make_brlcad_name( line );
	    break;
	case 10:
	case 20:
	case 30:
	    coord = code / 10 - 1;
	    center[coord] = atof( line ) * units_conv[units] * scale_factor;
	    if ( verbose ) {
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
	    if ( verbose ) {
		bu_log( "Found a circle\n" );
	    }

	    if ( !layers[curr_layer]->m ) {
		create_nmg();
	    }

	    layers[curr_layer]->circle_count++;

	    /* calculate circle at origin first */
	    VSET( circle_pts[0], radius, 0.0, 0.0 );
	    for ( i=1; i<segs_per_circle; i++ ) {
		circle_pts[i][X] = circle_pts[i-1][X]*cos_delta - circle_pts[i-1][Y]*sin_delta;
		circle_pts[i][Y] = circle_pts[i-1][Y]*cos_delta + circle_pts[i-1][X]*sin_delta;
	    }

	    /* move everything to the specified center */
	    for ( i=0; i<segs_per_circle; i++ ) {
		point_t tmp_pt;
		VADD2( circle_pts[i], circle_pts[i], center );

		/* apply transformation */
		MAT4X3PNT( tmp_pt, curr_state->xform, circle_pts[i] );
		VMOVE( circle_pts[i], tmp_pt );
	    }

	    /* make nmg wire edges */
	    for ( i=0; i<segs_per_circle; i++ ) {
		if ( i+1 == segs_per_circle ) {
		    v2 = v0;
		}
		eu = nmg_me( v1, v2, layers[curr_layer]->s );
		if ( i == 0 ) {
		    v1 = eu->vu_p->v_p;
		    v0 = v1;
		    nmg_vertex_gv( v1, circle_pts[0] );
		}
		v2 = eu->eumate_p->vu_p->v_p;
		if ( i+1 < segs_per_circle ) {
		    nmg_vertex_gv( v2, circle_pts[i+1] );
		}
		if ( verbose ) {
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

    return 0;
}

/* horizontal alignment codes for text */
#define LEFT 0
#define CENTER 1
#define RIGHT 2
#define ALIGNED 3
#define HMIDDLE 4
#define FIT 5

/* vertical alignment codes */
#define BASELINE 0
#define BOTTOM 1
#define VMIDDLE 2
#define TOP 3

/* attachment point codes */
#define TOPLEFT 1
#define TOPCENTER 2
#define TOPRIGHT 3
#define MIDDLELEFT 4
#define MIDDLECENTER 5
#define MIDDLERIGHT 6
#define BOTTOMLEFT 7
#define BOTTOMCENTER 8
#define BOTTOMRIGHT 9

/* secret codes for MTEXT entries
 * \P - new paragraph (line)
 * \X - new line (everthing before is above the dimension line, everything after is below)
 * \~ - blank space
 * \f - TrueType font
 * \F - .SHX font
 * 	    ex:
 *		Hello \fArial;World
 *	    or:
 *		Hello {\fArial;World}
 */

int
convertSecretCodes( char *c, char *cp, int *maxLineLen )
{
    int lineCount = 0;
    int lineLen = 0;

    while ( *c ) {
	if ( *c == '%' && *(c+1) == '%' ) {
	    switch ( *(c+2) ) {
		case 'o':
		case 'O':
		    overstrikemode = !overstrikemode;
		    c += 3;
		    break;
		case 'u':
		case 'U':
		    underscoremode = !underscoremode;
		    c += 3;
		    break;
		case 'd':	/* degree */
		case 'D':
		    *cp++ = 8;
		    c += 3;
		    break;
		case 'p':	 /* plus/minus */
		case 'P':
		    *cp++ = 6;
		    c += 3;
		    break;
		case 'c':	/* diameter */
		case 'C':
		    *cp++ = 7;
		    c += 3;
		    break;
		case '%':
		    *cp++ = '%';
		    c += 3;
		    break;
		default:
		    *cp++ = *c;
		    c++;
		    break;
	    }
	    lineLen++;
	} else if ( *c == '\\' ) {
	    switch ( *(c+1) ) {
		case 'P':
		case 'X':
		    *cp++ = '\n';
		    c += 2;
		    lineCount++;
		    if ( lineLen > *maxLineLen ) {
			*maxLineLen = lineLen;
		    }
		    lineLen = 0;
		    break;
		case 'A':
		    while ( *c != ';' && *c != '\0' ) c++;
		    c++;
		    break;
		case '~':
		    *cp++ = ' ';
		    c += 2;
		    lineLen++;
		    break;
		default:
		    *cp++ = *c++;
		    lineLen++;
		    break;
	    }
	} else {
	    *cp++ = *c++;
	    lineLen++;
	}
    }
    if ( *(c-1) != '\n' ) {
	lineCount++;
    }

    if ( lineLen > *maxLineLen ) {
	*maxLineLen = lineLen;
    }

    return lineCount;
}


void
drawString( char *theText, point_t firstAlignmentPoint, point_t secondAlignmentPoint,
	    double textHeight, double UNUSED(textScale), double textRotation, int horizAlignment, int vertAlignment, int UNUSED(textFlag) )
{
    double stringLength=0.0;
    char *copyOfText;
    char *c, *cp;
    vect_t diff;
    double allowedLength;
    double xScale=1.0;
    double yScale=1.0;
    double scale;
    struct bu_list vhead;
    int maxLineLen=0;

    BU_LIST_INIT( &vhead );

    copyOfText = bu_calloc( (unsigned int)strlen( theText )+1, 1, "copyOfText" );
    c = theText;
    cp = copyOfText;
    (void)convertSecretCodes( c, cp, &maxLineLen );

    bu_free( theText, "theText" );
    stringLength = strlen( copyOfText );

    if ( horizAlignment == FIT && vertAlignment == BASELINE ) {
	/* fit along baseline */
	VSUB2( diff, firstAlignmentPoint, secondAlignmentPoint );
	allowedLength = MAGNITUDE( diff );
	xScale = allowedLength / stringLength;
	yScale = textHeight;
	scale = xScale < yScale ? xScale : yScale;
	bn_vlist_2string( &vhead, &free_hd, copyOfText,
			  firstAlignmentPoint[X], firstAlignmentPoint[Y],
			  scale, textRotation );
	nmg_vlist_to_eu( &vhead, layers[curr_layer]->s );
	BN_FREE_VLIST( &free_hd, &vhead );
    } else if ( horizAlignment == LEFT && vertAlignment == BASELINE ) {
	bn_vlist_2string( &vhead, &free_hd, copyOfText,
			  firstAlignmentPoint[X], firstAlignmentPoint[Y],
			  textHeight, textRotation );
	nmg_vlist_to_eu( &vhead, layers[curr_layer]->s );
	BN_FREE_VLIST( &free_hd, &vhead );
    } else if ( (horizAlignment == CENTER || horizAlignment == HMIDDLE) && vertAlignment == BASELINE ) {
	double len = stringLength * textHeight;
	firstAlignmentPoint[X] = secondAlignmentPoint[X] - cos(textRotation) * len / 2.0;
	firstAlignmentPoint[Y] = secondAlignmentPoint[Y] - sin(textRotation) * len / 2.0;
	bn_vlist_2string( &vhead, &free_hd, copyOfText,
			  firstAlignmentPoint[X], firstAlignmentPoint[Y],
			  textHeight, textRotation );
	nmg_vlist_to_eu( &vhead, layers[curr_layer]->s );
	BN_FREE_VLIST( &free_hd, &vhead );
    } else if ( (horizAlignment == CENTER || horizAlignment == HMIDDLE) && vertAlignment == VMIDDLE ) {
	double len = stringLength * textHeight;
	firstAlignmentPoint[X] = secondAlignmentPoint[X] - len / 2.0;
	firstAlignmentPoint[Y] = secondAlignmentPoint[Y] - textHeight / 2.0;
	firstAlignmentPoint[X] = firstAlignmentPoint[X] - (1.0 - cos(textRotation)) * len / 2.0;
	firstAlignmentPoint[Y] = firstAlignmentPoint[Y] - sin(textRotation) * len / 2.0;
	bn_vlist_2string( &vhead, &free_hd, copyOfText,
			  firstAlignmentPoint[X], firstAlignmentPoint[Y],
			  textHeight, textRotation );
	nmg_vlist_to_eu( &vhead, layers[curr_layer]->s );
	BN_FREE_VLIST( &free_hd, &vhead );
    } else if ( horizAlignment == RIGHT && vertAlignment == BASELINE ) {
	double len = stringLength * textHeight;
	firstAlignmentPoint[X] = secondAlignmentPoint[X] - cos(textRotation) * len;
	firstAlignmentPoint[Y] = secondAlignmentPoint[Y] - sin(textRotation) * len;
	bn_vlist_2string( &vhead, &free_hd, copyOfText,
			  firstAlignmentPoint[X], firstAlignmentPoint[Y],
			  textHeight, textRotation );
	nmg_vlist_to_eu( &vhead, layers[curr_layer]->s );
	BN_FREE_VLIST( &free_hd, &vhead );
    } else {
	bu_log( "cannot handle this alignment: horiz = %d, vert = %d\n", horizAlignment, vertAlignment );
    }

    bu_free( copyOfText, "copyOfText" );
}

void
drawMtext( char *text, int attachPoint, int UNUSED(drawingDirection), double textHeight, double entityHeight,
	   double charWidth, double UNUSED(rectWidth), double rotationAngle, double insertionPoint[3] )
{
    struct bu_list vhead;
    int done;
    char *c;
    char *cp;
    int lineCount;
    double lineSpace;
    vect_t xdir, ydir;
    double startx, starty;
    int maxLineLen=0;
    double scale = 1.0;
    double xdel = 0.0, ydel = 0.0;
    double radians = rotationAngle * M_PI / 180.0;
    char *copyOfText = bu_calloc( (unsigned int)strlen( text )+1, 1, "copyOfText" );

    BU_LIST_INIT( &vhead );

    c = text;
    cp = copyOfText;
    lineCount = convertSecretCodes( c, cp, &maxLineLen );

    if ( textHeight > 0.0 ) {
	scale = textHeight;
    } else if ( charWidth > 0.0 ) {
	scale = charWidth;
    } else if ( entityHeight > 0.0 ) {
	scale = (entityHeight / (double)lineCount) * 0.9;
    }

    lineSpace = 1.25 * scale;

    VSET( xdir, cos( radians ), sin( radians ), 0.0 );
    VSET( ydir, -sin( radians ), cos( radians ), 0.0 );

    switch ( attachPoint ) {
	case TOPLEFT:
	    xdel = 0.0;
	    ydel =  -scale;
	    break;
	case TOPCENTER:
	    xdel = -((double)maxLineLen * scale) / 2.0;
	    ydel = -scale;
	    break;
	case TOPRIGHT:
	    xdel = -(double)maxLineLen * scale;
	    ydel = -scale;
	    break;
	case MIDDLELEFT:
	    xdel = 0.0;
	    ydel = -((double)lineCount * lineSpace) / 2.0;
	    break;
	case MIDDLECENTER:
	    xdel = -((double)maxLineLen * scale) / 2.0;
	    ydel = -((double)lineCount * lineSpace) / 2.0;
	    break;
	case MIDDLERIGHT:
	    xdel = -(double)maxLineLen * scale;
	    ydel = -((double)lineCount * lineSpace) / 2.0;
	    break;
	case BOTTOMLEFT:
	    xdel = 0.0;
	    ydel = (double)lineCount * lineSpace - scale;
	    break;
	case BOTTOMCENTER:
	    xdel = -((double)maxLineLen * scale) / 2.0;
	    ydel = (double)lineCount * lineSpace - scale;
	    break;
	case BOTTOMRIGHT:
	    xdel = -(double)maxLineLen * scale;
	    ydel = (double)lineCount * lineSpace - scale;
	    break;
    }

    startx = insertionPoint[X] + xdel * xdir[X] + ydel * ydir[X];
    starty = insertionPoint[Y] + xdel * xdir[Y] + ydel * ydir[Y];

    cp = copyOfText;
    c = copyOfText;
    done = 0;
    while ( !done ) {
	if ( *cp == '\n' || *cp == '\0' ) {
	    if ( *cp == '\0' ) {
		done = 1;
	    }
	    *cp = '\0';
	    bn_vlist_2string( &vhead, &free_hd, c,
			      startx, starty,
			      scale, rotationAngle );
	    nmg_vlist_to_eu( &vhead, layers[curr_layer]->s );
	    BN_FREE_VLIST( &free_hd, &vhead );
	    c = ++cp;
	    startx -= lineSpace * ydir[X];
	    starty -= lineSpace * ydir[Y];
	} else {
	    cp++;
	}
    }


    bu_free( copyOfText, "copyOfText" );
}

static int
process_leader_entities_code( int code )
{
    static int arrowHeadFlag=0;
    static int vertNo=0;
    static point_t pt;
    point_t tmp_pt;
    int i;
    struct edgeuse *eu;
    struct vertex *v1=NULL, *v2=NULL;

    switch ( code ) {
	case 8:
	    if ( curr_layer_name ) {
		bu_free( curr_layer_name, "curr_layer_name" );
	    }
	    curr_layer_name = make_brlcad_name( line );
	    if ( verbose ) {
		bu_log( "LINE is in layer: %s\n", curr_layer_name );
	    }
	    break;
	case 62:	/* color number */
	    curr_color = atoi( line );
	    break;
	case 71:
	    arrowHeadFlag = atoi( line );
	    break;
	case 72:
	    /* path type, unimplemented */
	    break;
	case 73:
	    /* creation, unimplemented */
	    break;
	case 74:
	    /* hookline direction, unimplemented */
	    break;
	case 75:
	    /* hookline, unimplemented */
	    break;
	case 40:
	    /* text height, unimplemented */
	    break;
	case 41:
	    /* text width, unimplemented */
	    break;
	case 76:
	    /* num vertices, unimplemented */
	    break;
	case 210:
	    /* normal, unimplemented */
	    break;
	case 220:
	    /* normal, unimplemented */
	    break;
	case 230:
	    /* normal, unimplemented */
	    break;
	case 211:
	    /* horizontal direction, unimplemented */
	    break;
	case 221:
	    /* horizontal direction, unimplemented */
	    break;
	case 231:
	    /* horizontal direction, unimplemented */
	    break;
	case 212:
	    /* offsetB, unimplemented */
	    break;
	case 222:
	    /* offsetB, unimplemented */
	    break;
	case 232:
	    /* offsetB, unimplemented */
	    break;
	case 213:
	    /* offset, unimplemented */
	    break;
	case 223:
	    /* offset, unimplemented */
	    break;
	case 233:
	    /* offset, unimplemented */
	    break;
	case 10:
	    pt[X] = atof( line );
	    break;
	case 20:
	    pt[Y] = atof( line );
	    break;
	case 30:
	    pt[Z] = atof( line );
	    if ( verbose ) {
		bu_log( "LEADER vertex #%d = (%g %g %g)\n", vertNo, V3ARGS( pt ) );
	    }
	    MAT4X3PNT( tmp_pt, curr_state->xform, pt );
	    add_polyline_vertex( V3ARGS( tmp_pt ) );
	    break;
	case 0:
	    /* end of this line */
	    get_layer();
	    if ( verbose ) {
		bu_log( "Found end of LEADER: arrowhead flag = %d\n", arrowHeadFlag );
	    }

	    layers[curr_layer]->leader_count++;

	    if ( polyline_vertex_count > 1 ) {
		if ( !layers[curr_layer]->m ) {
		    create_nmg();
		}

		for ( i=0; i<polyline_vertex_count-1; i++ ) {
		    eu = nmg_me( v1, v2, layers[curr_layer]->s );
		    if ( i == 0 ) {
			v1 = eu->vu_p->v_p;
			nmg_vertex_gv( v1, polyline_verts );
		    }
		    v2 = eu->eumate_p->vu_p->v_p;
		    nmg_vertex_gv( v2, &polyline_verts[(i+1)*3] );
		    if ( verbose ) {
			bu_log( "Wire edge (LEADER): (%g %g %g) <-> (%g %g %g)\n",
				V3ARGS( v1->vg_p->coord ),
				V3ARGS( v2->vg_p->coord ) );
		    }
		    v1 = v2;
		    v2 = NULL;
		}
	    }
	    polyline_vert_indices_count=0;
	    polyline_vertex_count = 0;
	    arrowHeadFlag = 0;
	    vertNo = 0;

	    curr_state->sub_state = UNKNOWN_ENTITY_STATE;
	    process_entities_code[curr_state->sub_state]( code );
	    break;
    }

    return 0;
}

static int
process_mtext_entities_code( int code )
{
    static struct bu_vls *vls = NULL;
    static int attachPoint=0;
    static int drawingDirection=0;
    static double textHeight=0.0;
    static double entityHeight=0.0;
    static double charWidth=0.0;
    static double rectWidth=0.0;
    static double rotationAngle=0.0;
    static double insertionPoint[3]={0, 0, 0};
    static double xAxisDirection[3]={0, 0, 0};
    point_t tmp_pt;
    int coord;

    switch ( code ) {
	case 3:
	    if (!vls) {
		BU_GETSTRUCT(vls, bu_vls);
		bu_vls_init(vls);
	    }
	    bu_vls_strcat(vls, line);
	    break;
	case 1:
	    if (!vls) {
		BU_GETSTRUCT(vls, bu_vls);
		bu_vls_init(vls);
	    }
	    bu_vls_strcat(vls, line);
	    break;
	case 8:		/* layer name */
	    if ( curr_layer_name ) {
		bu_free( curr_layer_name, "curr_layer_name" );
	    }
	    curr_layer_name = make_brlcad_name( line );
	    break;
	case 10:
	case 20:
	case 30:
	    coord = (code / 10) - 1;
	    insertionPoint[coord] = atof( line ) * units_conv[units] * scale_factor;
	    break;
	case 11:
	case 21:
	case 31:
	    coord = (code / 10) - 1;
	    xAxisDirection[coord] = atof( line );
	    if ( code == 31 ) {
		rotationAngle = atan2( xAxisDirection[Y], xAxisDirection[X] ) * 180.0 / M_PI;
	    }
	    break;
	case 40:
	    textHeight = atof( line );
	    break;
	case 41:
	    rectWidth = atof( line );
	    break;
	case 42:
	    charWidth = atof( line );
	    break;
	case 43:
	    entityHeight = atof( line );
	    break;
	case 50:
	    rotationAngle = atof( line );
	    break;
	case 62:	/* color number */
	    curr_color = atoi( line );
	    break;
	case 71:
	    attachPoint = atoi( line );
	    break;
	case 72:
	    drawingDirection = atoi( line );
	    break;
	case 0:
	    if ( verbose ) {
		bu_log( "MTEXT (%s), height = %g, entityHeight = %g, rectWidth = %g\n", (vls) ? bu_vls_addr(vls) : "NO_NAME", textHeight, entityHeight, rectWidth );
		bu_log( "\tattachPoint = %d, charWidth = %g, insertPt = (%g %g %g)\n", attachPoint, charWidth, V3ARGS( insertionPoint ) );
	    }
	    /* draw the text */
	    get_layer();

	    if ( !layers[curr_layer]->m ) {
		create_nmg();
	    }

	    layers[curr_layer]->mtext_count++;

	    /* apply transformation */
	    MAT4X3PNT( tmp_pt, curr_state->xform, insertionPoint );
	    VMOVE( insertionPoint, tmp_pt );

	    drawMtext( (vls) ? bu_vls_addr(vls) : "NO_NAME", attachPoint, drawingDirection, textHeight, entityHeight,
		       charWidth, rectWidth, rotationAngle, insertionPoint );

	    bu_vls_free(vls);
	    vls = NULL;

	    attachPoint = 0;
	    textHeight = 0.0;
	    entityHeight = 0.0;
	    charWidth = 0.0;
	    rectWidth = 0.0;
	    rotationAngle = 0.0;
	    VSET( insertionPoint, 0, 0, 0 );
	    VSET( xAxisDirection, 0, 0, 0 );
	    curr_state->sub_state = UNKNOWN_ENTITY_STATE;
	    process_entities_code[curr_state->sub_state]( code );
	    break;
    }

    return 0;
}


static int
process_text_attrib_entities_code( int code )
{
    /* Secret text code used in DXF files:
     *
     * %%o - toggle overstrike mode
     * %%u - toggle underscore mode
     * %%d - degree symbol
     * %%p - tolerance symbol (plus/minus)
     * %%c - diameter symbol (circle with line through it, lower left to upper right
     * %%% - percent symbol
     */

    static char *theText=NULL;
    static int horizAlignment=0;
    static int vertAlignment=0;
    static int textFlag=0;
    static point_t firstAlignmentPoint = VINIT_ZERO;
    static point_t secondAlignmentPoint = VINIT_ZERO;
    static double textScale=1.0;
    static double textHeight;
    static double textRotation=0.0;
    point_t tmp_pt;
    int coord;

    switch ( code ) {
	case 1:
	    theText = bu_strdup( line );
	    break;
	case 8:		/* layer name */
	    if ( curr_layer_name ) {
		bu_free( curr_layer_name, "curr_layer_name" );
	    }
	    curr_layer_name = make_brlcad_name( line );
	    break;
	case 10:
	case 20:
	case 30:
	    coord = (code / 10) - 1;
	    firstAlignmentPoint[coord] = atof( line ) * units_conv[units] * scale_factor;
	    break;
	case 11:
	case 21:
	case 31:
	    coord = (code / 10) - 1;
	    secondAlignmentPoint[coord] = atof( line ) * units_conv[units] * scale_factor;
	    break;
	case 40:
	    textHeight = atof( line );
	    break;
	case 41:
	    textScale = atof( line );
	    break;
	case 50:
	    textRotation = atof( line );
	    break;
	case 62:	/* color number */
	    curr_color = atoi( line );
	    break;
	case 71:
	    textFlag = atoi( line );
	    break;
	case 72:
	    horizAlignment = atoi( line );
	    break;
	case 73:
	    vertAlignment = atoi( line );
	    break;
	case 0:
	    if ( theText != NULL ) {
		if ( verbose ) {
		    bu_log( "TEXT (%s), height = %g, scale = %g\n", theText, textHeight, textScale );
		}
		/* draw the text */
		get_layer();

		if ( !layers[curr_layer]->m ) {
		    create_nmg();
		}

		/* apply transformation */
		MAT4X3PNT( tmp_pt, curr_state->xform, firstAlignmentPoint );
		VMOVE( firstAlignmentPoint, tmp_pt );
		MAT4X3PNT( tmp_pt, curr_state->xform, secondAlignmentPoint );
		VMOVE( secondAlignmentPoint, tmp_pt );

		drawString( theText, firstAlignmentPoint, secondAlignmentPoint,
			    textHeight, textScale, textRotation, horizAlignment, vertAlignment, textFlag );
		layers[curr_layer]->text_count++;
	    }
	    horizAlignment=0;
	    vertAlignment=0;
	    textFlag=0;
	    VSET( firstAlignmentPoint, 0.0, 0.0, 0.0 );
	    VSET( secondAlignmentPoint, 0.0, 0.0, 0.0 );
	    textScale=1.0;
	    textRotation=0.0;
	    curr_state->sub_state = UNKNOWN_ENTITY_STATE;
	    process_entities_code[curr_state->sub_state]( code );
	    break;
    }

    return 0;
}

static int
process_dimension_entities_code( int code )
{
    /* static point_t def_pt={0.0, 0.0, 0.0}; */
    static char *block_name=NULL;
    static struct state_data *new_state=NULL;
    struct block_list *blk;
    /* int coord; */

    switch ( code ) {
	case 10:
	case 20:
	case 30:
	    /* coord = (code / 10) - 1; */
	    /* def_pt[coord] = atof( line ) * units_conv[units] * scale_factor; */
	    break;
	case 8:		/* layer name */
	    if ( curr_layer_name ) {
		bu_free( curr_layer_name, "curr_layer_name" );
	    }
	    curr_layer_name = make_brlcad_name( line );
	    break;
	case 2:	/* block name */
	    block_name = bu_strdup( line );
	    break;
	case 0:
	    if ( block_name != NULL  ) {
		/* insert this dimension block */
		get_layer();
		BU_GETSTRUCT( new_state, state_data );
		*new_state = *curr_state;
		if ( verbose ) {
		    bu_log( "Created a new state for DIMENSION\n" );
		}
		for ( BU_LIST_FOR( blk, block_list, &block_head ) ) {
		    if (block_name) {
			if ( BU_STR_EQUAL( blk->block_name, block_name ) ) {
			    break;
			}
		    }
		}
		if ( BU_LIST_IS_HEAD( blk, &block_head ) ) {
		    bu_log( "ERROR: DIMENSION references non-existent block (%s)\n", block_name );
		    bu_log( "\tignoring missing block\n" );
		    blk = NULL;
		}
		new_state->curr_block = blk;
		if ( verbose && blk ) {
		    bu_log( "Inserting block %s\n", blk->block_name );
		}

		if (block_name) {
		    bu_free( block_name, "block_name" );
		}

		if ( new_state->curr_block ) {
		    BU_LIST_PUSH( &state_stack, &(curr_state->l) );
		    curr_state = new_state;
		    new_state = NULL;
		    fseek( dxf, curr_state->curr_block->offset, SEEK_SET );
		    curr_state->state = ENTITIES_SECTION;
		    curr_state->sub_state = UNKNOWN_ENTITY_STATE;
		    if ( verbose ) {
			bu_log( "Changing state for INSERT\n" );
			bu_log( "seeked to %ld\n", curr_state->curr_block->offset );
		    }
		    layers[curr_layer]->dimension_count++;
		}
	    }
	    else
	    {
		curr_state->sub_state = UNKNOWN_ENTITY_STATE;
		process_entities_code[curr_state->sub_state]( code );
	    }
	    break;
    }

    return 0;
}

static int
process_arc_entities_code( int code )
{
    static point_t center={0, 0, 0};
    static fastf_t radius;
    static fastf_t start_angle, end_angle;
    int num_segs;
    int coord, i;
    struct vertex *v0=NULL, *v1=NULL, *v2=NULL;
    struct edgeuse *eu;

    switch ( code ) {
	case 8:		/* layer name */
	    if ( curr_layer_name ) {
		bu_free( curr_layer_name, "curr_layer_name" );
	    }
	    curr_layer_name = make_brlcad_name( line );
	    break;
	case 10:
	case 20:
	case 30:
	    coord = code / 10 - 1;
	    center[coord] = atof( line ) * units_conv[units] * scale_factor;
	    if ( verbose ) {
		bu_log( "ARC center coord #%d = %g\n", coord, center[coord] );
	    }
	    break;
	case 40:
	    radius = atof( line ) * units_conv[units] * scale_factor;
	    if ( verbose) {
		bu_log( "ARC radius = %g\n", radius );
	    }
	    break;
	case 50:
	    start_angle = atof( line );
	    if ( verbose ) {
		bu_log( "ARC start angle = %g\n", start_angle );
	    }
	    break;
	case 51:
	    end_angle = atof( line );
	    if ( verbose ) {
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
	    if ( verbose ) {
		bu_log( "Found an arc\n" );
	    }

	    layers[curr_layer]->arc_count++;

	    if ( !layers[curr_layer]->m ) {
		create_nmg();
	    }

	    while ( end_angle < start_angle ) {
		end_angle += 360.0;
	    }

	    /* calculate arc at origin first */
	    num_segs = (end_angle - start_angle) / 360.0 * segs_per_circle;
	    start_angle *= M_PI / 180.0;
	    end_angle *= M_PI / 180.0;
	    if ( verbose ) {
		bu_log( "arc has %d segs\n", num_segs );
	    }

	    if ( num_segs < 1 ) {
		num_segs = 1;
	    }
	    VSET( circle_pts[0], radius * cos( start_angle ), radius * sin( start_angle ), 0.0 );
	    for ( i=1; i<num_segs; i++ ) {
		circle_pts[i][X] = circle_pts[i-1][X]*cos_delta - circle_pts[i-1][Y]*sin_delta;
		circle_pts[i][Y] = circle_pts[i-1][Y]*cos_delta + circle_pts[i-1][X]*sin_delta;
	    }
	    circle_pts[num_segs][X] = radius * cos( end_angle );
	    circle_pts[num_segs][Y] = radius * sin( end_angle );
	    num_segs++;

	    if ( verbose ) {
		bu_log( "ARC points calculated:\n" );
		for ( i=0; i<num_segs; i++ ) {
		    bu_log( "\t point #%d: (%g %g %g)\n", i, V3ARGS( circle_pts[i] ) );
		}
	    }

	    /* move everything to the specified center */
	    for ( i=0; i<num_segs; i++ ) {
		point_t tmp_pt;

		VADD2( circle_pts[i], circle_pts[i], center );

		/* apply transformation */
		MAT4X3PNT( tmp_pt, curr_state->xform, circle_pts[i] );
		VMOVE( circle_pts[i], tmp_pt );
	    }

	    if ( verbose ) {
		bu_log( "ARC points after move to center at (%g %g %g):\n", V3ARGS( center ) );
		for ( i=0; i<num_segs; i++ ) {
		    bu_log( "\t point #%d: (%g %g %g)\n", i, V3ARGS( circle_pts[i] ) );
		}
	    }

	    /* make nmg wire edges */
	    for ( i=1; i<num_segs; i++ ) {
		if ( i == num_segs ) {
		    v2 = v0;
		}
		eu = nmg_me( v1, v2, layers[curr_layer]->s );
		if ( i == 1 ) {
		    v1 = eu->vu_p->v_p;
		    v0 = v1;
		    nmg_vertex_gv( v1, circle_pts[0] );
		}
		v2 = eu->eumate_p->vu_p->v_p;
		if ( i < segs_per_circle ) {
		    nmg_vertex_gv( v2, circle_pts[i] );
		}
		if ( verbose ) {
		    bu_log( "Wire edge (arc) #%d (%g %g %g) <-> (%g %g %g)\n", i,
			    V3ARGS( v1->vg_p->coord ),
			    V3ARGS( v2->vg_p->coord ) );
		}
		v1 = v2;
		v2 = NULL;
	    }

	    VSETALL( center, 0.0 );
	    for ( i=0; i<segs_per_circle; i++ ) {
		VSETALL( circle_pts[i], 0.0 );
	    }

	    curr_state->sub_state = UNKNOWN_ENTITY_STATE;
	    process_entities_code[curr_state->sub_state]( code );
	    break;
    }

    return 0;
}

static int
process_spline_entities_code( int code )
{
    static int flag=0;
    static int degree=0;
    static int numKnots=0;
    static int numCtlPts=0;
    static int numFitPts=0;
    static fastf_t *knots=NULL;
    static fastf_t *weights=NULL;
    static fastf_t *ctlPts=NULL;
    static fastf_t *fitPts=NULL;
    static int knotCount=0;
    static int weightCount=0;
    static int ctlPtCount=0;
    static int fitPtCount=0;
    static int subCounter=0;
    static int subCounter2=0;
    int i;
    int coord;
    struct edge_g_cnurb *crv;
    int pt_type;
    int ncoords;
    struct vertex *v1=NULL;
    struct vertex *v2=NULL;
    struct edgeuse *eu;
    fastf_t startParam;
    fastf_t stopParam;
    fastf_t paramDelta;
    point_t pt;

    switch ( code ) {
	case 8:
	    if ( curr_layer_name ) {
		bu_free( curr_layer_name, "curr_layer_name" );
	    }
	    curr_layer_name = make_brlcad_name( line );
	    break;
	case 210:
	case 220:
	case 230:
	    coord = code / 10 - 21;
	    /* normal, unhandled */
	    /* normal[coord] = atof( line ) * units_conv[units] * scale_factor; */
	    break;
	case 70:
	    flag = atoi( line );
	    break;
	case 71:
	    degree = atoi( line );
	    break;
	case 72:
	    numKnots = atoi( line );
	    if ( numKnots > 0 ) {
		knots = (fastf_t *)bu_malloc( numKnots*sizeof(fastf_t),
					      "spline knots" );
	    }
	    break;
	case 73:
	    numCtlPts = atoi( line );
	    if ( numCtlPts > 0 ) {
		ctlPts = (fastf_t *)bu_malloc( numCtlPts*3*sizeof(fastf_t),
					       "spline control points" );
		weights = (fastf_t *)bu_malloc( numCtlPts*sizeof(fastf_t),
						"spline weights" );
	    }
	    for ( i=0; i<numCtlPts; i++ ) {
		weights[i] = 1.0;
	    }
	    break;
	case 74:
	    numFitPts = atoi( line );
	    if ( numFitPts > 0 ) {
		fitPts = (fastf_t *)bu_malloc( numFitPts*3*sizeof(fastf_t),
					       "fit control points" );
	    }
	    break;
	case 42:
	    /* unhandled */
	    /* knotTol = atof( line ) * units_conv[units] * scale_factor; */
	    break;
	case 43:
	    /* unhandled */
	    /* ctlPtTol = atof( line ) * units_conv[units] * scale_factor; */
	    break;
	case 44:
	    /* unhandled */
	    /* fitPtTol = atof( line ) * units_conv[units] * scale_factor; */
	    break;
	case 12:
	case 22:
	case 32:
	    coord = code / 10 - 1;
	    /* start tangent, unimplemented */
	    break;
	case 13:
	case 23:
	case 33:
	    coord = code / 10 - 1;
	    /* end tangent, unimplemented */
	    break;
	case 40:
	    knots[knotCount++] = atof( line );
	    break;
	case 41:
	    weights[weightCount++] = atof( line );
	    break;
	case 10:
	case 20:
	case 30:
	    coord = (code / 10) - 1 + ctlPtCount*3;
	    ctlPts[coord] = atof( line ) * units_conv[units] * scale_factor;
	    subCounter++;
	    if ( subCounter > 2 ) {
		ctlPtCount++;
		subCounter = 0;
	    }
	    break;
	case 11:
	case 21:
	case 31:
	    coord = (code / 10) - 1 + fitPtCount*3;
	    fitPts[coord] = atof( line ) * units_conv[units] * scale_factor;
	    subCounter2++;
	    if ( subCounter2 > 2 ) {
		fitPtCount++;
		subCounter2 = 0;
	    }
	    break;
	case 62:	/* color number */
	    curr_color = atoi( line );
	    break;
	case 0:
	    /* draw the spline */
	    get_layer();
	    layers[curr_layer]->spline_count++;

	    if ( flag & SPLINE_RATIONAL ) {
		ncoords = 4;
		pt_type = RT_NURB_MAKE_PT_TYPE( ncoords, RT_NURB_PT_XYZ, RT_NURB_PT_RATIONAL );
	    } else {
		ncoords = 3;
		pt_type = RT_NURB_MAKE_PT_TYPE( ncoords, RT_NURB_PT_XYZ, RT_NURB_PT_NONRAT );
	    }
	    crv = rt_nurb_new_cnurb( degree+1, numCtlPts+degree+1, numCtlPts, pt_type );

	    for ( i=0; i<numKnots; i++ ) {
		crv->k.knots[i] = knots[i];
	    }
	    for ( i=0; i<numCtlPts; i++ ) {
		crv->ctl_points[i*ncoords + 0] = ctlPts[i*3+0];
		crv->ctl_points[i*ncoords + 1] = ctlPts[i*3+1];
		crv->ctl_points[i*ncoords + 2] = ctlPts[i*3+2];
		if ( flag & SPLINE_RATIONAL ) {
		    crv->ctl_points[i*ncoords + 3] = weights[i];
		}
	    }
	    if ( !layers[curr_layer]->m ) {
		create_nmg();
	    }
	    startParam = knots[0];
	    stopParam = knots[numKnots-1];
	    paramDelta = (stopParam - startParam) / (double)splineSegs;
	    rt_nurb_c_eval( crv, startParam, pt );
	    for ( i=0; i<splineSegs; i++ ) {
		fastf_t param = startParam + paramDelta * (i+1);
		eu = nmg_me( v1, v2, layers[curr_layer]->s );
		v1 = eu->vu_p->v_p;
		if ( i == 0 ) {
		    nmg_vertex_gv( v1, pt );
		}
		rt_nurb_c_eval( crv, param, pt );
		v2 = eu->eumate_p->vu_p->v_p;
		nmg_vertex_gv( v2, pt );

		v1 = v2;
		v2 = NULL;
	    }

	    rt_nurb_free_cnurb( crv );

	    if ( knots != NULL ) bu_free( knots, "spline knots" );
	    if ( weights != NULL ) bu_free( weights, "spline weights" );
	    if ( ctlPts != NULL ) bu_free( ctlPts, "spline control points" );
	    if ( fitPts != NULL ) bu_free( fitPts, "spline fit points" );
	    flag = 0;
	    degree = 0;
	    numKnots = 0;
	    numCtlPts = 0;
	    numFitPts = 0;
	    knotCount = 0;
	    weightCount = 0;
	    ctlPtCount = 0;
	    fitPtCount = 0;
	    subCounter = 0;
	    subCounter2 = 0;
	    knots = NULL;
	    weights = NULL;
	    ctlPts = NULL;
	    fitPts = NULL;

	    curr_state->sub_state = UNKNOWN_ENTITY_STATE;
	    process_entities_code[curr_state->sub_state]( code );
	    break;
    }

    return 0;
}
static int
process_3dface_entities_code( int code )
{
    int vert_no;
    int coord;
    int face[5];

    switch ( code ) {
	case 8:
	    if ( curr_layer_name ) {
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
	    if ( verbose ) {
		bu_log( "3dface vertex #%d coord #%d = %g\n", vert_no, coord, pts[vert_no][coord] );
	    }
	    if ( vert_no == 2 ) {
		pts[3][coord] = pts[2][coord];
	    }
	    break;
	case 62:	/* color number */
	    curr_color = atoi( line );
	    break;
	case 0:
	    /* end of this 3dface */
	    get_layer();
	    if ( verbose ) {
		bu_log( "Found end of 3DFACE\n" );
	    }
	    if ( verbose ) {
		bu_log( "\tmaking two triangles\n" );
	    }
	    layers[curr_layer]->face3d_count++;
	    for ( vert_no=0; vert_no<4; vert_no++ ) {
		point_t tmp_pt1;
		MAT4X3PNT( tmp_pt1, curr_state->xform, pts[vert_no] );
		VMOVE( pts[vert_no], tmp_pt1 );
		face[vert_no] = Add_vert( V3ARGS( pts[vert_no]),
					  layers[curr_layer]->vert_tree_root,
					  tol_sq );
	    }
	    add_triangle( face[0], face[1], face[2], curr_layer );
	    add_triangle( face[2], face[3], face[0], curr_layer );
	    if ( verbose ) {
		bu_log( "finished face\n" );
	    }

	    curr_state->sub_state = UNKNOWN_ENTITY_STATE;
	    process_entities_code[curr_state->sub_state]( code );
	    break;
    }

    return 0;
}

static int
process_entity_code( int code )
{
    return process_entities_code[curr_state->sub_state]( code );
}

static int
process_objects_code( int code )
{
    switch ( code ) {
	case 999:	/* comment */
	    printf( "%s\n", line );
	    break;
	case 0:		/* text string */
	    if ( !strncmp( line, "SECTION", 7 ) ) {
		curr_state->state = UNKNOWN_SECTION;
		break;
	    } else if ( !strncmp( line, "ENDSEC", 6 ) ) {
		curr_state->state = UNKNOWN_SECTION;
		break;
	    }
	    break;
    }

    return 0;
}

static int
process_thumbnail_code( int code )
{
    switch ( code ) {
	case 999:	/* comment */
	    printf( "%s\n", line );
	    break;
	case 0:		/* text string */
	    if ( !strncmp( line, "SECTION", 7 ) ) {
		curr_state->state = UNKNOWN_SECTION;
		break;
	    } else if ( !strncmp( line, "ENDSEC", 6 ) ) {
		curr_state->state = UNKNOWN_SECTION;
		break;
	    }
	    break;
    }

    return 0;
}

/*
 * Create a sketch object based on the wire eddges in an NMG
 */
static struct rt_sketch_internal *
nmg_wire_edges_to_sketch( struct model *m )
{
    struct rt_sketch_internal *skt;
    struct bu_ptbl segs;
    struct nmgregion *r;
    struct shell *s;
    struct edgeuse *eu;
    struct vertex *v;
    struct vert_root *vr;
    size_t idx;

    skt = bu_calloc( 1, sizeof( struct rt_sketch_internal ), "rt_sketch_internal" );
    skt->magic = RT_SKETCH_INTERNAL_MAGIC;
    VSET( skt->V, 0.0, 0.0, 0.0 );
    VSET( skt->u_vec, 1.0, 0.0, 0.0 );
    VSET( skt->v_vec, 0.0, 1.0, 0.0 );

    vr = create_vert_tree();
    bu_ptbl_init( &segs, 64, "segs for sketch" );
    for ( BU_LIST_FOR( r, nmgregion, &m->r_hd ) ) {
	for ( BU_LIST_FOR( s, shell, &r->s_hd ) ) {
            struct edgeuse *eu1;

	    /* add a line segment for each wire edge */
	    bu_ptbl_reset( &segs );
            eu1 = NULL;
	    for ( BU_LIST_FOR( eu, edgeuse, &s->eu_hd ) ) {
		struct line_seg * lseg;
                if( eu == eu1 ) {
                    continue;
                } else {
                    eu1 = eu->eumate_p;
                }
		BU_GETSTRUCT( lseg, line_seg );
		lseg->magic = CURVE_LSEG_MAGIC;
		v = eu->vu_p->v_p;
                lseg->start = Add_vert( V3ARGS(v->vg_p->coord), vr, tol_sq );
		v = eu->eumate_p->vu_p->v_p;
                lseg->end = Add_vert( V3ARGS(v->vg_p->coord), vr, tol_sq );
                if( verbose ) {
                    bu_log( "making sketch line seg from #%d (%g %g %g) to #%d (%g %g %g)\n",
                            lseg->start, V3ARGS( &vr->the_array[lseg->start] ),
                            lseg->end, V3ARGS( &vr->the_array[lseg->end] ) );
                }
		bu_ptbl_ins( &segs, (long int *)lseg );
	    }
	}
    }

    if (BU_PTBL_LEN(&segs) < 1) {
        bu_free( skt, "rt_sketch_internal" );
        return NULL;
    }
    skt->vert_count = vr->curr_vert;
    skt->verts = (point2d_t *)bu_malloc(skt->vert_count * sizeof( point2d_t), "skt->verts");
    for( idx = 0 ; idx < vr->curr_vert ; idx++ ) {
        skt->verts[idx][0] = vr->the_array[idx*3];
        skt->verts[idx][1] = vr->the_array[idx*3 + 1];
    }
    skt->skt_curve.seg_count = BU_PTBL_LEN(&segs);
    skt->skt_curve.reverse = bu_realloc(skt->skt_curve.reverse, skt->skt_curve.seg_count * sizeof (int), "curve segment reverse");
    memset(skt->skt_curve.reverse, 0, skt->skt_curve.seg_count * sizeof (int));
    skt->skt_curve.segments = bu_realloc(skt->skt_curve.segments, skt->skt_curve.seg_count * sizeof ( genptr_t), "curve segments");
    for (idx = 0; idx < BU_PTBL_LEN(&segs); idx++) {
        genptr_t ptr = BU_PTBL_GET(&segs, idx);
        skt->skt_curve.segments[idx] = ptr;
    }

    free_vert_tree(vr);
    bu_ptbl_free( &segs );

    return skt;
}

int
readcodes()
{
    int code;
    size_t line_len;
    static int line_num=0;

    curr_state->file_offset = ftell( dxf );

    if ( bu_fgets( line, MAX_LINE_SIZE, dxf ) == NULL ) {
	return ERROR_FLAG;
    } else {
	code = atoi( line );
    }

    if ( bu_fgets( line, MAX_LINE_SIZE, dxf ) == NULL ) {
	return ERROR_FLAG;
    }

    if ( !strncmp( line, "EOF", 3 ) ) {
	return EOF_FLAG;
    }

    line_len = strlen( line );
    if ( line_len ) {
	line[line_len-1] = '\0';
	line_len--;
    }

    if ( line_len && line[line_len-1] == '\r' ) {
	line[line_len-1] = '\0';
	line_len--;
    }

    if ( verbose ) {
	line_num++;
	bu_log( "%d:\t%d\n", line_num, code );
	line_num++;
	bu_log( "%d:\t%s\n", line_num, line );
    }

    return code;
}

int
main( int argc, char *argv[] )
{
    struct bu_list head_all;
    size_t name_len;
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
    while ((c = bu_getopt(argc, argv, "cdvt:s:")) != -1)
    {
	switch ( c )
	{
	    case 's':	/* scale factor */
		scale_factor = atof( bu_optarg );
		if (scale_factor < SQRT_SMALL_FASTF) {
		    bu_log("scale factor too small (%g < %g)\n", scale_factor, SQRT_SMALL_FASTF);
		    bu_exit(1, "%s", usage);
		}
		break;
	    case 'c':	/* ignore colors */
		ignore_colors = 1;
		break;
	    case 'd':	/* debug */
		bu_debug = BU_DEBUG_COREDUMP;
		break;
	    case 't':	/* tolerance */
		tol = atof( bu_optarg );
		tol_sq = tol * tol;
		break;
	    case 'v':	/* verbose */
		verbose = 1;
		break;
	    default:
                bu_exit(1, "%s", usage);
		break;
	}
    }

    if ( argc - bu_optind < 2 ) {
        bu_exit(1, "%s", usage);
    }

    dxf_file = argv[bu_optind++];
    output_file = argv[bu_optind];

    if ( (dxf=fopen( dxf_file, "rb")) == NULL ) {
	perror( dxf_file );
	bu_exit( 1, "Cannot open DXF file (%s)\n", dxf_file );
    }

    if ( (out_fp = wdb_fopen( output_file )) == NULL ) {
	perror( output_file );
	bu_exit( 1, "Cannot open BRL-CAD geometry file (%s)\n", output_file );
    }

    ptr1 = strrchr( dxf_file, '/' );
    if ( ptr1 == NULL )
	ptr1 = dxf_file;
    else
	ptr1++;
    ptr2 = strchr( ptr1, '.' );

    if ( ptr2 == NULL )
	name_len = strlen( ptr1 );
    else
	name_len = ptr2 - ptr1;

    base_name = (char *)bu_calloc( (unsigned int)name_len + 1, 1, "base_name" );
    bu_strlcpy( base_name , ptr1 , name_len+1 );

    mk_id( out_fp, base_name );

    BU_LIST_INIT( &block_head );
    BU_LIST_INIT( &free_hd );

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
    process_entities_code[TEXT_ENTITY_STATE] = process_text_attrib_entities_code;
    process_entities_code[SOLID_ENTITY_STATE] = process_solid_entities_code;
    process_entities_code[LWPOLYLINE_ENTITY_STATE] = process_lwpolyline_entities_code;
    process_entities_code[MTEXT_ENTITY_STATE] = process_mtext_entities_code;
    process_entities_code[ATTRIB_ENTITY_STATE] = process_text_attrib_entities_code;
    process_entities_code[ATTDEF_ENTITY_STATE] = process_text_attrib_entities_code;
    process_entities_code[ELLIPSE_ENTITY_STATE] = process_ellipse_entities_code;
    process_entities_code[LEADER_ENTITY_STATE] = process_leader_entities_code;
    process_entities_code[SPLINE_ENTITY_STATE] = process_spline_entities_code;

    process_tables_sub_code[UNKNOWN_TABLE_STATE] = process_tables_unknown_code;
    process_tables_sub_code[LAYER_TABLE_STATE] = process_tables_layer_code;

    /* create storage for circles */
    circle_pts = (point_t *)bu_calloc( segs_per_circle, sizeof( point_t ), "circle_pts" );
    for ( i=0; i<segs_per_circle; i++ ) {
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
    for ( i=0; i<max_layers; i++ ) {
	BU_GETSTRUCT( layers[i], layer );
    }
    layers[0]->name = bu_strdup( "noname" );
    layers[0]->color_number = 7;	/* default white */
    layers[0]->vert_tree_root = create_vert_tree();
    bu_ptbl_init( &layers[0]->solids, 8, "layers[curr_layer]->solids" );

    curr_color = layers[0]->color_number;
    curr_layer_name = bu_strdup( layers[0]->name );

    while ( (code=readcodes()) > -900 ) {
	process_code[curr_state->state](code);
    }

    BU_LIST_INIT( &head_all );
    for ( i=0; i<next_layer; i++ ) {
	struct bu_list head;
	int j;

	BU_LIST_INIT( &head );

	if ( layers[i]->color_number < 0 ) layers[i]->color_number = 7;
	if ( layers[i]->curr_tri || BU_PTBL_END( &layers[i]->solids ) || layers[i]->m ) {
	    bu_log( "LAYER: %s, color = %d (%d %d %d)\n", layers[i]->name, layers[i]->color_number, V3ARGS( &rgb[layers[i]->color_number*3]) );
	}

	if ( layers[i]->curr_tri && layers[i]->vert_tree_root->curr_vert > 2 ) {
	    sprintf( tmp_name, "bot.s%d", i );
	    if ( mk_bot( out_fp, tmp_name, RT_BOT_SURFACE, RT_BOT_UNORIENTED, 0,
			 layers[i]->vert_tree_root->curr_vert, layers[i]->curr_tri, layers[i]->vert_tree_root->the_array,
			 layers[i]->part_tris, (fastf_t *)NULL, (struct bu_bitv *)NULL ) ) {
		bu_log( "Failed to make Bot\n" );
	    } else {
		(void)mk_addmember( tmp_name, &head, NULL, WMOP_UNION );
	    }
	}

	for ( j=0; j<BU_PTBL_END( &layers[i]->solids ); j++ ) {
	    (void)mk_addmember((char *)BU_PTBL_GET( &layers[i]->solids, j ), &head,
			       NULL, WMOP_UNION );
	    bu_free( (char *)BU_PTBL_GET( &layers[i]->solids, j), "solid_name" );
	}

	if ( layers[i]->m ) {
	    char name[32];
	    struct rt_sketch_internal *skt;

	    sprintf( name, "sketch.%d", i );
	    skt = nmg_wire_edges_to_sketch( layers[i]->m );
            if( skt != NULL ) {
                mk_sketch(out_fp, name, skt);
                (void) mk_addmember(name, &head, NULL, WMOP_UNION);
            }
	}

	if ( layers[i]->line_count ) {
	    bu_log( "\t%zu lines\n", layers[i]->line_count );
	}

	if ( layers[i]->solid_count ) {
	    bu_log( "\t%zu solids\n", layers[i]->solid_count );
	}

	if ( layers[i]->polyline_count ) {
	    bu_log( "\t%zu polylines\n", layers[i]->polyline_count );
	}

	if ( layers[i]->lwpolyline_count ) {
	    bu_log( "\t%zu lwpolylines\n", layers[i]->lwpolyline_count );
	}

	if ( layers[i]->ellipse_count ) {
	    bu_log( "\t%zu ellipses\n", layers[i]->ellipse_count );
	}

	if ( layers[i]->circle_count ) {
	    bu_log( "\t%zu circles\n", layers[i]->circle_count );
	}

	if ( layers[i]->arc_count ) {
	    bu_log( "\t%zu arcs\n", layers[i]->arc_count );
	}

	if ( layers[i]->text_count ) {
	    bu_log( "\t%zu texts\n", layers[i]->text_count );
	}

	if ( layers[i]->mtext_count ) {
	    bu_log( "\t%zu mtexts\n", layers[i]->mtext_count );
	}

	if ( layers[i]->attrib_count ) {
	    bu_log( "\t%zu attribs\n", layers[i]->attrib_count );
	}

	if ( layers[i]->dimension_count ) {
	    bu_log( "\t%zu dimensions\n", layers[i]->dimension_count );
	}

	if ( layers[i]->leader_count ) {
	    bu_log( "\t%zu leaders\n", layers[i]->leader_count );
	}

	if ( layers[i]->face3d_count ) {
	    bu_log( "\t%zu 3d faces\n", layers[i]->face3d_count );
	}

	if ( layers[i]->point_count ) {
	    bu_log( "\t%zu points\n", layers[i]->point_count );
	}
	if ( layers[i]->spline_count ) {
	    bu_log( "\t%zu splines\n", layers[i]->spline_count );
	}


	if ( BU_LIST_NON_EMPTY( &head ) ) {
	    unsigned char *tmp_rgb;
	    struct bu_vls comb_name;

	    tmp_rgb = &rgb[layers[i]->color_number*3];
	    bu_vls_init( &comb_name );
	    bu_vls_printf( &comb_name, "%s.c.%d", layers[i]->name, i );
	    if ( mk_comb( out_fp, bu_vls_addr( &comb_name ), &head, 1, NULL, NULL,
			  tmp_rgb, 1, 0, 1, 100, 0, 0, 0 ) ) {
		bu_log( "Failed to make region %s\n", layers[i]->name );
	    } else {
		(void)mk_addmember( bu_vls_addr( &comb_name ), &head_all, NULL, WMOP_UNION );
	    }
	}

    }


    if ( BU_LIST_NON_EMPTY( &head_all ) ) {
	struct bu_vls top_name;
	int count=0;

	bu_vls_init(&top_name);
	bu_vls_strcpy( &top_name, "all" );
	while ( db_lookup( out_fp->dbip, bu_vls_addr( &top_name ), LOOKUP_QUIET ) != RT_DIR_NULL ) {
	    count++;
	    bu_vls_trunc( &top_name, 0 );
	    bu_vls_printf( &top_name, "all.%d", count );
	}

	(void)mk_comb( out_fp, bu_vls_addr( &top_name ), &head_all, 0, NULL, NULL, NULL, 0, 0, 0, 0, 0, 0, 0 );
    }

    return 0;
}

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
