/*
 *			T C L . C
 *
 *  Tcl interfaces to LIBRT routines.
 *
 *  LIBRT routines are not for casual command-line use;
 *  as a result, the Tcl name for the function should be exactly
 *  the same as the C name for the underlying function.
 *  Typing "rt_" is no hardship when writing Tcl procs, and
 *  clarifies the origin of the routine.
 *
 *  Authors -
 *	Michael John Muuss
 *      Glenn Durfee
 *  
 *  Source -
 *	The U. S. Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5068  USA
 *  
 *  Distribution Notice -
 *	Re-distribution of this software is restricted, as described in
 *	your "Statement of Terms and Conditions for the Release of
 *	The BRL-CAD Package" agreement.
 *
 *  Copyright Notice -
 *	This software is Copyright (C) 1997 by the United States Army
 *	in all countries except the USA.  All rights reserved.
 */
#ifndef lint
static char RCSid[] = "@(#)$Header$ (ARL)";
#endif

#include "conf.h"

#include <stdio.h>
#include <ctype.h>
#include <math.h>
#ifdef USE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif

#include "tcl.h"

#include "machine.h"
#include "bu.h"
#include "vmath.h"
#include "bn.h"
#include "rtgeom.h"
#include "raytrace.h"
#include "externs.h"

#include "./debug.h"

#define RT_CK_DBI_TCL(_p)	BU_CKMAG_TCL(interp,_p,(long)DBI_MAGIC,"struct db_i")
#define RT_CK_RTI_TCL(_p)	BU_CKMAG_TCL(interp,_p,(long)RTI_MAGIC,"struct rt_i")
#define RT_CK_WDB_TCL(_p)	BU_CKMAG_TCL(interp,_p,(long)RT_WDB_MAGIC,"struct rt_wdb")

struct dbcmdstruct {
	char *cmdname;
	int (*cmdfunc)();
};

/*
 *			R T _ T C L _ P R _ H I T
 *
 *  Format a hit in a TCL-friendly format.
 *
 *  It is possible that a solid may have been removed from the
 *  directory after this database was prepped, so check pointers
 *  carefully.
 *
 *  It might be beneficial to use some format other than %g to
 *  give the user more precision.
 */
void
rt_tcl_pr_hit( interp, hitp, segp, rayp, flipflag )
Tcl_Interp	*interp;
struct hit	*hitp;
struct seg	*segp;
struct xray	*rayp;
int		flipflag;
{
	struct bu_vls	str;
	vect_t		norm;
	struct soltab	*stp;
	CONST struct directory	*dp;
	struct curvature crv;

	RT_CK_SEG(segp);
	stp = segp->seg_stp;
	RT_CK_SOLTAB(stp);
	dp = stp->st_dp;
	RT_CK_DIR(dp);

	RT_HIT_NORMAL( norm, hitp, stp, rayp, flipflag );
	RT_CURVATURE( &crv, hitp, flipflag, stp );

	bu_vls_init(&str);
	bu_vls_printf( &str, " {dist %g point {", hitp->hit_dist);
	bn_encode_vect( &str, hitp->hit_point );
	bu_vls_printf( &str, "} normal {" );
	bn_encode_vect( &str, norm );
	bu_vls_printf( &str, "} c1 %g c2 %g pdir {",
		crv.crv_c1, crv.crv_c2 );
	bn_encode_vect( &str, crv.crv_pdir );
	bu_vls_printf( &str, "} surfno %d", hitp->hit_surfno );
	if( stp->st_path.magic == DB_FULL_PATH_MAGIC )  {
		/* Magic is left 0 if the path is not filled in. */
		char	*sofar = db_path_to_string(&stp->st_path);
		bu_vls_printf( &str, " path ");
		bu_vls_strcat( &str, sofar );
		bu_free( (genptr_t)sofar, "path string" );
	}
	bu_vls_printf( &str, " solid %s}", dp->d_namep );

	Tcl_AppendResult( interp, bu_vls_addr( &str ), (char *)NULL );
	bu_vls_free( &str );
}

/*
 *			R T _ T C L _ A _ H I T
 */
int
rt_tcl_a_hit( ap, PartHeadp, segHeadp )
struct application	*ap;
struct partition	*PartHeadp;
struct seg		*segHeadp;
{
	Tcl_Interp *interp = (Tcl_Interp *)ap->a_uptr;
	register struct partition *pp;

	RT_CK_PT_HD(PartHeadp);

	for( pp=PartHeadp->pt_forw; pp != PartHeadp; pp = pp->pt_forw )  {
		RT_CK_PT(pp);
		Tcl_AppendResult( interp, "{in", (char *)NULL );
		rt_tcl_pr_hit( interp, pp->pt_inhit, pp->pt_inseg,
			&ap->a_ray, pp->pt_inflip );
		Tcl_AppendResult( interp, "\nout", (char *)NULL );
		rt_tcl_pr_hit( interp, pp->pt_outhit, pp->pt_outseg,
			&ap->a_ray, pp->pt_outflip );
		Tcl_AppendResult( interp,
			"\nregion ",
			pp->pt_regionp->reg_name,
			(char *)NULL );
		Tcl_AppendResult( interp, "}\n", (char *)NULL );
	}

	return 1;
}

/*
 *			R T _ T C L _ A _ M I S S
 */
int
rt_tcl_a_miss( ap )
struct application	*ap;
{
	Tcl_Interp *interp = (Tcl_Interp *)ap->a_uptr;

	return 0;
}


/*
 *			R T _ T C L _ S H O O T R A Y
 *
 *  Usage -
 *	procname shootray {P} dir|at {V}
 *
 *  Example -
 *	set glob_compat_mode 0
 *	.inmem rt_gettrees .rt all.g
 *	.rt shootray {0 0 0} dir {0 0 -1}
 *
 *	set tgt [bu_get_value_by_keyword V [concat type [.inmem get LIGHT]]]
 *	.rt shootray {20 -13.5 20} at $tgt
 *		
 *
 *  Returns -
 *	This "shootray" operation returns a nested set of lists. It returns
 *	a list of zero or more partitions. Inside each partition is a list
 *	containing an in, out, and region keyword, each with an associated
 *	value. The associated value for each "inhit" and "outhit" is itself
 *	a list containing a dist, point, normal, surfno, and solid keyword,
 *	each with an associated value.
 */
int
rt_tcl_rt_shootray( clientData, interp, argc, argv )
ClientData clientData;
Tcl_Interp *interp;
int argc;
char **argv;
{
	struct application	*ap = (struct application *)clientData;
	struct rt_i		*rtip;

	if( argc != 5 )  {
		Tcl_AppendResult( interp,
				"wrong # args: should be \"",
				argv[0], " ", argv[1], " {P} dir|at {V}\"",
				(char *)NULL );
		return TCL_ERROR;
	}

	/* Could core dump */
	RT_AP_CHECK(ap);
	rtip = ap->a_rt_i;
	RT_CK_RTI_TCL(rtip);

	if( bn_decode_vect( ap->a_ray.r_pt,  argv[2] ) != 3 ||
	    bn_decode_vect( ap->a_ray.r_dir, argv[4] ) != 3 )  {
		Tcl_AppendResult( interp,
			"badly formatted vector", (char *)NULL );
		return TCL_ERROR;
	}
	switch( argv[3][0] )  {
	case 'd':
		/* [4] is direction vector */
		break;
	case 'a':
		/* [4] is target point, build a vector from start pt */
		VSUB2( ap->a_ray.r_dir, ap->a_ray.r_dir, ap->a_ray.r_pt );
		break;
	default:
		Tcl_AppendResult( interp,
				"wrong keyword: '", argv[4],
				"', should be one of 'dir' or 'at'",
				(char *)NULL );
		return TCL_ERROR;
	}
	VUNITIZE( ap->a_ray.r_dir );	/* sanity */
	ap->a_hit = rt_tcl_a_hit;
	ap->a_miss = rt_tcl_a_miss;
	ap->a_uptr = (genptr_t)interp;

	(void)rt_shootray( ap );

	return TCL_OK;
}

/*
 *			R T _ T C L _ R T _ O N E H I T
 *  Usage -
 *	procname onehit
 *	procname onehit #
 */
int
rt_tcl_rt_onehit( clientData, interp, argc, argv )
ClientData clientData;
Tcl_Interp *interp;
int argc;
char **argv;
{
	struct application	*ap = (struct application *)clientData;
	struct rt_i		*rtip;
	char			buf[64];

	if( argc < 2 || argc > 3 )  {
		Tcl_AppendResult( interp,
				"wrong # args: should be \"",
				argv[0], " ", argv[1], " [#]\"",
				(char *)NULL );
		return TCL_ERROR;
	}

	/* Could core dump */
	RT_AP_CHECK(ap);
	rtip = ap->a_rt_i;
	RT_CK_RTI_TCL(rtip);

	if( argc == 3 )  {
		ap->a_onehit = atoi(argv[2]);
	}
	sprintf(buf, "%d", ap->a_onehit );
	Tcl_AppendResult( interp, buf, (char *)NULL );
	return TCL_OK;
}

