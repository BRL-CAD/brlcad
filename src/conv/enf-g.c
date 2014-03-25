/*                         E N F - G . C
 * BRL-CAD
 *
 * Copyright (c) 2001-2014 United States Government as represented by
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
 *
 */
/** @file conv/enf-g.c
 *
 * Program to convert the tessellated Elysium Neutral File format to
 * BRL-CAD format.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include "bio.h"

#include "bu/getopt.h"
#include "db.h"
#include "vmath.h"
#include "nmg.h"
#include "rtgeom.h"
#include "raytrace.h"
#include "wdb.h"
#include "plot3.h"


#define MAX_LINE_SIZE	256

static	FILE *fd_in;
static	struct rt_wdb *fd_out;
static	fastf_t local_tol;
static	fastf_t local_tol_sq;
static	int ident;
static	char *part_name_file=NULL;
static	int use_part_name_hash=0;
static	int max_name_len=0;
static	Tcl_HashTable htbl;
static	int name_not_converted=0;
static	int indent_level=0;
static	int indent_delta=4;

static struct vert_root *tree_root;

#define DO_INDENT	{ int _i; \
				for ( _i=0; _i<indent_level; _i++ ) {\
					bu_log( " " ); \
				} \
			}

static int verbose=0;

extern int optopt;

struct obj_info {
    char obj_type;			/* type of this object (from defines below) */
    char *obj_name;			/* name of this object */
    char *brlcad_comb;		/* unique BRL-CAD name for this region or assembly */
    char *brlcad_solid;		/* unique BRL-CAD name for the solid if this is a region */
    int obj_id;			/* id number (from ENF file) */
    size_t part_count;			/* number of members (for assembly), number of faces (for part) */
    struct obj_info **members;	/* pointer to array of member objects (only valid for assemblies) */
};

/* object types */
#define UNKNOWN_TYPE	0
#define PART_TYPE	1
#define ASSEMBLY_TYPE	2

static int *part_tris=NULL;		/* list of triangles for current part */
static size_t max_tri=0;		/* number of triangles currently malloced */
static size_t curr_tri=0;		/* number of triangles currently being used */
static char progname[]="enf-g";

#define TRI_BLOCK 512			/* number of triangles to malloc per call */

void
lower_case( char *name )
{
    unsigned char *c;

    c = (unsigned char *)name;
    while ( *c ) {
	(*c) = tolower( *c );
	c++;
    }
}

