#include "conf.h"

#include <stdio.h>
#include <math.h>
#ifdef USE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif
#include <errno.h>
#include "machine.h"
#include "externs.h"
#include "vmath.h"
#include "nmg.h"
#include "rtgeom.h"
#include "raytrace.h"
#include "wdb.h"
#include "../librt/debug.h"

#define	LINELEN	256 /* max input line length from elements file */

static char *usage="dxf-g [-v] [-p] [-t tolerance] [-i input_file] [-o output_file_name]";

int
main( argc , argv )
int argc;
char *argv[];
{
	register int c;
	FILE *dxf;
	struct rt_wdb *out_fp;
	char *output_file = "dxf.g";
	char *base_name,*dxf_name;
	char *ptr1,*ptr2;
	char curr_name[LINELEN];
	int name_len;
	struct bn_tol tol;
	int done=0;
	int polysolids=0;
	int verbose=0;
	int i,j,group_code;
	char line[LINELEN];
	struct bu_ptbl vertices;
	struct bu_ptbl faces;
	struct model *m;
	struct nmgregion *r;
	struct shell *s;
	struct faceuse *fu;

        /* XXX These need to be improved */
        tol.magic = BN_TOL_MAGIC;
        tol.dist = 0.0005;
        tol.dist_sq = tol.dist * tol.dist;
        tol.perp = 1e-6;
        tol.para = 1 - tol.perp;

	dxf = stdin;
	dxf_name = (char *)NULL;
	base_name = (char *)NULL;

	/* get command line arguments */
	while ((c = getopt(argc, argv, "pvt:i:o:")) != EOF)
	{
		switch( c )
		{
			case 't':	/* tolerance */
				tol.dist = atof( optarg );
				tol.dist_sq = tol.dist * tol.dist;
				break;
			case 'v':	/* verbose */
				verbose = 1;
				break;
			case 'i':	/* input DXF file name */
				dxf_name = (char *)bu_malloc( strlen( optarg ) + 1 , "Dxf-g: dxf_name" );
				strcpy( dxf_name , optarg );
				if( (dxf = fopen( dxf_name , "r" )) == NULL )
				{
					bu_log( "Cannot open %s\n" , dxf_name );
					perror( "viewpoint-g" );
					rt_bomb( "Cannot open input file" );
				}
				break;
			case 'o':	/* output file name */
				output_file = optarg;
				break;
			case 'p':	/* produce polysolids as output instead of NMG's */
				polysolids = 1;
				break;
			default:
				rt_bomb( usage );
				break;
		}
	}

	if( (out_fp = wdb_fopen( output_file )) == NULL )
	{
		rt_bomb( "Cannot open output file\n" );
		bu_log( "tankill-g: Cannot open %s\n" , output_file );
		perror( output_file );

	}

	if( dxf_name )
	{
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

		base_name = (char *)bu_malloc( name_len + 1 , "base_name" );
		strncpy( base_name , ptr1 , name_len );

		mk_id( out_fp , base_name );
	}
	else
		mk_id( out_fp , "Conversion from DXF" );
	

	/* Find the ENTITIES SECTION */
	if( verbose )
		bu_log( "Looking for ENTITIES section..\n" );
	while( fgets( line , LINELEN , dxf ) != NULL )
	{
		sscanf( line , "%d" , &group_code );
		if( group_code == 0 )
		{
			if( verbose )
				bu_log( "Found group code 0\n" );
			/* read label from next line */
			if( fgets( line , LINELEN , dxf ) == NULL )
				rt_bomb( "Unexpected EOF in input file\n" );
			if( !strncmp( line , "SECTION" , 7 ) )
			{
				if( verbose )
					bu_log( "Found 'SECTION'\n" );
				if( fgets( line , LINELEN , dxf ) == NULL )
					rt_bomb( "Unexpected EOF in input file\n" );
				sscanf( line , "%d" , &group_code );
				if( group_code == 2 )
				{
					if( verbose )
						bu_log( "Found group code 2\n" );
					if( fgets( line , LINELEN , dxf ) == NULL )
						rt_bomb( "Unexpected EOF in input file\n" );
					if( !strncmp( line , "ENTITIES" , 8 ) )
					{
						if( verbose )
							bu_log( "Found 'ENTITIES'\n" );
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
	s = BU_LIST_FIRST( shell , &r->s_hd );

	bu_ptbl_init( &vertices , 64, " &vertices ");
	bu_ptbl_init( &faces , 64, " &faces ");

	/* Read the ENTITIES section */
	while( !done )
	{
		struct vertex *vp[4];
		point_t pt[4];
		int no_of_pts;
		int line_entity=0;
		int face_entity=0;

		if( fgets( line , LINELEN , dxf ) == NULL )
			rt_bomb( "Unexpected EOF in input file\n" );
		while( strncmp( line , "3DFACE" , 6 ) && strncmp( line, "LINE", 4 ) )
		{
			if( !strncmp( line , "ENDSEC" , 6 ) )
			{
				bu_log( "Found end of ENTITIES section\n" );
				done = 1;
				break;
			}
			line[ strlen(line)-1 ] = '\0';
			bu_log( "Unknown entity type (%s), skipping\n" , line );
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

		if( !strncmp( line, "3DFACE", 6 ) )
			face_entity = 1;
		else if( !strncmp( line, "LINE", 4 ) )
			line_entity = 1;
		else
		{
			bu_log(  "Unknown entity type (%s), skipping\n" , line );
			continue;
		}
		if( verbose )
		{
			if( face_entity )
				bu_log( "Found 3DFACE\n" );
			else if( line_entity )
				bu_log( "Found LINE\n" );
		}

		no_of_pts = (-1);
		group_code = 1;
		while( group_code )
		{
			struct loopuse *lu;

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
						bu_log( "Making %s\n" , curr_name );
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
					if( verbose )
						bu_log( "\tpt[%d][%d]=%g\n", i,j,pt[i][j] );
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

								if( verbose )
									bu_log( "Combining points %d (%g,%g,%g) and %d (%g,%g,%g)\n",
										i, V3ARGS(pt[i]),j, V3ARGS(pt[j]) );
								no_of_pts--;
								for( k=j ; k<no_of_pts ; k++ )
								{
									VMOVE( pt[k] , pt[k+1] );
								}
								j--;
							}
						}
					}
					if( face_entity)
					{
						if( no_of_pts != 3 && no_of_pts != 4 )
						{
							bu_log( "Skipping face with %d vertices\n" , no_of_pts );
							break;
						}
					}

					if( verbose )
					{
						if( face_entity )
							bu_log( "FACE:\n" );
						else if( line_entity )
							bu_log( "LINE:\n" );
					}
					for( i=0 ; i<no_of_pts ; i++ )
					{
						if( verbose )
							bu_log( "\t( %f %f %f )\n" , V3ARGS( pt[i] ) );
						vp[i] = (struct vertex *)NULL;
					}

					if( face_entity )
					{
						fu = nmg_cface( s , vp , no_of_pts );
						bu_ptbl_ins( &faces , (long *)fu );

						for( i=0 ; i<no_of_pts ; i++ )
						{
							if( vp[i]->vg_p == NULL )
							{
								nmg_vertex_gv( vp[i] , pt[i] );
								bu_ptbl_ins( &vertices , (long *)vp[i] );
							}
						}
						for( BU_LIST_FOR( lu , loopuse , &fu->lu_hd ) )
						{
							fastf_t area;
							plane_t pl;

							area = nmg_loop_plane_area( lu , pl );
							if( area > 0.0 )
							{
								if( lu->orientation == OT_OPPOSITE )
									HREVERSE( pl , pl );
								nmg_face_g( fu , pl );
								break;
							}
						}
					}
					else if( line_entity )
					{
						struct edgeuse *eu = 
							(struct edgeuse *)NULL;

						for( i=1 ; i<no_of_pts ; i++ )
						{
							eu = nmg_me( vp[i-1], vp[i], s );
							nmg_vertex_gv( eu->vu_p->v_p, pt[i-1] );
						}
						nmg_vertex_gv( eu->eumate_p->vu_p->v_p, pt[no_of_pts-1] );
					}
					break;
				default:
					if( fgets( line , LINELEN , dxf ) == NULL )
						rt_bomb( "Unexpected EOF in input file\n" );
					break;
			}
		}
	}

	/* fuse vertices that are within tolerance of each other */
	(void)nmg_model_vertex_fuse( m , &tol );

	/* glue faces together */
	bu_log( "Glueing %d faces together...\n" , BU_PTBL_END( &faces ) );
	nmg_gluefaces( (struct faceuse **)BU_PTBL_BASEADDR( &faces) , BU_PTBL_END( &faces ), &tol );

	nmg_rebound( m , &tol );

	nmg_fix_normals( s , &tol );

	nmg_vshell( &r->s_hd , r );

	/* write the nmg to the output file */
	if( polysolids )
	{
		bu_log( "Writing polysolid to output...\n" );
		if( base_name )
			write_shell_as_polysolid( out_fp , base_name , s );
		else
			write_shell_as_polysolid( out_fp , "DXF" , s );
	}
	else
	{
		bu_log( "Writing NMG to output...\n" );
		if( base_name )
			mk_nmg( out_fp , base_name  , m );
		else
			mk_nmg( out_fp , "DXF"  , m );
	}

	fprintf( stderr , "%d polygons\n" , BU_PTBL_END( &faces ) );
	return 0;
}