/*
 *			R T _ T C L _ R T _ N O _ B O O L
 *  Usage -
 *	procname no_bool
 *	procname no_bool #
 */
int
rt_tcl_rt_no_bool( clientData, interp, argc, argv )
ClientData clientData;
Tcl_Interp *interp;
int argc;
char **argv;
{
	struct application	*ap = (struct application *)clientData;
	struct rt_i		*rtip;
	char			buf[64];

	if( argc < 2 || argc > 3 )  {
		Tcl_AppendResult( interp,
				"wrong # args: should be \"",
				argv[0], " ", argv[1], " [#]\"",
				(char *)NULL );
		return TCL_ERROR;
	}

	/* Could core dump */
	RT_AP_CHECK(ap);
	rtip = ap->a_rt_i;
	RT_CK_RTI_TCL(rtip);

	if( argc == 3 )  {
		ap->a_no_booleans = atoi(argv[2]);
	}
	sprintf(buf, "%d", ap->a_no_booleans );
	Tcl_AppendResult( interp, buf, (char *)NULL );
	return TCL_OK;
}

/*
 *			R T _ T C L _ R T _ C H E C K
 *
 *  Run some of the internal consistency checkers over the data structures.
 *
 *  Usage -
 *	procname check
 */
int
rt_tcl_rt_check( clientData, interp, argc, argv )
ClientData clientData;
Tcl_Interp *interp;
int argc;
char **argv;
{
	struct application	*ap = (struct application *)clientData;
	struct rt_i		*rtip;
	char			buf[64];

	if( argc != 2 )  {
		Tcl_AppendResult( interp,
				"wrong # args: should be \"",
				argv[0], " ", argv[1], "\"",
				(char *)NULL );
		return TCL_ERROR;
	}

	/* Could core dump */
	RT_AP_CHECK(ap);
	rtip = ap->a_rt_i;
	RT_CK_RTI_TCL(rtip);

	rt_ck(rtip);

	return TCL_OK;
}

/*
 *			R T _ T C L _ R T _ P R E P
 *
 *  When run with no args, just prints current status of prepping.
 *
 *  Usage -
 *	procname prep
 *	procname prep use_air [hasty_prep]
 */
int
rt_tcl_rt_prep( clientData, interp, argc, argv )
ClientData clientData;
Tcl_Interp *interp;
int argc;
char **argv;
{
	struct application	*ap = (struct application *)clientData;
	struct rt_i		*rtip;
	struct bu_vls		str;

	if( argc < 2 || argc > 4 )  {
		Tcl_AppendResult( interp,
				"wrong # args: should be \"",
				argv[0], " ", argv[1],
				" [hasty_prep]\"",
				(char *)NULL );
		return TCL_ERROR;
	}

	/* Could core dump */
	RT_AP_CHECK(ap);
	rtip = ap->a_rt_i;
	RT_CK_RTI_TCL(rtip);

	if( argc >= 3 && !rtip->needprep )  {
		Tcl_AppendResult( interp,
			argv[0], " ", argv[1],
			" invoked when model has already been prepped.\n",
			(char *)NULL );
		return TCL_ERROR;
	}

	if( argc == 4 )  rtip->rti_hasty_prep = atoi(argv[3]);

	/* If args were given, prep now. */
	if( argc >= 3 )  rt_prep_parallel( rtip, 1 );

	/* Now, describe the current state */
	bu_vls_init( &str );
	bu_vls_printf( &str, "hasty_prep %d dont_instance %d useair %d needprep %d",
		rtip->rti_hasty_prep,
		rtip->rti_dont_instance,
		rtip->useair,
		rtip->needprep
	);

	Tcl_AppendResult( interp, bu_vls_addr(&str), (char *)NULL );
	bu_vls_free( &str );
	return TCL_OK;
}

static struct dbcmdstruct rt_tcl_rt_cmds[] = {
	"shootray",	rt_tcl_rt_shootray,
	"onehit",	rt_tcl_rt_onehit,
	"no_bool",	rt_tcl_rt_no_bool,
	"check",	rt_tcl_rt_check,
	"prep",		rt_tcl_rt_prep,
	(char *)0,	(int (*)())0
};

/*
 *			R T _ T C L _ R T
 *
 * Generic interface for the LIBRT_class manipulation routines.
 * Usage:
 *        procname dbCmdName ?args?
 * Returns: result of cmdName LIBRT operation.
 */
int
rt_tcl_rt( clientData, interp, argc, argv )
ClientData clientData;
Tcl_Interp *interp;
int argc;
char **argv;
{
	struct dbcmdstruct	*dbcmd;

	if( argc < 2 ) {
		Tcl_AppendResult( interp,
				  "wrong # args: should be \"", argv[0],
				  " command [args...]\"",
				  (char *)NULL );
		return TCL_ERROR;
	}

	for( dbcmd = rt_tcl_rt_cmds; dbcmd->cmdname != NULL; dbcmd++ ) {
		if( strcmp(dbcmd->cmdname, argv[1]) == 0 ) {
			return (*dbcmd->cmdfunc)( clientData, interp,
						  argc, argv );
		}
	}


	Tcl_AppendResult( interp, "unknown LIBRT command; must be one of:",
			  (char *)NULL );
	for( dbcmd = rt_tcl_rt_cmds; dbcmd->cmdname != NULL; dbcmd++ ) {
		Tcl_AppendResult( interp, " ", dbcmd->cmdname, (char *)NULL );
	}
	return TCL_ERROR;
}

/*
 *		D B _ T C L _ T R E E _ D E S C R I B E
 *
 * Fills a Tcl_DString with a representation of the given tree appropriate
 * for processing by Tcl scripts.  The reason we use Tcl_DStrings instead
 * of bu_vlses is that Tcl_DStrings provide "start/end sublist" commands
 * and automatic escaping of Tcl-special characters.
 *
 * A tree 't' is represented in the following manner:
 *
 *	t := { l dbobjname { mat } }
 *	   | { l dbobjname }
 *	   | { u t1 t2 }
 * 	   | { n t1 t2 }
 *	   | { - t1 t2 }
 *	   | { ^ t1 t2 }
 *         | { ! t1 }
 *	   | { G t1 }
 *	   | { X t1 }
 *	   | { N }
 *	   | {}
 *
 * where 'dbobjname' is a string containing the name of a database object,
 *       'mat'       is the matrix preceeding a leaf,
 *       't1', 't2'  are trees (recursively defined).
 *
 * Notice that in most cases, this tree will be grossly unbalanced.
 */

void
db_tcl_tree_describe( dsp, tp )
Tcl_DString		*dsp;
union tree		*tp;
{
	if( !tp ) return;

	RT_CK_TREE(tp);
	switch( tp->tr_op ) {
	case OP_DB_LEAF:
		Tcl_DStringAppendElement( dsp, "l" );
		Tcl_DStringAppendElement( dsp, tp->tr_l.tl_name );
		if( tp->tr_l.tl_mat )  {
			struct bu_vls vls;
			bu_vls_init( &vls );
			bn_encode_mat( &vls, tp->tr_l.tl_mat );
			Tcl_DStringAppendElement( dsp, bu_vls_addr(&vls) );
			bu_vls_free( &vls );
		}
		break;

		/* This node is known to be a binary op */
	case OP_UNION:
		Tcl_DStringAppendElement( dsp, "u" );
		goto bin;
	case OP_INTERSECT:
		Tcl_DStringAppendElement( dsp, "n" );
		goto bin;
	case OP_SUBTRACT:
		Tcl_DStringAppendElement( dsp, "-" );
		goto bin;
	case OP_XOR:
		Tcl_DStringAppendElement( dsp, "^" );
	bin:
		Tcl_DStringStartSublist( dsp );
		db_tcl_tree_describe( dsp, tp->tr_b.tb_left );
		Tcl_DStringEndSublist( dsp );

		Tcl_DStringStartSublist( dsp );
		db_tcl_tree_describe( dsp, tp->tr_b.tb_right );
		Tcl_DStringEndSublist( dsp );

		break;

		/* This node is known to be a unary op */
	case OP_NOT:
		Tcl_DStringAppendElement( dsp, "!" );
		goto unary;
	case OP_GUARD:
		Tcl_DStringAppendElement( dsp, "G" );
		goto unary;
	case OP_XNOP:
		Tcl_DStringAppendElement( dsp, "X" );
	unary:
		Tcl_DStringStartSublist( dsp );
		db_tcl_tree_describe( dsp, tp->tr_b.tb_left );
		Tcl_DStringEndSublist( dsp );
		break;
			
	case OP_NOP:
		Tcl_DStringAppendElement( dsp, "N" );
		break;

	default:
		bu_log("db_tcl_tree_describe: bad op %d\n", tp->tr_op);
		bu_bomb("db_tcl_tree_describe\n");
	}
}

