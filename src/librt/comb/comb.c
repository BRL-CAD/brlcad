/*                          C O M B . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2011 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @addtogroup db5 */
/** @{ */
/** @file comb.c
 *
 * Implement support for combinations in v5 format.
 *
 * The on-disk record looks like this:
 *	width byte
 *	n matricies (only non-identity matricies stored).
 *	n leaves
 *	len of RPN expression.  (len=0 signals all-union expression)
 *	depth of stack
 *	Section 1:  matricies
 *	Section 2:  leaves
 *	Section 3:  (Optional) RPN expression.
 *
 * Encoding of a matrix is (ELEMENTS_PER_MAT * SIZEOF_NETWORK_DOUBLE)
 * bytes, in network order (big-Endian, IEEE double precision).
 *
 */
/** @} */

#include "common.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "bio.h"

#include "bu.h"
#include "vmath.h"
#include "bn.h"
#include "db5.h"
#include "raytrace.h"


struct db_tree_counter_state {
    uint32_t magic;
    long n_mat;			/* # leaves with non-identity matricies */
    long n_leaf;			/* # leaf nodes */
    long n_oper;			/* # operator nodes */
    long leafbytes;		/* # bytes for name section */
    int non_union_seen;		/* boolean, 1 = non-unions seen */
};
#define DB_TREE_COUNTER_STATE_MAGIC 0x64546373	/* dTcs */
#define DB_CK_TREE_COUNTER_STATE(_p) BU_CKMAG(_p, DB_TREE_COUNTER_STATE_MAGIC, "db_tree_counter_state");


/**
 * D B _ T R E E _ C O U N T E R
 *
 * Count number of non-identity matricies, number of leaf nodes,
 * number of operator nodes, etc.
 *
 * Returns - maximum depth of stack needed to unpack this tree, if
 * serialized.
 *
 * Notes - We over-estimate the size of the width fields used for
 * holding the matrix subscripts.  The caller is responsible for
 * correcting by saying:
 *
 * tcsp->leafbytes -= tcsp->n_leaf * (8 - db5_enc_len[wid]);
 */
long
db_tree_counter(const union tree *tp, struct db_tree_counter_state *tcsp)
{
    long ldepth, rdepth;

    RT_CK_TREE(tp);
    DB_CK_TREE_COUNTER_STATE(tcsp);

    switch (tp->tr_op) {
	case OP_DB_LEAF:
	    tcsp->n_leaf++;
	    if (tp->tr_l.tl_mat && !bn_mat_is_identity(tp->tr_l.tl_mat)) tcsp->n_mat++;
	    /* Over-estimate storage requirement for matrix # */
	    tcsp->leafbytes += (long)strlen(tp->tr_l.tl_name) + 1 + 8;
	    return 1;

	case OP_NOT:
	    /* Unary ops */
	    tcsp->n_oper++;
	    tcsp->non_union_seen = 1;
	    return 1 + db_tree_counter(tp->tr_b.tb_left, tcsp);

	case OP_UNION:
	    /* This node is known to be a binary op */
	    tcsp->n_oper++;
	    ldepth = db_tree_counter(tp->tr_b.tb_left, tcsp);
	    rdepth = db_tree_counter(tp->tr_b.tb_right, tcsp);
	    if (ldepth > rdepth) return ldepth;
	    return rdepth;

	case OP_INTERSECT:
	case OP_SUBTRACT:
	case OP_XOR:
	    /* This node is known to be a binary op */
	    tcsp->n_oper++;
	    tcsp->non_union_seen = 1;
	    ldepth = db_tree_counter(tp->tr_b.tb_left, tcsp);
	    rdepth = db_tree_counter(tp->tr_b.tb_right, tcsp);
	    if (ldepth > rdepth) return ldepth;
	    return rdepth;

	default:
	    bu_log("db_tree_counter: bad op %d\n", tp->tr_op);
	    bu_bomb("db_tree_counter\n");
	    /* NOTREACHED */
    }
    /* NOTREACHED */
    return 0;
}


#define DB5COMB_TOKEN_LEAF		1
#define DB5COMB_TOKEN_UNION		2
#define DB5COMB_TOKEN_INTERSECT		3
#define DB5COMB_TOKEN_SUBTRACT		4
#define DB5COMB_TOKEN_XOR		5
#define DB5COMB_TOKEN_NOT		6

struct rt_comb_v5_serialize_state {
    uint32_t magic;
    long mat_num;	/* current matrix number */
    long nmat;		/* # matricies, total */
    unsigned char *matp;
    unsigned char *leafp;
    unsigned char *exprp;
    int wid;
};
#define RT_COMB_V5_SERIALIZE_STATE_MAGIC 0x43357373	/* C5ss */
#define RT_CK_COMB_V5_SERIALIZE_STATE(_p) BU_CKMAG(_p, RT_COMB_V5_SERIALIZE_STATE_MAGIC, "rt_comb_v5_serialize_state")


/**
 * R T _ C O M B _ V 5 _ S E R I A L I Z E
 *
 * In one single pass through the tree, serialize out all three output
 * sections at once.
 */
