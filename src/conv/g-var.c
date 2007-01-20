/*                     G - V A R . C
 * BRL-CAD
 *
 * Copyright (c) 2002-2007 United States Government as represented by
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
 *
 */
/** @file g-var.c
 *
 *  BRL-CAD to (OpenGL) Vertex Array Exporter.
 *
 *  Author -
 *      Prasad P. Silva
 *
 *  Source -
 *      The U. S. Army Research Laboratory
 *      Aberdeen Proving Ground, Maryland  21005-5068  USA
 */

#ifndef lint
static const char RCSid[] = "$Header $";
#endif

#include "common.h"

/* system headers */
#include <stdlib.h>
#include <stdio.h>
#ifdef HAVE_UNISTD_H
#  include <unistd.h>
#endif
#ifndef HAVE_STRING_H
#  include <string.h>
#else
#  include <strings.h>
#endif
#ifdef HAVE_STDINT_H
#  include <stdint.h>
#endif
#include <math.h>

/* interface headers */
#include "machine.h"		/* machine specific definitions */
#include "wdb.h"
#include "raytrace.h"

#define MESH_FORMAT_VERSION 2

/*
 *          S T R U C T   M E S H
 */
struct mesh {
    char			*name;
    struct rt_bot_internal 	*bot;
    struct mesh 		*next;
};


static char		usage[] = "Usage: %s [-v] [-y] [-s scale] [-f] [-o out_file] brlcad_db.h object\n";

static uint8_t 		verbose = 0;
static uint8_t		yup = 0;
static uint8_t		flip_normals = 0;
static float		scale = 0.001f;
static char		*out_file = NULL;
static char		*db_file = NULL;
static char		*object = NULL;

static FILE		*fp_out;
static struct db_i	*dbip;

#ifndef WORDS_BIGENDIAN
static uint8_t		endian = 0;
#else
static uint8_t		endian = 1;
#endif
static uint8_t		format_version = MESH_FORMAT_VERSION;
static struct mesh	*head = NULL;
static struct mesh	*curr = NULL;
static uint32_t		mesh_count = 0;
static uint32_t 	total_vertex_count = 0;
static uint32_t 	total_face_count = 0;

/*
 *                 M E S H   T R A C K E R
 */
void mesh_tracker( struct db_i *dbip, struct directory *dp, genptr_t ptr )
{
    struct rt_db_internal internal;

    /* leaf node must be a solid */
    if ( !( dp->d_flags & DIR_SOLID ) )	{
	fprintf(stderr, "warning: '%s' is not a solid! (not processed)\n", dp->d_namep);
	return;
    }

    /* solid must be a bot */
    if ( rt_db_get_internal( &internal, dp, dbip, NULL, &rt_uniresource ) != ID_BOT ) {
	fprintf(stderr, "warning: '%s' is not a bot! (not processed)\n", dp->d_namep);
	return;
    }
    /* track bot */
    if ( NULL == curr )	{
	head = (struct mesh *)bu_malloc( sizeof( struct mesh ), dp->d_namep );
	head->name = dp->d_namep;
	head->bot = (struct rt_bot_internal *)internal.idb_ptr;
	head->next = NULL;
	curr = head;
    } else {
	curr->next = (struct mesh *)bu_malloc( sizeof( struct mesh ), dp->d_namep );
	curr = curr->next;
	curr->name = dp->d_namep;
	curr->bot = (struct rt_bot_internal *)internal.idb_ptr;
	curr->next = NULL;
    }
    /* accumulate counts */
    total_vertex_count += curr->bot->num_vertices;
    total_face_count += curr->bot->num_faces;
    mesh_count++;
}


/*
 *        D E A L L O C   M E S H   L I S T
 */
void dealloc_mesh_list()
{
    struct mesh* tmp;
    curr = head;
    while ( curr != NULL ) {
	tmp = curr;
	curr = curr->next;
	bu_free( tmp, "a mesh" );
    }
}


/*
 *       W R I T E   H E A D E R
 */
void write_header( struct db_i *dbip )
{
    uint16_t len;
    /*
      Header format:
      Endian (1 byte) {0=little; !0=big}
      File format version (1 byte)
      Size of Model Name String (2 bytes - short)
      Model Name String (n bytes - char[])
      Number of Meshes (4 bytes - int)
      Total number of vertices (4 bytes - int)
      Total number of faces (4 bytes - int)
    */

    /* endian */
    fwrite( &endian, sizeof(char), 1, fp_out );
    /* format version */
    fwrite( &format_version, sizeof(char), 1, fp_out );
    len = strlen( dbip->dbi_title );
    /* model name string length */
    fwrite( &len, sizeof(uint16_t), 1, fp_out );
    /* model name string */
    fwrite( dbip->dbi_title, sizeof(char), len, fp_out );
    /* mesh count */
    fwrite( &mesh_count, sizeof(uint32_t), 1, fp_out );
    /* total number of vertices */
    fwrite( &total_vertex_count, sizeof(uint32_t), 1, fp_out );
    /* total number of faces */
    fwrite( &total_face_count, sizeof(uint32_t), 1, fp_out );
}


