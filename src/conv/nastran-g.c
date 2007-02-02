/*                     N A S T R A N - G . C
 * BRL-CAD
 *
 * Copyright (c) 1997-2007 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 *
 */
/** @file nastran-g.c
 *
 *	Code to convert a NASTRAN finite element model to BRL-CAD.
 *
 *  Author -
 *      John R. Anderson
 *
 *  Source -
 *	The U. S. Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5068  USA
 */

#ifndef lint
static const char RCSid[] = "$Header$";
#endif

#include "common.h"

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#ifdef HAVE_STRING_H
#  include <string.h>
#else
#  include <strings.h>
#endif
#include <ctype.h>
#include <errno.h>
#ifdef HAVE_UNISTD_H
#  include <unistd.h>
#endif

#include "machine.h"
#include "vmath.h"
#include "nmg.h"
#include "rtgeom.h"
#include "raytrace.h"
#include "wdb.h"
#include "../librt/debug.h"

struct coord_sys
{
	struct bu_list	l;	/* for the linked list */
	int cid;		/* coordinate system identification number */
	char type;		/* coordinate system type */
	int rid;		/* reference coordinate system */
	point_t origin;		/* origin of this coord system */
	vect_t v1, v2, v3;	/* three unit vectors */
};

struct grid_point
{
	int gid;		/* grid point ID number */
	int cid;		/* coordinate system this point uses */
	point_t pt;		/* actual grid point */
	struct vertex **v;	/* corresponding NMG vertex structure (indexed by pid) */
};

struct pbar
{
	struct bu_list l;	/* for the linked list */
	int pid;		/* PBAR ID number */
	int mid;		/* material ID number */
	fastf_t area;		/* cross-sectional area */
	struct wmember head;	/* head of linked list of members for this pid */
};

struct pshell
{
	struct bu_list l;	/* for the linked list */
	int pid;		/* PSHELL ID number */
	int mid;		/* material ID number */
	fastf_t thick;		/* thickness of shell */
	struct shell *s;	/* actual NMG shell */
};


#define		CORD_CYL	'C'
#define		CORD_RECT	'R'
#define		CORD_SPH	'S'

extern char *bu_optarg;
extern int bu_optind, bu_opterr, optopt;

#define		NAMESIZE	16	/* from db.h */

#define		MAXLINELEN	256	/* maximum allowed input line length */

#define		INCHES		1
#define		MM		2

static fastf_t conv[3]={
	1.0,
	25.4,
	1.0
};

static int			units;		/* units flag */
static char			*output_file = "nastran.g";
static struct rt_wdb		*fdout;		/* brlcad output file */
static FILE			*fdin;		/* NASTRAN input file */
static FILE			*fdtmp;		/* temporary version of NASTRAN input */
static char			*Usage="Usage:\n\t%s [-p] [-xX lvl] [-t tol.dist] [-i NASTRAN_file] -o BRL-CAD_file\n";
static long			start_off;
static char			*delims=", \t";
static struct coord_sys		coord_head;	/* head of linked list of coordinate systems */
static struct pbar		pbar_head;	/* head of linked list of PBAR's */
static struct pshell		pshell_head;	/* head of linked list of PSHELL's */
static int			pshell_count=0;	/* number of different PSHELL's */
static int			grid_count=0;	/* number of grid points */
static int			grid_used=0;	/* actual count of grid points currently in memory */
static struct grid_point	*g_pts;		/* array of grid points */
static int			bar_def_pid=0;	/* default property ID number for CBAR's */
static char			**curr_rec;	/* current input record */
static char			**prev_rec;	/* previous input record */
static char			*line;		/* current input line */
static char			*next_line;	/* next input line */
static char			*prev_line;	/* previous input line */
static int			input_status;		/* status of input FILE */
static long			line_count;	/* count of input lines */
static long			bulk_data_start_line;	/* line number where BULK DATA begins */
static struct model		*nmg_model;	/* NMG solid for surfaces */
static struct shell		*nmg_shell;	/* NMG shell */
static struct bn_tol		tol;		/* tolerance for NMG's */
static int			polysolids=1;	/* flag for outputting NMG's rather than BOT's */

HIDDEN int get_next_record( FILE *fd, int call_input, int write_flag );
HIDDEN int convert_pt( const point_t pt, struct coord_sys *cs, point_t out_pt );

#define		INPUT_OK	0
#define		INPUT_NULL	1

#define		UNKNOWN		0
#define		FREE_FIELD	1
#define		SMALL_FIELD	2
#define		LARGE_FIELD	3

#define		NO_OF_FIELDS	20
#define		FIELD_LENGTH	17

HIDDEN void
reset_input(void)
{
	int i;
	char *tmp;

	for( i=0 ; i < 20 ; i++ )
		prev_rec[i][0] = '\0';

	fseek( fdin, start_off, SEEK_SET );
	line_count = bulk_data_start_line;

	tmp = fgets( next_line, MAXLINELEN, fdin );
	while( tmp && *tmp == '$' )
		tmp = fgets( next_line, MAXLINELEN, fdin );

	if( tmp != (char *)NULL )
		input_status = INPUT_OK;
	else
		input_status = INPUT_NULL;
}

HIDDEN void
write_fields(void)
{
	int i, j;

	for( i=0 ; i<NO_OF_FIELDS ; i++ )
	{
		/* eliminate trailing blanks */
		j = strlen( curr_rec[i] ) - 1;
		while( j && ( isspace( curr_rec[i][j] ) || curr_rec[i][j] == '\012' || curr_rec[i][j] == '\015' ) )
			j--;
		curr_rec[i][++j] = '\0';

		/* skip leading blanks */
		j = 0;
		while( curr_rec[i][j] != '\0' && isspace( curr_rec[i][j] ) )
			j++;
		fprintf( fdtmp, "%s,", &curr_rec[i][j] );
	}
	fprintf( fdtmp, "\n" );
}

