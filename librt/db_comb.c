/*
 *			D B _ C O M B . C
 *
 *  This module contains the import/export routines for "Combinations",
 *  the non-leaf nodes in the directed acyclic graphs (DAGs) in the
 *  BRL-CAD ".g" database.
 *
 *  This parallels the function of the geometry (leaf-node) import/export
 *  routines found in the g_xxx.c routines.
 *
 *  As a reminder, some combinations are special, when marked with
 *  the "Region" flag, everything from that node down is considered to
 *  be made of uniform material.
 *
 *  Authors -
 *	Michael John Muuss
 *	John R. Anderson
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

#include "conf.h"

#include <stdio.h>
#include <math.h>
#ifdef USE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif

#include "machine.h"
#include "bu.h"
#include "vmath.h"
#include "bn.h"
#include "db.h"
#include "raytrace.h"

#include "./debug.h"

struct tree_list {
	union tree *tl_tree;
	int	tl_op;
};
#define TREE_LIST_NULL	((struct tree_list *)0)

RT_EXTERN( union tree *db_mkbool_tree , (struct tree_list *tree_list , int howfar ) );
RT_EXTERN( union tree *db_mkgift_tree , (struct tree_list *tree_list , int howfar , struct db_tree_state *tsp ) );

/*
 *			D B _ T R E E _ N L E A V E S
 *
 *  Return count of number of leaf nodes in this tree.
 */
int
db_tree_nleaves( tp )
CONST union tree	*tp;
{

	RT_CK_TREE(tp);

	switch( tp->tr_op )  {
	case OP_NOP:
		return 0;
	case OP_DB_LEAF:
		return 1;
	case OP_SOLID:
		return 1;
	case OP_REGION:
		return 1;

	case OP_NOT:
	case OP_GUARD:
	case OP_XNOP:
		/* Unary ops */
		return db_tree_nleaves( tp->tr_b.tb_left );

	case OP_UNION:
	case OP_INTERSECT:
	case OP_SUBTRACT:
	case OP_XOR:
		/* This node is known to be a binary op */
		return	db_tree_nleaves( tp->tr_b.tb_left ) +
			db_tree_nleaves( tp->tr_b.tb_right );

	default:
		bu_log("db_tree_nleaves: bad op %d\n", tp->tr_op);
		rt_bomb("db_tree_nleaves\n");
	}
}

/*
 *			D B _ F L A T T E N _ T R E E
 *
 *  Take a binary tree in "V4-ready" layout (non-unions pushed below unions,
 *  left-heavy), and flatten it into an array layout, ready for conversion
 *  back to the GIFT-inspired V4 database format.
 */
struct tree_list *
db_flatten_tree( tree_list, tp, op )
struct tree_list	*tree_list;
union tree		*tp;
int			op;
{

	RT_CK_TREE(tp);

	switch( tp->tr_op )  {
	case OP_DB_LEAF:
		tree_list->tl_op = op;
		tree_list->tl_tree = tp;
		return tree_list+1;

	case OP_UNION:
	case OP_INTERSECT:
	case OP_SUBTRACT:
		/* This node is known to be a binary op */
		tree_list = db_flatten_tree( tree_list, tp->tr_b.tb_left, op );
		tree_list = db_flatten_tree( tree_list, tp->tr_b.tb_right, tp->tr_op );
		return tree_list;

	default:
		bu_log("db_flatten_tree: bad op %d\n", tp->tr_op);
		rt_bomb("db_flatten_tree\n");
	}
}

/*
 *			R T _ C O M B _ V 4 _ I M P O R T
 *
 *  Import a combination record from a V4 database into internal form.
 */
