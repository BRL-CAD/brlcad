#include <stdio.h>
#include <math.h>
#include <string.h>
#include "machine.h"
#include "db.h"
#include "externs.h"
#include "vmath.h"
#include "nmg.h"
#include "rtgeom.h"
#include "raytrace.h"
#include "wdb.h"
#include "../librt/debug.h"

extern int errno;

#define START_ARRAY_SIZE	64
#define ARRAY_BLOCK_SIZE	64

/* structure for storing coordinates and associated vertex pointer */
struct viewpoint_verts
{
	point_t coord;
	struct vertex *vp;
};

#define	LINELEN	256 /* max input line length from elements file */

static char *tok_sep=" ";
static char *coords_suffix=".coor.txt";
static char *elems_suffix=".elem.txt";
static char *usage="viewpoint-g input_file_base output_file_name";

main( int argc , char *argv[] )
{
	register int c;
	FILE *coords,*elems;
	FILE *out_fp;
	char *base_name,*coords_name,*elems_name;
	struct rt_tol tol;
	float x,y,z;
	int done=0;
	int i;
	int no_of_verts;
	int no_of_faces=0;
	char line[LINELEN];
	struct nmg_ptbl vertices;
	struct nmg_ptbl faces;
	struct nmg_ptbl names;
	struct model *m;
	struct nmgregion *r;
	struct shell *s;
	struct faceuse *fu;
	struct viewpoint_verts *verts;
	struct wmember reg_head,*wmem;
	float tmp;

        /* XXX These need to be improved */
        tol.magic = RT_TOL_MAGIC;
        tol.dist = 0.0005;
        tol.dist_sq = tol.dist * tol.dist;
        tol.perp = 1e-6;
        tol.para = 1 - tol.perp;

	out_fp = stdout;
	coords = NULL;
	elems = NULL;

	if( argc < 2 )
		rt_bomb( usage );

	/* get command line arguments */
	while ((c = getopt(argc, argv, "t:i:o:")) != EOF)
	{
		switch( c )
		{
			case 't': /* tolerance */
				tol.dist = atof( optarg );
				tol.dist_sq = tol.dist * tol.dist;
				break;
			case 'i': /* input file name */
				base_name = (char *)rt_malloc( strlen( optarg ) + 1 , "Viewpoint-g: base name" );
				strcpy( base_name , optarg );
				coords_name = (char *)rt_malloc( strlen( optarg ) + strlen( coords_suffix ) + 1 , "viewpoint-g: coord file name" );
				strcpy( coords_name , optarg );
				strcat( coords_name , coords_suffix );
				if( (coords = fopen( coords_name , "r" )) == NULL )
				{
					rt_log( "Cannot open %s\n" , coords_name );
					perror( "viewpoint-g" );
					rt_bomb( "Cannot open input file" );
				}
				elems_name = (char *)rt_malloc( strlen( optarg ) + strlen( elems_suffix ) + 1 , "viewpoint-g: coord file name" );
				strcpy( elems_name , optarg );
				strcat( elems_name , elems_suffix );
				if( (elems = fopen( elems_name , "r" )) == NULL )
				{
					rt_log( "Cannot open %s\n" , elems_name );
					perror( "viewpoint-g" );
					rt_bomb( "Cannot open input file" );
				}
				break;
			case 'o': /* output file name */
				if( (out_fp = fopen( optarg , "w" )) == NULL )
				{
					rt_log( "Cannot open %s\n" , optarg );
					perror( "tankill-g" );
					rt_bomb( "Cannot open output file\n" );
				}
				break;
			default:
				rt_bomb( usage );
				break;
		}
	}

	if( coords == NULL || elems == NULL )
		rt_bomb( usage );

	mk_id( out_fp , base_name );

	/* count vertices */
	no_of_verts = 1;
	while( fscanf( coords , "%d,%f,%f,%f" , &i , &tmp , &tmp , &tmp ) != EOF )
		no_of_verts++;

	verts = (struct viewpoint_verts *)rt_calloc( no_of_verts , sizeof( struct viewpoint_verts ) , "viewpoint-g: vertex list" );
	rewind( coords );
	while( fscanf( coords , "%d,%f,%f,%f" , &i , &x , &z , &y ) != EOF )
	{
		if( i >= no_of_verts )
			rt_log( "vertex number too high (%d) only allowed for %d\n" , i , no_of_verts );
		VSET( verts[i].coord , (-x) , y , z );
	}

	fprintf( stderr , "%d vertices\n" , no_of_verts-1 );

	nmg_tbl( &vertices , TBL_INIT , NULL );
	nmg_tbl( &faces , TBL_INIT , NULL );
	nmg_tbl( &names , TBL_INIT , NULL );

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
			if( NMG_TBL_END( &names ) == 0 )
			{
				/* this is the first element processed */
				curr_name = rt_malloc( sizeof( name ) + 1 , "viewpoint-g: component name" );
				strcpy( curr_name , name );
				nmg_tbl( &names , TBL_INS , (long *)curr_name );
				done = 0;
				break;
			}
			else
			{
				int found=0;

				/* check the list to see if this name is already there */
				for( i=0 ; i<NMG_TBL_END( &names ) ; i++ )
				{
					if( !strcmp( (char *)NMG_TBL_GET( &names , i ) , name ) )
					{
						/* found it, so go back and read the next line */
						found = 1;
						break;
					}
				}
				if( !found )
				{
					/* didn't find name, so this becomes the current name */
					curr_name = rt_malloc( sizeof( name ) + 1 , "viewpoint-g: component name" );
					strcpy( curr_name , name );
					nmg_tbl( &names , TBL_INS , (long *)curr_name );
					done = 0;
					break;
				}
			}
		}

		/* if no current name, then we are done */
		if( curr_name == NULL )
			break;

		fprintf( stderr , "\tMaking %s\n" , curr_name );

		/* make basic nmg structures */
		m = nmg_mm();
		r = nmg_mrsv( m );
		s = RT_LIST_FIRST( shell , &r->s_hd );

		/* set all vertex pointers to NULL so that different models don't share vertices */
		for( i=0 ; i<no_of_verts ; i++ )
			verts[i].vp = (struct vertex *)NULL;

		/* read elements file and make faces */
		while( !eof )
		{
			/* loop through vertx numbers */
			while( (ptr = strtok( (char *)NULL , tok_sep ) ) != NULL )
			{
				i = atoi( ptr );
				if( i >= no_of_verts )
					rt_log( "vertex number too high in element (%d) only allowed for %d\n" , i , no_of_verts );

				/* put vertex pointer in list for this face */
				nmg_tbl( &vertices , TBL_INS , (long *)(&verts[i].vp) );
			}

			/* make face */
			fu = nmg_cmface( s , (struct vertex ***)NMG_TBL_BASEADDR( &vertices ) , NMG_TBL_END( &vertices ) );
			no_of_faces++;

			/* put faceuse in list for the current named object */
			nmg_tbl( &faces , TBL_INS , (long *)fu );

			/* restart the vertex list for the next face */
			nmg_tbl( &vertices , TBL_RST , NULL );

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
				nmg_vertex_gv( verts[i].vp , verts[i].coord );
		}

		/* calculate plane equations for faces */
		for (RT_LIST_FOR(s, shell, &r->s_hd))
		{
		    NMG_CK_SHELL( s );
		    for (RT_LIST_FOR(fu, faceuse, &s->fu_hd))
		    {
		        NMG_CK_FACEUSE( fu );
		        if( fu->orientation == OT_SAME )
		                if( nmg_fu_planeeqn( fu , &tol ) )
		                        rt_log( "Failed to calculate plane eqn\n" );
		    }
		}

		/* glue faces together */
		nmg_gluefaces( (struct faceuse **)NMG_TBL_BASEADDR( &faces) , NMG_TBL_END( &faces ) );

		/* restart the list of faces for the next object */
		nmg_tbl( &faces , TBL_RST , NULL );

		/* write the nmg to the output file */
		mk_nmg( out_fp , curr_name  , m );

		/* kill the current model */
		nmg_km( m );

		/* rewind the elements file for the next object */
		rewind( elems );
	}

	fprintf( stderr , "%d polygons\n" , no_of_faces );

	/* make a top level group with all the objects as members */
	RT_LIST_INIT( &reg_head.l );
	for( i=0 ; i<NMG_TBL_END( &names ) ; i++ )
		if( (wmem = mk_addmember( (char *)NMG_TBL_GET( &names , i ) , &reg_head , WMOP_UNION ) ) == WMEMBER_NULL )
		{
			rt_log( "Cannot make top level group\n" );
			exit( 1 );
		}

	fprintf( stderr , "Making top level group (%s)\n" , base_name );
	if( mk_lcomb( out_fp , base_name , &reg_head , 0, (char *)0, (char *)0, (char *)0, 0 ) )
		rt_log( "viewpoint-g: Error in making top level group" );

}
