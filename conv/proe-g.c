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
#include <libgen.h>		/* this is sgi-specific, or maybe ANSI-specific */
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
#include "../librt/debug.h"

RT_EXTERN( fastf_t nmg_loop_plane_area , ( struct loopuse *lu , plane_t pl ) );
RT_EXTERN( fastf_t mat_determinant , ( mat_t m ) );
RT_EXTERN( fastf_t mat_det3 , ( mat_t m ) );
RT_EXTERN( struct edgeuse *nmg_next_radial_eu, ( struct edgeuse *eu, struct shell *s, int wires ) );

extern char *__loc1;	/* used by regex */
extern char *optarg;
extern int optind,opterr,optopt;
extern int errno;

static char *brlcad_file;	/* name of output file */
static int polysolid=0;		/* Flag for polysolid output rather than NMG's */
static int solid_count=0;	/* count of solids converted */
static struct rt_tol tol;	/* Tolerance structure */
static int id_no=1000;		/* Ident numbers */
static int air_no=1;		/* Air numbers */
static int debug=0;		/* Debug flag */
static int cut_count=0;		/* count of assembly cut HAF solids created */
static int do_regex=0;		/* flag to indicate if 'u' option is in effect */
static int do_simplify=0;	/* flag to try to simplify solids */
static char *reg_cmp=(char *)NULL;		/* compiled regular expression */
static char *usage="proe-g [-psdar] [-u reg_exp] [-x rt_debug_flag] [-X nmg_debug_flag] proe_file.brl output.g\n\
	where proe_file.brl is the output from Pro/Engineer's BRL-CAD EXPORT option\n\
	and output.g is the name of a BRL-CAD database file to receive the conversion.\n\
	The -p option is to create polysolids rather than NMG's.\n\
	The -s option is to simplify the objects to ARB's where possible.\n\
	The -d option prints additional debugging information.\n\
	The -i option sets the initial region ident number (default is 1000).\n\
	The -u option indicates that portions of object names that match the regular expression\n\
		'reg_exp' should be ignored.\n\
	The -a option creates BRL-CAD 'air' regions from everything in the model.\n\
	The -r option indicates that the model should not be re-oriented or scaled,\n\
		but left in the same orientation as it was in Pro/E.\n\
		This is to allow conversion of parts to be included in\n\
		previously converted Pro/E assemblies.\n\
	The -x option specifies an RT debug flags (see cad/librt/debug.h).\n\
	The -X option specifies an NMG debug flag (see cad/h/nmg.h).\n";
static FILE *fd_in;		/* input file (from Pro/E) */
static FILE *fd_out;		/* Resulting BRL-CAD file */
static struct nmg_ptbl null_parts; /* Table of NULL solids */
static float conv_factor=1.0;	/* conversion factor from model units to mm */
static int top_level=1;		/* flag to catch top level assembly or part */
static mat_t re_orient;		/* rotation matrix to put model in BRL-CAD orientation
				 * (+x towards front +z is up ) */
static int do_air=0;		/* When set, all regions are BRL-CAD "air" regions */
static int do_reorient=1;	/* When set, reorient entire model to BRL-CAD style */

#define NAMESIZE	16	/* from db.h */

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

struct ptc_plane
{
	double  e1[3], e2[3], e3[3], origin[3];
};

struct ptc_cylinder
{
	double  e1[3], e2[3], e3[3], origin[3];
	double  radius;
};

union ptc_surf
{
	struct ptc_plane plane;
	struct ptc_cylinder cylinder;
};

struct ptc_surf_list
{
	struct rt_list	l;
	int type;
	union ptc_surf	surf;
} *surf_root=(struct ptc_surf_list *)NULL;

/* for type in struct ptc_plane and struct ptc_cylinder */
#define	SURF_PLANE	1
#define	SURF_CYLINDER	2

#define	MAX_LINE_LEN	512

#define	UNKNOWN_TYPE	0
#define	ASSEMBLY_TYPE	1
#define	PART_TYPE	2
#define	CUT_SOLID_TYPE	3