int
rt_comb_v4_import( ip, ep, matrix )
struct rt_db_internal		*ip;
CONST struct bu_external	*ep;
CONST matp_t			matrix;		/* NULL if identity */
{
	union record		*rp;
	struct tree_list	*tree_list;
	union tree		*tree;
	struct rt_comb_internal	*comb;
	int			i,j;
	int			node_count;

	BU_CK_EXTERNAL( ep );
	rp = (union record *)ep->ext_buf;

	if( rp[0].u_id != ID_COMB )
	{
		bu_log( "rt_comb_v4_import: Attempt to import a non-combination\n" );
		return( -1 );
	}

	/* Compute how many granules of MEMBER records follow */
	node_count = ep->ext_nbytes/sizeof( union record ) - 1;

	if( node_count )
		tree_list = (struct tree_list *)bu_calloc( node_count , sizeof( struct tree_list ) , "tree_list" );
	else
		tree_list = (struct tree_list *)NULL;

	for( j=0 ; j<node_count ; j++ )
	{
		if( rp[j+1].u_id != ID_MEMB )
		{
			bu_free( (genptr_t)tree_list , "rt_comb_v4_import: tree_list" );
			bu_log( "rt_comb_v4_import(): granule in external buffer is not ID_MEMB, id=%d\n", rp[j+1].u_id );
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
				bu_log("rt_comb_v4_import() unknown op=x%x, assuming UNION\n", rp[j+1].M.m_relation );
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

			BU_GETUNION( tp, tree );
			tree_list[j].tl_tree = tp;
			tp->tr_l.magic = RT_TREE_MAGIC;
			tp->tr_l.tl_op = OP_DB_LEAF;
			strncpy( namebuf, rp[j+1].M.m_instname, NAMESIZE );
			namebuf[NAMESIZE] = '\0';	/* ensure null term */
			tp->tr_l.tl_name = bu_strdup( namebuf );

			/* See if disk record is identity matrix */
			rt_mat_dbmat( diskmat, rp[j+1].M.m_mat );
			if( mat_is_identity( diskmat ) )  {
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
/* bu_log("M_name=%s, matp=x%x\n", tp->tr_l.tl_name, tp->tr_l.tl_mat ); */
		}
	}
	if( node_count )
		tree = db_mkgift_tree( tree_list, node_count, (struct db_tree_state *)NULL );
	else
		tree = (union tree *)NULL;

	RT_INIT_DB_INTERNAL( ip );
	ip->idb_type = ID_COMBINATION;
	comb = (struct rt_comb_internal *)bu_malloc( sizeof( struct rt_comb_internal ) , "rt_comb_v4_import: rt_comb_internal" );
	ip->idb_ptr = (genptr_t)comb;
	comb->magic = RT_COMB_MAGIC;
	bu_vls_init( &comb->shader_name );
	bu_vls_init( &comb->shader_param );
	bu_vls_init( &comb->material );
	comb->tree = tree;
	comb->region_flag = (rp[0].c.c_flags == 'R');

	if( comb->region_flag )  {
		comb->region_id = rp[0].c.c_regionid;
		comb->aircode = rp[0].c.c_aircode;
		comb->GIFTmater = rp[0].c.c_material;
		comb->los = rp[0].c.c_los;
	}

	if( comb->rgb_valid = rp[0].c.c_override )  {
		comb->rgb[0] = rp[0].c.c_rgb[0];
		comb->rgb[1] = rp[0].c.c_rgb[1];
		comb->rgb[2] = rp[0].c.c_rgb[2];
	}
	bu_vls_strncpy( &comb->shader_name , rp[0].c.c_matname, 32 );
	bu_vls_strncpy( &comb->shader_param , rp[0].c.c_matparm, 60 );
	/* XXX Separate flags for color inherit, shader inherit, (new) material inherit? */
	/* XXX cf: ma_cinherit, ma_minherit */
	comb->inherit = rp[0].c.c_inherit;
	/* Automatic material table lookup here? */
	bu_vls_printf( &comb->material, "gift%d", comb->GIFTmater );

	if( tree_list )  bu_free( (genptr_t)tree_list, "tree_list" );

	return( 0 );
}

/*
 *			R T _ C O M B _ V 4 _ E X P O R T
 */
int
rt_comb_v4_export( ep, ip, local2mm )
struct bu_external		*ep;
CONST struct rt_db_internal	*ip;
double				local2mm;
{
	struct rt_comb_internal	*comb;
	int			node_count;
	int			actual_count;
	struct tree_list	*tree_list;
	union tree		*tp;
	union record		*rp;
	int			j;

	RT_CK_DB_INTERNAL( ip );
	if( ip->idb_type != ID_COMBINATION ) bu_bomb("rt_comb_v4_export() type not ID_COMBINATION");
	comb = (struct rt_comb_internal *)ip->idb_ptr;
	RT_CK_COMB(comb);

	if( db_ck_v4gift_tree( comb->tree ) < 0 )  {
		db_non_union_push( comb->tree );
		if( db_ck_v4gift_tree( comb->tree ) < 0 )  {
			/* Need to further modify tree */
			bu_log("rt_comb_v4_export() Unfinished: need to V4-ify tree\n");
			rt_pr_tree( comb->tree, 0 );
			return -1;
		}
	}

	/* Count # leaves in tree -- that's how many Member records needed. */
	node_count = db_tree_nleaves( comb->tree );
	if( node_count )
		tree_list = (struct tree_list *)bu_calloc( node_count , sizeof( struct tree_list ) , "tree_list" );
	else
		tree_list = (struct tree_list *)NULL;

	/* Convert tree into array form */
	actual_count = db_flatten_tree( tree_list, comb->tree, OP_UNION ) - tree_list;
	if( actual_count > node_count )  bu_bomb("rt_comb_v4_export() array overflow!");
	if( actual_count < node_count )  bu_log("WARNING rt_comb_v4_export() array underflow! %d < %d", actual_count, node_count);

	/* Reformat the data into the necessary V4 granules */
	RT_INIT_EXTERNAL(ep);
	ep->ext_nbytes = sizeof(union record) * ( 1 + node_count );
	ep->ext_buf = bu_calloc( 1, ep->ext_nbytes, "v4 comb external" );
	rp = (union record *)ep->ext_buf;

	/* Convert the member records */
	for( j = 0; j < node_count; j++ )  {
		tp = tree_list[j].tl_tree;
		RT_CK_TREE(tp);
		if( tp->tr_op != OP_DB_LEAF )  bu_bomb("rt_comb_v4_export() tree not OP_DB_LEAF");

		rp[j+1].u_id = ID_MEMB;
		switch( tree_list[j].tl_op )  {
		case OP_INTERSECT:
			rp[j+1].M.m_relation = '+';
			break;
		case OP_SUBTRACT:
			rp[j+1].M.m_relation = '-';
			break;
		case OP_UNION:
			rp[j+1].M.m_relation = 'u';
			break;
		default:
			bu_bomb("rt_comb_v4_export() corrupt tree_list");
		}
		strncpy( rp[j+1].M.m_instname, tp->tr_l.tl_name, NAMESIZE );
		if( tp->tr_l.tl_mat )  {
			rt_dbmat_mat( rp[j+1].M.m_mat, tp->tr_l.tl_mat );
		} else {
			rt_dbmat_mat( rp[j+1].M.m_mat, mat_identity );
		}
	}

	/* Build the Combination record, on the front */
	rp[0].u_id = ID_COMB;
	/* c_name[] filled in by db_wrap_v4_external() */
	if( comb->region_flag )  {
		rp[0].c.c_flags = 'R';
		rp[0].c.c_regionid = comb->region_id;
		rp[0].c.c_aircode = comb->aircode;
		rp[0].c.c_material = comb->GIFTmater;
		rp[0].c.c_los = comb->los;
	} else {
		rp[0].c.c_flags = ' ';
	}
	if( comb->rgb_valid )  {
		rp[0].c.c_override = 1;
		rp[0].c.c_rgb[0] = comb->rgb[0];
		rp[0].c.c_rgb[1] = comb->rgb[1];
		rp[0].c.c_rgb[2] = comb->rgb[2];
	}
	strncpy( rp[0].c.c_matname, bu_vls_addr(&comb->shader_name), 32 );
	strncpy( rp[0].c.c_matparm, bu_vls_addr(&comb->shader_param), 60 );
	rp[0].c.c_inherit = comb->inherit;

	return 0;		/* OK */
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

	/* If tree hasn't been stolen, release it */
	if(comb->tree) db_free_tree( comb->tree );
	comb->tree = NULL;

	bu_vls_free( &comb->shader_name );
	bu_vls_free( &comb->shader_param );
	bu_vls_free( &comb->material );

	comb->magic = 0;			/* sanity */
	bu_free( (genptr_t)comb, "comb ifree" );
	ip->idb_ptr = GENPTR_NULL;	/* sanity */
}

/*
 *			R T _ G E T _ C O M B
 *
 *  A convenience wrapper to retrive a combination from the database
 *  into internal form.
 *
 *  Returns -
 *	-1	FAIL
 *	 0	OK
 */
int
rt_get_comb( ip, dp, matp, dbip )
struct rt_db_internal	*ip;
CONST struct directory	*dp;
CONST mat_t		*matp;
CONST struct db_i	*dbip;
{
	struct bu_external	ext;

	RT_CK_DIR(dp);
	RT_CHECK_DBI( dbip );

	/* Load the entire combination into contiguous memory */
	BU_INIT_EXTERNAL( &ext );
	if( db_get_external( &ext, dp, dbip ) < 0 )
		return -1;

	/* Switch out to right routine, based on database version */
	if( rt_comb_v4_import( ip, &ext, matp ) < 0 )
		return -1;

	db_free_external( &ext );
	return 0;
}

/*
 *			D B _ W R A P _ V 4 _ E X T E R N A L
 *
 *  Wraps the v4 object body in "ip" into a v4 wrapper in "op".
 *  db_free_external(ip) will be performed.
 *  op and ip must not point at the same bu_external structure.
 *
 *  As the v4 database does not really have the notion of "wrapping",
 *  this function primarily writes the object name into the
 *  proper place (a standard location in all granules),
 *  and (maybe) checks/sets the u_id field.
 */
int
db_wrap_v4_external( op, ip, dp )
struct bu_external	*op;
struct bu_external	*ip;
CONST struct directory	*dp;
{
	union record *rec;

	BU_CK_EXTERNAL(ip);
	RT_CK_DIR(dp);

	*op = *ip;		/* struct copy */
	ip->ext_buf = NULL;
	ip->ext_nbytes = 0;

	rec = (union record *)op->ext_buf;
	NAMEMOVE( dp->d_namep, rec->s.s_name );

	return 0;
}

/*
 *			R T _ V 4 _ E X P O R T
 *
 *  Soup-to-nuts v4 export, for combinations and geometry.
 *  (See also rt_db_put_internal() ).
 *  The next step after this is probably to call db_put_external().
 *  (which needs to be changed to not do the NAMEMOVE.)
 */
int
rt_v4_export( ep, ip, local2mm, dp )
struct bu_external		*ep;
CONST struct rt_db_internal	*ip;
double				local2mm;
CONST struct directory		*dp;
{
	struct bu_external	temp;
	int			ret;

	RT_CK_DB_INTERNAL(ip);
	RT_CK_DIR(dp);

	if( ip->idb_type == ID_COMBINATION )  {
		ret = rt_comb_v4_export( &temp, ip, local2mm );
	} else {
		ret = rt_functab[ip->idb_type].ft_export( &temp, ip, 1.0 );
	}
	if( ret < 0 )  {
		bu_log("rt_v4_export(%s): ft_export error %d\n",
			dp->d_namep, ret );
		return ret;
	}

	if( (ret = db_wrap_v4_external( ep, &temp, dp )) < 0 )  {
		bu_log("rt_v4_export(%s): db_wrap_v4_external error %d\n",
			dp->d_namep, ret );
		return ret;
	}
	/* "temp" has been freed by db_wrap_v4_external() */
	return 0;
}

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
		bu_log("db_ck_left_heavy_tree: bad op %d\n", tp->tr_op);
		bu_bomb("db_ck_left_heavy_tree\n");
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
		bu_log("db_ck_v4gift_tree: bad op %d\n", tp->tr_op);
		bu_bomb("db_ck_v4gift_tree\n");
	}
	return 0;
}


