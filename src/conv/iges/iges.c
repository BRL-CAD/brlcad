/*                          I G E S . C
 * BRL-CAD
 *
 * Copyright (c) 1993-2006 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this file; see the file named COPYING for more
 * information.
 */
/** @file iges.c
 *
 *  Code to support the g-iges converter
 *
 *  Author -
 *	John R. Anderson
 *
 *  Source -
 *	The U. S. Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066
 *
 */

/*	Yet to do:
 *

 *		Utilize the IGES Right Angle Wedge entity (may not be worth the effort):
 *			1. arb_is_raw()
 *			2. raw_to_iges()
 *			3. modify arb_to_iges to use above
 *
 *		How to handle half-space solids????
 *
 *		How to handle xforms with scale factors?????
 */

#ifndef lint
static const char RCSid[] = "$Header$";
#endif

#include "common.h"



#include <stdio.h>
#include <math.h>
#ifdef HAVE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif
#include <ctype.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "machine.h"
#include "vmath.h"
#include "rtlist.h"
#include "rtstring.h"
#include "nmg.h"
#include "rtgeom.h"
#include "raytrace.h"
#include "./iges.h"
#include "../../librt/debug.h"

/* define defaulted entry for directory entry array */
#define	DEFAULT	(-99999)

extern int	verbose;
static int	solids_to_nmg=0;/* Count of solids that were converted to nmg's in CSG mode */
static int	dir_seq=0;	/* IGES file directory section sequence number */
static int	param_seq=0;	/* IGES file parameter section sequence number */
static int	start_len=0;	/* Length of Start Section */
static int	global_len=0;	/* Length of Global Section */
static int	attribute_de;	/* DE of attribute definition entity */
static char	*global_form="%-72.72s%c%07d\n"; /* format for global section */
static char	*param_form="%-64.64s %7d%c%07d\n"; /* format for parameter section */
static char	*att_string="BRLCAD attribute definition:material name,material parameters,region flag,ident number,air code,material code (GIFT),los density,inheritance";
static struct bn_tol tol;	/* tolerances */
static struct rt_tess_tol ttol;	/* tolerances */
static struct db_i *dbip=NULL;
static char	*unknown="Unknown";
static int	unknown_count=0;
static int	de_pointer_number;
extern char	**independent;
extern int	no_of_indeps;
extern int	solid_is_brep;
extern int	comb_form;
extern int	do_nurbs;
extern int	mode;

BU_EXTERN( int write_freeform, ( FILE *fp, char s[], int de, char c ) );
BU_EXTERN( int write_dir_entry, ( FILE *fp, int entry[] ) );
BU_EXTERN( int write_vertex_list, ( struct nmgregion *r, struct bu_ptbl *vtab, FILE *fp_dir, FILE *fp_param ) );
BU_EXTERN( int write_edge_list, ( struct nmgregion *r, int vert_de, struct bu_ptbl *etab, struct bu_ptbl *vtab, FILE *fp_dir, FILE *fp_param ) );
BU_EXTERN( int write_shell_face_loop, ( char *name, struct nmgregion *r, int dependent, int edge_de, struct bu_ptbl *etab, int vert_de, struct bu_ptbl *vtab, FILE *fp_dir, FILE *fp_param ) );
BU_EXTERN( int write_solid_assembly, ( char *name, int de_list[], int length, int dependent, FILE *fp_dir, FILE *fp_param ) );
BU_EXTERN( int write_planar_nurb, ( struct faceuse *fu, vect_t u_dir, vect_t v_dir, fastf_t *u_max, fastf_t *v_max, point_t base_pt, FILE *fp_dir, FILE *fp_param ) );
BU_EXTERN( int write_name_entity, ( char *name, FILE *fp_dir, FILE *fp_param ) );
BU_EXTERN( int write_att_entity, ( struct iges_properties *props, FILE *fp_dir, FILE *fp_param ) );
BU_EXTERN( int nmg_to_iges, ( struct rt_db_internal *ip, char *name, FILE *fp_dir, FILE *fp_param ) );

#define		NO_OF_TYPES	31
static int	type_count[NO_OF_TYPES][2]={
			{ 106 , 0 },	/* Copious Data */
			{ 110 , 0 },	/* Line */
			{ 116 , 0 },	/* Point */
			{ 123 , 0 },	/* Direction */
			{ 124 , 0 },	/* Transformation Matrix */
			{ 126 , 0 },	/* Rational B-spline Curve */
			{ 128 , 0 },	/* Rational B-spline Surface */
			{ 142 , 0 },	/* Curve on a Parametric Surface */
			{ 144 , 0 },	/* Trimmed Surface */
			{ 150 , 0 },	/* Block */
			{ 152 , 0 },	/* Right Angle Wedge */
			{ 154 , 0 },	/* Right Circular Cylinder */
			{ 156 , 0 },	/* Right Circular Cone Frustum */
			{ 158 , 0 },	/* Sphere */
			{ 160 , 0 },	/* Torus */
			{ 164 , 0 },	/* Solid of Linear Extrusion */
			{ 168 , 0 },	/* Ellipsoid */
			{ 180 , 0 },	/* Boolean Tree */
			{ 184 , 0 },	/* Solid Assembly */
			{ 186 , 0 },	/* Manifold Solid BREP Object */
			{ 190 , 0 },	/* Plane Surface */
			{ 314 , 0 },	/* Color */
			{ 322 , 0 },	/* Attribute Table Definition */
			{ 406 , 0 },	/* Property Entity */
			{ 422 , 0 },	/* Attribute Table Instance */
			{ 430 , 0 },	/* Solid Instance */
			{ 502 , 0 },	/* Vertex List */
			{ 504 , 0 },	/* Edge List */
			{ 508 , 0 },	/* Loop */
			{ 510 , 0 },	/* Face */
			{ 514 , 0 }	/* Shell */
		};

static char	*type_label[NO_OF_TYPES]={
			"CopiusDa",
			"Line",
			"Point",
			"Directin",
			"Matrix",
			"B-spline",
			"NURB",
			"TrimCurv",
			"TrimSurf",
			"Block",
			"RA Wedge",
			"RC Cylin",
			"RC Frust",
			"Sphere",
			"Torus",
			"Extruson",
			"Ellipsod",
			"BoolTree",
			"Assembly",
			"BREP Obj",
			"PlaneSur",
			"Color",
			"Att Def",
			"Property",
			"Att Inst",
			"SolInst",
			"VertList",
			"EdgeList",
			"Loop",
			"Face",
			"Shell"
		};

static char	*type_name[NO_OF_TYPES]={
			"Copious Data",
			"Line",
			"Point",
			"Direction",
			"Transformation Matrix",
			"Rational B_spline Curve",
			"NURB Surface",
			"Curve on a Parametric Surface",
			"Trimmed Surface",
			"Block",
			"Right Angle Wedge",
			"Right Circular Cylinder",
			"Right Circular Cone Frustum",
			"Sphere",
			"Torus",
			"Linear Sketch Extrusion",
			"Ellipsoid",
			"Boolean Tree",
			"Assembly Primitive",
			"Manifold Boundary Representation",
			"Plane Surface",
			"Color Definition",
			"Attribute Table Definition",
			"Property Entity",
			"Attribute Table Instance",
			"Primitive Instance",
			"Vertex List",
			"Edge List",
			"Loop",
			"Face",
			"Shell"
		};

static unsigned char colortab[9][4]={
	{ 0 , 217 , 217 , 217 }, /* index 0 actually represents an undefined color */
	{ 1 ,   0 ,   0 ,   0 },
	{ 2 , 255 ,   0 ,   0 },
	{ 3 ,   0 , 255 ,   0 },
	{ 4 ,   0 ,   0 , 255 },
	{ 5 , 255 , 255 ,   0 },
	{ 6 , 255 ,   0 , 255 },
	{ 7 ,   0 , 255 , 255 },
	{ 8 , 255 , 255 , 255 }};


void
nmg_to_winged_edge( r )
struct nmgregion *r;
{
	struct shell *s;

	NMG_CK_REGION( r );

	for( BU_LIST_FOR( s , shell , &r->s_hd ) )
	{
		struct faceuse *fu;

		NMG_CK_SHELL( s );
		for( BU_LIST_FOR( fu , faceuse , &s->fu_hd ) )
		{
			struct loopuse *lu;

			NMG_CK_FACEUSE( fu );
			if( fu->orientation != OT_SAME )
				continue;

			for( BU_LIST_FOR( lu , loopuse , &fu->lu_hd ) )
			{
				struct edgeuse *eu1,*eu2;

				NMG_CK_LOOPUSE( lu );
				if( BU_LIST_FIRST_MAGIC( &lu->down_hd ) != NMG_EDGEUSE_MAGIC )
					continue;

				for( BU_LIST_FOR( eu1 , edgeuse , &lu->down_hd ) )
				{
					struct edgeuse *eu_new;
					struct vertex *v1=NULL,*v2=NULL;

					NMG_CK_EDGEUSE( eu1 );
					if( eu1->radial_p == eu1->eumate_p )
						continue;	/* dangling edge (?warning?) */

					if( eu1->radial_p->eumate_p->radial_p == eu1->eumate_p )
						continue;	/* winged edge */

					/* this edge has more than two radial faces
					 * find the other face from this shell
					 */
					eu2 = eu1->radial_p;
					while( eu2 != eu1 && eu2 != eu1->eumate_p
						&& nmg_find_s_of_eu( eu2 ) != s )
							eu2 = eu2->eumate_p->radial_p;

					/* unglue edge eu1 */
					nmg_unglueedge( eu1 );

					/* Make a new edge */
					eu_new = nmg_me( v1 , v2 , s );

					/* Give the endpoints the same coordinates as the originbal edge */
					nmg_vertex_gv( eu_new->vu_p->v_p , eu1->vu_p->v_p->vg_p->coord );
					nmg_vertex_gv( eu_new->eumate_p->vu_p->v_p , eu1->eumate_p->vu_p->v_p->vg_p->coord );

					/* Move edgeuses to the new vertices */
					nmg_movevu( eu1->vu_p , eu_new->vu_p->v_p );
					nmg_movevu( eu1->eumate_p->vu_p , eu_new->eumate_p->vu_p->v_p );

					/* kill the new edge (I only wanted it for its vertices) */
					if( nmg_keu( eu_new ) )
						rt_bomb( "nmg_to_winged_edge: Can't happen nmg_keu resulted in empty shell!!!!\n" );

					/* move the other edgeuse to the same edge */
					if( eu2 == eu1 || eu2 == eu1->eumate_p )
						bu_log( "nmg_to_winged_edge: couldn't find second radial face for eu x%x in shell x%x\n" , eu1 , s );
					else
						nmg_moveeu( eu1 , eu2 );
				}
			}
		}
	}
}

void
get_props( props , comb )
struct iges_properties *props;
struct rt_comb_internal *comb;
{
	char *endp;

	RT_CK_COMB( comb );

	endp = strchr( bu_vls_addr(&comb->shader), ' ' );
	bzero( props->material_name, 32 );
	bzero( props->material_params, 60 );
	if( endp )  {
		int	len;
		len = endp - bu_vls_addr(&comb->shader);
		if( len > 31 ) len = 31;
		strncpy( props->material_name, bu_vls_addr(&comb->shader), len );
		strncpy( props->material_params, endp+1, 59 );
	} else {
		strncpy( props->material_name, bu_vls_addr(&comb->shader), 31 );
		props->material_params[0] = '\0';
	}
	if( comb->region_flag )
	{
		props->region_flag = 'R';
		props->ident = comb->region_id;
		props->air_code = comb->aircode;
		props->material_code = comb->GIFTmater;
		props->los_density = comb->los;
	}
	props->color_defined = ( comb->rgb_valid ? 1 : 0 );
	if( props->color_defined )
	{
		props->color[0] = comb->rgb[0];
		props->color[1] = comb->rgb[1];
		props->color[2] = comb->rgb[2];
	}
	else
	{
		props->color[0] = 0;
		props->color[1] = 0;
		props->color[2] = 0;
	}
	props->inherit = ( comb->inherit ? 1 : 0 );
}

