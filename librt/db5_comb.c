/*
 *			D B 5 _ C O M B . C
 *
 *  Handle import/export of combinations (union tree) in v5 format.
 *
 *  The on-disk record looks like this:
 *	width
 *	n matricies (only non-identity matricies stored).
 *	n leaves
 *	len of RPN expression.  (len=0 signals all-union expression)
 *
 *  Encoding of a matrix is (16 * SIZEOF_NETWORK_DOUBLE)
 *
 *  Author -
 *	Michael John Muuss
 *  
 *  Source -
 *	The U. S. Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5068  USA
 *  
 *  Distribution Status -
 *	Public Domain, Distribution Unlimited.
 */
#ifndef lint
static char RCSid[] = "@(#)$Header$ (ARL)";
#endif

#include "conf.h"

#include <stdio.h>
#ifdef USE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif

#include "machine.h"
#include "bu.h"
#include "vmath.h"
#include "bn.h"
#include "db5.h"
#include "raytrace.h"

#include "./debug.h"


/*
 *			D B _ T R E E _ C O U N T E R
 *
 *  Count number of non-identity matricies,
 *  number of leaf nodes, number of operator nodes.
 *
 *  Returns -
 *	1 if operators other than UNION were seen.
 *	0 if all operators seen were UNION.
 */
int
db_tree_counter( tp, noperp, nleafp, nmatp, nnamesize )
CONST union tree	*tp;
long			*noperp;
long			*nleafp;
long			*nmatp;
long			*nnamesize;
{
	if( tp == TREE_NULL )  return 0;
	RT_CK_TREE(tp);

	switch( tp->tr_op )  {
	case OP_DB_LEAF:
		(*nleafp)++;
		if( tp->tr_l.tl_mat )  (*nmatp)++;
		(*nnamesize) += strlen(tp->tr_l.tl_name) + 1 + SIZEOF_NETWORK_LONG;
		return 0;

	case OP_NOT:
		/* Unary ops */
		(*noperp)++;
		return 1 | db_tree_counter( tp->tr_b.tb_left, noperp, nleafp, nmatp, nnamesize );

	case OP_UNION:
		/* This node is known to be a binary op */
		(*noperp)++;
		return	db_tree_counter( tp->tr_b.tb_left, noperp, nleafp, nmatp, nnamesize ) |
			db_tree_counter( tp->tr_b.tb_right, noperp, nleafp, nmatp, nnamesize );

	case OP_INTERSECT:
	case OP_SUBTRACT:
	case OP_XOR:
		/* This node is known to be a binary op */
		(*noperp)++;
		return 1 | 
			db_tree_counter( tp->tr_b.tb_left, noperp, nleafp, nmatp, nnamesize ) |
			db_tree_counter( tp->tr_b.tb_right, noperp, nleafp, nmatp, nnamesize );

	default:
		bu_log("db_tree_counter: bad op %d\n", tp->tr_op);
		bu_bomb("db_tree_counter\n");
	}
}

/*
 *			R T _ C O M B _ V 5 _ S E R I A L I Z E _ L E A V E S
 */
void
rt_comb_v5_serialize_leaves(
	const union tree	*tp,
	long			*matnum,
	unsigned char		**matp,
	unsigned char		**leafp)
{
	int	n;

	if( tp == TREE_NULL )  return;
	RT_CK_TREE(tp);

	switch( tp->tr_op )  {
	case OP_DB_LEAF:
		/*
		 *  Encoding of the leaf:
		 *	A null-terminate name string,
		 *	the matrix subscript.  -1 == identity.
		 */
		n = strlen(tp->tr_l.tl_name) + 1;
		bcopy( tp->tr_l.tl_name, *leafp, n );
		(*leafp) += n;

		if( tp->tr_l.tl_mat )
			n = (*matnum)++;
		else
			n = -1;
		*leafp = bu_plong( *leafp, n );

		/* Encoding of the matrix */
		if( tp->tr_l.tl_mat )  {
			htond( (*matp),
				(const unsigned char *)tp->tr_l.tl_mat,
				16 );
			(*matp) += 16 * SIZEOF_NETWORK_DOUBLE;
		}
		return;

	case OP_NOT:
		/* Unary ops */
		rt_comb_v5_serialize_leaves( tp->tr_b.tb_left, matnum, matp, leafp );
		return;

	case OP_UNION:
	case OP_INTERSECT:
	case OP_SUBTRACT:
	case OP_XOR:
		/* This node is known to be a binary op */
		rt_comb_v5_serialize_leaves( tp->tr_b.tb_left, matnum, matp, leafp );
		rt_comb_v5_serialize_leaves( tp->tr_b.tb_right, matnum, matp, leafp );
		return;

	default:
		bu_log("rt_comb_v5_serialize_leaves: bad op %d\n", tp->tr_op);
		bu_bomb("rt_comb_v5_serialize_leaves\n");
	}
}

