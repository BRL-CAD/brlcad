/*
 *		P R O E - G
 *
 * Code to convert ascii output from Pro/Engineer to BRL-CAD
 * The required output is from the Pro/Develop application proe-brl
 * that must be initiated from the "BRL-CAD" option of Pro/Engineer's
 * "EXPORT" menu.  The Pro/develop application may be obtained via
 * anonymous FTP from ftp.brl.mil or via email from "jra@arl.mil"
 *
 *  Author -
 *	John R. Anderson
 *  
 *  Source -
 *	The U. S. Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066
 *  
 *  Distribution Notice -
 *	Re-distribution of this software is restricted, as described in
 *	your "Statement of Terms and Conditions for the Release of
 *	The BRL-CAD Pacakge" agreement.
 *
 *  Copyright Notice -
 *	This software is Copyright (C) 1994 by the United States Army
 *	in all countries except the USA.  All rights reserved.
 */

#ifndef lint
static char RCSid[] = "$Header$";
#endif


#include <stdio.h>
#include <math.h>
#include <string.h>
#include "machine.h"
#include "externs.h"
#include "vmath.h"
#include "nmg.h"
#include "rtgeom.h"
#include "raytrace.h"
#include "wdb.h"
#include "db.h"
#include "../librt/debug.h"

RT_EXTERN( fastf_t nmg_loop_plane_area , ( struct loopuse *lu , plane_t pl ) );

static int polysolid=0;		/* Flag for polysolid output rather than NMG's */
static int NMG_debug=0;		/* NMG debug flag */
static int solid_count=0;	/* count of solids converted */
static struct rt_tol tol;	/* Tolerance structure */
static int id_no=1000;		/* Ident numbers */
static char *usage="proe-g < proe_file.brl > output.g\n\
	where proe_file.brl is the output from Pro/Engineer's BRL-CAD EXPORT option\n\
	and output.g is the name of a BRL-CAD database file\n";

struct render_verts
{
	point_t pt;
	struct vertex *v;
};

#define	MAX_LINE_LEN	256

void
Convert_assy( line )
char line[MAX_LINE_LEN];
{
	struct wmember head;
	struct wmember *wmem;
	char line1[MAX_LINE_LEN];
	char name[NAMESIZE+1];
	char memb_name[NAMESIZE+1];
	double mat_col[4];
	int start;

	RT_LIST_INIT( &head.l );

	start = 7;
	while( isspace( line[++start] ) );

	strncpy( name , &line[start] , NAMESIZE );
	name[NAMESIZE] = '\0';

	while( gets( line1 ) )
	{
		if( !strncmp( line1 , "endassembly" , 11 ) )
		{
			mk_lcomb( stdout , name , &head , 0 , (char *)NULL , (char *)NULL , (char *)NULL , 0 );
			break;
		}
		else if( !strncmp( line1 , " member" , 7 ) )
		{
			start = 6;
			while( isspace( line1[++start] ) );
			strncpy( memb_name , &line1[start] , NAMESIZE );
			memb_name[NAMESIZE] = '\0';
			wmem = mk_addmember( memb_name , &head , WMOP_UNION );
		}
		else if( !strncmp( line1 , " matrix" , 7 ) )
		{
			int i,j;

			for( j=0 ; j<4 ; j++ )
			{
				gets( line1 );
				sscanf( line1 , "%lf %lf %lf %lf" , &mat_col[0] , &mat_col[1] , &mat_col[2] , &mat_col[3] );
				for( i=0 ; i<4 ; i++ )
					wmem->wm_mat[4*i+j] = mat_col[i];
			}

			wmem->wm_mat[3] *= 25.4;
			wmem->wm_mat[7] *= 25.4;
			wmem->wm_mat[11] *= 25.4;
		}
		else
		{
			fprintf( stderr , "Unrecognized line in assembly (%s)\n%s\n" , name , line1 );
		}
	}
}