int
lookup_props( props , name )
struct iges_properties *props;
char *name;
{
	struct directory *dp;
	struct rt_db_internal intern;
	struct rt_comb_internal *comb;
	int id;

	strcpy( props->name, name );
	props->material_name[0] = '\0';
	props->material_params[0] = '\0';
	props->region_flag = ' ';
	props->ident = 0;
	props->air_code = 0;
	props->material_code = 0;
	props->los_density = 0;
	props->color[0] = 0;
	props->color[1] = 0;
	props->color[2] = 0;

	if( name == NULL )
		return( 1 );

	dp  = db_lookup( dbip , name , 1 );
	if( dp == DIR_NULL )
		return( 1 );

	if( !(dp->d_flags & DIR_COMB) )
		return( 1 );

	id = rt_db_get_internal( &intern, dp, dbip, (matp_t)NULL , &rt_uniresource);

	if( id < 0 )
	{
		rt_db_free_internal( &intern , &rt_uniresource);
		bu_log( "Could not get internal form of %s\n", dp->d_namep );
		return( 1 );
	}

	if( id != ID_COMBINATION )
	{
		rt_db_free_internal( &intern , &rt_uniresource);
		bu_log( "Directory/Database mismatch!!!! is %s a combination or not???\n", dp->d_namep );
		return( 1 );
	}

	comb = (struct rt_comb_internal *)intern.idb_ptr;
	RT_CK_COMB( comb );

	get_props( props , comb );
	rt_db_free_internal( &intern , &rt_uniresource);
	return( 0 );
}

int
write_color_entity( color , fp_dir , fp_param )
unsigned char color[3];
FILE *fp_dir,*fp_param;
{
	struct bu_vls		str;
	int			dir_entry[21];
	int			i;
	float			c[3];

	bu_vls_init( &str );

	/* initialize directory entry */
	for( i=0 ; i<21 ; i++ )
	        dir_entry[i] = DEFAULT;

	/* convert colors to percents */
	for( i=0 ; i<3 ; i++ )
		c[i] = (float)color[i]/2.55;

	bu_vls_printf( &str , "314,%g,%g,%g;" , c[0] , c[1] , c[2] );

	dir_entry[1] = 314;
	dir_entry[2] = param_seq + 1;
	dir_entry[8] = 0;
	dir_entry[9] = 10201;
	dir_entry[11] = 314;
	dir_entry[15] = 0;
	dir_entry[14] = write_freeform( fp_param , bu_vls_addr( &str ) , dir_seq+1 , 'P' );

	bu_vls_free( &str );

	return( write_dir_entry( fp_dir , dir_entry ) );
}

int
get_color( color , fp_dir , fp_param )
unsigned char color[3];
FILE *fp_dir,*fp_param;
{
	int color_de;

	for( color_de=0 ; color_de < 9 ; color_de++ )
	{
		if( color[0] == colortab[color_de][1] &&
		    color[1] == colortab[color_de][2] &&
		    color[2] == colortab[color_de][3] )
			break;
	}

	if( color_de == 9 )
		color_de = (-write_color_entity( color , fp_dir , fp_param ));

	return( color_de );
}

int
write_attribute_definition( fp_dir , fp_param )
FILE *fp_dir,*fp_param;
{
	struct bu_vls str;
	int dir_entry[21];
	int i;

	bu_vls_init( &str );

	/* initialize directory entry */
	for( i=0 ; i<21 ; i++ )
		dir_entry[i] = DEFAULT;

	/* start with parameter data */
	bu_vls_printf( &str , "322,%dH%s,5001,9" , strlen( att_string ) , att_string );
	bu_vls_printf( &str , ",1,3,1" ); /* material name */
	bu_vls_printf( &str , ",2,3,1" ); /* material parameters */
	bu_vls_printf( &str , ",3,6,1" ); /* region flag (logical value) */
	bu_vls_printf( &str , ",4,1,1" ); /* ident number */
	bu_vls_printf( &str , ",5,1,1" ); /* air code number */
	bu_vls_printf( &str , ",6,1,1" ); /* material code number */
	bu_vls_printf( &str , ",7,1,1" ); /* los density (X100) */
	bu_vls_printf( &str , ",8,1,1" ); /* inheritance */
	bu_vls_printf( &str , ",9,6,1;" ); /* color_defined (logical value) */


	/* remember where parameter data is going */
	dir_entry[2] = param_seq + 1;

	/* get parameter line count */
	dir_entry[14] =  write_freeform( fp_param , bu_vls_addr( &str ) , dir_seq+1 , 'P' );

	/* write directory entry for line entity */
	dir_entry[1] = 322;
	dir_entry[8] = 0;
	dir_entry[9] = 10201;
	dir_entry[11] = 322;
	dir_entry[15] = 0;

	bu_vls_free( &str );

	return( write_dir_entry( fp_dir , dir_entry ) );

}

void
iges_init( set_tol , set_ttol , set_verbose , dbip_set )
struct bn_tol *set_tol;
struct rt_tess_tol *set_ttol;
int set_verbose;
struct db_i *dbip_set;
{
	BN_CK_TOL( set_tol );
	RT_CK_TESS_TOL( set_ttol );
	tol = (*set_tol);
	ttol = (*set_ttol);
	verbose = set_verbose;
	dbip = dbip_set;

	dir_seq = 0;
	param_seq = 0;
	start_len = 0;
	global_len = 0;
}

void
Print_stats( fp )
FILE *fp;
{
	int i;
	int total_entities=0;

	fprintf( fp , "Wrote the following numbers and type of entities:\n" );
	for( i=0 ; i<NO_OF_TYPES ; i++ )
		if( type_count[i][1] > 0 )
		{
			total_entities += type_count[i][1];
			fprintf( fp , "\t%d - %s\n" , type_count[i][1] , type_name[i] );
		}

	fprintf( fp , "Total of %d entities written\n" , total_entities );

	if( solids_to_nmg )
		fprintf( fp , "%d solids converted to NMG's before exporting to IGES\n" , solids_to_nmg );
}

int
write_dir_entry( fp , entry )
FILE *fp;
int entry[];
{
	int i,j,type_index;
	char *label;

	for( type_index=0; type_index<NO_OF_TYPES ; type_index++ )
		if( type_count[type_index][0] == entry[1] )
			break;
	if( type_index == NO_OF_TYPES )
	{
		bu_log( "Writing directory entry for an unknown entity type (%d)\n" , entry[1] );
		label = unknown;
		unknown_count++;
		entry[19] = unknown_count;
	}
	else
	{
		label = type_label[type_index];
		type_count[type_index][1]++;
		entry[19] = type_count[type_index][1];
	}

	entry[10] = dir_seq + 1;
	entry[20] = dir_seq + 2;

	for( j=0 ; j<2 ; j++ )
	{

		for( i=j*10+1 ; i<(j+1)*10 ; i++ )
		{
			if( i == 18 )
				fprintf( fp , "%8.8s" , label );
			else if( entry[i] == DEFAULT )
				fprintf( fp , "        " );
			else if( i == 9 )
				fprintf( fp , "%08d" , entry[i] );
			else
				fprintf( fp , "%8d" , entry[i] );
		}
		fprintf( fp , "D%07d\n" , entry[(j+1)*10] );
	}

	dir_seq += 2;
	return( dir_seq - 1 );
}

int
write_freeform(
FILE *fp,	/* output file */
char s[],	/* the string to be output (must not contain any NL's) */
int de,		/* the directory entry # that this belongs to (ignored for Global section) */
char c)		/* 'G' for global section
		 * 'P' for parameter section
		 * 'S' for start section */

{
	int paramlen;
	int start_seq;
	int str_len;
	int line_start=0;
	int line_end=0;
	int *seq_no;
	int remaining_chars=0;
	int i;

	if( c == 'P' )
	{
		seq_no = &param_seq;
		paramlen = 64;
	}
	else if( c == 'G' )
	{
		seq_no = &global_len;
		paramlen = 72;
	}
	else if( c == 'S' )
	{
		seq_no = &start_len;
		paramlen = 72;
	}
	else
	{
		bu_log( "Bad section character passed to 'write_freeform' (%c)\n" , c );
		exit( 1 );
	}

	str_len = strlen( s );
	start_seq = (*seq_no);


	/* the start section is just one big string, so just print in pieces if necessary */
	if( c == 'S' )
	{
		line_start = 0;
		while( line_start < str_len )
		{
			(*seq_no)++;
			fprintf( fp , global_form , &s[line_start] , c , *seq_no );
			line_start += paramlen;
		}
		return( *seq_no - start_seq );
	}

	/* Just print any string that will fit */
	if( str_len <= paramlen )
	{
		(*seq_no)++;
		if( c == 'P' )
			fprintf( fp , param_form , s , de , c , *seq_no );
		else
			fprintf( fp , global_form , s , c , *seq_no );
		return( *seq_no - start_seq );
	}
	else /* break string into lines */
	{
		int curr_loc=0;
		int field_start=0;

		while( 1 )
		{
			line_end = line_start + paramlen - 1;
			curr_loc = line_start;
			if( line_end >= str_len ) /* write the last line and break */
			{
				(*seq_no)++;
				if( c == 'P' )
					fprintf( fp , param_form , &s[line_start] , de , c , *seq_no );
				else
					fprintf( fp , global_form , &s[line_start] , c , *seq_no );
				break;
			}
			else /* find the end of this line */
			{
				/* cannot extend numbers across lines,
				 * but character strings may. The only way
				 * to be sure is to interpret the entire string */

				if( remaining_chars >= paramlen )
				{
					/* just print more of the string */
					(*seq_no)++;
					if( c == 'P' )
						fprintf( fp , param_form , &s[line_start] , de , c , *seq_no );
					else
						fprintf( fp , global_form , &s[line_start] , c , *seq_no );
					remaining_chars -= paramlen;
				}
				else
				{
					int done=0;

					while( !done )
					{
						char num[81];
						int j=0;

						/* skip over any remainder of a string */
						if( remaining_chars )
						{
							curr_loc = line_start + remaining_chars;
							remaining_chars = 0;
						}
						field_start = curr_loc;

						if( s[curr_loc] == ',' || s[curr_loc] == ';' )
						{
							/* empty field */
							curr_loc++;
						}
						else
						{
							while( isdigit(s[curr_loc]) )
								num[j++] = s[curr_loc++];
							num[j] = '\0';

							if( s[curr_loc] == 'H' )
							{
								/* This is a string */
								int len;

								len = atoi( num );

								/* skip over the 'H' */
								curr_loc++;

								if( curr_loc + len >= line_end )
								{
									/* break this line in a string */
									(*seq_no)++;
									if( c == 'P' )
										fprintf( fp , param_form , &s[line_start] , de , c , *seq_no );
									else
										fprintf( fp , global_form , &s[line_start] , c , *seq_no );
									remaining_chars = curr_loc + len - line_end;
									done = 1;
								}
								else
									curr_loc += len + 1;
							}
							else
							{
								/* this is not a string and cannot be continued to next line */

								/* find end of this field */
								while( curr_loc < str_len && s[curr_loc] != ',' && s[curr_loc] != ';' )
								{
									curr_loc++;
								}

								if( s[curr_loc] == ',' || s[curr_loc] == ';' )
									curr_loc++;

								if( curr_loc > line_end )
								{
									/* end of line must be at start of this field */

									line_end = field_start-1;
									for( i=line_start ; i<=line_end ; i++ )
										fputc( s[i] , fp );

									/* fill out line with blanks */
									for( i=0 ; i<paramlen-(line_end-line_start+1) ; i++ )
										fputc( ' ' , fp );
									if( c == 'P' )
										fprintf( fp , " %7d" , de );
									/* add columns 73 through 80 */
									(*seq_no)++;
									fprintf( fp , "%c%07d\n" , c , *seq_no );
									done = 1;
								}
							}
						}
					}
				}
			}
			line_start = line_end + 1;
		}
		return( *seq_no - start_seq );
	}
}

void
w_start_global(
	FILE *fp_dir,
	FILE *fp_param,
	const char *db_name,
	const char *prog_name,
	const char *output_file,
	const char *id,
	const char *version)
{
	struct bu_vls str;
	time_t now;
	struct tm *timep;
	struct stat db_stat;

	bu_vls_init( &str );

	/* Write Start Section */
	bu_vls_printf( &str , "This IGES file created by %s from the database %s." , prog_name , db_name );
	(void)write_freeform( fp_dir , bu_vls_addr( &str ) , 0 , 'S' );
	bu_vls_free( &str );

	/* Write Global Section */
	bu_vls_printf( &str , ",,%dH%s" , strlen( db_name ), db_name);

	if( output_file == NULL )
		bu_vls_printf( &str , ",7Hstd_out" );
	else
		bu_vls_printf( &str , ",%dH%s" , strlen( output_file ) , output_file );

	bu_vls_printf( &str , ",%dH%s,%dH%s,32,38,6,308,15,%dH%s,1.0,2,2HMM,,1.0" ,
		strlen( version ) , version ,
		strlen( id ) , id,
		strlen( db_name ) , db_name );

	(void)time( &now );
	timep = localtime( &now );
	bu_vls_printf( &str , ",15H%04d%02d%02d.%02d%02d%02d",
		timep->tm_year+1900,
		timep->tm_mon + 1,
		timep->tm_mday,
		timep->tm_hour,
		timep->tm_min,
		timep->tm_sec );

	bu_vls_printf( &str , ",%g,100000.0,7HUnknown,7HUnknown,9,0" ,
		RT_LEN_TOL );

	if( stat( db_name , &db_stat ) )
	{
		bu_log( "Cannot stat %s\n" , db_name );
		perror( prog_name );
		bu_vls_strcat( &str , ",15H00000101.000000;" );
	}
	else
	{
		timep = localtime( &db_stat.st_mtime );
		bu_vls_printf( &str , ",15H%04d%02d%02d.%02d%02d%02d;",
			timep->tm_year+1900,
			timep->tm_mon + 1,
			timep->tm_mday,
			timep->tm_hour,
			timep->tm_min,
			timep->tm_sec );
	}

	(void)write_freeform( fp_dir , bu_vls_addr( &str ) , 0 , 'G' );

	/* write attribute definition entity */

	if( mode != TRIMMED_SURF_MODE )
		attribute_de = write_attribute_definition( fp_dir ,  fp_param );

	bu_vls_free( &str );
}


