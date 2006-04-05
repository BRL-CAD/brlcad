/*                         V D E C K . C
 * BRL-CAD
 *
 * Copyright (c) 1990-2006 United States Government as represented by
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
/** @file vdeck.c
 *
 *  Author -
 *	Gary S. Moss
 *	U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground
 *	Maryland 21005-5066
 *	(301)278-6647 or AV-298-6647
 *
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066
 *
 */
#ifndef lint
static const char RCSid[] = "@(#)$Header$ (BRL)";
#endif

/*
	Derived from KARDS, written by Keith Applin.

	Generate a COM-GEOM card images suitable for input to gift5
	(also gift(1V)) from an mged(1V) target description.

	There are 3 files generated at a time, the Solid table, Region table,
	and Ident table, which when concatenated in that order, make a
	COM-GEOM deck.  The record formats in the order that they appear, are
	described below, and are strictly column oriented.

	Note that the Solid table begins with a Title and a Control card, the
	rest of the record types appear once for each object, that is, one
	Solid record for each Solid, one Region and one
  	Ident record for each Region as totaled on the Control card, however,
  	the Solid and Region records may span more than 1 card.

----------------------------------------------------------------------------
|File|Record  :             Contents              :       Format           |
|----|---------------------------------------------------------------------|
| 1  |Title   : target_units, title               : a2,3x,a60              |
|    |Control : #_of_solids, #_of_regions         : i5,i5                  |
|    |Solid   : sol_#,sol_type,params.,comment    : i5,a5,6f10.0,a10       |
|    | cont'  : sol_#,parameters,comment          : i5,5x,6f10.0,a10       |
|----|---------------------------------------------------------------------|
| 2  |Region  : reg_#,(op,[sol/reg]_#)*,comment   : i5,1x,9(a2,i5),1x,a10  |
|    | cont'  : reg_#,(op,[sol/reg]_#)*,comment   : i5,1x,9(a2,i5),1x,a10  |
|----|---------------------------------------------------------------------|
| 3  |Flag    : a -1 marks end of region table    : i5                     |
|    |Idents  : reg_#,ident,space,mat,%,descriptn : 5i5,5x,a50             |
|--------------------------------------------------------------------------|

 */
#include "common.h"

#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <math.h>
#ifdef HAVE_STRING_H
#  include <string.h>
#else
#  include <strings.h>
#endif
#ifdef HAVE_SYS_TYPES_H
#  include <sys/types.h>
#endif
#ifdef HAVE_SYS_WAIT_H
#  include <sys/wait.h>
#endif
#ifdef HAVE_UNISTD_H
#  include <unistd.h>
#endif

#include "machine.h"
#include "vmath.h"
#include "rtstring.h"
#include "raytrace.h"
#include "rtgeom.h"

#include "./vextern.h"

#include "../librt/debug.h"


int	debug = 0;

char	*cmd[] = {
	"",
	"C O M M A N D                  D E S C R I P T I O N",
	"",
	"deck [output file prefix]      Produce COM GEOM card deck.",
	"erase                          Erase current list of objects.",
	"insert [object[s]]             Add an object to current list.",
	"list [object[s]]               Display current list of selected objects.",
	"number [solid] [region]        Specify starting numbers for objects.",
	"quit                           Terminate run.",
	"remove [object[s]]             Remove an object from current list.",
	"sort                           Sort table of contents alphabetically.",
	"toc [object[s]]                Table of contents of solids database.",
	"! [shell command]              Execute a UNIX shell command.",
	"",
	"NOTE:",
	"First letter of command is sufficient, and all arguments are optional.",
	"Objects may be specified with string matching operators (*, [], -, ? or \\)",
	"as in the UNIX shell.",
	0
};

char	*usage[] = {
	"",
	"v d e c k ($Revision$)",
	"Make COMGEOM decks of objects from a \"mged\" file suitable as",
	"input to GIFT5 or gift(1V).",
	"",
	"Usage: vdeck file.g",
	"",
	0
};

/*  These arrays are now dynamically allocated by toc() */

char	**toc_list;		/* Sorted table of contents. */
char	**curr_list;		/* regions and solids to be processed. */
int	curr_ct = 0;
char	**tmp_list;		/* Temporary list of names */
int	tmp_ct = 0;

/* List of arguments from command line parser.				*/
char	*arg_list[MAXARG];
int	arg_ct = 0;

/* Structure used by setjmp() and longjmp() to save environment.	*/
jmp_buf	env;

/* File names and descriptors.						*/
char	*objfile;
FILE	*regfp;
struct bu_vls	rt_vls;
struct bu_vls	st_vls;
struct bu_vls	id_vls;
char	*rt_file;
FILE	*solfp;
char	*st_file;
FILE	*ridfp;
char	*id_file;

/* Counters.								*/
int	nns;		/* Solids.					*/
int	nnr;		/* Regions not members of other regions.	*/
int	ndir;		/* Entries in directory.			*/

/* Miscellaneous globals leftover from Keith's KARDS code.		*/
int		delsol = 0, delreg = 0;
char		buff[30];
long		savsol;		/* File postion of # of solids & regions */

/* Structures.								*/
mat_t		identity;

extern void		menu();
extern void		quit();

char			getarg();
void			quit(), abort_sig();

char			getcmd();
void			prompt();

void			addarb();
void			addtgc();
void			addtor();
void			addhalf();
void			addarbn();
void			addell();
void			addars();
void			deck();
void			itoa();
void			vls_blanks();
void			vls_itoa();
void			vls_ftoa_vec_cvt();
void			vls_ftoa_vec();
void			vls_ftoa_cvt();
void			vls_ftoa();
extern int		parsArg();
extern int		insert();
extern int		col_prt();
extern int		match();
extern int		delete();
extern int		shell();
extern int		cgarbs();
extern int		redoarb();

BU_EXTERN(void ewrite, (FILE *fp, const char *buf, unsigned bytes) );
BU_EXTERN(void blank_fill, (FILE *fp, int count) );

/* Head of linked list of solids */
struct soltab	sol_hd;

struct db_i	*dbip;		/* Database instance ptr */


/*
 *			P R O M P T
 *
 *  Print a non-newline-terminate string, and flush stdout
 */
void
prompt( fmt )
char	*fmt;
{
	fputs( fmt, stdout );
	fflush(stdout);
}

/*
 *			S O R T F U N C
 *
 *	Comparison function for qsort().
 */
static int
sortFunc( a, b )
#if __STDC__
const void	*a;		/* The exact template expected by qsort */
const void	*b;
#else
const genptr_t	a;
const genptr_t	b;
#endif
{
	const char **lhs = (const char **)a;
	const char **rhs = (const char **)b;

	return( strcmp( *lhs, *rhs ) );
}

/*
 *			M A I N
 */