/*
 *			D B _ T C L _ T R E E _ P A R S E
 *
 *  Take a TCL-style string description of a binary tree, as produced by
 *  db_tcl_tree_describe(), and reconstruct
 *  the in-memory form of that tree.
 */
union tree *
db_tcl_tree_parse( interp, str )
Tcl_Interp	*interp;
char		*str;
{
	int	argc;
	char	**argv;
	union tree	*tp = TREE_NULL;
	union tree	*lhs;
	union tree	*rhs;

	/* Skip over leading spaces in input */
	while( *str && isspace(*str) ) str++;

	if( Tcl_SplitList( interp, str, &argc, &argv ) != TCL_OK )
		return TREE_NULL;

	if( argc <= 0 || argc > 3 )  {
		Tcl_AppendResult( interp, "db_tcl_tree_parse: tree node does not have 1, 2 or 2 elements: ",
			str, "\n", (char *)NULL );
		goto out;
	}

#if 0
Tcl_AppendResult( interp, "\n\ndb_tcl_tree_parse(): ", str, "\n", NULL );

Tcl_AppendResult( interp, "db_tcl_tree_parse() arg0=", argv[0], NULL );
if(argc > 1 ) Tcl_AppendResult( interp, "\n\targ1=", argv[1], NULL );
if(argc > 2 ) Tcl_AppendResult( interp, "\n\targ2=", argv[2], NULL );
Tcl_AppendResult( interp, "\n\n", NULL);
#endif

	if( argv[0][1] != '\0' )  {
		Tcl_AppendResult( interp, "db_tcl_tree_parse() operator is not single character: ",
			argv[0], (char *)NULL );
		goto out;
	}

	switch( argv[0][0] )  {
	case 'l':
		/* Leaf node: {l name {mat}} */
		BU_GETUNION( tp, tree );
		tp->tr_l.magic = RT_TREE_MAGIC;
		tp->tr_op = OP_DB_LEAF;
		tp->tr_l.tl_name = bu_strdup( argv[1] );
		if( argc == 3 )  {
			tp->tr_l.tl_mat = (matp_t)bu_malloc( sizeof(mat_t), "tl_mat");
			if( bn_decode_mat( tp->tr_l.tl_mat, argv[2] ) != 16 )  {
				Tcl_AppendResult( interp, "db_tcl_tree_parse: unable to parse matrix '",
					argv[2], "', using all-zeros", (char *)NULL );
				bn_mat_zero( tp->tr_l.tl_mat );
			}
		}
		break;

	case 'u':
		/* Binary: Union: {u {lhs} {rhs}} */
		BU_GETUNION( tp, tree );
		tp->tr_b.tb_op = OP_UNION;
		goto binary;
	case 'n':
		/* Binary: Intersection */
		BU_GETUNION( tp, tree );
		tp->tr_b.tb_op = OP_INTERSECT;
		goto binary;
	case '-':
		/* Binary: Union */
		BU_GETUNION( tp, tree );
		tp->tr_b.tb_op = OP_SUBTRACT;
		goto binary;
	case '^':
		/* Binary: Xor */
		BU_GETUNION( tp, tree );
		tp->tr_b.tb_op = OP_XOR;
		goto binary;
binary:
		tp->tr_b.magic = RT_TREE_MAGIC;
		if( argv[1] == (char *)NULL || argv[2] == (char *)NULL )  {
			Tcl_AppendResult( interp, "db_tcl_tree_parse: binary operator ",
				argv[0], " has insufficient operands in ",
				str, (char *)NULL );
			bu_free( (char *)tp, "union tree" );
			tp = TREE_NULL;
			goto out;
		}
		tp->tr_b.tb_left = db_tcl_tree_parse( interp, argv[1] );
		if( tp->tr_b.tb_left == TREE_NULL )  {
			bu_free( (char *)tp, "union tree" );
			tp = TREE_NULL;
			goto out;
		}
		tp->tr_b.tb_right = db_tcl_tree_parse( interp, argv[2] );
		if( tp->tr_b.tb_left == TREE_NULL )  {
			db_free_tree( tp->tr_b.tb_left );
			bu_free( (char *)tp, "union tree" );
			tp = TREE_NULL;
			goto out;
		}
		break;

	case '!':
		/* Unary: not {! {lhs}} */
		BU_GETUNION( tp, tree );
		tp->tr_b.tb_op = OP_NOT;
		goto unary;
	case 'G':
		/* Unary: GUARD {G {lhs}} */
		BU_GETUNION( tp, tree );
		tp->tr_b.tb_op = OP_GUARD;
		goto unary;
	case 'X':
		/* Unary: XNOP {X {lhs}} */
		BU_GETUNION( tp, tree );
		tp->tr_b.tb_op = OP_XNOP;
		goto unary;
unary:
		tp->tr_b.magic = RT_TREE_MAGIC;
		if( argv[1] == (char *)NULL )  {
			Tcl_AppendResult( interp, "db_tcl_tree_parse: unary operator ",
				argv[0], " has insufficient operands in ",
				str, "\n", (char *)NULL );
			bu_free( (char *)tp, "union tree" );
			tp = TREE_NULL;
			goto out;
		}
		tp->tr_b.tb_left = db_tcl_tree_parse( interp, argv[1] );
		if( tp->tr_b.tb_left == TREE_NULL )  {
			bu_free( (char *)tp, "union tree" );
			tp = TREE_NULL;
			goto out;
		}
		break;

	case 'N':
		/* NOP: no args.  {N} */
		BU_GETUNION( tp, tree );
		tp->tr_b.tb_op = OP_XNOP;
		tp->tr_b.magic = RT_TREE_MAGIC;
		break;

	default:
		Tcl_AppendResult( interp, "db_tcl_tree_parse: unable to interpret operator '",
			argv[1], "'\n", (char *)NULL );
	}

out:
	free( (char *)argv);		/* not bu_free() */
	return tp;
}

/*
 *		D B _ T C L _ C O M B _ D E S C R I B E
 *
 * Sets the interp->result string to a description of the given combination.
 */

