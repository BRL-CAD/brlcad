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
#include <libgen.h>
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
static char *usage="proe-g [-p] [-s] [-d] [-a] [-u reg_exp] [-x rt_debug_flag] [-X nmg_debug_flag] proe_file.brl output.g\n\
	where proe_file.brl is the output from Pro/Engineer's BRL-CAD EXPORT option\n\
	and output.g is the name of a BRL-CAD database file to receive the conversion.\n\
	The -p option is to create polysolids rather than NMG's.\n\
	The -s option is to simplify the objects to ARB's or TGC's where possible.\n\
	The -d option prints additional debugging information.\n\
	The -u option indicates that portions of object names that match the regular expression\n\
		'reg_exp' shouold be ignored.\n\
	The -a option creates BRL-CAD 'air' regions from everything in the model.\n\
	The -r option indicates that the model should not be re-oriented,\n\
		but left in the same orientation as it was in Pro/E.\n\
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

		NAMEMOVE( ptr->brlcad_name, tmp_name );
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

				NAMEMOVE( ptr->brlcad_name, tmp_name );
				sprintf( &tmp_name[suffix_insert] , "_%c" , try_char );
				if( debug )
					rt_log( "\t\tNew name to try is %s\n", tmp_name );
				ptr2 = name_root;
			}
			else
				ptr2 = ptr2->next;
		}

		NAMEMOVE( tmp_name, ptr->brlcad_name );
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

	NAMEMOVE( ptr->solid_name, tmp_name );
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

			NAMEMOVE( ptr->solid_name, tmp_name );
			sprintf( &tmp_name[suffix_insert] , "_%c" , try_char );
			if( debug )
				rt_log( "\t\tNew name to try is %s\n", tmp_name );
			ptr2 = name_root;
		}
		else
			ptr2 = ptr2->next;
	}

	NAMEMOVE( tmp_name, ptr->solid_name );

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

