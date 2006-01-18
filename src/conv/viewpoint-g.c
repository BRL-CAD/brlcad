/*                   V I E W P O I N T - G . C
 * BRL-CAD
 *
 * Copyright (c) 1993-2006 United States Government as represented by
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
/** @file viewpoint-g.c
 *	V I E W P O I N T - G
 *
 *  Converter from Viewpoint Datalabs coor/elem format
 *  to BRL-CAD format.  Will assign vertex normals if they
 *  are present in the input files.  Two files are expected
 *  one containing vertex coordinates (and optional normals)
 *  and the second which lists the vertex numbers for each polygonal face.
 *
 *  Author:
 *	John R. Anderson
 *
 *  Source -
 *	The U. S. Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5068  USA
 */

#include "common.h"

/* system headers */
#include <stdio.h>
#include <math.h>
#ifdef HAVE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif
#include <errno.h>
#if defined(HAVE_UNISTD_H)
#  include <unistd.h>
#else
#  if defined(HAVE_SYS_UNISTD_H)
#    include <sys/unistd.h>
#  endif
#endif

/* interface headers */
#include "machine.h"
#include "vmath.h"
#include "nmg.h"
#include "rtgeom.h"
#include "raytrace.h"
#include "wdb.h"

/* local headers */
#include "../librt/debug.h"


#define START_ARRAY_SIZE	64
#define ARRAY_BLOCK_SIZE	64

/* structure for storing coordinates and associated vertex pointer */
struct viewpoint_verts
{
	point_t coord;
	vect_t norm;
	struct vertex *vp;
	short has_norm;
};

#define	LINELEN	256 /* max input line length from elements file */

static char *tok_sep=" ";		/* seperator used in input files */
static char *usage="viewpoint-g [-t tol] -c coord_file_name -e elements_file_name -o output_file_name";