int
db_tcl_comb_describe( interp, comb, item )
Tcl_Interp		*interp;
struct rt_comb_internal *comb;
CONST char		*item;
{
	char buf[128];
	Tcl_DString	ds;
	
	if( !comb ) {
		Tcl_AppendResult( interp, "error: null combination",
				  (char *)NULL );
		return TCL_ERROR;
	}

	if( item==0 ) {
		/* Print out the whole combination. */
		Tcl_DStringInit( &ds );

		Tcl_DStringAppendElement( &ds, "comb" );
		Tcl_DStringAppendElement( &ds, "region" );
		if( comb->region_flag ) {
			Tcl_DStringAppendElement( &ds, "yes" );

			Tcl_DStringAppendElement( &ds, "id" );
			sprintf( buf, "%d", comb->region_id );
			Tcl_DStringAppendElement( &ds, buf );

			if( comb->aircode )  {
				Tcl_DStringAppendElement( &ds, "air" );
				sprintf( buf, "%d", comb->aircode );
				Tcl_DStringAppendElement( &ds, buf );
			}
			if( comb->los )  {
				Tcl_DStringAppendElement( &ds, "los" );
				sprintf( buf, "%d", comb->los );
				Tcl_DStringAppendElement( &ds, buf );
			}

			if( comb->GIFTmater )  {
				Tcl_DStringAppendElement( &ds, "GIFTmater" );
				sprintf( buf, "%d", comb->GIFTmater );
				Tcl_DStringAppendElement( &ds, buf );
			}
		} else {
			Tcl_DStringAppendElement( &ds, "no" );
		}
		
		if( comb->rgb_valid ) {
			Tcl_DStringAppendElement( &ds, "rgb" );
			sprintf( buf, "%d %d %d", V3ARGS(comb->rgb) );
			Tcl_DStringAppendElement( &ds, buf );
		}

		if( bu_vls_strlen(&comb->shader) > 0 )  {
			Tcl_DStringAppendElement( &ds, "shader" );
			Tcl_DStringAppendElement( &ds, bu_vls_addr(&comb->shader) );
		}

		if( bu_vls_strlen(&comb->material) > 0 )  {
			Tcl_DStringAppendElement( &ds, "material" );
			Tcl_DStringAppendElement( &ds, bu_vls_addr(&comb->material) );
		}

		if( comb->inherit ) {
			Tcl_DStringAppendElement( &ds, "inherit" );
			Tcl_DStringAppendElement( &ds, "yes" );
		}

		Tcl_DStringAppendElement( &ds, "tree" );
		Tcl_DStringStartSublist( &ds );
		db_tcl_tree_describe( &ds, comb->tree );
		Tcl_DStringEndSublist( &ds );

		Tcl_DStringResult( interp, &ds );
		Tcl_DStringFree( &ds );

		return TCL_OK;
	} else {
		/* Print out only the requested item. */
		register int i;
		char itemlwr[128];

		for( i = 0; i < 128 && item[i]; i++ ) {
			itemlwr[i] = (isupper(item[i]) ? tolower(item[i]) :
				      item[i]);
		}
		itemlwr[i] = 0;

		if( strcmp(itemlwr, "region")==0 ) {
			strcpy( buf, comb->region_flag ? "yes" : "no" );
		} else if( strcmp(itemlwr, "id")==0 ) {
			if( !comb->region_flag ) goto not_region;
			sprintf( buf, "%d", comb->region_id );
		} else if( strcmp(itemlwr, "air")==0 ) {
			if( !comb->region_flag ) goto not_region;
			sprintf( buf, "%d", comb->aircode );
		} else if( strcmp(itemlwr, "los")==0 ) {
			if( !comb->region_flag ) goto not_region;
			sprintf( buf, "%d", comb->los );
		} else if( strcmp(itemlwr, "giftmater")==0 ) {
			if( !comb->region_flag ) goto not_region;
			sprintf( buf, "%d", comb->GIFTmater );
		} else if( strcmp(itemlwr, "rgb")==0 ) {
			if( comb->rgb_valid )
				sprintf( buf, "%d %d %d", V3ARGS(comb->rgb) );
			else
				strcpy( buf, "invalid" );
		} else if( strcmp(itemlwr, "shader")==0 ) {
			Tcl_AppendResult( interp, bu_vls_addr(&comb->shader),
					  (char *)NULL );
			return TCL_OK;
		} else if( strcmp(itemlwr, "material")==0 ) {
			Tcl_AppendResult( interp, bu_vls_addr(&comb->material),
					  (char *)NULL );
			return TCL_OK;
		} else if( strcmp(itemlwr, "inherit")==0 ) {
			strcpy( buf, comb->inherit ? "yes" : "no" );
		} else if( strcmp(itemlwr, "tree")==0 ) {
			Tcl_DStringInit( &ds );
			db_tcl_tree_describe( &ds, comb->tree );
			Tcl_DStringResult( interp, &ds );
			Tcl_DStringFree( &ds );
			return TCL_OK;
		} else {
			Tcl_AppendResult( interp, "no such attribute",
					  (char *)NULL );
			return TCL_ERROR;
		}

		Tcl_AppendResult( interp, buf, (char *)NULL );
		return TCL_OK;

	not_region:
		Tcl_AppendResult( interp, "item not valid for non-region",
				  (char *)NULL );
		return TCL_ERROR;
	}
}


/*
 *			D B _ T C L _ C O M B _ A D J U S T
 *
 *  Invocation:
 *	rgb "1 2 3" ...
 */

int
db_tcl_comb_adjust( comb, interp, argc, argv )
struct rt_comb_internal	       *comb;
Tcl_Interp		       *interp;
int				argc;
char			      **argv;
{
	char	buf[128];
	int	i;
	double	d;
	
	RT_CK_COMB( comb );

	while( argc >= 2 ) {
		/* Force to lower case */
		for( i=0; i<128 && argv[0][i]!='\0'; i++ )
			buf[i] = isupper(argv[0][i])?tolower(argv[0][i]):argv[0][i];
		buf[i] = 0;

		if( strcmp(buf, "region")==0 ) {
			if( strcmp( argv[1], "none" ) == 0 )
				comb->region_flag = 0;
			else
			{
				if( Tcl_GetBoolean( interp, argv[1], &i )!= TCL_OK )
					return TCL_ERROR;
				comb->region_flag = (char)i;
			}
		} else if( strcmp(buf, "temp")==0 ) {
			if( !comb->region_flag ) goto not_region;
			if( strcmp( argv[1], "none" ) == 0 )
				comb->temperature = 0.0;
			else
			{
				if( Tcl_GetDouble( interp, argv[1], &d ) != TCL_OK )
					return TCL_ERROR;
				comb->temperature = (float)d;
			}
		} else if( strcmp(buf, "id")==0 ) {
			if( !comb->region_flag ) goto not_region;
			if( strcmp( argv[1], "none" ) == 0 )
				comb->region_id = 0;
			else
			{
				if( Tcl_GetInt( interp, argv[1], &i ) != TCL_OK )
					return TCL_ERROR;
				comb->region_id = (short)i;
			}
		} else if( strcmp(buf, "air")==0 ) {
			if( !comb->region_flag ) goto not_region;
			if( strcmp( argv[1], "none" ) == 0 )
				comb->aircode = 0;
			else
			{
				if( Tcl_GetInt( interp, argv[1], &i ) != TCL_OK)
					return TCL_ERROR;
				comb->aircode = (short)i;
			}
		} else if( strcmp(buf, "los")==0 ) {
			if( !comb->region_flag ) goto not_region;
			if( strcmp( argv[1], "none" ) == 0 )
				comb->los = 0;
			else
			{
				if( Tcl_GetInt( interp, argv[1], &i ) != TCL_OK )
					return TCL_ERROR;
				comb->los = (short)i;
			}
		} else if( strcmp(buf, "giftmater")==0 ) {
			if( !comb->region_flag ) goto not_region;
			if( strcmp( argv[1], "none" ) == 0 )
				comb->GIFTmater = 0;
			else
			{
				if( Tcl_GetInt( interp, argv[1], &i ) != TCL_OK )
					return TCL_ERROR;
				comb->GIFTmater = (short)i;
			}
		} else if( strcmp(buf, "rgb")==0 ) {
			if( strcmp(argv[1], "invalid")==0 || strcmp( argv[1], "none" ) == 0 ) {
				comb->rgb[0] = comb->rgb[1] =
					comb->rgb[2] = 0;
				comb->rgb_valid = 0;
			} else {
				unsigned int r, g, b;
				i = sscanf( argv[1], "%u %u %u",
					&r, &g, &b );
				if( i != 3 )   {
					Tcl_AppendResult( interp, "adjust rgb ",
						argv[1], ": not valid rgb 3-tuple\n", (char *)NULL );
					return TCL_ERROR;
				}
				comb->rgb[0] = (unsigned char)r;
				comb->rgb[1] = (unsigned char)g;
				comb->rgb[2] = (unsigned char)b;
				comb->rgb_valid = 1;
			}
		} else if( strcmp(buf, "shader" )==0 ) {
			bu_vls_trunc( &comb->shader, 0 );
			if( strcmp( argv[1], "none" ) )
			{
				bu_vls_strcat( &comb->shader, argv[1] );
				/* Leading spaces boggle the combination exporter */
				bu_vls_trimspace( &comb->shader );
			}
		} else if( strcmp(buf, "material" )==0 ) {
			bu_vls_trunc( &comb->material, 0 );
			if( strcmp( argv[1], "none" ) )
			{
				bu_vls_strcat( &comb->material, argv[1] );
				bu_vls_trimspace( &comb->material );
			}
		} else if( strcmp(buf, "inherit" )==0 ) {
			if( strcmp( argv[1], "none" ) == 0 )
				comb->inherit = 0;
			else
			{
				if( Tcl_GetBoolean( interp, argv[1], &i ) != TCL_OK )
					return TCL_ERROR;
				comb->inherit = (char)i;
			}
		} else if( strcmp(buf, "tree" )==0 ) {
			union tree	*new;

			if( strcmp( argv[1], "none" ) == 0 )
			{
				db_free_tree( comb->tree );
				comb->tree = TREE_NULL;
			}
			else
			{
				new = db_tcl_tree_parse( interp, argv[1] );
				if( new == TREE_NULL )  {
					Tcl_AppendResult( interp, "db adjust tree: bad tree '",
						argv[1], "'\n", (char *)NULL );
					return TCL_ERROR;
				}
				if( comb->tree )
					db_free_tree( comb->tree );
				comb->tree = new;
			}
		} else {
			Tcl_AppendResult( interp, "db adjust ", buf,
					  ": no such attribute",
					  (char *)NULL );
			return TCL_ERROR;
		}
		argc -= 2;
		argv += 2;
	}

	return TCL_OK;

 not_region:
	Tcl_AppendResult( interp, "attribute not valid for non-region",
			  (char *)NULL );
	return TCL_ERROR;
}

