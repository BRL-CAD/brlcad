/*                  C O M B _ S T D . C
 * BRL-CAD
 *
 * Copyright (c) 2008-2026 United States Government as represented by
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

#include "bu/cmdschema.h"
#include "vmath.h"
#include "rt/geom.h"
#include "ged.h"

static const struct bu_cmd_schema *comb_std_schema_for_command(const char *command);

struct comb_std_args {
    int region_flag;
    int help;
};


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

static void
free_tokens(struct bu_list *hp)
{
    struct tokens *tok;

    BU_CK_LIST_HEAD(hp);

    while (BU_LIST_WHILE(tok, tokens, hp)) {
	BU_LIST_DEQUEUE(&tok->l);
	if (tok->type == TOK_TREE) {
	    db_free_tree(tok->tp);
	}
    }
}


static void
append_union(struct bu_list *hp)
{
    struct tokens *tok;

    BU_CK_LIST_HEAD(hp);

    BU_ALLOC(tok, struct tokens);
    tok->type = TOK_UNION;
    tok->tp = (union tree *)NULL;
    BU_LIST_INSERT(hp, &tok->l);
}


static void
append_inter(struct bu_list *hp)
{
    struct tokens *tok;

    BU_CK_LIST_HEAD(hp);

    BU_ALLOC(tok, struct tokens);
    tok->type = TOK_INTER;
    tok->tp = (union tree *)NULL;
    BU_LIST_INSERT(hp, &tok->l);
}


static void
append_subtr(struct bu_list *hp)
{
    struct tokens *tok;

    BU_CK_LIST_HEAD(hp);

    BU_ALLOC(tok, struct tokens);
    tok->type = TOK_SUBTR;
    tok->tp = (union tree *)NULL;
    BU_LIST_INSERT(hp, &tok->l);
}


static void
append_lparen(struct bu_list *hp)
{
    struct tokens *tok;

    BU_CK_LIST_HEAD(hp);

    BU_ALLOC(tok, struct tokens);
    tok->type = TOK_LPAREN;
    tok->tp = (union tree *)NULL;
    BU_LIST_INSERT(hp, &tok->l);
}


static void
append_rparen(struct bu_list *hp)
{
    struct tokens *tok;

    BU_CK_LIST_HEAD(hp);

    BU_ALLOC(tok, struct tokens);
    tok->type = TOK_RPAREN;
    tok->tp = (union tree *)NULL;
    BU_LIST_INSERT(hp, &tok->l);
}


static int
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
	    return BRLCAD_ERROR;
    }
    return BRLCAD_OK;
}


static int
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

    BU_GET(node, union tree);
    RT_TREE_INIT(node);
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


static void
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


static void
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


static int
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


static union tree *
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


static int
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
		else if (db_lookup(gedp->dbip, tok->tp->tr_l.tl_name, LOOKUP_QUIET) == RT_DIR_NULL)
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
ged_comb_std_core(struct ged *gedp, int argc, const char *argv[])
{
    char *comb_name;
    struct comb_std_args args = {-1, 0};
    struct directory *dp = RT_DIR_NULL;
    struct rt_db_internal intern;
    struct rt_comb_internal *comb = NULL;
    struct tokens tok_hd;
    short last_tok;
    int i;
    union tree *final_tree;
    static const char *usage = "[-cr] comb_name <boolean_expr>";
    const char *command;
    int operand_index;

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_READ_ONLY(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);
    command = argv[0];

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    operand_index = bu_cmd_schema_parse_complete(comb_std_schema_for_command(argv[0]), &args,
	gedp->ged_result_str, argc - 1, argv + 1);
    if (operand_index < 0) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }
    argc -= operand_index + 1;
    argv += operand_index + 1;

    if (args.help) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", command, usage);
	return BRLCAD_OK;
    }

    comb_name = (char *)argv[0];
    argc--;
    argv++;

    if ((args.region_flag != -1) && (argc == 0)) {
	/*
	 * Set/Reset the REGION flag of an existing combination
	 */
	GED_DB_LOOKUP(gedp, dp, comb_name, LOOKUP_NOISY, BRLCAD_ERROR & GED_QUIET);

	if (!(dp->d_flags & RT_DIR_COMB)) {
	    bu_vls_printf(gedp->ged_result_str, "%s is not a combination\n", comb_name);
	    return BRLCAD_ERROR;
	}

	GED_DB_GET_INTERN(gedp, &intern, dp, (fastf_t *)NULL, BRLCAD_ERROR);
	comb = (struct rt_comb_internal *)intern.idb_ptr;
	RT_CK_COMB(comb);

	if (args.region_flag) {
	    if (!comb->region_flag) {
		/* assign values from the defaults */
		struct rt_wdb *wdbp = wdb_dbopen(gedp->dbip, RT_WDB_TYPE_DB_DEFAULT);
		comb->region_id = wdbp->wdb_item_default++;
		comb->aircode = wdbp->wdb_air_default;
		comb->GIFTmater = wdbp->wdb_mat_default;
		comb->los = wdbp->wdb_los_default;
	    }
	    comb->region_flag = 1;
	} else
	    comb->region_flag = 0;

	GED_DB_PUT_INTERN(gedp, dp, &intern, BRLCAD_ERROR);

	return BRLCAD_OK;
    }
    /*
     * At this point, we know we have a Boolean expression.
     * If the combination already existed and region_flag is -1,
     * then leave its region_flag alone.
     * If the combination didn't exist yet,
     * then pretend region_flag was 0.
     * Otherwise, make sure to set its c_flags according to region_flag.
     */

    GED_CHECK_EXISTS(gedp, comb_name, LOOKUP_QUIET, BRLCAD_ERROR);
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
		if (add_operator(gedp, &tok_hd.l, ptr, &last_tok) & BRLCAD_ERROR) {
		    free_tokens(&tok_hd.l);
		    return BRLCAD_ERROR;
		}
		ptr++;
	    } else if (last_tok == TOK_LPAREN) {
		/* next token MUST be an operand */
		int name_len;

		name_len = add_operand(gedp, &tok_hd.l, ptr);
		if (name_len < 1) {
		    free_tokens(&tok_hd.l);
		    return BRLCAD_ERROR;
		}
		last_tok = TOK_TREE;
		ptr += name_len;
	    } else if (last_tok == TOK_TREE) {
		/* must be an operator */
		if (add_operator(gedp, &tok_hd.l, ptr, &last_tok) == BRLCAD_ERROR) {
		    free_tokens(&tok_hd.l);
		    return BRLCAD_ERROR;
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
		    return BRLCAD_ERROR;
		}
		last_tok = TOK_TREE;
		ptr += name_len;
	    }
	}
    }

    if (check_syntax(gedp, &tok_hd.l, comb_name, dp)) {
	free_tokens(&tok_hd.l);
	return BRLCAD_ERROR;
    }

    final_tree = eval_bool(&tok_hd.l);

    {
	int flags;

	flags = RT_DIR_COMB;
	BU_ALLOC(comb, struct rt_comb_internal);
	RT_COMB_INTERNAL_INIT(comb);

	comb->tree = final_tree;

	comb->region_id = -1;
	if (args.region_flag == (-1))
	    comb->region_flag = 0;
	else
	    comb->region_flag = args.region_flag;

	if (comb->region_flag) {
	    struct rt_wdb *wdbp = wdb_dbopen(gedp->dbip, RT_WDB_TYPE_DB_DEFAULT);
	    comb->region_flag = 1;
	    comb->region_id = wdbp->wdb_item_default++;
	    comb->aircode = wdbp->wdb_air_default;
	    comb->los = wdbp->wdb_los_default;
	    comb->GIFTmater = wdbp->wdb_mat_default;

	    bu_vls_printf(gedp->ged_result_str, "Creating region with attrs: region_id=%ld, ", comb->region_id);
	    if (comb->aircode)
		bu_vls_printf(gedp->ged_result_str, "air=%ld, ", comb->aircode);
	    bu_vls_printf(gedp->ged_result_str, "los=%ld, material_id=%ld\n",
			  comb->los,
			  comb->GIFTmater);

	    flags |= RT_DIR_REGION;
	}

	RT_DB_INTERNAL_INIT(&intern);
	intern.idb_major_type = DB5_MAJORTYPE_BRLCAD;
	intern.idb_type = ID_COMBINATION;
	intern.idb_meth = &OBJ[ID_COMBINATION];
	intern.idb_ptr = (void *)comb;

	GED_DB_DIRADD(gedp, dp, comb_name, RT_DIR_PHONY_ADDR, 0, flags, (void *)&intern.idb_type, BRLCAD_ERROR);
	GED_DB_PUT_INTERN(gedp, dp, &intern, BRLCAD_ERROR);
    }

    return BRLCAD_OK;
}

