/*
 *			D B 5 _ C O M B . C
 *
 *  Handle import/export of combinations (union tree) in v5 format.
 *
 *  The on-disk record looks like this:
 *	width byte
 *	n matricies (only non-identity matricies stored).
 *	n leaves
 *	len of RPN expression.  (len=0 signals all-union expression)
 *	depth of stack
 *	Section 1:  matricies
 *	Section 2:  leaves
 *	Section 3:  (Optional) RPN expression.
 *
 *  Encoding of a matrix is (ELEMENTS_PER_MAT * SIZEOF_NETWORK_DOUBLE) bytes,
 *  in network order (big-Endian, IEEE double precision).
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


struct db_tree_counter_state {
	long	magic;
	long	n_mat;			/* # leaves with non-identity matricies */
	long	n_leaf;			/* # leaf nodes */
	long	n_oper;			/* # operator nodes */
	long	namebytes;		/* # bytes for name section */
	int	non_union_seen;		/* boolean, 1 = non-unions seen */
};
#define DB_TREE_COUNTER_STATE_MAGIC	0x64546373	/* dTcs */
#define DB_CK_TREE_COUNTER_STATE(_p)	BU_CKMAG(_p, DB_TREE_COUNTER_STATE_MAGIC, "db_tree_counter_state");

/*
 *			D B _ T R E E _ C O U N T E R
 *
 *  Count number of non-identity matricies,
 *  number of leaf nodes, number of operator nodes, etc.
 *
 *  Returns -
 *	maximum depth of stack needed to unpack this tree, if serialized.
 */
long
db_tree_counter( CONST union tree *tp, struct db_tree_counter_state *tcsp )
{
	long	ldepth, rdepth;

	RT_CK_TREE(tp);
	DB_CK_TREE_COUNTER_STATE(tcsp);

	switch( tp->tr_op )  {
	case OP_DB_LEAF:
		tcsp->n_leaf++;
		if( tp->tr_l.tl_mat )  tcsp->n_mat++;
		tcsp->namebytes += strlen(tp->tr_l.tl_name) + 1 + SIZEOF_NETWORK_LONG;
		return 1;

	case OP_NOT:
		/* Unary ops */
		tcsp->n_oper++;
		tcsp->non_union_seen = 1;
		return 1 + db_tree_counter( tp->tr_b.tb_left, tcsp );

	case OP_UNION:
		/* This node is known to be a binary op */
		tcsp->n_oper++;
		ldepth = db_tree_counter( tp->tr_b.tb_left, tcsp );
		rdepth = db_tree_counter( tp->tr_b.tb_right, tcsp );
		if( ldepth > rdepth )  return ldepth;
		return rdepth;

	case OP_INTERSECT:
	case OP_SUBTRACT:
	case OP_XOR:
		/* This node is known to be a binary op */
		tcsp->n_oper++;
		tcsp->non_union_seen = 1;
		ldepth = db_tree_counter( tp->tr_b.tb_left, tcsp );
		rdepth = db_tree_counter( tp->tr_b.tb_right, tcsp );
		if( ldepth > rdepth )  return ldepth;
		return rdepth;

	default:
		bu_log("db_tree_counter: bad op %d\n", tp->tr_op);
		bu_bomb("db_tree_counter\n");
	}
}

#define DB5COMB_TOKEN_LEAF		1
#define DB5COMB_TOKEN_UNION		2
#define DB5COMB_TOKEN_INTERSECT		3
#define DB5COMB_TOKEN_SUBTRACT		4
#define DB5COMB_TOKEN_XOR		5
#define DB5COMB_TOKEN_NOT		6

struct rt_comb_v5_serialize_state  {
	long		magic;
	long		mat_num;
	unsigned char	*matp;
	unsigned char	*leafp;
	unsigned char	*exprp;
};
#define RT_COMB_V5_SERIALIZE_STATE_MAGIC	0x43357373	/* C5ss */
#define RT_CK_COMB_V5_SERIALIZE_STATE(_p)	BU_CKMAG(_p, RT_COMB_V5_SERIALIZE_STATE_MAGIC, "rt_comb_v5_serialize_state")

