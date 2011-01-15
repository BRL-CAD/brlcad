/*                  C O M B _ S T D . C
 * BRL-CAD
 *
 * Copyright (c) 2008-2011 United States Government as represented by
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
/** @file comb_std.c
 *
 * The c command.
 *
 */


#include "common.h"

#include <stdio.h>
#include <string.h>
#include "bio.h"

#include "tcl.h"
#include "bu.h"
#include "vmath.h"
#include "rtgeom.h"
#include "ged.h"


struct tokens {
    struct bu_list l;
    short type;
    union tree *tp;
};

/* token types */
#define	GED_TOK_NULL	0
#define	GED_TOK_LPAREN	1
#define	GED_TOK_RPAREN	2
#define	GED_TOK_UNION	3
#define	GED_TOK_INTER	4
#define	GED_TOK_SUBTR	5
#define	GED_TOK_TREE	6

HIDDEN void
ged_free_tokens(struct bu_list *hp)
{
    struct tokens *tok;

    BU_CK_LIST_HEAD(hp);

    while (BU_LIST_WHILE(tok, tokens, hp)) {
	BU_LIST_DEQUEUE(&tok->l);
	if (tok->type == GED_TOK_TREE) {
	    db_free_tree(tok->tp, &rt_uniresource);
	}
    }
}

HIDDEN void
ged_append_union(struct bu_list *hp)
{
    struct tokens *tok;

    BU_CK_LIST_HEAD( hp );

    tok = (struct tokens *)bu_malloc(sizeof(struct tokens), "tok");
    tok->type = GED_TOK_UNION;
    tok->tp = (union tree *)NULL;
    BU_LIST_INSERT(hp, &tok->l);
}

HIDDEN void
ged_append_inter(struct bu_list *hp)
{
    struct tokens *tok;

    BU_CK_LIST_HEAD(hp);

    tok = (struct tokens *)bu_malloc(sizeof(struct tokens), "tok");
    tok->type = GED_TOK_INTER;
    tok->tp = (union tree *)NULL;
    BU_LIST_INSERT( hp, &tok->l );
}

HIDDEN void
ged_append_subtr(struct bu_list *hp)
{
    struct tokens *tok;

    BU_CK_LIST_HEAD(hp);

    tok = (struct tokens *)bu_malloc(sizeof(struct tokens), "tok");
    tok->type = GED_TOK_SUBTR;
    tok->tp = (union tree *)NULL;
    BU_LIST_INSERT( hp, &tok->l );
}

HIDDEN void
ged_append_lparen(struct bu_list *hp)
{
    struct tokens *tok;

    BU_CK_LIST_HEAD(hp);

    tok = (struct tokens *)bu_malloc(sizeof(struct tokens), "tok");
    tok->type = GED_TOK_LPAREN;
    tok->tp = (union tree *)NULL;
    BU_LIST_INSERT( hp, &tok->l );
}

HIDDEN void
ged_append_rparen(struct bu_list *hp)
{
    struct tokens *tok;

    BU_CK_LIST_HEAD(hp);

    tok = (struct tokens *)bu_malloc(sizeof(struct tokens), "tok");
    tok->type = GED_TOK_RPAREN;
    tok->tp = (union tree *)NULL;
    BU_LIST_INSERT(hp, &tok->l);
}

HIDDEN int
ged_add_operator(struct ged *gedp, struct bu_list *hp, char ch, short int *last_tok)
{
    char illegal[2];

    BU_CK_LIST_HEAD(hp);

    switch ( ch )
    {
	case 'u':
	    ged_append_union(hp);
	    *last_tok = GED_TOK_UNION;
	    break;
	case '+':
	    ged_append_inter(hp);
	    *last_tok = GED_TOK_INTER;
	    break;
	case '-':
	    ged_append_subtr(hp);
	    *last_tok = GED_TOK_SUBTR;
	    break;
	default:
	    illegal[0] = ch;
	    illegal[1] = '\0';
	    bu_vls_printf(&gedp->ged_result_str, "Illegal operator: %s, aborting\n", illegal);
	    ged_free_tokens(hp);
	    return GED_ERROR;
    }
    return GED_OK;
}