#include "../include/plugin.h"

static const char * const comb_std_expr_keywords[] = {"u", "+", "-", "(", ")", NULL};

static int
comb_std_set_non_region(struct bu_vls *UNUSED(msg), const char *UNUSED(arg), void *storage)
{
    if (storage)
	*((int *)storage) = 0;
    return 0;
}

static int
comb_std_set_region(struct bu_vls *UNUSED(msg), const char *UNUSED(arg), void *storage)
{
    if (storage)
	*((int *)storage) = 1;
    return 0;
}

static int
comb_std_expr_keyword(const char *s)
{
    if (!s)
	return 0;
    for (size_t i = 0; comb_std_expr_keywords[i]; i++)
	if (BU_STR_EQUAL(s, comb_std_expr_keywords[i]))
	    return 1;
    return 0;
}

static void
comb_std_expr_candidates(struct bu_cmd_validate_result *result, const char *prefix)
{
    size_t count = 0;

    for (size_t i = 0; comb_std_expr_keywords[i]; i++)
	if (!prefix || !prefix[0] || bu_strncmp(comb_std_expr_keywords[i], prefix, strlen(prefix)) == 0)
	    count++;
    if (!count)
	return;
    result->completion_candidates = (const char **)bu_calloc(count + 1,
	sizeof(char *), "comb_std expression candidates");
    for (size_t i = 0, oi = 0; comb_std_expr_keywords[i]; i++)
	if (!prefix || !prefix[0] || bu_strncmp(comb_std_expr_keywords[i], prefix, strlen(prefix)) == 0)
	    result->completion_candidates[oi++] = bu_strdup(comb_std_expr_keywords[i]);
    result->completion_count = count;
}