int
main( argc, argv )
char	*argv[];
{
	setbuf( stdout, bu_malloc( BUFSIZ, "stdout buffer" ) );
	RT_LIST_INIT( &(sol_hd.l) );

	bu_vls_init( &rt_vls );
	bu_vls_init( &st_vls );
	bu_vls_init( &id_vls );

	if( ! parsArg( argc, argv ) )
	{
		menu( usage );
		return 1;
	}

	rt_init_resource( &rt_uniresource, 0, NULL );

	/* Build directory from object file.	 	*/
	if( db_dirbuild(dbip) < 0 )  {
		fprintf(stderr,"db_dirbuild() failure\n");
		return 1;
	}

	toc();		/* Build table of contents from directory.	*/

#if 0
	rt_g.debug |= DEBUG_TREEWALK;
#endif

	/*      C o m m a n d   I n t e r p r e t e r			*/
	(void) setjmp( env );/* Point of re-entry from aborted command.	*/
	prompt( CMD_PROMPT );
	while( 1 )
	{
		/* Return to default interrupt handler after every command,
		 allows exit from program only while command interpreter
		 is waiting for input from keyboard.
		 */
		(void) signal( SIGINT, quit );

		switch( getcmd( arg_list, 0 ) )
		{
		case DECK :
			deck( arg_list[1] );
			break;
		case ERASE :
			while( curr_ct > 0 )
				bu_free( curr_list[--curr_ct], "curr_list[ct]" );
			break;
		case INSERT :
			if( arg_list[1] == 0 )
			{
				prompt( "enter object[s] to insert: " );
				(void) getcmd( arg_list, arg_ct );
			}
			(void) insert( arg_list, arg_ct );
			break;
		case LIST :
			{
				register int	i;
				if( arg_list[1] == 0 )
				{
					(void) col_prt( curr_list, curr_ct );
					break;
				}
				for( tmp_ct = 0, i = 0; i < curr_ct; i++ )
					if( match( arg_list[1], curr_list[i] ) )
						tmp_list[tmp_ct++] = curr_list[i];
				(void) col_prt( tmp_list, tmp_ct );
				break;
			}
		case MENU :
			menu( cmd );
			prompt( PROMPT );
			continue;
		case NUMBER :
			if( arg_list[1] == 0 )
			{
				prompt( "enter number of 1st solid: " );
				(void) getcmd( arg_list, arg_ct );
				prompt( "enter number of 1st region: " );
				(void) getcmd( arg_list, arg_ct );
			}
			if( arg_list[1] )
				delsol = atoi( arg_list[1] ) - 1;
			if( arg_list[2] )
				delreg = atoi( arg_list[2] ) - 1;
			break;
		case REMOVE :
			if( arg_list[1] == 0 )
			{
				prompt( "enter object[s] to remove: " );
				(void) getcmd( arg_list, arg_ct );
			}
			(void) delete( arg_list );
			break;
		case RETURN :
			prompt( PROMPT );
			continue;
		case SHELL :
			if( arg_list[1] == 0 )
			{
				prompt( "enter shell command: " );
				(void) getcmd( arg_list, arg_ct );
			}
			(void) shell( arg_list );
			break;
		case SORT_TOC :
			qsort( (genptr_t)toc_list, (unsigned)ndir,
			    sizeof(char *), sortFunc );
			break;
		case TOC :
			list_toc( arg_list );
			break;
		case EOF :
		case QUIT :
			(void) printf( "quitting...\n" );
			goto out;
		default :
			(void) printf( "Invalid command\n" );
			prompt( PROMPT );
			continue;
		}
		prompt( CMD_PROMPT );
	}
out:
	return 0;
}

/*
 *			F L A T T E N _ T R E E
 *
 *  This routine turns a union tree into a flat string.
 */
void
flatten_tree( vls, tp, op, neg )
struct rt_vls	*vls;
union tree	*tp;
char		*op;
int		neg;
{
	int	bit;

	RT_VLS_CHECK( vls );

	switch( tp->tr_op )  {

	case OP_NOP:
		rt_log("NOP\n");
		return;

	case OP_SOLID:
		bit = tp->tr_a.tu_stp->st_bit;
		if( bit < 10000 )  {
			/* Old way, just use negative number in I5 field */
			rt_vls_strncat( vls, op, 2 );
			if(neg) bit = -bit;
		} else {
			/* New way, due to Tom Sullivan of Sandia. */
			/* "or" becomes "nr", "  " becomes "nn" */
			if(neg)  {
				if( *op == ' ' )
					rt_vls_strncat( vls, "nn", 2 );
				else if( *op == 'o' && op[1] == 'r' )
					rt_vls_strncat( vls, "nr", 2 );
				else
					rt_vls_strncat( vls, "??", 2 );
			} else {
				rt_vls_strncat( vls, op, 2 );
			}
		}
		vls_itoa( vls, bit, 5 );
		/* tp->tr_a.tu_stp->st_name */
		return;

	case OP_REGION:
		rt_log("REGION 'stp'=x%x\n",
		    tp->tr_a.tu_stp );
		return;

	default:
		rt_log("Unknown op=x%x\n", tp->tr_op );
		return;

	case OP_UNION:
		flatten_tree( vls, tp->tr_b.tb_left, "or", neg );
		flatten_tree( vls, tp->tr_b.tb_right, "or", 0 );
		break;
	case OP_INTERSECT:
		flatten_tree( vls, tp->tr_b.tb_left, op, neg );
		flatten_tree( vls, tp->tr_b.tb_right, "  ", 0 );
		break;
	case OP_SUBTRACT:
		flatten_tree( vls, tp->tr_b.tb_left, op, neg );
		flatten_tree( vls, tp->tr_b.tb_right, "  ", 1 );
		break;
	}
}

/*
 *			R E G I O N _ E N D
 *
 *  This routine will be called by db_walk_tree() once all the
 *  solids in this region have been visited.
 */
union tree *region_end( tsp, pathp, curtree, client_data )
register struct db_tree_state	*tsp;
struct db_full_path	*pathp;
union tree		*curtree;
genptr_t		client_data;
{
	struct directory	*dp;
	char			*fullname;
	struct rt_vls		ident;
	struct rt_vls		reg;
	struct rt_vls		flat;
	char			obuf[128];
	char			*cp;
	int			left;
	int			length;
	struct directory	*regdp = DIR_NULL;
	int			i;
	int			first;

	RT_VLS_INIT( &ident );
	RT_VLS_INIT( &reg );
	RT_VLS_INIT( &flat );
	fullname = db_path_to_string(pathp);

	dp = DB_FULL_PATH_CUR_DIR(pathp);
	dp->d_uses++;		/* instance number */

	/* For name, find pointer to region combination */
	for( i=0; i < pathp->fp_len; i++ )  {
		regdp = pathp->fp_names[i];
		if( regdp->d_flags & DIR_REGION )  break;
	}

	nnr++;			/* Start new region */

	/* Print an indicator of our progress */
	if( debug )
		(void) printf( "%4d:%s\n", nnr+delreg, fullname );

	/*
	 *  Write the boolean formula into the region table.
	 */

	/* Convert boolean tree into string of 7-char chunks */
	if( curtree->tr_op == OP_NOP )  {
		rt_vls_strcat( &flat, "" );
	} else {
		/* Rewrite tree so that all unions are at tree top */
		db_non_union_push( curtree, &rt_uniresource );
		flatten_tree( &flat, curtree, "  ", 0 );
	}

	/* Output 9 of the 7-char chunks per region "card" */
	cp = rt_vls_addr( &flat );
	left = rt_vls_strlen( &flat );
	first = 1;

	do  {
		register char	*op;

		op = obuf;
		if( first )  {
			(void) sprintf( op, "%5d ", nnr+delreg );
			first = 0;
		} else {
			strncpy( op, "      ", 6 );
		}
		op += 6;

		if( left > 9*7 )  {
			strncpy( op, cp, 9*7 );
			cp += 9*7;
			op += 9*7;
			left -= 9*7;
		} else {
			strncpy( op, cp, left );
			op += left;
			while( left < 9*7 )  {
				*op++ = ' ';
				left++;
			}
			left = 0;
		}
		strcpy( op, regdp->d_namep );
		op += strlen(op);
		*op++ = '\n';
		*op = '\0';
		ewrite( regfp, obuf, strlen(obuf) );
	} while( left > 0 );

	/*
	 *  Write a record into the region ident table.
	 */
	vls_itoa( &ident, nnr+delreg, 5 );
	vls_itoa( &ident, tsp->ts_regionid, 5 );
	vls_itoa( &ident, tsp->ts_aircode, 5 );
	vls_itoa( &ident, tsp->ts_gmater, 5 );
	vls_itoa( &ident, tsp->ts_los, 5 );
	rt_vls_strcat( &ident, "     " );		/* 5 spaces */

	length = strlen( fullname );
	if( length > 50 )  {
		register char	*bp;

		bp = fullname + (length - 50);
		*bp = '*';
		rt_vls_strcat( &ident, bp );
	} else {
		/* Omit leading slash, for compat with old version */
		rt_vls_strcat( &ident, fullname+1 );
	}
	rt_vls_strcat( &ident, "\n" );
	rt_vls_fwrite( ridfp, &ident );

	rt_vls_free( &ident );
	rt_vls_free( &reg );
	rt_vls_free( &flat );
	bu_free( fullname, "fullname" );

	/*
	 *  Returned tree will be freed by caller.
	 *  To keep solid table available for seraching,
	 *  add this tree to a list of trees to be released once
	 *  everything is finished.
	 */
	/* XXX Should make list of "regions" (trees) here */
	return  (union tree *)0;
}