void
create_name_hash( FILE *fd )
{
    char line[MAX_LINE_SIZE];
    Tcl_HashEntry *hash_entry=NULL;
    int new_entry=0;

    Tcl_InitHashTable( &htbl, TCL_STRING_KEYS );

    while ( bu_fgets( line, MAX_LINE_SIZE, fd ) ) {
	char *part_no, *desc, *ptr;

	ptr = strtok( line, " \t\n" );
	if ( !ptr )
	    bu_exit( 1, "%s: *****Error processing part name file at line:\n\t%s\n", progname,line );
	part_no = bu_strdup( ptr );
	lower_case( part_no );
	ptr = strtok( (char *)NULL, " \t\n" );
	if ( !ptr )
	    bu_exit( 1, "%s: *****Error processing part name file at line:\n\t%s\n", progname,line );
	desc = bu_strdup( ptr );
	lower_case( desc );

	hash_entry = Tcl_CreateHashEntry( &htbl, part_no, &new_entry );
	if ( new_entry ) {
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
bad_triangle( int v[3], fastf_t *vertices )
{
    fastf_t dist;
    fastf_t coord;
    int i;

    if ( v[0] == v[1] || v[1] == v[2] || v[0] == v[2] )
	return 1;

    dist = 0;
    for ( i=0; i<3; i++ ) {
	coord = vertices[v[0]*3+i] - vertices[v[1]*3+i];
	dist += coord * coord;
    }
    dist = sqrt( dist );
    if ( dist < local_tol ) {
	return 1;
    }

    dist = 0;
    for ( i=0; i<3; i++ ) {
	coord = vertices[v[1]*3+i] - vertices[v[2]*3+i];
	dist += coord * coord;
    }
    dist = sqrt( dist );
    if ( dist < local_tol ) {
	return 1;
    }

    dist = 0;
    for ( i=0; i<3; i++ ) {
	coord = vertices[v[0]*3+i] - vertices[v[2]*3+i];
	dist += coord * coord;
    }
    dist = sqrt( dist );
    if ( dist < local_tol ) {
	return 1;
    }

    return 0;
}


/* routine to add a new triangle to the current part */
void
add_triangle( int v[3] )
{
    if ( curr_tri >= max_tri ) {
	/* allocate more memory for triangles */
	max_tri += TRI_BLOCK;
	part_tris = (int *)bu_realloc( part_tris, sizeof(int) * max_tri * 3, "part_tris" );
    }

    /* fill in triangle info */
    VMOVE( &part_tris[curr_tri*3], v );

    /* increment count */
    curr_tri++;
}


void
List_assem( struct obj_info *assem )
{
    size_t i;

    if ( assem->obj_type != ASSEMBLY_TYPE ) {
	bu_log( "ERROR: List_assem called for non-assembly\n" );
    }
    bu_log( "Assembly: %s (id=%d)\n", assem->obj_name, assem->obj_id );
    bu_log( "\t%zu members\n", assem->part_count );
    for ( i=0; i<assem->part_count; i++ ) {
	bu_log( "\t\ty %s\n", assem->members[i]->obj_name );
    }
}

void
Usage(void)
{
    bu_log( "Usage: %s [-i starting_ident] [-t tolerance] [-l name_length_limit] [-n part_number_to_name_list] input_facets_file output_brlcad_file.g\n",progname);
}

void
Make_brlcad_names( struct obj_info *part )
{
    struct bu_vls vls = BU_VLS_INIT_ZERO;
    int count=0;
    char *tmp_name, *ptr;
    Tcl_HashEntry *hash_entry=NULL;

    if ( use_part_name_hash ) {
	hash_entry = Tcl_FindHashEntry( &htbl, part->obj_name );
	if ( !hash_entry ) {
	    /* try without any name extension */
	    if ( (ptr=strrchr( part->obj_name, '_' )) != NULL ) {
		bu_vls_strncpy( &vls, part->obj_name, (ptr - part->obj_name) );
		hash_entry = Tcl_FindHashEntry( &htbl, bu_vls_addr( &vls ) );
	    }
	}

	if ( !hash_entry ) {
	    /* try without any name extension */
	    if ( (ptr=strchr( part->obj_name, '_' )) != NULL ) {
		bu_vls_strncpy( &vls, part->obj_name, (ptr - part->obj_name) );
		hash_entry = Tcl_FindHashEntry( &htbl, bu_vls_addr( &vls ) );
	    }
	}

	if ( !hash_entry ) {
	    /* try adding "-011" */
	    if ( (ptr=strchr( part->obj_name, '-' ))  != NULL ) {
		bu_vls_strncpy( &vls, part->obj_name, (ptr - part->obj_name) );
		bu_vls_strcat( &vls, "-011" );
		hash_entry = Tcl_FindHashEntry( &htbl, bu_vls_addr( &vls ) );
	    }
	}

	if ( !hash_entry ) {
	    name_not_converted++;
	}
    }

    bu_vls_free( &vls );
    if ( hash_entry ) {
	tmp_name = bu_strdup( (char *)Tcl_GetHashValue( hash_entry ) );
    } else {
	if ( use_part_name_hash ) {
	    bu_log( "\tWarning: no name found for part %s\n", part->obj_name );
	}
	/* make a copy of object name, then make it a legal BRL-CAD name */
	if ( strlen( part->obj_name ) < 1 ) {
	    tmp_name = bu_strdup( "s.1" );
	} else {
	    tmp_name = bu_strdup( part->obj_name );
	    ptr = tmp_name;
	    while ( *ptr != '\0' ) {
		if ( !(isalnum( (int)*ptr ) || *ptr == '-')) {
		    *ptr = '_';
		}
		ptr++;
	    }
	}
    }

    if ( part->obj_type == PART_TYPE ) {
	/* find a unique solid name */
	bu_vls_printf( &vls, "s.%s", tmp_name );
	if ( max_name_len ) {
	    bu_vls_trunc( &vls, max_name_len );
	}
	while ( db_lookup( fd_out->dbip, bu_vls_addr( &vls ), LOOKUP_QUIET ) != RT_DIR_NULL) {
	    count++;

	    if ( max_name_len ) {
		int digits = 1;
		int val = 10;

		while ( count >= val ) {
		    digits++;
		    val *= 10;
		}

		bu_vls_trunc( &vls, 0 );
		bu_vls_printf( &vls, "s.%s", tmp_name );
		bu_vls_trunc( &vls, max_name_len - digits - 1 );
		bu_vls_printf( &vls, ".%d", count );
	    } else {
		bu_vls_trunc( &vls, 0 );
		bu_vls_printf( &vls, "s.%s.%d", tmp_name, count );
	    }
	}
	part->brlcad_solid = bu_vls_strgrab( &vls );
    } else {
	part->brlcad_solid = NULL;
    }

    /* find a unique non-primitive name */
    bu_vls_printf( &vls, "%s", tmp_name );
    if ( max_name_len ) {
	bu_vls_trunc( &vls, max_name_len );
    }
    while ( db_lookup( fd_out->dbip, bu_vls_addr( &vls ), LOOKUP_QUIET) != RT_DIR_NULL ) {
	count++;

	if ( max_name_len ) {
	    int digits = 1;
	    int val = 10;

	    while ( count >= val ) {
		digits++;
		val *= 10;
	    }
	    bu_vls_trunc( &vls, 0 );
	    bu_vls_printf( &vls, "%s", tmp_name );
	    bu_vls_trunc( &vls, max_name_len - digits - 1 );
	    bu_vls_printf( &vls, ".%d", count );
	} else {
	    bu_vls_trunc( &vls, 0 );
	    bu_vls_printf( &vls, "%s.%d", tmp_name, count );
	}
    }
    part->brlcad_comb = bu_vls_strgrab( &vls );

    switch ( part->obj_type ) {
	case UNKNOWN_TYPE:
	    bu_log( "ERROR: Unknown object type for %s\n", part->obj_name );
	    break;
	case PART_TYPE:
	    if ( use_part_name_hash ) {
		DO_INDENT
		    bu_log( "part %s changed name to (%s)\n",
			    part->obj_name,
			    part->brlcad_comb );
	    } else {
		DO_INDENT
		    bu_log( "part %s\n", part->brlcad_comb );
	    }
	    break;
	case ASSEMBLY_TYPE:
	    if ( use_part_name_hash ) {
		DO_INDENT
		    bu_log( "assembly %s changed name to (%s)\n",
			    part->obj_name,
			    part->brlcad_comb );
	    } else {
		DO_INDENT
		    bu_log( "assembly %s\n", part->brlcad_comb );
	    }
	    break;
    }

    bu_free( tmp_name, "tmp_name" );
}


struct obj_info *
Part_import( int id_start )
{
    char line[MAX_LINE_SIZE];
    struct obj_info *part;
    struct wmember reg_head;
    unsigned char rgb[3];
    int surf_count=0;
    int id_end;
    int last_surf=0;
    int i;
    int tri[3];
    int corner_index=-1;

    clean_vert_tree( tree_root );

    VSETALL( rgb, 128 );

    BU_ALLOC(part, struct obj_info);
    part->obj_type = PART_TYPE;
    part->obj_id = id_start;
    while ( bu_fgets( line, MAX_LINE_SIZE, fd_in ) ) {
	if ( !bu_strncmp( line, "PartName", 8 ) ) {
	    line[strlen( line ) - 1] = '\0';
	    part->obj_name = bu_strdup( &line[9] );
	    lower_case( part->obj_name );
	    Make_brlcad_names( part );
	} else if ( !bu_strncmp( line, "FaceCount", 9 ) ) {
	    surf_count = atoi( &line[10] );
	    if ( surf_count == 0 ) {
		last_surf = 1;
	    }
	} else if ( !bu_strncmp( line, "EndPartId", 9 ) ) {
	    /* found end of part, check id */
	    id_end = atoi( &line[10] );
	    if ( id_end != id_start )
		bu_exit( 1, "%s: ERROR: found end of part id %d while processing part %d\n", progname,id_end, id_start );
	    if ( last_surf ) {
		break;
	    }
	} else if ( !bu_strncmp( line, "FaceRGB", 7 ) ) {
	    /* get face color */
	    char *ptr;

	    i = 8;
	    ptr = strtok( &line[i], " \t" );
	    for ( i=0; i<3 && ptr; i++ ) {
		rgb[i] = atof( ptr );
		ptr = strtok( (char *)NULL, " \t" );
	    }
	} else if ( !bu_strncmp( line, "Facet", 5 ) ) {
	    /* read a triangle */
	    VSETALL( tri, -1 );
	    corner_index = -1;
	} else if ( !bu_strncmp( line, "Face", 4 ) ) {
	    /* start of a surface */
	    int surf_no;

	    surf_no = atoi( &line[5] );
	    if ( surf_no == surf_count ) {
		last_surf = 1;
	    }
	} else if ( !bu_strncmp( line, "TriangleCount", 13 ) ) {
	    /* get number of triangles for this surface */
	} else if ( !bu_strncmp( line, "Vertices", 9 ) ) {
	    /* get vertex list for this triangle */
	} else if ( !bu_strncmp( line, "Vertex", 6 ) ) {
	    /* get a vertex */
	    char *ptr = NULL;
	    vect_t v = VINIT_ZERO;

	    i = 7;
	    while ( !isspace( (int)line[i] ) && line[i] != '\0' )
		i++;
	    ptr = strtok( &line[i], " \t" );
	    for ( i=0; i<3 && ptr; i++ ) {
		v[i] = atof( ptr );
		ptr = strtok( (char *)NULL, " \t" );
	    }
	    tri[++corner_index] = Add_vert( V3ARGS( v ), tree_root, local_tol_sq );
	    if ( corner_index == 2 ) {
		if ( !bad_triangle( tri, tree_root->the_array ) ) {
		    add_triangle( tri );
		}
	    }
	} else if ( !bu_strncmp( line, "Normal", 6 ) ) {
	    /* get a vertex normal */
	} else if ( !bu_strncmp( line, "PointCount", 10 ) ) {
	    /* get number of vertices for this surface */
	} else
	    bu_exit( 1, "%s: ERROR: unrecognized line encountered while processing part id %d:\n%s\n", progname,id_start, line );
    }

    if ( curr_tri == 0 ) {
	/* no facets in this part, so ignore it */
	bu_free( (char *)part, "part" );
	part = (struct obj_info *)NULL;
    } else {

	/* write this part to database, first make a primitive solid */
	if ( mk_bot( fd_out, part->brlcad_solid, RT_BOT_SOLID, RT_BOT_UNORIENTED, 0,
		     tree_root->curr_vert, curr_tri, tree_root->the_array, part_tris, NULL, NULL ) )
	    bu_exit( 1, "%s: Failed to write primitive %s (%s) to database\n", progname,part->brlcad_solid, part->obj_name );
	if ( verbose ) {
	    DO_INDENT;
	    bu_log( "Wrote BOT %s\n", part->brlcad_solid );
	}

	/* then a region */
	BU_LIST_INIT( &reg_head.l );
	if ( mk_addmember( part->brlcad_solid, &reg_head.l, NULL, WMOP_UNION ) == WMEMBER_NULL )
	    bu_exit( 1, "%s: ERROR: Failed to add solid (%s), to region (%s)\n", progname,part->brlcad_solid, part->brlcad_comb );
	if ( mk_comb( fd_out, part->brlcad_comb, &reg_head.l, 1, NULL, NULL, rgb, ident++,
		      0, 1, 100, 0, 0, 0 ) )
	    bu_exit( 1, "%s: Failed to write region %s (%s) to database\n", progname,part->brlcad_comb, part->obj_name );
	if ( verbose ) {
	    DO_INDENT;
	    bu_log( "Wrote region %s\n", part->brlcad_comb );
	}

	if ( use_part_name_hash ) {
	    if ( db5_update_attribute( part->brlcad_comb, "Part_No",
				       part->obj_name, fd_out->dbip ) ) {
		bu_log( "Failed to assign Part_no attribute to %s\n",
			part->brlcad_comb );
	    }
	}
    }

    /* free some memory */
    if ( part_tris ) {
	bu_free( (char *)part_tris, "part_tris" );
    }
    max_tri = 0;
    curr_tri = 0;
    part_tris = NULL;

    return part;
}

struct obj_info *
Assembly_import( int id_start )
{
    char line[MAX_LINE_SIZE];
    struct obj_info *this_assem, *member;
    struct wmember assem_head;
    int id_end, member_id;
    size_t i;

    BU_ALLOC(this_assem, struct obj_info);

    this_assem->obj_type = ASSEMBLY_TYPE;
    this_assem->obj_id = id_start;
    this_assem->part_count = 0;
    this_assem->members = NULL;
    while ( bu_fgets( line, MAX_LINE_SIZE, fd_in ) ) {
	if ( !bu_strncmp( line, "AssemblyName", 12 ) ) {
	    line[strlen( line ) - 1] = '\0';
	    this_assem->obj_name = bu_strdup( &line[13] );
	    lower_case( this_assem->obj_name );
	    DO_INDENT;
	    bu_log( "Start of assembly %s (id = %d)\n", this_assem->obj_name, id_start );
	    indent_level += indent_delta;
	} else if ( !bu_strncmp( line, "PartId", 6 ) ) {
	    /* found a member part */
	    member_id = atoi( &line[7] );
	    member = Part_import( member_id );
	    if ( !member )
		continue;
	    this_assem->part_count++;
	    this_assem->members = (struct obj_info **)bu_realloc(
		this_assem->members,
		this_assem->part_count * sizeof( struct obj_info *),
		"this_assem->members" );
	    this_assem->members[this_assem->part_count-1] = member;
	} else if ( !bu_strncmp( line, "AssemblyId", 10 ) ) {
	    /* found a member assembly */
	    member_id = atoi( &line[11] );
	    member = Assembly_import( member_id );
	    this_assem->part_count++;
	    this_assem->members = (struct obj_info **)bu_realloc(
		this_assem->members,
		this_assem->part_count * sizeof( struct obj_info *),
		"this_assem->members" );
	    this_assem->members[this_assem->part_count-1] = member;
	} else if ( !bu_strncmp( line, "EndAssemblyId", 13 ) ) {
	    /* found end of assembly, make sure it is this one */
	    id_end = atoi( &line[14] );
	    if ( id_end != id_start )
		bu_exit( 1, "%s: ERROR: found end of assembly id %d while processing id %d\n", progname,id_end, id_start );
	    indent_level -= indent_delta;
	    DO_INDENT;
	    bu_log( "Found end of assembly %s (id = %d)\n",  this_assem->obj_name, id_start );
	    break;
	} else {
	    bu_log( "%s: Unrecognized line encountered while processing assembly id %d:\n", progname,id_start );
	    bu_exit( 1, "%s\n", line );
	}
    }

    Make_brlcad_names( this_assem );

    /* write this assembly to the database */
    BU_LIST_INIT( &assem_head.l );

    for ( i=0; i<this_assem->part_count; i++ )
	if ( mk_addmember( this_assem->members[i]->brlcad_comb,
			   &assem_head.l, NULL, WMOP_UNION ) == WMEMBER_NULL )
	    bu_exit( 1, "%s: ERROR: Failed to add region %s to assembly %s\n",
			progname,this_assem->members[i]->brlcad_comb, this_assem->brlcad_comb );

    if ( mk_comb( fd_out, this_assem->brlcad_comb, &assem_head.l, 0, NULL, NULL, NULL,
		  0, 0, 0, 0, 0, 0, 0 ) )
	bu_exit( 1, "%s: ERROR: Failed to write combination (%s) to database\n", progname,this_assem->brlcad_comb );
    if ( use_part_name_hash ) {
	if ( db5_update_attribute( this_assem->brlcad_comb, "Part_No",
				   this_assem->obj_name, fd_out->dbip ) ) {
	    bu_log( "Failed to assign Part_no attribute to %s\n",
		    this_assem->brlcad_comb );
	}
    }

    return this_assem;
}

int
main( int argc, char *argv[] )
{
    char line[MAX_LINE_SIZE];
    char *input_file, *output_file;
    FILE *fd_parts;
    struct obj_info **top_level_assems = NULL;
    int top_level_assem_count = 0;
    int curr_top_level = -1;
    fastf_t tmp;
    int id;
    int c;

    bu_setprogname(argv[0]);

    local_tol = 0.0005;
    local_tol_sq = local_tol * local_tol;
    ident = 1000;

    while ( (c=bu_getopt( argc, argv, "vi:t:n:l:h?" ) ) != -1 ) {
	switch ( c ) {
	    case 'v':	/* verbose */
		verbose = 1;
		break;
	    case 'i':	/* starting ident number */
		ident = atoi( bu_optarg );
		break;
	    case 't':	/* tolerance */
		tmp = atof( bu_optarg );
		if ( tmp <= 0.0 )
		    bu_exit( 1, "%s: Illegal tolerance (%g), must be > 0.0\n", progname,tmp );
		break;
	    case 'n':	/* part name list */
		part_name_file = bu_optarg;
		use_part_name_hash = 1;
		break;
	    case 'l':	/* max name length */
		max_name_len = atoi( bu_optarg );
		if ( max_name_len < 5 )
		    bu_exit( 1, "%s: Unreasonable name length limitation\n",progname );
		break;
	    default:
		Usage();
		bu_exit( 1, NULL );
	}
    }

    if ( argc - bu_optind != 2 ) {
	bu_log( "Not enough arguments!! (need at least input & output file names)\n" );
	Usage();
	bu_exit( 1, NULL );
    }

    input_file = bu_strdup( argv[bu_optind] );

    if ((fd_in=fopen(input_file, "rb")) == NULL) {
	bu_log( "%s: Cannot open %s for reading\n", progname,input_file );
	perror( argv[0] );
	bu_exit( 1, NULL );
    }

    output_file = bu_strdup( argv[bu_optind+1] );

    if ( (fd_out=wdb_fopen( output_file )) == NULL ) {
	bu_log( "%s: Cannot open %s for writing\n", progname,output_file );
	perror( argv[0] );
	bu_exit( 1, NULL );
    }

    if ( use_part_name_hash ) {
	if ( (fd_parts=fopen( part_name_file, "rb" )) == NULL ) {
	    bu_log( "%s,Cannot open part name file (%s)\n", progname,part_name_file );
	    perror( argv[0] );
	    bu_exit( 1, NULL );
	}

	create_name_hash( fd_parts );
    }

    tree_root = create_vert_tree();

    /* finally, start processing the input */
    while ( bu_fgets( line, MAX_LINE_SIZE, fd_in ) ) {
	if ( !bu_strncmp( line, "FileName", 8 ) ) {
	    bu_log( "Converting facets originally from %s",
		    &line[9] );
	} else if ( !bu_strncmp( line, "TopAssemblies", 13 ) ) {
	    bu_log( "Top level assemblies: %s", &line[14] );
	    top_level_assem_count = atoi( &line[14] );
	    if ( top_level_assem_count < 1 ) {
		top_level_assems = (struct obj_info **)NULL;
	    } else {
		top_level_assems = (struct obj_info **)bu_calloc( top_level_assem_count,
								  sizeof( struct obj_info * ),
								  "top_level_assems" );
	    }
	} else if ( !bu_strncmp( line, "PartCount", 9 ) ) {
	    bu_log( "Part count: %s", &line[10] );
	} else if ( !bu_strncmp( line, "AssemblyId", 10 ) ) {
	    id = atoi( &line[11] );
	    curr_top_level++;
	    if ( curr_top_level >= top_level_assem_count ) {
		bu_log( "Warning: too many top level assemblies\n" );
		bu_log( "\texpected %d, this os number %d\n",
			top_level_assem_count, curr_top_level+1 );
		top_level_assem_count = curr_top_level+1;
		top_level_assems = (struct obj_info **)bu_realloc( top_level_assems,
								   top_level_assem_count *
								   sizeof( struct obj_info * ),
								   "top_level_assems" );
	    }
	    top_level_assems[curr_top_level] = Assembly_import( id );
	} else if ( !bu_strncmp( line, "PartId", 6 ) ) {
	    /* found a top-level part */
	    id = atoi( &line[7] );
	    (void)Part_import( id );
	}
    }

    if ( name_not_converted ) {
	bu_log( "Warning %d objects were not found in the part number to name mapping,\n",
		name_not_converted );
	bu_log( "\ttheir names remain as part numbers.\n" );
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
