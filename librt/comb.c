/*
 *			C O M B . C
 *
 * XXX Move to db_tree.c when complete.
 *
 *  Authors -
 *	John R. Anderson
 *	Michael John Muuss
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
 *	This software is Copyright (C) 1996 by the United States Army
 *	in all countries except the USA.  All rights reserved.
 */
#ifndef lint
static char RCSid[] = "@(#)$Header$ (ARL)";
#endif


#include <stdio.h>
#include <math.h>
#include <string.h>
#include "machine.h"
#include "db.h"
#include "externs.h"
#include "vmath.h"
#include "nmg.h"
#include "rtgeom.h"
#include "raytrace.h"
#include "/m/cad/librt/debug.h"

/*
 *  In-memory format for combination.
 *  (Regions and Groups are both a kind of Combination).
 *  Move to h/wdb.h
 */
struct rt_comb_internal  {
	long		magic;
	union tree	*tree;		/* Leading to tree_db_leaf leaves */
	char		region_flag;	/* !0 ==> this COMB is a REGION */
	/* Begin GIFT compatability */
	short		region_id;
	short		aircode;
	short		GIFTmater;
	short		los;
	/* End GIFT compatability */
	char		rgb_valid;	/* !0 ==> rgb[] has valid color */
	unsigned char	rgb[3];
	struct rt_vls	shader_name;
	struct rt_vls	shader_param;
	struct rt_vls	material;
	char		inherit;
};
#define RT_COMB_MAGIC	0x436f6d49	/* "ComI" */
#define RT_CK_COMB(_p)		NMG_CKMAG( _p , RT_COMB_MAGIC , "rt_comb_internal" )


/* Internal. Ripped off from db_tree.c */
struct tree_list {
	union tree *tl_tree;
	int	tl_op;
};
#define TREE_LIST_NULL	((struct tree_list *)0)

RT_EXTERN( union tree *db_mkbool_tree , (struct tree_list *tree_list , int howfar ) );
RT_EXTERN( union tree *db_mkgift_tree , (struct tree_list *tree_list , int howfar , struct db_tree_state *tsp ) );

/*
 *		R T _ C O M B _ V 4 _ I M P O R T
 *
 *  Import a combination record from a V4 database into internal form.
 */