void
rt_comb_v5_serialize(
    const union tree *tp,
    struct rt_comb_v5_serialize_state *ssp)
{
    size_t n;
    int mi;

    RT_CK_TREE(tp);
    RT_CK_COMB_V5_SERIALIZE_STATE(ssp);

    switch (tp->tr_op) {
	case OP_DB_LEAF:
	    /*
	     * Encoding of the leaf: A null-terminated name string,
	     *and the matrix subscript.  -1 == identity.
	     */
	    n = strlen(tp->tr_l.tl_name) + 1;
	    memcpy(ssp->leafp, tp->tr_l.tl_name, n);
	    ssp->leafp += n;

	    if (tp->tr_l.tl_mat && !bn_mat_is_identity(tp->tr_l.tl_mat))
		mi = ssp->mat_num++;
	    else
		mi = -1;
	    BU_ASSERT_LONG(mi, <, ssp->nmat);
	    ssp->leafp = db5_encode_length(ssp->leafp, mi, ssp->wid);

	    /* Encoding of the matrix */
	    if (mi > -1) {
		htond(ssp->matp,
		      (const unsigned char *)tp->tr_l.tl_mat,
		      ELEMENTS_PER_MAT);
		ssp->matp += ELEMENTS_PER_MAT * SIZEOF_NETWORK_DOUBLE;
	    }

	    /* Encoding of the "leaf" operator */
	    if (ssp->exprp)
		*ssp->exprp++ = DB5COMB_TOKEN_LEAF;
	    return;

	case OP_NOT:
	    /* Unary ops */
	    rt_comb_v5_serialize(tp->tr_b.tb_left, ssp);
	    if (ssp->exprp)
		*ssp->exprp++ = DB5COMB_TOKEN_NOT;
	    return;

	case OP_UNION:
	    /* This node is known to be a binary op */
	    rt_comb_v5_serialize(tp->tr_b.tb_left, ssp);
	    rt_comb_v5_serialize(tp->tr_b.tb_right, ssp);
	    if (ssp->exprp)
		*ssp->exprp++ = DB5COMB_TOKEN_UNION;
	    return;
	case OP_INTERSECT:
	    /* This node is known to be a binary op */
	    rt_comb_v5_serialize(tp->tr_b.tb_left, ssp);
	    rt_comb_v5_serialize(tp->tr_b.tb_right, ssp);
	    if (ssp->exprp)
		*ssp->exprp++ = DB5COMB_TOKEN_INTERSECT;
	    return;
	case OP_SUBTRACT:
	    /* This node is known to be a binary op */
	    rt_comb_v5_serialize(tp->tr_b.tb_left, ssp);
	    rt_comb_v5_serialize(tp->tr_b.tb_right, ssp);
	    if (ssp->exprp)
		*ssp->exprp++ = DB5COMB_TOKEN_SUBTRACT;
	    return;
	case OP_XOR:
	    /* This node is known to be a binary op */
	    rt_comb_v5_serialize(tp->tr_b.tb_left, ssp);
	    rt_comb_v5_serialize(tp->tr_b.tb_right, ssp);
	    if (ssp->exprp)
		*ssp->exprp++ = DB5COMB_TOKEN_XOR;
	    return;

	default:
	    bu_log("rt_comb_v5_serialize: bad op %d\n", tp->tr_op);
	    bu_bomb("rt_comb_v5_serialize\n");
    }
}


/**
 * R T _ C O M B _ E X P O R T 5
 */
int
rt_comb_export5(
    struct bu_external *ep,
    const struct rt_db_internal *ip,
    double UNUSED(local2mm),
    const struct db_i *dbip,
    struct resource *resp)
{
    struct rt_comb_internal *comb;
    struct db_tree_counter_state tcs;
    struct rt_comb_v5_serialize_state ss;
    long max_stack_depth;
    long need;
    int rpn_len = 0;	/* # items in RPN expression */
    int wid;
    unsigned char *cp;
    unsigned char *leafp_end;
    struct bu_attribute_value_set *avsp;
    struct bu_vls value;

    /* check inputs */
    RT_CK_DB_INTERNAL(ip);
    if (dbip) RT_CK_DBI(dbip);
    RT_CK_RESOURCE(resp);

    /* validate it's a comb */
    if (ip->idb_type != ID_COMBINATION) bu_bomb("rt_comb_export5() type not ID_COMBINATION");
    comb = (struct rt_comb_internal *)ip->idb_ptr;
    RT_CK_COMB(comb);

    /* First pass -- count number of non-identity matricies, number of
     * leaf nodes, number of operator nodes.
     */
    memset((char *)&tcs, 0, sizeof(tcs));
    tcs.magic = DB_TREE_COUNTER_STATE_MAGIC;
    if (comb->tree)
	max_stack_depth = db_tree_counter(comb->tree, &tcs);
    else
	max_stack_depth = 0;	/* some combinations have no tree */

    if (tcs.non_union_seen) {
	/* RPN expression needs one byte for each leaf or operator node */
	rpn_len = tcs.n_leaf + tcs.n_oper;
    } else {
	rpn_len = 0;
    }