int
nmgregion_to_iges( name , r , dependent , fp_dir , fp_param )
char *name;
struct nmgregion *r;
int dependent;
FILE *fp_dir,*fp_param;
{
	struct nmgregion	*new_r;		/* temporary nmgregion */
	struct shell		*s_new;		/* shell made by nmg_mrsv */
	struct bu_ptbl		vtab;		/* vertex table */
	struct bu_ptbl		etab;		/* edge table */
	struct bu_ptbl		**shells;	/* array of tables of shells */
	int			*brep_de;	/* Directory entry sequence # for BREP Object(s) */
	int			vert_de;	/* Directory entry sequence # for vertex list */
	int			edge_de;	/* Directory entry sequence # for edge list */
	int			outer_shell_count; /* number of outer shells in nmgregion */
	int			face_count=0;	/* number of faces in nmgregion */
	int			i;

	NMG_CK_REGION( r );

	{
		struct shell *s;
		for( BU_LIST_FOR( s , shell , &r->s_hd ) )
		{
			struct faceuse *fu;

			for( BU_LIST_FOR( fu , faceuse , &s->fu_hd ) )
			{
				struct loopuse *lu;

				if( fu->orientation != OT_SAME )
					continue;

				face_count++;

				for( BU_LIST_FOR( lu , loopuse , &fu->lu_hd ) )
				{
					struct edgeuse *eu;

					if( BU_LIST_FIRST_MAGIC( &lu->down_hd ) != NMG_EDGEUSE_MAGIC )
						continue;

					for( BU_LIST_FOR( eu , edgeuse , &lu->down_hd ) )
					{
						struct vertex *v;

						v = eu->vu_p->v_p;
						NMG_CK_VERTEX( v );
						if( !v->vg_p )
							bu_log( "vertex with no geometry!!\n" );
					}
				}
			}
		}
	}

	if( face_count == 0 )
		return( 0 );

	/* Find outer shells and void shells and their associations */
	outer_shell_count = nmg_find_outer_and_void_shells( r , &shells , &tol );

	brep_de = (int *)bu_calloc( outer_shell_count , sizeof( int ) , "nmgregion_to_iges: brep_de" );

	for( i=0 ; i<outer_shell_count ; i++ )
	{
		int j;
		int tmp_dependent;
		struct shell *s;
		char *tmp_name;

		if( outer_shell_count == 1 )
		{
			new_r = r;
			s_new = NULL;
			tmp_name = name;
			tmp_dependent = dependent;
		}
		else
		{
			new_r = nmg_mrsv( r->m_p );
			s_new = BU_LIST_FIRST( shell , &new_r->s_hd );
			tmp_name = NULL;
			tmp_dependent = 1;

			for( j=BU_PTBL_END( shells[i] )-1 ; j >= 0  ; j-- )
			{
				s = (struct shell *)BU_PTBL_GET( shells[i] , j );
				nmg_mv_shell_to_region( s , new_r );
			}
			(void)nmg_ks( s_new );
		}


		/* Make the vertex list entity */
		vert_de = write_vertex_list( new_r , &vtab , fp_dir , fp_param );

		/* Make the edge list entity */
		edge_de = write_edge_list( new_r , vert_de , &etab , &vtab , fp_dir , fp_param );

		/* Make the face, loop, shell entities */
		brep_de[i] = write_shell_face_loop( tmp_name , new_r , tmp_dependent , edge_de , &etab , vert_de , &vtab , fp_dir , fp_param );

		/* Clear the tables */
		(void)bu_ptbl_reset( &vtab );
		(void)bu_ptbl_reset( &etab );

		if( outer_shell_count != 1 )
			(void)nmg_kr( new_r );
	}

	/* Free the tables */
	(void)bu_ptbl_free( &vtab );
	(void)bu_ptbl_free( &etab );

	if( outer_shell_count != 1 )
		return( write_solid_assembly( name, brep_de , outer_shell_count , dependent , fp_dir , fp_param ) );
	else
		return( brep_de[0] );
}

int
verts_to_copious_data( pts , vert_count , pt_size , fp_dir , fp_param )
point_t *pts;
int vert_count;
int pt_size;
FILE *fp_dir,*fp_param;
{
	struct bu_vls		str;
	int			dir_entry[21];
	int			i;

	if( vert_count < 2 )
		return( 0 );

	bu_vls_init( &str );

	/* initialize directory entry */
	for( i=0 ; i<21 ; i++ )
		dir_entry[i] = DEFAULT;

	bu_vls_printf( &str , "106,%d,%d" , pt_size-1 , vert_count+1 );
	if( pt_size == 2 )
	{
		bu_vls_printf( &str , ",0.0" );
		for( i=0 ; i<vert_count ; i++ )
			bu_vls_printf( &str , ",%f,%f" , pts[i][0] , pts[i][1] );
		bu_vls_printf( &str , ",%f,%f" , pts[0][0] , pts[0][1] );
	}
	else if( pt_size == 3 )
	{
		for( i=0 ; i<vert_count ; i++ )
			bu_vls_printf( &str , ",%f,%f,%f" , V3ARGS( pts[i] ) );
		bu_vls_printf( &str , ",%f,%f,%f" , V3ARGS( pts[0] ) );
	}

	bu_vls_strcat( &str , ";" );

	/* remember where parameter data is going */
	dir_entry[2] = param_seq + 1;

	/* get parameter line count */
	dir_entry[14] = write_freeform( fp_param , bu_vls_addr( &str ), dir_seq+1 , 'P' );

	/* write directory entry for vertex list entity */
	dir_entry[1] = 106;
	dir_entry[8] = 0;
	if( pt_size == 2 )
	{
		dir_entry[9] = 10500;
		dir_entry[15] = 63;
	}
	else
	{
		dir_entry[9] = 10000;
		dir_entry[15] = 12;
	}
	dir_entry[11] = 106;


	bu_vls_free( &str );

	return( write_dir_entry( fp_dir , dir_entry ));
}

int
nmg_loop_to_tcurve( lu , surf_de , u_dir , v_dir , u_max , v_max , base_pt , fp_dir , fp_param )
struct loopuse *lu;
int surf_de;
vect_t u_dir,v_dir;
fastf_t u_max,v_max;
point_t base_pt;
FILE *fp_dir,*fp_param;
{
	struct bu_vls		str;
	int			dir_entry[21];
	struct edgeuse		*eu;
	int			vert_count=0;
	point_t			*model_pts;
	point_t			*param_pts;
	int			i;

	NMG_CK_LOOPUSE( lu );

	if( BU_LIST_FIRST_MAGIC( &lu->down_hd ) != NMG_EDGEUSE_MAGIC )
		return( 0 );

	/* count vertices */
	for( BU_LIST_FOR( eu , edgeuse , &lu->down_hd ) )
		vert_count++;

	if( vert_count < 3 )
		return( 0 );

	/* Allocate memory for points */
	model_pts = (point_t *)bu_calloc( vert_count , sizeof( point_t ) , "nmg_loop_to_tcurve: model_pts" );
	param_pts = (point_t *)bu_calloc( vert_count , sizeof( point_t ) , "nmg_loop_to_tcurve: param_pts" );

	bu_vls_init( &str );

	/* initialize directory entry */
	for( i=0 ; i<21 ; i++ )
		dir_entry[i] = DEFAULT;

	bu_vls_printf( &str , "142,0,%d" , surf_de );

	i = 0;
	for( BU_LIST_FOR( eu , edgeuse , &lu->down_hd ) )
	{
		struct vertex_g *vg;
		vect_t from_base;
		fastf_t u,v;

		NMG_CK_EDGEUSE( eu );
		NMG_CK_VERTEXUSE( eu->vu_p );
		NMG_CK_VERTEX( eu->vu_p->v_p );
		NMG_CK_VERTEX_G( eu->vu_p->v_p->vg_p );
		vg = eu->vu_p->v_p->vg_p;

		if( i >= vert_count )
			rt_bomb( "nmg_loop_to_tcurve: too many vertices in loop!!!!\n" );

		VMOVE( model_pts[i] , vg->coord );

		VSUB2( from_base , vg->coord , base_pt );
		u = VDOT( u_dir , from_base )/u_max;
		v = VDOT( v_dir , from_base )/v_max;
		if( u < 0.0 )
			u = 0.0;
		if( u > 1.0 )
			u = 1.0;
		if( v < 0.0 )
			v = 0.0;
		if( v > 1.0 )
			v = 1.0;
		VSET( param_pts[i] , u , v , 0.0 );

		i++;
	}

	bu_vls_printf( &str , ",%d" , verts_to_copious_data( param_pts , vert_count , 2 , fp_dir , fp_param ) );
	bu_vls_printf( &str , ",%d,0;" , verts_to_copious_data( model_pts , vert_count , 3 , fp_dir , fp_param ));

	/* remember where parameter data is going */
	dir_entry[2] = param_seq + 1;

	/* get parameter line count */
	dir_entry[14] = write_freeform( fp_param , bu_vls_addr( &str ), dir_seq+1 , 'P' );

	/* write directory entry for vertex list entity */
	dir_entry[1] = 142;
	dir_entry[8] = 0;
	dir_entry[9] = 10500;
	dir_entry[11] = 142;
	dir_entry[15] = 0;


	bu_vls_free( &str );

	return( write_dir_entry( fp_dir , dir_entry ));
}

void
nmg_fu_to_tsurf( fu , fp_dir , fp_param )
struct faceuse *fu;
FILE *fp_dir,*fp_param;
{
	struct bu_vls		str;
	int			dir_entry[21];
	struct loopuse		*lu;
	int			surf_de;
	vect_t			u_dir,v_dir;
	fastf_t			u_max,v_max;
	point_t			base_pt;
	int			loop_count=0;
	int			*curve_de;
	int			i;

	NMG_CK_FACEUSE( fu );

	bu_vls_init( &str );

	/* initialize directory entry */
	for( i=0 ; i<21 ; i++ )
		dir_entry[i] = DEFAULT;

	/* count loops */
	for( BU_LIST_FOR( lu , loopuse , &fu->lu_hd ) )
	{
		NMG_CK_LOOPUSE( lu );

		if( BU_LIST_FIRST_MAGIC( &lu->down_hd ) != NMG_EDGEUSE_MAGIC )
			continue;

		loop_count++;
	}

	if( loop_count < 1 )	/* nothing to output */
		return;

	/* write underlying surface */
	surf_de = write_planar_nurb( fu , u_dir , v_dir , &u_max , &v_max , base_pt , fp_dir , fp_param );

	/* allocate space to hold DE for each trimming curve */
	curve_de = (int *)bu_calloc( loop_count , sizeof( int ) , "nmg_fu_to_tsurf: curve_de" );

	i = (-1);
	for( BU_LIST_FOR( lu , loopuse , &fu->lu_hd ) )
	{
		if( BU_LIST_FIRST_MAGIC( &lu->down_hd ) != NMG_EDGEUSE_MAGIC )
			continue;

		curve_de[++i] = nmg_loop_to_tcurve( lu , surf_de , u_dir , v_dir , u_max , v_max , base_pt , fp_dir , fp_param );
	}

	bu_vls_printf( &str , "144,%d,0,%d" , surf_de , loop_count-1 );
	for( i=0 ; i<loop_count ; i++ )
		bu_vls_printf( &str , ",%d" , curve_de[i] );
	bu_vls_strcat( &str , ";" );

	bu_free( (char *)curve_de , "nmg_fu_to_tsurf: curve_de" );

	/* remember where parameter data is going */
	dir_entry[2] = param_seq + 1;

	/* get parameter line count */
	dir_entry[14] = write_freeform( fp_param , bu_vls_addr( &str ), dir_seq+1 , 'P' );

	/* write directory entry for vertex list entity */
	dir_entry[1] = 144;
	dir_entry[8] = 0;
	dir_entry[9] = 0;
	dir_entry[11] = 144;
	dir_entry[15] = 0;

	bu_vls_free( &str );

	(void)write_dir_entry( fp_dir , dir_entry );

}