HIDDEN void
do_silly_nastran_shortcuts(void)
{
	int field_no;

	for( field_no=0 ; field_no < NO_OF_FIELDS ; field_no++ )
	{
		if( !strcmp( curr_rec[field_no], "=" ) )
		{
			strcpy( curr_rec[field_no], prev_rec[field_no] );
		}
		else if( !strcmp( curr_rec[field_no], "==" ) )
		{
			while( field_no < NO_OF_FIELDS )
			{
				strcpy( curr_rec[field_no], prev_rec[field_no] );
				field_no++;
			}
		}
		else if( curr_rec[field_no][0] == '*' )
		{
			int i=0;

			while( curr_rec[field_no][++i] == '(' );

			if( strchr( prev_rec[field_no], '.' ) )
			{
				fastf_t a, b;

				a = atof( prev_rec[field_no] );
				b = atof( &curr_rec[field_no][i] );
				sprintf( curr_rec[field_no], "%-#*E", FIELD_LENGTH-6, a+b );
			}
			else
			{
				int a, b;

				a = atoi( prev_rec[field_no] );
				b = atoi( &curr_rec[field_no][i] );
				sprintf( curr_rec[field_no], "%d", a+b );
			}
		}
	}
}

HIDDEN void
get_large_field_input(FILE *fd, int write_flag)
{
	char **tmp_rec;
	int field_no;
	int card_len;
	int last_field;
	int i;

	tmp_rec = prev_rec;
	prev_rec = curr_rec;
	curr_rec = tmp_rec;

	for( field_no=0 ; field_no < NO_OF_FIELDS ; field_no ++ )
		curr_rec[field_no][0] = '\0';

	card_len = strlen( line );
	last_field = (card_len - 8)/16 + 1;
	if( ((last_field - 1) * 16 + 8) < card_len )
		last_field++;
	if( last_field > 5 )
		last_field = 5;
	strncpy( curr_rec[0], line, 8 );
	curr_rec[0][8] = '\0';
	for( field_no=1 ; field_no < last_field ; field_no++ )
	{
		strncpy( curr_rec[field_no], &line[field_no*16 - 8], 16 );
		curr_rec[field_no][16] = '\0';
	}

	/* remove the newline from the end of the last field */
	i = strlen( curr_rec[last_field-1] ) - 1;
	while( isspace( curr_rec[last_field-1][i] ) || curr_rec[last_field-1][i] == '\012' || curr_rec[last_field-1][i] == '\015' )
		i--;
	curr_rec[last_field-1][++i] = '\0';

	if( next_line[0] == '*' )
	{
		if( !get_next_record( fd, 0, 0 ) )
		{
			bu_log( "unexpected end of INPUT at line #%d\n", line_count );
			bu_bomb( "unexpected end of INPUT\n" );
		}

		card_len = strlen( line );
		last_field = (card_len - 8)/16 + 1;
		if( ((last_field - 1) * 16 + 8) < card_len )
			last_field++;
		if( last_field > 5 )
			last_field = 5;
		last_field += 4;
		for( field_no=5 ; field_no < last_field ; field_no++ )
		{
			strncpy( curr_rec[field_no], &line[(field_no-4)*16 - 8], 16 );
			curr_rec[field_no][16] = '\0';
		}
	}

	if( write_flag )
		write_fields();
}

HIDDEN void
get_small_field_input(FILE *fd, int write_flag)
{
	char **tmp_rec;
	int field_no;
	int card_len;
	int last_field;

	tmp_rec = prev_rec;
	prev_rec = curr_rec;
	curr_rec = tmp_rec;

	for( field_no=0 ; field_no < NO_OF_FIELDS ; field_no ++ )
		curr_rec[field_no][0] = '\0';

	card_len = strlen( line );
	last_field = card_len/8 + 1;
	if( (last_field * 8) < card_len )
		last_field++;
	if( last_field > 9 )
		last_field = 9;
	strncpy( curr_rec[0], line, 8 );
	curr_rec[0][8] = '\0';
	for( field_no=2 ; field_no < last_field+1 ; field_no++ )
	{
		strncpy( curr_rec[field_no-1], &line[(field_no-1)*8], 8 );
		curr_rec[field_no-1][8] = '\0';
	}

	if( next_line[0] == '+' )
	{
		if( !get_next_record( fd, 0, 0 ) )
		{
			bu_log( "unexpected end of INPUT at line #%d\n", line_count );
			bu_bomb( "unexpected end of INPUT\n" );
		}

		card_len = strlen( line );
		last_field = card_len/8 + 1;
		if( (last_field * 8) < card_len )
			last_field++;
		if( last_field > 9 )
			last_field = 9;
		last_field += 9;
		for( field_no=10 ; field_no < last_field+1 ; field_no++ )
		{
			strncpy( curr_rec[field_no-1], &line[(field_no-9)*8], 8 );
			curr_rec[field_no-1][8] = '\0';
		}
	}

	if( write_flag )
		write_fields();
}