/*
 *     G E T   V E R T E X
 */
void get_vertex( struct rt_bot_internal *bot, int idx, float *dest )
{
    dest[0] = bot->vertices[3*idx] * scale;
    dest[1] = bot->vertices[3*idx+1] * scale;
    dest[2] = bot->vertices[3*idx+2] * scale;

    if ( yup ) {
	/* perform 90deg x-axis rotation */
	float q = -(M_PI/2.0f);
	float y = dest[1];
	float z = dest[2];
	dest[1] = y * cos(q) - z * sin(q);
	dest[2] = y * sin(q) + z * cos(q);
    }

}

/*
 *    C O M P U T E   N O R M A L
 */
void compute_normal( struct rt_bot_internal *bot, int p1, int p2,
		     int p3, float *dest )
{
    float v1[3];
    float v2[3];
    float v3[3];
    float vec1[3];
    float vec2[3];
    float fnorm[3];
    float temp[3];
    float *np1, *np2, *np3;

    /* get face normal */
    get_vertex( bot, p1, v1 );
    if ( flip_normals )	{
	get_vertex( bot, p3, v2 );
	get_vertex( bot, p2, v3 );
    } else {
	get_vertex( bot, p2, v2 );
	get_vertex( bot, p3, v3 );
    }

    VSUB2( vec1, v1, v2 );
    VSUB2( vec2, v1, v3 );
    VCROSS( fnorm, vec1, vec2 );
    VUNITIZE( fnorm );

    /* average existing normal with face normal per vertex */
    np1 = dest + 3*p1;
    np2 = dest + 3*p2;
    np3 = dest + 3*p3;
    VADD2( temp, fnorm, np1 );
    VUNITIZE( temp );
    VMOVE( np1, temp );
    VADD2( temp, fnorm, np2 );
    VUNITIZE( temp );
    VMOVE( np2, temp );
    VADD2( temp, fnorm, np3 );
    VUNITIZE( temp );
    VMOVE( np3, temp );
}

/*
 *     G E T   N O R M A L S
 */
void get_normals( struct rt_bot_internal *bot, float *dest )
{
    int i;
    for (i=0; i < bot->num_faces; i++) {
	compute_normal( curr->bot, bot->faces[3*i], bot->faces[3*i+1],
			bot->faces[3*i+2], dest );
    }
}

/*
 *      W R I T E   M E S H   D A T A
 */