/*
 *			R T _ D B _ M A T C H
 *
 * Returns (in interp->result) a list (possibly empty) of all matches to
 * the (possibly wildcard-containing) arguments given.
 * Does *NOT* return tokens that do not match anything, unlike the
 * "expand" command.
 */

int
rt_db_match( clientData, interp, argc, argv )
ClientData	clientData;
Tcl_Interp     *interp;
int		argc;
char	      **argv;
{
	struct rt_wdb  *wdb = (struct rt_wdb *)clientData;
	struct bu_vls	matches;

	--argc;
	++argv;

	RT_CK_WDB_TCL(wdb);
	
	/* Verify that this wdb supports lookup operations
	   (non-null dbip) */
	if( wdb->dbip == 0 ) {
		Tcl_AppendResult( interp, "this database does not support lookup operations" );
		return TCL_ERROR;
	}

	bu_vls_init( &matches );
	for( ++argv; *argv != NULL; ++argv ) {
		if( db_regexp_match_all( &matches, wdb->dbip, *argv ) > 0 )
			bu_vls_strcat( &matches, " " );
	}
	bu_vls_trimspace( &matches );
	Tcl_AppendResult( interp, bu_vls_addr(&matches), (char *)NULL );
	bu_vls_free( &matches );
	return TCL_OK;
}

/*
 *			R T _ D B _ R E P O R T
 *
 *  Report on a particular object from the database.
 *  Used to support "db get".
 *  Also used in MGED for get_edit_solid.
 */
int
rt_db_report( interp, intern, attr )
Tcl_Interp		*interp;
struct rt_db_internal	*intern;
char			*attr;
{
	register CONST struct bu_structparse	*sp = NULL;
	register CONST struct rt_functab	*ftp;

	int                     id, status;
	Tcl_DString             ds;
	struct bu_vls           str;

	RT_CK_DB_INTERNAL( intern );

       /* Find out what type of object we are dealing with and report on it. */
	id = intern->idb_type;
	ftp = intern->idb_meth;
	RT_CK_FUNCTAB(ftp);

	switch( id ) {
	case ID_COMBINATION:
		status = db_tcl_comb_describe( interp,
			       (struct rt_comb_internal *)intern->idb_ptr,
					       attr );
		break;
	default:
		if( ftp->ft_parsetab == (struct bu_structparse *)NULL ) {
			Tcl_AppendResult( interp, ftp->ft_label,
 " {a Tcl output routine for this type of object has not yet been implemented}",
				  (char *)NULL );
			return TCL_OK;
		}

		bu_vls_init( &str );
		Tcl_DStringInit( &ds );

		sp = ftp->ft_parsetab;
		if( attr == (char *)0 ) {
			/* Print out solid type and all attributes */
			Tcl_DStringAppendElement( &ds, ftp->ft_label );
			while( sp->sp_name != NULL ) {
				Tcl_DStringAppendElement( &ds, sp->sp_name );
				bu_vls_trunc( &str, 0 );
				bu_vls_struct_item( &str, sp,
						 (char *)intern->idb_ptr, ' ' );
				Tcl_DStringAppendElement( &ds,
							  bu_vls_addr(&str) );
				++sp;
			}
			status = TCL_OK;
		} else {
			if( bu_vls_struct_item_named( &str, sp, attr,
					   (char *)intern->idb_ptr, ' ') < 0 ) {
				Tcl_DStringAppend( &ds, "no such attribute",
						   17 );
				status = TCL_ERROR;
			} else {
				Tcl_DStringAppendElement( &ds,
							  bu_vls_addr(&str) );
				status = TCL_OK;
			}
		}

		Tcl_DStringResult( interp, &ds );
		Tcl_DStringFree( &ds );
		bu_vls_free( &str );
	}

	return status;
}

/*
 *			R T _ D B _ G E T
 *
 **
 ** For use with Tcl, this routine accepts as its first argument the name
 ** of an object in the database.  If only one argument is given, this routine
 ** then fills the result string with the (minimal) attributes of the item.
 ** If a second, optional, argument is provided, this function looks up the
 ** property with that name of the item given, and returns it as the result
 ** string.
 **/

int
rt_db_get( clientData, interp, argc, argv )
ClientData	clientData;
Tcl_Interp     *interp;
int		argc;
char	      **argv;
{
	register struct directory	       *dp;
	register struct bu_structparse	       *sp = NULL;
	int			id, status;
	struct rt_db_internal	intern;
	char			objecttype;
	char		       *objname;
	struct rt_wdb	       *wdb = (struct rt_wdb *)clientData;
	Tcl_DString		ds;
	struct bu_vls		str;
	char			*curr_elem;

	--argc;
	++argv;

	if( argc < 2 || argc > 3) {
		Tcl_AppendResult( interp,
				  "wrong # args: should be \"", argv[0],
				  " objName ?attr?\"", (char *)NULL );
		return TCL_ERROR;
	}

	/* Verify that this wdb supports lookup operations
	   (non-null dbip) */
	if( wdb->dbip==0 ) {
		Tcl_AppendResult( interp,
				  "db does not support lookup operations",
				  (char *)NULL );
		return TCL_ERROR;
	}

#if 0
	dp = db_lookup( wdb->dbip, argv[1], LOOKUP_QUIET );
	if( dp == NULL ) {
		Tcl_AppendResult( interp, argv[1], ": not found\n",
				  (char *)NULL );
		return TCL_ERROR;
	}

	status = rt_db_get_internal( &intern, dp, wdb->dbip, (matp_t)NULL );
	if( status < 0 ) {
		Tcl_AppendResult( interp, "rt_db_get_internal failure: ",
				  argv[1], (char *)NULL );
		return TCL_ERROR;
	}
#else
	if( strchr( argv[1], '/' ) )
	{
		/* This is a path */
		struct db_tree_state	ts;
		struct db_full_path	old_path;
		struct db_full_path	new_path;
		struct directory	*dp_curr;
		int			ret;

		db_init_db_tree_state( &ts, wdb->dbip );
		db_full_path_init(&old_path);
		db_full_path_init(&new_path);

		if( db_string_to_path( &new_path, wdb->dbip, argv[1] ) < 0 )  {
			Tcl_AppendResult(interp, "tcl_follow_path: '",
				argv[1], "' contains unknown object names\n", (char *)NULL);
			return TCL_ERROR;
		}

		dp_curr = DB_FULL_PATH_CUR_DIR( &new_path );
		ret = db_follow_path( &ts, &old_path, &new_path, LOOKUP_NOISY );
		db_free_full_path( &old_path );
		db_free_full_path( &new_path );

		if( ret < 0 )  {
			Tcl_AppendResult(interp, "tcl_follow_path: '",
				argv[1], "' is a bad path\n", (char *)NULL );
			return TCL_ERROR;
		}

		status = wdb_import( wdb, &intern, dp_curr->d_namep, ts.ts_mat );
		if( status == -4 )  {
			Tcl_AppendResult( interp, dp_curr->d_namep, ": not found\n",
					  (char *)NULL );
			return TCL_ERROR;
		}
		if( status < 0 ) {
			Tcl_AppendResult( interp, "wdb_import failure: ",
					  dp_curr->d_namep, (char *)NULL );
			return TCL_ERROR;
		}
	}
	else
	{
		status = wdb_import( wdb, &intern, argv[1], (matp_t)NULL );
		if( status == -4 )  {
			Tcl_AppendResult( interp, argv[1], ": not found\n",
					  (char *)NULL );
			return TCL_ERROR;
		}
		if( status < 0 ) {
			Tcl_AppendResult( interp, "wdb_import failure: ",
					  argv[1], (char *)NULL );
			return TCL_ERROR;
		}
	}
#endif
	status = rt_db_report( interp, &intern, argv[2] );

	intern.idb_meth->ft_ifree( &intern );
	return status;
}

/*
 *			R T _ D B _ P U T
 **
 ** Creates an object and stuffs it into the databse.
 ** All arguments must be specified.  Object cannot already exist.
 **/