HIDDEN void
get_free_form_input(FILE *fd, int write_flag)
{
	char **tmp_rec;
	int field_no;
	int i, j;

	i = (-1);
	while( isspace( line[++i] ) );
	if( line[i] == '=' && isdigit( line[i+1] ) )
	{
		int count;

		count = atoi( &line[i+1] );

		i = (-1);
		while( isspace( prev_line[++i] ) );
		if( prev_line[i] == '=' && isdigit( prev_line[i+1] ) )
		{
			bu_log( "Cannot use consecutive replication cards:\n" );
			bu_log( "%s", prev_line );
			bu_log( "%s", line );
			bu_bomb( "ERROR: Cannot use consecutive replication cards\n" );
		}

		for( i=0 ; i<count ; i++ )
		{
			strcpy( line, prev_line );
			get_free_form_input( fd, write_flag );
		}
		return;
	}
	else
	{
		tmp_rec = prev_rec;
		prev_rec = curr_rec;
		curr_rec = tmp_rec;

		for( field_no=0 ; field_no < NO_OF_FIELDS ; field_no ++ )
			curr_rec[field_no][0] = '\0';

		field_no = (-1);
		i = 0;
		while( ++field_no < NO_OF_FIELDS && line[i] != '\0' )
		{
			while( line[i] != '\0' && isspace( line[i] ) )
				i++;
			j = (-1);
			while( line[i] != '\0' && line[i] != ',' && !isspace( line[i] ) )
				curr_rec[field_no][++j] = line[i++];
			curr_rec[field_no][++j] = '\0';
			if( line[i] == ',' )
				i++;
		}

		if( strchr( curr_rec[9], '+' ) )
		{
			/* continuation card */
			if( !get_next_record( fd, 0, 0 ) )
			{
				bu_log( "unexpected end of INPUT at line #%d\n", line_count );
				bu_bomb( "unexpected end of INPUT\n" );
			}

			i = 0;
			while( ++field_no < NO_OF_FIELDS && line[i] != '\0' )
			{
				while( line[i] != '\0' && isspace( line[i] ) )
					i++;
				j = (-1);
				while( line[i] != '\0' && line[i] != ',' && !isspace( line[i] ) )
					curr_rec[field_no][++j] = line[i++];
				curr_rec[field_no][++j] = '\0';
				if( line[i] == ',' )
					i++;
			}
		}
	}

	do_silly_nastran_shortcuts();

	if( write_flag )
		write_fields();

}

HIDDEN int
get_next_record( FILE *fd, int call_input, int write_flag )
{
	char *tmp;
	int form;
	int i;

	form = UNKNOWN;

	/* read next line of input, skipping comments */
	while( 1 )
	{
		line_count++;
		tmp = fgets( prev_line, MAXLINELEN, fd );
		if( !tmp || prev_line[0] != '$' )
			break;
	}

	/* Convert to all UPPER case */
	i = (-1);
	while( prev_line[++i] != '\0' )
	{
		if( isalpha( prev_line[i] ) )
			prev_line[i] = toupper( prev_line[i] );
	}

	if( tmp == (char *)NULL )
	{
		/* encountered end of input file */
		if( input_status != INPUT_NULL )
		{
			/* still have "next_line" to process */
			tmp = prev_line;
			prev_line = line;
			line = next_line;
			next_line = tmp;

			/* set flag to indicate end of input */
			input_status = INPUT_NULL;
		}
		else	/* no more input */
			return( 0 );
	}
	else
	{
		/* put next line of input into "line"
		 * and save just read line in "next_line"
		 */
		tmp = prev_line;
		prev_line = line;
		line = next_line;
		next_line = tmp;
	}

	if( !call_input )
		return( 1 );

	/* check which format is being used */
	tmp = strchr( line, ',' );
	if( tmp && tmp - line < 10 )
		form = FREE_FIELD;
	else
	{
		tmp = strchr( line, '=' );
		if( tmp && tmp - line < 10 )
			form = FREE_FIELD;
	}

	/* if this is FREE_FIELD, call approporiate processor */
	if( form == FREE_FIELD )
	{
		get_free_form_input( fd, write_flag );
		return( 1 );
	}

	/* not FREE_FIELD, check for LARGE_FIELD */
	i = (-1);
	while( ++i < 8 && (isalpha( line[i] ) || isspace( line[i] )) );
	if( i < 8 && line[i] == '*' )
		form = LARGE_FIELD;

	if( form == LARGE_FIELD )
	{
		get_large_field_input( fd, write_flag );
		return( 1 );
	}

	/* default is SMALL_FIELD */
	form = SMALL_FIELD;
	get_small_field_input( fd, write_flag );
	return( 1 );

}

HIDDEN void
log_line(char *str)
{
	int i;

	i = (-1);
	while( ++i < MAXLINELEN && line[i] != '\n' )
		if( line[i] == '\0' )
			line[i] = ' ';
	bu_log( "%s:\n", str );
	bu_log( "%s", line );

}

HIDDEN void
convert_input(void)
{

	reset_input();

	while( get_next_record( fdin, 1, 1 ) );
}

HIDDEN int
convert_cs(struct coord_sys *cs)
{
	struct coord_sys *cs2;
	point_t tmp_orig, tmp_pt1, tmp_pt2;

	if( !cs->rid )
		return( 0 );

	for( BU_LIST_FOR( cs2, coord_sys, &coord_head.l ) )
	{
		if( cs2->cid != cs->rid )
			continue;
		break;
	}

	if( BU_LIST_IS_HEAD( &cs2->l, &coord_head.l ) )
	{
		bu_log( "A coordinate system is defined in terms of a non-existent coordinate system!!!\n" );
		exit( 1 );
	}

	if( convert_pt( cs->origin, cs2, tmp_orig ) )
		return( 1 );

	if( convert_pt( cs->v1, cs2, tmp_pt1 ) )
		return( 1 );

	if( convert_pt( cs->v2, cs2, tmp_pt2 ) )
		return( 1 );

	VMOVE( cs->origin, tmp_orig );
	VSUB2( cs->v3, tmp_pt1, cs->origin );
	VUNITIZE( cs->v3 );
	VSUB2( cs->v1, tmp_pt2, cs->origin );
	VCROSS( cs->v2, cs->v3, cs->v1 );
	VUNITIZE( cs->v2 );
	VCROSS( cs->v1, cs->v3, cs->v2 );
	cs->rid = 0;
	return( 0 );
}