static int
comb_std_validation_result(struct bu_cmd_validate_result *result,
	bu_cmd_validate_state_t state, size_t token, bu_cmd_value_t type,
	const char *hint, const char *provider)
{
    bu_cmd_validate_result_clear(result);
    result->state = state;
    result->token_start = token;
    result->token_end = token;
    result->expected = BU_CMD_EXPECT_OPERAND;
    result->completion_type = type;
    result->hint = hint;
    result->semantic_provider = provider;
    return 0;
}

static int
comb_std_option_token(const char *arg)
{
    if (!arg || arg[0] != '-' || !arg[1])
	return 0;
    for (size_t i = 1; arg[i]; i++)
	if (arg[i] != 'c' && arg[i] != 'g' && arg[i] != 'r' && arg[i] != '?')
	    return 0;
    return 1;
}

static size_t
comb_std_first_operand(size_t argc, const char **argv)
{
    size_t i = 0;

    while (i < argc && comb_std_option_token(argv[i]))
	i++;
    if (i < argc && BU_STR_EQUAL(argv[i], "--"))
	i++;
    return i;
}

static int
comb_std_schema_validate(const struct bu_cmd_schema *cmd, size_t argc, const char **argv,
	size_t cursor_arg, struct bu_cmd_validate_result *result)
{
    struct bu_cmd_schema flat = *cmd;
    size_t operands;
    size_t first;
    int set_comb;
    int set_region;
    int help;
    int ret;

    flat.validation.custom_validate = NULL;
    ret = bu_cmd_schema_validate(&flat, argc, argv, cursor_arg, result);
    if (ret || result->state == BU_CMD_VALIDATE_INVALID)
	return ret;


    set_comb = bu_cmd_schema_option_present(cmd, argc, argv, "c");
    set_region = bu_cmd_schema_option_present(cmd, argc, argv, "r");
    help = bu_cmd_schema_option_present(cmd, argc, argv, "?");
    if (cursor_arg < argc && argv[cursor_arg] && argv[cursor_arg][0] == '-' &&
	argv[cursor_arg][1] && (result->expected & BU_CMD_EXPECT_OPTION))
	return 0;

    operands = bu_cmd_schema_operand_count(cmd, argc, argv);
    first = comb_std_first_operand(argc, argv);
    if (help) {
	if (operands)
	    return comb_std_validation_result(result, BU_CMD_VALIDATE_INVALID,
		cursor_arg < argc ? cursor_arg : argc, BU_CMD_VALUE_STRING,
		"-? does not accept operands", NULL);
	return comb_std_validation_result(result, BU_CMD_VALIDATE_VALID, argc,
	    BU_CMD_VALUE_FLAG, "command help", NULL);
    }

    if (!operands)
	return 0;

    if (operands == 1) {
	if (!set_comb && !set_region && cursor_arg >= argc) {
	    comb_std_validation_result(result, BU_CMD_VALIDATE_INCOMPLETE, argc,
		BU_CMD_VALUE_RAW, "boolean expression expected", NULL);
	    comb_std_expr_candidates(result, "");
	}
	return 0;
    }

    if (cursor_arg <= first || cursor_arg >= argc)
	return 0;

    if (comb_std_expr_keyword(argv[cursor_arg])) {
	comb_std_validation_result(result, BU_CMD_VALIDATE_VALID, cursor_arg,
	    BU_CMD_VALUE_KEYWORD, "boolean operator or parenthesis", NULL);
	comb_std_expr_candidates(result, argv[cursor_arg]);
    } else {
	comb_std_validation_result(result, BU_CMD_VALIDATE_VALID, cursor_arg,
	    BU_CMD_VALUE_DB_OBJECT, "boolean expression object", "ged.db_object");
    }
    return 0;
}