/*
 *			D B _ M K B O O L _ T R E E
 *
 *  Given a tree_list array, build a tree of "union tree" nodes
 *  appropriately connected together.  Every element of the
 *  tree_list array used is replaced with a TREE_NULL.
 *  Elements which are already TREE_NULL are ignored.
 *  Returns a pointer to the top of the tree.
 */
HIDDEN union tree *
db_mkbool_tree( tree_list, howfar )
struct tree_list *tree_list;
int		howfar;
{
	register struct tree_list *tlp;
	register int		i;
	register struct tree_list *first_tlp = (struct tree_list *)0;
	register union tree	*xtp;
	register union tree	*curtree;
	register int		inuse;

	if( howfar <= 0 )
		return(TREE_NULL);

	/* Count number of non-null sub-trees to do */
	for( i=howfar, inuse=0, tlp=tree_list; i>0; i--, tlp++ )  {
		if( tlp->tl_tree == TREE_NULL )
			continue;
		if( inuse++ == 0 )
			first_tlp = tlp;
	}

	/* Handle trivial cases */
	if( inuse <= 0 )
		return(TREE_NULL);
	if( inuse == 1 )  {
		curtree = first_tlp->tl_tree;
		first_tlp->tl_tree = TREE_NULL;
		return( curtree );
	}

	if( first_tlp->tl_op != OP_UNION )  {
		first_tlp->tl_op = OP_UNION;	/* Fix it */
		if( rt_g.debug & DEBUG_TREEWALK )  {
			bu_log("db_mkbool_tree() WARNING: non-union (%c) first operation ignored\n",
				first_tlp->tl_op );
		}
	}

	curtree = first_tlp->tl_tree;
	first_tlp->tl_tree = TREE_NULL;
	tlp=first_tlp+1;
	for( i=howfar-(tlp-tree_list); i>0; i--, tlp++ )  {
		if( tlp->tl_tree == TREE_NULL )
			continue;

		BU_GETUNION( xtp, tree );
		xtp->magic = RT_TREE_MAGIC;
		xtp->tr_b.tb_left = curtree;
		xtp->tr_b.tb_right = tlp->tl_tree;
		xtp->tr_b.tb_regionp = (struct region *)0;
		xtp->tr_op = tlp->tl_op;
		curtree = xtp;
		tlp->tl_tree = TREE_NULL;	/* empty the input slot */
	}
	return(curtree);
}

