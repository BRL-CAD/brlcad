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

#include "conf.h"

#include <stdio.h>
#include <math.h>
#ifdef USE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif
#include <ctype.h>

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

extern char *optarg;
extern int optind,opterr,optopt;
extern int errno;

static char *brlcad_file;	/* name of output file */
static int polysolid=0;		/* Flag for polysolid output rather than NMG's */
static int solid_count=0;	/* count of solids converted */
static struct rt_tol tol;	/* Tolerance structure */
static int id_no=1000;		/* Ident numbers */
static int debug=0;		/* Debug flag */
static char *usage="proe-g [-p] [-d] [-x rt_debug_flag] [-X nmg_debug_flag] proe_file.brl output.g\n\
	where proe_file.brl is the output from Pro/Engineer's BRL-CAD EXPORT option\n\
	and output.g is the name of a BRL-CAD database file to receive the conversion.\n\
	The -p option is to create polysolids rather than NMG's.\n\
	The -d option prints additional debugging information.\n\
	The -x option specifies an RT debug flags (see cad/librt/debug.h).\n\
	The -X option specifies an NMG debug flag (see cad/h/nmg.h).\n";
static FILE *fd_in;		/* input file (from Pro/E) */
static FILE *fd_out;		/* Resulting BRL-CAD file */
static struct db_i *dbip;

struct render_verts
{
	point_t pt;
	struct vertex *v;
};

struct name_conv_list
{
	char brlcad_name[NAMESIZE];
	char solid_name[NAMESIZE];
	char name[80];
	char *obj;
	int solid_use_no;
	int comb_use_no;
	struct name_conv_list *next;
} *name_root=(struct name_conv_list *)NULL;

#define	MAX_LINE_LEN	512

#define	UNKNOWN_TYPE	0
#define	ASSEMBLY_TYPE	1
#define	PART_TYPE	2

struct name_conv_list *
Add_new_name( name , obj , type )
char *name,*obj;
int type;
{
	struct name_conv_list *ptr,*ptr2;
	char tmp_name[NAMESIZE];
	int len;
	char try_char='@';

	if( type != ASSEMBLY_TYPE && type != PART_TYPE )
	{
		rt_log( "Bad type for name (%s) in Add_new_name\n", name );
		rt_bomb( "Add_new_name\n" );
	}

	/* Add a new name */
	ptr = (struct name_conv_list *)rt_malloc( sizeof( struct name_conv_list ) , "Add_new_name: prev->next" );
	ptr->next = (struct name_conv_list *)NULL;
	strcpy( ptr->name , name );
	ptr->obj = obj;
	strncpy( ptr->brlcad_name , name , NAMESIZE-2 );
	ptr->brlcad_name[NAMESIZE-2] = '\0';
	ptr->solid_use_no = 0;
	ptr->comb_use_no = 0;

	/* make sure brlcad_name is unique */
	len = strlen( ptr->brlcad_name );
	strcpy( tmp_name , ptr->brlcad_name );
	ptr2 = name_root;
	while( ptr2 )
	{
		if( !strncmp( tmp_name , ptr2->brlcad_name , NAMESIZE-2 ) || !strncmp( tmp_name , ptr2->solid_name , NAMESIZE-2 ) )
		{
			try_char++;
			if( try_char == '[' )
				try_char = 'a';
			if( try_char == '{' )
				rt_log( "Too many objects with same name (%s)\n" , ptr->brlcad_name );

			strcpy( tmp_name , ptr->brlcad_name );
			sprintf( &tmp_name[len-2] , "_%c" , try_char );
			ptr2 = name_root;
		}
		else
			ptr2 = ptr2->next;
	}

	strcpy( ptr->brlcad_name , tmp_name );

	if( type == ASSEMBLY_TYPE )
	{
		ptr->solid_name[0] = '\0';
		return( ptr );
	}
	else
	{
		strcpy( ptr->solid_name , "s." );
		strncpy( &ptr->solid_name[2] , name , NAMESIZE-4 );
		ptr->solid_name[NAMESIZE-1] = '\0';
	}

	/* make sure solid name is unique */
	len = strlen( ptr->solid_name );
	strcpy( tmp_name , ptr->solid_name );
	ptr2 = name_root;
	while( ptr2 )
	{
		if( !strncmp( tmp_name , ptr2->brlcad_name , NAMESIZE-2 ) || !strncmp( tmp_name , ptr2->solid_name , NAMESIZE-2 ) )
		{
			try_char++;
			if( try_char == '[' )
				try_char = 'a';
			if( try_char == '{' )
				rt_log( "Too many objects with same name (%s)\n" , ptr->brlcad_name );

			strcpy( tmp_name , ptr->solid_name );
			sprintf( &tmp_name[len-2] , "_%c" , try_char );
			ptr2 = name_root;
		}
		else
			ptr2 = ptr2->next;
	}

	strcpy( ptr->solid_name , tmp_name );

	return( ptr );
}