int nmgregion_to_tsurf( name , r , fp_dir , fp_param )
char *name;
struct nmgregion *r;
FILE *fp_dir,*fp_param;
{
	struct shell *s;

	NMG_CK_REGION( r );

	for( BU_LIST_FOR( s , shell , &r->s_hd ) )
	{
		struct faceuse *fu;

		NMG_CK_SHELL( s );
		for( BU_LIST_FOR( fu , faceuse , &s->fu_hd ) )
		{
			NMG_CK_FACEUSE( fu );

			if( fu->orientation != OT_SAME )
				continue;

			nmg_fu_to_tsurf( fu , fp_dir , fp_param );
		}
	}

	return( -1 );
}

int
write_vertex_list( r , vtab , fp_dir , fp_param )
struct nmgregion *r;
struct bu_ptbl *vtab;   /* vertex table */
FILE *fp_dir,*fp_param;
{
	struct bu_vls		str;
	int			dir_entry[21];
	int			i;

	NMG_CK_REGION( r );

	bu_vls_init( &str );

	/* initialize directory entry */
	for( i=0 ; i<21 ; i++ )
		dir_entry[i] = DEFAULT;

	/* Built list of vertex structs */
	nmg_vertex_tabulate( vtab, &r->l.magic );

	/* write parameter data for vertex list entity */
	bu_vls_printf( &str , "502,%d" , BU_PTBL_END( vtab  ) );

	for( i=0 ; i<BU_PTBL_END( vtab ) ; i++ ) {
		struct vertex                   *v;
		register struct vertex_g        *vg;

		v = (struct vertex *)BU_PTBL_GET(vtab,i);
		NMG_CK_VERTEX(v);
		vg = v->vg_p;
if( !vg )
	bu_log( "No geometry for vertex x%x #%d in table\n" , v , i );
		NMG_CK_VERTEX_G(vg);
		bu_vls_printf( &str, ",%g,%g,%g",
			vg->coord[X],
			vg->coord[Y],
			vg->coord[Z] );
	}
	bu_vls_strcat( &str , ";" );

	/* remember where parameter data is going */
	dir_entry[2] = param_seq + 1;

	/* get parameter line count */
	dir_entry[14] = write_freeform( fp_param , bu_vls_addr( &str ), dir_seq+1 , 'P' );

	/* write directory entry for vertex list entity */
	dir_entry[1] = 502;
	dir_entry[8] = 0;
	dir_entry[9] = 10001;
	dir_entry[11] = 502;
	dir_entry[15] = 1;


	bu_vls_free( &str );

	return( write_dir_entry( fp_dir , dir_entry ));
}

int
write_line_entity( start_vg , end_vg , fp_dir , fp_param )
struct vertex_g	*start_vg;
struct vertex_g	*end_vg;
FILE *fp_dir,*fp_param;
{
	struct bu_vls str;
	int dir_entry[21];
	int i;

	NMG_CK_VERTEX_G( start_vg );
	NMG_CK_VERTEX_G( end_vg );

	bu_vls_init( &str );

	/* initialize directory entry */
	for( i=0 ; i<21 ; i++ )
		dir_entry[i] = DEFAULT;

	/* start with parameter data */
	bu_vls_printf( &str , "110,%g,%g,%g,%g,%g,%g;" ,
			start_vg->coord[X],
			start_vg->coord[Y],
			start_vg->coord[Z],
			end_vg->coord[X],
			end_vg->coord[Y],
			end_vg->coord[Z] );

	/* remember where parameter data is going */
	dir_entry[2] = param_seq + 1;

	/* get parameter line count */
	dir_entry[14] =  write_freeform( fp_param , bu_vls_addr( &str ) , dir_seq+1 , 'P' );

	/* write directory entry for line entity */
	dir_entry[1] = 110;
	dir_entry[8] = 0;
	dir_entry[9] = 10001;
	dir_entry[11] = 110;
	dir_entry[15] = 0;

	bu_vls_free( &str );

	return( write_dir_entry( fp_dir , dir_entry ) );
}

int
write_linear_bspline( start_vg , end_vg , fp_dir , fp_param )
struct vertex_g	*start_vg;
struct vertex_g	*end_vg;
FILE *fp_dir,*fp_param;
{
	struct bu_vls str;
	int dir_entry[21];
	int i;

	NMG_CK_VERTEX_G( start_vg );
	NMG_CK_VERTEX_G( end_vg );

	bu_vls_init( &str );

	/* initialize directory entry */
	for( i=0 ; i<21 ; i++ )
		dir_entry[i] = DEFAULT;

	/* start with parameter data */
	bu_vls_printf( &str , "126,1,1,0,0,1,0,0.,0.,1.,1.,1.,1.,%g,%g,%g,%g,%g,%g,0.,1.;" ,
			start_vg->coord[X],
			start_vg->coord[Y],
			start_vg->coord[Z],
			end_vg->coord[X],
			end_vg->coord[Y],
			end_vg->coord[Z] );

	/* remember where parameter data is going */
	dir_entry[2] = param_seq + 1;

	/* get parameter line count */
	dir_entry[14] =  write_freeform( fp_param , bu_vls_addr( &str ) , dir_seq+1 , 'P' );

	/* write directory entry for line entity */
	dir_entry[1] = 126;
	dir_entry[8] = 0;
	dir_entry[9] = 10001;
	dir_entry[11] = 126;
	dir_entry[15] = 1;

	bu_vls_free( &str );

	return( write_dir_entry( fp_dir , dir_entry ) );
}

int
write_edge_list( r , vert_de , etab , vtab , fp_dir , fp_param )
struct nmgregion *r;
int vert_de;		/* DE# for vertex list */
struct bu_ptbl *etab;	/* edge table */
struct bu_ptbl *vtab;	/* vertex table (already filled in) */
FILE *fp_dir,*fp_param;
{
	struct bu_vls str;
	int dir_entry[21];
	int i;

	NMG_CK_REGION( r );

	bu_vls_init( &str );

	/* initialize directory entry */
	for( i=0 ; i<21 ; i++ )
		dir_entry[i] = DEFAULT;

	/* Build list of edge structures */
	nmg_edge_tabulate( etab , &r->l.magic );

	bu_vls_printf( &str , "504,%d" , BU_PTBL_END( etab ) );

	/* write parameter data for edge list entity */
	for( i=0 ; i<BU_PTBL_END( etab ) ; i++ )
	{
		struct edge			*e;
		struct edgeuse			*eu;
		struct vertexuse		*vu;
		struct vertex			*start_v,*end_v;
		struct vertex_g			*start_vg,*end_vg;
		int				line_de; /* directory entry # for line entity */

		e = (struct edge *)BU_PTBL_GET(etab,i);
		NMG_CK_EDGE(e);
		eu = e->eu_p;
		NMG_CK_EDGEUSE(eu);
		vu = eu->vu_p;
		NMG_CK_VERTEXUSE(vu);
		start_v = vu->v_p;
		NMG_CK_VERTEX(start_v);
		start_vg = start_v->vg_p;
		NMG_CK_VERTEX_G(start_vg);
		vu = eu->eumate_p->vu_p;
		NMG_CK_VERTEXUSE(vu);
		end_v = vu->v_p;
		NMG_CK_VERTEX(end_v);
		end_vg = end_v->vg_p;
		NMG_CK_VERTEX_G(end_vg);
		if( do_nurbs )
			line_de = write_linear_bspline( start_vg , end_vg , fp_dir , fp_param );
		else
			line_de = write_line_entity( start_vg , end_vg , fp_dir , fp_param );
		bu_vls_printf( &str , ",%d,%d,%d,%d,%d",
			line_de,
			vert_de , bu_ptbl_locate( vtab , (long *)start_v ) + 1,
			vert_de , bu_ptbl_locate( vtab , (long *)end_v ) + 1 );
	}
	bu_vls_strcat( &str , ";" );

	/* remember where parameter data is going */
	dir_entry[2] = param_seq + 1;

	/* get parameter line count */
	dir_entry[14] = write_freeform( fp_param , bu_vls_addr( &str ) , dir_seq+1 , 'P' );

	/* write directory entry for edge list entity */
	dir_entry[1] = 504;
	dir_entry[8] = 0;
	dir_entry[9] = 10001;
	dir_entry[11] = 504;
	dir_entry[15] = 1;

	bu_vls_free( &str );

	return( write_dir_entry( fp_dir , dir_entry ) );
}

int
write_point_entity( pt , fp_dir , fp_param )
point_t pt;
FILE *fp_dir,*fp_param;
{
	struct bu_vls		str;
	int			dir_entry[21];
	int			i;

	for( i=0 ; i<21 ; i++ )
		dir_entry[i] = DEFAULT;

	bu_vls_init( &str );

	bu_vls_printf( &str , "116,%g,%g,%g,0;" ,
		pt[X],
		pt[Y],
		pt[Z] );

	/* remember where parameter data is going */
	dir_entry[2] = param_seq + 1;

	/* get parameter line count */
	dir_entry[14] = write_freeform( fp_param , bu_vls_addr( &str ) , dir_seq+1 , 'P' );

	dir_entry[1] = 116;
	dir_entry[8] = 0;
	dir_entry[9] = 10001;
	dir_entry[11] = 116;
	dir_entry[15] = 0;

	bu_vls_free( &str );

	return( write_dir_entry( fp_dir , dir_entry ) );
}

int
write_direction_entity( pt , fp_dir , fp_param )
point_t pt;
FILE *fp_dir,*fp_param;
{
	struct bu_vls		str;
	int			dir_entry[21];
	int			i;

	for( i=0 ; i<21 ; i++ )
		dir_entry[i] = DEFAULT;

	bu_vls_init( &str );

	bu_vls_printf( &str , "123,%g,%g,%g;" ,
		pt[X],
		pt[Y],
		pt[Z] );

	/* remember where parameter data is going */
	dir_entry[2] = param_seq + 1;

	/* get parameter line count */
	dir_entry[14] = write_freeform( fp_param , bu_vls_addr( &str ) , dir_seq+1 , 'P' );

	dir_entry[1] = 123;
	dir_entry[8] = 0;
	dir_entry[9] = 10001;
	dir_entry[11] = 123;
	dir_entry[15] = 0;

	bu_vls_free( &str );

	return( write_dir_entry( fp_dir , dir_entry ) );
}

int
write_plane_entity( plane , fp_dir , fp_param )
plane_t plane;
FILE *fp_dir,*fp_param;
{
	struct bu_vls	str;
	point_t		pt_on_plane;	/* a point on the plane */
	int		dir_entry[21];
	int		i;

	for( i=0 ; i<21 ; i++ )
		dir_entry[i] = DEFAULT;

	bu_vls_init( &str );

	VSCALE( pt_on_plane , plane , plane[3] );

	bu_vls_printf( &str , "190,%d,%d;" ,
		write_point_entity( pt_on_plane , fp_dir , fp_param ),
		write_direction_entity( plane , fp_dir , fp_param ) );

	/* remember where parameter data is going */
	dir_entry[2] = param_seq + 1;

	/* get parameter line count */
	dir_entry[14] = write_freeform( fp_param , bu_vls_addr( &str ) , dir_seq+1 , 'P' );

	dir_entry[1] = 190;
	dir_entry[8] = 0;
	dir_entry[9] = 10001;
	dir_entry[11] = 190;
	dir_entry[15] = 0;

	bu_vls_free( &str );

	return( write_dir_entry( fp_dir , dir_entry ) );

}

