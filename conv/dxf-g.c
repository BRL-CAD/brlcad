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

#define	LINELEN	256 /* max input line length from elements file */

static char *usage="dxf-g [-p] [-t tolerance] [-i input_file] [-o output_file_name]";

main( int argc , char *argv[] )
{
	register int c;
	FILE *dxf;
	FILE *out_fp;
	char *base_name,*dxf_name;
	char *ptr1,*ptr2;
	char curr_name[LINELEN];
	int name_len;
	struct rt_tol tol;
	int done=0;
	int polysolids=0;
	int i,j,group_code;
	char line[LINELEN];
	struct nmg_ptbl vertices;
	struct nmg_ptbl faces;
	struct model *m;
	struct nmgregion *r;
	struct shell *s;
	struct faceuse *fu;

        /* XXX These need to be improved */
        tol.magic = RT_TOL_MAGIC;
        tol.dist = 0.0005;
        tol.dist_sq = tol.dist * tol.dist;
        tol.perp = 1e-6;
        tol.para = 1 - tol.perp;

	out_fp = stdout;
	dxf = stdin;

	if( argc < 2 )
		rt_bomb( usage );

	/* get command line arguments */
	while ((c = getopt(argc, argv, "pt:i:o:")) != EOF)
	{
		switch( c )
		{
			case 't':	/* tolerance */
				tol.dist = atof( optarg );
				tol.dist_sq = tol.dist * tol.dist;
				break;
			case 'i':	/* input DXF file name */
				dxf_name = (char *)rt_malloc( strlen( optarg ) + 1 , "Viewpoint-g: base name" );
				strcpy( dxf_name , optarg );
				if( (dxf = fopen( dxf_name , "r" )) == NULL )
				{
					rt_log( "Cannot open %s\n" , dxf_name );
					perror( "viewpoint-g" );
					rt_bomb( "Cannot open input file" );
				}
				break;
			case 'o':	/* output file name */
				if( (out_fp = fopen( optarg , "w" )) == NULL )
				{
					rt_log( "Cannot open %s\n" , optarg );
					perror( "tankill-g" );
					rt_bomb( "Cannot open output file\n" );
				}
				break;
			case 'p':	/* produce polysolids as output instead of NMG's */
				polysolids = 1;
				break;
			default:
				rt_bomb( usage );
				break;
		}
	}

	if( dxf == NULL )
		rt_bomb( usage );

	ptr1 = strrchr( dxf_name , '/' );
	if( ptr1 == NULL )
		ptr1 = dxf_name;
	else
		ptr1++;
	ptr2 = strchr( ptr1 , '.' );

	if( ptr2 == NULL )
		name_len = strlen( ptr1 );
	else
		name_len = ptr2 - ptr1;

	base_name = (char *)rt_malloc( name_len + 1 , "base_name" );
	strncpy( base_name , ptr1 , name_len );
	
	mk_id( out_fp , base_name );

	/* Find the ENTITIES SECTION */
	while( fgets( line , LINELEN , dxf ) != NULL )
	{
		sscanf( line , "%d" , &group_code );
		if( group_code == 0 )
		{
			/* read label from next line */
			if( fgets( line , LINELEN , dxf ) == NULL )
				rt_bomb( "Unexpected EOF in input file\n" );
			if( !strncmp( line , "SECTION" , 7 ) )
			{
				if( fgets( line , LINELEN , dxf ) == NULL )
					rt_bomb( "Unexpected EOF in input file\n" );
				sscanf( line , "%d" , &group_code );
				if( group_code == 2 )
				{
					if( fgets( line , LINELEN , dxf ) == NULL )
						rt_bomb( "Unexpected EOF in input file\n" );
					if( !strncmp( line , "ENTITIES" , 8 ) )
					{
						if( fgets( line , LINELEN , dxf ) == NULL )
							rt_bomb( "Unexpected EOF in input file\n" );
						sscanf( line , "%d" , &group_code );
						if( group_code != 0 )
							rt_bomb( "Expected group code 0 at start of ENTITIES\n" );
						break;
					}
				}
			}
		}
	}

	/* make basic nmg structures */
	m = nmg_mm();
	r = nmg_mrsv( m );
	s = RT_LIST_FIRST( shell , &r->s_hd );

	nmg_tbl( &vertices , TBL_INIT , NULL );
	nmg_tbl( &faces , TBL_INIT , NULL );

	/* Read the ENTITIES section */
	while( !done )
	{
		struct vertex *vp[4];
		point_t pt[4];
		int no_of_pts;

		if( fgets( line , LINELEN , dxf ) == NULL )
			rt_bomb( "Unexpected EOF in input file\n" );
		while( strncmp( line , "3DFACE" , 6 ) )
		{
			if( !strncmp( line , "ENDSEC" , 6 ) )
			{
				rt_log( "Found end of ENTITIES section\n" );
				done = 1;
				break;
			}
			line[ strlen(line)-1 ] = '\0';
			rt_log( "Unknown entity type (%s), skipping\n" , line );
			group_code = 5;
			while( group_code != 0 )
			{
				if( fgets( line , LINELEN , dxf ) == NULL )
					rt_bomb( "Unexpected EOF in input file\n" );
				sscanf( line , "%d" , &group_code );
				if( fgets( line , LINELEN , dxf ) == NULL )
					rt_bomb( "Unexpected EOF in input file\n" );
			}
		}

		if( done )
			break;

		no_of_pts = (-1);
		group_code = 1;
		while( group_code )
		{
			if( fgets( line , LINELEN , dxf ) == NULL )
				rt_bomb( "Unexpected EOF in input file\n" );
			sscanf( line , "%d" , &group_code );
			switch( group_code )
			{
				case 8:
					if( fgets( line , LINELEN , dxf ) == NULL )
						rt_bomb( "Unexpected EOF in input file\n" );
					line[ strlen( line ) - 1 ] = '\0';
					if( strcmp( line , curr_name ) )
					{
						strcpy( curr_name , line );
						rt_log( "Making %s\n" , curr_name );
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
					i = group_code%10;
					j = group_code/10 - 1;
					if( i > no_of_pts )
						no_of_pts = i;
					if( fgets( line , LINELEN , dxf ) == NULL )
						rt_bomb( "Unexpected EOF in input file\n" );
					pt[i][j] = atof( line );
					break;
				case 0:
					no_of_pts++;
					for( i=0 ; i<no_of_pts ; i++ )
					{
						for( j=i+1 ; j<no_of_pts ; j++ )
						{
							if( VAPPROXEQUAL( pt[i] , pt[j] , tol.dist ) )
							{
								int k;
/* rt_log( "Combining points %d and %d\n" , i , j ); */
								no_of_pts--;
								for( k=j ; k<no_of_pts ; k++ )
								{
									VMOVE( pt[k] , pt[k+1] );
								}
							}
						}
					}
					if( no_of_pts != 3 && no_of_pts != 4 )
					{
						rt_log( "Skipping face with %d vertices\n" , no_of_pts );
						break;
					}
/* rt_log( "FACE:\n" ); */
					for( i=0 ; i<no_of_pts ; i++ )
					{
/* rt_log( "\t( %f %f %f )\n" , V3ARGS( pt[i] ) ); */
						vp[i] = (struct vertex *)NULL;
						for( j=0 ; j<NMG_TBL_END( &vertices ) ; j++ )
						{
							struct vertex *v;

							v = (struct vertex *)NMG_TBL_GET( &vertices , j );
							if( VAPPROXEQUAL( pt[i] , v->vg_p->coord , tol.dist ) )
							{
								vp[i] = v;
								break;
							}
						}
					}
					fu = nmg_cface( s , vp , no_of_pts );
					nmg_tbl( &faces , TBL_INS , (long *)fu );
					for( i=0 ; i<no_of_pts ; i++ )
					{
						if( vp[i]->vg_p == NULL )
						{
							nmg_vertex_gv( vp[i] , pt[i] );
							nmg_tbl( &vertices , TBL_INS , (long *)vp[i] );
						}
					}
			                if( nmg_fu_planeeqn( fu , &tol ) )
			                        rt_log( "Failed to calculate plane eqn\n" );
					for( i=0 ; i<no_of_pts ; i++ )
					{
						for( j=i+1 ; j<no_of_pts ; j++ )
						{
							if( vp[i] == vp[j] )
								rt_log( "vertex %d is same as vertex %d\n" , i , j );
						}
					}
					break;
			}
		}
	}

	/* glue faces together */
	rt_log( "Glueing %d faces together...\n" , NMG_TBL_END( &faces ) );
	nmg_gluefaces( (struct faceuse **)NMG_TBL_BASEADDR( &faces) , NMG_TBL_END( &faces ) );

	nmg_fix_normals( s );

	nmg_vshell( &r->s_hd , r );

	/* write the nmg to the output file */
	if( polysolids )
	{
		rt_log( "Writing polysolid to output...\n" );
		write_shell_as_polysolid( out_fp , base_name , s );
	}
	else
	{
		rt_log( "Writing NMG to output...\n" );
		mk_nmg( out_fp , base_name  , m );
	}

	fprintf( stderr , "%d polygons\n" , NMG_TBL_END( &faces ) );
}