/*
 *			R T _ C O M B _ V 5 _ S E R I A L I Z E
 *
 *  In one single pass through the tree, serialize out all three
 *  output sections at once.
 */
void
rt_comb_v5_serialize(
	const union tree	*tp,
	struct rt_comb_v5_serialize_state	*ssp)
{
	int	n;

	RT_CK_TREE(tp);
	RT_CK_COMB_V5_SERIALIZE_STATE(ssp);

	switch( tp->tr_op )  {
	case OP_DB_LEAF:
		/*
		 *  Encoding of the leaf:
		 *	A null-terminated name string, and
		 *	the matrix subscript.  -1 == identity.
		 */
		n = strlen(tp->tr_l.tl_name) + 1;
		bcopy( tp->tr_l.tl_name, ssp->leafp, n );
		ssp->leafp += n;

		if( tp->tr_l.tl_mat )
			n = ssp->mat_num++;
		else
			n = -1;
		ssp->leafp = bu_plong( ssp->leafp, n );

		/* Encoding of the matrix */
		if( tp->tr_l.tl_mat )  {
			htond( ssp->matp,
				(const unsigned char *)tp->tr_l.tl_mat,
				ELEMENTS_PER_MAT );
			ssp->matp += ELEMENTS_PER_MAT * SIZEOF_NETWORK_DOUBLE;
		}

		/* Encoding of the "leaf" operator */
		if( ssp->exprp )
			*ssp->exprp++ = DB5COMB_TOKEN_LEAF;
		return;

	case OP_NOT:
		/* Unary ops */
		rt_comb_v5_serialize( tp->tr_b.tb_left, ssp );
		if( ssp->exprp )
			*ssp->exprp++ = DB5COMB_TOKEN_NOT;
		return;

	case OP_UNION:
		/* This node is known to be a binary op */
		rt_comb_v5_serialize( tp->tr_b.tb_left, ssp );
		rt_comb_v5_serialize( tp->tr_b.tb_right, ssp );
		if( ssp->exprp )
			*ssp->exprp++ = DB5COMB_TOKEN_UNION;
		return;
	case OP_INTERSECT:
		/* This node is known to be a binary op */
		rt_comb_v5_serialize( tp->tr_b.tb_left, ssp );
		rt_comb_v5_serialize( tp->tr_b.tb_right, ssp );
		if( ssp->exprp )
			*ssp->exprp++ = DB5COMB_TOKEN_INTERSECT;
		return;
	case OP_SUBTRACT:
		/* This node is known to be a binary op */
		rt_comb_v5_serialize( tp->tr_b.tb_left, ssp );
		rt_comb_v5_serialize( tp->tr_b.tb_right, ssp );
		if( ssp->exprp )
			*ssp->exprp++ = DB5COMB_TOKEN_SUBTRACT;
		return;
	case OP_XOR:
		/* This node is known to be a binary op */
		rt_comb_v5_serialize( tp->tr_b.tb_left, ssp );
		rt_comb_v5_serialize( tp->tr_b.tb_right, ssp );
		if( ssp->exprp )
			*ssp->exprp++ = DB5COMB_TOKEN_XOR;
		return;

	default:
		bu_log("rt_comb_v5_serialize: bad op %d\n", tp->tr_op);
		bu_bomb("rt_comb_v5_serialize\n");
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
	struct db_tree_counter_state		tcs;
	struct rt_comb_v5_serialize_state	ss;
	long	max_stack_depth;
	long	need;
	int	rpn_len = 0;	/* # items in RPN expression */
	int	wid;
	unsigned char	*cp;

	RT_CK_DB_INTERNAL( ip );

	if( ip->idb_type != ID_COMBINATION ) bu_bomb("rt_comb_v4_export() type not ID_COMBINATION");
	comb = (struct rt_comb_internal *)ip->idb_ptr;
	RT_CK_COMB(comb);

	/* First pass -- count number of non-identity matricies,
	 * number of leaf nodes, number of operator nodes.
	 */
	bzero( (char *)&tcs, sizeof(tcs) );
	tcs.magic = DB_TREE_COUNTER_STATE_MAGIC;
	max_stack_depth = db_tree_counter( comb->tree, &tcs );
bu_log("n_max=%d, n_leaf=%d, n_oper=%d, namebytes=%d, non_union_seen=%d, max_stack_depth=%d\n",
tcs.n_mat, tcs.n_leaf, tcs.n_oper, tcs.namebytes, tcs.non_union_seen, max_stack_depth);
rt_pr_tree( comb->tree, 1 );

	if( tcs.non_union_seen )  {
		/* RPN expression needs one byte for each leaf or operator node */
		rpn_len = tcs.n_leaf + tcs.n_oper;
		bu_log("Only do groups so far, sorry\n");
		return -1;	/* FAIL */
	} else {
		rpn_len = 0;
	}

	wid = db5_select_length_encoding(
		tcs.n_mat | tcs.n_leaf | tcs.namebytes |
		rpn_len | max_stack_depth );

	/* Second pass -- determine amount of on-disk storage needed */
	need =  1 +			/* width code */
		db5_enc_len[wid] + 	/* size for nmatricies */
		db5_enc_len[wid] +	/* size for nleaves */
		db5_enc_len[wid] +	/* size for namebytes */
		db5_enc_len[wid] +	/* size for len of RPN */
		db5_enc_len[wid] +	/* size for max_stack_depth */
		tcs.n_mat * (ELEMENTS_PER_MAT * SIZEOF_NETWORK_DOUBLE) +	/* sizeof matrix array */
		tcs.namebytes +		/* size for leaf nodes */
		rpn_len;		/* storage for RPN expression */

	BU_INIT_EXTERNAL(ep);
	ep->ext_nbytes = need;
	ep->ext_buf = bu_malloc( need, "rt_comb_export5 ext_buf" );

	/* Build combination's on-disk header section */
	cp = (unsigned char *)ep->ext_buf;
	*cp++ = wid;
	cp = db5_encode_length( cp, tcs.n_mat, wid );
	cp = db5_encode_length( cp, tcs.n_leaf, wid );
	cp = db5_encode_length( cp, tcs.namebytes, wid );
	cp = db5_encode_length( cp, rpn_len, wid );
	cp = db5_encode_length( cp, max_stack_depth, wid );

	/*
	 *  The output format has three sections:
	 *	Section 1:  matricies
	 *	Section 2:  leaf nodes
	 *	Section 3:  Optional RPN expression
	 *
	 *  We have pre-computed the exact size of all three sections,
	 *  so they can all be searialized together in one pass.
	 *  Establish pointers to the start of each section.
	 */
	ss.magic = RT_COMB_V5_SERIALIZE_STATE_MAGIC;
	ss.mat_num = 0;
	ss.matp = cp;
	ss.leafp = cp + tcs.n_mat * (ELEMENTS_PER_MAT * SIZEOF_NETWORK_DOUBLE);
	if( rpn_len )
		ss.exprp = ss.leafp + tcs.namebytes;
	else
		ss.exprp = NULL;

	rt_comb_v5_serialize( comb->tree, &ss );
bu_hexdump_external( stderr, ep, "v5comb" );

	BU_ASSERT_LONG( ss.mat_num, ==, tcs.n_mat );
	BU_ASSERT_PTR( ss.matp, ==, cp + tcs.n_mat * (ELEMENTS_PER_MAT * SIZEOF_NETWORK_DOUBLE) );
	BU_ASSERT_PTR( ss.leafp, <=, ((unsigned char *)ep->ext_buf) + ep->ext_nbytes );
	if( rpn_len )
		BU_ASSERT_PTR( ss.exprp, <=, ((unsigned char *)ep->ext_buf) + ep->ext_nbytes );

	/* How to encode all the other stuff as attributes??? */

	if( rpn_len == 0 )  return 0;	/* OK */

	return -1;	/* FAIL */
}

/*
 *			R T _ C O M B _ I M P O R T 5
 */
int
rt_comb_import5(ip, ep, mat, dbip)
struct rt_db_internal	*ip;
const struct bu_external *ep;
const mat_t		mat;
const struct db_i	*dbip;
{
	struct rt_comb_internal	*comb;
	unsigned char	*cp;
	int		wid;
	long		nmat, nleaf, rpn_len, max_stack_depth;
	long		namebytes;
	unsigned char	*matp;
	unsigned char	*leafp;
	unsigned char	*leafp_end;
	unsigned char	*exprp;
#define MAX_V5_STACK	100
	union tree	*stack[MAX_V5_STACK];
	union tree	**sp;			/* stack pointer */
	int		i;

	BU_CK_EXTERNAL(ep);

	RT_INIT_DB_INTERNAL( ip );
	ip->idb_type = ID_COMBINATION;
	ip->idb_meth = &rt_functab[ID_COMBINATION];
	BU_GETSTRUCT( comb, rt_comb_internal );
	ip->idb_ptr = (genptr_t)comb;
	comb->magic = RT_COMB_MAGIC;
	bu_vls_init( &comb->shader );
	bu_vls_init( &comb->material );
	comb->temperature = -1;

	cp = ep->ext_buf;
	wid = *cp++;
	cp += db5_decode_length( &nmat, cp, wid );
	cp += db5_decode_length( &nleaf, cp, wid );
	cp += db5_decode_length( &namebytes, cp, wid );
	cp += db5_decode_length( &rpn_len, cp, wid );
	cp += db5_decode_length( &max_stack_depth, cp, wid );
	matp = cp;
	leafp = cp + nmat * (ELEMENTS_PER_MAT * SIZEOF_NETWORK_DOUBLE);
	exprp = leafp + namebytes;
	leafp_end = exprp;
bu_log("nmat=%d, nleaf=%d, rpn_len=%d, max_stack_depth=%d\n", nmat, nleaf, rpn_len, max_stack_depth);

	if( rpn_len == 0 )  {
		int	i;
		for( i = nleaf-1; i >= 0; i-- )  {
			union tree	*tp;
			int		mi;

			BU_GETUNION( tp, tree );
			tp->tr_l.magic = RT_TREE_MAGIC;
			tp->tr_l.tl_op = OP_DB_LEAF;
			tp->tr_l.tl_name = bu_strdup( leafp );
			leafp += strlen(leafp) + 1;

			/* Get matrix index */
			mi = bu_glong( leafp );
			leafp += 4;

			if( mi < 0 )  {
				/* Signal identity matrix */
				tp->tr_l.tl_mat = NULL;
			} else {
				/* Unpack indicated matrix mi */
				BU_ASSERT_LONG( mi, <, nmat );
				tp->tr_l.tl_mat = (matp_t)bu_malloc(
					sizeof(mat_t), "v5comb mat");
				ntohd( (unsigned char *)tp->tr_l.tl_mat,
					&matp[mi*ELEMENTS_PER_MAT*SIZEOF_NETWORK_DOUBLE],
					ELEMENTS_PER_MAT);
			}

			if( comb->tree == TREE_NULL )  {
				comb->tree = tp;
			} else {
				union tree	*unionp;
				BU_GETUNION( unionp, tree );
				unionp->tr_b.magic = RT_TREE_MAGIC;
				unionp->tr_b.tb_op = OP_UNION;
				unionp->tr_b.tb_left = comb->tree;
				unionp->tr_b.tb_right = tp;
				comb->tree = unionp;
			}
		}
		db_ck_tree( comb->tree );
		rt_pr_tree( comb->tree, 1 );
		BU_ASSERT_PTR( leafp, ==, leafp_end );
		return 0;
	}

	/*
	 *  Bring the RPN expression back from the disk,
	 *  populating leaves and matricies in the order they are encountered.
	 */
	if( max_stack_depth > MAX_V5_STACK )  {
		bu_log("Combination needs stack depth %d, only have %d, aborted\n",
			max_stack_depth, MAX_V5_STACK);
		return -1;
	}
	sp = &stack[0];

	for( i=0; i < rpn_len; i++,exprp++ )  {
		union tree	*tp;
		int		mi;

		BU_GETUNION( tp, tree );
		tp->tr_b.magic = RT_TREE_MAGIC;

		switch( *exprp )  {
		case DB5COMB_TOKEN_LEAF:
			tp->tr_l.tl_op = OP_DB_LEAF;
			tp->tr_l.tl_name = bu_strdup( leafp );
			leafp += strlen(leafp) + 1;

			/* Get matrix index */
			mi = bu_glong( leafp );
			leafp += 4;

			if( mi < 0 )  {
				/* Signal identity matrix */
				tp->tr_l.tl_mat = NULL;
			} else {
				/* Unpack indicated matrix mi */
				BU_ASSERT_LONG( mi, <, nmat );
				tp->tr_l.tl_mat = (matp_t)bu_malloc(
					sizeof(mat_t), "v5comb mat");
				ntohd( (unsigned char *)tp->tr_l.tl_mat,
					&matp[mi*ELEMENTS_PER_MAT*SIZEOF_NETWORK_DOUBLE],
					ELEMENTS_PER_MAT);
			}
			break;

		case DB5COMB_TOKEN_UNION:
		case DB5COMB_TOKEN_INTERSECT:
		case DB5COMB_TOKEN_SUBTRACT:
		case DB5COMB_TOKEN_XOR:
			/* These are all binary operators */
			tp->tr_b.tb_regionp = REGION_NULL;
			tp->tr_b.tb_right = *--sp;
			RT_CK_TREE(tp->tr_b.tb_right);
			tp->tr_b.tb_left = *--sp;
			RT_CK_TREE(tp->tr_b.tb_left);
			switch( *exprp )  {
			case DB5COMB_TOKEN_UNION:
				tp->tr_b.tb_op = OP_UNION;
				break;
			case DB5COMB_TOKEN_INTERSECT:
				tp->tr_b.tb_op = OP_INTERSECT;
				break;
			case DB5COMB_TOKEN_SUBTRACT:
				tp->tr_b.tb_op = OP_SUBTRACT;
				break;
			case DB5COMB_TOKEN_XOR:
				tp->tr_b.tb_op = OP_XOR;
				break;
			}
			break;

		case DB5COMB_TOKEN_NOT:
			/* This is a unary operator */
			tp->tr_b.tb_regionp = REGION_NULL;
			tp->tr_b.tb_left = *--sp;
			RT_CK_TREE(tp->tr_b.tb_left);
			tp->tr_b.tb_right = TREE_NULL;
			tp->tr_b.tb_op = OP_NOT;
			break;
		default:
			bu_log("rt_comb_import5() unknown RPN expression token=%d, import aborted\n", *exprp);
			return -1;
		}

		/* Push this node on the stack */
		*sp++ = tp;
	}
	BU_ASSERT_PTR( leafp, ==, leafp_end );

	/* There should only be one thing left on the stack, the result */
	BU_ASSERT_PTR( sp, ==, &stack[1] );

	comb->tree = stack[0];
	RT_CK_TREE(comb->tree);
	db_ck_tree( comb->tree );
	rt_pr_tree( comb->tree, 1 );

	return 0;			/* OK */
}