static struct name_conv_list *
Add_new_name( name , obj , type )
char *name,*obj;
int type;
{
	struct name_conv_list *ptr,*ptr2;
	char tmp_name[NAMESIZE];
	int suffix_insert;
	char try_char='@';
	char *after_match;

	if( debug )
		rt_log( "Add_new_name( %s, x%x, %d )\n", name, obj, type );

	if( type != ASSEMBLY_TYPE && type != PART_TYPE && type != CUT_SOLID_TYPE )
	{
		rt_log( "Bad type for name (%s) in Add_new_name\n", name );
		rt_bomb( "Add_new_name\n" );
	}


	/* Add a new name */
	ptr = (struct name_conv_list *)rt_malloc( sizeof( struct name_conv_list ) , "Add_new_name: prev->next" );
	ptr->next = (struct name_conv_list *)NULL;
	strncpy( ptr->name , name, 80 );
	ptr->obj = obj;
	if( do_regex && type != CUT_SOLID_TYPE )
	{
		after_match = regex( reg_cmp, ptr->name );
		if( after_match && *after_match != '\0' )
		{
			if( __loc1 == ptr->name )
				strncpy( ptr->brlcad_name , after_match , NAMESIZE-2 );
			else
			{
				char *str;
				int i=0;

				str = ptr->name;
				while( str != __loc1 && i<NAMESIZE )
					ptr->brlcad_name[i++] = *str++;
				strncpy( str, after_match, NAMESIZE-2-i );
			}
		}
		else
			strncpy( ptr->brlcad_name , name , NAMESIZE-2 );
		if( debug )
			rt_log( "\tafter reg_ex, name is %s\n", ptr->brlcad_name );
	}
	else if( type == CUT_SOLID_TYPE )
		ptr->brlcad_name[0] = '\0';
	else
		strncpy( ptr->brlcad_name , name , NAMESIZE-2 );
	ptr->brlcad_name[NAMESIZE-2] = '\0';
	ptr->solid_use_no = 0;
	ptr->comb_use_no = 0;

	if( type != CUT_SOLID_TYPE )
	{
		/* make sure brlcad_name is unique */
		suffix_insert = strlen( ptr->brlcad_name );
		if( suffix_insert > NAMESIZE - 3 )
			suffix_insert = NAMESIZE - 3;

		strncpy( tmp_name, ptr->brlcad_name, NAMESIZE );
		if( debug )
			rt_log( "\tMaking sure %s is a unique name\n", tmp_name );
		ptr2 = name_root;
		while( ptr2 )
		{
			if( !strncmp( tmp_name , ptr2->brlcad_name , NAMESIZE ) || !strncmp( tmp_name , ptr2->solid_name , NAMESIZE ) )
			{
				if( debug )
					rt_log( "\t\t%s matches existing name (%s or %s)\n", tmp_name, ptr2->brlcad_name, ptr2->solid_name );
				try_char++;
				if( try_char == '[' )
					try_char = 'a';
				if( debug )
					rt_log( "\t\t\ttry_char = %c\n", try_char );
				if( try_char == '{' )
				{
					rt_log( "Too many objects with same name (%s)\n" , ptr->brlcad_name );
					exit(1);
				}

				strncpy( tmp_name, ptr->brlcad_name, NAMESIZE );
				sprintf( &tmp_name[suffix_insert] , "_%c" , try_char );
				if( debug )
					rt_log( "\t\tNew name to try is %s\n", tmp_name );
				ptr2 = name_root;
			}
			else
				ptr2 = ptr2->next;
		}

		strncpy( ptr->brlcad_name, tmp_name, NAMESIZE );
	}

	if( type == ASSEMBLY_TYPE )
	{
		ptr->solid_name[0] = '\0';
		return( ptr );
	}
	else if( type == PART_TYPE )
	{
		strcpy( ptr->solid_name , "s." );
		strncpy( &ptr->solid_name[2] , ptr->brlcad_name , NAMESIZE-4 );
		ptr->solid_name[NAMESIZE-1] = '\0';
	}
	else
	{
		strcpy( ptr->solid_name , "s." );
		strncpy( &ptr->solid_name[2] , name , NAMESIZE-4 );
		ptr->solid_name[NAMESIZE-1] = '\0';
	}

	/* make sure solid name is unique */
	suffix_insert = strlen( ptr->solid_name );
	if( suffix_insert > NAMESIZE - 3 )
		suffix_insert = NAMESIZE - 3;

	strncpy( tmp_name, ptr->solid_name, NAMESIZE );
	if( debug )
		rt_log( "\tMaking sure %s is a unique solid name\n", tmp_name );
	ptr2 = name_root;
	try_char = '@';
	while( ptr2 )
	{
		if( !strncmp( tmp_name , ptr2->brlcad_name , NAMESIZE ) || !strncmp( tmp_name , ptr2->solid_name , NAMESIZE ) )
		{
			if( debug )
				rt_log( "\t\t%s matches existing name (%s or %s)\n", tmp_name, ptr2->brlcad_name, ptr2->solid_name );
			try_char++;
			if( try_char == '[' )
				try_char = 'a';
			if( debug )
				rt_log( "\t\t\ttry_char = %c\n", try_char );
			if( try_char == '{' )
			{
				rt_log( "Too many solids with same name (%s)\n" , ptr->solid_name );
				exit(1);
			}

			strncpy( tmp_name, ptr->solid_name, NAMESIZE );
			sprintf( &tmp_name[suffix_insert] , "_%c" , try_char );
			if( debug )
				rt_log( "\t\tNew name to try is %s\n", tmp_name );
			ptr2 = name_root;
		}
		else
			ptr2 = ptr2->next;
	}

	strncpy( ptr->solid_name, tmp_name, NAMESIZE );

	return( ptr );
}