    wid = db5_select_length_encoding(
	tcs.n_mat | tcs.n_leaf | tcs.leafbytes |
	rpn_len | max_stack_depth);

    /* Apply correction factor to tcs.leafbytes now that we know
     * 'wid'.  Ignore the slight chance that a smaller 'wid' might now
     * be possible.
     */
    tcs.leafbytes -= tcs.n_leaf * (8 - db5_enc_len[wid]);

    /* Second pass -- determine amount of on-disk storage needed */
    need =  1 +			/* width code */
	db5_enc_len[wid] + 	/* size for nmatricies */
	db5_enc_len[wid] +	/* size for nleaves */
	db5_enc_len[wid] +	/* size for leafbytes */
	db5_enc_len[wid] +	/* size for len of RPN */
	db5_enc_len[wid] +	/* size for max_stack_depth */
	tcs.n_mat * (ELEMENTS_PER_MAT * SIZEOF_NETWORK_DOUBLE) +	/* sizeof matrix array */
	tcs.leafbytes +		/* size for leaf nodes */
	rpn_len;		/* storage for RPN expression */

    BU_EXTERNAL_INIT(ep);
    ep->ext_nbytes = need;
    ep->ext_buf = bu_calloc(1, need, "rt_comb_export5 ext_buf");

    /* Build combination's on-disk header section */
    cp = (unsigned char *)ep->ext_buf;
    *cp++ = wid;
    cp = db5_encode_length(cp, tcs.n_mat, wid);
    cp = db5_encode_length(cp, tcs.n_leaf, wid);
    cp = db5_encode_length(cp, tcs.leafbytes, wid);
    cp = db5_encode_length(cp, rpn_len, wid);
    cp = db5_encode_length(cp, max_stack_depth, wid);

    /*
     * The output format has three sections:
     * Section 1:  matricies
     * Section 2:  leaf nodes
     * Section 3:  Optional RPN expression
     *
     * We have pre-computed the exact size of all three sections, so
     * they can all be searialized together in one pass.  Establish
     * pointers to the start of each section.
     */
    ss.magic = RT_COMB_V5_SERIALIZE_STATE_MAGIC;
    ss.wid = wid;
    ss.mat_num = 0;
    ss.nmat = tcs.n_mat;
    ss.matp = cp;
    ss.leafp = cp + tcs.n_mat * (ELEMENTS_PER_MAT * SIZEOF_NETWORK_DOUBLE);
    leafp_end = ss.leafp + tcs.leafbytes;
    if (rpn_len)
	ss.exprp = leafp_end;
    else
	ss.exprp = NULL;

    if (comb->tree)
	rt_comb_v5_serialize(comb->tree, &ss);

    BU_ASSERT_LONG(ss.mat_num, ==, tcs.n_mat);
    BU_ASSERT_PTR(ss.matp, ==, cp + tcs.n_mat * (ELEMENTS_PER_MAT * SIZEOF_NETWORK_DOUBLE));
    BU_ASSERT_PTR(ss.leafp, ==, leafp_end);
    if (rpn_len)
	BU_ASSERT_PTR(ss.exprp, <=, ((unsigned char *)ep->ext_buf) + ep->ext_nbytes);

    /* Encode all the other stuff as attributes. */
    bu_vls_init(&value);
    /* WARNING:  We remove const from the ip pointer!!! */
    avsp = (struct bu_attribute_value_set *)&ip->idb_avs;
    if (avsp->magic != BU_AVS_MAGIC)
	bu_avs_init(avsp, 32, "rt_comb v5 attributes");
    if (comb->region_flag) {
	/* Presence of this attribute means this comb is a region.
	 * Current code values are 0, 1, and 2; all are regions.  See
	 * raytrace.h for meanings of different values
	 */
	bu_vls_trunc(&value, 0);
	switch (comb->is_fastgen) {
	    case REGION_FASTGEN_PLATE:
		bu_vls_printf(&value, "P");
		break;
	    case REGION_FASTGEN_VOLUME:
		bu_vls_printf(&value, "V");
		break;
	    case REGION_NON_FASTGEN: /* fallthrough */
	    default:
		bu_vls_printf(&value, "R");
		break;
	}
	bu_avs_add_vls(avsp, "region", &value);
    } else
	bu_avs_remove(avsp, "region");

    if (comb->inherit)
	bu_avs_add(avsp, "inherit", "1");
    else
	bu_avs_remove(avsp, "inherit");

    if (comb->rgb_valid) {
	bu_vls_trunc(&value, 0);
	bu_vls_printf(&value, "%d/%d/%d", V3ARGS(comb->rgb));
	bu_avs_add_vls(avsp, "rgb", &value);
    } else
	bu_avs_remove(avsp, "rgb");

    /* optical shader string goes in an attribute */
    if (bu_vls_strlen(&comb->shader) > 0)
	bu_avs_add_vls(avsp, "shader", &comb->shader);
    else
	bu_avs_remove(avsp, "shader");

    /* GIFT compatability */
    if (comb->region_id != 0) {
	bu_vls_trunc(&value, 0);
	bu_vls_printf(&value, "%ld", comb->region_id);
	bu_avs_add_vls(avsp, "region_id", &value);
    } else
	bu_avs_remove(avsp, "region_id");