char *
Get_unique_name( name , obj , type )
char *name,*obj;
int type;
{
	struct name_conv_list *ptr,*prev;

	if( name_root == (struct name_conv_list *)NULL )
	{
		/* start new list */
		name_root = Add_new_name( name , obj , type );
		ptr = name_root;
	}
	else
	{
		int found=0;

		prev = (struct name_conv_list *)NULL;
		ptr = name_root;
		while( ptr && !found )
		{
			if( obj == ptr->obj )
				found = 1;
			else
			{
				prev = ptr;
				ptr = ptr->next;
			}
		}

		if( !found )
		{
			prev->next = Add_new_name( name , obj , type );
			ptr = prev->next;
		}
	}

	return( ptr->brlcad_name );
}

char *
Get_solid_name( name , obj )
char *name,*obj;
{
	struct name_conv_list *ptr;

	ptr = name_root;

	while( ptr && obj != ptr->obj )
		ptr = ptr->next;

	if( !ptr )
		ptr = Add_new_name( name , (char *)NULL , PART_TYPE );

	return( ptr->solid_name );
}

void
Convert_assy( line )
char line[MAX_LINE_LEN];
{
	struct wmember head;
	struct wmember *wmem;
	char line1[MAX_LINE_LEN];
	char name[80];
	char *obj;
	char memb_name[80];
	char *memb_obj;
	char *brlcad_name;
	double mat_col[4];
	double conv_factor;
	int start;
	int i;

	RT_LIST_INIT( &head.l );

	start = (-1);
	/* skip leading blanks */
	while( isspace( line[++start] ) && line[start] != '\0' );
	if( strncmp( &line[start] , "assembly" , 8 ) )
	{
		rt_log( "PROE-G: Convert_assy called for non-assembly:\n%s\n" , line );
		return;
	}

	/* skip blanks before name */
	start += 7;
	while( isspace( line[++start] ) && line[start] != '\0' );

	/* get name */
	i = (-1);
	start--;
	while( !isspace( line[++start] ) && line[start] != '\0' && line[start] != '\n' )
		name[++i] = line[start];
	name[++i] = '\0';

	/* get object pointer and conversion factor */
	sscanf( &line[start] , "%x %f" , &obj, &conv_factor );

	rt_log( "Converting Assembly: %s\n" , name );

	if( debug )
		rt_log( "Convert_assy: %s x%x\n" , name , obj );

	while( fgets( line1, MAX_LINE_LEN, fd_in ) )
	{
		/* skip leading blanks */
		start = (-1);
		while( isspace( line1[++start] ) && line[start] != '\0' );

		if( !strncmp( &line1[start] , "endassembly" , 11 ) )
		{

			brlcad_name = Get_unique_name( name , obj , ASSEMBLY_TYPE );
			if( debug )
				rt_log( "\tmake assembly ( %s)\n" , brlcad_name );
			mk_lcomb( fd_out , brlcad_name , &head , 0 ,
			(char *)NULL , (char *)NULL , (unsigned char *)NULL , 0 );
			break;
		}
		else if( !strncmp( &line1[start] , "member" , 6 ) )
		{
			start += 5;
			while( isspace( line1[++start] ) && line1[start] != '\0' );
			i = (-1);
			start--;
			while( !isspace( line1[++start] ) && line1[start] != '\0' && line1[start] != '\n' )
				memb_name[++i] = line1[start];
			memb_name[++i] = '\0';


			sscanf( &line1[start] , "%x" , &memb_obj );

			brlcad_name = Get_unique_name( memb_name , memb_obj , PART_TYPE );
			if( debug )
				rt_log( "\tmember (%s)\n" , brlcad_name );
			wmem = mk_addmember( brlcad_name , &head , WMOP_UNION );
		}
		else if( !strncmp( &line1[start] , "matrix" , 6 ) )
		{
			int i,j;

			for( j=0 ; j<4 ; j++ )
			{
				fgets( line1, MAX_LINE_LEN, fd_in );
				sscanf( line1 , "%lf %lf %lf %lf" , &mat_col[0] , &mat_col[1] , &mat_col[2] , &mat_col[3] );
				for( i=0 ; i<4 ; i++ )
					wmem->wm_mat[4*i+j] = mat_col[i];
			}

			for( j=0 ; j<15 ; j++ )
			{
				/* element #15 should not be affected by conversion factor */
				wmem->wm_mat[j] *= conv_factor;
			}
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
	char name[80];
	char *obj;
	char *solid_name;
	int start;
	int i;
	struct model *m;
	struct nmgregion *r;
	struct shell *s;
	struct render_verts verts[3];
	struct vertex   **vts[3];
	struct nmg_ptbl faces;
	float colr[3];
	unsigned char color[3];
	char *brlcad_name;
	struct wmember head;
	vect_t normal;
	double conv_factor;

	RT_LIST_INIT( &head.l );
	nmg_tbl( &faces , TBL_INIT , (long *)NULL );

	m = nmg_mm();
	r = nmg_mrsv( m );
	s = RT_LIST_FIRST( shell , &r->s_hd );

	start = (-1);
	/* skip leading blanks */
	while( isspace( line[++start] ) && line[start] != '\0' );
	if( strncmp( &line[start] , "solid" , 5 ) )
	{
		rt_log( "Convert_part: Called for non-part\n%s\n" , line );
		return;
	}

	/* skip blanks before name */
	start += 4;
	while( isspace( line[++start] ) && line[start] != '\0' );

	/* get name */
	i = (-1);
	start--;
	while( !isspace( line[++start] ) && line[start] != '\0' && line[start] != '\n' )
		name[++i] = line[start];
	name[++i] = '\0';

	/* get object id and conversion factor */
	sscanf( &line[start] , "%x %f" , &obj, &conv_factor );

	rt_log( "Converting Part: %s\n" , name );

	if( debug )
		rt_log( "Conv_part %s x%x\n" , name , obj );

	while( fgets( line1, MAX_LINE_LEN, fd_in ) != NULL )
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
		else if( !strncmp( &line1[start] , "normal" , 6 ) )
		{
			float x,y,z;

			start += 6;
			sscanf( &line1[start] , "%f%f%f" , &x , &y , &z );
			VSET( normal , x , y , z );
		}
		else if( !strncmp( &line1[start] , "facet" , 5 ) )
		{
			VSET( normal , 0.0 , 0.0 , 0.0 );
		}
		else if( !strncmp( &line1[start] , "outer loop" , 10 ) )
		{
			struct faceuse *fu;
			fastf_t area;
			int endloop=0;
			int vert_no=0;
			struct loopuse *lu;
			plane_t pl;

			while( !endloop )
			{
				fgets( line1, MAX_LINE_LEN, fd_in );
				
				start = (-1);
				while( isspace( line1[++start] ) );

				if( !strncmp( &line1[start] , "endloop" , 7 ) )
					endloop = 1;
				else if ( !strncmp( &line1[start] , "vertex" , 6 ) )
				{
					float x,y,z;

					sscanf( &line1[start+6] , "%f%f%f" , &x , &y , &z );
					VSET( verts[vert_no].pt , x , y , z );
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

			lu = RT_LIST_FIRST( loopuse , &fu->lu_hd );

			area = nmg_loop_plane_area( lu , pl );
			if( area > 0.0 )
			{
				if( normal[X] != 0.0 || normal[Y] != 0.0 || normal[Z] != 0.0 )
				{
					if( VDOT( normal , pl ) < 0.0 )
					{
						HREVERSE( pl , pl );
					}
				}
				nmg_face_g( fu , pl );
			}

			if( area > 0.0 )
				nmg_tbl( &faces , TBL_INS , (long *)fu );
			else
				(void)nmg_kfu( fu );
		}
	}

	/* Check if this part has any solid parts */
	if( NMG_TBL_END( &faces ) == 0 )
	{
		rt_log( "\t%s has no solid parts, ignoring\n" , name );
		nmg_km( m );
		return;
	}

	/* fuse vertices that are within tolerance of each other */
	rt_log( "\tFusing vertices for part\n" );
	(void)nmg_model_vertex_fuse( m , &tol );

	rt_log( "\tGlueing faces together\n" );
	nmg_gluefaces( (struct faceuse **)NMG_TBL_BASEADDR( &faces) , NMG_TBL_END( &faces ) );
	nmg_tbl( &faces , TBL_FREE , (long *)NULL );

/*	if( !polysolid )
	{
		nmg_shell_coplanar_face_merge( s , &tol , 0 );

		nmg_simplify_shell( s );

		nmg_rebound( m , &tol );

		(void)nmg_model_vertex_fuse( m , &tol );
	}
	else
*/
		nmg_rebound( m , &tol );

	solid_count++;
	solid_name = Get_solid_name( name , obj );

	if( debug )
		rt_log( "Writing solid (%s)\n" , solid_name );

	if( polysolid )
	{
		rt_log( "\tWriting polysolid\n" );
		write_shell_as_polysolid( fd_out , solid_name , s );
	}
	else
	{
		rt_log( "\tWriting NMG\n" );
		mk_nmg( fd_out , solid_name , m );
	}

	nmg_km( m );

	mk_addmember( solid_name , &head , WMOP_UNION );

	brlcad_name = Get_unique_name( name , obj , PART_TYPE );
	if( debug )
		rt_log( "\tMake region (%s)\n" , brlcad_name );

	mk_lrcomb( fd_out, brlcad_name, &head, 1, (char *)NULL, (char *)NULL,
	color, id_no, 0, 1, 100, 0 );
	id_no++;
}

void
Convert_input()
{
	char line[ MAX_LINE_LEN ];

	while( fgets( line, MAX_LINE_LEN, fd_in ) )
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

	if( argc < 2 )
	{
		fprintf(stderr, usage, argv[0]);
		exit(1);
	}

	/* Get command line arguments. */
	while ((c = getopt(argc, argv, "dx:X:p")) != EOF) {
		switch (c) {
		case 'd':
			debug = 1;
			break;
		case 'x':
			sscanf( optarg, "%x", &rt_g.debug );
			break;
		case 'X':
			sscanf( optarg, "%x", &rt_g.NMG_debug );
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

	if( (fd_in=fopen( argv[optind], "r")) == NULL )
	{
		fprintf( stderr, "Cannot open input file (%s)\n" , argv[optind] );
		perror( argv[0] );
		exit( 1 );
	}
	optind++;
	brlcad_file = argv[optind];
	if( (fd_out=fopen( brlcad_file, "w")) == NULL )
	{
		fprintf( stderr, "Cannot open BRL-CAD file (%s)\n" , brlcad_file );
		perror( argv[0] );
		exit( 1 );
	}

	mk_id_units( fd_out , "Conversion from Pro/Engineer" , "in" );

	Convert_input();

}