void write_mesh_data()
{
    /*
      Data format:
      Size of Mesh Name String (2 bytes - short)
      Mesh Name String (n chars)
      Number of vertices (4 bytes - int)
      Number of faces (4 bytes - int)
      Vertex triples (m*3*4 bytes - float[])
      Normal triples (m*3*4 bytes - float[])
      Face index format (1 byte) {0=1 byte; 1=2 bytes - short; 2=4 bytes - int}
      Face triples (m*3*4 bytes - int[])
    */

    curr = head;
    while ( NULL != curr ) {
	uint16_t len;
	uint32_t nvert, nface;
	int i;
	float vec[3];
	char format;

	/* face triples */
	uint8_t ind8[3] = {0,0,0};
	uint16_t ind16[3] = {0,0,0};
	uint32_t ind32[3] = {0,0,0};

	if ( verbose ) {
	    fprintf( stderr, ">> writing out mesh '%s' (%u, %u)\n", curr->name,
		     curr->bot->num_vertices, curr->bot->num_faces );
	}

	len = strlen( curr->name );
	/* mesh name string length */
	fwrite( &len, sizeof(uint16_t), 1, fp_out );
	/* mesh name string */
	fwrite( curr->name, sizeof(char), len, fp_out );
	nvert = curr->bot->num_vertices;
	nface = curr->bot->num_faces;
	/* number of vertices */
	fwrite( &nvert, sizeof(uint32_t), 1, fp_out );
	/* number of faces */
	fwrite( &nface, sizeof(uint32_t), 1, fp_out );

	/* vertex triples */
	for (i=0; i < curr->bot->num_vertices; i++) {
	    get_vertex( curr->bot, i, vec );
	    fwrite( vec, sizeof(float), 3, fp_out );
	}
	/* normal triples */
	if ( curr->bot->num_normals == curr->bot->num_vertices ) {
	    if ( verbose )
		fprintf(stderr, ">> .. normals found!\n");
	    /* normals are provided */
	    fwrite( curr->bot->normals, sizeof(float), curr->bot->num_normals * 3, fp_out );
	} else {
	    float *normals;
	    if ( verbose ) {
		fprintf(stderr, ">> .. normals will be computed\n");
	    }
	    /* normals need to be computed */
	    normals = bu_calloc( sizeof(float), curr->bot->num_vertices * 3, "normals" );
	    get_normals( curr->bot, normals );
	    fwrite( normals, sizeof(float), curr->bot->num_vertices * 3, fp_out );
	    bu_free( normals, "normals" );
	}

	if ( nface < 1<<8 ) {
	    format = 0;
	} else if ( nface < 1<<16 ) {
	    format = 1;
	} else {
	    format = 2;
	}
	/* face index format */
	fwrite( &format, sizeof(char), 1, fp_out );
	switch (format) {
	    case 0:
		for ( i=0; i< nface; i++) {
		    ind8[0] = curr->bot->faces[3*i];
		    if ( flip_normals ) {
			ind8[1] = curr->bot->faces[3*i+2];
			ind8[2] = curr->bot->faces[3*i+1];
		    } else {
			ind8[1] = curr->bot->faces[3*i+1];
			ind8[2] = curr->bot->faces[3*i+2];
		    }
		    fwrite(&ind8, sizeof(uint8_t), 3, fp_out );
		}
		break;
	    case 1:
		for ( i=0; i< nface; i++) {
		    ind16[0] = curr->bot->faces[3*i];
		    if ( flip_normals ) {
			ind16[1] = curr->bot->faces[3*i+2];
			ind16[2] = curr->bot->faces[3*i+1];
		    } else {
			ind16[1] = curr->bot->faces[3*i+1];
			ind16[2] = curr->bot->faces[3*i+2];
		    }
		    fwrite( &ind16, sizeof(uint16_t), 3, fp_out );
		}
		break;
	    case 2:
		for ( i=0; i< nface; i++) {
		    ind32[0] = curr->bot->faces[3*i];
		    if ( flip_normals ) {
			ind32[1] = curr->bot->faces[3*i+2];
			ind32[2] = curr->bot->faces[3*i+1];
		    } else {
			ind32[1] = curr->bot->faces[3*i+1];
			ind32[2] = curr->bot->faces[3*i+2];
		    }
		    fwrite( &ind32, sizeof(uint32_t), 3, fp_out );
		}
		break;
	    default:
		break;
	}

	curr = curr->next;
    }
}

/*
 *                                M A I N
 */
int main(int argc, char *argv[])
{
    register int	c;
    struct directory* dp;

    /* setup BRLCAD environment */
    bu_setlinebuf( stderr );
    rt_init_resource( &rt_uniresource, 0, NULL );

    /* process command line arguments */
    while ( (c = bu_getopt(argc, argv, "vo:ys:f") ) != EOF ) {
	switch(c) {
	    case 'v':
		verbose++;
		break;

	    case 'o':
		out_file = bu_optarg;
		break;

	    case 'y':
		yup++;
		break;

	    case 's':
		sscanf( bu_optarg, "%f", &scale );
		break;

	    case 'f':
		flip_normals++;
		break;

	    default:
		fprintf(stderr, usage, argv[0]);
		exit(1);
		break;
	}
    }
    /* param check */
    if (bu_optind+1 >= argc) {
	fprintf(stderr, usage, argv[0]);
	exit(1);
    }
    /* get database filename and object */
    db_file = argv[bu_optind++];
    object = argv[bu_optind];

    /* open BRL-CAD database */
    if ( (dbip = db_open( db_file, "r") ) == DBI_NULL ) {
	bu_log( "Cannot open %s\n", db_file );
	perror(argv[0]);
	exit(1);
    }
    if ( db_dirbuild( dbip ) ) {
	bu_bomb( "db_dirbuild() failed!\n" );
    }
    if ( verbose ) {
	fprintf(stderr, ">> opened db '%s'\n", dbip->dbi_title);
    }

    /* setup output stream */
    if ( out_file == NULL ) {
	fp_out = stdout;
    } else {
	if ( (fp_out = fopen( out_file, "wb") ) == NULL ) {
	    bu_log( "Cannot open %s\n", out_file );
	    perror( argv[0] );
	    return 2;
	}
    }

    /* find requested object */
    db_update_nref(dbip, &rt_uniresource);

    dp = db_lookup( dbip, object, 0 );
    if ( dp == DIR_NULL ) {
	bu_log( "Object %s not found in database!\n", object );
	exit(1);
    }

    /* generate mesh list */
    db_functree( dbip, dp, NULL, mesh_tracker, &rt_uniresource, NULL );
    if ( verbose ) {
	fprintf(stderr, ">> mesh count: %d\n", mesh_count);
    }

    /* writeout header */
    write_header( dbip );

    /* writeout meshes */
    write_mesh_data();

    /* finish */
    dealloc_mesh_list();
    db_close( dbip );
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
