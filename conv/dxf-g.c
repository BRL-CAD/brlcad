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
#define SKIP_LINE_SEGS	1	/* flag to inhibit creation of wire edges */

struct dxf_verts
{
	point_t pt;
	struct vertex *vp;
};

static char *usage="dxf-g [-d debug_level] [-l] [-n] [-p] [-t tolerance] [-i input_file] [-o output_file_name]\n\
\tdebug_level is 1 or 2\n\
\t-l -> do not process DXF file by layers\n\
\t-n -> try to fix surface normal directions\n\
\t-p -> build polysolids instead of NMG's\n\
\t\tIf an input file is not specified, the -l option must be used\n";

static char *all="all";
static FILE *dxf;			/* Input DXF file */
static FILE *out_fp;			/* Output BRLCAD file */
static char *std_in_name="stdin";	/* Name for standard input */
static struct rt_tol tol;		/* Tolerance */
static char line[LINELEN];		/* Buffer for line from input file */
static char *curr_name;			/* Current layer name */
static int debug=0;			/* Debug flag */
static struct nmg_ptbl vertices;	/* Table of vertices */
static struct nmg_ptbl faces;		/* Table of faceuses */
static struct nmg_ptbl layers;		/* Table of layer names */
static struct model *m;
static struct nmgregion *r;
static struct shell *s;

int
Do_vertex( pt , flags )
point_t pt;
int *flags;
{
	int done=0;
	int group_code;

	while( !done )
	{
		if( fgets( line , LINELEN , dxf ) == NULL )
			rt_bomb( "Unexpected EOF in input file\n" );
		group_code = atoi( line );
		switch( group_code )
		{
			default:
				if( fgets( line , LINELEN , dxf ) == NULL )
					rt_bomb( "Unexpected EOF in input file\n" );
				break;
			case 70:	/* vertex flags */
				if( fgets( line , LINELEN , dxf ) == NULL )
					rt_bomb( "Unexpected EOF in input file\n" );
				(*flags) = atoi( line );
				break;
			case 10:
			case 20:
			case 30:	/* Coordinates */
				if( fgets( line , LINELEN , dxf ) == NULL )
					rt_bomb( "Unexpected EOF in input file\n" );
				pt[ group_code/10 - 1 ] = atof( line );
				break;
			case 0:
				done = 1;
		}
	}
	return( group_code );
}