/*
 *			G E T T R E E _ L E A F
 *
 *  Re-use the librt "soltab" structures here, for our own purposes.
 */
union tree *gettree_leaf( tsp, pathp, ip, client_data )
struct db_tree_state	*tsp;
struct db_full_path	*pathp;
struct rt_db_internal	*ip;
genptr_t		client_data;
{
	register fastf_t	f;
	register struct soltab	*stp;
	union tree		*curtree;
	struct directory	*dp;
	struct rt_vls		sol;
	register int		i;
	register matp_t		mat;

	RT_VLS_INIT( &sol );

	RT_CK_DB_INTERNAL(ip);
	dp = DB_FULL_PATH_CUR_DIR(pathp);

	/* Determine if this matrix is an identity matrix */
	for( i=0; i<16; i++ )  {
		f = tsp->ts_mat[i] - rt_identity[i];
		if( !NEAR_ZERO(f, 0.0001) )
			break;
	}
	if( i < 16 )  {
		/* Not identity matrix */
		mat = tsp->ts_mat;
	} else {
		/* Identity matrix */
		mat = (matp_t)0;
	}

	/*
	 *  Check to see if this exact solid has already been processed.
	 *  Match on leaf name and matrix.
	 */
	for( RT_LIST_FOR( stp, soltab, &(sol_hd.l) ) )  {
		RT_CHECK_SOLTAB(stp);				/* debug */

		/* Leaf solids must be the same */
		if( dp != stp->st_dp )  continue;

		if( mat == (matp_t)0 )  {
			if( stp->st_matp == (matp_t)0 )  {
				if( debug )
					rt_log("rt_gettree_leaf:  %s re-referenced (ident)\n",
						dp->d_namep );
				goto found_it;
			}
			goto next_one;
		}
		if( stp->st_matp == (matp_t)0 )  goto next_one;

		for( i=0; i<16; i++ )  {
			f = mat[i] - stp->st_matp[i];
			if( !NEAR_ZERO(f, 0.0001) )
				goto next_one;
		}
		/* Success, we have a match! */
		if( debug )  {
			rt_log("rt_gettree_leaf:  %s re-referenced\n",
			    dp->d_namep );
		}
		goto found_it;
next_one:
		;
	}

	GETSTRUCT(stp, soltab);
	stp->l.magic = RT_SOLTAB_MAGIC;
	stp->st_id = ip->idb_type;
	stp->st_dp = dp;
	if( mat )  {
		stp->st_matp = (matp_t)bu_malloc( sizeof(mat_t), "st_matp" );
		MAT_COPY( stp->st_matp, mat );
	} else {
		stp->st_matp = mat;
	}
	stp->st_specific = (genptr_t)0;

	/* init solid's maxima and minima */
	VSETALL( stp->st_max, -INFINITY );
	VSETALL( stp->st_min,  INFINITY );

	RT_CK_DB_INTERNAL( ip );

	if(debug)  {
		struct rt_vls	str;
		rt_vls_init( &str );
		/* verbose=1, mm2local=1.0 */
		if( ip->idb_meth->ft_describe( &str, ip, 1, 1.0, &rt_uniresource, dbip ) < 0 )  {
			rt_log("rt_gettree_leaf(%s):  solid describe failure\n",
			    dp->d_namep );
		}
		bu_log( "%s:  %s", dp->d_namep, rt_vls_addr( &str ) );
		bu_vls_free( &str );
	}

	/* For now, just link them all onto the same list */
	RT_LIST_INSERT( &(sol_hd.l), &(stp->l) );

	stp->st_bit = ++nns;

	/* Solid number is stp->st_bit + delsol */

	/* Process appropriate solid type.				*/
	switch( ip->idb_type )  {
	case ID_TOR:
		addtor( &sol, (struct rt_tor_internal *)ip->idb_ptr,
		    dp->d_namep, stp->st_bit+delsol );
		break;
	case ID_ARB8:
		addarb( &sol, (struct rt_arb_internal *)ip->idb_ptr,
		    dp->d_namep, stp->st_bit+delsol );
		break;
	case ID_ELL:
		addell( &sol, (struct rt_ell_internal *)ip->idb_ptr,
		    dp->d_namep, stp->st_bit+delsol );
		break;
	case ID_TGC:
		addtgc( &sol, (struct rt_tgc_internal *)ip->idb_ptr,
		    dp->d_namep, stp->st_bit+delsol );
		break;
	case ID_ARS:
		addars( &sol, (struct rt_ars_internal *)ip->idb_ptr,
		    dp->d_namep, stp->st_bit+delsol );
		break;
	case ID_HALF:
		addhalf( &sol, (struct rt_half_internal *)ip->idb_ptr,
		    dp->d_namep, stp->st_bit+delsol );
		break;
	case ID_ARBN:
		addarbn( &sol, (struct rt_arbn_internal *)ip->idb_ptr,
		    dp->d_namep, stp->st_bit+delsol );
		break;
	case ID_PIPE:
		/* XXX */
	default:
		(void) fprintf( stderr,
		    "vdeck: '%s' Primitive type %s has no corresponding COMGEOM primitive, skipping\n",
		    dp->d_namep, ip->idb_meth->ft_name );
		vls_itoa( &sol, stp->st_bit+delsol, 5 );
		rt_vls_strcat( &sol, ip->idb_meth->ft_name );
		vls_blanks( &sol, 5*10 );
		rt_vls_strcat( &sol, dp->d_namep );
		rt_vls_strcat( &sol, "\n");
		break;
	}

	rt_vls_fwrite( solfp, &sol );
	rt_vls_free( &sol );

found_it:
	GETUNION( curtree, tree );
	curtree->magic = RT_TREE_MAGIC;
	curtree->tr_op = OP_SOLID;
	curtree->tr_a.tu_stp = stp;
	curtree->tr_a.tu_regionp = (struct region *)0;

	return(curtree);
}