HIDDEN int
ged_add_operand(struct ged *gedp, struct bu_list *hp, char *name)
{
    char *ptr_lparen;
    char *ptr_rparen;
    int name_len;
    union tree *node;
    struct tokens *tok;

    BU_CK_LIST_HEAD(hp);

    ptr_lparen = strchr(name, '(');
    ptr_rparen = strchr(name, ')');

    RT_GET_TREE( node, &rt_uniresource );
    node->magic = RT_TREE_MAGIC;
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
	}
	else {
	    bu_vls_printf(&gedp->ged_result_str, "Cannot determine length of operand name: %s, aborting\n", name);
	    return 0;
	}
    } else
	name_len = (int)strlen( name );

    node->tr_l.tl_name = (char *)bu_malloc(name_len+1, "node name");
    bu_strlcpy(node->tr_l.tl_name, name, name_len+1);

    tok = (struct tokens *)bu_malloc(sizeof(struct tokens), "tok");
    tok->type = GED_TOK_TREE;
    tok->tp = node;
    BU_LIST_INSERT(hp, &tok->l);
    return name_len;
}

HIDDEN void
ged_do_inter(struct bu_list *hp)
{
    struct tokens *tok;

    for (BU_LIST_FOR(tok, tokens, hp )) {
	struct tokens *prev, *next;
	union tree *tp;

	if (tok->type != GED_TOK_INTER)
	    continue;

	prev = BU_LIST_PREV( tokens, &tok->l );
	next = BU_LIST_NEXT( tokens, &tok->l );

	if (prev->type !=GED_TOK_TREE || next->type != GED_TOK_TREE)
	    continue;

	/* this is an eligible intersection operation */
	tp = (union tree *)bu_malloc( sizeof( union tree ), "tp" );
	tp->magic = RT_TREE_MAGIC;
	tp->tr_b.tb_op = OP_INTERSECT;
	tp->tr_b.tb_regionp = (struct region *)NULL;
	tp->tr_b.tb_left = prev->tp;
	tp->tr_b.tb_right = next->tp;
	BU_LIST_DEQUEUE( &tok->l );
	bu_free( (char *)tok, "tok" );
	BU_LIST_DEQUEUE( &prev->l );
	bu_free( (char *)prev, "prev" );
	next->tp = tp;
	tok = next;
    }
}