static int
Write_shell_as_tgc( fd, s, solid_name, tol )
FILE *fd;
struct shell *s;
char *solid_name;
CONST struct rt_tol *tol;
{
	struct faceuse *fu;
	struct loopuse *lu;
	struct edgeuse *eu;
	struct faceuse *fu_base=(struct faceuse *)NULL;
	struct faceuse *fu_top=(struct faceuse *)NULL;
	int four_vert_faces=0;
	int many_vert_faces=0;
	int base_vert_count=0;
	int top_vert_count=0;
	int ret=0;
	point_t sum;
	fastf_t vert_count=0.0;
	fastf_t one_over_vert_count;
	point_t base_center;
	fastf_t min_base_r_sq;
	fastf_t max_base_r_sq;
	fastf_t sum_base_r_sq;
	fastf_t ave_base_r_sq;
	fastf_t base_r;
	point_t top_center;
	fastf_t min_top_r_sq;
	fastf_t max_top_r_sq;
	fastf_t sum_top_r_sq;
	fastf_t ave_top_r_sq;
	fastf_t top_r;
	plane_t top_pl;
	plane_t base_pl;
	vect_t height;
	vect_t plv_1,plv_2;
	vect_t a,b,c,d;

	NMG_CK_SHELL( s );
	RT_CK_TOL( tol );

	for( RT_LIST_FOR( fu, faceuse, &s->fu_hd ) )
	{
		int lu_count=0;

		NMG_CK_FACEUSE( fu );
		if( fu->orientation != OT_SAME )
			continue;

		vert_count = 0.0;

		for( RT_LIST_FOR( lu, loopuse, &fu->lu_hd ) )
		{

			NMG_CK_LOOPUSE( lu );

			if( RT_LIST_FIRST_MAGIC( &lu->down_hd ) != NMG_EDGEUSE_MAGIC )
				return( 0 );

			lu_count++;
			if( lu_count > 1 )
				return( 0 );

			for( RT_LIST_FOR( eu, edgeuse, &lu->down_hd ) )
				vert_count++;
		}

		if( vert_count < 4 )
			return( 0 );

		if( vert_count == 4 )
			four_vert_faces++;
		else
		{
			many_vert_faces++;
			if( many_vert_faces > 2 )
				return( 0 );

			if( many_vert_faces == 1 )
			{
				fu_base = fu;
				base_vert_count = vert_count;
				NMG_GET_FU_PLANE( base_pl, fu_base );
			}
			else if( many_vert_faces == 2 )
			{
				fu_top = fu;
				top_vert_count = vert_count;
				NMG_GET_FU_PLANE( top_pl, fu_top );
			}
		}
	}
	if( base_vert_count != top_vert_count )
		return( 0 );
	if( base_vert_count != four_vert_faces )
		return( 0 );

	if( !NEAR_ZERO( 1.0 + VDOT( top_pl, base_pl ), tol->perp ) )
		return( 0 );

	/* This looks like a good candidate,
	 * Calculate center of base and top faces
	 */

	vert_count = 0.0;
	VSETALL( sum, 0.0 );
	lu = RT_LIST_FIRST( loopuse, &fu_base->lu_hd );
	for( RT_LIST_FOR( eu, edgeuse, &lu->down_hd ) )
	{
		struct vertex_g *vg;

		NMG_CK_EDGEUSE( eu );

		NMG_CK_VERTEXUSE( eu->vu_p );
		NMG_CK_VERTEX( eu->vu_p->v_p );
		vg = eu->vu_p->v_p->vg_p;
		NMG_CK_VERTEX_G( vg );

		VADD2( sum, sum, vg->coord );
		vert_count++;
	}

	one_over_vert_count = 1.0/vert_count;
	VSCALE( base_center, sum, one_over_vert_count );

	/* Calculate Average Radius */
	min_base_r_sq = MAX_FASTF;
	max_base_r_sq = (-min_base_r_sq);
	sum_base_r_sq = 0.0;
	lu = RT_LIST_FIRST( loopuse, &fu_base->lu_hd );
	for( RT_LIST_FOR( eu, edgeuse, &lu->down_hd ) )
	{
		struct vertex_g *vg;
		vect_t rad_vect;
		fastf_t r_sq;

		vg = eu->vu_p->v_p->vg_p;

		VSUB2( rad_vect, vg->coord, base_center );
		r_sq = MAGSQ( rad_vect );
		if( r_sq > max_base_r_sq )
			max_base_r_sq = r_sq;
		if( r_sq < min_base_r_sq )
			min_base_r_sq = r_sq;

		sum_base_r_sq += r_sq;
	}

	ave_base_r_sq = sum_base_r_sq/vert_count;

	base_r = sqrt( max_base_r_sq );

	if( !NEAR_ZERO( (max_base_r_sq - ave_base_r_sq)/ave_base_r_sq, 0.001 ) ||
	    !NEAR_ZERO( (min_base_r_sq - ave_base_r_sq)/ave_base_r_sq, 0.001 ) )
			return( 0 );

	VSETALL( sum, 0.0 );
	lu = RT_LIST_FIRST( loopuse, &fu_top->lu_hd );
	for( RT_LIST_FOR( eu, edgeuse, &lu->down_hd ) )
	{
		struct vertex_g *vg;

		NMG_CK_EDGEUSE( eu );

		NMG_CK_VERTEXUSE( eu->vu_p );
		NMG_CK_VERTEX( eu->vu_p->v_p );
		vg = eu->vu_p->v_p->vg_p;
		NMG_CK_VERTEX_G( vg );

		VADD2( sum, sum, vg->coord );
	}

	VSCALE( top_center, sum, one_over_vert_count );

	/* Calculate Average Radius */
	min_top_r_sq = MAX_FASTF;
	max_top_r_sq = (-min_top_r_sq);
	sum_top_r_sq = 0.0;
	lu = RT_LIST_FIRST( loopuse, &fu_top->lu_hd );
	for( RT_LIST_FOR( eu, edgeuse, &lu->down_hd ) )
	{
		struct vertex_g *vg;
		vect_t rad_vect;
		fastf_t r_sq;

		vg = eu->vu_p->v_p->vg_p;

		VSUB2( rad_vect, vg->coord, top_center );
		r_sq = MAGSQ( rad_vect );
		if( r_sq > max_top_r_sq )
			max_top_r_sq = r_sq;
		if( r_sq < min_top_r_sq )
			min_top_r_sq = r_sq;

		sum_top_r_sq += r_sq;
	}

	ave_top_r_sq = sum_top_r_sq/vert_count;
	top_r = sqrt( max_top_r_sq );

	if( !NEAR_ZERO( (max_top_r_sq - ave_top_r_sq)/ave_top_r_sq, 0.001 ) ||
	    !NEAR_ZERO( (min_top_r_sq - ave_top_r_sq)/ave_top_r_sq, 0.001 ) )
			return( 0 );


	VSUB2( height, top_center, base_center );

	mat_vec_perp( plv_1, top_pl );
	VCROSS( plv_2, top_pl, plv_1 );
	VUNITIZE( plv_1 );
	VUNITIZE( plv_2 );
	VSCALE( a, plv_1, base_r );
	VSCALE( b, plv_2, base_r );
	VSCALE( c, plv_1, top_r );
	VSCALE( d, plv_2, top_r );

	if( (ret=mk_tgc( fd, solid_name, base_center, height, a, b, c, d )) )
	{
		rt_log( "Write_shell_as_tgc: Failed to make TGC for %s (%d)\n", solid_name, ret );
		rt_bomb( "Write_shell_as_tgc failed\n" );
	}

	return( 1 );
}

