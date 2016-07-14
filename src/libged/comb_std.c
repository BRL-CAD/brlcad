/*                  C O M B _ S T D . C
 * BRL-CAD
 *
 * Copyright (c) 2008-2016 United States Government as represented by
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
/** @file libged/comb_std.c
 *
 * The c command.
 *
 */


#include "common.h"

#include <string.h>

#include "tcl.h"

#include "bu/getopt.h"
#include "vmath.h"
#include "rt/geom.h"
#include "ged.h"


struct tokens {
    struct bu_list l;
    short type;
    union tree *tp;
};


/* token types */
#define TOK_NULL	0
#define TOK_LPAREN	1
#define TOK_RPAREN	2
#define TOK_UNION	3
#define TOK_INTER	4
#define TOK_SUBTR	5
#define TOK_TREE	6

HIDDEN void
free_tokens(struct bu_list *hp)
{
    struct tokens *tok;

    BU_CK_LIST_HEAD(hp);

    while (BU_LIST_WHILE(tok, tokens, hp)) {
	BU_LIST_DEQUEUE(&tok->l);
	if (tok->type == TOK_TREE) {
	    db_free_tree(tok->tp, &rt_uniresource);
	}
    }
}


HIDDEN void
append_union(struct bu_list *hp)
{
    struct tokens *tok;

    BU_CK_LIST_HEAD(hp);

    BU_ALLOC(tok, struct tokens);
    tok->type = TOK_UNION;
    tok->tp = (union tree *)NULL;
    BU_LIST_INSERT(hp, &tok->l);
}


HIDDEN void
append_inter(struct bu_list *hp)
{
    struct tokens *tok;

    BU_CK_LIST_HEAD(hp);

    BU_ALLOC(tok, struct tokens);
    tok->type = TOK_INTER;
    tok->tp = (union tree *)NULL;
    BU_LIST_INSERT(hp, &tok->l);
}


HIDDEN void
append_subtr(struct bu_list *hp)
{
    struct tokens *tok;

    BU_CK_LIST_HEAD(hp);

    BU_ALLOC(tok, struct tokens);
    tok->type = TOK_SUBTR;
    tok->tp = (union tree *)NULL;
    BU_LIST_INSERT(hp, &tok->l);
}


HIDDEN void
append_lparen(struct bu_list *hp)
{
    struct tokens *tok;

    BU_CK_LIST_HEAD(hp);

    BU_ALLOC(tok, struct tokens);
    tok->type = TOK_LPAREN;
    tok->tp = (union tree *)NULL;
    BU_LIST_INSERT(hp, &tok->l);
}


HIDDEN void
append_rparen(struct bu_list *hp)
{
    struct tokens *tok;

    BU_CK_LIST_HEAD(hp);

    BU_ALLOC(tok, struct tokens);
    tok->type = TOK_RPAREN;
    tok->tp = (union tree *)NULL;
    BU_LIST_INSERT(hp, &tok->l);
}


HIDDEN int
add_operator(struct ged *gedp, struct bu_list *hp, char *ptr, short int *last_tok)
{
    db_op_t op = db_str2op(ptr);

    BU_CK_LIST_HEAD(hp);

    switch (op) {
	case DB_OP_UNION:
	    append_union(hp);
	    *last_tok = TOK_UNION;
	    break;
	case DB_OP_INTERSECT:
	    append_inter(hp);
	    *last_tok = TOK_INTER;
	    break;
	case DB_OP_SUBTRACT:
	    append_subtr(hp);
	    *last_tok = TOK_SUBTR;
	    break;
	default:
	    bu_vls_printf(gedp->ged_result_str, "Illegal operator: %c (0x%x), aborting\n", ptr[0], ptr[0]);
	    free_tokens(hp);
	    return GED_ERROR;
    }
    return GED_OK;
}


HIDDEN int
add_operand(struct ged *gedp, struct bu_list *hp, char *name)
{
    char *ptr_lparen;
    char *ptr_rparen;
    int name_len;
    union tree *node;
    struct tokens *tok;

    BU_CK_LIST_HEAD(hp);

    ptr_lparen = strchr(name, '(');
    ptr_rparen = strchr(name, ')');

    RT_GET_TREE(node, &rt_uniresource);
    node->tr_op = OP_DB_LEAF;
    node->tr_l.tl_mat = (matp_t)NULL;
    if (ptr_lparen || ptr_rparen) {
	int tmp1, tmp2;

	if (ptr_rparen)
	    tmp1 = ptr_rparen - name;
	else
	    tmp1 = (-1);
	if (ptr_lparen)
	    tmp2 = ptr_lparen - name;
	else
	    tmp2 = (-1);

	if (tmp2 == (-1) && tmp1 > 0)
	    name_len = tmp1;
	else if (tmp1 == (-1) && tmp2 > 0)
	    name_len = tmp2;
	else if (tmp1 > 0 && tmp2 > 0) {
	    if (tmp1 < tmp2)
		name_len = tmp1;
	    else
		name_len = tmp2;
	} else {
	    bu_vls_printf(gedp->ged_result_str, "Cannot determine length of operand name: %s, aborting\n", name);
	    return 0;
	}
    } else
	name_len = (int)strlen(name);

    node->tr_l.tl_name = (char *)bu_malloc(name_len+1, "node name");
    bu_strlcpy(node->tr_l.tl_name, name, name_len+1);

    BU_ALLOC(tok, struct tokens);
    tok->type = TOK_TREE;
    tok->tp = node;
    BU_LIST_INSERT(hp, &tok->l);
    return name_len;
}


