/*
	Author:		Gary S. Moss
			U. S. Army Ballistic Research Laboratory
			Aberdeen Proving Ground
			Maryland 21005-5066
			(301)278-6647 or AV-298-6647
*/
#ifndef lint
static char RCSid[] = "@(#)$Header$ (BRL)";
#endif

/*
	Derived from KARDS, written by Keith Applin.

	Generate a COM-GEOM card images suitable for input to gift5
	(also gift(1V)) from a vged(1V) target description.

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
#include <stdio.h>
#include <signal.h>
#ifdef BSD
#include <strings.h>
#else
#include <string.h>
#endif
#include <math.h>
#include <setjmp.h>

#include "machine.h"
#include "vmath.h"
#include "externs.h"
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
0 };

char	*usage[] = {
"",
"v d e c k ($Revision$)",
"Make COMGEOM decks of objects from a \"mged\" file suitable as",
"input to GIFT5 or gift(1V).",
"",
"Usage: vdeck file.g",
"",
0 };

/* Units conversion factor from milimeters to whatever is specified in
	the ident record.  If nothing is specified, unity scaling is used.
	See 'db.h' for defined units and ident record definition.  This
	capability dates back to GED database version (v4).
 */
double	unit_conversion = 1.0;

/* Sorted table of contents.						*/
char	*toc_list[NDIR];

/* List of regions and solids to be processed.				*/
char	*curr_list[NDIR];
int	curr_ct = 0;

/* List of arguments from command line parser.				*/
char	*arg_list[MAXARG];
int	arg_ct = 0;

/* Temporary list of names.						*/
char	*tmp_list[NDIR];
int	tmp_ct = 0;

/* Structure used by setjmp() and longjmp() to save environment.	*/
jmp_buf	env;

/* File names and descriptors.						*/
char	*objfile;
int	regfd;		char	rt_file[15];
int	solfd;		char	st_file[73];
int	ridfd;		char	id_file[73];

/* Counters.								*/
int	nns;		/* Solids.					*/
int	nnr;		/* Regions not members of other regions.	*/
int	ndir;		/* Entries in directory.			*/

/* Miscellaneous globals leftover from Keith's KARDS code.		*/
int		delsol = 0, delreg = 0;
char		buff[30];
long		savsol;		/* File postion of # of solids & regions */

/* Structures.								*/
mat_t		xform, notrans, identity;

extern void		blank_fill(), menu();
extern void		quit();

char			getcmd();
void			prompt();
void			eread(), ewrite();

/* Head of linked list of solids */
struct soltab	sol_hd;

struct db_i	*dbip;		/* Database instance ptr */

/*	s o r t F u n c ( )
	Comparison function for qsort().
 */
static int
sortFunc( a, b )
char	**a, **b;
	{
	return( strcmp( *a, *b ) );
	}

/*
 *			M A I N
 */
main( argc, argv )
char	*argv[];
	{
	setbuf( stdout, rt_malloc( BUFSIZ, "stdout buffer" ) );
	RT_LIST_INIT( &(sol_hd.l) );

	if( ! parsArg( argc, argv ) )
		{
		menu( usage );
		exit( 1 );
		}

	/* Build directory from object file.	 	*/
	if( db_scan( dbip, (int (*)())db_diradd, 1 ) < 0 )  {
		fprintf(stderr,"db_scan failure\n");
		exit(1);
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
				rt_free( curr_list[--curr_ct], "curr_list[ct]" );
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
			{	register int	i;
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
			exit( 0 );
		UNKNOWN :
			(void) printf( "Invalid command\n" );
			prompt( PROMPT );
			continue;
			}
		prompt( CMD_PROMPT );		
		}
	}

/*
 *  XXX This is clearly not right, but should do simple test cases.
 */
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
		rt_vls_strncat( vls, op, 2 );
		bit = tp->tr_a.tu_stp->st_bit;
		if(neg) bit = -bit;
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

extern CONST struct db_tree_state	rt_initial_tree_state;

/*
 *			R E G I O N _ E N D
 *
 *  This routine will be called by db_walk_tree() once all the
 *  solids in this region have been visited.
 */