int
rt_db_put(clientData, interp, argc, argv)
ClientData	clientData;
Tcl_Interp     *interp;
int		argc;
char	      **argv;
{
	struct rt_db_internal			intern;
	register CONST struct rt_functab	*ftp;
	register struct directory	       *dp;
	int					status, ngran, i;
	char				       *name;
	struct rt_wdb			  *wdb = (struct rt_wdb *)clientData;
	char				        type[16];

	--argc;
	++argv;

	if( argc < 2 ) {
		Tcl_AppendResult( interp,
 	               "wrong # args: should be db put objName objType attrs",
				  (char *)NULL );
		return TCL_ERROR;
	}

	name = argv[1];
    
	/* XXX
	   Verify that this wdb supports lookup operations (non-null dbip) */
	/* If not, just skip the lookup test and write the object */

	if( db_lookup(wdb->dbip, argv[1], LOOKUP_QUIET) != DIR_NULL ) {
		Tcl_AppendResult(interp, argv[1], " already exists",
				 (char *)NULL);
		return TCL_ERROR;
	}

	RT_INIT_DB_INTERNAL(&intern);

	for( i = 0; argv[2][i] != 0 && i < 16; i++ ) {
		type[i] = isupper(argv[2][i]) ? tolower(argv[2][i]) :
			  argv[2][i];
	}
	type[i] = 0;

	if( strcmp(type, "comb")==0 ) {
		struct rt_comb_internal *comb;
		intern.idb_type = ID_COMBINATION;
		intern.idb_ptr = bu_calloc( sizeof(struct rt_comb_internal), 1,
					    "rt_db_put" );
		/* Create combination with appropriate values */
		comb = (struct rt_comb_internal *)intern.idb_ptr;
		comb->magic = (long)RT_COMB_MAGIC;
		comb->temperature = -1;
		comb->tree = (union tree *)0;
		comb->region_flag = 1;
		comb->region_id = 0;
		comb->aircode = 0;
		comb->GIFTmater = 0;
		comb->los = 0;
		comb->rgb_valid = 0;
		comb->rgb[0] = comb->rgb[1] = comb->rgb[2] = 0;
		bu_vls_init( &comb->shader );
		bu_vls_init( &comb->material );
		comb->inherit = 0;

		if( db_tcl_comb_adjust( comb, interp, argc-3,
					argv+3 ) == TCL_ERROR ) {
			rt_db_free_internal( &intern );
			return TCL_ERROR;
		}
	} else {
		ftp = rt_get_functab_by_label( type );
		if( ftp == NULL ) {
			Tcl_AppendResult( interp, type,
					  " is an unknown object type.",
					  (char *)NULL );
			return TCL_ERROR;
		}

		if( ftp->ft_parsetab == (struct bu_structparse *)NULL ) {
			Tcl_AppendResult( interp, type,
					  " type objects do not yet have a 'db put' handler.",
					  (char *)NULL );
			return TCL_ERROR;
		}

		intern.idb_type = ftp - rt_functab;
		BU_ASSERT(&rt_functab[intern.idb_type] == ftp);
		intern.idb_meth = ftp;
		intern.idb_ptr = bu_calloc( ftp->ft_internal_size, 1,
					    "rt_db_put" );
		*((long *)intern.idb_ptr) = ftp->ft_internal_magic;
		if( bu_structparse_argv(interp, argc-3, argv+3, ftp->ft_parsetab,
					(char *)intern.idb_ptr )==TCL_ERROR ) {
			rt_db_free_internal(&intern);
			return TCL_ERROR;
		}
	}

	if( wdb_export( wdb, name, intern.idb_ptr, intern.idb_type,
			1.0 ) < 0 )  {
		Tcl_AppendResult( interp, "wdb_export(", argv[1],
				  ") failure\n", (char *)NULL );
		rt_db_free_internal( &intern );
		return TCL_ERROR;
	}

	rt_db_free_internal( &intern );
	return TCL_OK;
}

/*
 *			R T _ D B _ A D J U S T
 *
 **
 ** For use with Tcl, this routine accepts as its first argument an item in
 ** the database; as its remaining arguments it takes the properties that
 ** need to be changed and their values.
 **	.inmem adjust LIGHT V { -46 -13 5 }
 **/

int
rt_db_adjust( clientData, interp, argc, argv )
ClientData	clientData;
Tcl_Interp     *interp;
int		argc;
char	      **argv;
{
	register struct directory	*dp;
	register CONST struct bu_structparse	*sp = NULL;
	int				 id, status, i;
	char				*name;
	struct rt_db_internal		 intern;
	mat_t				 idn;
	char				 objecttype;
	struct rt_wdb		        *wdb = (struct rt_wdb *)clientData;

	if( argc < 5 ) {
		Tcl_AppendResult( interp,
		"wrong # args: should be \"db adjust objName attr value ?attr? ?value?...\"",
				  (char *)NULL );
		return TCL_ERROR;
	}
	name = argv[2];

	/* XXX
	   Verify that this wdb supports lookup operations (non-null dbip) */
	RT_CK_DBI_TCL(wdb->dbip);

	dp = db_lookup( wdb->dbip, name, LOOKUP_QUIET );
	if( dp == DIR_NULL ) {
		Tcl_AppendResult( interp, name, ": not found\n",
				  (char *)NULL );
		return TCL_ERROR;
	}

	status = rt_db_get_internal( &intern, dp, wdb->dbip, (matp_t)NULL );
	if( status < 0 ) {
		Tcl_AppendResult( interp, "rt_db_get_internal(", name,
				  ") failure\n", (char *)NULL );
		return TCL_ERROR;
	}
	RT_CK_DB_INTERNAL( &intern );

	/* Find out what type of object we are dealing with and tweak it. */
	id = intern.idb_type;
	RT_CK_FUNCTAB(intern.idb_meth);

	if( id == ID_COMBINATION ) {
		struct rt_comb_internal *comb =
			(struct rt_comb_internal *)intern.idb_ptr;
		RT_CK_COMB(comb);

		/* .inmem adjust light.r rgb { 255 255 255 } */
		status = db_tcl_comb_adjust( comb, interp, argc-3, argv+3 );
	} else if( (sp = intern.idb_meth->ft_parsetab) == NULL )  {
		Tcl_AppendResult( interp,
	           "Manipulation routines for objects of type ",
		   intern.idb_meth->ft_label,
		   " have not yet been implemented.",
				  (char *)NULL );
		status = TCL_ERROR;
	} else {
		/* If we were able to find an entry in the strutparse "cheat sheet",
		   just use the handy parse functions to return the object.
		   Pass first attribute as argv[0].
		 */
		/* .inmem adjust LIGHT V { -46 -13 5 } */
		status = bu_structparse_argv( interp, argc-3, argv+3, sp,
					      (char *)intern.idb_ptr );
		if( status != TCL_OK )  Tcl_AppendResult( interp,
			"bu_structparse_argv(", name, ") failure\n", (char *)NULL );
		/* fall through */
	}

	if( status == TCL_OK && wdb_export( wdb, name, intern.idb_ptr,
					    intern.idb_type, 1.0 ) < 0 )  {
		Tcl_AppendResult( interp, "wdb_export(", name,
				  ") failure\n", (char *)NULL );
		rt_db_free_internal( &intern );
		return TCL_ERROR;
	}

	rt_db_free_internal( &intern );
	return status;
}

/*
 *			R T _ D B _ F O R M
 */
int
rt_db_form( clientData, interp, argc, argv )
ClientData clientData;
Tcl_Interp *interp;
int argc;
char **argv;
{
	CONST struct bu_structparse	*sp = NULL;
	CONST struct rt_functab		*ftp;

	--argc;
	++argv;

	if (argc != 2) {
		Tcl_AppendResult(interp, "wrong # args: should be \"db form objType\"",
				 (char *)NULL);
		return TCL_ERROR;
	}

	if( (ftp = rt_get_functab_by_label(argv[1])) == NULL )  {
		Tcl_AppendResult(interp, "There is no geometric object type \"",
			argv[1], "\".", (char *)NULL);
		return TCL_ERROR;
	}
	sp = ftp->ft_parsetab;
    
	if (sp != NULL)
		bu_structparse_get_terse_form(interp, sp);
	else {
		Tcl_AppendResult(interp, argv[1],
			" is a valid object type, but a 'form' routine has not yet been implemented.",
			(char *)NULL );
		return TCL_ERROR;
	}
	return TCL_OK;
}

/*
 *			R T _ D B _ T O P S
 */

int
rt_db_tops( clientData, interp, argc, argv )
ClientData	clientData;
Tcl_Interp     *interp;
int		argc;
char	      **argv;
{
	struct rt_wdb *wdp = (struct rt_wdb *)clientData;
	register struct directory *dp;
	register int i;

	RT_CK_WDB_TCL( wdp );
	RT_CK_DBI_TCL( wdp->dbip );

	/* Can this be executed only sometimes?
	   Perhaps a "dirty bit" on the database? */
	db_update_nref( wdp->dbip );
	
	for( i = 0; i < RT_DBNHASH; i++ )
		for( dp = wdp->dbip->dbi_Head[i];
		     dp != DIR_NULL;
		     dp = dp->d_forw )
			if( dp->d_nref == 0 )
				Tcl_AppendElement( interp, dp->d_namep );
	return TCL_OK;
}