HIDDEN void
do_inter(struct bu_list *hp)
{
    struct tokens *tok;

    for (BU_LIST_FOR(tok, tokens, hp)) {
	struct tokens *prev, *next;
	union tree *tp;

	if (tok->type != TOK_INTER)
	    continue;

	prev = BU_LIST_PREV(tokens, &tok->l);
	next = BU_LIST_NEXT(tokens, &tok->l);

	if (prev->type !=TOK_TREE || next->type != TOK_TREE)
	    continue;

	/* this is an eligible intersection operation */
	BU_ALLOC(tp, union tree);
	RT_TREE_INIT(tp);
	tp->tr_b.tb_op = OP_INTERSECT;
	tp->tr_b.tb_regionp = (struct region *)NULL;
	tp->tr_b.tb_left = prev->tp;
	tp->tr_b.tb_right = next->tp;
	BU_LIST_DEQUEUE(&tok->l);
	bu_free((char *)tok, "tok");
	BU_LIST_DEQUEUE(&prev->l);
	bu_free((char *)prev, "prev");
	next->tp = tp;
	tok = next;
    }
}


HIDDEN void
do_union_subtr(struct bu_list *hp)
{
    struct tokens *tok;

    for (BU_LIST_FOR(tok, tokens, hp)) {
	struct tokens *prev, *next;
	union tree *tp;

	if (tok->type != TOK_UNION && tok->type != TOK_SUBTR)
	    continue;

	prev = BU_LIST_PREV(tokens, &tok->l);
	next = BU_LIST_NEXT(tokens, &tok->l);

	if (prev->type !=TOK_TREE || next->type != TOK_TREE)
	    continue;

	/* this is an eligible operation */
	BU_ALLOC(tp, union tree);
	RT_TREE_INIT(tp);
	if (tok->type == TOK_UNION)
	    tp->tr_b.tb_op = OP_UNION;
	else
	    tp->tr_b.tb_op = OP_SUBTRACT;
	tp->tr_b.tb_regionp = (struct region *)NULL;
	tp->tr_b.tb_left = prev->tp;
	tp->tr_b.tb_right = next->tp;
	BU_LIST_DEQUEUE(&tok->l);
	bu_free((char *)tok, "tok");
	BU_LIST_DEQUEUE(&prev->l);
	bu_free((char *)prev, "prev");
	next->tp = tp;
	tok = next;
    }
}


HIDDEN int
do_paren(struct bu_list *hp)
{
    struct tokens *tok;

    for (BU_LIST_FOR(tok, tokens, hp)) {
	struct tokens *prev, *next;

	if (tok->type != TOK_TREE)
	    continue;

	prev = BU_LIST_PREV(tokens, &tok->l);
	next = BU_LIST_NEXT(tokens, &tok->l);

	if (prev->type !=TOK_LPAREN || next->type != TOK_RPAREN)
	    continue;

	/* this is an eligible operand surrounded by parens */
	BU_LIST_DEQUEUE(&next->l);
	bu_free((char *)next, "next");
	BU_LIST_DEQUEUE(&prev->l);
	bu_free((char *)prev, "prev");
    }

    if (hp->forw == hp->back && hp->forw != hp)
	return 1;	/* done */
    else if (BU_LIST_IS_EMPTY(hp))
	return -1;	/* empty tree!!!! */
    else
	return 0;	/* more to do */

}


HIDDEN union tree *
eval_bool(struct bu_list *hp)
{
    int done=0;
    union tree *final_tree;
    struct tokens *tok;

    while (done != 1) {
	do_inter(hp);
	do_union_subtr(hp);
	done = do_paren(hp);
    }

    tok = BU_LIST_NEXT(tokens, hp);
    final_tree = tok->tp;
    BU_LIST_DEQUEUE(&tok->l);
    bu_free((char *)tok, "tok");

    return final_tree;
}