union tree *region_end( tsp, pathp, curtree )
register struct db_tree_state	*tsp;
struct db_full_path	*pathp;
union tree		*curtree;
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
	struct directory	*regdp;
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
	(void) printf( "%4d:%s\n", nnr+delreg, fullname );

	/*
	 *  Write the boolean formula into the region table.
	 */

	/* Convert boolean tree into string of 7-char chunks */
	if( curtree->tr_op == OP_NOP )  {
		rt_vls_strcat( &flat, "" );
	} else {
		/* XXX More may be needed */
		db_non_union_push( curtree );
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
		ewrite( regfd, obuf, strlen(obuf) );
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
	ewrite( ridfd, rt_vls_addr(&ident), rt_vls_strlen(&ident) );

	/* ---------------- */

	rt_vls_free( &ident );
	rt_vls_free( &reg );
	rt_vls_free( &flat );
	rt_free( fullname, "fullname" );

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
union tree *gettree_leaf( tsp, pathp, ep, id )
struct db_tree_state	*tsp;
struct db_full_path	*pathp;
struct rt_external	*ep;
int			id;
{
	register fastf_t	f;
	register struct soltab	*stp;
	union tree		*curtree;
	struct directory	*dp;
	struct rt_db_internal	intern;
	struct rt_tol		tol;
	struct rt_vls		sol;

	RT_VLS_INIT( &sol );

	RT_CK_EXTERNAL(ep);
	dp = DB_FULL_PATH_CUR_DIR(pathp);

	/*
	 *  Check to see if this exact solid has already been processed.
	 *  Match on leaf name and matrix.
	 */
	for( RT_LIST( stp, soltab, &(sol_hd.l) ) )  {
		register int i;

		RT_CHECK_SOLTAB(stp);				/* debug */
		if(	dp->d_namep[0] != stp->st_name[0]  ||	/* speed */
			dp->d_namep[1] != stp->st_name[1]  ||	/* speed */
			strcmp( dp->d_namep, stp->st_name ) != 0
		)
			continue;
		for( i=0; i<16; i++ )  {
			f = tsp->ts_mat[i] - stp->st_pathmat[i];
			if( !NEAR_ZERO(f, 0.0001) )
				goto next_one;
		}
		/* Success, we have a match! */
		if( debug )  {
			rt_log("rt_gettree_leaf:  %s re-referenced\n",
				dp->d_namep );
		}
		goto found_it;
next_one: ;
	}

	GETSTRUCT(stp, soltab);
	stp->l.magic = RT_SOLTAB_MAGIC;
	stp->st_id = id;
	stp->st_dp = dp;
	mat_copy( stp->st_pathmat, tsp->ts_mat );
	stp->st_specific = (genptr_t)0;

	/* init solid's maxima and minima */
	VSETALL( stp->st_max, -INFINITY );
	VSETALL( stp->st_min,  INFINITY );

    	RT_INIT_DB_INTERNAL(&intern);
	if( rt_functab[id].ft_import( &intern, ep, stp->st_pathmat ) < 0 )  {
		rt_log("rt_gettree_leaf(%s):  solid import failure\n", dp->d_namep );
	    	if( intern.idb_ptr )  rt_functab[id].ft_ifree( &intern );
		rt_free( (char *)stp, "struct soltab");
		return( TREE_NULL );		/* BAD */
	}
	RT_CK_DB_INTERNAL( &intern );

	if(debug)  {
		struct rt_vls	str;
		rt_vls_init( &str );
		/* verbose=1, mm2local=1.0 */
		if( rt_functab[id].ft_describe( &str, &intern, 1, 1.0 ) < 0 )  {
			rt_log("rt_gettree_leaf(%s):  solid describe failure\n",
				dp->d_namep );
		}
		rt_log( "%s:  %s", dp->d_namep, rt_vls_addr( &str ) );
		rt_vls_free( &str );
	}

	/* For now, just link them all onto the same list */
	RT_LIST_INSERT( &(sol_hd.l), &(stp->l) );

	/* ---------------- */
	/* XXX output the solid */

	stp->st_bit = ++nns;

	/* Solid number is stp->st_bit + delsol */

	/* Process appropriate solid type.				*/
	switch( intern.idb_type )  {
	case ID_TOR:
		addtor( &sol, (struct rt_tor_internal *)intern.idb_ptr,
			dp->d_namep, stp->st_bit+delsol );
		break;
	case ID_ARB8:
		addarb( &sol, (struct rt_arb_internal *)intern.idb_ptr,
			dp->d_namep, stp->st_bit+delsol );
		break;
	case ID_ELL:
		addell( &sol, (struct rt_ell_internal *)intern.idb_ptr,
			dp->d_namep, stp->st_bit+delsol );
		break;
	case ID_TGC:
		addtgc( &sol, (struct rt_tgc_internal *)intern.idb_ptr,
			dp->d_namep, stp->st_bit+delsol );
		break;
	case ID_ARS:
		addars( &sol, (struct rt_tgc_internal *)intern.idb_ptr,
			dp->d_namep, stp->st_bit+delsol );
		break;
	case ID_HALF:
		/* XXX */
	case ID_PIPE:
		/* XXX */
	default:
		(void) fprintf( stderr,
			"vdeck: '%s' Solid type has no corresponding COMGEOM soild, skipping\n",
			rt_functab[id].ft_name );
		vls_itoa( &sol, stp->st_bit+delsol, 5 );
		rt_vls_strcat( &sol, rt_functab[id].ft_name );
		vls_blanks( &sol, 5*10 );
		rt_vls_strcat( &sol, dp->d_namep );
		rt_vls_strcat( &sol, "\n");
		break;
	}

	ewrite( solfd, rt_vls_addr(&sol), rt_vls_strlen(&sol) );

	/* ---------------- */

	/* Free storage for internal form */
    	if( intern.idb_ptr )  rt_functab[id].ft_ifree( &intern );

found_it:
	GETUNION( curtree, tree );
	curtree->tr_op = OP_SOLID;
	curtree->tr_a.tu_stp = stp;
	curtree->tr_a.tu_name = db_path_to_string( pathp );
	curtree->tr_a.tu_regionp = (struct region *)0;

	return(curtree);
}

/*
 *  Called from deck().
 */
void
treewalk( str )
char	*str;
{
	if( !sol_hd.l.magic )  RT_LIST_INIT( &sol_hd.l );

	if( db_walk_tree( dbip, 1, &str, 1, &rt_initial_tree_state,
	    0, region_end, gettree_leaf ) < 0 )  {
		fprintf(stderr,"Unable to treewalk '%s'\n", str );
	}
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
	{	double	t;
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
addtor( v, gp, name, num )
struct rt_vls		*v;
struct rt_tor_internal	*gp;
char			*name;
int			num;
{
	vect_t	norm;

	RT_VLS_CHECK(v);
	RT_TOR_CK_MAGIC(gp);

	/* V, N, r1, r2 */
	VSCALE( norm, gp->h, 1/gp->r_h );

	vls_itoa( v, num, 5 );
	rt_vls_strcat( v, "tor  " );		/* 5 */
	vls_ftoa_vec_cvt( v, gp->v, 10, 4 );
	vls_ftoa_vec( v, norm, 10, 4 );
	rt_vls_strcat( v, name );
	rt_vls_strcat( v, "\n" );

	vls_itoa( v, num, 5 );
	vls_blanks( v, 5 );
	vls_ftoa( v, gp->r_a*unit_conversion, 10, 4 );
	vls_ftoa( v, gp->r_h*unit_conversion, 10, 4 );
	vls_blanks( v, 4*10 );
	rt_vls_strcat( v, name );
	rt_vls_strcat( v, "\n");
}

static void
vls_solid_pts( v, pts, npts, name, num, kind )
struct rt_vls	*v;
CONST point_t	pts[];
CONST char	*name;
CONST int	num;
CONST char	*kind;
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
		vls_solid_pts( v, pts, 8, name, num, "arb8 " );
		break;
	case 7:
		vls_solid_pts( v, pts, 7, name, num, "arb7 " );
		break;
	case 6:
		VMOVE( pts[5], pts[6] );
		vls_solid_pts( v, pts, 6, name, num, "arb6 " );
		break;
	case 5:
		vls_solid_pts( v, pts, 5, name, num, "arb5 " );
		break;
	case 4:
		VMOVE( pts[3], pts[4] );
		vls_solid_pts( v, pts, 4, name, num, "arb4 " );
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

/*	a d d e l l ( )
	Process the general ellipsoid.
 */
addell( v, gp, name, num )
struct rt_vls		*v;
struct rt_ell_internal	*gp;
char			*name;
int			num;
{
	register int	i;
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

/*	a d d t g c ( )
	Process generalized truncated cone.
 */
addtgc( v, gp, name, num )
struct rt_vls		*v;
struct rt_tgc_internal	*gp;
char			*name;
int			num;
{
	register int	i;
	vect_t	axb, cxd;
	vect_t	work;
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

	/* Tec if ratio top and bot vectors equal and base parallel to top.
	 */
	if( mc == 0.0 )  {
		(void) fprintf( stderr,
				"Error in TGC, C vector has zero magnitude!\n"
				);
		return;
	}
	if( md == 0.0 )  {
		(void) fprintf( stderr,
				"Error in TGC, D vector has zero magnitude!\n"
				);
		return;
	}
	if(	fabs( (mb/md)-(ma/mc) ) < CONV_EPSILON
	    &&  fabs( fabs(VDOT(axb,cxd)) - (maxb*mcxd) ) < CONV_EPSILON
		)
		cgtype = TEC;

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

/*	a d d a r s ( )
	Process triangular surfaced polyhedron - ars.
 */
addars( v, gp, name, num )
struct rt_vls		*v;
struct rt_tgc_internal	*gp;
char			*name;
int			num;
{
#if 0
	char	buf[10];
	register int	i, vec;
	int	npt, npts, ncurves, ngrans, granule, totlen;
	float	work[3], vertex[3];
	hvect_t	v_work, v_workk;

	ngrans = rec->a.a_curlen;
	totlen = rec->a.a_totlen;
	npts = rec->a.a_n;
	ncurves = rec->a.a_m;

	/* Write ars header line in solid table.			*/
	ewrite( solfd, "ars  ", 5 );
	itoa( ncurves, buf, 10 );
	ewrite( solfd, buf, 10 );
	itoa( npts, buf, 10 );
	ewrite( solfd, buf, 10 );
	blank_fill( solfd, 40 );
	ewrite( solfd, rec->a.a_name, (unsigned) strlen( rec->a.a_name ) );
	ewrite( solfd, LF, 1 );

	/* Process the data one granule at a time.			*/
	for( granule = 1; granule <= totlen; granule++ )
		{
		/* Read a granule (ars extension record 'B').		*/
		eread( objfd, (char *) rec, sizeof record );
		/* Find number of points in this granule.		*/
		if( rec->b.b_ngranule == ngrans && (npt = npts % 8) != 0 )
			;
		else
			npt = 8;
		/* Operate on vertex.					*/
		if( granule == 1 )
			{
			vtoh_move( v_workk, &(rec->b.b_values[0]) );
			matXvec( v_work, xform, v_workk );
			htov_move( &(rec->b.b_values[0]), v_work );
			VMOVE( vertex, &(rec->b.b_values[0]) );
			vec = 1;
			}
		else
			vec = 0;

		/* Rest of vectors.					*/
		for( i = vec; i < npt; i++, vec++ )
			{
			vtoh_move( v_workk, &(rec->b.b_values[vec*3]) );
			matXvec( v_work, notrans, v_workk );
			htov_move( work, v_work );
			VADD2( &(rec->b.b_values[vec*3]), vertex, work );
			}

		/* Print the solid parameters.				*/
		parsp( npt, rec );
		}
	return;
#endif
}

#if 0
/*	p a r s p ( )
	Print npts points of an ars.
 */
parsp( npts, rec )
register int	npts;
register Record	*rec;
	{ 	register int	i, j, k, jk;
		char		bufout[80];

	j = jk = 0;

	itoa( nns+delsol, &bufout[0], 5 );
	for( i = 5; i < 10; i++ )
		bufout[i] = ' ';
	(void) strncpy( &bufout[70], "curve ", 6 );
	itoa( rec->b.b_n, &bufout[76], 3 );
	bufout[79] = '\n';

	for( i = 0; i < npts*3; i += 3 )
		{ /* Write 3 points.  */
		for( k = i; k <= i+2; k++ )
			{
			++jk;
			rec->b.b_values[k] *= unit_conversion;
			ftoascii(	rec->b.b_values[k],
					&bufout[jk*10],
					10,
					4
					);
			}
		if( (++j & 01) == 0 )
			{ /* End of line. */
			bufout[70] = 'c';
			ewrite( solfd, bufout, 80 );
			jk = 0;
			}
		}
	if( (j & 01) == 1 )
		{ /* Finish off line. */
		for( k = 40; k < 70; k++ )
			bufout[k] = ' ';
		ewrite( solfd, bufout, 80 );
		}
	return;
	}
#endif

#include <varargs.h>
/*	p r o m p t ( )							*/
/*VARARGS*/
void
prompt( fmt, va_alist )
char	*fmt;
va_dcl
	{	va_list		ap;
	va_start( ap );
	(void) _doprnt( fmt, ap, stdout );
	(void) fflush( stdout );
	va_end( ap );
	return;
	}

/*	e r e a d ( )
	Read with error checking.
 */
void
eread( fd, buf, bytes )
int		fd;
char		*buf;
unsigned	bytes;
	{	int	bytes_read;
	if(	(bytes_read = read( fd, buf, bytes )) != (int) bytes
	    &&	bytes_read != 0
		)
		(void) fprintf( stderr,
				"ERROR: Read of %d bytes returned %d\n",
				bytes,
				bytes_read
				);
	return;
	}

/*	e w r i t e
	Write with error checking.
 */
void
ewrite( fd, buf, bytes )
int		fd;
char		*buf;
unsigned	bytes;
	{	int	bytes_written;
	if( (bytes_written = write( fd, buf, bytes )) != (int) bytes )
		(void) fprintf( stderr,
				"ERROR: Write of %d bytes returned %d\n",
				bytes,
				bytes_written
				);
	return;
	}

pr_dir( dirp )
struct directory	*dirp;
	{
	(void) printf(	"dirp(0x%x)->d_namep=%s d_addr=%ld\n",
			dirp, dirp->d_namep, dirp->d_addr
			);
	(void) fflush( stdout );
	return	1;
	}