void
swap_vec( v1, v2 )
vect_t	v1, v2;
{
	vect_t	work;

	VMOVE( work, v1 );
	VMOVE( v1, v2 );
	VMOVE( v2, work );
}

void
swap_dbl( d1, d2 )
register double	*d1, *d2;
{
	double	t;
	t = *d1;
	*d1 = *d2;
	*d2 = t;
	return;
}

/*
 *			A D D T O R
 *
 *  Process torus.
 */
void
addtor( v, gp, name, num )
struct rt_vls		*v;
struct rt_tor_internal	*gp;
char			*name;
int			num;
{
	RT_VLS_CHECK(v);
	RT_TOR_CK_MAGIC(gp);

	/* V, N, r1, r2 */
	vls_itoa( v, num, 5 );
	rt_vls_strcat( v, "tor  " );		/* 5 */
	vls_ftoa_vec_cvt( v, gp->v, 10, 4 );
	vls_ftoa_vec( v, gp->h, 10, 4 );
	rt_vls_strcat( v, name );
	rt_vls_strcat( v, "\n" );

	vls_itoa( v, num, 5 );
	vls_blanks( v, 5 );
	vls_ftoa_cvt( v, gp->r_a, 10, 4 );
	vls_ftoa_cvt( v, gp->r_h, 10, 4 );
	vls_blanks( v, 4*10 );
	rt_vls_strcat( v, name );
	rt_vls_strcat( v, "\n");
}

/*
 *			A D D H A L F
 */
void
addhalf( v, gp, name, num )
struct rt_vls		*v;
struct rt_half_internal	*gp;
char			*name;
int			num;
{
	RT_VLS_CHECK(v);
	RT_HALF_CK_MAGIC(gp);

	/* N, d */
	vls_itoa( v, num, 5 );
	rt_vls_strcat( v, "haf  " );		/* 5 */
	vls_ftoa_vec( v, gp->eqn, 10, 4 );
	vls_ftoa_cvt( v, -(gp->eqn[3]), 10, 4 );
	vls_blanks( v, 2*10 );
	rt_vls_strcat( v, name );
	rt_vls_strcat( v, "\n" );
}

/*
 *			A D D A R B N
 */
void
addarbn( v, gp, name, num )
struct rt_vls		*v;
struct rt_arbn_internal	*gp;
char			*name;
int			num;
{
	register int	i;

	RT_VLS_CHECK(v);
	RT_ARBN_CK_MAGIC(gp);

	/* nverts, nverts_index_nums, nplane_eqns, naz_el */
	vls_itoa( v, num, 5 );
	rt_vls_strcat( v, "arbn " );		/* 5 */
	vls_itoa( v, 0, 10 );			/* vertex points, 2/card */
	vls_itoa( v, 0, 10 );			/* vertex index #, 6/card */
	vls_itoa( v, gp->neqn, 10 );		/* plane eqn, 1/card */
	vls_itoa( v, 0, 10 );			/* az/el & index #, 2/card */
	vls_blanks( v, 20 );
	rt_vls_strcat( v, name );
	rt_vls_strcat( v, "\n" );

	for( i=0; i < gp->neqn; i++ )  {
		vls_itoa( v, num, 5 );
		vls_blanks( v, 5 );
		vls_ftoa_vec( v, gp->eqn[i], 10, 4 );
		vls_ftoa_cvt( v, gp->eqn[i][3], 10, 4 );
		vls_blanks( v, 2*10 );
		rt_vls_strcat( v, name );
		rt_vls_strcat( v, "\n" );
	}
}

static void
vls_solid_pts( v, pts, npts, name, num, kind )
struct rt_vls	*v;
const point_t	pts[];
const int	npts;
const char	*name;
const int	num;
const char	*kind;
{
	register int	i;

	for( i = 0; i < npts; )  {
		vls_itoa( v, num, 5 );
		if( i == 0 )
			rt_vls_strncat( v, kind, 5 );
		else
			rt_vls_strcat( v, "     " );
		vls_ftoa_vec_cvt( v, pts[i], 10, 4 );
		if( ++i < npts )  {
			vls_ftoa_vec_cvt( v, pts[i], 10, 4 );
		} else {
			vls_blanks( v, 3*10 );
		}
		i++;
		rt_vls_strcat( v, name );
		rt_vls_strcat( v, "\n");
	}
}

/*
 *			A D D A R B
 *
 *  Process generalized arb.
 */
void
addarb( v, gp, name, num )
struct rt_vls		*v;
struct rt_arb_internal	*gp;
char			*name;
int			num;
{
	register int	i;
	int	uniq_pts[8];
	int	samevecs[11];
	int	cgtype;
	point_t	pts[8];		/* GIFT-order points */

	/* Enter new arb code.						*/
	if( (i = cgarbs( &cgtype, gp, uniq_pts, samevecs, CONV_EPSILON )) == 0 ||
	    redoarb( pts, gp, uniq_pts, samevecs, i, cgtype ) == 0 )  {
		fprintf(stderr,"vdeck: addarb(%s): failure\n", name);
		vls_itoa( v, num, 5 );
		rt_vls_strncat( v, "arb??", 5 );
		vls_blanks( v, 6*10 );
		rt_vls_strcat( v, name );
		rt_vls_strcat( v, "\n");
		return;
	}

	/* Print the solid parameters.					*/
	switch( cgtype )  {
	case 8:
		vls_solid_pts( v, (const point_t *)pts, 8, name, num, "arb8 " );
		break;
	case 7:
		vls_solid_pts( v, (const point_t *)pts, 7, name, num, "arb7 " );
		break;
	case 6:
		VMOVE( pts[5], pts[6] );
		vls_solid_pts( v, (const point_t *)pts, 6, name, num, "arb6 " );
		break;
	case 5:
		vls_solid_pts( v, (const point_t *)pts, 5, name, num, "arb5 " );
		break;
	case 4:
		VMOVE( pts[3], pts[4] );
		vls_solid_pts( v, (const point_t *)pts, 4, name, num, "arb4 " );
		break;

		/* Currently, cgarbs() will not return RAW, BOX, or RPP */
	default:
		(void) fprintf( stderr, "addarb: Unknown arb cgtype=%d.\n",
		    cgtype );
		exit( 10 );
	}
}

#define GENELL	1
#define	ELL1	2
#define SPH	3

/*
 *			A D D E L L
 *
 *	Process the general ellipsoid.
 */
