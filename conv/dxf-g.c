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

#define	LINELEN		256	/* max input line length from elements file */
#define	LAYERLEN	32	/* max length for a layer name */

static char *usage="dxf-g [-d] [-l] [-n] [-p] [-t tolerance] [-i input_file] [-o output_file_name]";
static char *all="all";

main( int argc , char *argv[] )
{
	register int c;			/* Command line option letter */
	FILE *dxf;			/* Input DXF file */
	FILE *out_fp;			/* Output BRLCAD file */
	char *dxf_name;			/* Name of input file */
	char *base_name;		/* Base name from dxf_name (no /`s) */
	char *curr_name;		/* Current layer name */
	int name_len;			/* Length of name */
	struct rt_tol tol;		/* Tolerance */
	int done=0;			/* Flag for loop control */
	int polysolids=0;		/* Flag for polysolid output */
	int debug=0;			/* Debug flag */
	int do_layers=1;		/* Flag for grouping according to layer name */
	int no_of_layers;		/* number of layers to process */
	int curr_layer;			/* current layer number */
	int do_normals=0;
	int obj_count=0;		/* count of objects written to BRLCAD DB file */
	char line[LINELEN];		/* Buffer for line from input file */
	struct nmg_ptbl vertices;	/* Table of vertices */
	struct nmg_ptbl faces;		/* Table of faceuses */
	struct nmg_ptbl	layers;		/* Table of layer names */
	struct model *m;
	struct nmgregion *r;
	struct shell *s;
	struct faceuse *fu;
	char *ptr1,*ptr2;
	int i,j,group_code;

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
	while ((c = getopt(argc, argv, "dlpt:i:o:")) != EOF)
	{
		switch( c )
		{
			case 'n':	/* Try to fix normals */
				do_normals = 1;
				break;
			case 'l':	/* do not use layers to form groups */
				do_layers = 0;
				break;
			case 'd':	/* debug */
				debug = 1;
				break;
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

	curr_name = NULL;

	if( do_layers )
	{
		/* Make list of layer names */
		nmg_tbl( &layers , TBL_INIT , NULL );
		while( fgets( line , LINELEN , dxf ) != NULL )
		{
			group_code = atoi( line );
			if( fgets( line , LINELEN , dxf ) == NULL )
				rt_bomb( "Unexpected EOF in input file\n" );
			if( group_code == 8 )	/* layer name */
			{
				char *name;
				int found=0;

				name_len = strlen( line );
				line[--name_len] = '\0';
				for( i=0 ; i<NMG_TBL_END( &layers ) ; i++ )
				{
					if( !strcmp( line , (char *)NMG_TBL_GET( &layers , i ) ) )
					{
						found = 1;
						break;
					}
				}
				if( !found )
				{
					name = (char *)rt_malloc( name_len , "dxf-g: layer name" );
					strcpy( name , line );
					nmg_tbl( &layers , TBL_INS , (long *)name );
				}
			}
		}
		no_of_layers = NMG_TBL_END( &layers );
	}
	else
		no_of_layers = 1;

	if( debug && do_layers )
	{
		rt_log( "%d layers: \n" , NMG_TBL_END( &layers ) );
		for( i=0 ; i<NMG_TBL_END( &layers ) ; i++ )
			rt_log( "\t%s\n" , (char *)NMG_TBL_GET( &layers , i ) );
	}
	
	mk_id( out_fp , base_name );

	for( curr_layer = 0; curr_layer < no_of_layers ; curr_layer++ )
	{
		if( do_layers )
			curr_name = (char *)NMG_TBL_GET( &layers , curr_layer );
		else
			curr_name = (char *)NULL;

		if( debug )
		{
			if( do_layers )
				rt_log( "Making layer %d of %d: %s\n", curr_layer+1 , NMG_TBL_END( &layers ),
					curr_name );
			else
				rt_log( "Making one layer\n" );
		}

		/* Find the ENTITIES SECTION */
		rewind( dxf );
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
					else	/* skip next line */
					{
						if( fgets( line , LINELEN , dxf ) == NULL )
							rt_bomb( "Unexpected EOF in input file\n" );
					}
				}
			}
			else	/* skip next line */
			{
				if( fgets( line , LINELEN , dxf ) == NULL )
					rt_bomb( "Unexpected EOF in input file\n" );
			}
		}

		/* make basic nmg structures */
		m = nmg_mm();
		r = nmg_mrsv( m );
		s = RT_LIST_FIRST( shell , &r->s_hd );

		nmg_tbl( &vertices , TBL_INIT , NULL );
		nmg_tbl( &faces , TBL_INIT , NULL );

		/* Read the ENTITIES section */
		done = 0;
		while( !done )
		{
			struct vertex *vp[4];
			point_t pt[4];
			int no_of_pts;
			int skip_face;

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
				if( debug )
					rt_log( "Unknown entity type (%s), skipping\n" , line );
				group_code = 1;
				while( group_code != 0 )
				{
					if( fgets( line , LINELEN , dxf ) == NULL )
						rt_bomb( "Unexpected EOF in input file\n" );
					sscanf( line , "%d" , &group_code );
					if( fgets( line , LINELEN , dxf ) == NULL )
						rt_bomb( "Unexpected EOF in input file\n" );
				}
			}
			skip_face = 0;

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
					default:
						if( fgets( line , LINELEN , dxf ) == NULL )
							rt_bomb( "Unexpected EOF in input file\n" );
						break;
					case 8:
						if( fgets( line , LINELEN , dxf ) == NULL )
							rt_bomb( "Unexpected EOF in input file\n" );
						line[ strlen( line ) - 1 ] = '\0';
						if( curr_name != NULL && strcmp( line , curr_name ) )
							skip_face = 1;
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
						if( fgets( line , LINELEN , dxf ) == NULL )
							rt_bomb( "Unexpected EOF in input file\n" );
						if( !skip_face )
						{
							i = group_code%10;
							j = group_code/10 - 1;
							if( i > no_of_pts )
								no_of_pts = i;
							pt[i][j] = atof( line );
						}
						break;
					case 0:
						if( skip_face )
							break;
						no_of_pts++;
						if( debug )
						{
							rt_log( "Original FACE:\n" );
							for( i=0 ; i<no_of_pts ; i++ )
								rt_log( "\t( %f %f %f )\n" , V3ARGS( pt[i] ) );
						}
						for( i=0 ; i<no_of_pts ; i++ )
						{
							for( j=i+1 ; j<no_of_pts ; j++ )
							{
								if( VAPPROXEQUAL( pt[i] , pt[j] , tol.dist ) )
								{
									int k;

									if( debug )
										rt_log( "Combining points %d and %d\n" , i , j );
									no_of_pts--;
									for( k=j ; k<no_of_pts ; k++ )
									{
										VMOVE( pt[k] , pt[k+1] );
									}
									j--;
								}
							}
						}
						if( no_of_pts != 3 && no_of_pts != 4 )
						{
							if( debug )
								rt_log( "Skipping face with %d vertices\n" , no_of_pts );
							break;
						}
						if( no_of_pts == 3 && rt_3pts_collinear( pt[0],pt[1],pt[2],&tol ) )
						{
							if( debug )
								rt_log( "Skipping triangular face with collinear vertices\n" );
							break;
						}
						if( debug )
							rt_log( "final FACE:\n" );
						for( i=0 ; i<no_of_pts ; i++ )
						{
							if( debug )
								rt_log( "\t( %f %f %f )\n" , V3ARGS( pt[i] ) );
							vp[i] = (struct vertex *)NULL;
							for( j=0 ; j<NMG_TBL_END( &vertices ) ; j++ )
							{
								struct vertex *v;

								v = (struct vertex *)NMG_TBL_GET( &vertices , j );
								if( rt_pt3_pt3_equal( pt[i] , v->vg_p->coord , &tol) )
								{
									vp[i] = v;
									break;
								}
							}
						}
						fu = nmg_cface( s , vp , no_of_pts );
						for( i=0 ; i<no_of_pts ; i++ )
						{
							if( vp[i]->vg_p == NULL )
							{
								nmg_vertex_gv( vp[i] , pt[i] );
								nmg_tbl( &vertices , TBL_INS , (long *)vp[i] );
							}
						}
				                if( nmg_fu_planeeqn( fu , &tol ) )
						{
				                        rt_log( "Failed to calculate plane eqn\n" );
							rt_log( "FACE:\n" );
							for( i=0 ; i<no_of_pts ; i++ )
								rt_log( "( %f %f %f )\n" , V3ARGS( pt[i] ) );
						}
						else
							nmg_tbl( &faces , TBL_INS , (long *)fu );
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

		if( debug )
			rt_log( "%d unique vertices\n" , NMG_TBL_END( &vertices ) );

		/* glue faces together */
		if( debug )
			rt_log( "Glueing %d faces together...\n" , NMG_TBL_END( &faces ) );
		nmg_gluefaces( (struct faceuse **)NMG_TBL_BASEADDR( &faces) , NMG_TBL_END( &faces ) );

		if( do_normals )
			nmg_fix_normals( s );

		/* write the nmg to the output file */
		obj_count++;
		if( polysolids )
		{
			if( debug )
				rt_log( "Writing polysolid to output...\n" );
			write_shell_as_polysolid( out_fp , curr_name , s );
		}
		else
		{
			if( debug )
				rt_log( "Writing NMG to output...\n" );
			mk_nmg( out_fp , curr_name  , m );
		}

		rt_log( "%d polygons\n" , NMG_TBL_END( &faces ) );
		nmg_km( m );
		nmg_tbl( &faces , TBL_RST , NULL );
		nmg_tbl( &vertices , TBL_RST , NULL );
	}
	for( i=0 ; i<NMG_TBL_END( &layers ) ; i++ )
		rt_free( (char *)NMG_TBL_GET( &layers , i ) , "dxf-g: layer names" );
	nmg_tbl( &layers , TBL_RST , NULL );

	rt_log( "%d objects written to output file\n" , obj_count );
}