HIDDEN int
check_syntax(struct ged *gedp, struct bu_list *hp, char *comb_name, struct directory *dp)
{
    struct tokens *tok;
    int paren_count  = 0;
    int paren_error  = 0;
    int missing_exp  = 0;
    int missing_op   = 0;
    int op_count     = 0;
    int arg_count    = 0;
    int circular_ref = 0;
    int errors       = 0;
    short last_tok   = TOK_NULL;

    for (BU_LIST_FOR(tok, tokens, hp)) {
	switch (tok->type) {
	    case TOK_LPAREN:
		paren_count++;
		if (last_tok == TOK_RPAREN)
		    missing_op++;
		break;
	    case TOK_RPAREN:
		paren_count--;
		if (last_tok == TOK_LPAREN)
		    missing_exp++;
		break;
	    case TOK_UNION:
	    case TOK_SUBTR:
	    case TOK_INTER:
		op_count++;
		break;
	    case TOK_TREE:
		arg_count++;
		if (!dp && BU_STR_EQUAL(comb_name, tok->tp->tr_l.tl_name))
		    circular_ref++;
		else if (db_lookup(gedp->ged_wdbp->dbip, tok->tp->tr_l.tl_name, LOOKUP_QUIET) == RT_DIR_NULL)
		    bu_vls_printf(gedp->ged_result_str, "WARNING: '%s' does not currently exist\n", tok->tp->tr_l.tl_name);
		break;
	}
	if (paren_count < 0)
	    paren_error++;
	last_tok = tok->type;
    }

    if (paren_count || paren_error) {
	bu_vls_printf(gedp->ged_result_str, "ERROR: unbalanced parenthesis\n");
	errors++;
    }

    if (missing_exp) {
	bu_vls_printf(gedp->ged_result_str, "ERROR: empty parenthesis (missing expression)\n");
	errors++;
    }

    if (missing_op) {
	bu_vls_printf(gedp->ged_result_str, "ERROR: must have operator between ')('\n");
	errors++;
    }

    if (op_count != arg_count-1) {
	bu_vls_printf(gedp->ged_result_str, "ERROR: mismatch of operators and operands\n");
	errors++;
    }

    if (circular_ref) {
	bu_vls_printf(gedp->ged_result_str, "ERROR: combination cannot reference itself during initial creation\n");
	errors++;
    }

    if (errors) {
	bu_vls_printf(gedp->ged_result_str, "\t---------aborting!\n");
	return 1;
    }

    return 0;
}