static char *
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

static char *
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

static void
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
	float mat_col[4];
	int start;
	int i;

	if( rt_g.debug & DEBUG_MEM_FULL )
	{
		rt_log( "Barrier check at start of Convert_assy:\n" );
		if( rt_mem_barriercheck() )
			rt_bomb( "Barrier check failed!!!\n" );
	}

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

	/* get object pointer */
	sscanf( &line[start] , "%x %f" , &obj );

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
			{
				struct wmember *wp;

				rt_log( "\tmake assembly ( %s)\n" , brlcad_name );
				for( RT_LIST_FOR( wp, wmember, &head.l ) )
					rt_log( "\t%c %s\n", wp->wm_op, wp->wm_name );
			}
			else
				rt_log( "\tUsing name: %s\n", brlcad_name );

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
			double scale,inv_scale;

			for( j=0 ; j<4 ; j++ )
			{
				fgets( line1, MAX_LINE_LEN, fd_in );
				sscanf( line1 , "%f %f %f %f" , &mat_col[0] , &mat_col[1] , &mat_col[2] , &mat_col[3] );
				for( i=0 ; i<4 ; i++ )
					wmem->wm_mat[4*i+j] = mat_col[i];
			}

			/* convert this matrix to seperate scale factor into element #15 */
/*			scale = MAGNITUDE( &wmem->wm_mat[0] ); */
			scale = pow( mat_det3( wmem->wm_mat ), 1.0/3.0 );
			if( debug )
			{
				mat_print( brlcad_name, wmem->wm_mat );
				rt_log( "\tscale = %g, conv_factor = %g\n", scale, conv_factor );
			}
			if( scale != 1.0 )
			{
				inv_scale = 1.0/scale;
				for( j=0 ; j<3 ; j++ )
					HSCALE( &wmem->wm_mat[j*4], &wmem->wm_mat[j*4], inv_scale )

				if( top_level)
					wmem->wm_mat[15] *= (inv_scale/conv_factor);
				else
					wmem->wm_mat[15] *= inv_scale;
			}
			else if( top_level )
				wmem->wm_mat[15] /= conv_factor;

			if( top_level && do_reorient )
			{
				/* apply re_orient transformation here */
				if( debug )
				{
					rt_log( "Applying re-orient matrix to member %s\n", brlcad_name );
					mat_print( "re-orient matrix", re_orient );
				}
				mat_mul2( re_orient, wmem->wm_mat );
			}
			if( debug )
				mat_print( "final matrix", wmem->wm_mat );
		}
		else
		{
			rt_log( "Unrecognized line in assembly (%s)\n%s\n" , name , line1 );
		}
	}

	if( rt_g.debug & DEBUG_MEM_FULL )
	{
		rt_log( "Barrier check at end of Convet_assy:\n" );
		if( rt_mem_barriercheck() )
			rt_bomb( "Barrier check failed!!!\n" );
	}

	top_level = 0;

}

