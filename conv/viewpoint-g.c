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

struct viewpoint_verts
{
	point_t coord;
	struct vertex *vp;
};

struct components
{
	char *name;
	int no_of_solids;
	struct comp_idents *next;
} *comp_root;

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
	char *coords_name,*elems_name;
	struct rt_tol tol;
	int polysolids=0;
	float x,y,z;
	int i;
	int no_of_verts;
	char tmp_name[80];
	char line[LINELEN];
	struct nmg_ptbl vertices;
	struct nmg_ptbl faces;
	struct model *m;
	struct nmgregion *r;
	struct shell *s;
	struct faceuse *fu;
	struct viewpoint_verts *verts;
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
	while ((c = getopt(argc, argv, "pt:i:o:")) != EOF)
	{
		switch( c )
		{
			case 'p': /* choose polysolid output */
				polysolids = 1;
				break;
			case 't': /* tolerance */
				tol.dist = atof( optarg );
				tol.dist_sq = tol.dist * tol.dist;
				break;
			case 'i': /* input file name */
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

	mk_id( out_fp , "test" );

	/* count vertices */
	no_of_verts = 1;
	while( fscanf( coords , "%d,%f,%f,%f" , &i , &tmp , &tmp , &tmp ) != EOF )
		no_of_verts++;

	if( i+1 != no_of_verts )
		rt_log( "screwy vertex numbers!!!\n" );

	
	verts = (struct viewpoint_verts *)rt_calloc( no_of_verts , sizeof( struct viewpoint_verts ) , "viewpoint-g: vertex list" );
	rewind( coords );
	while( fscanf( coords , "%d,%f,%f,%f" , &i , &x , &z , &y ) != EOF )
	{
		if( i >= no_of_verts )
			rt_log( "vertex number too high (%d) only allowed for %d\n" , i , no_of_verts );
		VSET( verts[i].coord , (-x) , y , z );
		verts[i].vp = (struct vertex *)NULL;
	}

	m = nmg_mm();
	r = nmg_mrsv( m );
	s = RT_LIST_FIRST( shell , &r->s_hd );

	nmg_tbl( &vertices , TBL_INIT , NULL );
	nmg_tbl( &faces , TBL_INIT , NULL );
	/* read elements file and make faces */
	while( fgets( line , LINELEN , elems ) != NULL )
	{
		char *name,*ptr;

		line[strlen(line)-1] = '\0';

		name = strtok( line ,  tok_sep );
		while( (ptr = strtok( (char *)NULL , tok_sep ) ) != NULL )
		{
			i = atoi( ptr );
			if( i >= no_of_verts )
				rt_log( "vertex number too high in element (%d) only allowed for %d\n" , i , no_of_verts );
			nmg_tbl( &vertices , TBL_INS , (long *)(&verts[i].vp) );
		}

		fu = nmg_cmface( s , (struct vertex ***)NMG_TBL_BASEADDR( &vertices ) , NMG_TBL_END( &vertices ) );
		nmg_tbl( &faces , TBL_INS , (long *)fu );
		nmg_tbl( &vertices , TBL_RST , NULL );
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


	nmg_gluefaces( (struct faceuse **)NMG_TBL_BASEADDR( &faces) , NMG_TBL_END( &faces ) );

	nmg_tbl( &faces , TBL_FREE , NULL );
	nmg_tbl( &vertices , TBL_FREE , NULL );

	mk_nmg( out_fp , "test" , m );
}