HIDDEN int
convert_pt( const point_t pt, struct coord_sys *cs, point_t out_pt )
{
	point_t tmp_pt;
	fastf_t c1, c2, c3, c4;

	if( cs->rid )
	{
		if( convert_cs( cs ) )
			return( 1 );
	}

	switch( cs->type )
	{
		case CORD_CYL:
			c1 = pt[X] * cos( pt[Y] * bn_degtorad );
			c2 = pt[X] * sin( pt[Y] * bn_degtorad );
			VJOIN3( tmp_pt, cs->origin, c1, cs->v1, c2, cs->v2, pt[Z], cs->v3);
			VMOVE( out_pt, tmp_pt );
			break;

		case CORD_RECT:
			VJOIN3( tmp_pt, cs->origin, pt[X], cs->v1, pt[Y], cs->v2, pt[Z], cs->v3 );
			VMOVE( out_pt, tmp_pt );
			break;

		case CORD_SPH:
			c4 = pt[X] * sin( pt[Y] * bn_degtorad );
			c1 = c4 * cos( pt[Z] * bn_degtorad );
			c2 = c4 * sin( pt[Z] * bn_degtorad );
			c3 = pt[X] * cos( pt[Y] * bn_degtorad );
			VJOIN3( tmp_pt, cs->origin, c1, cs->v1, c2, cs->v2, c3, cs->v3);
			VMOVE( out_pt, tmp_pt );
			break;

		default:
			bu_log( "Unrecognized coordinate system type (%c) for cid=%d!!!\n",
				cs->type, cs->cid );
			exit( 1 );
	}
	return( 0 );
}

/* routine to convert a grid point ot BRL-CAD (default cartesian)
 *
 *	returns:
 *		0 on success
 *		1 if cannot convert
 */

HIDDEN int
convert_grid(int index)
{
	struct coord_sys *cs;
	point_t tmp_pt;

	if( !g_pts[index].cid )
		return( 0 );

	for( BU_LIST_FOR( cs, coord_sys, &coord_head.l ) )
	{
		if( cs->cid != g_pts[index].cid )
			continue;
		break;
	}

	if( BU_LIST_IS_HEAD( &cs->l, &coord_head.l ) )
	{
		bu_log( "No coordinate system defined for grid point #%d!!!\n", g_pts[index].gid );
		exit( 1 );
	}

	if( convert_pt( g_pts[index].pt, cs, tmp_pt ) )
		return( 1 );

	VMOVE( g_pts[index].pt, tmp_pt );
	g_pts[index].cid = 0;

	return( 0 );
}

HIDDEN int
get_gridi(int gid)
{
	int i;
	int found=(-1);

	for( i=0 ; i<grid_used ; i++ )
	{
		if( g_pts[i].gid != gid )
			continue;

		found = i;
		break;
	}

	if( found < 0 )
	{
		bu_log( "Grid point %d is not defined!!\n", gid );
		exit( 1 );
	}

	if( g_pts[found].cid )
	{
		if( !convert_grid( found ) )
		{
			bu_log( "Could not convert grid point #%d to BRL-CAD!!!\n", gid );
			exit( 1 );
		}
	}

	return( found );
}

HIDDEN void
get_grid(int gid, fastf_t *pt)
{
	int i;
	int found=(-1);

	for( i=0 ; i<grid_used ; i++ )
	{
		if( g_pts[i].gid != gid )
			continue;

		found = i;
		break;
	}

	if( found < 0 )
	{
		bu_log( "Grid point %d is not defined!!\n", gid );
		exit( 1 );
	}

	if( g_pts[found].cid )
	{
		if( !convert_grid( found ) )
		{
			bu_log( "Could not convert grid point #%d to BRL-CAD!!!\n", gid );
			exit( 1 );
		}
	}

	VMOVE( pt, g_pts[found].pt );
}