static void
do_modifiers( line1, start, head, name, min, max )
char *line1;
int *start;
struct wmember *head;
char *name;
point_t min, max;
{
	struct wmember *wmem;
	int i;

	while( strncmp( &line1[*start], "endmodifiers", 12 ) )
	{
		if( !strncmp( &line1[*start], "plane", 5 ) )
		{
			struct name_conv_list *ptr;
			char haf_name[NAMESIZE+1];
			fastf_t dist;
			fastf_t tmp_dist;
			point_t origin;
			plane_t plane;
			vect_t e1,e2;
			double u_min,u_max,v_min,v_max;
			double x,y,z;
			int orient;
			point_t arb_pt[8];
			point_t rpp_corner;

			fgets( line1, MAX_LINE_LEN, fd_in );
			sscanf( line1, "%lf %lf %lf", &x, &y, &z );
			VSET( origin, x, y, z );
			fgets( line1, MAX_LINE_LEN, fd_in );
			sscanf( line1, "%lf %lf %lf", &x, &y, &z );
			VSET( e1, x, y, z );
			fgets( line1, MAX_LINE_LEN, fd_in );
			sscanf( line1, "%lf %lf %lf", &x, &y, &z );
			VSET( e2, x, y, z );
			fgets( line1, MAX_LINE_LEN, fd_in );
			sscanf( line1, "%lf %lf %lf", &x, &y, &z );
			VSET( plane, x, y, z );
			fgets( line1, MAX_LINE_LEN, fd_in );
			sscanf( line1, "%lf %lf", &u_min, &v_min );
			fgets( line1, MAX_LINE_LEN, fd_in );
			sscanf( line1, "%lf %lf", &u_max, &v_max );
			fgets( line1, MAX_LINE_LEN, fd_in );
			sscanf( line1, "%d", &orient );

			plane[H] = VDOT( plane, origin );

			VJOIN2( arb_pt[0], origin, u_min, e1, v_min, e2 );
			VJOIN2( arb_pt[1], origin, u_max, e1, v_min, e2 );
			VJOIN2( arb_pt[2], origin, u_max, e1, v_max, e2 );
			VJOIN2( arb_pt[3], origin, u_min, e1, v_max, e2 );

			/* find max distance to corner of enclosing RPP */
			dist = 0.0;
			VSET( rpp_corner, min[X], min[Y], min[Z] );
			tmp_dist = DIST_PT_PLANE( rpp_corner, plane ) * (fastf_t)orient;
			if( tmp_dist > dist )
				dist = tmp_dist;

			VSET( rpp_corner, min[X], min[Y], max[Z] );
			tmp_dist = DIST_PT_PLANE( rpp_corner, plane ) * (fastf_t)orient;
			if( tmp_dist > dist )
				dist = tmp_dist;

			VSET( rpp_corner, min[X], max[Y], min[Z] );
			tmp_dist = DIST_PT_PLANE( rpp_corner, plane ) * (fastf_t)orient;
			if( tmp_dist > dist )
				dist = tmp_dist;

			VSET( rpp_corner, min[X], max[Y], max[Z] );
			tmp_dist = DIST_PT_PLANE( rpp_corner, plane ) * (fastf_t)orient;
			if( tmp_dist > dist )
				dist = tmp_dist;

			VSET( rpp_corner, max[X], min[Y], min[Z] );
			tmp_dist = DIST_PT_PLANE( rpp_corner, plane ) * (fastf_t)orient;
			if( tmp_dist > dist )
				dist = tmp_dist;

			VSET( rpp_corner, max[X], min[Y], max[Z] );
			tmp_dist = DIST_PT_PLANE( rpp_corner, plane ) * (fastf_t)orient;
			if( tmp_dist > dist )
				dist = tmp_dist;

			VSET( rpp_corner, max[X], max[Y], min[Z] );
			tmp_dist = DIST_PT_PLANE( rpp_corner, plane ) * (fastf_t)orient;
			if( tmp_dist > dist )
				dist = tmp_dist;

			VSET( rpp_corner, max[X], max[Y], max[Z] );
			tmp_dist = DIST_PT_PLANE( rpp_corner, plane ) * (fastf_t)orient;
			if( tmp_dist > dist )
				dist = tmp_dist;

			for( i=0 ; i<4 ; i++ )
			{
				VJOIN1( arb_pt[i+4], arb_pt[i], dist*(fastf_t)orient, plane );
			}

			if( top_level )
			{
				for( i=0 ; i<8 ; i++ )
					VSCALE( arb_pt[i], arb_pt[i], conv_factor )
			}

			cut_count++;

			sprintf( haf_name, "cut.%d", cut_count );
			ptr = Add_new_name( haf_name, (char *)NULL, CUT_SOLID_TYPE );
			if( mk_arb8( fd_out, ptr->solid_name, (fastf_t *)arb_pt ) )
				rt_log( "Failed to create ARB8 solid for Assembly cut in part %s\n", name );
			else
			{
				/* Add this cut to the region */
				wmem = mk_addmember( ptr->solid_name, head,
						WMOP_SUBTRACT );

				if( top_level && do_reorient )
				{
					/* apply re_orient transformation here */
					if( debug )
					{
						rt_log( "Applying re-orient matrix to solid %s\n", ptr->solid_name );
						mat_print( "re-orient matrix", re_orient );
					}
					mat_mul2( re_orient, wmem->wm_mat );
				}
				
			}
		}
		fgets( line1, MAX_LINE_LEN, fd_in );
		(*start) = (-1);
		while( isspace( line1[++(*start)] ) );
	}
}