int
Do_polyline()
{
	int done=0;
	int group_code;
	int flags=0;
	int m_count=0,n_count=0;
	int mesh_size;
	int vert_count=0;
	int i,j;
	struct dxf_verts *mesh;

	mesh = (struct dxf_verts *)NULL;
	while( !done )
	{
		if( fgets( line , LINELEN , dxf ) == NULL )
			rt_bomb( "Unexpected EOF in input file\n" );
		group_code = atoi( line );
		switch( group_code )
		{
			default:
				if( fgets( line , LINELEN , dxf ) == NULL )
					rt_bomb( "Unexpected EOF in input file\n" );
				break;
			case 8:		/* Layer name */
				if( fgets( line , LINELEN , dxf ) == NULL )
					rt_bomb( "Unexpected EOF in input file\n" );
				line[ strlen( line ) - 1 ] = '\0';
				if( curr_name != NULL && strcmp( curr_name , line ) )
				{
					/* skip this entity */
					while( !done )
					{
						if( fgets( line , LINELEN , dxf ) == NULL )
							rt_bomb( "Unexpected EOF in input file\n" );
						group_code = atoi( line );
						if( fgets( line , LINELEN , dxf ) == NULL )
							rt_bomb( "Unexpected EOF in input file\n" );
						if( group_code == 0 && !strncmp( line , "SEQEND" , 6 ) )
							done = 1;
					}
				}
				else if( debug )
					rt_log( "Polyline:\n" );
				break;
			case 66:	/* vertices follow flag */
				if( fgets( line , LINELEN , dxf ) == NULL )
					rt_bomb( "Unexpected EOF in input file\n" );
				if( atoi( line ) != 1 )
					rt_bomb( "dxf-g: POLYLINE with 'vertices follow' flag not one\n" );
				break;
			case 70:	/* flags */
				if( fgets( line , LINELEN , dxf ) == NULL )
					rt_bomb( "Unexpected EOF in input file\n" );
				flags = atoi( line );
				break;
			case 71:	/* Mesh count 'M' */
				if( fgets( line , LINELEN , dxf ) == NULL )
					rt_bomb( "Unexpected EOF in input file\n" );
				m_count = atoi( line );
				break;
			case 72:	/* Mesh count 'N' */
				if( fgets( line , LINELEN , dxf ) == NULL )
					rt_bomb( "Unexpected EOF in input file\n" );
				n_count = atoi( line );
				break;
			case 0:		/* Start of something (hopefully vertex list) */
				if( fgets( line , LINELEN , dxf ) == NULL )
					rt_bomb( "Unexpected EOF in input file\n" );
				mesh_size = m_count * n_count;
				if( mesh_size )
				{
					struct faceuse *fu;

					/* read and store a mesh */
					mesh = (struct dxf_verts *)rt_calloc( mesh_size , sizeof( struct dxf_verts ) , "dxf-g: mesh" );
					while( !strncmp( line , "VERTEX" , 6 ) )
					{
						int pt_flags=0;

						group_code = Do_vertex( mesh[vert_count].pt , &pt_flags );
						mesh[vert_count].vp = (struct vertex *)NULL;
						if( ++vert_count > mesh_size )
							rt_bomb( "dxf-g: Too many vertices for mesh size\n" );
						if( fgets( line , LINELEN , dxf ) == NULL )
							rt_bomb( "Unexpected EOF in input file\n" );
					}
					/* Look for vertices already existing */
					for( i=0 ; i<vert_count ; i++ )
					{
						for( j=0 ; j<NMG_TBL_END( &vertices ) ; j++ )
						{
							struct vertex *v;

							v = (struct vertex *)NMG_TBL_GET( &vertices , j );
							if( rt_pt3_pt3_equal( mesh[i].pt , v->vg_p->coord , &tol) )
							{
								mesh[i].vp = v;
								if( debug == 2 )
									rt_log( "\tReusing existing vertex at ( %f %f %f )\n" , V3ARGS( mesh[i].vp->vg_p->coord ) );
								break;
							}
						}
					}


					/* make faces */
					for( i=1 ; i<m_count ; i++ )
					{
						for( j=0 ; j<n_count-1 ; j++ )
						{
							struct vertex **vp[4];
							point_t pt[4];
							int nverts;
							int k,l,m;

							VMOVE( pt[0] , mesh[i*n_count + j].pt);
							VMOVE( pt[1] , mesh[i*n_count + j + 1].pt);
							VMOVE( pt[2] , mesh[(i-1)*n_count + j + 1].pt);
							VMOVE( pt[3] , mesh[(i-1)*n_count + j].pt);
							vp[0] = &mesh[i*n_count + j].vp;
							vp[1] = &mesh[i*n_count + j + 1].vp;
							vp[2] = &mesh[(i-1)*n_count + j + 1].vp;
							vp[3] = &mesh[(i-1)*n_count + j].vp;
							nverts = 4;
							if( debug == 2 )
							{
								rt_log( "Making face:\n" ) ;
								rt_log( "\t( %f %f %f )\n" , V3ARGS( pt[0] ) );
								rt_log( "\t( %f %f %f )\n" , V3ARGS( pt[1] ) );
								rt_log( "\t( %f %f %f )\n" , V3ARGS( pt[2] ) );
								rt_log( "\t( %f %f %f )\n" , V3ARGS( pt[3] ) );
							}

							/* check for zero length edges */
							for( k=0 ; k<nverts-1 ; k++ )
							{
								if( rt_pt3_pt3_equal( pt[k] , pt[k+1] , &tol ) )
								{
									for( m=k+1 ; m<nverts-1 ; m++ )
									{
										VMOVE( pt[m] , pt[m+1] );
										vp[m] = vp[m+1];
									}
									nverts--;
									k--;
								}
							}

							if( debug == 2 )
							{
								if( nverts > 2 )
								{
									rt_log( "Making face:\n" ) ;
									for( k=0 ; k<nverts ; k++ )
										rt_log( "\t( %f %f %f )\n" , V3ARGS( pt[k] ) );
								}
								else
									rt_log( "Skip this face\n" );
							}
							if( nverts > 2 )
							{
								fu = nmg_cmface( s , vp , nverts );
								nmg_tbl( &faces , TBL_INS , (long *)fu );
							}
						}
					}
					/* assign geometry */
					for( i=0 ; i<mesh_size ; i++ )
					{
						if( mesh[i].vp )
						{
							if( !mesh[i].vp->vg_p )
							{
								nmg_vertex_gv( mesh[i].vp , mesh[i].pt );
								nmg_tbl( &vertices , TBL_INS , (long *)mesh[i].vp );
							}
						}
					}
				}
				else if( !SKIP_LINE_SEGS )
				{
					/* just a series of line segments */
					point_t pt1,pt2;
					struct vertex *v1,*v2;
					int first=1;

					if( debug )
						rt_log( "\tjust a line segment\n" );

					v1 = (struct vertex *)NULL;
					v2 = (struct vertex *)NULL;
					while( !strncmp( line , "VERTEX" , 6 ) )
					{
						int pt_flags=0;

						group_code = Do_vertex( pt2 , &pt_flags );
						if( debug == 2 )
							rt_log( "\t( %f %f %f )\n" , V3ARGS( pt2 ) );

						if( first )
						{
							VMOVE( pt1 , pt2 );
							first = 0;
						}
						else
						{
							struct edgeuse *eu;

							eu = nmg_me( v1 , v2 , s );
							nmg_vertex_gv( eu->eumate_p->vu_p->v_p , pt2 );
							if( !eu->vu_p->v_p->vg_p )
								nmg_vertex_gv( eu->vu_p->v_p , pt1 );
							VMOVE( pt1 , pt2 );
							v1 = v2;
							v2 = (struct vertex *)NULL;
						}
						if( fgets( line , LINELEN , dxf ) == NULL )
							rt_bomb( "Unexpected EOF in input file\n" );
					}
				}
				if( !strncmp( line , "SEQEND" , 6 ) )
					done = 1;
				break;
		}
	}
	if( debug )
	{
		int i;

		if( mesh_size )
		{
			rt_log( "POLYLINE: %d by %d mesh\n" , m_count , n_count );
			for( i=0 ; i<vert_count ; i++ )
				rt_log( "\t( %f %f %f )\n" , V3ARGS( mesh[i].pt ) );
		}
	}
	if( mesh != NULL )
		rt_free( (char *)mesh , "dxf-g: mesh" );

	return( group_code );
}