void
addell( v, gp, name, num )
struct rt_vls		*v;
struct rt_ell_internal	*gp;
char			*name;
int			num;
{
	double	ma, mb, mc;
	int	cgtype;

	/* Check for ell1 or sph.					*/
	ma = MAGNITUDE( gp->a );
	mb = MAGNITUDE( gp->b );
	mc = MAGNITUDE( gp->c );
	if( fabs( ma-mb ) < CONV_EPSILON )  {
		/* vector A == vector B */
		cgtype = ELL1;
		/* SPH if vector B == vector C also */
		if( fabs( mb-mc ) < CONV_EPSILON )
			cgtype = SPH;
		else	/* switch A and C */
		{
			swap_vec( gp->a, gp->c );
			swap_dbl( &ma, &mc );
		}
	} else if( fabs( ma-mc ) < CONV_EPSILON ) {
		/* vector A == vector C */
		cgtype = ELL1;
		/* switch vector A and vector B */
		swap_vec( gp->a, gp->b );
		swap_dbl( &ma, &mb );
	} else if( fabs( mb-mc ) < CONV_EPSILON )
		cgtype = ELL1;
	else
		cgtype = GENELL;

	/* Print the solid parameters.					*/
	vls_itoa( v, num, 5 );
	switch( cgtype )  {
	case GENELL:
		rt_vls_strcat( v, "ellg " );		/* 5 */
		vls_ftoa_vec_cvt( v, gp->v, 10, 4 );
		vls_ftoa_vec_cvt( v, gp->a, 10, 4 );
		rt_vls_strcat( v, name );
		rt_vls_strcat( v, "\n" );

		vls_itoa( v, num, 5 );
		vls_blanks( v, 5 );
		vls_ftoa_vec_cvt( v, gp->b, 10, 4 );
		vls_ftoa_vec_cvt( v, gp->c, 10, 4 );
		rt_vls_strcat( v, name );
		rt_vls_strcat( v, "\n");
		break;
	case ELL1:
		rt_vls_strcat( v, "ell1 " );		/* 5 */
		vls_ftoa_vec_cvt( v, gp->v, 10, 4 );
		vls_ftoa_vec_cvt( v, gp->a, 10, 4 );
		rt_vls_strcat( v, name );
		rt_vls_strcat( v, "\n" );

		vls_itoa( v, num, 5 );
		vls_blanks( v, 5 );
		vls_ftoa_cvt( v, mb, 10, 4 );
		vls_blanks( v, 5*10 );
		rt_vls_strcat( v, name );
		rt_vls_strcat( v, "\n");
		break;
	case SPH:
		rt_vls_strcat( v, "sph  " );		/* 5 */
		vls_ftoa_vec_cvt( v, gp->v, 10, 4 );
		vls_ftoa_cvt( v, ma, 10, 4 );
		vls_blanks( v, 2*10 );
		rt_vls_strcat( v, name );
		rt_vls_strcat( v, "\n" );
		break;
	default:
		(void) fprintf( stderr,
		    "Error in type of ellipse (%d).\n",
		    cgtype
		    );
		exit( 10 );
	}
}

#define TGC	1
#define TEC	2
#define TRC	3
#define REC	4
#define RCC	5

/*
 *			A D D T G C
 *
 *	Process generalized truncated cone.
 */
void
addtgc( v, gp, name, num )
struct rt_vls		*v;
struct rt_tgc_internal	*gp;
char			*name;
int			num;
{
	vect_t	axb, cxd;
	double	ma, mb, mc, md, maxb, mcxd, mh;
	int	cgtype;

	/* Check for tec rec trc rcc.					*/
	cgtype = TGC;
	VCROSS( axb, gp->a, gp->b );
	VCROSS( cxd, gp->c, gp->d );

	ma = MAGNITUDE( gp->a );
	mb = MAGNITUDE( gp->b );
	mc = MAGNITUDE( gp->c );
	md = MAGNITUDE( gp->d );
	maxb = MAGNITUDE( axb );
	mcxd = MAGNITUDE( cxd );
	mh = MAGNITUDE( gp->h );

	if( ma <= 0.0 || mb <= 0.0 )  {
		fprintf(stderr, "addtgc(%s): ma=%e, mb=%e, skipping\n", name, ma, mb );
		return;
	}

	/* TEC if ratio top and bot vectors equal and base parallel to top.
	 */
	if( mc != 0.0 && md != 0.0 &&
	    fabs( (mb/md)-(ma/mc) ) < CONV_EPSILON &&
	    fabs( fabs(VDOT(axb,cxd)) - (maxb*mcxd) ) < CONV_EPSILON )  {
		cgtype = TEC;
	}

	/* Check for right cylinder.					*/
	if( fabs( fabs(VDOT(gp->h,axb)) - (mh*maxb) ) < CONV_EPSILON )  {
		if( fabs( ma-mb ) < CONV_EPSILON )  {
			if( fabs( ma-mc ) < CONV_EPSILON )
				cgtype = RCC;
			else
				cgtype = TRC;
		} else {
			/* elliptical */
			if( fabs( ma-mc ) < CONV_EPSILON )
				cgtype = REC;
		}
	}

	/* Insure that magnitude of A is greater than B, and magnitude of
		C is greater than D for the GIFT code (boy, is THIS a shame).
		This need only be done for the elliptical REC and TEC types.
	 */
	if( (cgtype == REC || cgtype == TEC) && ma < mb )  {
		swap_vec( gp->a, gp->b );
		swap_dbl( &ma, &mb );
		swap_vec( gp->c, gp->d );
		swap_dbl( &mc, &md );
	}

	/* Print the solid parameters.					*/
	vls_itoa( v, num, 5 );
	switch( cgtype )  {
	case TGC:
		rt_vls_strcat( v, "tgc  " );		/* 5 */
		vls_ftoa_vec_cvt( v, gp->v, 10, 4 );
		vls_ftoa_vec_cvt( v, gp->h, 10, 4 );
		rt_vls_strcat( v, name );
		rt_vls_strcat( v, "\n" );

		vls_itoa( v, num, 5 );
		vls_blanks( v, 5 );
		vls_ftoa_vec_cvt( v, gp->a, 10, 4 );
		vls_ftoa_vec_cvt( v, gp->b, 10, 4 );
		rt_vls_strcat( v, name );
		rt_vls_strcat( v, "\n");

		vls_itoa( v, num, 5 );
		vls_blanks( v, 5 );
		vls_ftoa_cvt( v, mc, 10, 4 );
		vls_ftoa_cvt( v, md, 10, 4 );
		vls_blanks( v, 4*10 );
		rt_vls_strcat( v, name );
		rt_vls_strcat( v, "\n");
		break;
	case RCC:
		rt_vls_strcat( v, "rcc  " );		/* 5 */
		vls_ftoa_vec_cvt( v, gp->v, 10, 4 );
		vls_ftoa_vec_cvt( v, gp->h, 10, 4 );
		rt_vls_strcat( v, name );
		rt_vls_strcat( v, "\n" );

		vls_itoa( v, num, 5 );
		vls_blanks( v, 5 );
		vls_ftoa_cvt( v, ma, 10, 4 );
		vls_blanks( v, 5*10 );
		rt_vls_strcat( v, name );
		rt_vls_strcat( v, "\n");
		break;
	case TRC:
		rt_vls_strcat( v, "trc  " );		/* 5 */
		vls_ftoa_vec_cvt( v, gp->v, 10, 4 );
		vls_ftoa_vec_cvt( v, gp->h, 10, 4 );
		rt_vls_strcat( v, name );
		rt_vls_strcat( v, "\n" );

		vls_itoa( v, num, 5 );
		vls_blanks( v, 5 );
		vls_ftoa_cvt( v, ma, 10, 4 );
		vls_ftoa_cvt( v, mc, 10, 4 );
		vls_blanks( v, 4*10 );
		rt_vls_strcat( v, name );
		rt_vls_strcat( v, "\n");
		break;
	case TEC:
		rt_vls_strcat( v, "tec  " );		/* 5 */
		vls_ftoa_vec_cvt( v, gp->v, 10, 4 );
		vls_ftoa_vec_cvt( v, gp->h, 10, 4 );
		rt_vls_strcat( v, name );
		rt_vls_strcat( v, "\n" );

		vls_itoa( v, num, 5 );
		vls_blanks( v, 5 );
		vls_ftoa_vec_cvt( v, gp->a, 10, 4 );
		vls_ftoa_vec_cvt( v, gp->b, 10, 4 );
		rt_vls_strcat( v, name );
		rt_vls_strcat( v, "\n");

		vls_itoa( v, num, 5 );
		vls_blanks( v, 5 );
		vls_ftoa( v, ma/mc, 10, 4 );	/* dimensionless ratio */
		vls_blanks( v, 5*10 );
		rt_vls_strcat( v, name );
		rt_vls_strcat( v, "\n");
		break;
	case REC:
		rt_vls_strcat( v, "rec  " );		/* 5 */
		vls_ftoa_vec_cvt( v, gp->v, 10, 4 );
		vls_ftoa_vec_cvt( v, gp->h, 10, 4 );
		rt_vls_strcat( v, name );
		rt_vls_strcat( v, "\n" );

		vls_itoa( v, num, 5 );
		vls_blanks( v, 5 );
		vls_ftoa_vec_cvt( v, gp->a, 10, 4 );
		vls_ftoa_vec_cvt( v, gp->b, 10, 4 );
		rt_vls_strcat( v, name );
		rt_vls_strcat( v, "\n");
		break;
	default:
		(void) fprintf( stderr,
		    "Error in tgc type (%d).\n",
		    cgtype
		    );
		exit( 10 );
	}
}