/*
 *			R T _ C O M B _ E X P O R T 5
 */
int
rt_comb_export5( ep, ip, local2mm, dbip )
struct bu_external		*ep;
CONST struct rt_db_internal	*ip;
double				local2mm;
CONST struct db_i		*dbip;
{
	struct rt_comb_internal	*comb;
	long	noper=0, nleaf=0, nmat=0, nnamesize=0;
	int	full_expression;	/* 0 = all UNION */
	long	need;
	int	rpn_len = 0;	/* # items in RPN expression */
	int	wid;
	unsigned char	*cp;
	unsigned char	*matp;
	unsigned char	*leafp;
	long	matnum = 0;

	RT_CK_DB_INTERNAL( ip );

	if( ip->idb_type != ID_COMBINATION ) bu_bomb("rt_comb_v4_export() type not ID_COMBINATION");
	comb = (struct rt_comb_internal *)ip->idb_ptr;
	RT_CK_COMB(comb);

	/* First pass -- count number of non-identity matricies,
	 * number of leaf nodes, number of operator nodes.
	 */
	full_expression = db_tree_counter( comb->tree, &noper,
		&nleaf, &nmat, &nnamesize );
bu_log("noper=%d, nleaf=%d, nmat=%d, nnamesize=%d\n", noper, nleaf, nmat, nnamesize);

	if( full_expression )  {
		/* XXX Calculate storage needs for RPN expression here */
		rpn_len = 0;
		bu_log("Only do groups so far, sorry\n");
		return -1;	/* FAIL */
	}

	wid = db5_select_length_encoding( nmat | nleaf | rpn_len );

	/* Second pass -- determine upper bound of on-disk memory needed */
	need =  1 +			/* width code */
		db5_enc_len[wid] + 	/* size for nmatricies */
		db5_enc_len[wid] +	/* size for nleaves */
		db5_enc_len[wid] +	/* size for len of RPN */
		nmat * (16 * SIZEOF_NETWORK_DOUBLE) +	/* sizeof matrix array */
		nnamesize;		/* size for leaf nodes */
	if( full_expression )  {
		need += rpn_len * db5_enc_len[wid];
	}

	BU_INIT_EXTERNAL(ep);
	ep->ext_nbytes = need;
	ep->ext_buf = bu_malloc( need, "rt_comb_export5" );

	/* Build combination's on-disk header section */
	cp = (unsigned char *)ep->ext_buf;
	*cp++ = wid;
	cp = db5_encode_length( cp, nmat, wid );
	cp = db5_encode_length( cp, nleaf, wid );
	cp = db5_encode_length( cp, rpn_len, wid );

	/*
	 *  Because matricies are fixed size, we can compute the pointer
	 *  to the start of the leaf node section now,
	 *  and output them both, together, with one tree walk
	 *  and no temporary storage.
	 */
	matp = cp;
	leafp = cp + nmat * (16 * SIZEOF_NETWORK_DOUBLE);

	rt_comb_v5_serialize_leaves( comb->tree, &matnum, &matp, &leafp );
bu_hexdump_external( stderr, ep, "v5comb" );

	BU_ASSERT_LONG( matnum, ==, nmat );
	BU_ASSERT_PTR( matp, ==, cp + nmat * (16 * SIZEOF_NETWORK_DOUBLE) );
	BU_ASSERT_PTR( leafp, <=, ((unsigned char *)ep->ext_buf) + ep->ext_nbytes );

	if( full_expression == 0 )  return 0;	/* OK */

	return -1;	/* FAIL */
}

/*
 */
int
rt_comb_import5(ip, ep, mat, dbip)
struct rt_db_internal	*ip;
CONST struct bu_external *ep;
CONST mat_t		mat;
CONST struct db_i	*dbip;
{
	return -1;
}