HIDDEN void
get_coord_sys(void)
{
	int form;
	char type;
	char *ptr;
	struct coord_sys *cs;
	int i,gid;
	double tmp[3];
	point_t tmp_pt;

	form = atoi( &curr_rec[0][4] );
	if( form != 1 && form != 2 )
	{
		bu_log( "unrecognized form for coordinate system definition (%d):\n", form );
		bu_log( "%s\n", line );
		return;
	}
	type = curr_rec[0][5];

	if( type != CORD_CYL && type != CORD_RECT && type != CORD_SPH )
	{
		bu_log( "unrecognized coordinate system type (%c):\n", type );
		bu_log( "%s\n", line );
		return;
	}

	ptr = strtok( line, delims );
	ptr = strtok( (char *)NULL, delims );
	if( !ptr )
	{
		log_line( "Incomplete coordinate system definition" );
		return;
	}

	BU_GETSTRUCT( cs, coord_sys );

	switch( form )
	{
		case 1:
			cs->type = type;
			cs->cid = atoi( curr_rec[1] );
			gid = atoi( curr_rec[2] );
			get_grid( gid, cs->origin );
			gid = atoi( curr_rec[3] );
			get_grid( gid, tmp_pt );
			VSUB2( cs->v3, tmp_pt, cs->origin );
			VUNITIZE( cs->v3 );
			gid = atoi( curr_rec[4] );
			get_grid( gid, tmp_pt );
			VSUB2( cs->v1, tmp_pt, cs->origin );
			VCROSS( cs->v2, cs->v3, cs->v1 );
			VUNITIZE( cs->v2 );
			VCROSS( cs->v1, cs->v2, cs->v3 );
			BU_LIST_INSERT( &coord_head.l, &cs->l );

			if( !strlen( curr_rec[5] ) )
				break;

			BU_GETSTRUCT( cs, coord_sys );
			cs->type = type;
			cs->cid = atoi( curr_rec[5] );
			gid = atoi( curr_rec[6] );
			get_grid( gid, cs->origin );
			gid = atoi( curr_rec[7] );
			get_grid( gid, tmp_pt );
			VSUB2( cs->v3, tmp_pt, cs->origin );
			VUNITIZE( cs->v3 );
			gid = atoi( curr_rec[8] );
			get_grid( gid, tmp_pt );
			VSUB2( cs->v1, tmp_pt, cs->origin );
			VCROSS( cs->v2, cs->v3, cs->v1 );
			VUNITIZE( cs->v2 );
			VCROSS( cs->v1, cs->v2, cs->v3 );
			BU_LIST_INSERT( &coord_head.l, &cs->l );

			break;
		case 2:
			cs->type = type;
			cs->cid = atoi( curr_rec[1] );
			cs->rid = atoi( curr_rec[2] );
			for( i=0 ; i<3 ; i++ )
				tmp[i] = atof( curr_rec[3+i] );
			VMOVE( cs->origin, tmp );
			for( i=0 ; i<3 ; i++ )
				tmp[i] = atof( curr_rec[6+i] );
			VMOVE( cs->v1, tmp );
			for( i=0 ; i<3 ; i++ )
				tmp[i] = atof( curr_rec[9+i] );
			VMOVE( cs->v2, tmp );

			if( !cs->rid )
			{
				point_t tmp_pt1, tmp_pt2;

				/* this coordinate system is defined in terms of the default */
				VMOVE( tmp_pt1, cs->v1 );
				VMOVE( tmp_pt2, cs->v2 );

				VSUB2( cs->v3, tmp_pt1, cs->origin );
				VUNITIZE( cs->v3 );
				VSUB2( cs->v1, tmp_pt2, cs->origin );
				VCROSS( cs->v2, cs->v3, cs->v1 );
				VUNITIZE( cs->v2 );
				VCROSS( cs->v1, cs->v3, cs->v2 );
			}

			BU_LIST_INSERT( &coord_head.l, &cs->l );
			break;
	}
}

HIDDEN int
convert_all_cs(void)
{
	int ret=0;
	struct coord_sys *cs;

	for( BU_LIST_FOR( cs, coord_sys, &coord_head.l ) )
	{
		if( convert_cs( cs ) )
			ret = 1;
	}

	return( ret );
}

HIDDEN int
convert_all_pts(void)
{
	int i;
	int ret=0;

	for( i=0 ; i<grid_used ; i++ )
	{
		if( convert_grid( i ) )
			ret = 1;
	}

	return( ret );
}

HIDDEN int
get_pid_index(int pid)
{
	struct pshell *psh;
	int index=0;

	if( pid == 0 )
		return( 0 );

	for( BU_LIST_FOR( psh, pshell, &pshell_head.l ) )
	{
		index++;
		if( psh->pid == pid )
			return( index );
	}

	return( 0 );
}

HIDDEN void
get_cquad4(void)
{
	int pid;
	int g1, g2, g3, g4;
	int gin1, gin2, gin3, gin4;
	point_t pt1, pt2, pt3, pt4;
	struct vertex **v[3];
	struct faceuse *fu;
	struct shell *s;
	int pid_index=0;

	pid = atoi( curr_rec[2] );

	pid_index = get_pid_index( pid );

	g1 = atoi( curr_rec[3] );

	g2 = atoi( curr_rec[4] );

	g3 = atoi( curr_rec[5] );

	g4 = atoi( curr_rec[6] );

	gin1 = get_gridi( g1 );
	gin2 = get_gridi( g2 );
	gin3 = get_gridi( g3 );
	gin4 = get_gridi( g4 );

	VSCALE( pt1, g_pts[gin1].pt, conv[units] );
	VSCALE( pt2, g_pts[gin2].pt, conv[units] );
	VSCALE( pt3, g_pts[gin3].pt, conv[units] );
	VSCALE( pt4, g_pts[gin4].pt, conv[units] );

	if( !nmg_model && !pid )
	{
		struct nmgregion *r;

		nmg_model = nmg_mm();
		r = nmg_mrsv( nmg_model );
		nmg_shell = BU_LIST_FIRST( shell, &r->s_hd );
	}

	if( !pid )
		s = nmg_shell;
	else
	{
		struct pshell *psh;
		int found=0;

		/* find pshell entry for this pid */
		for( BU_LIST_FOR( psh, pshell, &pshell_head.l ) )
		{
			if( psh->pid == pid )
			{
				found = 1;
				break;
			}
		}

		if( !found )
		{
			bu_log( "Cannot find PSHELL entry for a CQUAD4 element (ignoring)!!!\n" );
			write_fields();
			return;
		}

		if( psh->s )
			s = psh->s;
		else
		{
			struct model *m;
			struct nmgregion *r;

			m = nmg_mm();
			r = nmg_mrsv( m );
			s = BU_LIST_FIRST( shell, &r->s_hd );
			psh->s = s;
		}
	}

	v[0] = &g_pts[gin1].v[pid_index];
	v[1] = &g_pts[gin2].v[pid_index];
	v[2] = &g_pts[gin3].v[pid_index];

	fu = nmg_cmface( s, v, 3 );

	if( !g_pts[gin1].v[pid_index]->vg_p )
		nmg_vertex_gv( g_pts[gin1].v[pid_index], pt1 );
	if( !g_pts[gin2].v[pid_index]->vg_p )
		nmg_vertex_gv( g_pts[gin2].v[pid_index], pt2 );
	if( !g_pts[gin3].v[pid_index]->vg_p )
		nmg_vertex_gv( g_pts[gin3].v[pid_index], pt3 );
	nmg_calc_face_g( fu );

	v[0] = &g_pts[gin1].v[pid_index];
	v[1] = &g_pts[gin3].v[pid_index];
	v[2] = &g_pts[gin4].v[pid_index];

	fu = nmg_cmface( s, v, 3 );

	if( !g_pts[gin4].v[pid_index]->vg_p )
		nmg_vertex_gv( g_pts[gin4].v[pid_index], pt4 );
	nmg_calc_face_g( fu );
}