    if (comb->aircode != 0) {
	bu_vls_trunc(&value, 0);
	bu_vls_printf(&value, "%ld", comb->aircode);
	bu_avs_add_vls(avsp, "aircode", &value);
    } else
	bu_avs_remove(avsp, "aircode");

    if (comb->GIFTmater != 0) {
	bu_vls_trunc(&value, 0);
	bu_vls_printf(&value, "%ld", comb->GIFTmater);
	bu_avs_add_vls(avsp, "material_id", &value);
    } else
	bu_avs_remove(avsp, "material_id");

    if (comb->los != 0) {
	bu_vls_trunc(&value, 0);
	bu_vls_printf(&value, "%ld", comb->los);
	bu_avs_add_vls(avsp, "los", &value);
    } else
	bu_avs_remove(avsp, "los");

    bu_vls_free(&value);
    return 0;	/* OK */
}


/**
 * R T _ C O M B _ I M P O R T 5
 *
 * Read a combination object in v5 external (on-disk) format, and
 * convert it into the internal format described in rtgeom.h
 *
 * This is an unusual conversion, because some of the data is taken
 * from attributes, not just from the object body.  By the time this
 * is called, the attributes will already have been cracked into
 * ip->idb_avs, we get the attributes from there.
 *
 * Returns -
 * 0 OK
 * -1 FAIL
 */