int
write_planar_nurb( fu , u_dir , v_dir , u_max , v_max , base_pt , fp_dir , fp_param )
struct faceuse *fu;
vect_t u_dir,v_dir;	/* output */
fastf_t *u_max,*v_max;	/* output */
point_t base_pt;	/* output ( point at u,v==0 ) */
FILE *fp_dir;
FILE *fp_param;
{
	struct loopuse		*lu;
	struct edgeuse		*eu,*eu_next;
	struct vertex_g		*vg,*vg_next;
	point_t			ctl_pt;
	fastf_t			umin,umax,vmin,vmax;
	struct bu_vls   	str;
	int           	 	dir_entry[21];
	int             	i;

	NMG_CK_FACEUSE( fu );

	for( i=0 ; i<21 ; i++ )
		dir_entry[i] = DEFAULT;

	bu_vls_init( &str );

	/* create direction vectors in u and v directions in plane */
	lu = BU_LIST_FIRST( loopuse , &fu->lu_hd );
	NMG_CK_LOOPUSE( lu );
	while( BU_LIST_FIRST_MAGIC( &lu->down_hd ) != NMG_EDGEUSE_MAGIC &&
				BU_LIST_NOT_HEAD( lu , &fu->lu_hd ) )
	{
		lu = BU_LIST_PNEXT( loopuse , lu );
		NMG_CK_LOOPUSE( lu );
	}
	if( BU_LIST_FIRST_MAGIC( &lu->down_hd ) != NMG_EDGEUSE_MAGIC )
	{
		bu_log( "Write_planar_nurb: could not find a loop (with edges) in face\n" );
		return( 0 );
	}

	eu = BU_LIST_FIRST( edgeuse , &lu->down_hd );
	NMG_CK_EDGEUSE( eu );
		vg = eu->vu_p->v_p->vg_p;
	NMG_CK_VERTEX_G( vg );
	eu_next = BU_LIST_PNEXT( edgeuse , eu );
	NMG_CK_EDGEUSE( eu_next );
	vg_next = eu_next->vu_p->v_p->vg_p;
	NMG_CK_VERTEX_G( vg_next );

	VSUB2( u_dir , vg_next->coord , vg->coord );
	VUNITIZE( u_dir );
	VCROSS( v_dir , fu->f_p->g.plane_p->N , u_dir );
	VUNITIZE( v_dir );

	/* find the max and min distances from vg along u_dir and v_dir in the face */
	umin = MAX_FASTF;
	vmin = MAX_FASTF;
	umax = (-umin);
	vmax = (-vmin);

	for( BU_LIST_FOR( lu , loopuse , &fu->lu_hd ) )
	{
		NMG_CK_LOOPUSE( lu );
		if( BU_LIST_FIRST_MAGIC( &lu->down_hd ) != NMG_EDGEUSE_MAGIC )
			continue;

		for( BU_LIST_FOR( eu , edgeuse , &lu->down_hd ) )
		{
			vect_t	tmp_vect;
			fastf_t	distu,distv;

			NMG_CK_EDGEUSE( eu );

			VSUB2( tmp_vect , eu->vu_p->v_p->vg_p->coord , vg->coord );
			distu = VDOT( tmp_vect , u_dir );
			V_MIN( umin , distu );
			V_MAX( umax , distu );
			distv = VDOT( tmp_vect , v_dir );
			V_MIN( vmin , distv );
			V_MAX( vmax , distv );
		}
	}

	*u_max = umax - umin;
	*v_max = vmax - vmin;

	/* Put preliminary stuff for planar nurb in string */
	bu_vls_printf( &str , "128,1,1,1,1,0,0,1,0,0,0.,0.,1.,1.,0.,0.,1.,1.,1.,1.,1.,1." );

	/* Now put control points in string */
	VJOIN2( ctl_pt , vg->coord , umin , u_dir , vmin , v_dir );
	VMOVE( base_pt , ctl_pt );
	bu_vls_printf( &str , ",%g,%g,%g" , V3ARGS( ctl_pt ) );
	VJOIN1( ctl_pt , ctl_pt , umax-umin , u_dir );
	bu_vls_printf( &str , ",%g,%g,%g" , V3ARGS( ctl_pt ) );
	VJOIN2( ctl_pt , vg->coord , umin , u_dir , vmax , v_dir );
	bu_vls_printf( &str , ",%g,%g,%g" , V3ARGS( ctl_pt ) );
	VJOIN1( ctl_pt , ctl_pt , umax-umin , u_dir );
	bu_vls_printf( &str , ",%g,%g,%g" , V3ARGS( ctl_pt ) );

	/* Now put parameter ranges last */
	bu_vls_printf( &str , ",0.,1.,0.,1.;" );

	/* remember where parameter data is going */
	dir_entry[2] = param_seq + 1;

	/* get parameter line count */
	dir_entry[14] = write_freeform( fp_param , bu_vls_addr( &str ) , dir_seq+1 , 'P' );

	dir_entry[1] = 128;
	dir_entry[8] = 0;
	dir_entry[9] = 10001;
	dir_entry[11] = 128;
	dir_entry[15] = 1;

	bu_vls_free( &str );

	return( write_dir_entry( fp_dir , dir_entry ) );

}

int
write_shell_face_loop( name , r , dependent , edge_de , etab , vert_de , vtab , fp_dir , fp_param )
char *name;
struct nmgregion *r;
int dependent;
int edge_de;		/* directory entry # for edge list */
struct bu_ptbl *etab;	/* Table of edge pointers */
int vert_de;		/* directory entry # for vertex list */
struct bu_ptbl *vtab;	/* Table of vertex pointers */
FILE *fp_dir,*fp_param;
{
	struct edgeuse		*eu;
	struct faceuse		*fu;
	struct loopuse		*lu;
	struct shell		*s;
	struct vertex		*v;
	struct bu_vls		str;
	struct iges_properties	props;
	int			*shell_list;
	int			i;
	int			shell_count=0;
	int			dir_entry[21];
	int			name_de;
	int			prop_de;
	int			color_de=DEFAULT;

	NMG_CK_REGION( r );

	bu_vls_init( &str );

	/* count the shells */
	for( BU_LIST_FOR( s , shell , &r->s_hd ) )
		shell_count++;

	/* make space for the list of shell DE's */
	shell_list = (int *)bu_calloc( shell_count , sizeof( int ) , "write_shell_face_loop: shell_list" );

	shell_count = 0;
	for( BU_LIST_FOR( s , shell , &r->s_hd ) )
	{
		int	*face_list;
		int	face_count=0;


		/* Count faces */
		for (BU_LIST_FOR(fu, faceuse, &s->fu_hd))
		{
			NMG_CK_FACEUSE(fu);
			if (fu->orientation != OT_SAME)
				continue;
			face_count++;
		}
		face_list = (int *)bu_calloc( face_count , sizeof( int ) , "face_list" );
		face_count = 0;

		/* Shell is made of faces. */
		for (BU_LIST_FOR(fu, faceuse, &s->fu_hd))
		{
			int	*loop_list;
			int	loop_count=0;
			int	exterior_loop=(-1);	/* index of outer loop (in loop_list) */
			int	outer_loop_flag=1;	/* IGES flag to indicate a selected outer loop */

			if (fu->orientation != OT_SAME)
				continue;

			/* Count loops */
			for (BU_LIST_FOR(lu, loopuse, &fu->lu_hd))
			{
				NMG_CK_LOOPUSE(lu);
				if( lu->orientation == OT_SAME )
					exterior_loop = loop_count;
				loop_count++;
			}
			loop_list = (int *)bu_calloc( loop_count , sizeof( int ) , "loop_list" );

			loop_count = 0;
			for (BU_LIST_FOR(lu, loopuse, &fu->lu_hd))
			{
				int		edge_count=0;

				/* Count edges */
				if (BU_LIST_FIRST_MAGIC(&lu->down_hd) == NMG_EDGEUSE_MAGIC)
				{
					for (BU_LIST_FOR(eu, edgeuse, &lu->down_hd))
					{
						NMG_CK_EDGEUSE(eu);
						NMG_CK_EDGE(eu->e_p);
						NMG_CK_VERTEXUSE(eu->vu_p);
						NMG_CK_VERTEX(eu->vu_p->v_p);
						NMG_CK_VERTEX_G(eu->vu_p->v_p->vg_p);
						edge_count++;
					}
				}
				else if (BU_LIST_FIRST_MAGIC(&lu->down_hd) == NMG_VERTEXUSE_MAGIC)
				{
		  			v = BU_LIST_PNEXT(vertexuse, &lu->down_hd)->v_p;
					NMG_CK_VERTEX(v);
					NMG_CK_VERTEX_G(v->vg_p);
		  			edge_count++;
				}
				else
					bu_log("write_shell_face_loop: loopuse mess up! (1)\n");

				bu_vls_printf( &str , "508,%d" , edge_count );

				if (BU_LIST_FIRST_MAGIC(&lu->down_hd) == NMG_EDGEUSE_MAGIC)
				{
					for (BU_LIST_FOR(eu, edgeuse, &lu->down_hd))
					{
						int orientation;
						struct edge *e;
						struct edgeuse *eu_tab;

						e = eu->e_p;
						eu_tab = e->eu_p;
						if( eu_tab->vu_p->v_p == eu->vu_p->v_p )
							orientation = 1;
						else
							orientation = 0;

						bu_vls_printf( &str , ",0,%d,%d,%d,0",
							edge_de ,
							bu_ptbl_locate( etab , (long *)(e)) + 1,
							orientation );

						edge_count++;
					}
				}
				else if (BU_LIST_FIRST_MAGIC(&lu->down_hd) == NMG_VERTEXUSE_MAGIC)
				{
		  			v = BU_LIST_PNEXT(vertexuse, &lu->down_hd)->v_p;
					bu_vls_printf( &str , ",1,%d,%d,1,0",
						vert_de,
						bu_ptbl_locate( vtab , (long *)v )+1 );
				} else
					bu_log("write_shell_face_loop: loopuse mess up! (2)\n");

				bu_vls_strcat( &str , ";" );

				/* write loop entry */
				/* initialize directory entry */
				for( i=0 ; i<21 ; i++ )
					dir_entry[i] = DEFAULT;

				/* remember where parameter data is going */
				dir_entry[2] = param_seq + 1;

				/* get parameter line count */
				dir_entry[14] = write_freeform( fp_param , bu_vls_addr( &str ) , dir_seq+1 , 'P' );

				/* write directory entry for loop entity */
				dir_entry[1] = 508;
				dir_entry[8] = 0;
				dir_entry[9] = 10001;
				dir_entry[11] = 508;
				dir_entry[15] = 1;
				loop_list[loop_count++] = write_dir_entry( fp_dir , dir_entry );

				bu_vls_free( &str );

			}

			if( exterior_loop < 0 )
			{
				bu_log( "No outside loop found for face\n" );
				outer_loop_flag = 0;
			}
			else if( exterior_loop != 0 ) /* move outside loop to start of list */
			{
				int tmp;

				tmp = loop_list[0];
				loop_list[0] = loop_list[exterior_loop];
				loop_list[exterior_loop] = tmp;
			}

			if( do_nurbs )
			{
				vect_t u_dir,v_dir;
				point_t base_pt;
				fastf_t u_max,v_max;

				bu_vls_printf( &str , "510,%d,%d,%d" ,
					write_planar_nurb( fu , u_dir , v_dir , &u_max , &v_max , base_pt , fp_dir , fp_param ),
					loop_count,
					outer_loop_flag );
			}
			else
				bu_vls_printf( &str , "510,%d,%d,%d" ,
					write_plane_entity( fu->f_p->g.plane_p->N , fp_dir , fp_param ),
					loop_count,
					outer_loop_flag );

			for( i=0 ; i<loop_count ; i++ )
				bu_vls_printf( &str , ",%d" , loop_list[i] );

			bu_vls_strcat( &str , ";" );

			for( i=0 ; i<21 ; i++ )
				dir_entry[i] = DEFAULT;

			dir_entry[1] = 510;
			dir_entry[2] = param_seq + 1;
			dir_entry[8] = 0;
			dir_entry[9] = 10001;
			dir_entry[11] = 510;
			dir_entry[15] = 1;
			dir_entry[14] = write_freeform( fp_param , bu_vls_addr( &str ) , dir_seq+1 , 'P' );

			face_list[face_count++] = write_dir_entry( fp_dir , dir_entry );;

			bu_free( (char *)loop_list , "loop list" );
			bu_vls_free( &str );
		}

		/* write shell entity */
		bu_vls_printf( &str , "514,%d" , face_count );
		for( i=0 ; i<face_count ; i++ )
			bu_vls_printf( &str , ",%d,1" , face_list[i] );
		bu_vls_strcat( &str , ";" );

		/* initialize directory entry */
		for( i=0 ; i<21 ; i++ )
			dir_entry[i] = DEFAULT;

		dir_entry[1] = 514;
		dir_entry[2] = param_seq + 1;
		dir_entry[8] = 0;
		dir_entry[9] = 10001;
		dir_entry[11] = 514;
		dir_entry[15] = 1;
		dir_entry[14] = write_freeform( fp_param , bu_vls_addr( &str ) , dir_seq+1 , 'P' );

		shell_list[shell_count++] = write_dir_entry( fp_dir , dir_entry );

		bu_free( (char *)face_list , "face list" );
		bu_vls_free( &str );
	}

	/* write BREP object entity */

	/* write name entity */
	if( name != NULL )
		name_de = write_name_entity( name , fp_dir , fp_param );
	else
		name_de = 0;

	/* write color and attributes entities, if appropriate */
	if( !name || lookup_props( &props , name ) )
	{
		prop_de = 0;
		color_de = 0;
	}
	else
	{
		prop_de = write_att_entity( &props , fp_dir , fp_param );
		if( props.color_defined )
			color_de = get_color( props.color , fp_dir , fp_param );
	}

	/* Put outer shell in BREP object first */
	bu_vls_printf( &str , "186,%d,1,%d" , shell_list[0] , shell_count-1 );

	/* Add all other shells */
	for( i=1 ; i<shell_count ; i++ )
	{
			bu_vls_printf( &str , ",%d,1" , shell_list[i] );
	}

	if( prop_de || name_de )
	{
		if( prop_de && name_de )
			bu_vls_strcat( &str , ",0,2" );
		else
			bu_vls_printf( &str , ",0,1" );
		if( prop_de )
			bu_vls_printf( &str , ",%d" , prop_de );
		if( name_de )
			bu_vls_printf( &str , ",%d" , name_de );

	}

	bu_vls_strcat( &str , ";"  );

	/* initialize directory entry */
	for( i=0 ; i<21 ; i++ )
		dir_entry[i] = DEFAULT;

	dir_entry[1] = 186;
	dir_entry[2] = param_seq + 1;
	dir_entry[8] = 0;
	if( dependent )
		dir_entry[9] = 10001;
	else
		dir_entry[9] = 1;
	dir_entry[11] = 186;
	dir_entry[13] = color_de;
	dir_entry[15] = 0;
	dir_entry[14] = write_freeform( fp_param , bu_vls_addr( &str ) , dir_seq+1 , 'P' );

	bu_free( (char *)shell_list , "shell list" );
	bu_vls_free( &str );

	return( write_dir_entry( fp_dir , dir_entry ) );
}