static void
Convert_part( line )
char line[MAX_LINE_LEN];
{
	char line1[MAX_LINE_LEN];
	char name[80];
	char *obj;
	char *solid_name;
	int start;
	int i;
	int face_count=0;
	struct model *m;
	struct nmgregion *r;
	struct shell *s;
	struct render_verts verts[3];
	struct vertex   **vts[3];
	float colr[3];
	float part_conv_factor;
	unsigned char color[3];
	char *brlcad_name;
	struct wmember head;
	struct wmember *wmem;
	vect_t normal;
	int solid_in_region=0;
	int solid_is_written=0;
	point_t part_max,part_min;	/* Part RPP */

	if( rt_g.debug & DEBUG_MEM_FULL )
		rt_prmem( "At start of Conv_prt():\n" );

	if( rt_g.debug & DEBUG_MEM_FULL )
	{
		rt_log( "Barrier check at start of Convet_part:\n" );
		if( rt_mem_barriercheck() )
			rt_bomb( "Barrier check failed!!!\n" );
	}


	RT_LIST_INIT( &head.l );
	VSETALL( part_min, MAX_FASTF );
	VSETALL( part_max, -MAX_FASTF );

	m = nmg_mm();
	NMG_CK_MODEL( m );
	r = nmg_mrsv( m );
	NMG_CK_REGION( r );
	s = RT_LIST_FIRST( shell , &r->s_hd );
	NMG_CK_SHELL( s );

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

	/* get object id */
	sscanf( &line[start] , "%x" , &obj );

	rt_log( "Converting Part: %s\n" , name );

	if( debug )
		rt_log( "Conv_part %s x%x\n" , name , obj );

	solid_count++;
	solid_name = Get_solid_name( name , obj );

	rt_log( "\tUsing solid name: %s\n" , solid_name );

	if( rt_g.debug & DEBUG_MEM || rt_g.debug & DEBUG_MEM_FULL )
		rt_prmem( "At start of Convert_part()" );

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
					if( top_level )
					{
						x *= conv_factor;
						y *= conv_factor;
						z *= conv_factor;
					}

					if( vert_no > 2 )
					{
						int n;

						rt_log( "Non-triangular loop:\n" );
						for( n=0 ; n<3 ; n++ )
							rt_log( "\t( %g %g %g )\n", V3ARGS( verts[n].pt ) );

						rt_log( "\t( %g %g %g )\n", x, y, z );
					}
					VSET( verts[vert_no].pt , x , y , z );
					VMINMAX( part_min, part_max, verts[vert_no].pt );
					vert_no++;

				}
				else
					rt_log( "Unrecognized line: %s\n", line1 );
			}

			for( i=0 ; i<3 ; i++ )
			{
				verts[i].v = (struct vertex *)NULL;
				vts[i] = &verts[i].v;
			}

			if( debug )
			{
				int n;

				rt_log( "Making Face:\n" );
				for( n=0 ; n<3; n++ )
					rt_log( "\t( %g %g %g )\n" , V3ARGS( verts[n].pt ) );
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
				face_count++;
			else
				(void)nmg_kfu( fu );
		}
		else if( !strncmp( &line1[start], "modifiers", 9 ) )
		{
			if( face_count )
			{
				wmem = mk_addmember( solid_name , &head , WMOP_UNION );
				if( top_level && do_reorient )
				{
					/* apply re_orient transformation here */
					if( debug )
					{
						rt_log( "Applying re-orient matrix to solid %s\n", solid_name );
						mat_print( "re-orient matrix", re_orient );
					}
					mat_mul2( re_orient, wmem->wm_mat );
				}
				solid_in_region = 1;
			}
			do_modifiers( line1, &start, &head, name, part_min, part_max );
		}
	}

	/* Check if this part has any solid parts */
	if( face_count == 0 )
	{
		char *save_name;

		rt_log( "\t%s has no solid parts, ignoring\n" , name );
		save_name = (char *)rt_malloc( NAMESIZE*sizeof( char ), "proe-g: save_name" );
		brlcad_name = Get_unique_name( name , obj , PART_TYPE );
		strncpy( save_name, brlcad_name, NAMESIZE );
		nmg_tbl( &null_parts, TBL_INS, (long *)save_name );
		nmg_km( m );
		return;
	}

	if( !polysolid || (do_simplify && face_count < 175) )
	{
		/* fuse vertices that are within tolerance of each other */
		rt_log( "\tFusing vertices for part\n" );
		(void)nmg_model_vertex_fuse( m , &tol );

		/* Break edges on vertices */
		if( debug )
			rt_log( "\tBreak edges\n" );
		(void)nmg_model_break_e_on_v( m, &tol );

		/* kill zero length edgeuses */
		if( debug )
			rt_log( "\tKill zero length edges\n" );
		if( nmg_kill_zero_length_edgeuses( m ) )
			goto empty_model;

		/* kill cracks */
		if( debug )
			rt_log( "\tKill cracks\n" );
		if( nmg_kill_cracks( s ) )
		{
			if( nmg_ks( s ) )
				goto empty_model;
		}

		nmg_rebound( m , &tol );

		/* Glue faceuses together. */
		if( debug )
			rt_log( "\tEdge fuse\n" );
		(void)nmg_model_edge_fuse( m, &tol );

		if( debug )
			rt_log( "\tJoin touching loops\n" );
		nmg_s_join_touchingloops( s, &tol );

		/* kill cracks */
		if( debug )
			rt_log( "\tKill cracks\n" );
		r = RT_LIST_FIRST( nmgregion, &m->r_hd );
		if( nmg_kill_cracks( s ) )
		{
			if( nmg_ks( s ) )
				goto empty_model;
		}

		if( debug )
			rt_log( "\tSplit touching loops\n" );
		nmg_s_split_touchingloops( s, &tol);

		/* kill cracks */
		if( debug )
			rt_log( "\tKill cracks\n" );
			
		if( nmg_kill_cracks( s ) )
		{
			if( nmg_ks( s ) )
				goto empty_model;
		}

		/* verify face plane calculations */
		if( debug )
			rt_log( "\tMake faces within tolerance\n" );
		nmg_make_faces_within_tol( s, &tol );

		nmg_shell_coplanar_face_merge( s , &tol , 0 );

		nmg_simplify_shell( s );

		nmg_rebound( m , &tol );

	}
	else

		nmg_rebound( m , &tol );

	if( do_simplify && face_count < 165 )
	{
		struct rt_arb_internal arb_int;
		struct rt_tgc_internal tgc_int;

		if( solid_is_written = nmg_to_arb( m, &arb_int ) )
		{
			if( mk_arb8( fd_out, solid_name, (fastf_t *)arb_int.pt ) )
			{
				rt_log( "proe-g: failed to write ARB8 to database for solid %s\n", solid_name );
				rt_bomb( "proe-g: failed to write to database" );
			}
			rt_log( "\t%s written as an ARB\n", solid_name );
		}
/*		else if( solid_is_written = nmg_to_tgc( m, &tgc_int, &tol ) )
		{
			if( mk_tgc( fd_out, solid_name, tgc_int.v, tgc_int.h, tgc_int.a,
					tgc_int.b, tgc_int.c, tgc_int.d ) )
			{
				rt_log( "proe-g: failed to write TGC to database for solid %s\n", solid_name );
				rt_bomb( "proe-g: failed to write to database" );
			}
			rt_log( "\t%s written as a TGC\n", solid_name );
		} */
	}

	if( polysolid && !solid_is_written )
	{
		rt_log( "\tWriting polysolid\n" );
		write_shell_as_polysolid( fd_out , solid_name , s );
	}
	else if( !solid_is_written )
	{
		rt_log( "\tWriting NMG\n" );
		mk_nmg( fd_out , solid_name , m );
	}

	nmg_km( m );

	if( face_count && !solid_in_region )
	{
		wmem = mk_addmember( solid_name , &head , WMOP_UNION );
		if( top_level && do_reorient )
		{
			/* apply re_orient transformation here */
			if( debug )
			{
				rt_log( "Applying re-orient matrix to solid %s\n", solid_name );
				mat_print( "re-orient matrix", re_orient );
			}
			mat_mul2( re_orient, wmem->wm_mat );
		}
	}
	brlcad_name = Get_unique_name( name , obj , PART_TYPE );

	if( do_air )
	{
		rt_log( "\tMaking air region (%s)\n" , brlcad_name );

		mk_lrcomb( fd_out, brlcad_name, &head, 1, (char *)NULL, (char *)NULL,
		color, 0, air_no, 1, 100, 0 );
		air_no++;
	}
	else
	{
		rt_log( "\tMaking region (%s)\n" , brlcad_name );

		mk_lrcomb( fd_out, brlcad_name, &head, 1, (char *)NULL, (char *)NULL,
		color, id_no, 0, 1, 100, 0 );
		id_no++;
	}

	if( rt_g.debug & DEBUG_MEM_FULL )
	{
		rt_log( "Barrier check at end of Convert_part:\n" );
		if( rt_mem_barriercheck() )
			rt_bomb( "Barrier check failed!!!\n" );
	}

	top_level = 0;

	return;