HIDDEN void
get_ctria3(void)
{
	int pid;
	int g1, g2, g3;
	int gin1, gin2, gin3;
	point_t pt1, pt2, pt3;
	struct vertex **v[3];
	struct faceuse *fu;
	struct shell *s;
	struct pshell *psh;
	int pid_index=0;

	pid = atoi( curr_rec[2] );

	pid_index = get_pid_index( pid );

	g1 = atoi( curr_rec[3] );

	g2 = atoi( curr_rec[4] );

	g3 = atoi( curr_rec[5] );

	gin1 = get_gridi( g1 );
	gin2 = get_gridi( g2 );
	gin3 = get_gridi( g3 );

	v[0] = &g_pts[gin1].v[pid_index];
	v[1] = &g_pts[gin2].v[pid_index];
	v[2] = &g_pts[gin3].v[pid_index];

	VSCALE( pt1, g_pts[gin1].pt, conv[units] );
	VSCALE( pt2, g_pts[gin2].pt, conv[units] );
	VSCALE( pt3, g_pts[gin3].pt, conv[units] );

	if( !nmg_model && !pid )
	{
		struct nmgregion *r;

		nmg_model = nmg_mm();
		r = nmg_mrsv( nmg_model );
		nmg_shell = BU_LIST_FIRST( shell, &r->s_hd );
	}

	if( !pid )
		s = nmg_shell;
	else
	{
		int found=0;

		/* find pshell entry for this pid */
		for( BU_LIST_FOR( psh, pshell, &pshell_head.l ) )
		{
			if( psh->pid == pid )
			{
				found = 1;
				break;
			}
		}

		if( !found )
		{
			bu_log( "Cannot find PSHELL entry for a CTRIA3 element (ignoring)!!!\n" );
			write_fields();
			return;
		}

		if( psh->s )
			s = psh->s;
		else
		{
			struct model *m;
			struct nmgregion *r;

			m = nmg_mm();
			r = nmg_mrsv( m );
			s = BU_LIST_FIRST( shell, &r->s_hd );
			psh->s = s;
		}
	}

	fu = nmg_cmface( s, v, 3 );

	if( !g_pts[gin1].v[pid_index]->vg_p )
		nmg_vertex_gv( g_pts[gin1].v[pid_index], pt1 );
	if( !g_pts[gin2].v[pid_index]->vg_p )
		nmg_vertex_gv( g_pts[gin2].v[pid_index], pt2 );
	if( !g_pts[gin3].v[pid_index]->vg_p )
		nmg_vertex_gv( g_pts[gin3].v[pid_index], pt3 );

	nmg_calc_face_g( fu );
}

HIDDEN void
get_cbar(void)
{
	int eid, pid;
	int g1, g2;
	point_t pt1, pt2;
	fastf_t radius;
	vect_t height;
	struct pbar *pb;
	char cbar_name[NAMESIZE];

	eid = atoi( curr_rec[1] );

	pid = atoi( curr_rec[2] );
	if( !pid )
	{
		if( bar_def_pid )
			pid = bar_def_pid;
		else
			pid = eid;
	}

	g1 = atoi( curr_rec[3] );

	g2 = atoi( curr_rec[4] );

	get_grid( g1, pt1 );
	get_grid( g2, pt2 );

	for( BU_LIST_FOR( pb, pbar, &pbar_head.l ) )
	{
		if( pb->pid == pid )
			break;
	}

	if( BU_LIST_IS_HEAD( &pb->l, &pbar_head.l ) )
	{
		log_line( "Non-existent PID referenced in CBAR" );
		return;
	}

	VSCALE( pt1, pt1, conv[units] );
	VSCALE( pt2, pt2, conv[units] );

	radius = sqrt( pb->area/bn_pi );
	radius = radius * conv[units];

	VSUB2( height, pt2, pt1 );

	sprintf( cbar_name, "cbar.%d", eid );
	mk_rcc( fdout, cbar_name, pt1, height, radius );

	mk_addmember( cbar_name, &pb->head.l, NULL, WMOP_UNION );
}