void
w_terminate( fp )
FILE *fp;
{
	fprintf( fp , "S%07dG%07dD%07dP%07d%40.40sT0000001\n" , start_len , global_len , dir_seq , param_seq , " " );
}

int
arb_is_rpp( arb )
struct rt_arb_internal *arb;
{
	vect_t v0,v1,v2;
	int i;

	RT_ARB_CK_MAGIC( arb );

	/* for an rpp, all the height edge vectors must be equal,
	   all the width edge vectors must be equal, all the
	   depth edge vectors must be equal, and at least one
	   vertex must have three right angles */

	/* check the height vectors */
	VSUB2( v0 , arb->pt[4] , arb->pt[0] );
	for( i=5 ; i<8 ; i++ )
	{
		VSUB2( v1 , arb->pt[i] , arb->pt[i-4] );
		if( !VAPPROXEQUAL( v0 , v1 , tol.dist ) )
			return( 0 );
	}

	/* check the width vectors */
	VSUB2( v0 , arb->pt[1] , arb->pt[0] );
	VSUB2( v1 , arb->pt[2] , arb->pt[3] );
	if( !VAPPROXEQUAL( v0 , v1 , tol.dist ) )
			return( 0 );
	VSUB2( v1 , arb->pt[6] , arb->pt[7] );
	if( !VAPPROXEQUAL( v0 , v1 , tol.dist ) )
			return( 0 );
	VSUB2( v1 , arb->pt[5] , arb->pt[4] );
	if( !VAPPROXEQUAL( v0 , v1 , tol.dist ) )
			return( 0 );

	/* check the depth vectors */
	VSUB2( v0 , arb->pt[3] , arb->pt[0] );
	VSUB2( v1 , arb->pt[2] , arb->pt[1] );
	if( !VAPPROXEQUAL( v0 , v1 , tol.dist ) )
			return( 0 );
	VSUB2( v1 , arb->pt[6] , arb->pt[5] );
	if( !VAPPROXEQUAL( v0 , v1 , tol.dist ) )
			return( 0 );
	VSUB2( v1 , arb->pt[7] , arb->pt[4] );
	if( !VAPPROXEQUAL( v0 , v1 , tol.dist ) )
			return( 0 );

	/* check for a right angle corner */
	VSUB2( v0 , arb->pt[3] , arb->pt[0] );
	VSUB2( v1 , arb->pt[1] , arb->pt[0] );
	VSUB2( v2 , arb->pt[4] , arb->pt[0] );
	VUNITIZE( v0 );
	VUNITIZE( v1 );
	VUNITIZE( v2 );
	if( !BN_VECT_ARE_PERP( VDOT( v0 , v1 ) , &tol ) )
		return( 0 );
	if( !BN_VECT_ARE_PERP( VDOT( v0 , v2 ) , &tol ) )
		return( 0 );
	if( !BN_VECT_ARE_PERP( VDOT( v1 , v2 ) , &tol ) )
		return( 0 );

	return( 1 );
}

int
write_name_entity( name , fp_dir , fp_param )
char *name;
FILE *fp_dir,*fp_param;
{
	struct bu_vls		str;
	int			dir_entry[21];
	int			i;
	int			name_len;

	name_len = strlen( name );
	if( !name_len )
		return( 0 );

	bu_vls_init( &str );

	/* initialize directory entry */
	for( i=0 ; i<21 ; i++ )
		dir_entry[i] = DEFAULT;

	/* write parameter data into a string */
	if( name_len >= NAMESIZE )
		bu_vls_printf( &str , "406,1,16H%16.16s;" , name );
	else
		bu_vls_printf( &str , "406,1,%dH%s;" , strlen( name ) , name );

	/* remember where parameter data is going */
	dir_entry[2] = param_seq + 1;

	/* get parameter line count */
	dir_entry[14] = write_freeform( fp_param , bu_vls_addr( &str ), dir_seq+1 , 'P' );
	bu_vls_free( &str );

	/* write directory entry for name property entity */
	dir_entry[1] = 406;
	dir_entry[8] = 0;
	dir_entry[9] = 1010301;
	dir_entry[11] = 406;
	dir_entry[15] = 15;
	return( write_dir_entry( fp_dir , dir_entry ));
}

int
tor_to_iges( ip , name , fp_dir , fp_param )
struct rt_db_internal *ip;
char *name;
FILE *fp_dir,*fp_param;
{
	struct rt_tor_internal	*tor;
	struct bu_vls		str;
	int			dir_entry[21];
	int			name_de;
	int			i;

	if( ip->idb_type != ID_TOR )
		bu_log( "tor_to_iges called for non-torus (type=%d)\n" , ip->idb_type );

	tor = (struct rt_tor_internal *)ip->idb_ptr;

	RT_TOR_CK_MAGIC( tor );

	bu_vls_init( &str );

	/* initialize directory entry */
	for( i=0 ; i<21 ; i++ )
		dir_entry[i] = DEFAULT;

	/* write a name entity for this solid */
	name_de = write_name_entity( name , fp_dir , fp_param );

	/* write parameter data into a string */
	bu_vls_printf( &str , "160,%g,%g,%g,%g,%g,%g,%g,%g,0,1,%d;",
		tor->r_a,
		tor->r_h,
		tor->v[X] , tor->v[Y] , tor->v[Z],
		tor->h[X] , tor->h[Y] , tor->h[Z],
		name_de );

	/* remember where parameter data is going */
	dir_entry[2] = param_seq + 1;

	/* get parameter line count */
	dir_entry[14] = write_freeform( fp_param , bu_vls_addr( &str ), dir_seq+1 , 'P' );
	bu_vls_free( &str );

	/* write directory entry for torus entity */
	dir_entry[1] = 160;
	dir_entry[8] = 0;
	dir_entry[9] = 10001;
	dir_entry[11] = 160;
	dir_entry[15] = 0;
	return( write_dir_entry( fp_dir , dir_entry ));

}

int
sph_to_iges( ip , name , fp_dir , fp_param )
char *name;
struct rt_db_internal *ip;
FILE *fp_dir,*fp_param;
{
	struct rt_ell_internal	*sph;
	struct bu_vls		str;
	double			radius;
	int			dir_entry[21];
	int			name_de;
	int			i;

	if( ip->idb_type != ID_SPH )
		bu_log( "sph_to_iges called for non-sph (type=%d)\n" , ip->idb_type );

	sph = (struct rt_ell_internal *)ip->idb_ptr;

	RT_ELL_CK_MAGIC( sph );

	/* write name entity */
	name_de = write_name_entity( name , fp_dir , fp_param );

	bu_vls_init( &str );

	/* initialize directory entry */
	for( i=0 ; i<21 ; i++ )
		dir_entry[i] = DEFAULT;

	radius = MAGNITUDE( sph->a );

	/* write parameter data into a string */
	bu_vls_printf( &str , "158,%g,%g,%g,%g,0,1,%d;",
		radius,
		sph->v[X] , sph->v[Y] , sph->v[Z] ,
		name_de );

	/* remember where parameter data is going */
	dir_entry[2] = param_seq + 1;

	/* get parameter line count */
	dir_entry[14] = write_freeform( fp_param , bu_vls_addr( &str ), dir_seq+1 , 'P' );
	bu_vls_free( &str );

	/* write directory entry for sphere entity */
	dir_entry[1] = 158;
	dir_entry[8] = 0;
	dir_entry[9] = 10001;
	dir_entry[11] = 158;
	dir_entry[15] = 0;
	return( write_dir_entry( fp_dir , dir_entry ));

}

int
ell_to_iges( ip , name , fp_dir , fp_param )
char *name;
struct rt_db_internal *ip;
FILE *fp_dir,*fp_param;
{
	struct rt_ell_internal	*ell;
	struct bu_vls		str;
	double			radius_a;
	double			radius_b;
	double			radius_c;
	vect_t			a_dir;
	vect_t			b_dir;
	vect_t			c_dir;
	int			dir_entry[21];
	int			name_de;
	int			i;

	if( ip->idb_type != ID_ELL )
		bu_log( "ell_to_iges called for non-ell (type=%d)\n" , ip->idb_type );

	ell = (struct rt_ell_internal *)ip->idb_ptr;

	RT_ELL_CK_MAGIC( ell );

	/* write name entity */
	name_de = write_name_entity( name , fp_dir , fp_param );

	bu_vls_init( &str );

	/* initialize directory entry */
	for( i=0 ; i<21 ; i++ )
		dir_entry[i] = DEFAULT;

	radius_a = MAGNITUDE( ell->a );
	radius_b = MAGNITUDE( ell->b );
	radius_c = MAGNITUDE( ell->c );

	VMOVE( a_dir , ell->a );
	VMOVE( b_dir , ell->b );
	VMOVE( c_dir , ell->c );

	VUNITIZE( a_dir );
	VUNITIZE( b_dir );
	VUNITIZE( c_dir );

	/* write parameter data into a string */
	bu_vls_printf( &str , "168,%g,%g,%g,%g,%g,%g,%g,%g,%g,%g,%g,%g,0,1,%d;",
		radius_a , radius_b , radius_c ,
		ell->v[X] , ell->v[Y] , ell->v[Z] ,
		a_dir[X] , a_dir[Y] , a_dir[Z] ,
		c_dir[X] , c_dir[Y] , c_dir[Z] ,
		name_de );

	/* remember where parameter data is going */
	dir_entry[2] = param_seq + 1;

	/* get parameter line count */
	dir_entry[14] = write_freeform( fp_param , bu_vls_addr( &str ), dir_seq+1 , 'P' );
	bu_vls_free( &str );

	/* write directory entry for ellipsoid entity */
	dir_entry[1] = 168;
	dir_entry[8] = 0;
	dir_entry[9] = 10001;
	dir_entry[11] = 168;
	dir_entry[15] = 0;
	return( write_dir_entry( fp_dir , dir_entry ));

}
int
rpp_to_iges( ip , name , fp_dir , fp_param )
char *name;
struct rt_db_internal *ip;
FILE *fp_dir,*fp_param;
{
	struct rt_arb_internal	*arb;
	struct bu_vls		str;
	double			length_a;
	double			length_b;
	double			length_c;
	vect_t			a_dir;
	vect_t			b_dir;
	vect_t			c_dir;
	vect_t			tmp_dir;
	int			dir_entry[21];
	int			name_de;
	int			i;

	if( ip->idb_type != ID_ARB8 )
		bu_log( "rpp_to_iges called for non-arb (type=%d)\n" , ip->idb_type );

	arb = (struct rt_arb_internal *)ip->idb_ptr;

	RT_ARB_CK_MAGIC( arb );

	/* write name entity */
	name_de = write_name_entity( name , fp_dir , fp_param );

	bu_vls_init( &str );

	/* initialize directory entry */
	for( i=0 ; i<21 ; i++ )
		dir_entry[i] = DEFAULT;

	VSUB2( a_dir , arb->pt[1] , arb->pt[0] );
	VSUB2( b_dir , arb->pt[3] , arb->pt[0] );
	VSUB2( c_dir , arb->pt[4] , arb->pt[0] );

	length_a = MAGNITUDE( a_dir );
	length_b = MAGNITUDE( b_dir );
	length_c = MAGNITUDE( c_dir );

	VUNITIZE( a_dir );
	VUNITIZE( b_dir );
	VUNITIZE( c_dir );

	/* c_dir cross a_dir must give b_dir for IGES */
	VCROSS( tmp_dir , c_dir , a_dir );
	if( VDOT( b_dir , tmp_dir ) < 0.0 )
	{
		/* not a right-handed system, so exchange a_dir with c_dir */
		double tmp_length;

		VMOVE( tmp_dir , a_dir );
		VMOVE( a_dir , c_dir );
		VMOVE( c_dir , tmp_dir );
		tmp_length = length_a;
		length_a = length_c;
		length_c = tmp_length;
	}

	/* write parameter data into a string */
	bu_vls_printf( &str , "150,%g,%g,%g,%g,%g,%g,%g,%g,%g,%g,%g,%g,0,1,%d;",
		length_a , length_b , length_c ,
		arb->pt[0][X] , arb->pt[0][Y] , arb->pt[0][Z] ,
		a_dir[X] , a_dir[Y] , a_dir[Z] ,
		c_dir[X] , c_dir[Y] , c_dir[Z] ,
		name_de );

	/* remember where parameter data is going */
	dir_entry[2] = param_seq + 1;

	/* get parameter line count */
	dir_entry[14] = write_freeform( fp_param , bu_vls_addr( &str ), dir_seq+1 , 'P' );
	bu_vls_free( &str );

	/* write directory entry for block entity */
	dir_entry[1] = 150;
	dir_entry[8] = 0;
	dir_entry[9] = 10001;
	dir_entry[11] = 150;
	dir_entry[15] = 0;
	return( write_dir_entry( fp_dir , dir_entry ));
}