/*
 *			A R S _ C U R V E _ O U T
 */
void
ars_curve_out( v, fp, todo, curveno, num )
struct rt_vls		*v;
fastf_t			*fp;
int			todo;
int			curveno;
int			num;
{
	while( todo > 0 )  {
		vls_itoa( v, num, 5 );
		vls_blanks( v, 5 );

		/* First point */
		vls_ftoa_vec_cvt( v, fp, 10, 4 );
		fp += 3;
		todo--;

		/* Second point */
		if( todo >= 1 )  {
			vls_ftoa_vec_cvt( v, fp, 10, 4 );
			fp += 3;
			todo--;
		} else {
			vls_blanks( v, 3*10 );
		}

		rt_vls_strcat( v, "curve " );
		vls_itoa( v, curveno, 3 );
		rt_vls_strcat( v, "\n" );
	}
}

/*
 *			A D D A R S
 *
 *	Process triangular surfaced polyhedron - ars.
 */
void
addars( v, gp, name, num )
struct rt_vls		*v;
struct rt_ars_internal	*gp;
char			*name;
int			num;
{
	register int	i;

	RT_ARS_CK_MAGIC(gp);

	vls_itoa( v, num, 5 );
	rt_vls_strcat( v, "ars  " );		/* 5 */
	vls_itoa( v, gp->ncurves, 10 );
	vls_itoa( v, gp->pts_per_curve, 10 );
	vls_blanks( v, 4*10 );
	rt_vls_strcat( v, name );
	rt_vls_strcat( v, "\n" );

	for( i=0; i < gp->ncurves; i++ )  {
		/* Output the points on this curve */
		ars_curve_out( v, gp->curves[i], gp->pts_per_curve, i, num );
	}
}

/*	e w r i t e
	Write with error checking.
 */
void
ewrite( fp, buf, bytes )
FILE		*fp;
const char	*buf;
unsigned	bytes;
{
	if( bytes == 0 )  return;

	if( fwrite( buf, bytes, 1, fp ) != 1 )  {
		perror("write");
		(void)fprintf(stderr, "vdeck: write error\n");
		exit(2);
	}
}

/*
	Section 1:	C O M M A N D S

			deck()
			shell()
 */

/*	d e c k ( )
	make a COMGEOM deck for current list of objects

 */
void
deck( prefix )
register char *prefix;
{
	nns = nnr = 0;

	/* Create file for solid table.					*/
	if( prefix != 0 )
	{
		(void) bu_vls_strcpy( &st_vls, prefix );
		(void) bu_vls_strcat( &st_vls, ".st" );
	}
	else
		(void) bu_vls_strcpy( &st_vls, "solids" );
	st_file = bu_vls_addr( &st_vls );
	if( (solfp = fopen( st_file, "w")) == NULL )  {
		perror( st_file );
		exit( 10 );
	}


	/* Target units (a2,3x)						*/
	ewrite( solfp, rt_units_string(dbip->dbi_local2base), 2 );
	blank_fill( solfp, 3 );

	/* Title							*/
	if( dbip->dbi_title == NULL )
		ewrite( solfp, objfile, (unsigned) strlen( objfile ) );
	else
		ewrite( solfp, dbip->dbi_title, (unsigned) strlen( dbip->dbi_title ) );
	ewrite( solfp, LF, 1 );

	/* Save space for number of solids and regions.			*/
	savsol = ftell( solfp );
	blank_fill( solfp, 10 );
	ewrite( solfp, LF, 1 );

	/* Create file for region table.				*/
	if( prefix != 0 )
	{
		(void) bu_vls_strcpy( &rt_vls, prefix );
		(void) bu_vls_strcat( &rt_vls, ".rt" );
	}
	else
		(void) bu_vls_strcpy( &rt_vls, "regions" );
	rt_file = bu_vls_addr( &rt_vls );
	if( (regfp = fopen( rt_file, "w" )) == NULL )  {
		perror( rt_file );
		exit( 10 );
	}

	/* create file for region ident table
	 */
	if( prefix != 0 )
	{
		(void) bu_vls_strcpy( &id_vls, prefix );
		(void) bu_vls_strcat( &id_vls, ".id" );
	}
	else
		(void) bu_vls_strcpy( &id_vls, "region_ids" );
	id_file = bu_vls_addr( &id_vls );
	if( (ridfp = fopen( id_file, "w" )) == NULL )  {
		perror( id_file );
		exit( 10 );
	}
	itoa( -1, buff, 5 );
	ewrite( ridfp, buff, 5 );
	ewrite( ridfp, LF, 1 );

	/* Initialize matrices.						*/
	MAT_IDN( identity );

	if( !sol_hd.l.magic )  RT_LIST_INIT( &sol_hd.l );

	/*  Build the whole card deck.	*/
	/*  '1' indicates one CPU.  This code isn't ready for parallelism */
	if( db_walk_tree( dbip, curr_ct, (const char **)curr_list,
	    1, &rt_initial_tree_state,
	    0, region_end, gettree_leaf, (genptr_t)NULL ) < 0 )  {
		fprintf(stderr,"Unable to treewalk any trees!\n");
	    	exit(11);
	}

	/* Go back, and add number of solids and regions on second card. */
	fseek( solfp, savsol, 0 );
	itoa( nns, buff, 5 );
	ewrite( solfp, buff, 5 );
	itoa( nnr, buff, 5 );
	ewrite( solfp, buff, 5 );

	/* Finish region id table.					*/
	ewrite( ridfp, LF, 1 );

	(void) printf( "====================================================\n" );
	(void) printf( "O U T P U T    F I L E S :\n\n" );
	(void) printf( "solid table = \"%s\"\n", st_file );
	(void) printf( "region table = \"%s\"\n", rt_file );
	(void) printf( "region identification table = \"%s\"\n", id_file );
	(void) fclose( solfp );
	(void) fclose( regfp );
	(void) fclose( ridfp );

	/* reset starting numbers for solids and regions
	 */
	delsol = delreg = 0;
	/* XXX should free soltab list */
}