int
Do_3dface()
{
	struct faceuse *fu;
	struct vertex *vp[4];
	point_t pt[4];
	int no_of_pts;
	int skip_face;
	int i,j,group_code;

	skip_face = 0;
	no_of_pts = (-1);
	group_code = 1;
	while( group_code )
	{
		if( fgets( line , LINELEN , dxf ) == NULL )
			rt_bomb( "Unexpected EOF in input file\n" );
		group_code = atoi( line );
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
				else if( debug )
					rt_log( "3DFACE\n" );
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
				if( debug == 2 )
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

							if( debug == 2 )
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
					if( debug == 2 )
						rt_log( "Skipping face with %d vertices\n" , no_of_pts );
					break;
				}
				if( no_of_pts == 3 && rt_3pts_collinear( pt[0],pt[1],pt[2],&tol ) )
				{
					if( debug == 2 )
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
							if( debug == 2 )
								rt_log( "\tRe-using existing vertex at ( %f %f %f )\n" , V3ARGS( vp[i]->vg_p->coord ) );
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
	return( group_code );
}

main( int argc , char *argv[] )
{
	register int c;			/* Command line option letter */
	char *dxf_name;			/* Name of input file */
	char *base_name;		/* Base name from dxf_name (no /`s) */
	int name_len;			/* Length of name */
	int done=0;			/* Flag for loop control */
	int polysolids=0;		/* Flag for polysolid output */
	int do_layers=1;		/* Flag for grouping according to layer name */
	int no_of_layers;		/* number of layers to process */
	int curr_layer;			/* current layer number */
	int do_normals=0;
	int obj_count=0;		/* count of objects written to BRLCAD DB file */
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
	dxf_name = std_in_name;

	if( argc < 2 )
		rt_bomb( usage );

	/* get command line arguments */
	while ((c = getopt(argc, argv, "nd:lpt:i:o:")) != EOF)
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
				debug = atoi( optarg );
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

	if( dxf == NULL || do_layers && dxf == stdin )
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
	nmg_tbl( &layers , TBL_INIT , NULL );

	if( do_layers )
	{
		/* Make list of layer names */
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
		if( do_layers )
			rewind( dxf );
		while( fgets( line , LINELEN , dxf ) != NULL )
		{
			group_code = atoi( line );
			if( group_code == 0 )
			{
				if( fgets( line , LINELEN , dxf ) == NULL )
					rt_bomb( "Unexpected EOF in input file\n" );
				if( !strncmp( line , "SECTION" , 7 ) )
				{
					/* start of a section, is it the ENTITIES section ??? */
					if( fgets( line , LINELEN , dxf ) == NULL )
						rt_bomb( "Unexpected EOF in input file\n" );
					group_code = atoi( line );
					/* look for the section name */
					while( group_code != 2 )
					{
						if( fgets( line , LINELEN , dxf ) == NULL )
							rt_bomb( "Unexpected EOF in input file\n" );
						if( fgets( line , LINELEN , dxf ) == NULL )
							rt_bomb( "Unexpected EOF in input file\n" );
						group_code = atoi(line );
					}
					if( fgets( line , LINELEN , dxf ) == NULL )
						rt_bomb( "Unexpected EOF in input file\n" );
					if( !strncmp( line , "ENTITIES" , 8 ) )
					{
						if( debug )
							rt_log( "Found ENTITIES Section\n" );
						break;
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
		if( fgets( line , LINELEN , dxf ) == NULL )
			rt_bomb( "Unexpected EOF in input file\n" );
		group_code = atoi( line );
		done = 0;
		while( !done )
		{
			if (group_code == 0 )
			{
				if( fgets( line , LINELEN , dxf ) == NULL )
					rt_bomb( "Unexpected EOF in input file\n" );
				if( !strncmp( line , "3DFACE" , 6 ) )
					group_code = Do_3dface();
				else if( !strncmp( line , "POLYLINE" , 8 ) )
					group_code = Do_polyline();
				else if( !strncmp( line , "ENDSEC" , 6 ) )
				{
					if( debug )
						rt_log( "Found end of ENTITIES section\n" );
					done = 1;
					break;
				}
			}
			else
			{
				if( fgets( line , LINELEN , dxf ) == NULL )
					rt_bomb( "Unexpected EOF in input file\n" );
				if( fgets( line , LINELEN , dxf ) == NULL )
					rt_bomb( "Unexpected EOF in input file\n" );
				group_code = atoi( line );
			}
		}

		if( debug )
			rt_log( "%d unique vertices\n" , NMG_TBL_END( &vertices ) );

		/* glue faces together */
		if( NMG_TBL_END( &faces ) )
		{
			if( debug )
				rt_log( "Glueing %d faces together...\n" , NMG_TBL_END( &faces ) );
			nmg_gluefaces( (struct faceuse **)NMG_TBL_BASEADDR( &faces) , NMG_TBL_END( &faces ) );
		}

		if( do_normals )
			nmg_fix_normals( s );

		/* write the nmg to the output file */
		obj_count++;

		if( !do_layers )
			curr_name = all;
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

		if( debug )
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