HIDDEN void
ged_do_union_subtr(struct bu_list *hp)
{
    struct tokens *tok;

    for (BU_LIST_FOR(tok, tokens, hp)) {
	struct tokens *prev, *next;
	union tree *tp;

	if (tok->type != GED_TOK_UNION && tok->type != GED_TOK_SUBTR)
	    continue;

	prev = BU_LIST_PREV( tokens, &tok->l );
	next = BU_LIST_NEXT( tokens, &tok->l );

	if (prev->type !=GED_TOK_TREE || next->type != GED_TOK_TREE)
	    continue;

	/* this is an eligible operation */
	tp = (union tree *)bu_malloc( sizeof( union tree ), "tp" );
	tp->magic = RT_TREE_MAGIC;
	if (tok->type == GED_TOK_UNION)
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
ged_do_paren(struct bu_list *hp)
{
    struct tokens *tok;

    for (BU_LIST_FOR(tok, tokens, hp)) {
	struct tokens *prev, *next;

	if (tok->type != GED_TOK_TREE)
	    continue;

	prev = BU_LIST_PREV( tokens, &tok->l );
	next = BU_LIST_NEXT( tokens, &tok->l );

	if (prev->type !=GED_TOK_LPAREN || next->type != GED_TOK_RPAREN)
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
ged_eval_bool(struct bu_list *hp)
{
    int done=0;
    union tree *final_tree;
    struct tokens *tok;

    while (done != 1) {
	ged_do_inter(hp);
	ged_do_union_subtr(hp);
	done = ged_do_paren(hp);
    }

    if (done == 1) {
	tok = BU_LIST_NEXT(tokens, hp);
	final_tree = tok->tp;
	BU_LIST_DEQUEUE(&tok->l);
	bu_free((char *)tok, "tok");
	return final_tree;
    }

    return (union tree *)NULL;
}

HIDDEN int
ged_check_syntax(struct ged *gedp, struct bu_list *hp, char *comb_name, struct directory *dp)
{
    struct tokens *tok;
    int paren_count=0;
    int paren_error=0;
    int missing_exp=0;
    int missing_op=0;
    int op_count=0;
    int arg_count=0;
    int circular_ref=0;
    int errors=0;
    short last_tok=GED_TOK_NULL;

    for (BU_LIST_FOR(tok, tokens, hp)) {
	switch (tok->type) {
	    case GED_TOK_LPAREN:
		paren_count++;
		if ( last_tok == GED_TOK_RPAREN )
		    missing_op++;
		break;
	    case GED_TOK_RPAREN:
		paren_count--;
		if ( last_tok == GED_TOK_LPAREN )
		    missing_exp++;
		break;
	    case GED_TOK_UNION:
	    case GED_TOK_SUBTR:
	    case GED_TOK_INTER:
		op_count++;
		break;
	    case GED_TOK_TREE:
		arg_count++;
		if ( !dp && BU_STR_EQUAL( comb_name, tok->tp->tr_l.tl_name ) )
		    circular_ref++;
		else if ( db_lookup( gedp->ged_wdbp->dbip, tok->tp->tr_l.tl_name, LOOKUP_QUIET ) == DIR_NULL )
		    bu_vls_printf(&gedp->ged_result_str, "WARNING: '%s' does not actually exist\n", tok->tp->tr_l.tl_name);
		break;
	}
	if (paren_count < 0)
	    paren_error++;
	last_tok = tok->type;
    }

    if (paren_count || paren_error) {
	bu_vls_printf(&gedp->ged_result_str, "ERROR: unbalanced parenthesis\n");
	errors++;
    }

    if (missing_exp) {
	bu_vls_printf(&gedp->ged_result_str, "ERROR: empty parenthesis (missing expression)\n");
	errors++;
    }

    if (missing_op) {
	bu_vls_printf(&gedp->ged_result_str, "ERROR: must have operator between ')('\n");
	errors++;
    }

    if (op_count != arg_count-1) {
	bu_vls_printf(&gedp->ged_result_str, "ERROR: mismatch of operators and operands\n");
	errors++;
    }

    if (circular_ref) {
	bu_vls_printf(&gedp->ged_result_str, "ERROR: combination cannot reference itself during initial creation\n");
	errors++;
    }

    if (errors) {
	bu_vls_printf(&gedp->ged_result_str, "\t---------aborting!\n");
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
    struct directory *dp = DIR_NULL;
    struct rt_db_internal intern;
    struct rt_comb_internal *comb = NULL;
    struct tokens tok_hd;
    struct tokens *tok;
    short last_tok;
    int i;
    union tree *final_tree;
    static const char *usage = "[-cr] comb_name <boolean_expr>";

    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_READ_ONLY(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

    /* initialize result */
    bu_vls_trunc(&gedp->ged_result_str, 0);
 
    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    if (argc < 3) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    /* Parse options */
    bu_optind = 1;	/* re-init bu_getopt() */
    while ((ch = bu_getopt(argc, (char * const *)argv, "cgr?")) != EOF) {
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
		bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
		return GED_OK;
	}
    }
    argc -= (bu_optind + 1);
    argv += bu_optind;

    comb_name = (char *)*argv++;
    if (argc == -1) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_OK;
    }

    if ((region_flag != -1) && (argc == 0)) {
	/*
	 *	Set/Reset the REGION flag of an existing combination
	 */
	GED_DB_LOOKUP(gedp, dp, comb_name, LOOKUP_NOISY, GED_ERROR & GED_QUIET);

	if (!(dp->d_flags & DIR_COMB)) {
	    bu_vls_printf(&gedp->ged_result_str, "%s is not a combination\n", comb_name);
	    return GED_ERROR;
	}

	GED_DB_GET_INTERNAL(gedp, &intern, dp, (fastf_t *)NULL, &rt_uniresource, GED_ERROR);
	comb = (struct rt_comb_internal *)intern.idb_ptr;
	RT_CK_COMB(comb);

	if (region_flag) {
	    if ( !comb->region_flag ) {
		/* assign values from the defaults */
		comb->region_id = gedp->ged_wdbp->wdb_item_default++;
		comb->aircode = gedp->ged_wdbp->wdb_air_default;
		comb->GIFTmater = gedp->ged_wdbp->wdb_mat_default;
		comb->los = gedp->ged_wdbp->wdb_los_default;
	    }
	    comb->region_flag = 1;
	}
	else
	    comb->region_flag = 0;

	GED_DB_PUT_INTERNAL(gedp, dp, &intern, &rt_uniresource, GED_ERROR);

	return GED_OK;
    }
    /*
     *	At this point, we know we have a Boolean expression.
     *	If the combination already existed and region_flag is -1,
     *	then leave its region_flag alone.
     *	If the combination didn't exist yet,
     *	then pretend region_flag was 0.
     *	Otherwise, make sure to set its c_flags according to region_flag.
     */

    GED_CHECK_EXISTS(gedp, comb_name, LOOKUP_QUIET, GED_ERROR);
    dp = DIR_NULL;

    /* parse Boolean expression */
    BU_LIST_INIT(&tok_hd.l);
    tok_hd.type = GED_TOK_NULL;

    last_tok = GED_TOK_LPAREN;
    for (i=0; i<argc; i++) {
	char *ptr;

	ptr = (char *)argv[i];
	while (*ptr) {
	    while (*ptr == '(' || *ptr == ')') {
		switch (*ptr) {
		    case '(':
			ged_append_lparen( &tok_hd.l );
			last_tok = GED_TOK_LPAREN;
			break;
		    case ')':
			ged_append_rparen( &tok_hd.l );
			last_tok = GED_TOK_RPAREN;
			break;
		}
		ptr++;
	    }

	    if (*ptr == '\0')
		continue;

	    if (last_tok == GED_TOK_RPAREN) {
		/* next token MUST be an operator */
		if (ged_add_operator(gedp, &tok_hd.l, *ptr, &last_tok) == GED_ERROR) {
		    ged_free_tokens(&tok_hd.l);
		    if (dp != DIR_NULL)
			rt_db_free_internal(&intern);
		    return GED_ERROR;
		}
		ptr++;
	    } else if (last_tok == GED_TOK_LPAREN) {
		/* next token MUST be an operand */
		int name_len;

		name_len = ged_add_operand(gedp, &tok_hd.l, ptr );
		if (name_len < 1) {
		    ged_free_tokens(&tok_hd.l);
		    if (dp != DIR_NULL)
			rt_db_free_internal(&intern);
		    return GED_ERROR;
		}
		last_tok = GED_TOK_TREE;
		ptr += name_len;
	    } else if (last_tok == GED_TOK_TREE) {
		/* must be an operator */
		if (ged_add_operator(gedp, &tok_hd.l, *ptr, &last_tok) == GED_ERROR) {
		    ged_free_tokens(&tok_hd.l);
		    if (dp != DIR_NULL)
			rt_db_free_internal(&intern);
		    return GED_ERROR;
		}
		ptr++;
	    } else if (last_tok == GED_TOK_UNION ||
		       last_tok == GED_TOK_INTER ||
		       last_tok == GED_TOK_SUBTR) {
		/* must be an operand */
		int name_len;

		name_len = ged_add_operand(gedp, &tok_hd.l, ptr );
		if (name_len < 1) {
		    ged_free_tokens(&tok_hd.l);
		    if (dp != DIR_NULL)
			rt_db_free_internal(&intern);
		    return GED_ERROR;
		}
		last_tok = GED_TOK_TREE;
		ptr += name_len;
	    }
	}
    }

    if (ged_check_syntax(gedp, &tok_hd.l, comb_name, dp)) {
	ged_free_tokens(&tok_hd.l);
	return GED_ERROR;
    }

    /* replace any occurences of comb_name with existing tree */
    if (dp != DIR_NULL) {
	for (BU_LIST_FOR(tok, tokens, &tok_hd.l)) {
	    struct rt_db_internal intern1;
	    struct rt_comb_internal *comb1;

	    switch (tok->type) {
		case GED_TOK_LPAREN:
		case GED_TOK_RPAREN:
		case GED_TOK_UNION:
		case GED_TOK_INTER:
		case GED_TOK_SUBTR:
		    break;
		case GED_TOK_TREE:
		    if (tok->tp && BU_STR_EQUAL(tok->tp->tr_l.tl_name, comb_name)) {
			db_free_tree( tok->tp, &rt_uniresource );
			GED_DB_GET_INTERNAL(gedp, &intern1, dp, (fastf_t *)NULL, &rt_uniresource, GED_ERROR);
			comb1 = (struct rt_comb_internal *)intern1.idb_ptr;
			RT_CK_COMB(comb1);

			tok->tp = comb1->tree;
			comb1->tree = (union tree *)NULL;
			rt_db_free_internal(&intern1);
		    }
		    break;
		default:
		    bu_vls_printf(&gedp->ged_result_str, "ERROR: Unrecognized token type\n");
		    ged_free_tokens(&tok_hd.l);
		    return GED_ERROR;
	    }
	}
    }

    final_tree = ged_eval_bool(&tok_hd.l);

    if (dp == DIR_NULL) {
	int flags;

	flags = DIR_COMB;
	BU_GETSTRUCT(comb, rt_comb_internal);
	comb->magic = RT_COMB_MAGIC;
	comb->tree = final_tree;
	bu_vls_init(&comb->shader);
	bu_vls_init(&comb->material);
	comb->region_id = -1;
	if (region_flag == (-1))
	    comb->region_flag = 0;
	else
	    comb->region_flag = region_flag;

	if (comb->region_flag) {
	    comb->region_flag = 1;
	    comb->region_id = gedp->ged_wdbp->wdb_item_default++;;
	    comb->aircode = gedp->ged_wdbp->wdb_air_default;
	    comb->los = gedp->ged_wdbp->wdb_los_default;
	    comb->GIFTmater = gedp->ged_wdbp->wdb_mat_default;
	    bu_vls_printf(&gedp->ged_result_str,
			  "Creating region id=%ld, air=%ld, los=%ld, GIFTmaterial=%ld\n",
			  comb->region_id, comb->aircode, comb->los, comb->GIFTmater);

	    flags |= DIR_REGION;
	}

	RT_INIT_DB_INTERNAL(&intern);
	intern.idb_major_type = DB5_MAJORTYPE_BRLCAD;
	intern.idb_type = ID_COMBINATION;
	intern.idb_meth = &rt_functab[ID_COMBINATION];
	intern.idb_ptr = (genptr_t)comb;

	GED_DB_DIRADD(gedp, dp, comb_name, RT_DIR_PHONY_ADDR, 0, flags, (genptr_t)&intern.idb_type, GED_ERROR);
	GED_DB_PUT_INTERNAL(gedp, dp, &intern, &rt_uniresource, GED_ERROR);
    } else {
	db_delete(gedp->ged_wdbp->dbip, dp);

	dp->d_len = 0;
	dp->d_un.file_offset = (off_t)-1;
	db_free_tree(comb->tree, &rt_uniresource);
	comb->tree = final_tree;

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