/*	s h e l l ( )
	Execute shell command.
 */
int
shell( args )
char  *args[];
{
	register char	*from, *to;
	char		*argv[4], cmdbuf[MAXLN];
	int		pid, ret, status;
	register int	i;

	(void) signal( SIGINT, SIG_IGN );

	/* Build arg vector.						*/
	argv[0] = "Shell( deck )";
	argv[1] = "-c";
	to = argv[2] = cmdbuf;
	for( i = 1; i < arg_ct; i++ ) {
		from = args[i];
		if( (to + strlen( args[i] )) - argv[2] > MAXLN - 1 ) {
			(void) fprintf( stderr, "\ncommand line too long\n" );
			exit( 10 );
		}
		(void) printf( "%s ", args[i] );
		while( *from )
			*to++ = *from++;
		*to++ = ' ';
	}
	to[-1] = '\0';
	(void) printf( "\n" );
	argv[3] = 0;
	if( (pid = fork()) == -1 ) {
		perror( "shell()" );
		return( -1 );
	} else	if( pid == 0 ) { /*
				  * CHILD process - execs a shell command
					  */
		(void) signal( SIGINT, SIG_DFL );
		(void) execv( "/bin/sh", argv );
		perror( "/bin/sh -c" );
		exit( 99 );
	} else	/*
		 * PARENT process - waits for shell command
		 * to finish.
			 */
		do {
			if( (ret = wait( &status )) == -1 ) {
				perror( "wait( /bin/sh -c )" );
				break;
			}
		} while( ret != pid );
	return( 0 );
}

/*	t o c ( )
	Build a sorted list of names of all the objects accessable
	in the object file.
 */
void
toc()
{
	register struct directory *dp;
	register int		i;
	register int		count;

	/* Determine necessary table size */
	count = 0;
	FOR_ALL_DIRECTORY_START(dp, dbip) {
	    count++;
	} FOR_ALL_DIRECTORY_END;
	count += 1;		/* Safety */

	/* Allocate tables */
	toc_list = (char **)bu_malloc( count * sizeof(char *), "toc_list[]" );
	tmp_list = (char **)bu_malloc( count * sizeof(char *), "tmp_list[]" );
	curr_list = (char **)bu_malloc( count * sizeof(char *), "curr_list[]" );
	ndir = 0;

	/* Fill in toc_list[] */
	FOR_ALL_DIRECTORY_START(dp, dbip) {
	    toc_list[ndir++] = dp->d_namep;
	} FOR_ALL_DIRECTORY_END;
}

/*
	Section 3:	L I S T   P R O C E S S I N G   R O U T I N E S

			list_toc()
			col_prt()
			insert()
			delete()
*/

/*	l i s t _ t o c ( )
	List the table of contents.
 */
void
list_toc( args )
char	 *args[];
{
	register int	i, j;
	(void) fflush( stdout );
	for( tmp_ct = 0, i = 1; args[i] != NULL; i++ )
	{
		for( j = 0; j < ndir; j++ )
		{
			if( match( args[i], toc_list[j] ) )
				tmp_list[tmp_ct++] = toc_list[j];
		}
	}
	if( i > 1 )
		(void) col_prt( tmp_list, tmp_ct );
	else
		(void) col_prt( toc_list, ndir );
	return;
}

#define NAMESIZE	16
#define MAX_COL	(NAMESIZE*5)
#define SEND_LN()	{\
			buf[column++] = '\n';\
			ewrite( stdout, buf, (unsigned) column );\
			column = 0;\
			}

/*	c o l _ p r t ( )
	Print list of names in tabular columns.
 */
int
col_prt( list, ct )
register char	*list[];
register int	ct;
{
	char		buf[MAX_COL+2];
	register int	i, column, spaces;

	for( i = 0, column = 0; i < ct; i++ )
	{
		if( column + (int)strlen( list[i] ) > MAX_COL )
		{
			SEND_LN();
			i--;
		}
		else
		{
			(void) strcpy( &buf[column], list[i] );
			column += strlen( list[i] );
			spaces = NAMESIZE - (column % NAMESIZE );
			if( column + spaces < MAX_COL )
				for( ; spaces > 0; spaces-- )
					buf[column++] = ' ';
			else
				SEND_LN();
		}
	}
	SEND_LN();
	return	ct;
}

/*	i n s e r t ( )
	Insert each member of the table of contents 'toc_list' which
	matches one of the arguments into the current list 'curr_list'.
 */
int
insert(  args,	ct )
char		*args[];
register int	ct;
{
	register int	i, j, nomatch;

	/* For each argument (does not include args[0]).			*/
	for( i = 1; i < ct; i++ )
	{ /* If object is in table of contents, insert in current list.	*/
		nomatch = YES;
		for( j = 0; j < ndir; j++ )
		{
			if( match( args[i], toc_list[j] ) )
			{
				nomatch = NO;
				/* Allocate storage for string.			*/
				curr_list[curr_ct++] = bu_strdup(toc_list[j]);
			}
		}
		if( nomatch )
			(void) fprintf( stderr,
			    "Object \"%s\" not found.\n", args[i] );
	}
	return	curr_ct;
}

/*	d e l e t e ( )
	delete all members of current list 'curr_list' which match
	one of the arguments
 */
int
delete(  args )
char	*args[];
{
	register int	i;
	register int	nomatch;

	/* for each object in arg list
	 */
	for( i = 1; i < arg_ct; i++ ) {
		register int	j;
		nomatch = YES;

		/* traverse list to find string
		 */
		for( j = 0; j < curr_ct; )
			if( match( args[i], curr_list[j] ) )
			{
				register int	k;

				nomatch = NO;
				bu_free( curr_list[j], "curr_list" );
				--curr_ct;
				/* starting from bottom of list,
				 * pull all entries up to fill up space
				 made by deletion
				 */
				for( k = j; k < curr_ct; k++ )
					curr_list[k] = curr_list[k+1];
			}
			else	++j;
		if( nomatch )
			(void) fprintf( stderr,
			    "Object \"%s\" not found.\n",
			    args[i]
			    );
	}
	return( curr_ct );
}

/*
	Section 4:	S T R I N G   P R O C E S S I N G   R O U T I N E S

			itoa()
			check()
 */

/*	i t o a ( )
	Convert integer to ascii  wd format.
 */