/*
 *			D B _ M K G I F T _ T R E E
 */
HIDDEN union tree *
db_mkgift_tree( trees, subtreecount, tsp )
struct tree_list	*trees;
int			subtreecount;
struct db_tree_state	*tsp;
{
	register struct tree_list *tstart;
	register struct tree_list *tnext;
	union tree		*curtree;
	int	i;
	int	j;

	/*
	 * This is how GIFT interpreted equations, so it is duplicated here.
	 * Any expressions between UNIONs are evaluated first.  For example:
	 *		A - B - C u D - E - F
	 * becomes	(A - B - C) u (D - E - F)
	 * so first do the parenthesised parts, and then go
	 * back and glue the unions together.
	 * As always, unions are the downfall of free enterprise!
	 */
	tstart = trees;
	tnext = trees+1;
	for( i=subtreecount-1; i>=0; i--, tnext++ )  {
		/* If we went off end, or hit a union, do it */
		if( i>0 && tnext->tl_op != OP_UNION )
			continue;
		if( (j = tnext-tstart) <= 0 )
			continue;
		curtree = db_mkbool_tree( tstart, j );
		/* db_mkbool_tree() has side effect of zapping tree array,
		 * so build new first node in array.
		 */
		tstart->tl_op = OP_UNION;
		tstart->tl_tree = curtree;

		if(rt_g.debug&DEBUG_TREEWALK)  {
			bu_log("db_mkgift_tree() intermediate term:\n");
			rt_pr_tree(tstart->tl_tree, 0);
		}

		/* tstart here at union */
		tstart = tnext;
	}

final:
	curtree = db_mkbool_tree( trees, subtreecount );
	if(rt_g.debug&DEBUG_TREEWALK)  {
		bu_log("db_mkgift_tree() returns:\n");
		rt_pr_tree(curtree, 0);
	}
	return( curtree );
}