int
rt_comb_import5(struct rt_db_internal *ip, const struct bu_external *ep, const mat_t mat, const struct db_i *dbip, struct resource *resp)
{
    struct rt_comb_internal *comb;
    unsigned char *cp;
    int wid;
    size_t nmat, nleaf, rpn_len, max_stack_depth;
    size_t leafbytes;
    unsigned char *matp;
    unsigned char *leafp;
    unsigned char *leafp_end;
    unsigned char *exprp;
#define MAX_V5_STACK 8000
    union tree *stack[MAX_V5_STACK];
    union tree **sp;			/* stack pointer */
    const char *ap;
    size_t ius;

    RT_CK_DB_INTERNAL(ip);
    BU_CK_EXTERNAL(ep);
    RT_CK_DBI(dbip);
    RT_CK_RESOURCE(resp);

    ip->idb_major_type = DB5_MAJORTYPE_BRLCAD;
    ip->idb_type = ID_COMBINATION;
    ip->idb_meth = &rt_functab[ID_COMBINATION];
    BU_GETSTRUCT(comb, rt_comb_internal);
    RT_COMB_INTERNAL_INIT(comb);

    ip->idb_ptr = (genptr_t)comb;

    cp = ep->ext_buf;
    wid = *cp++;
    cp += db5_decode_length(&nmat, cp, wid);
    cp += db5_decode_length(&nleaf, cp, wid);
    cp += db5_decode_length(&leafbytes, cp, wid);
    cp += db5_decode_length(&rpn_len, cp, wid);
    cp += db5_decode_length(&max_stack_depth, cp, wid);
    matp = cp;
    leafp = cp + nmat * (ELEMENTS_PER_MAT * SIZEOF_NETWORK_DOUBLE);
    exprp = leafp + leafbytes;
    leafp_end = exprp;

    if (rpn_len == 0) {
	ssize_t is;

	/* This tree is all union operators, import it as a balanced tree */
	struct bu_ptbl *tbl1, *tbl2;

	tbl1 = (struct bu_ptbl *)bu_malloc(sizeof(struct bu_ptbl), "rt_comb_import5: tbl1");
	tbl2 = (struct bu_ptbl *)bu_malloc(sizeof(struct bu_ptbl), "rt_comb_import5: tbl2");

	/* insert all the leaf nodes into a bu_ptbl */
	bu_ptbl_init(tbl1, nleaf, "rt_comb_import5: tbl");
	for (is = nleaf-1; is >= 0; is--) {
	    union tree *tp;
	    size_t mi;

	    RT_GET_TREE(tp, resp);
	    tp->tr_l.tl_op = OP_DB_LEAF;
	    tp->tr_l.tl_name = bu_strdup((const char *)leafp);
	    leafp += strlen((const char *)leafp) + 1;

	    /* Get matrix index */
	    mi = 4095;			/* sanity */
	    leafp += db5_decode_signed(&mi, leafp, wid);

	    if ((ssize_t)mi < 0) {
		/* Signal identity matrix */
		if (!mat || bn_mat_is_identity(mat)) {
		    tp->tr_l.tl_mat = (matp_t)NULL;
		} else
		    tp->tr_l.tl_mat = bn_mat_dup(mat);
	    } else {
		mat_t diskmat;

		/* Unpack indicated matrix mi */
		BU_ASSERT_LONG(mi, <, nmat);
		ntohd((unsigned char *)diskmat,
		      &matp[mi*ELEMENTS_PER_MAT*SIZEOF_NETWORK_DOUBLE],
		      ELEMENTS_PER_MAT);
		if (!mat || bn_mat_is_identity(mat)) {
		    tp->tr_l.tl_mat = bn_mat_dup(diskmat);
		} else {
		    tp->tr_l.tl_mat = (matp_t)bu_malloc(
			sizeof(mat_t), "v5comb mat");
		    bn_mat_mul(tp->tr_l.tl_mat, mat, diskmat);
		    if (bn_mat_is_identity(tp->tr_l.tl_mat)) {
			bu_free((char *)tp->tr_l.tl_mat, "tl_mat");
			tp->tr_l.tl_mat = (matp_t)NULL;
		    }
		}
	    }
	    bu_ptbl_ins(tbl1, (long *)tp);
	}

	/* use a second bu_ptbl to help build a balanced tree
	 * 1 - pick off pairs of pointers from tbl1
	 * 2 - make a small tree thats unions the pair
	 * 3 - insert that tree into tbl2
	 * 4 - insert any leftover pointer from tbl1 into tbl2
	 * 5 - swap tbl1 and tbl2
	 * 6 - truncate tbl2 and go to step 1
	 * stop when tbl2 has less than 2 members
	 */
	bu_ptbl_init(tbl2, (BU_PTBL_LEN(tbl1) + 1)/2, "rt_comb_import5: tbl1");
	while (1) {
	    struct bu_ptbl *tmp;

	    for (ius=0; ius<BU_PTBL_LEN(tbl1); ius += 2) {
		union tree *tp1, *tp2, *unionp;
		size_t j;

		j = ius + 1;
		tp1 = (union tree *)BU_PTBL_GET(tbl1, ius);
		if (j < BU_PTBL_LEN(tbl1)) {
		    tp2 = (union tree *)BU_PTBL_GET(tbl1, j);
		} else {
		    tp2 = (union tree *)NULL;
		}

		if (tp2) {
		    RT_GET_TREE(unionp, resp);
		    unionp->tr_b.tb_op = OP_UNION;
		    unionp->tr_b.tb_left = tp1;
		    unionp->tr_b.tb_right = tp2;
		    bu_ptbl_ins(tbl2, (long *)unionp);
		} else {
		    bu_ptbl_ins(tbl2, (long *)tp1);
		}

	    }

	    if (BU_PTBL_LEN(tbl2) == 0) {
		comb->tree = (union tree *)NULL;
		bu_ptbl_free(tbl1);
		bu_ptbl_free(tbl2);
		bu_free((char *)tbl1, "rt_comb_import5: tbl1");
		bu_free((char *)tbl2, "rt_comb_import5: tbl2");
		break;
	    } else if (BU_PTBL_LEN(tbl2) == 1) {
		comb->tree = (union tree *)BU_PTBL_GET(tbl2, 0);
		bu_ptbl_free(tbl1);
		bu_ptbl_free(tbl2);
		bu_free((char *)tbl1, "rt_comb_import5: tbl1");
		bu_free((char *)tbl2, "rt_comb_import5: tbl2");
		break;
	    }

	    tmp = tbl2;
	    tbl2 = tbl1;
	    tbl1 = tmp;
	    bu_ptbl_trunc(tbl2, 0);
	}
	BU_ASSERT_PTR(leafp, ==, leafp_end);
	goto finish;
    }

    /*
     * Bring the RPN expression back from the disk, populating leaves
     * and matricies in the order they are encountered.
     */
    if (max_stack_depth > MAX_V5_STACK) {
	bu_log("Combination needs stack depth %zu, only have %d, aborted\n",
	       max_stack_depth, MAX_V5_STACK);
	return -1;
    }
    sp = &stack[0];

    for (ius=0; ius < rpn_len; ius++, exprp++) {
	union tree *tp;
	size_t mi;

	RT_GET_TREE(tp, resp);

	switch (*exprp) {
	    case DB5COMB_TOKEN_LEAF:
		tp->tr_l.tl_op = OP_DB_LEAF;
		tp->tr_l.tl_name = bu_strdup((const char *)leafp);
		leafp += strlen((const char *)leafp) + 1;

		/* Get matrix index */
		mi = 4095;			/* sanity */
		leafp += db5_decode_signed(&mi, leafp, wid);

		if ((ssize_t)mi < 0) {
		    /* Signal identity matrix */
		    if (!mat || bn_mat_is_identity(mat)) {
			tp->tr_l.tl_mat = (matp_t)NULL;
		    } else
			tp->tr_l.tl_mat = bn_mat_dup(mat);
		} else {
		    mat_t diskmat;

		    /* Unpack indicated matrix mi */
		    BU_ASSERT_LONG(mi, <, nmat);
		    ntohd((unsigned char *)diskmat,
			  &matp[mi*ELEMENTS_PER_MAT*SIZEOF_NETWORK_DOUBLE],
			  ELEMENTS_PER_MAT);
		    if (!mat || bn_mat_is_identity(mat)) {
			tp->tr_l.tl_mat = bn_mat_dup(diskmat);
		    } else {
			tp->tr_l.tl_mat = (matp_t)bu_malloc(
			    sizeof(mat_t), "v5comb mat");
			bn_mat_mul(tp->tr_l.tl_mat, mat, diskmat);
			if (bn_mat_is_identity(tp->tr_l.tl_mat)) {
			    bu_free((char *)tp->tr_l.tl_mat, "tl_mat");
			    tp->tr_l.tl_mat = (matp_t)NULL;
			}
		    }
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
		switch (*exprp) {
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
    BU_ASSERT_PTR(leafp, ==, leafp_end);

    /* There should only be one thing left on the stack, the result */
    BU_ASSERT_PTR(sp, ==, &stack[1]);

    comb->tree = stack[0];
    RT_CK_TREE(comb->tree);

finish:
    if (ip->idb_avs.magic != BU_AVS_MAGIC) return 0;	/* OK */

    /* Unpack the attributes */
    comb->rgb_valid = 0;
    
    if ((ap = bu_avs_get(&ip->idb_avs, db5_standard_attribute(ATTR_COLOR))) != NULL) {
	int ibuf[3];
	if (sscanf(ap, "%d/%d/%d", ibuf, ibuf+1, ibuf+2) == 3) {
	    VMOVE(comb->rgb, ibuf);
	    comb->rgb_valid = 1;
	} else {
	    bu_log("unable to parse 'rgb' attribute '%s'\n", ap);
	}
    }
    if ((ap = bu_avs_get(&ip->idb_avs, db5_standard_attribute(ATTR_INHERIT))) != NULL) {
	comb->inherit = atoi(ap);
    }
    if ((ap = bu_avs_get(&ip->idb_avs, db5_standard_attribute(ATTR_REGION))) != NULL) {
	/* Presence of this attribute implies it is a region */
	comb->region_flag = 1;

	/* Determine if this is a FASTGEN region */
	switch (*ap) {
	    case 'V' : /* fallthrough */
	    case '2' :
		comb->is_fastgen = REGION_FASTGEN_VOLUME;
		break;
	    case 'P' : /* fallthrough */
	    case '1' :
		comb->is_fastgen = REGION_FASTGEN_PLATE;
		break;
	    case 'R' : /* fallthrough */
	    case '0' :
		comb->is_fastgen = REGION_NON_FASTGEN;
		break;
	    default:
		bu_log("unable to parse 'region' attribute '%s'\n", ap);
		break;
	}

	/* get the other GIFT "region" attributes */
	if ((ap = bu_avs_get(&ip->idb_avs, db5_standard_attribute(ATTR_REGION_ID))) != NULL) {
	    comb->region_id = atol(ap);
	}
	if ((ap = bu_avs_get(&ip->idb_avs, db5_standard_attribute(ATTR_AIR))) != NULL) {
	    comb->aircode = atol(ap);
	}
	if ((ap = bu_avs_get(&ip->idb_avs, db5_standard_attribute(ATTR_MATERIAL_ID))) != NULL) {
	    comb->GIFTmater = atol(ap);
	}
	if ((ap = bu_avs_get(&ip->idb_avs, db5_standard_attribute(ATTR_LOS))) != NULL) {
	    comb->los = atol(ap);
	}
    }
    if ((ap = bu_avs_get(&ip->idb_avs, db5_standard_attribute(ATTR_SHADER))) != NULL) {
	bu_vls_strcat(&comb->shader, ap);
    }

    return 0; /* OK */
}


/**
 * R T _ C O M B _ G E T
 *
 * Sets the result string to a description of the given combination.
 * Entered via rt_functab[].ft_get().
 */
int
rt_comb_get(struct bu_vls *logstr, const struct rt_db_internal *intern, const char *item)
{
    const struct rt_comb_internal *comb;
    char buf[128];

    RT_CK_DB_INTERNAL(intern);
    comb = (struct rt_comb_internal *)intern->idb_ptr;
    RT_CK_COMB(comb);

    if (item==0) {
	/* Print out the whole combination. */

	bu_vls_printf(logstr, "comb region ");
	if (comb->region_flag) {
	    bu_vls_printf(logstr, "yes id %ld ", comb->region_id);

	    if (comb->aircode) {
		bu_vls_printf(logstr, "air %ld ", comb->aircode);
	    }
	    if (comb->los) {
		bu_vls_printf(logstr, "los %ld ", comb->los);
	    }

	    if (comb->GIFTmater) {
		bu_vls_printf(logstr, "GIFTmater %ld ", comb->GIFTmater);
	    }
	} else {
	    bu_vls_printf(logstr, "no ");
	}

	if (comb->rgb_valid) {
	    bu_vls_printf(logstr, "rgb {%d %d %d} ", V3ARGS(comb->rgb));
	}

	if (bu_vls_strlen(&comb->shader) > 0) {
	    bu_vls_printf(logstr, "shader {%s} ", bu_vls_addr(&comb->shader));
	}

	if (bu_vls_strlen(&comb->material) > 0) {
	    bu_vls_printf(logstr, "material %s ", bu_vls_addr(&comb->material));
	}

	if (comb->inherit) {
	    bu_vls_printf(logstr, "inherit yes ");
	}

	bu_vls_printf(logstr, "tree {");
	db_tree_list(logstr, comb->tree);
	bu_vls_putc(logstr, '}');

	return BRLCAD_OK;
    } else {
	/* Print out only the requested item. */
	register int i;
	char itemlwr[128];

	for (i = 0; i < 128 && item[i]; i++) {
	    itemlwr[i] = (isupper(item[i]) ? tolower(item[i]) :
			  item[i]);
	}
	itemlwr[i] = 0;

	if (BU_STR_EQUAL(itemlwr, "region")) {
	    snprintf(buf, 128, "%s", comb->region_flag ? "yes" : "no");
	} else if (BU_STR_EQUAL(itemlwr, "id")) {
	    if (!comb->region_flag) goto not_region;
	    snprintf(buf, 128, "%ld", comb->region_id);
	} else if (BU_STR_EQUAL(itemlwr, "air")) {
	    if (!comb->region_flag) goto not_region;
	    snprintf(buf, 128, "%ld", comb->aircode);
	} else if (BU_STR_EQUAL(itemlwr, "los")) {
	    if (!comb->region_flag) goto not_region;
	    snprintf(buf, 128, "%ld", comb->los);
	} else if (BU_STR_EQUAL(itemlwr, "giftmater")) {
	    if (!comb->region_flag) goto not_region;
	    snprintf(buf, 128, "%ld", comb->GIFTmater);
	} else if (BU_STR_EQUAL(itemlwr, "rgb")) {
	    if (comb->rgb_valid)
		snprintf(buf, 128, "%d %d %d", V3ARGS(comb->rgb));
	    else
		snprintf(buf, 128, "invalid");
	} else if (BU_STR_EQUAL(itemlwr, "shader")) {
	    bu_vls_printf(logstr, "%s", bu_vls_addr(&comb->shader));
	    return BRLCAD_OK;
	} else if (BU_STR_EQUAL(itemlwr, "material")) {
	    bu_vls_printf(logstr, "%s", bu_vls_addr(&comb->material));
	    return BRLCAD_OK;
	} else if (BU_STR_EQUAL(itemlwr, "inherit")) {
	    snprintf(buf, 128, "%s", comb->inherit ? "yes" : "no");
	} else if (BU_STR_EQUAL(itemlwr, "tree")) {
	    db_tree_list(logstr, comb->tree);
	    return BRLCAD_OK;
	} else {
	    bu_vls_printf(logstr, "no such attribute");
	    return BRLCAD_ERROR;
	}

	bu_vls_printf(logstr, "%s", buf);
	return BRLCAD_OK;

    not_region:
	bu_vls_printf(logstr, "item not valid for non-region");
	return BRLCAD_ERROR;
    }
}


/**
 * R T _ C O M B _ A D J U S T
 *
 * Example -
 * rgb "1 2 3" ...
 *
 * Invoked via rt_functab[ID_COMBINATION].ft_adjust()
 */
int
rt_comb_adjust(struct bu_vls *logstr, struct rt_db_internal *intern, int argc, char **argv)
{
    struct rt_comb_internal *comb;
    char buf[128];
    int i;
    double d;

    RT_CK_DB_INTERNAL(intern);
    comb = (struct rt_comb_internal *)intern->idb_ptr;
    RT_CK_COMB(comb);

    while (argc >= 2) {
	/* Force to lower case */
	for (i=0; i<128 && argv[0][i]!='\0'; i++)
	    buf[i] = isupper(argv[0][i])?tolower(argv[0][i]):argv[0][i];
	buf[i] = 0;

	if (BU_STR_EQUAL(buf, "region")) {
	    if (BU_STR_EQUAL(argv[1], "none")) {
		comb->region_flag = 0;
	    } else if (BU_STR_EQUAL(argv[1], "no")) {
		comb->region_flag = 0;
	    } else if (BU_STR_EQUAL(argv[1], "yes")) {
		comb->region_flag = 1;
	    } else {
		if (sscanf(argv[1], "%d", &i) != 1)
		    return BRLCAD_ERROR;

		if (i != 0)
		    i = 1;

		comb->region_flag = (char)i;
	    }
	} else if (BU_STR_EQUAL(buf, "temp")) {
	    if (!comb->region_flag) goto not_region;
	    if (BU_STR_EQUAL(argv[1], "none")) {
		comb->temperature = 0.0;
	    } else {
		if (sscanf(argv[1], "%lf", &d) != 1)
		    return BRLCAD_ERROR;
		comb->temperature = (float)d;
	    }
	} else if (BU_STR_EQUAL(buf, "id")) {
	    if (!comb->region_flag) goto not_region;
	    if (BU_STR_EQUAL(argv[1], "none")) {
		comb->region_id = 0;
	    } else {
		if (sscanf(argv[1], "%d", &i) != 1)
		    return BRLCAD_ERROR;
		comb->region_id = i;
	    }
	} else if (BU_STR_EQUAL(buf, "air")) {
	    if (!comb->region_flag) goto not_region;
	    if (BU_STR_EQUAL(argv[1], "none")) {
		comb->aircode = 0;
	    } else {
		if (sscanf(argv[1], "%d", &i) != 1)
		    return BRLCAD_ERROR;
		comb->aircode = i;
	    }
	} else if (BU_STR_EQUAL(buf, "los")) {
	    if (!comb->region_flag) goto not_region;
	    if (BU_STR_EQUAL(argv[1], "none")) {
		comb->los = 0;
	    } else {
		if (sscanf(argv[1], "%d", &i) != 1)
		    return BRLCAD_ERROR;
		comb->los = i;
	    }
	} else if (BU_STR_EQUAL(buf, "giftmater")) {
	    if (!comb->region_flag) goto not_region;
	    if (BU_STR_EQUAL(argv[1], "none")) {
		comb->GIFTmater = 0;
	    } else {
		if (sscanf(argv[1], "%d", &i) != 1)
		    return BRLCAD_ERROR;
		comb->GIFTmater = i;
	    }
	} else if (BU_STR_EQUAL(buf, "rgb")) {
	    if (BU_STR_EQUAL(argv[1], "invalid") || BU_STR_EQUAL(argv[1], "none")) {
		comb->rgb[0] = comb->rgb[1] =
		    comb->rgb[2] = 0;
		comb->rgb_valid = 0;
	    } else {
		unsigned int r, g, b;
		i = sscanf(argv[1], "%u %u %u",
			   &r, &g, &b);
		if (i != 3) {
		    bu_vls_printf(logstr, "adjust rgb %s: not valid rgb 3-tuple\n", argv[1]);
		    return BRLCAD_ERROR;
		}
		comb->rgb[0] = (unsigned char)r;
		comb->rgb[1] = (unsigned char)g;
		comb->rgb[2] = (unsigned char)b;
		comb->rgb_valid = 1;
	    }
	} else if (BU_STR_EQUAL(buf, "shader")) {
	    bu_vls_trunc(&comb->shader, 0);
	    if (!BU_STR_EQUAL(argv[1], "none")) {
		bu_vls_strcat(&comb->shader, argv[1]);
		/* Leading spaces boggle the combination exporter */
		bu_vls_trimspace(&comb->shader);
	    }
	} else if (BU_STR_EQUAL(buf, "material")) {
	    bu_vls_trunc(&comb->material, 0);
	    if (!BU_STR_EQUAL(argv[1], "none")) {
		bu_vls_strcat(&comb->material, argv[1]);
		bu_vls_trimspace(&comb->material);
	    }
	} else if (BU_STR_EQUAL(buf, "inherit")) {
	    if (BU_STR_EQUAL(argv[1], "none")) {
		comb->inherit = 0;
	    } else if (BU_STR_EQUAL(argv[1], "no")) {
		comb->inherit = 0;
	    } else if (BU_STR_EQUAL(argv[1], "yes")) {
		comb->inherit = 1;
	    } else {
		if (sscanf(argv[1], "%d", &i) != 1)
		    return BRLCAD_ERROR;

		if (i != 0)
		    i = 1;

		comb->inherit = (char)i;
	    }
	} else if (BU_STR_EQUAL(buf, "tree")) {
	    union tree *new;

	    if (*argv[1] == '\0' || BU_STR_EQUAL(argv[1], "none")) {
		db_free_tree(comb->tree, &rt_uniresource);
		comb->tree = TREE_NULL;
	    } else {
		new = db_tree_parse(logstr, argv[1], &rt_uniresource);
		if (new == TREE_NULL) {
		    bu_vls_printf(logstr, "db adjust tree: bad tree '%s'\n", argv[1]);
		    return BRLCAD_ERROR;
		}
		db_free_tree(comb->tree, &rt_uniresource);
		comb->tree = new;
	    }
	} else {
	    bu_vls_printf(logstr, "db adjust %s : no such attribute", buf);
	    return BRLCAD_ERROR;
	}
	argc -= 2;
	argv += 2;
    }

    return BRLCAD_OK;

not_region:
    bu_vls_printf(logstr, "adjusting attribute %s is not valid for a non-region combination.", buf);
    return BRLCAD_ERROR;
}


/**
 * R T _ C O M B _ F O R M
 */
int
rt_comb_form(struct bu_vls *logstr, const struct rt_functab *ftp)
{
    RT_CK_FUNCTAB(ftp);

    bu_vls_printf(logstr, "region {%%s} id {%%d} air {%%d} los {%%d} GIFTmater {%%d} rgb {%%d %%d %%d} shader {%%s} material {%%s} inherit {%%s} tree {%%s}");

    return BRLCAD_OK;
}


/**
 * R T _ C O M B _ M A K E
 *
 * Create a blank combination with appropriate values.  Called via
 * rt_functab[ID_COMBINATION].ft_make().
 */
void
rt_comb_make(const struct rt_functab *UNUSED(ftp), struct rt_db_internal *intern)
{
    struct rt_comb_internal *comb;

    intern->idb_major_type = DB5_MAJORTYPE_BRLCAD;
    intern->idb_type = ID_COMBINATION;
    intern->idb_meth = &rt_functab[ID_COMBINATION];
    intern->idb_ptr = bu_calloc(sizeof(struct rt_comb_internal), 1,
				"rt_comb_internal");

    comb = (struct rt_comb_internal *)intern->idb_ptr;
    RT_COMB_INTERNAL_INIT(comb);
    bu_vls_init(&comb->shader);
    bu_vls_init(&comb->material);
}


/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