void
itoa( n, s, w )
register
char	*s;
register
int   n,    w;
{
	int	 c, i, j, sign;

	if( (sign = n) < 0 )	n = -n;
	i = 0;
	do
		s[i++] = n % 10 + '0';
	while( (n /= 10) > 0 );
	if( sign < 0 )	s[i++] = '-';

	/* Blank fill array.					*/
	for( j = i; j < w; j++ )	s[j] = ' ';
	if( i > w ) {
		s[w-1] = (s[w]-1-'0')*10 + (s[w-1]-'0')  + 'A';
	}
	s[w] = '\0';

	/* Reverse the array.					*/
	for( i = 0, j = w - 1; i < j; i++, j-- ) {
		c    = s[i];
		s[i] = s[j];
		s[j] =    c;
	}
}

void
vls_blanks( v, n )
struct rt_vls	*v;
int		n;
{
	RT_VLS_CHECK(v);
	rt_vls_strncat( v, "                                                                                                                                ",
	    n);
}

/*
 *			V L S _ I T O A
 *
 *	Convert integer to ascii  wd format.
 */
void
vls_itoa( v, n, w )
struct rt_vls	*v;
register int	n;
register int	w;
{
	int	 c, i, j, sign;
	register char	*s;

	RT_VLS_CHECK(v);
	rt_vls_strncat( v, "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx", w);
	s = rt_vls_addr(v)+rt_vls_strlen(v)-w;

	if( (sign = n) < 0 )	n = -n;
	i = 0;
	do
		s[i++] = n % 10 + '0';
	while( (n /= 10) > 0 );
	if( sign < 0 )	s[i++] = '-';

	/* Blank fill array.					*/
	for( j = i; j < w; j++ )	s[j] = ' ';
	if( i > w ) {
		s[w-1] = (s[w]-1-'0')*10 + (s[w-1]-'0')  + 'A';
	}
	s[w] = '\0';

	/* Reverse the array.					*/
	for( i = 0, j = w - 1; i < j; i++, j-- ) {
		c    = s[i];
		s[i] = s[j];
		s[j] =    c;
	}
}

void
vls_ftoa_vec_cvt( v, vec, w, d )
struct rt_vls	*v;
vect_t		vec;
int		w;
int		d;
{
	vls_ftoa( v, vec[X]*dbip->dbi_base2local, w, d );
	vls_ftoa( v, vec[Y]*dbip->dbi_base2local, w, d );
	vls_ftoa( v, vec[Z]*dbip->dbi_base2local, w, d );
}

void
vls_ftoa_vec( v, vec, w, d )
struct rt_vls	*v;
vect_t		vec;
int		w;
int		d;
{
	vls_ftoa( v, vec[X], w, d );
	vls_ftoa( v, vec[Y], w, d );
	vls_ftoa( v, vec[Z], w, d );
}

void
vls_ftoa_cvt( v, f, w, d )
struct rt_vls	*v;
register double	f;
register int	w, d;
{
	vls_ftoa( v, f*dbip->dbi_base2local, w, d );
}

/*
 *			V L S _ F T O A
 *
 *	Convert float to ascii  w.df format.
 */
void
vls_ftoa( v, f, w, d )
struct rt_vls	*v;
register double	f;
register int	w, d;
{
	register char	*s;
	register int	c, i, j;
	long	n, sign;

	RT_VLS_CHECK(v);
	rt_vls_strncat( v, "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx", w);
	s = rt_vls_addr(v)+rt_vls_strlen(v)-w;

	if( w <= d + 2 )
	{
		(void) fprintf( stderr,
		    "ftoascii: incorrect format  need w.df  stop"
		    );
		exit( 10 );
	}
	for( i = 1; i <= d; i++ )
		f = f * 10.0;

	/* round up */
	if( f < 0.0 )
		f -= 0.5;
	else
		f += 0.5;
	n = f;
	if( (sign = n) < 0 )
		n = -n;
	i = 0;
	do	{
		s[i++] = n % 10 + '0';
		if( i == d )
			s[i++] = '.';
	}	while( (n /= 10) > 0 );

	/* Zero fill the d field if necessary.				*/
	if( i < d )
	{
		for( j = i; j < d; j++ )
			s[j] = '0';
		s[j++] = '.';
		i = j;
	}
	if( sign < 0 )
		s[i++] = '-';

	/* Blank fill rest of field.					*/
	for ( j = i; j < w; j++ )
		s[j] = ' ';
	if( i > w )
		(void) fprintf( stderr, "Ftoascii: field length too small\n" );
	s[w] = '\0';

	/* Reverse the array.						*/
	for( i = 0, j = w - 1; i < j; i++, j-- )
	{
		c    = s[i];
		s[i] = s[j];
		s[j] =    c;
	}
}

/*
	Section 5:	I / O   R O U T I N E S
 *
			getcmd()
			getarg()
			menu()
			blank_fill()
			bug()
			fbug()
 */

/*	g e t c m d ( )
	Return first character read from keyboard,
	copy command into args[0] and arguments into args[1]...args[n].

 */
char
getcmd( args, ct )
char		*args[];
register int	ct;
{
	/* Get arguments.						 */
	if( ct == 0 )
		while( --arg_ct >= 0 )
			bu_free( args[arg_ct], "args[arg_ct]" );
	for( arg_ct = ct; arg_ct < MAXARG - 1; ++arg_ct )
	{
		args[arg_ct] = bu_malloc( MAXLN, "getcmd buffer" );
		if( ! getarg( args[arg_ct] ) )
			break;
	}
	++arg_ct;
	args[arg_ct] = 0;

	/* Before returning to command interpreter,
	 * set up interrupt handler for commands...
	 * trap interrupts such that command is aborted cleanly and
	 * command line is restored rather than terminating program
	 */
	(void) signal( SIGINT, abort_sig );
	return	(args[0])[0];
}

/*	g e t a r g ( )
	Get a word of input into 'str',
	Return 0 if newline is encountered.
 	Return 1 otherwise.
 */
char
getarg( str )
register char	*str;
{
	do
	{
		*str = getchar();
		if( (int)(*str) == ' ' )
		{
			*str = '\0';
			return( 1 );
		}
		else
			++str;
	}	while( (int)(str[-1]) != EOF && (int)(str[-1]) != '\n' );
	if( (int)(str[-1]) == '\n' )
		--str;
	*str = '\0';
	return	0;
}

/*	m e n u ( )
	Display menu stored at address 'addr'.
 */
void
menu( addr )
char **addr;
{
	register char	**sbuf = addr;
	while( *sbuf )
		(void) printf( "%s\n", *sbuf++ );
	(void) fflush( stdout );
	return;
}

/*	b l a n k _ f i l l ( )
	Write count blanks to fildes.
 */
void
blank_fill( fp, count )
FILE		*fp;
register int	count;
{
	ewrite( fp, BLANKS, (unsigned) count );
}

/*
	Section 6:	I N T E R R U P T   H A N D L E R S
 *
			abort_sig()
			quit()
 */

/*	a b o r t ( )
	Abort command without terminating run (restore command prompt) and
	cleanup temporary files.
 */
/*ARGSUSED*/
void
abort_sig( sig )
{
	(void) signal( SIGINT, quit );	/* reset trap */

	/* goto command interpreter with environment restored.		*/
	longjmp( env, sig );
}

/*	q u i t ( )
	Terminate run.
 */
/*ARGSUSED*/
void
quit( sig )
{
	(void) fprintf( stdout, "quitting...\n" );
	exit( 0 );
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