void
Convert_part( line )
char line[MAX_LINE_LEN];
{
	char line1[MAX_LINE_LEN];
	char name[NAMESIZE+1];
	char solid_name[NAMESIZE+1];
	int start;
	int i;
	struct model *m;
	struct nmgregion *r;
	struct shell *s;
	struct render_verts verts[3];
	struct vertex   **vts[3];
	struct nmg_ptbl faces;
	float colr[3];
	char color[3];
	struct wmember head;
	struct wmember *wmem;

	RT_LIST_INIT( &head.l );
	nmg_tbl( &faces , TBL_INIT , (long *)NULL );

	m = nmg_mm();
	r = nmg_mrsv( m );
	s = RT_LIST_FIRST( shell , &r->s_hd );

	start = 5;
	while( isspace( line[++start] ) );
	strcpy( name , &line[start] );
	name[NAMESIZE] = '\0';

	while( gets( line1 ) != NULL )
	{
		start = (-1);
		while( isspace( line1[++start] ) );
		if( !strncmp( &line1[start] , "endsolid" , 8 ) )
			break;
		else if( !strncmp( &line1[start] , "color" , 5 ) )
		{
			sscanf( &line1[start+5] , "%f%f%f" , &colr[0] , &colr[1] , &colr[2] );
			for( i=0 ; i<3 ; i++ )
				color[i] = (int)(colr[i] * 255.0);
		}
		else if( !strncmp( &line1[start] , "outer loop" , 10 ) )
		{
			struct faceuse *fu;
			fastf_t area;
			int endloop=0;
			int vert_no=0;
			struct loopuse *lu;

			while( !endloop )
			{
				gets( line1 );
				
				start = (-1);
				while( isspace( line1[++start] ) );

				if( !strncmp( &line1[start] , "endloop" , 7 ) )
					endloop = 1;
				else if ( !strncmp( &line1[start] , "vertex" , 6 ) )
				{
					float x,y,z;

					sscanf( &line1[start+6] , "%f%f%f" , &x , &y , &z );
					VSET( verts[vert_no].pt , x*25.4 , y*25.4 , z*25.4 );
					vert_no++;
				}
			}

			for( i=0 ; i<3 ; i++ )
			{
				verts[i].v = (struct vertex *)NULL;
				vts[i] = &verts[i].v;
			}

			fu = nmg_cmface( s , vts , 3 );

			for( i=0 ; i<3 ; i++ )
				nmg_vertex_gv( verts[i].v , verts[i].pt );

			for( RT_LIST_FOR( lu , loopuse , &fu->lu_hd ) )
			{
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

			if( area > 0.0 )
				nmg_tbl( &faces , TBL_INS , (long *)fu );
			else
				(void)nmg_kfu( fu );
		}
	}

	/* fuse vertices that are within tolerance of each other */
	(void)nmg_model_vertex_fuse( m , &tol );

	nmg_gluefaces( (struct faceuse **)NMG_TBL_BASEADDR( &faces) , NMG_TBL_END( &faces ) );
	nmg_tbl( &faces , TBL_FREE , (long *)NULL );

	nmg_fix_normals( s , &tol );

	if( !polysolid )
		nmg_shell_coplanar_face_merge( s , &tol , 1 );

	nmg_rebound( m , &tol );

	nmg_model_vertex_fuse( m , &tol );

	sprintf( solid_name , "sol.%d" , solid_count );
	solid_count++;

	if( polysolid )
		write_shell_as_polysolid( stdout , solid_name , s );
	else
	{
		nmg_shell_coplanar_face_merge( s , &tol , 1 );
		mk_nmg( stdout , solid_name , m );
	}

	nmg_km( m );

	mk_addmember( solid_name , &head , WMOP_UNION );
	mk_lrcomb( stdout , name , &head , 1 , (char *)NULL , (char *)NULL , color , id_no , 0 , 1 , 100 , 0 );
	id_no++;
}

void
Convert_input()
{
	char line[ MAX_LINE_LEN ];

	while( gets( line ) )
	{
		if( !strncmp( line , "assembly" , 8 ) )
			Convert_assy( line );
		else if( !strncmp( line , "solid" , 5 ) )
			Convert_part( line );
		else
			fprintf( stderr , "Unrecognized line:\n%s\n" , line );
	}
}

/*
 *			M A I N
 */
int
main(argc, argv)
int	argc;
char	*argv[];
{
	register int c;

        /* XXX These need to be improved */
        tol.magic = RT_TOL_MAGIC;
        tol.dist = 0.0005;
        tol.dist_sq = tol.dist * tol.dist;
        tol.perp = 1e-6;
        tol.para = 1 - tol.perp;

	/* Get command line arguments. */
	while ((c = getopt(argc, argv, "x:X:p")) != EOF) {
		switch (c) {
		case 'x':
			sscanf( optarg, "%x", &rt_g.debug );
			break;
		case 'X':
			sscanf( optarg, "%x", &rt_g.NMG_debug );
			NMG_debug = rt_g.NMG_debug;
			break;
		case 'p':
			polysolid = 1;
			break;
		default:
			fprintf(stderr, usage, argv[0]);
			exit(1);
			break;
		}
	}

	mk_id_units( stdout , "Conversion from Pro/Engineer" , "in" );

	Convert_input();
}