int
rt_comb_v4_import( ip, ep, matrix, tol )
struct rt_db_internal		*ip;
CONST struct rt_external	*ep;
CONST matp_t			matrix;		/* NULL if identity */
CONST struct rt_tol		*tol;
{
	union record		*rp;
	struct tree_list	*tree_list;
	union tree		*tree;
	struct rt_comb_internal	*comb;
	int			i,j;
	int			node_count;

	RT_CK_TOL( tol );
	RT_CK_EXTERNAL( ep );
	rp = (union record *)ep->ext_buf;

	if( rp[0].u_id != ID_COMB )
	{
		rt_log( "rt_comb_v4_import: Attempt to import a non-combination\n" );
		return( -1 );
	}

	/* Compute how many granules of MEMBER records follow */
	node_count = ep->ext_nbytes/sizeof( union record ) - 1;

	if( node_count )
		tree_list = (struct tree_list *)rt_calloc( node_count , sizeof( struct tree_list ) , "tree_list" );
	else
		tree_list = (struct tree_list *)NULL;

	for( j=0 ; j<node_count ; j++ )
	{
		if( rp[j+1].u_id != ID_MEMB )
		{
			rt_free( (char *)tree_list , "rt_comb_v4_import: tree_list" );
			rt_log( "rt_comb_v4_import(): granule in external buffer is not ID_MEMB, id=%d\n", rp[j+1].u_id );
			return( -1 );
		}

		switch( rp[j+1].M.m_relation )
		{
			case '+':
				tree_list[j].tl_op = OP_INTERSECT;
				break;
			case '-':
				tree_list[j].tl_op = OP_SUBTRACT;
				break;
			default:
				rt_log("rt_comb_v4_import() unknown op=x%x, assuming UNION\n", rp[j+1].M.m_relation );
				/* Fall through */
			case 'u':
				tree_list[j].tl_op = OP_UNION;
				break;
		}
		/* Build leaf node for in-memory tree */
		{
			union tree		*tp;
			struct tree_db_leaf	*mi;
			mat_t			diskmat;
			char			namebuf[NAMESIZE+2];

			GETUNION( tp, tree );
			tree_list[j].tl_tree = tp;
			tp->tr_l.magic = RT_TREE_MAGIC;
			tp->tr_l.tl_op = OP_DB_LEAF;
			strncpy( namebuf, rp[j+1].M.m_instname, NAMESIZE );
			namebuf[NAMESIZE] = '\0';	/* ensure null term */
			tp->tr_l.tl_name = rt_strdup( namebuf );

			/* See if disk record is identity matrix */
			rt_mat_dbmat( diskmat, rp[j+1].M.m_mat );
			/* XXX There should be rt_mat_is_identity() */
			/* XXX Could be used in librt/tree.c as well */
			if( rt_mat_is_equal( diskmat, rt_identity, tol ) )  {
				if( matrix == NULL )  {
					tp->tr_l.tl_mat = NULL;	/* identity */
				} else {
					tp->tr_l.tl_mat = mat_dup( matrix );
				}
			} else {
				if( matrix == NULL )  {
					tp->tr_l.tl_mat = mat_dup( diskmat );
				} else {
					mat_t	prod;
					mat_mul( prod, matrix, diskmat );
					tp->tr_l.tl_mat = mat_dup( prod );
				}
			}
rt_log("M_name=%s, matp=x%x\n", tp->tr_l.tl_name, tp->tr_l.tl_mat );
		}
	}
	if( node_count )
		tree = db_mkgift_tree( tree_list, node_count, (struct db_tree_state *)NULL );
	else
		tree = (union tree *)NULL;

	RT_INIT_DB_INTERNAL( ip );
	ip->idb_type = ID_COMBINATION;
	comb = (struct rt_comb_internal *)rt_malloc( sizeof( struct rt_comb_internal ) , "rt_comb_v4_import: rt_comb_internal" );
	ip->idb_ptr = (genptr_t)comb;
	comb->magic = RT_COMB_MAGIC;
	rt_vls_init( &comb->shader_name );
	rt_vls_init( &comb->shader_param );
	rt_vls_init( &comb->material );
	comb->tree = tree;
	comb->region_flag = (rp[0].c.c_flags == 'R');

	comb->region_id = rp[0].c.c_regionid;
	comb->aircode = rp[0].c.c_aircode;
	comb->GIFTmater = rp[0].c.c_material;
	comb->los = rp[0].c.c_los;

	comb->rgb_valid = rp[0].c.c_override;
	comb->rgb[0] = rp[0].c.c_rgb[0];
	comb->rgb[1] = rp[0].c.c_rgb[1];
	comb->rgb[2] = rp[0].c.c_rgb[2];
	rt_vls_strncpy( &comb->shader_name , rp[0].c.c_matname, 32 );
	rt_vls_strncpy( &comb->shader_param , rp[0].c.c_matparm, 60 );
	comb->inherit = rp[0].c.c_inherit;
	/* Automatic material table lookup here? */
	rt_vls_printf( &comb->material, "gift%d", comb->GIFTmater );

	return( 0 );
}

/*
 *			R T _ C O M B _ I F R E E
 *
 *  Free the storage associated with the rt_db_internal version of this combination.
 */
void
rt_comb_ifree( ip )
struct rt_db_internal	*ip;
{
	register struct rt_comb_internal	*comb;

	RT_CK_DB_INTERNAL(ip);
	comb = (struct rt_comb_internal *)ip->idb_ptr;
	RT_CK_COMB(comb);

	db_free_tree( comb->tree );
	comb->tree = NULL;

	rt_vls_free( &comb->shader_name );
	rt_vls_free( &comb->shader_param );
	rt_vls_free( &comb->material );