int
main(int argc, char **argv)
{
	register int c;
	FILE *coords,*elems;		/* input file pointers */
	struct rt_wdb *out_fp;		/* output file pointers */
	char *output_file = "viewpoint.g";
	char *base_name;		/* title and top level group name */
	char *coords_name=(char *)NULL;	/* input coordinates file name */
	char *elems_name;		/* input elements file name */
	float x,y,z,nx,ny,nz;		/* vertex and normal coords */
	char *ptr1,*ptr2;
	int name_len;
	struct bn_tol tol;
	int done=0;
	int i;
	int no_of_verts;
	int no_of_faces=0;
	char line[LINELEN];
	struct bu_ptbl vertices;	/* table of vertices for one face */
	struct bu_ptbl faces;		/* table of faces for one element */
	struct bu_ptbl names;		/* table of element names */
	struct model *m;
	struct nmgregion *r;
	struct shell *s;
	struct faceuse *fu;
	struct viewpoint_verts *verts;	/* array of structures holding coordinates and normals */
	struct wmember reg_head;

        /* XXX These need to be improved */
        tol.magic = BN_TOL_MAGIC;
        tol.dist = 0.005;
        tol.dist_sq = tol.dist * tol.dist;
        tol.perp = 1e-6;
        tol.para = 1 - tol.perp;

	coords = NULL;
	elems = NULL;

	if( argc < 2 )
		rt_bomb( usage );

	/* get command line arguments */
	while ((c = getopt(argc, argv, "t:c:e:o:")) != EOF)
	{
		switch( c )
		{
			case 't': /* tolerance */
				tol.dist = atof( optarg );
				tol.dist_sq = tol.dist * tol.dist;
				break;
			case 'c': /* input coordinates file name */
				coords_name = (char *)bu_malloc( strlen( optarg ) + 1 , "Viewpoint-g: base name" );
				strcpy( coords_name , optarg );
				if( (coords = fopen( coords_name , "r" )) == NULL )
				{
					bu_log( "Cannot open %s\n" , coords_name );
					perror( "viewpoint-g" );
					rt_bomb( "Cannot open input file" );
				}
				break;
			case 'e': /* input elements file name */
				elems_name = (char *)bu_malloc( strlen( optarg ) + 1 , "Viewpoint-g: base name" );
				strcpy( elems_name , optarg );
				if( (elems = fopen( elems_name , "r" )) == NULL )
				{
					bu_log( "Cannot open %s\n" , elems_name );
					perror( "viewpoint-g" );
					rt_bomb( "Cannot open input file" );
				}
				break;
			case 'o': /* output file name */
				output_file = optarg;
				break;
			default:
				rt_bomb( usage );
				break;
		}
	}
	if( (out_fp = wdb_fopen( output_file )) == NULL )
	{
		perror( output_file );
		bu_log( "tankill-g: Cannot open %s\n" , output_file );
		rt_bomb( "Cannot open output file\n" );
	}

	/* Must have some input */
	if( coords == NULL || elems == NULL )
		rt_bomb( usage );

	/* build a title for the BRL-CAD database */
	if ( !coords_name ) {
		bu_log("%s:%d no coords_name set\n", __FILE__, __LINE__);
		bu_bomb("croak\n");
	}

	ptr1 = strrchr( coords_name , '/' );
	if( ptr1 == NULL )
		ptr1 = coords_name;
	else
		ptr1++;
	ptr2 = strchr( ptr1 , '.' );

	if( ptr2 == NULL )
		name_len = strlen( ptr1 );
	else
		name_len = ptr2 - ptr1;

	base_name = (char *)bu_malloc( name_len + 1 , "base_name" );
	strncpy( base_name , ptr1 , name_len );

	/* make title record */
	mk_id( out_fp , base_name );

	/* count vertices */
	no_of_verts = 1;
	while( fgets( line , LINELEN , coords ) != NULL )
		no_of_verts++;

	/* allocate memory to store vertex coordinates and normal coordinates and a pointer to
	 * an NMG vertex structure */
	verts = (struct viewpoint_verts *)bu_calloc( no_of_verts , sizeof( struct viewpoint_verts ) , "viewpoint-g: vertex list" );

	/* Now read the vertices again and store them */
	rewind( coords );
	while( fgets( line , LINELEN ,  coords ) != NULL )
	{
		int number_scanned;
		number_scanned = sscanf( line , "%d,%f,%f,%f,%f,%f,%f" , &i , &x , &z , &y , &nx , &ny , &nz );
		if( number_scanned < 4 )
			break;
		if( i >= no_of_verts )
			bu_log( "vertex number too high (%d) only allowed for %d\n" , i , no_of_verts );
		VSET( verts[i].coord , x , y , z );

		if( number_scanned == 7 ) /* we get normals too!!! */
		{
			VSET( verts[i].norm , nx , ny , nz );
			verts[i].has_norm = 1;
		}
	}

	/* Let the user know that something is happening */
	fprintf( stderr , "%d vertices\n" , no_of_verts-1 );

	/* initialize tables */
	bu_ptbl_init( &vertices , 64, " &vertices ");
	bu_ptbl_init( &faces , 64, " &faces ");
	bu_ptbl_init( &names , 64, " &names ");

	while( !done )
	{
		char *name,*curr_name,*ptr;
		int eof=0;

		/* Find an element name that has not already been processed */
		curr_name = NULL;
		done = 1;
		while( fgets( line , LINELEN , elems ) != NULL )
		{
			line[strlen(line)-1] = '\0';
			name = strtok( line , tok_sep );
			if( BU_PTBL_END( &names ) == 0 )
			{
				/* this is the first element processed */
				curr_name = bu_malloc( sizeof( name ) + 1 , "viewpoint-g: component name" );
				strcpy( curr_name , name );

				/* add this name to the table */
				bu_ptbl_ins( &names , (long *)curr_name );
				done = 0;
				break;
			}
			else
			{
				int found=0;

				/* check the list to see if this name is already there */
				for( i=0 ; i<BU_PTBL_END( &names ) ; i++ )
				{
					if( !strcmp( (char *)BU_PTBL_GET( &names , i ) , name ) )
					{
						/* found it, so go back and read the next line */
						found = 1;
						break;
					}
				}
				if( !found )
				{
					/* didn't find name, so this becomes the current name */
					curr_name = bu_malloc( sizeof( name ) + 1 , "viewpoint-g: component name" );
					strcpy( curr_name , name );

					/* add it to the table */
					bu_ptbl_ins( &names , (long *)curr_name );
					done = 0;
					break;
				}
			}
		}

		/* if no current name, then we are done */
		if( curr_name == NULL )
			break;

		/* Hopefully, the user is still around */
		fprintf( stderr , "\tMaking %s\n" , curr_name );

		/* make basic nmg structures */
		m = nmg_mm();
		r = nmg_mrsv( m );
		s = BU_LIST_FIRST( shell , &r->s_hd );

		/* set all vertex pointers to NULL so that different models don't share vertices */
		for( i=0 ; i<no_of_verts ; i++ )
			verts[i].vp = (struct vertex *)NULL;

		/* read elements file and make faces */
		while( !eof )
		{
			/* loop through vertex numbers */
			while( (ptr = strtok( (char *)NULL , tok_sep ) ) != NULL )
			{
				i = atoi( ptr );
				if( i >= no_of_verts )
					bu_log( "vertex number (%d) too high in element, only allowed for %d\n" , i , no_of_verts );

				bu_ptbl_ins( &vertices , (long *)(&verts[i].vp) );
			}

			if( BU_PTBL_END( &vertices ) > 2 )
			{
				/* make face */
				fu = nmg_cmface( s , (struct vertex ***)BU_PTBL_BASEADDR( &vertices ) , BU_PTBL_END( &vertices ) );
				no_of_faces++;

				/* put faceuse in list for the current named object */
				bu_ptbl_ins( &faces , (long *)fu );

				/* restart the vertex list for the next face */
				bu_ptbl_reset( &vertices );
			}
			else
			{
				bu_log( "Skipping degenerate face\n" );
				bu_ptbl_reset( &vertices );
			}

			/* skip elements with the wrong name */
			name = NULL;
			while( name == NULL || strcmp( name , curr_name ) )
			{
				/* check for enf of file */
				if( fgets( line , LINELEN , elems ) == NULL )
				{
					eof = 1;
					break;
				}

				/* get name from input line (first item on line) */
				line[strlen(line)-1] = '\0';
				name = strtok( line ,  tok_sep );
			}

		}

		/* assign geometry */
		for( i=0 ; i<no_of_verts ; i++ )
		{
			if( verts[i].vp )
			{
				NMG_CK_VERTEX( verts[i].vp );
				nmg_vertex_gv( verts[i].vp , verts[i].coord );

				/* check if a vertex normal exists */
				if( verts[i].has_norm )
				{
					struct vertexuse *vu;

					/* assign this normal to all uses of this vertex */
					for( BU_LIST_FOR( vu , vertexuse , &verts[i].vp->vu_hd ) )
					{
						NMG_CK_VERTEXUSE( vu );
						nmg_vertexuse_nv( vu , verts[i].norm );
					}
				}
			}
		}

		(void)nmg_model_vertex_fuse( m , &tol );

		/* calculate plane equations for faces */
		    NMG_CK_SHELL( s );
		    fu = BU_LIST_FIRST( faceuse , &s->fu_hd );
		    while( BU_LIST_NOT_HEAD( fu , &s->fu_hd))
		    {
			struct faceuse *kill_fu=(struct faceuse *)NULL;
		    	struct faceuse *next_fu;

		        NMG_CK_FACEUSE( fu );

		    	next_fu = BU_LIST_NEXT( faceuse , &fu->l );
		        if( fu->orientation == OT_SAME )
		    	{
	    			struct loopuse *lu;
		    		struct edgeuse *eu;
	    			fastf_t area;
	    			plane_t pl;

	    			lu = BU_LIST_FIRST( loopuse , &fu->lu_hd );
		    		NMG_CK_LOOPUSE( lu );
		    		for( BU_LIST_FOR( eu , edgeuse , &lu->down_hd ) )
		    		{
		    			NMG_CK_EDGEUSE( eu );
		    			if( eu->vu_p->v_p == eu->eumate_p->vu_p->v_p )
		    				kill_fu = fu;
		    		}
		    		if( !kill_fu )
		    		{
		    			area = nmg_loop_plane_area( lu , pl );
		    			if( area <= 0.0 )
		    			{

		    				bu_log( "ERROR: Can't get plane for face\n" );

		    				kill_fu = fu;
		    			}
		    		}
		    		if( kill_fu )
		    		{
	    				if( next_fu == kill_fu->fumate_p )
	    					next_fu = BU_LIST_NEXT( faceuse , &next_fu->l );
		    			bu_ptbl_rm( &faces , (long *)kill_fu );
	    				nmg_kfu( kill_fu );
	    			}
		    		else
		    			nmg_face_g( fu , pl );
		    	}
		    	fu = next_fu;
		    }

		if( BU_PTBL_END( &faces ) )
		{
			/* glue faces together */
			nmg_gluefaces( (struct faceuse **)BU_PTBL_BASEADDR( &faces) , BU_PTBL_END( &faces ), &tol );

			nmg_rebound( m , &tol );
			nmg_fix_normals( s , &tol );

			nmg_shell_coplanar_face_merge( s , &tol , 1 );

			nmg_rebound( m , &tol );

			/* write the nmg to the output file */
			mk_bot_from_nmg( out_fp , curr_name  , s );
		}
		else
			bu_log( "Object %s has no faces\n" , curr_name );

		/* kill the current model */
		nmg_km( m );

		/* restart the list of faces for the next object */
		bu_ptbl_reset( &faces );

		/* rewind the elements file for the next object */
		rewind( elems );
	}

	fprintf( stderr , "%d polygons\n" , no_of_faces );

	/* make a top level group with all the objects as members */
	BU_LIST_INIT( &reg_head.l );
	for( i=0 ; i<BU_PTBL_END( &names ) ; i++ )
	{
		if( mk_addmember( (char *)BU_PTBL_GET( &names , i ) , &reg_head.l , NULL, WMOP_UNION ) == WMEMBER_NULL )
		{
			bu_log( "Cannot make top level group\n" );
			exit( 1 );
		}
	}

	fprintf( stderr , "Making top level group (%s)\n" , base_name );
	if( mk_lcomb( out_fp , base_name , &reg_head , 0, (char *)0, (char *)0, (unsigned char *)0, 0 ) )
		bu_log( "viewpoint-g: Error in making top level group" );

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