int
ged_comb_std(struct ged *gedp, int argc, const char *argv[])
{
    char *comb_name;
    int ch;
    int region_flag = -1;
    struct directory *dp = RT_DIR_NULL;
    struct rt_db_internal intern;
    struct rt_comb_internal *comb = NULL;
    struct tokens tok_hd;
    short last_tok;
    int i;
    union tree *final_tree;
    static const char *usage = "[-cr] comb_name <boolean_expr>";

    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_READ_ONLY(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    if (argc < 3) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    /* Parse options */
    bu_optind = 1;	/* re-init bu_getopt() */
    while ((ch = bu_getopt(argc, (char * const *)argv, "cgr?")) != -1) {
	switch (ch) {
	    case 'c':
	    case 'g':
		region_flag = 0;
		break;
	    case 'r':
		region_flag = 1;
		break;
		/* XXX How about -p and -v for FASTGEN? */
	    case '?':
	    default:
		bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
		return GED_OK;
	}
    }
    argc -= (bu_optind + 1);
    argv += bu_optind;

    comb_name = (char *)*argv++;
    if (argc == -1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_OK;
    }

    if ((region_flag != -1) && (argc == 0)) {
	/*
	 * Set/Reset the REGION flag of an existing combination
	 */
	GED_DB_LOOKUP(gedp, dp, comb_name, LOOKUP_NOISY, GED_ERROR & GED_QUIET);

	if (!(dp->d_flags & RT_DIR_COMB)) {
	    bu_vls_printf(gedp->ged_result_str, "%s is not a combination\n", comb_name);
	    return GED_ERROR;
	}

	GED_DB_GET_INTERNAL(gedp, &intern, dp, (fastf_t *)NULL, &rt_uniresource, GED_ERROR);
	comb = (struct rt_comb_internal *)intern.idb_ptr;
	RT_CK_COMB(comb);

	if (region_flag) {
	    if (!comb->region_flag) {
		/* assign values from the defaults */
		comb->region_id = gedp->ged_wdbp->wdb_item_default++;
		comb->aircode = gedp->ged_wdbp->wdb_air_default;
		comb->GIFTmater = gedp->ged_wdbp->wdb_mat_default;
		comb->los = gedp->ged_wdbp->wdb_los_default;
	    }
	    comb->region_flag = 1;
	} else
	    comb->region_flag = 0;

	GED_DB_PUT_INTERNAL(gedp, dp, &intern, &rt_uniresource, GED_ERROR);

	return GED_OK;
    }
    /*
     * At this point, we know we have a Boolean expression.
     * If the combination already existed and region_flag is -1,
     * then leave its region_flag alone.
     * If the combination didn't exist yet,
     * then pretend region_flag was 0.
     * Otherwise, make sure to set its c_flags according to region_flag.
     */

    GED_CHECK_EXISTS(gedp, comb_name, LOOKUP_QUIET, GED_ERROR);
    dp = RT_DIR_NULL;

    /* parse Boolean expression */
    BU_LIST_INIT(&tok_hd.l);
    tok_hd.type = TOK_NULL;

    last_tok = TOK_LPAREN;
    for (i = 0; i < argc; i++) {
	char *ptr;

	ptr = (char *)argv[i];
	while (*ptr) {
	    while (*ptr == '(' || *ptr == ')') {
		switch (*ptr) {
		    case '(':
			append_lparen(&tok_hd.l);
			last_tok = TOK_LPAREN;
			break;
		    case ')':
			append_rparen(&tok_hd.l);
			last_tok = TOK_RPAREN;
			break;
		}
		ptr++;
	    }

	    if (*ptr == '\0')
		continue;

	    if (last_tok == TOK_RPAREN) {
		/* next token MUST be an operator */
		if (add_operator(gedp, &tok_hd.l, ptr, &last_tok) == GED_ERROR) {
		    free_tokens(&tok_hd.l);
		    return GED_ERROR;
		}
		ptr++;
	    } else if (last_tok == TOK_LPAREN) {
		/* next token MUST be an operand */
		int name_len;

		name_len = add_operand(gedp, &tok_hd.l, ptr);
		if (name_len < 1) {
		    free_tokens(&tok_hd.l);
		    return GED_ERROR;
		}
		last_tok = TOK_TREE;
		ptr += name_len;
	    } else if (last_tok == TOK_TREE) {
		/* must be an operator */
		if (add_operator(gedp, &tok_hd.l, ptr, &last_tok) == GED_ERROR) {
		    free_tokens(&tok_hd.l);
		    return GED_ERROR;
		}
		ptr++;
	    } else if (last_tok == TOK_UNION ||
		       last_tok == TOK_INTER ||
		       last_tok == TOK_SUBTR) {
		/* must be an operand */
		int name_len;

		name_len = add_operand(gedp, &tok_hd.l, ptr);
		if (name_len < 1) {
		    free_tokens(&tok_hd.l);
		    return GED_ERROR;
		}
		last_tok = TOK_TREE;
		ptr += name_len;
	    }
	}
    }

    if (check_syntax(gedp, &tok_hd.l, comb_name, dp)) {
	free_tokens(&tok_hd.l);
	return GED_ERROR;
    }

    final_tree = eval_bool(&tok_hd.l);

    {
	int flags;

	flags = RT_DIR_COMB;
	BU_ALLOC(comb, struct rt_comb_internal);
	RT_COMB_INTERNAL_INIT(comb);

	comb->tree = final_tree;

	comb->region_id = -1;
	if (region_flag == (-1))
	    comb->region_flag = 0;
	else
	    comb->region_flag = region_flag;

	if (comb->region_flag) {
	    comb->region_flag = 1;
	    comb->region_id = gedp->ged_wdbp->wdb_item_default++;
	    comb->aircode = gedp->ged_wdbp->wdb_air_default;
	    comb->los = gedp->ged_wdbp->wdb_los_default;
	    comb->GIFTmater = gedp->ged_wdbp->wdb_mat_default;

	    bu_vls_printf(gedp->ged_result_str, "Creating region with attrs: region_id=%d, ", comb->region_id);
	    if (comb->aircode)
		bu_vls_printf(gedp->ged_result_str, "air=%d, ", comb->aircode);
	    bu_vls_printf(gedp->ged_result_str, "los=%d, material_id=%d\n",
			  comb->los,
			  comb->GIFTmater);

	    flags |= RT_DIR_REGION;
	}

	RT_DB_INTERNAL_INIT(&intern);
	intern.idb_major_type = DB5_MAJORTYPE_BRLCAD;
	intern.idb_type = ID_COMBINATION;
	intern.idb_meth = &OBJ[ID_COMBINATION];
	intern.idb_ptr = (void *)comb;

	GED_DB_DIRADD(gedp, dp, comb_name, RT_DIR_PHONY_ADDR, 0, flags, (void *)&intern.idb_type, GED_ERROR);
	GED_DB_PUT_INTERNAL(gedp, dp, &intern, &rt_uniresource, GED_ERROR);
    }

    return GED_OK;
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