static const struct bu_cmd_option comb_std_schema_options[] = {
    BU_CMD_CUSTOM_FLAG("c", NULL, "c", struct comb_std_args, region_flag,
	comb_std_set_non_region, "Create or mark a non-region combination"),
    BU_CMD_ALIAS_SHORT("g", "c", 1),
    BU_CMD_CUSTOM_FLAG("r", NULL, "r", struct comb_std_args, region_flag,
	comb_std_set_region, "Create or mark a region"),
    BU_CMD_FLAG("?", NULL, struct comb_std_args, help, "Print command usage"),
    BU_CMD_OPTION_NULL
};
static const struct bu_cmd_operand comb_std_schema_operands[] = {
    BU_CMD_OPERAND("combination", BU_CMD_VALUE_STRING, 1, 1,
	"Combination to create or modify", NULL),
    BU_CMD_OPERAND("boolean_expression", BU_CMD_VALUE_RAW, 0,
	BU_CMD_COUNT_UNLIMITED, "Parenthesized object boolean expression", NULL),
    BU_CMD_OPERAND_NULL
};
static const struct bu_cmd_schema comb_std_cmd_schema = {
    "comb_std", "Create a combination from a boolean expression", comb_std_schema_options,
    comb_std_schema_operands, BU_CMD_PARSE_OPTIONS_FIRST, {comb_std_schema_validate}
};
static const struct bu_cmd_schema c_cmd_schema = {
    "c", "Create a combination from a boolean expression", comb_std_schema_options,
    comb_std_schema_operands, BU_CMD_PARSE_OPTIONS_FIRST, {comb_std_schema_validate}
};

static const struct bu_cmd_schema *
comb_std_schema_for_command(const char *command)
{
    return BU_STR_EQUAL(command, "c") ? &c_cmd_schema : &comb_std_cmd_schema;
}

#define GED_COMB_STD_COMMANDS(X, XID) \
    X(c, ged_comb_std_core, GED_CMD_DEFAULT, &c_cmd_schema) \
    X(comb_std, ged_comb_std_core, GED_CMD_DEFAULT, &comb_std_cmd_schema) \

GED_DECLARE_COMMAND_SET_WITH_NATIVE_SCHEMA(GED_COMB_STD_COMMANDS)
GED_DECLARE_PLUGIN_MANIFEST_WITH_NATIVE_SCHEMA("libged_comb_std", 1, GED_COMB_STD_COMMANDS)

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