	comb->magic = 0;			/* sanity */
	rt_free( (char *)comb, "comb ifree" );
	ip->idb_ptr = GENPTR_NULL;	/* sanity */
}

/* -------------------- */
/* Some export support routines */

/*
 *			D B _ C K _ L E F T _ H E A V Y _ T R E E
 *
 *  Support routine for db_ck_v4gift_tree().
 *  Ensure that the tree below 'tp' is left-heavy, i.e. that there are
 *  nothing but solids on the right side of any binary operations.
 *
 *  Returns -
 *	-1	ERROR
 *	 0	OK
 */
int
db_ck_left_heavy_tree( tp, no_unions )
CONST union tree	*tp;
int			no_unions;
{
	RT_CK_TREE(tp);
	switch( tp->tr_op )  {

	case OP_DB_LEAF:
		break;

	case OP_UNION:
		if( no_unions )  return -1;
		/* else fall through */
	case OP_INTERSECT:
	case OP_SUBTRACT:
	case OP_XOR:
		if( tp->tr_b.tb_right->tr_op != OP_DB_LEAF )
			return -1;
		return db_ck_left_heavy_tree( tp->tr_b.tb_left, no_unions );

	default:
		rt_log("db_ck_left_heavy_tree: bad op %d\n", tp->tr_op);
		rt_bomb("db_ck_left_heavy_tree\n");
	}
	return 0;
}


/*
 *			D B _ C K _ V 4 G I F T _ T R E E
 *
 *  Look a gift-tree in the mouth.
 *  Ensure that this boolean tree conforms to the GIFT convention that
 *  union operations must bind the loosest.
 *  There are two stages to this check:
 *  1)  Ensure that if unions are present they are all at the root of tree,
 *  2)  Ensure non-union children of union nodes are all left-heavy
 *      (nothing but solid nodes permitted on rhs of binary operators).
 *
 *  Returns -
 *	-1	ERROR
 *	 0	OK
 */
int
db_ck_v4gift_tree( tp )
CONST union tree	*tp;
{
	RT_CK_TREE(tp);
	switch( tp->tr_op )  {

	case OP_DB_LEAF:
		break;

	case OP_UNION:
		if( db_ck_v4gift_tree( tp->tr_b.tb_left ) < 0 )
			return -1;
		return db_ck_v4gift_tree( tp->tr_b.tb_right );

	case OP_INTERSECT:
	case OP_SUBTRACT:
	case OP_XOR:
		return db_ck_left_heavy_tree( tp, 1 );

	default:
		rt_log("db_ck_v4gift_tree: bad op %d\n", tp->tr_op);
		rt_bomb("db_ck_v4gift_tree\n");
	}
	return 0;
}


/* --- Begin John's pretty-printer --- */

char
*Recurse_tree( tree )
union tree *tree;
{
	char *left,*right;
	char *return_str;
	char op;
	int return_length;

	if( tree == NULL )
		return( (char *)NULL );
	if( tree->tr_op == OP_UNION || tree->tr_op == OP_SUBTRACT || tree->tr_op == OP_INTERSECT )
	{
		left = Recurse_tree( tree->tr_b.tb_left );
		right = Recurse_tree( tree->tr_b.tb_right );
		switch( tree->tr_op )
		{
			case OP_UNION:
				op = 'u';
				break;
			case OP_SUBTRACT:
				op = '-';
				break;
			case OP_INTERSECT:
				op = '+';
				break;
		}
		return_length = strlen( left ) + strlen( right ) + 4;
		if( op == 'u' )
			return_length += 4;
		return_str = (char *)rt_malloc( return_length , "recurse_tree: return string" );
		if( op == 'u' )
		{
			char *blankl,*blankr;

			blankl = strchr( left , ' ' );
			blankr = strchr( right , ' ' );
			if( blankl && blankr )
				sprintf( return_str , "(%s) %c (%s)" , left , op , right );
			else if( blankl && !blankr )
				sprintf( return_str , "(%s) %c %s" , left , op , right );
			else if( !blankl && blankr )
				sprintf( return_str , "%s %c (%s)" , left , op , right );
			else
				sprintf( return_str , "%s %c %s" , left , op , right );
		}
		else
			sprintf( return_str , "%s %c %s" , left , op , right );
		if( tree->tr_b.tb_left->tr_op != OP_DB_LEAF )
			rt_free( left , "Recurse_tree: left string" );
		if( tree->tr_b.tb_right->tr_op != OP_DB_LEAF )
			rt_free( right , "Recurse_tree: right string" );
		return( return_str );
	}
	else if( tree->tr_op == OP_DB_LEAF ) {
		return tree->tr_l.tl_name ;
	}
}