static int
Shell_is_arb( s, tab )
struct shell *s;
struct nmg_ptbl *tab;
{
	struct faceuse *fu;
	struct face *f;
	int arb;
	int four_verts=0;
	int three_verts=0;
	int face_count=0;
	int loop_count;

	NMG_CK_SHELL( s );

	nmg_vertex_tabulate( tab, &s->l.magic );

	if( NMG_TBL_END( tab ) > 8 || NMG_TBL_END( tab ) < 4 )
		goto not_arb;

	for( RT_LIST_FOR( fu, faceuse, &s->fu_hd ) )
	{
		struct loopuse *lu;
		vect_t fu_norm;

		NMG_CK_FACEUSE( fu );

		if( fu->orientation != OT_SAME )
			continue;

		f = fu->f_p;
		NMG_CK_FACE( f );

		if( *f->g.magic_p != NMG_FACE_G_PLANE_MAGIC )
			goto not_arb;

		NMG_GET_FU_NORMAL( fu_norm, fu );

		loop_count = 0;
		for( RT_LIST_FOR( lu, loopuse, &fu->lu_hd ) )
		{
			struct edgeuse *eu;

			NMG_CK_LOOPUSE( lu );

			if( RT_LIST_FIRST_MAGIC( &lu->down_hd ) != NMG_EDGEUSE_MAGIC )
				goto not_arb;

			loop_count++;

			if( loop_count > 1 )
				goto not_arb;

			for( RT_LIST_FOR( eu, edgeuse, &lu->down_hd ) )
			{
				struct edgeuse *eu_radial;
				struct faceuse *fu_radial;
				struct face *f_radial;
				vect_t norm_radial;
				vect_t eu_dir;
				vect_t cross;

				NMG_CK_EDGEUSE( eu );

				eu_radial = nmg_next_radial_eu( eu, s, 0 );

				if( eu_radial == eu || eu_radial == eu->eumate_p )
					goto not_arb;

				fu_radial = nmg_find_fu_of_eu( eu_radial );
				NMG_CK_FACEUSE( fu_radial );

				if( fu_radial->orientation != OT_SAME )
					fu_radial = fu_radial->fumate_p;

				f_radial = fu_radial->f_p;
				NMG_CK_FACE( f_radial );

				if( *f_radial->g.magic_p != NMG_FACE_G_PLANE_MAGIC )
					goto not_arb;

				NMG_GET_FU_NORMAL( norm_radial, fu_radial );

				VCROSS( cross, fu_norm, norm_radial );

				if( eu->orientation == OT_NONE )
				{
					VSUB2( eu_dir, eu->vu_p->v_p->vg_p->coord, eu->eumate_p->vu_p->v_p->vg_p->coord )
					if( eu->orientation != OT_SAME )
						VREVERSE( eu_dir, eu_dir )
				}
				else
					VMOVE( eu_dir, eu->g.lseg_p->e_dir )

				if( eu->orientation == OT_SAME || eu->orientation == OT_NONE )
				{
					if( VDOT( cross, eu_dir ) < 0.0 )
						goto not_arb;
				}
				else
				{
					if( VDOT( cross, eu_dir ) > 0.0 )
						goto not_arb;
				}
			}
		}
	}