int
main(int argc, char **argv)
{
	int c;
	int i;
	struct pshell *psh;
	struct pbar *pb;
	struct wmember head;
	struct wmember all_head;
	char *nastran_file = "Converted from NASTRAN file (stdin)";

	fdin = stdin;

	units = INCHES;

	/* XXX These need to be improved */
	tol.magic = BN_TOL_MAGIC;
	tol.dist = 0.005;
	tol.dist_sq = tol.dist * tol.dist;
	tol.perp = 1e-6;
	tol.para = 1 - tol.perp;

	while( (c=bu_getopt( argc, argv, "x:X:t:ni:o:m")) != EOF )
	{
		switch( c )
		{
			case 'x':
				sscanf( bu_optarg, "%x", (unsigned int *)&rt_g.debug );
				bu_printb( "librt RT_G_DEBUG", RT_G_DEBUG, DEBUG_FORMAT );
				bu_log("\n");
				break;
			case 'X':
				sscanf( bu_optarg, "%x", (unsigned int *)&rt_g.NMG_debug );
				bu_printb( "librt rt_g.NMG_debug", rt_g.NMG_debug, NMG_DEBUG_FORMAT );
				bu_log("\n");
				break;
			case 't':		/* calculational tolerance */
				tol.dist = atof( bu_optarg );
				tol.dist_sq = tol.dist * tol.dist;
			case 'n':
				polysolids = 0;
				break;
			case 'm':
				units = MM;
				break;
			case 'i':
				nastran_file = bu_optarg;
				fdin = fopen( bu_optarg, "r" );
				if( fdin == (FILE *)NULL )
				{
					bu_log( "Cannot open NASTRAN file (%s) for reading!!!\n", bu_optarg );
					bu_log( "Usage", argv[0] );
					exit( 1 );
				}
				break;
			case 'o':
				output_file = bu_optarg;
				break;
		}
	}

	fdout = wdb_fopen( output_file );
	if( fdout == NULL )
	{
		bu_log( "Cannot open BRL-CAD file (%s) for writing!!!\n", output_file );
		bu_log( "Usage", argv[0] );
		exit( 1 );
	}

	if( !fdin || !fdout )
	{
		bu_log( Usage, argv[0] );
		exit( 1 );
	}

	line = (char *)bu_malloc( MAXLINELEN, "line" );
	next_line = (char *)bu_malloc( MAXLINELEN, "next_line" );
	prev_line = (char *)bu_malloc( MAXLINELEN, "prev_line" );
	curr_rec = (char **)bu_calloc( NO_OF_FIELDS, sizeof( char *), "curr_rec" );
	for( i=0 ; i<NO_OF_FIELDS ; i++ )
		curr_rec[i] = (char *)bu_malloc( sizeof( char )*FIELD_LENGTH, "curr_rec[i]" );
	prev_rec = (char **)bu_calloc( NO_OF_FIELDS, sizeof( char *), "prev_rec" );
	for( i=0 ; i<NO_OF_FIELDS ; i++ )
		prev_rec[i] = (char *)bu_malloc( sizeof( char )*FIELD_LENGTH, "prev_rec[i]" );

	/* first pass, find start of NASTRAN "bulk data" */
	start_off = (-1);
	bulk_data_start_line = 0;
	while( fgets( line, MAXLINELEN, fdin ) )
	{
		bulk_data_start_line++;
		if( strncmp( line, "BEGIN BULK", 10 ) )
			continue;

		start_off = ftell( fdin );
		break;
	}

	if( start_off < 0 )
	{
		bu_log( "Cannot find start of bulk data in NASTRAN file!!!\n" );
		bu_log( "Usage", argv[0] );
		exit( 1 );
	}

	/* convert BULK data deck into something reasonable */
	fdtmp = tmpfile();
	if( fdtmp == NULL )
	{
		perror( argv[0] );
		bu_bomb( "Cannot open temporary file\n" );
	}
	convert_input();

	/* initialize some lists */
	BU_LIST_INIT( &coord_head.l );
	BU_LIST_INIT( &pbar_head.l );
	BU_LIST_INIT( &pshell_head.l );
	BU_LIST_INIT( &all_head.l );

	nmg_model = (struct model *)NULL;

	/* count grid points */
	fseek( fdtmp, 0, SEEK_SET );
	while( fgets( line, MAXLINELEN, fdtmp  ) )
	{
		if( !strncmp( line, "GRID", 4 ) )
			grid_count++;
	}
	if( !grid_count )
	{
		bu_log( "No geometry in this NASTRAN file!!!\n" );
		exit( 1 );
	}

	/* get default values and properties */
	fseek( fdtmp, 0, SEEK_SET );
	while( get_next_record( fdtmp, 1, 0 ) )
	{
		if( !strncmp( curr_rec[0], "BAROR", 5 ) )
		{
			/* get BAR defaults */
			bar_def_pid = atoi( curr_rec[2] );
		}
		else if( !strncmp( curr_rec[0], "PBAR", 4 ) )
		{
			struct pbar *pb;

			BU_GETSTRUCT( pb, pbar );

			pb->pid = atoi( curr_rec[1] );
			pb->mid = atoi( curr_rec[2] );
			pb->area = atof( curr_rec[3] );

			BU_LIST_INIT( &pb->head.l );

			BU_LIST_INSERT( &pbar_head.l, &pb->l );
		}
		else if( !strncmp( curr_rec[0], "PSHELL", 6 ) )
		{
			BU_GETSTRUCT( psh, pshell );

			psh->s = (struct shell *)NULL;
			psh->pid = atoi( curr_rec[1] );
			psh->mid = atoi( curr_rec[2] );
			psh->thick = atof( curr_rec[3] );
			BU_LIST_INSERT( &pshell_head.l, &psh->l );
			pshell_count++;
		}
	}

	/* allocate storage for grid points */
	g_pts = (struct grid_point *)bu_calloc( grid_count, sizeof( struct grid_point ), "grid points" );

	/* get all grid points */
	fseek( fdtmp, 0, SEEK_SET );
	while( get_next_record( fdtmp, 1, 0 ) )
	{
		int gid;
		int cid;
		double tmp[3];

		if( strncmp( curr_rec[0], "GRID", 4 ) )
			continue;

		gid = atoi( curr_rec[1] );
		cid = atoi( curr_rec[2] );

		for( i=0 ; i<3 ; i++ )
		{
			tmp[i] = atof( curr_rec[i+3] );
		}

		g_pts[grid_used].gid = gid;
		g_pts[grid_used].cid = cid;
		g_pts[grid_used].v = (struct vertex **)bu_calloc( pshell_count + 1, sizeof( struct vertex *), "g_pts vertex array" );;
		VMOVE( g_pts[grid_used].pt, tmp );
		grid_used++;
	}


	/* find coordinate systems */
	fseek( fdtmp, 0, SEEK_SET );
	while( get_next_record( fdtmp, 1, 0 ) )
	{
		if( strncmp( curr_rec[0], "CORD", 4 ) )
			continue;

		get_coord_sys();
	}
#if 0
	bu_log( "Original grid:\n" );
	for( i=0 ; i<grid_used ; i++ )
	{
		bu_log( "Grid point: gid=%d, cid=%d, (%g %g %g)\n", g_pts[i].gid,
			g_pts[i].cid, V3ARGS( g_pts[i].pt ) );
	}

	bu_log( "\nOriginal coordinate systems:\n" );
	for( BU_LIST_FOR( cs, coord_sys, &coord_head.l ) )
	{
		bu_log( "cid=%d, type=%c, rid=%d\n", cs->cid, cs->type, cs->rid );
		bu_log( "\torigin=(%g %g %g)\n", V3ARGS( cs->origin ) );
		bu_log( "\tv1=(%g %g %g)\n", V3ARGS( cs->v1 ) );
		bu_log( "\tv2=(%g %g %g)\n", V3ARGS( cs->v2 ) );
		bu_log( "\tv3=(%g %g %g)\n", V3ARGS( cs->v3 ) );
	}
#endif
	/* convert everything to BRL-CAD coordinate system */
	i = 0;
	while( convert_all_cs() || convert_all_pts() )
	{
		i++;
		if( i > 10 )
		{
			bu_log( "Cannot convert to default coordinate system, check for circular definition\n" );
			exit( 1 );
		}
	}

#if 0
	bu_log( "Converted grid:\n" );
	for( i=0 ; i<grid_used ; i++ )
	{
		bu_log( "Grid point: gid=%d, cid=%d, (%g %g %g)\n", g_pts[i].gid,
			g_pts[i].cid, V3ARGS( g_pts[i].pt ) );
	}

	bu_log( "\nConverted coordinate systems:\n" );
	for( BU_LIST_FOR( cs, coord_sys, &coord_head.l ) )
	{
		bu_log( "cid=%d, type=%c, rid=%d\n", cs->cid, cs->type, cs->rid );
		bu_log( "\torigin=(%g %g %g)\n", V3ARGS( cs->origin ) );
		bu_log( "\tv1=(%g %g %g)\n", V3ARGS( cs->v1 ) );
		bu_log( "\tv2=(%g %g %g)\n", V3ARGS( cs->v2 ) );
		bu_log( "\tv3=(%g %g %g)\n", V3ARGS( cs->v3 ) );
	}
#endif

	mk_id( fdout, nastran_file );

	/* get elements */
	fseek( fdtmp, 0, SEEK_SET );
	while( get_next_record( fdtmp, 1, 0 ) )
	{
		if( !strncmp( curr_rec[0], "CBAR", 4 ) )
			get_cbar();
		else if( !strncmp( curr_rec[0], "CROD", 4 ) )
			get_cbar();
		else if( !strncmp( curr_rec[0], "CTRIA3", 6 ) )
			get_ctria3();
		else if( !strncmp( curr_rec[0], "CQUAD4", 6 ) )
			get_cquad4();
	}

	if( nmg_model )
	{
		nmg_rebound( nmg_model, &tol );
		if( polysolids )
			write_shell_as_polysolid( fdout, "pshell.0", nmg_shell );
		else
			mk_nmg( fdout, "pshell.0", nmg_model );
	}

	BU_LIST_INIT( &head.l );
	for( BU_LIST_FOR( psh, pshell, &pshell_head.l ) )
	{
		struct model *m;
		char name[NAMESIZE];

		if( !psh->s )
			continue;

		m = nmg_find_model( &psh->s->l.magic );
		nmg_rebound( m, &tol );
		nmg_fix_normals( psh->s, &tol );
		if( psh->thick > tol.dist )
		{
			nmg_model_face_fuse( m, &tol );
			nmg_hollow_shell( psh->s, psh->thick*conv[units], 1, &tol );
		}
		sprintf( name, "pshell.%d", psh->pid );
		if( polysolids )
			write_shell_as_polysolid( fdout, name, psh->s );
		else
			mk_nmg( fdout, name, m );

		mk_addmember( name, &head.l, NULL, WMOP_UNION );
	}
	if( BU_LIST_NON_EMPTY( &head.l ) )
	{
		mk_lfcomb( fdout, "shells", &head, 0 );
		mk_addmember( "shells", &all_head.l, NULL, WMOP_UNION );
	}

	BU_LIST_INIT( &head.l );
	for( BU_LIST_FOR( pb, pbar, &pbar_head.l ) )
	{
		char name[NAMESIZE];

		if( BU_LIST_IS_EMPTY( &pb->head.l ) )
			continue;

		sprintf( name, "pbar_group.%d", pb->pid );
		mk_lfcomb( fdout, name, &pb->head, 0 );

		mk_addmember( name, &head.l, NULL, WMOP_UNION );
	}
	if( BU_LIST_NON_EMPTY( &head.l ) )
	{
		mk_lfcomb( fdout, "pbars", &head, 0 );
		mk_addmember( "pbars", &all_head.l, NULL, WMOP_UNION );
	}

	if( BU_LIST_NON_EMPTY( &all_head.l ) )
	{
		mk_lfcomb( fdout, "all", &all_head, 0 );
	}
	wdb_close(fdout);
	return 0;
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