void
Print_tree( tree )
union tree *tree;
{
	char *str;

	str = Recurse_tree( tree );
	if( str != NULL )
	{
		printf( "%s\n" , str );
		rt_free( str , "Print_tree" );
	}
	else
		printf( "NULL Tree\n" );
}

main( argc , argv )
int argc;
char *argv[];
{
	struct db_i		*dbip;
	struct directory	*dp;
	struct rt_external	ep;
	struct rt_db_internal	ip;
	struct rt_comb_internal	*comb;
	mat_t			identity_mat;
	struct rt_tol		tol;
	int			i;

	if( argc < 3 )
	{
		fprintf( stderr , "Usage:\n\t%s db_file object1 object2 ...\n" , argv[0] );
		exit( 1 );
	}

	tol.magic = RT_TOL_MAGIC;
	tol.dist = 0.005;
	tol.dist_sq = tol.dist * tol.dist;
	tol.perp = 1e-6;
	tol.para = 1 - tol.perp;

	mat_idn( identity_mat );

	if( (dbip = db_open( argv[1] , "r" ) ) == NULL )
	{
		fprintf( stderr , "Cannot open %s\n" , argv[1] );
		perror( "test" );
		exit( 1 );
	}

	/* Scan the database */
	db_scan(dbip, (int (*)())db_diradd, 1);

	for( i=2 ; i<argc ; i++ )
	{
		int j;

		printf( "%s\n" , argv[i] );

		dp = db_lookup( dbip , argv[i] , 1 );
		if( db_get_external( &ep , dp , dbip ) )
		{
			rt_log( "db_get_external failed for %s\n" , argv[i] );
			continue;
		}
		if( rt_comb_v4_import( &ip , &ep , NULL, &tol ) < 0 )  {
			rt_log("import of %s failed\n", dp->d_namep);
			continue;
		}

		RT_CK_DB_INTERNAL( &ip );

		if( ip.idb_type != ID_COMBINATION )
		{
			rt_log( "idb_type = %d\n" , ip.idb_type );
			continue;
		}

		comb = (struct rt_comb_internal *)ip.idb_ptr;
		RT_CK_COMB(comb);
		if( comb->region_flag )
		{
			rt_log( "\tRegion id = %d, aircode = %d GIFTmater = %d, los = %d\n",
				comb->region_id, comb->aircode, comb->GIFTmater, comb->los );
		}
		rt_log( "\trgb_valid = %d, color = %d/%d/%d\n" , comb->rgb_valid , V3ARGS( comb->rgb ) );
		rt_log( "\tmaterial name = %s, parameters = %s, (%s)\n" ,
				rt_vls_addr( &comb->shader_name ),
				rt_vls_addr( &comb->shader_param ),
				rt_vls_addr( &comb->material )
		);

		/* John's way */
		Print_tree( comb->tree );

		/* Standard way */
		rt_pr_tree( comb->tree, 1 );

		/* Compact way */
		{
			struct rt_vls	str;
			rt_vls_init( &str );
			rt_pr_tree_vls( &str, comb->tree );
			rt_log("%s\n", rt_vls_addr(&str) );
			rt_vls_free(&str );
		}

		/* Test the support routines */
		if( db_ck_v4gift_tree( comb->tree ) < 0 )
			rt_log("ERROR: db_ck_v4gift_tree is unhappy\n");

		/* Test the lumberjacks */
		rt_comb_ifree( &ip );
	}
}