empty_model:
	{
		char *save_name;

		rt_log( "\t%s is empty, ignoring\n" , name );
		save_name = (char *)rt_malloc( NAMESIZE*sizeof( char ), "proe-g: save_name" );
		brlcad_name = Get_unique_name( name , obj , PART_TYPE );
		strncpy( save_name, brlcad_name, NAMESIZE );
		nmg_tbl( &null_parts, TBL_INS, (long *)save_name );
		nmg_km( m );
		return;
	}

}

static void
Convert_input()
{
	char line[ MAX_LINE_LEN ];

	if( !fgets( line, MAX_LINE_LEN, fd_in ) )
		return;

	sscanf( line, "%f", &conv_factor );

	if( !do_reorient )
		conv_factor = 1.0;

	while( fgets( line, MAX_LINE_LEN, fd_in ) )
	{
		if( !strncmp( line , "assembly" , 8 ) )
			Convert_assy( line );
		else if( !strncmp( line , "solid" , 5 ) )
			Convert_part( line );
		else
			rt_log( "Unrecognized line:\n%s\n" , line );
	}
}

static void
Rm_nulls()
{
	struct db_i *dbip;
	int i;	

	dbip = db_open( brlcad_file, "rw" );
	if( dbip == DBI_NULL )
	{
		rt_log( "Cannot db_open %s\n", brlcad_file );
		rt_log( "References to NULL parts not removed\n" );
		return;
	}

	if( debug || NMG_TBL_END( &null_parts )  )
	{
		rt_log( "Deleting references to the following null parts:\n" );
		for( i=0 ; i<NMG_TBL_END( &null_parts ) ; i++ )
		{
			char *save_name;

			save_name = (char *)NMG_TBL_GET( &null_parts, i );
			rt_log( "\t%s\n" , save_name );
		}
	}

	db_scan(dbip, (int (*)())db_diradd, 1);
	for( i=0 ; i<RT_DBNHASH ; i++ )
	{
		struct directory *dp;

		for( dp=dbip->dbi_Head[i] ; dp!=DIR_NULL ; dp=dp->d_forw )
		{
			struct rt_tree_array	*tree_list;
			struct rt_db_internal	intern;
			struct rt_comb_internal	*comb;
			int j;
			int node_count,actual_count;
			int changed=0;

			/* skip solids */
			if( dp->d_flags & DIR_SOLID )
				continue;

top:
			if( rt_db_get_internal( &intern, dp, dbip, (matp_t)NULL ) < 1 )
			{
				bu_log( "Cannot get internal form of combination %s\n", dp->d_namep );
				continue;
			}
			comb = (struct rt_comb_internal *)intern.idb_ptr;
			RT_CK_COMB( comb );
			if( comb->tree && db_ck_v4gift_tree( comb->tree ) < 0 )
			{
				db_non_union_push( comb->tree );
				if( db_ck_v4gift_tree( comb->tree ) < 0 )
				{
					bu_log( "Cannot flatten tree (%s) for editing\n", dp->d_namep );
					continue;
				}
			}
			node_count = db_tree_nleaves( comb->tree );
			if( node_count > 0 )
			{
				tree_list = (struct rt_tree_array *)bu_calloc( node_count,
					sizeof( struct rt_tree_array ), "tree list" );
				actual_count = (struct rt_tree_array *)db_flatten_tree( tree_list, comb->tree, OP_UNION ) - tree_list;
				if( actual_count > node_count )  bu_bomb("Rm_nulls() array overflow!");
				if( actual_count < node_count )  bu_log("WARNING Rm_nulls() array underflow! %d < %d", actual_count, node_count);
			}
			else
			{
				tree_list = (struct rt_tree_array *)NULL;
				actual_count = 0;
			}


			for( j=0; j<actual_count; j++ )
			{
				int k;
				int found=0;

				for( k=0 ; k<NMG_TBL_END( &null_parts ) ; k++ )
				{
					char *save_name;

					save_name = (char *)NMG_TBL_GET( &null_parts, k );
					if( !strncmp( save_name, tree_list[j].tl_tree->tr_l.tl_name, NAMESIZE ) )
					{
						found = 1;
						break;
					}
				}
				if( found )
				{
					/* This is a NULL part, delete the reference */
/*					if( debug ) */
						rt_log( "Deleting reference to null part (%s) from combination %s\n",
							tree_list[j].tl_tree->tr_l.tl_name, dp->d_namep );

					db_free_tree( tree_list[j].tl_tree );

					for( k=j+1 ; k<actual_count ; k++ )
						tree_list[k-1] = tree_list[k]; /* struct copy */

					actual_count--;
					j--;
					changed = 1;
				}
			}

			if( changed )
			{
				union tree *final_tree;
				char name[NAMESIZE+1];
				int flags;

				strncpy( name, dp->d_namep, NAMESIZE );
				flags = dp->d_flags;

				if( actual_count )
					comb->tree = (union tree *)db_mkgift_tree( tree_list, actual_count, (struct db_tree_state *)NULL );
				else
					comb->tree = (union tree *)NULL;

				if( db_delete( dbip, dp ) || db_dirdelete( dbip, dp ) )
				{
					bu_log( "Failed to delete combination (%s)\n", dp->d_namep );
					rt_comb_ifree( &intern );
					continue;
				}
				if( (dp=db_diradd( dbip, name, -1, 0, flags)) == DIR_NULL )
				{
					bu_log( "Could not add modified '%s' to directory\n", dp->d_namep );
					rt_comb_ifree( &intern );
					continue;
				}

				if( rt_db_put_internal( dp, dbip, &intern ) < 0 )
				{
					bu_log( "Unable to write modified combination '%s' to database\n", dp->d_namep );
					rt_comb_ifree( &intern );
					continue;
				}
			}
			bu_free( (char *)tree_list, "tree_list" );
		}
	}
	db_close( dbip );
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
        tol.dist = 0.1;
        tol.dist_sq = tol.dist * tol.dist;
        tol.perp = 1e-3;
        tol.para = 1 - tol.perp;

	nmg_tbl( &null_parts, TBL_INIT, (long *)NULL );

	if( argc < 2 )
	{
		rt_log( usage, argv[0]);
		exit(1);
	}

	/* Get command line arguments. */
	while ((c = getopt(argc, argv, "i:rsdax:X:pu:")) != EOF) {
		switch (c) {
		case 'i':
			id_no = atoi( optarg );
			break;
		case 'd':
			debug = 1;
			break;
		case 'x':
			sscanf( optarg, "%x", &rt_g.debug );
			rt_printb( "librt rt_g.debug", rt_g.debug, DEBUG_FORMAT );
			rt_log("\n");
			break;
		case 'X':
			sscanf( optarg, "%x", &rt_g.NMG_debug );
			rt_printb( "librt rt_g.NMG_debug", rt_g.NMG_debug, NMG_DEBUG_FORMAT );
			rt_log("\n");
			break;
		case 'p':
			polysolid = 1;
			break;
		case 'u':
			do_regex = 1;
			reg_cmp = regcmp( optarg, (char *)0 );
			if( reg_cmp == (char *)NULL )
			{
				rt_log( "proe-g: Bad regular expression (%s)\n", optarg );
				rt_log( usage, argv[0] );
				exit( 1 );
			}
			break;
		case 'a':
			do_air = 1;
			break;
		case 'r':
			do_reorient = 0;
			break;
		case 's':
			do_simplify = 1;
			break;
		default:
			rt_log( usage, argv[0]);
			exit(1);
			break;
		}
	}

	if( (fd_in=fopen( argv[optind], "r")) == NULL )
	{
		rt_log( "Cannot open input file (%s)\n" , argv[optind] );
		perror( argv[0] );
		exit( 1 );
	}
	optind++;
	brlcad_file = argv[optind];
	if( (fd_out=fopen( brlcad_file, "w")) == NULL )
	{
		rt_log( "Cannot open BRL-CAD file (%s)\n" , brlcad_file );
		perror( argv[0] );
		exit( 1 );
	}

	mk_id_units( fd_out , "Conversion from Pro/Engineer" , "in" );

	/* Create re-orient matrix */
	mat_angles( re_orient, 0.0, 90.0, 90.0 );

	Convert_input();

	fclose( fd_in );
	fclose( fd_out );

	/* Remove references to null parts */
	Rm_nulls();
}