	/* count face types */
	for( RT_LIST_FOR( fu, faceuse, &s->fu_hd ) )
	{
		struct loopuse *lu;
		int vert_count=0;

		if( fu->orientation != OT_SAME )
			continue;

		face_count++;
		for( RT_LIST_FOR( lu, loopuse, &fu->lu_hd ) )
		{
			struct edgeuse *eu;

			for( RT_LIST_FOR( eu, edgeuse, &lu->down_hd ) )
				vert_count++;
		}

		if( vert_count == 3 )
			three_verts++;
		else if( vert_count == 4 )
			four_verts++;
	}

	if( face_count != three_verts + four_verts )
		goto not_arb;

	switch( NMG_TBL_END( tab ) )
	{
		case 4:		/* each face must have 3 vertices */
			if( three_verts != 4 || four_verts != 0 )
				goto not_arb;
			break;
		case 5:		/* one face with 4 verts, four with 3 verts */
			if( four_verts != 1 || three_verts != 4 )
				goto not_arb;
			break;
		case 6:		/* three faces with 4 verts, two with 3 verts */
			if( three_verts != 2 || four_verts != 3 )
				goto not_arb;
			break;
		case 7:		/* four faces with 4 verts, two with 3 verts */
			if( four_verts != 4 || three_verts != 2 )
				goto not_arb;
			break;
		case 8:		/* each face must have 4 vertices */
			if( four_verts != 6 || three_verts != 0 )
				goto not_arb;
			break;
	}