int
arb_to_iges( ip , name , fp_dir , fp_param )
char *name;
struct rt_db_internal *ip;
FILE *fp_dir,*fp_param;
{
	struct rt_arb_internal *arb;

	if( ip->idb_type != ID_ARB8 )
	{
		bu_log( "arb_to_iges called for non-arb (type=%d)\n" , ip->idb_type );
		return( 0 );
	}

	arb = (struct rt_arb_internal *)ip->idb_ptr;

	RT_ARB_CK_MAGIC( arb );

	if( arb_is_rpp( arb ) )
		return( rpp_to_iges( ip , name , fp_dir , fp_param ) );
	else
		return( nmg_to_iges( ip , name , fp_dir , fp_param ) );
}

int
tgc_to_iges( ip , name , fp_dir , fp_param )
char *name;
struct rt_db_internal *ip;
FILE *fp_dir,*fp_param;
{
	struct rt_tgc_internal	*tgc;
	fastf_t			h_len,a_len,b_len,c_len,d_len;
	vect_t			h_dir,a_dir,b_dir;
	int			iges_type;
	struct bu_vls		str;
	int			dir_entry[21];
	int			name_de;
	int			i;

	if( ip->idb_type != ID_TGC )
	{
		bu_log( "tgc_to_iges called for non-tgc (type=%d)\n" , ip->idb_type );
                return( 0 );
	}

	tgc = (struct rt_tgc_internal *)ip->idb_ptr;

        RT_TGC_CK_MAGIC( tgc );

	h_len = MAGNITUDE( tgc->h );
	a_len = MAGNITUDE( tgc->a );
	b_len = MAGNITUDE( tgc->b );
	c_len = MAGNITUDE( tgc->c );
	d_len = MAGNITUDE( tgc->d );

	/* Use VSCALE rather than VUNITIZE, since we have
	   already done the sqrt */

	VMOVE( h_dir , tgc->h );
	VSCALE( h_dir , h_dir , 1.0/h_len );

	VMOVE( a_dir , tgc->a );
	VSCALE( a_dir , a_dir , 1.0/a_len );

	VMOVE( b_dir , tgc->b );
	VSCALE( b_dir , b_dir , 1.0/b_len );

	if( !BN_VECT_ARE_PERP( VDOT( h_dir , a_dir ) , &tol ) ||
	    !BN_VECT_ARE_PERP( VDOT( h_dir , b_dir ) , &tol ) )
	{
		/* this is not an rcc or a trc */
		return( nmg_to_iges( ip , name , fp_dir , fp_param ));
	}

	if( NEAR_ZERO( a_len-b_len , tol.dist ) &&
	    NEAR_ZERO( c_len-d_len , tol.dist ) )
	{
		/* this tgc is either an rcc or a trc */

		/* write name entity */
		name_de = write_name_entity( name , fp_dir , fp_param );

		bu_vls_init( &str );

		/* initialize directory entry */
		for( i=0 ; i<21 ; i++ )
			dir_entry[i] = DEFAULT;

		if( NEAR_ZERO( a_len-c_len , tol.dist ) )
		{
			/* its an rcc */
			iges_type = 154;
			bu_vls_printf( &str , "154,%g,%g,%g,%g,%g,%g,%g,%g" ,
				h_len , a_len ,
				tgc->v[X] , tgc->v[Y] , tgc->v[Z] ,
				h_dir[X] , h_dir[Y] , h_dir[Z] );
		}
		else
		{
			/* its a trc */

			fastf_t bigger_r,smaller_r;
			vect_t base;

			iges_type = 156;
			if( a_len > c_len )
			{
				bigger_r = a_len;
				smaller_r = c_len;
				VMOVE( base , tgc->v );
			}
			else
			{
				bigger_r = c_len;
				smaller_r = a_len;
				VADD2( base , tgc->v , tgc->h );
				VREVERSE( h_dir , h_dir );
			}
			bu_vls_printf( &str , "156,%g,%g,%g,%g,%g,%g,%g,%g,%g" ,
				h_len , bigger_r , smaller_r ,
				base[X] , base[Y] , base[Z] ,
				h_dir[X] , h_dir[Y] , h_dir[Z] );
		}

		bu_vls_printf( &str , ",0,1,%d;" , name_de );

		/* remember where parameter data is going */
		dir_entry[2] = param_seq + 1;

		/* get parameter line count */
		dir_entry[14] = write_freeform( fp_param , bu_vls_addr( &str ), dir_seq+1 , 'P' );
		bu_vls_free( &str );

		/* fill in directory entry */
		dir_entry[1] = iges_type;
		dir_entry[8] = 0;
		dir_entry[9] = 10001;
		dir_entry[11] = iges_type;
		dir_entry[15] = 0;

		return( write_dir_entry( fp_dir , dir_entry ));
	}
	else
		return( nmg_to_iges( ip , name , fp_dir , fp_param ));
}

int
write_tree_of_unions( name, de_list , length , dependent , fp_dir , fp_param )
char *name;
int de_list[];
int length;
int dependent;
FILE *fp_dir;
FILE *fp_param;
{
	struct bu_vls		str;
	struct iges_properties	props;
	int			dir_entry[21];
	int			name_de;
	int			prop_de;
	int			color_de=DEFAULT;
	int			i;

	bu_vls_init( &str );

	/* write name entity */
	if( name != NULL )
		name_de = write_name_entity( name , fp_dir , fp_param );
	else
		name_de = 0;

	/* write color and attributes entities, if appropriate */
	if( lookup_props( &props , name ) )
	{
		prop_de = 0;
		color_de = 0;
	}
	else
	{
		prop_de = write_att_entity( &props , fp_dir , fp_param );
		if( props.color_defined )
			color_de = get_color( props.color , fp_dir , fp_param );
	}

	/* write parameter data into a string */
	bu_vls_printf( &str , "180,%d,%d" , 2*length-1 , -de_list[0] );
	for( i=1 ; i<length ; i++ )
		bu_vls_printf( &str , ",%d,1" , -de_list[i] );

	if( prop_de || name_de )
	{
		if( prop_de && name_de )
			bu_vls_strcat( &str , ",0,2" );
		else
			bu_vls_printf( &str , ",0,1" );
		if( prop_de )
			bu_vls_printf( &str , ",%d" , prop_de );
		if( name_de )
			bu_vls_printf( &str , ",%d" , name_de );

	}

	bu_vls_strcat( &str , ";"  );

	/* initialize directory entry */
	for( i=0 ; i<21 ; i++ )
		dir_entry[i] = DEFAULT;

	dir_entry[1] = 180;
	dir_entry[2] = param_seq + 1;
	dir_entry[8] = 0;
	if( dependent )
		dir_entry[9] = 10001;
	else
		dir_entry[9] = 1;
	dir_entry[11] = 180;
	dir_entry[13] = color_de;
	dir_entry[15] = 0;
	dir_entry[14] = write_freeform( fp_param , bu_vls_addr( &str ) , dir_seq+1 , 'P' );

	bu_vls_free( &str );

	return( write_dir_entry( fp_dir , dir_entry ) );
}

int
write_solid_assembly( name, de_list , length , dependent , fp_dir , fp_param )
char *name;
int de_list[];
int length;
int dependent;
FILE *fp_dir;
FILE *fp_param;
{
	struct bu_vls		str;
	struct iges_properties	props;
	int			dir_entry[21];
	int			name_de;
	int			prop_de;
	int			color_de=DEFAULT;
	int			i;

	bu_vls_init( &str );

	/* write name entity */
	if( name != NULL )
		name_de = write_name_entity( name , fp_dir , fp_param );
	else
		name_de = 0;

	/* write color and attributes entities, if appropriate */
	if( lookup_props( &props , name ) )
	{
		prop_de = 0;
		color_de = 0;
	}
	else
	{
		prop_de = write_att_entity( &props , fp_dir , fp_param );
		if( props.color_defined )
			color_de = get_color( props.color , fp_dir , fp_param );
	}

	/* write parameter data into a string */
	bu_vls_printf( &str , "184,%d" , length );
	for( i=0 ; i<length ; i++ )
		bu_vls_printf( &str , ",%d" , de_list[i] );
	for( i=0 ; i<length ; i++ )
		bu_vls_printf( &str , ",0" );

	if( prop_de || name_de )
	{
		if( prop_de && name_de )
			bu_vls_strcat( &str , ",0,2" );
		else
			bu_vls_printf( &str , ",0,1" );
		if( prop_de )
			bu_vls_printf( &str , ",%d" , prop_de );
		if( name_de )
			bu_vls_printf( &str , ",%d" , name_de );

	}

	bu_vls_strcat( &str , ";"  );

	/* initialize directory entry */
	for( i=0 ; i<21 ; i++ )
		dir_entry[i] = DEFAULT;

	dir_entry[1] = 184;
	dir_entry[2] = param_seq + 1;
	dir_entry[8] = 0;
	if( dependent )
		dir_entry[9] = 10001;
	else
		dir_entry[9] = 1;
	dir_entry[11] = 184;
	dir_entry[13] = color_de;
	dir_entry[15] = 0;
	dir_entry[14] = write_freeform( fp_param , bu_vls_addr( &str ) , dir_seq+1 , 'P' );

	bu_vls_free( &str );

	return( write_dir_entry( fp_dir , dir_entry ) );
}

int
nmg_to_iges( ip , name , fp_dir , fp_param )
struct rt_db_internal *ip;
char *name;
FILE *fp_dir,*fp_param;
{
	struct model *model;
	struct nmgregion *r;
	int region_count;
	int *region_de;
	int brep_de;
	int dependent;
	int i;

	RT_CK_DB_INTERNAL( ip );

	dependent = 1;
	for( i=0 ; i<no_of_indeps ; i++ )
	{
		if( !strncmp( name , independent[i] , NAMESIZE ) )
		{
			dependent = 0;
			break;
		}
	}

	solid_is_brep = 1;
	comb_form = 1;
	if( ip->idb_type == ID_NMG )
	{
		model = (struct model *)ip->idb_ptr;
		NMG_CK_MODEL( model );

		/* count the number of nmgregions */
		region_count = 0;
		for( BU_LIST_FOR( r , nmgregion , &model->r_hd ) )
		{
			NMG_CK_REGION( r );
			region_count++;
		}

		if( region_count == 0 )
			return( 0 );
		else if( region_count == 1 )
			return( nmgregion_to_iges( name , BU_LIST_FIRST( nmgregion , &model->r_hd ) ,
				dependent , fp_dir , fp_param ) );
		else
		{
			/* make a boolean tree unioning all the regions */

			/* space to save the iges location of each nmgregion */
			region_de = (int *)bu_calloc( region_count , sizeof( int ) , "nmg_to_iges" );

			/* loop through all nmgregions in the model */
			region_count = 0;
			for( BU_LIST_FOR( r , nmgregion , &model->r_hd ) )
				region_de[region_count++] = nmgregion_to_iges( (char *)NULL , r , 1 ,
						fp_dir , fp_param );

			/* now make the boolean tree */
			brep_de = write_tree_of_unions( name , region_de , region_count ,
						dependent , fp_dir , fp_param );

			bu_free( (char *)region_de , "nmg_to_iges" );
			return( brep_de );
		}
	}
	else
	{
		if( ip->idb_type == ID_BOT )
		{
			struct rt_bot_internal *bot=(struct rt_bot_internal *)ip->idb_ptr;
			if( bot->mode != RT_BOT_SOLID )
			{
				bu_log( "%s is a plate mode primitive, and cannot be converted to IGES format!!!\n", name );
				return( 0 );
			}
		}
		model = nmg_mm();
		if( rt_functab[ip->idb_type].ft_tessellate( &r , model , ip , &ttol , &tol ) )
		{
			nmg_km( model );
			return( 0 );
		}
		else
		{
			solids_to_nmg++;
			brep_de =  nmgregion_to_iges( name , r , dependent , fp_dir , fp_param );
			nmg_km( model );
			return( brep_de );
		}
	}
}