/*
 *			R T _ T C L _ D E L E T E P R O C _ R T
 *
 *  Called when the named proc created by rt_gettrees() is destroyed.
 */
void
rt_tcl_deleteproc_rt(clientData)
ClientData clientData;
{
	struct application	*ap = (struct application *)clientData;
	struct rt_i		*rtip;

	RT_AP_CHECK(ap);
	rtip = ap->a_rt_i;
	RT_CK_RTI(rtip);

	rt_free_rti(rtip);
	ap->a_rt_i = (struct rt_i *)NULL;

	bu_free( (genptr_t)ap, "struct application" );
}

/*
 *			R T _ D B _ R T _ G E T T R E E S
 *
 *  Given an instance of a database and the name of some treetops,
 *  create a named "ray-tracing" object (proc) which will respond to
 *  subsequent operations.
 *  Returns new proc name as result.
 *
 *  Example:
 *	.inmem rt_gettrees .rt all.g light.r
 */
int
rt_db_rt_gettrees( clientData, interp, argc, argv )
ClientData	clientData;
Tcl_Interp     *interp;
int		argc;
char	      **argv;
{
	struct rt_wdb *wdp = (struct rt_wdb *)clientData;
	struct rt_i	*rtip;
	struct application	*ap;
	struct resource		*resp;
	char		*newprocname;
	char		buf[64];

	RT_CK_WDB_TCL( wdp );
	RT_CK_DBI_TCL( wdp->dbip );

	if( argc < 4 )  {
		Tcl_AppendResult( interp,
			"rt_gettrees: wrong # args: should be \"",
			argv[0], " ", argv[1],
			" newprocname [-i] [-u] treetops...\"\n", (char *)NULL );
		return TCL_ERROR;
	}

	rtip = rt_new_rti( wdp->dbip );
	newprocname = argv[2];

	/* Delete previous proc (if any) to release all that memory, first */
	(void)Tcl_DeleteCommand( interp, newprocname );

	while( argv[3][0] == '-' )  {
		if( strcmp( argv[3], "-i" ) == 0 )  {
			rtip->rti_dont_instance = 1;
			argc--;
			argv++;
			continue;
		}
		if( strcmp( argv[3], "-u" ) == 0 )  {
			rtip->useair = 1;
			argc--;
			argv++;
			continue;
		}
		break;
	}

	if( rt_gettrees( rtip, argc-3, (CONST char **)&argv[3], 1 ) < 0 )  {
		Tcl_AppendResult( interp,
			"rt_gettrees() returned error\n", (char *)NULL );
		rt_free_rti( rtip );
		return TCL_ERROR;
	}

	/* Establish defaults for this rt_i */
	rtip->rti_hasty_prep = 1;	/* Tcl isn't going to fire many rays */

	/*
	 *  In case of multiple instances of the library, make sure that
	 *  each instance has a separate resource structure,
	 *  because the bit vector lengths depend on # of solids.
	 *  And the "overwrite" sequence in Tcl is to create the new
	 *  proc before running the Tcl_CmdDeleteProc on the old one,
	 *  which in this case would trash rt_uniresource.
	 *  Once on the rti_resources list, rt_clean() will clean 'em up.
	 */
	BU_GETSTRUCT( resp, resource );
	rt_init_resource( resp, 0 );
	bu_ptbl_ins_unique( &rtip->rti_resources, (long *)resp );

	BU_GETSTRUCT( ap, application );
	ap->a_resource = resp;
	ap->a_rt_i = rtip;
	ap->a_purpose = "Conquest!";

	rt_ck(rtip);

	/* Instantiate the proc, with clientData of wdb */
	/* Beware, returns a "token", not TCL_OK. */
	(void)Tcl_CreateCommand( interp, newprocname, rt_tcl_rt,
				 (ClientData)ap, rt_tcl_deleteproc_rt );

	/* Return new function name as result */
	Tcl_AppendResult( interp, newprocname, (char *)NULL );

	return TCL_OK;

}

/*
 *			R T _ D B _ D U M P
 *
 *  Write the current state of a database object out to a file.
 *
 *  Example:
 *	.inmem dump "/tmp/foo.g"
 */
int
rt_db_dump( clientData, interp, argc, argv )
ClientData	clientData;
Tcl_Interp     *interp;
int		argc;
char	      **argv;
{
	struct rt_wdb	*wdp = (struct rt_wdb *)clientData;
	struct rt_wdb	*op;
	int		ret;

	RT_CK_WDB_TCL( wdp );
	RT_CK_DBI_TCL( wdp->dbip );

	if( argc != 3 )  {
		Tcl_AppendResult( interp,
			"dump: wrong # args: should be \"",
			argv[0], "dump filename.g\n", (char *)NULL );
		return TCL_ERROR;
	}

	if( (op = wdb_fopen( argv[2] )) == RT_WDB_NULL )  {
		Tcl_AppendResult( interp,
			argv[0], " dump:  ", argv[2], ": cannot create\n",
			(char *)NULL );
		return TCL_ERROR;
	}
	ret = db_dump( op, wdp->dbip );
	wdb_close( op );
	if( ret < 0 )  {
		Tcl_AppendResult( interp,
			argv[0], " dump ", argv[2], ": db_dump() error\n",
			(char *)NULL );
		return TCL_ERROR;
	}
	return TCL_OK;
}

static struct dbcmdstruct rt_db_cmds[] = {
	"match",	rt_db_match,
	"get",		rt_db_get,
	"put",		rt_db_put,
	"adjust",	rt_db_adjust,
	"form",		rt_db_form,
	"tops",		rt_db_tops,
	"rt_gettrees",	rt_db_rt_gettrees,
	"dump",		rt_db_dump,
	(char *)0,	(int (*)())0
};

/*
 *			R T _ D B
 *
 * Generic interface for the database_class manipulation routines.
 * Usage:
 *        procname dbCmdName ?args?
 * Returns: result of cmdName database command.
 */

int
rt_db( clientData, interp, argc, argv )
ClientData clientData;
Tcl_Interp *interp;
int argc;
char **argv;
{
	struct dbcmdstruct	*dbcmd;
	struct rt_wdb		*wdb = (struct rt_wdb *)clientData;

	if( argc < 2 ) {
		Tcl_AppendResult( interp,
				  "wrong # args: should be \"", argv[0],
				  " command [args...]\"",
				  (char *)NULL );
		return TCL_ERROR;
	}

	/* Could core dump */
	RT_CK_WDB_TCL(wdb);

	for( dbcmd = rt_db_cmds; dbcmd->cmdname != NULL; dbcmd++ ) {
		if( strcmp(dbcmd->cmdname, argv[1]) == 0 ) {
			return (*dbcmd->cmdfunc)( clientData, interp,
						  argc, argv );
		}
	}


	Tcl_AppendResult( interp, "unknown database command; must be one of:",
			  (char *)NULL );
	for( dbcmd = rt_db_cmds; dbcmd->cmdname != NULL; dbcmd++ ) {
		Tcl_AppendResult( interp, " ", dbcmd->cmdname, (char *)NULL );
	}
	Tcl_AppendResult( interp, "\n", (char *)NULL );
	return TCL_ERROR;
}

/*
 *			R T _ T C L _ D E L E T E P R O C _ W D B
 *
 *  Called when the named proc created by wdb_open() is destroyed.
 *  This is used instead of a "close" operation on the object.
 */
void
rt_tcl_deleteproc_wdb(clientData)
ClientData clientData;
{
	struct rt_wdb	*wdbp = (struct rt_wdb *)clientData;
	RT_CK_WDB(wdbp);

	wdb_close(wdbp);
}

/*
 *			W D B _ O P E N
 *
 *  A TCL interface to wdb_fopen() and wdb_dbopen().
 *
 *  Implicit return -
 *	Creates a new TCL proc which responds to get/put/etc. arguments
 *	when invoked.  clientData of that proc will be wdb pointer
 *	for this instance of the database.
 *	Easily allows keeping track of multiple databases.
 *
 *  Explicit return -
 *	wdb pointer, for more traditional C-style interfacing.
 *
 *  Example -
 *	set wdbp [wdb_open .inmem inmem $dbip]
 *	.inmem get box.s
 *	.inmem close
 */