	return( NMG_TBL_END( tab ) );


not_arb:
	nmg_tbl( tab, TBL_FREE, (long *)NULL );
	return( 0 );
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

static int
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
		NAMEMOVE( brlcad_name, save_name );
		nmg_tbl( &null_parts, TBL_INS, (long *)save_name );
		nmg_km( m );
		return;
	}

	if( !polysolid || (do_simplify && face_count < 165) )
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
		struct vertex *v;
		struct nmg_ptbl tab;
		int j;
		point_t arb_pts[8];
		struct faceuse *fu;
		struct loopuse *lu;
		struct edgeuse *eu;
		struct faceuse *fu1;
		struct faceuse *fu2;
		struct edgeuse *eu_start;
		int face_verts;
		int found;

		switch( Shell_is_arb( s, &tab ) )
		{
			case 0:
				break;
			case 4:
				v = (struct vertex *)NMG_TBL_GET( &tab, 0 );
				NMG_CK_VERTEX( v );
				VMOVE( arb_pts[0], v->vg_p->coord );
				v = (struct vertex *)NMG_TBL_GET( &tab, 1 );
				NMG_CK_VERTEX( v );
				VMOVE( arb_pts[1], v->vg_p->coord );
				v = (struct vertex *)NMG_TBL_GET( &tab, 2 );
				NMG_CK_VERTEX( v );
				VMOVE( arb_pts[2], v->vg_p->coord );
				VMOVE( arb_pts[3], v->vg_p->coord );
				v = (struct vertex *)NMG_TBL_GET( &tab, 3 );
				NMG_CK_VERTEX( v );
				VMOVE( arb_pts[4], v->vg_p->coord );
				VMOVE( arb_pts[5], v->vg_p->coord );
				VMOVE( arb_pts[6], v->vg_p->coord );
				VMOVE( arb_pts[7], v->vg_p->coord );
				if( mk_arb8( fd_out, solid_name, (fastf_t *)arb_pts ) )
				{
					rt_log( "mk_arb8 failed for part %s\n", name );
					rt_bomb( "mk_arb8 failed" );
				}
				solid_is_written = 1;
				nmg_tbl( &tab, TBL_FREE, (long *)NULL );
				break;
			case 5:
				fu = RT_LIST_FIRST( faceuse, &s->fu_hd );
				face_verts = 0;
				while( face_verts != 4 )
				{
					face_verts = 0;
					fu = RT_LIST_PNEXT_CIRC( faceuse, &fu->l );
					lu = RT_LIST_FIRST( loopuse, &fu->lu_hd );
					for( RT_LIST_FOR( eu, edgeuse, &lu->down_hd ) )
						face_verts++;
				}
				NMG_CK_FACEUSE( fu );
				if( fu->orientation != OT_SAME )
					fu = fu->fumate_p;
				NMG_CK_FACEUSE( fu );

				lu = RT_LIST_FIRST( loopuse, &fu->lu_hd );
				j = 0;
				eu_start = RT_LIST_FIRST( edgeuse, &lu->down_hd );
				for( RT_LIST_FOR( eu, edgeuse, &lu->down_hd ) )
				{
					VMOVE( arb_pts[j], eu->vu_p->v_p->vg_p->coord )
					j++;
				}

				eu = eu_start->radial_p;
				eu = RT_LIST_PNEXT_CIRC( edgeuse, &eu->l );
				eu = eu->eumate_p;
				for( i=0 ; i<4 ; i++ )
				{
					VMOVE( arb_pts[j], eu->vu_p->v_p->vg_p->coord )
					j++;
				}
				if( mk_arb8( fd_out, solid_name, (fastf_t *)arb_pts ) )
				{
					rt_log( "mk_arb8 failed for part %s\n", name );
					rt_bomb( "mk_arb8 failed" );
				}
				solid_is_written = 1;
				nmg_tbl( &tab, TBL_FREE, (long *)NULL );
				break;
			case 6:
				fu = RT_LIST_FIRST( faceuse, &s->fu_hd );
				face_verts = 0;
				while( face_verts != 3 )
				{
					face_verts = 0;
					fu = RT_LIST_PNEXT_CIRC( faceuse, &fu->l );
					lu = RT_LIST_FIRST( loopuse, &fu->lu_hd );
					for( RT_LIST_FOR( eu, edgeuse, &lu->down_hd ) )
						face_verts++;
				}
				NMG_CK_FACEUSE( fu );
				if( fu->orientation != OT_SAME )
					fu = fu->fumate_p;
				NMG_CK_FACEUSE( fu );

				lu = RT_LIST_FIRST( loopuse, &fu->lu_hd );

				eu_start = RT_LIST_FIRST( edgeuse, &lu->down_hd );
				eu = eu_start;
				VMOVE( arb_pts[1], eu->vu_p->v_p->vg_p->coord );
				eu = RT_LIST_PNEXT_CIRC( edgeuse, &eu->l );
				VMOVE( arb_pts[0], eu->vu_p->v_p->vg_p->coord );
				eu = RT_LIST_PNEXT_CIRC( edgeuse, &eu->l );
				VMOVE( arb_pts[4], eu->vu_p->v_p->vg_p->coord );
				VMOVE( arb_pts[5], eu->vu_p->v_p->vg_p->coord );

				eu = eu_start->radial_p;
				eu = RT_LIST_PNEXT_CIRC( edgeuse, &eu->l );
				eu = RT_LIST_PNEXT_CIRC( edgeuse, &eu->l );
				eu = eu->radial_p->eumate_p;
				VMOVE( arb_pts[2], eu->vu_p->v_p->vg_p->coord );
				eu = RT_LIST_PNEXT_CIRC( edgeuse, &eu->l );
				VMOVE( arb_pts[3], eu->vu_p->v_p->vg_p->coord );
				eu = RT_LIST_PNEXT_CIRC( edgeuse, &eu->l );
				VMOVE( arb_pts[6], eu->vu_p->v_p->vg_p->coord );
				VMOVE( arb_pts[7], eu->vu_p->v_p->vg_p->coord );

				if( mk_arb8( fd_out, solid_name, (fastf_t *)arb_pts ) )
				{
					rt_log( "mk_arb8 failed for part %s\n", name );
					rt_bomb( "mk_arb8 failed" );
				}
				solid_is_written = 1;
				nmg_tbl( &tab, TBL_FREE, (long *)NULL );
				break;
			case 7:
				found = 0;
				fu = RT_LIST_FIRST( faceuse, &s->fu_hd );
				while( !found )
				{
					int verts4=0,verts3=0;

					lu = RT_LIST_FIRST( loopuse, &fu->lu_hd );
					for( RT_LIST_FOR( eu, edgeuse, &lu->down_hd ) )
					{
						struct loopuse *lu1;
						struct edgeuse *eu1;
						int vert_count=0;

						fu1 = nmg_find_fu_of_eu( eu->radial_p );
						lu1 = RT_LIST_FIRST( loopuse, &fu1->lu_hd );
						for( RT_LIST_FOR( eu1, edgeuse, &lu1->down_hd ) )
							vert_count++;

						if( vert_count == 4 )
							verts4++;
						else if( vert_count == 3 )
							verts3++;
					}

					if( verts4 == 2 && verts3 == 2 )
						found = 1;
				}
				if( fu->orientation != OT_SAME )
					fu = fu->fumate_p;
				NMG_CK_FACEUSE( fu );

				lu = RT_LIST_FIRST( loopuse, &fu->lu_hd );
				j = 0;
				eu_start = RT_LIST_FIRST( edgeuse, &lu->down_hd );
				for( RT_LIST_FOR( eu, edgeuse, &lu->down_hd ) )
				{
					VMOVE( arb_pts[j], eu->vu_p->v_p->vg_p->coord )
					j++;
				}

				eu = eu_start->radial_p;
				eu = RT_LIST_PNEXT_CIRC( edgeuse, &eu->l );
				eu = RT_LIST_PNEXT_CIRC( edgeuse, &eu->l );
				eu = eu->radial_p->eumate_p;
				fu1 = nmg_find_fu_of_eu( eu );
				if( nmg_faces_are_radial( fu, fu1 ) )
				{
					eu = eu_start->radial_p;
					eu = RT_LIST_PNEXT_CIRC( edgeuse, &eu->l );
					eu = eu->radial_p->eumate_p;
				}
				for( i=0 ; i<4 ; i++ )
				{
					VMOVE( arb_pts[j], eu->vu_p->v_p->vg_p->coord )
					j++;
					eu = RT_LIST_PNEXT_CIRC( edgeuse, &eu->l );
				}
				if( mk_arb8( fd_out, solid_name, (fastf_t *)arb_pts ) )
				{
					rt_log( "mk_arb8 failed for part %s\n", name );
					rt_bomb( "mk_arb8 failed" );
				}
				solid_is_written = 1;
				nmg_tbl( &tab, TBL_FREE, (long *)NULL );
				break;
			case 8:
				fu = RT_LIST_FIRST( faceuse, &s->fu_hd );
				NMG_CK_FACEUSE( fu );
				if( fu->orientation != OT_SAME )
					fu = fu->fumate_p;
				NMG_CK_FACEUSE( fu );

				lu = RT_LIST_FIRST( loopuse, &fu->lu_hd );
				j = 0;
				eu_start = RT_LIST_FIRST( edgeuse, &lu->down_hd );
				for( RT_LIST_FOR( eu, edgeuse, &lu->down_hd ) )
				{
					VMOVE( arb_pts[j], eu->vu_p->v_p->vg_p->coord )
					j++;
				}

				eu = eu_start->radial_p;
				eu = RT_LIST_PNEXT_CIRC( edgeuse, &eu->l );
				eu = RT_LIST_PNEXT_CIRC( edgeuse, &eu->l );
				eu = eu->radial_p->eumate_p;
				for( i=0 ; i<4 ; i++ )
				{
					VMOVE( arb_pts[j], eu->vu_p->v_p->vg_p->coord )
					j++;
					eu = RT_LIST_PNEXT_CIRC( edgeuse, &eu->l );
				}
				if( mk_arb8( fd_out, solid_name, (fastf_t *)arb_pts ) )
				{
					rt_log( "mk_arb8 failed for part %s\n", name );
					rt_bomb( "mk_arb8 failed" );
				}
				solid_is_written = 1;
				nmg_tbl( &tab, TBL_FREE, (long *)NULL );
				break;
			default:
				fprintf( stderr, "Shell_is_arb returned illegal value for part %s\n", name );
				rt_bomb( "Shell_is_arb screwed up" );
				
		}
		if( solid_is_written )
			rt_log( "\t%s written as an ARB\n", solid_name );
	}

	if( do_simplify && !solid_is_written )
	{
		solid_is_written = Write_shell_as_tgc( fd_out, s, solid_name, &tol );
		if( solid_is_written )
			rt_log( "\t%s written as a TGC\n", solid_name );
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
		NAMEMOVE( brlcad_name, save_name );
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

	if( debug )
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
			union record *rp;
			int j;

			/* skip solids */
			if( dp->d_flags & DIR_SOLID )
				continue;

top:
			if( (rp=db_getmrec( dbip , dp )) == (union record *)0 )
			{
				rt_log( "Cannot get records for combination %s\n" , dp->d_namep );
				continue;
			}

			for( j=1; j<dp->d_len; j++ )
			{
				int k;
				int found=0;

				for( k=0 ; k<NMG_TBL_END( &null_parts ) ; k++ )
				{
					char *save_name;

					save_name = (char *)NMG_TBL_GET( &null_parts, k );
					if( !strncmp( save_name, rp[j].M.m_instname, NAMESIZE ) )
					{
						found = 1;
						break;
					}
				}
				if( found )
				{
					/* This is a NULL part, delete the reference */
					if( debug )
						rt_log( "Deleting reference to null part (%s) from combination %s\n",
							rp[j].M.m_instname, dp->d_namep );
					if( db_delrec( dbip, dp, j ) < 0 )
					{
						rt_log( "Error in deleting reference to null part (%s)\n", rp[j].M.m_instname );
						rt_log( "Your database should still be O.K., but there may be\n" );
						rt_log( "references to NULL parts (just a nuisance)\n" );
						exit( 1 );
					}
					rt_free( (char *)rp, "Rm_nulls: rp" );
					goto top;
				}
			}

			rt_free( (char *)rp, "Rm_nulls: rp" );
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
        tol.dist = 0.005;
        tol.dist_sq = tol.dist * tol.dist;
        tol.perp = 1e-6;
        tol.para = 1 - tol.perp;

	nmg_tbl( &null_parts, TBL_INIT, (long *)NULL );

	if( argc < 2 )
	{
		rt_log( usage, argv[0]);
		exit(1);
	}

	/* Get command line arguments. */
	while ((c = getopt(argc, argv, "rsdax:X:pu:")) != EOF) {
		switch (c) {
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