int
null_to_iges( ip , name , fp_dir , fp_param )
struct rt_db_internal *ip;
char *name;
FILE *fp_dir,*fp_param;
{
	return( 0 );
}

int
write_xform_entity( mat , fp_dir , fp_param )
mat_t mat;
FILE *fp_dir,*fp_param;
{
	struct bu_vls		str;
	int			dir_entry[21];
	int			i;

	bu_vls_init( &str );

	/* initialize directory entry */
	for( i=0 ; i<21 ; i++ )
	        dir_entry[i] = DEFAULT;

	/* write parameter data into a string */
	bu_vls_strcpy( &str , "124"  );
	for( i=0 ; i<12 ; i++ )
	{
		bu_vls_printf( &str , ",%g" , mat[i] );
	}
	bu_vls_strcat( &str , ";" );

	dir_entry[1] = 124;
	dir_entry[2] = param_seq + 1;
	dir_entry[8] = 0;
	dir_entry[9] = 10001;
	dir_entry[11] = 124;
	dir_entry[15] = 0;
	dir_entry[14] = write_freeform( fp_param , bu_vls_addr( &str ) , dir_seq+1 , 'P' );

	bu_vls_free( &str );

	return( write_dir_entry( fp_dir , dir_entry ) );
}

int
write_solid_instance( orig_de , mat , fp_dir , fp_param )
int orig_de;
mat_t mat;
FILE *fp_dir,*fp_param;
{
	struct bu_vls		str;
	int			dir_entry[21];
	int			i;

	bu_vls_init( &str );

	/* initialize directory entry */
	for( i=0 ; i<21 ; i++ )
	        dir_entry[i] = DEFAULT;

	/* write the transformation matrix and make the link */
	dir_entry[7] = write_xform_entity( mat , fp_dir , fp_param );

	/* write parameter data into a string */
	bu_vls_printf( &str , "430,%d;" , orig_de );

	dir_entry[1] = 430;
	dir_entry[2] = param_seq + 1;
	dir_entry[8] = 0;
	dir_entry[9] = 10001;
	dir_entry[11] = 430;
	dir_entry[15] = solid_is_brep ? 1 : 0;
	dir_entry[14] = write_freeform( fp_param , bu_vls_addr( &str ) , dir_seq+1 , 'P' );

	bu_vls_free( &str );

	return( write_dir_entry( fp_dir , dir_entry ) );
}

int
write_att_entity( props , fp_dir , fp_param )
struct iges_properties *props;
FILE *fp_dir,*fp_param;
{
	struct bu_vls		str;
	int			dir_entry[21];
	int			str_len;
	int			i;

	bu_vls_init( &str );

	/* initialize directory entry */
	for( i=0 ; i<21 ; i++ )
		dir_entry[i] = DEFAULT;

	/* write parameter data into a string */
	bu_vls_strcpy( &str , "422" );

	/* material name */
	str_len = strlen( props->material_name );
	if( str_len )
		bu_vls_printf( &str , ",%dH%s" , str_len , props->material_name );
	else
		bu_vls_strcat( &str , "," );

	/* material parameters */
	str_len = strlen( props->material_params );
	if( str_len )
		bu_vls_printf( &str , ",%dH%s" , str_len , props->material_params );
	else
		bu_vls_strcat( &str , "," );

	/* region flag */
	if( props->region_flag == 'R' )
		bu_vls_strcat( &str , ",1" );
	else
		bu_vls_strcat( &str , ",0" );

	/* ident number, air code, material code, los density */
	bu_vls_printf( &str , ",%d,%d,%d,%d,%d,%d;" ,
		props->ident ,
		props->air_code ,
		props->material_code ,
		props->los_density,
		props->inherit,
		props->color_defined );

	dir_entry[1] = 422;
	dir_entry[2] = param_seq + 1;
	dir_entry[3] = (-attribute_de);
	dir_entry[8] = 0;
	dir_entry[9] = 10301;
	dir_entry[11] = 422;
	dir_entry[15] = 0;
	dir_entry[14] = write_freeform( fp_param , bu_vls_addr( &str ) , dir_seq+1 , 'P' );

	bu_vls_free( &str );

	return( write_dir_entry( fp_dir , dir_entry ) );
}

/*
 *	Write a region or group of only one member as a solid instance
 *
 */
int
short_comb_to_iges( props , dependent , de_pointers , fp_dir , fp_param )
struct iges_properties *props;
int dependent;
int de_pointers[];
FILE *fp_dir,*fp_param;
{
	struct bu_vls		str;
	int			dir_entry[21];
	int			name_de;
	int			props_de;
	int			color_de=DEFAULT;
	int			status=1;
	int			i;

	/* if member has not been converted, don't try to write the tree */
	if( de_pointers[0] == 0  )
		return( 0 );

	if( dependent )
		status = 10001;

	bu_vls_init( &str );

	/* initialize directory entry */
	for( i=0 ; i<21 ; i++ )
		dir_entry[i] = DEFAULT;

	/* write name entity */
	name_de = write_name_entity( props->name , fp_dir , fp_param );

	/* write attributes entity */
	props_de = write_att_entity( props , fp_dir , fp_param );

	/* get color */
	if( props->color_defined )
		color_de = get_color( props->color , fp_dir , fp_param );
	else
		color_de = 0;

	/* write parameter data into a string */
	bu_vls_printf( &str , "430,%d" , de_pointers[0] );

	if( props_de || name_de )
	{
		if( props_de && name_de )
			bu_vls_strcat( &str , ",0,2" );
		else
			bu_vls_printf( &str , ",0,1" );
		if( props_de )
			bu_vls_printf( &str , ",%d" , props_de );
		if( name_de )
			bu_vls_printf( &str , ",%d" , name_de );

	}
	bu_vls_strcat( &str , ";" );

	dir_entry[1] = 430;
	dir_entry[2] = param_seq + 1;
	dir_entry[8] = 0;
	dir_entry[9] = status;
	dir_entry[11] = 430;
	dir_entry[13] = color_de;
	dir_entry[15] = comb_form;
	dir_entry[14] = write_freeform( fp_param , bu_vls_addr( &str ) , dir_seq+1 , 'P' );

	bu_vls_free( &str );

	return( write_dir_entry( fp_dir , dir_entry ) );

}

int
count_non_union_ops( tp )
union tree *tp;
{
	int count=0;

	RT_CK_TREE( tp );

	switch( tp->tr_op )
	{
		case OP_INTERSECT:
		case OP_SUBTRACT:
			count++;
		case OP_UNION:
			count += count_non_union_ops( tp->tr_b.tb_left );
			count += count_non_union_ops( tp->tr_b.tb_right );
			break;
		default:
			break;
	}

	return( count );
}

void
igs_tree( str, tp, length, de_pointers )
struct bu_vls *str;
union tree *tp;
int length;
int *de_pointers;
{
	RT_CK_TREE( tp );
	BU_CK_VLS( str );

	switch( tp->tr_op )
	{
		case OP_UNION:
			igs_tree( str, tp->tr_b.tb_left, length, de_pointers );
			igs_tree( str, tp->tr_b.tb_right, length, de_pointers );
			bu_vls_strcat( str, ",1" );
			break;
		case OP_INTERSECT:
			igs_tree( str, tp->tr_b.tb_left, length, de_pointers );
			igs_tree( str, tp->tr_b.tb_right, length, de_pointers );
			bu_vls_strcat( str, ",2" );
			break;
		case OP_SUBTRACT:
			igs_tree( str, tp->tr_b.tb_left, length, de_pointers );
			igs_tree( str, tp->tr_b.tb_right, length, de_pointers );
			bu_vls_strcat( str, ",3" );
			break;
		case OP_DB_LEAF:
			bu_vls_printf( str, ",%d", -de_pointers[de_pointer_number] );
			de_pointer_number++;
			break;
		default:
			bu_log( "Unrecognized operation in combination!!\n" );
			break;
	}
}

void
write_igs_tree( str, comb, length, de_pointers )
struct bu_vls *str;
struct rt_comb_internal *comb;
int length;
int *de_pointers;
{
	BU_CK_VLS( str );
	RT_CK_COMB( comb );

	bu_vls_printf( str , "180,%d", 2*length-1 );

	de_pointer_number = 0;
	igs_tree( str, comb->tree, length, de_pointers );
}

int
tree_to_iges( comb , length , dependent , props , de_pointers , fp_dir , fp_param )
struct rt_comb_internal *comb;
int length,dependent;
struct iges_properties *props;
int de_pointers[];
FILE *fp_dir,*fp_param;
{
	struct bu_vls		str;
	int			dir_entry[21];
	int			actual_length=0;
	int			name_de;
	int			props_de;
	int			color_de=DEFAULT;
	int			non_union_count=0;
	int			status=1;
	int			entity_type;
	int			i;

	RT_CK_COMB( comb );

	/* if any part of this tree has not been converted, don't try to write the tree */
	for( i=0 ; i<length ; i++ )
	{
		if( de_pointers[i] )
			actual_length++;
	}
	if( actual_length != length )
		return( 0 );

	if( dependent )
		status = 10001;

	bu_vls_init( &str );

	/* initialize directory entry */
	for( i=0 ; i<21 ; i++ )
		dir_entry[i] = DEFAULT;

	/* write name entity */
	name_de = write_name_entity( props->name , fp_dir , fp_param );

	/* write attributes entity */
	props_de = write_att_entity( props , fp_dir , fp_param );

	/* get color */
	if( props->color_defined )
		color_de = get_color( props->color , fp_dir , fp_param );
	else
		color_de = 0;

	non_union_count = count_non_union_ops( comb->tree );

	if( mode == CSG_MODE || non_union_count )
	{
		/* write the combination as a Boolean tree */
		entity_type = 180;
		write_igs_tree( &str, comb, length, de_pointers );
	}
	else
	{
		/* write the combination as a solid assembly */
		entity_type = 184;

		bu_vls_printf( &str , "%d,%d" , entity_type , length );
		for( i=0 ; i<length ; i++ )
			bu_vls_printf( &str , ",%d" , de_pointers[i] );
		for( i=0 ; i<length ; i++ )
			bu_vls_strcat( &str , ",0" );
	}

	if( props_de || name_de )
	{
		if( props_de && name_de )
			bu_vls_strcat( &str , ",0,2" );
		else
			bu_vls_printf( &str , ",0,1" );
		if( props_de )
			bu_vls_printf( &str , ",%d" , props_de );
		if( name_de )
			bu_vls_printf( &str , ",%d" , name_de );

	}
	bu_vls_strcat( &str , ";" );

	dir_entry[1] = entity_type;
	dir_entry[2] = param_seq + 1;
	dir_entry[8] = 0;
	dir_entry[9] = status;
	dir_entry[11] = entity_type;
	dir_entry[13] = color_de;
	dir_entry[15] = comb_form;
	dir_entry[14] = write_freeform( fp_param , bu_vls_addr( &str ) , dir_seq+1 , 'P' );

	bu_vls_free( &str );

	return( write_dir_entry( fp_dir , dir_entry ) );

}

int
comb_to_iges( comb , length , dependent , props , de_pointers , fp_dir , fp_param )
struct rt_comb_internal *comb;
int length,dependent;
struct iges_properties *props;
int de_pointers[];
FILE *fp_dir,*fp_param;
{
	RT_CK_COMB( comb );

	if( length == 1 )
		return( short_comb_to_iges( props , dependent , de_pointers , fp_dir , fp_param ) );
	else
		return( tree_to_iges( comb , length , dependent , props , de_pointers , fp_dir , fp_param ) );
}

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