int
wdb_open( clientData, interp, argc, argv )
ClientData	clientData;
Tcl_Interp	*interp;
int		argc;
char		**argv;
{
	struct rt_wdb	*wdb;
	char		buf[32];

	if( argc != 4 )  {
		Tcl_AppendResult(interp, "\
Usage: wdb_open newprocname file filename\n\
       wdb_open newprocname disk $dbip\n\
       wdb_open newprocname disk_append $dbip\n\
       wdb_open newprocname inmem $dbip\n\
       wdb_open newprocname inmem_append $dbip\n",
		NULL);
		return TCL_ERROR;
	}

	/* Delete previous proc (if any) to release all that memory, first */
	(void)Tcl_DeleteCommand( interp, argv[1] );

	if( strcmp( argv[2], "file" ) == 0 )  {
		wdb = wdb_fopen( argv[3] );
	} else {
		struct db_i	*dbip;

		dbip = (struct db_i *)atol( argv[3] );
		/* Could core dump */
		RT_CK_DBI_TCL(dbip);

		if( strcmp( argv[2], "disk" ) == 0 )  {
			wdb = wdb_dbopen( dbip, RT_WDB_TYPE_DB_DISK );
		} else if( strcmp( argv[2], "disk_append" ) == 0 )  {
			wdb = wdb_dbopen( dbip, RT_WDB_TYPE_DB_DISK_APPEND_ONLY );
		} else if( strcmp( argv[2], "inmem" ) == 0 )  {
			wdb = wdb_dbopen( dbip, RT_WDB_TYPE_DB_INMEM );
		} else if( strcmp( argv[2], "inmem_append" ) == 0 )  {
			wdb = wdb_dbopen( dbip, RT_WDB_TYPE_DB_INMEM_APPEND_ONLY );
		} else {
			Tcl_AppendResult(interp, "wdb_open ", argv[2],
				" target type not recognized\n", NULL);
			return TCL_ERROR;
		}
	}
	if( wdb == RT_WDB_NULL )  {
		Tcl_AppendResult(interp, "wdb_open ", argv[1], " failed\n", NULL);
		return TCL_ERROR;
	}

	/* Instantiate the newprocname, with clientData of wdb */
	/* Beware, returns a "token", not TCL_OK. */
	(void)Tcl_CreateCommand( interp, argv[1], rt_db,
				 (ClientData)wdb, rt_tcl_deleteproc_wdb );

	/* Return new function name as result */
	Tcl_AppendResult( interp, argv[1], (char *)NULL );
	
	return TCL_OK;
}

/*
 *			D B _ T C L _ F O L L O W _ P A T H
 *
 *  Provides the same format output as a "db get" operation on a
 *  combination, as much as possible.
 */
int
db_tcl_follow_path( clientData, interp, argc, argv )
ClientData	clientData;
Tcl_Interp	*interp;
int		argc;
char		**argv;
{
	struct db_i		*dbip;
	struct db_tree_state	ts;
	struct db_full_path	old_path;
	struct db_full_path	new_path;
	Tcl_DString		ds;
	char			buf[128];
	int			ret;

	if( argc != 3 )  {
		Tcl_AppendResult(interp, "Usage: db_follow_path [get_dbip] path\n", NULL);
		return TCL_ERROR;
	}

	dbip = (struct db_i *)atol( argv[1] );
	/* Could core dump */
	RT_CK_DBI_TCL(dbip);

	db_init_db_tree_state( &ts, dbip );
	db_full_path_init(&old_path);
	db_full_path_init(&new_path);

	if( db_string_to_path( &new_path, dbip, argv[2] ) < 0 )  {
		Tcl_AppendResult(interp, "tcl_follow_path: '",
			argv[2], "' contains unknown object names\n", (char *)NULL);
		return TCL_ERROR;
	}
	ret = db_follow_path( &ts, &old_path, &new_path, LOOKUP_NOISY );
	db_free_full_path( &old_path );
	db_free_full_path( &new_path );

	if( ret < 0 )  {
		Tcl_AppendResult(interp, "tcl_follow_path: '",
			argv[2], "' is a bad path\n", (char *)NULL );
		return TCL_ERROR;
	}

	/* Could be called db_tcl_tree_state_describe() */
	Tcl_DStringInit( &ds );

	Tcl_DStringAppendElement( &ds, "region" );
	if( ts.ts_sofar & TS_SOFAR_REGION ) {
		Tcl_DStringAppendElement( &ds, "yes" );

		Tcl_DStringAppendElement( &ds, "id" );
		sprintf( buf, "%d", ts.ts_regionid );
		Tcl_DStringAppendElement( &ds, buf );

		if( ts.ts_aircode )  {
			Tcl_DStringAppendElement( &ds, "air" );
			sprintf( buf, "%d", ts.ts_aircode );
			Tcl_DStringAppendElement( &ds, buf );
		}

		if( ts.ts_los )  {
			Tcl_DStringAppendElement( &ds, "los" );
			sprintf( buf, "%d", ts.ts_los );
			Tcl_DStringAppendElement( &ds, buf );
		}

		if( ts.ts_gmater )  {
			Tcl_DStringAppendElement( &ds, "GIFTmater" );
			sprintf( buf, "%d", ts.ts_gmater );
			Tcl_DStringAppendElement( &ds, buf );
		}
	} else {
		Tcl_DStringAppendElement( &ds, "no" );
	}
		
	if( ts.ts_mater.ma_color_valid ) {
		Tcl_DStringAppendElement( &ds, "rgb" );
		sprintf( buf, "%d %d %d",
			(int)(ts.ts_mater.ma_color[0]*255),
			(int)(ts.ts_mater.ma_color[1]*255),
			(int)(ts.ts_mater.ma_color[2]*255) );
		Tcl_DStringAppendElement( &ds, buf );
	}

	if( ts.ts_mater.ma_temperature > 0 )  {
		Tcl_DStringAppendElement( &ds, "temp" );
		sprintf( buf, "%g", ts.ts_mater.ma_temperature );
		Tcl_DStringAppendElement( &ds, buf );
	}

	if( ts.ts_mater.ma_shader && strlen( ts.ts_mater.ma_shader ) > 0 )  {
		Tcl_DStringAppendElement( &ds, "shader" );
		Tcl_DStringAppendElement( &ds, ts.ts_mater.ma_shader );
	}

#if 0
	Tcl_DStringAppendElement( &ds, "material" );
	Tcl_DStringAppendElement( &ds, bu_vls_addr(&comb->material) );
#endif
	if( ts.ts_mater.ma_minherit )
		Tcl_DStringAppendElement( &ds, "inherit" );

	if ( !bn_mat_is_identity( ts.ts_mat ) )  {
		struct bu_vls	str;
		Tcl_DStringAppendElement( &ds, "mat" );
		bu_vls_init(&str);
		bn_encode_mat( &str, ts.ts_mat );
		Tcl_DStringAppendElement( &ds, bu_vls_addr(&str) );
		bu_vls_free(&str);
	}

	db_free_db_tree_state( &ts );

	Tcl_DStringResult( interp, &ds );
	Tcl_DStringFree( &ds );

	return TCL_OK;
}

/*
 *  XXX This interface is deprecated.  Used "wdb_open" instead.
 */
int
rt_tcl_db_open( clientData, interp, argc, argv )
ClientData	clientData;
Tcl_Interp	*interp;
int		argc;
char		**argv;
{
  char buf[128];
  struct db_i *dbip = DBI_NULL;

  if(argc != 2)
    return TCL_ERROR;

  if( ((dbip = db_open( argv[1], "r+w" )) == DBI_NULL ) &&
      ((dbip = db_open( argv[1], "r"   )) == DBI_NULL ) )  {
    return TCL_ERROR;
  }

  /* --- Scan geometry database and build in-memory directory --- */
  db_scan( dbip, (int (*)())db_diradd, 1);

  sprintf( buf, "%ld", (long)(*((void **)&dbip)) );
  Tcl_AppendResult( interp, buf, (char *)NULL );

  return TCL_OK;
}

/*
 *			R T _ T C L _ S E T U P
 *
 *  Add all the supported Tcl interfaces to LIBRT routines to
 *  the list of commands known by the given interpreter.
 */
void
rt_tcl_setup(interp)
Tcl_Interp *interp;
{
	(void)Tcl_CreateCommand(interp, "wdb_open", wdb_open,
		(ClientData)NULL, (Tcl_CmdDeleteProc *)NULL);
	(void)Tcl_CreateCommand(interp, "rt_db_open", rt_tcl_db_open,
		(ClientData)NULL, (Tcl_CmdDeleteProc *)NULL);
	(void)Tcl_CreateCommand(interp, "db_follow_path", db_tcl_follow_path,
		(ClientData)NULL, (Tcl_CmdDeleteProc *)NULL);
	Tcl_SetVar(interp, "rt_version", (char *)rt_version+5, TCL_GLOBAL_ONLY);
}
