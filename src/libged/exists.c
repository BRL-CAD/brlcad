/*                         E X I S T S . C
 * BRL-CAD
 *
 * Copyright (c) 2008-2014 United States Government as represented by
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
/** @file libged/exists.c
 *
 * The exist command.
 *
 * Based on public domein code from:
 * NetBSD: test.c, v 1.38 2011/08/29 14:51:19 joerg
 *
 * test(1); version 7-like -- author Erik Baalbergen
 * modified by Eric Gisin to be used as built-in.
 * modified by Arnold Robbins to add SVR3 compatibility
 * (-x -c -b -p -u -g -k) plus Korn's -L -nt -ot -ef and new -S (socket).
 * modified by J.T. Conklin for NetBSD.
 *
 */

/* exists accepts the following grammar:
   oexpr   ::= aexpr | aexpr "-o" oexpr ;
   aexpr   ::= nexpr | nexpr "-a" aexpr ;
   nexpr   ::= primary | "!" primary
   primary ::= unary-operator operand
   | operand binary-operator operand
   | operand
   | "(" oexpr ")"
   ;
   unary-operator ::= "-c"|"-e"|"-n"|"-p"|"-v";

   binary-operator ::= "="|"!="|"-beq"|"-bne"|"-bge"|"-bgt"|"-ble"|"-blt";
   operand ::= <any legal BRL-CAD object name>
*/

#include "common.h"

#include <stdlib.h>
#include <string.h>

#include "bu/cmd.h"

#include "./ged_private.h"

#ifndef __arraycount
#  define __arraycount(__x)       (sizeof(__x) / sizeof(__x[0]))
#endif

/* expression handling logic */

enum token {
    EOI,
    OEXIST,
    OCOMB,
    ONULL,
    ONNULL,
    OPRIM,
    OBVOL,
    EXTEQ,
    EXTNE,
    EXTGT,
    EXTLT,
    BVOLEQ,
    BVOLNE,
    BVOLGE,
    BVOLGT,
    BVOLLE,
    BVOLLT,
    UNOT,
    BAND,
    BOR,
    LPAREN,
    RPAREN,
    OPERAND
};


enum token_types {
    UNOP,
    BINOP,
    BUNOP,
    BBINOP,
    PAREN
};


struct t_op {
    const char *op_text;
    short op_num, op_type;
};


/* The following option structures need to be kept in sorted order for
 * bsearch - be sure any new entries are in the right numerical order
 * per bu_strcmp (or the ASCII character values, in the single char
 * case.
 */
static const struct t_op cop[] = {
    {"!",   UNOT,   BUNOP},
    {"(",   LPAREN, PAREN},
    {")",   RPAREN, PAREN},
    {"<",   EXTLT,  BINOP},
    {"=",   EXTEQ,  BINOP},
    {">",   EXTGT,  BINOP},
};


static const struct t_op cop2[] = {
    {"!=",  EXTNE,  BINOP},
};


static const struct t_op mop4[] = {
    {"beq", BVOLEQ,  BINOP},
    {"bge", BVOLGE,  BINOP},
    {"bgt", BVOLGT,  BINOP},
    {"ble", BVOLLE,  BINOP},
    {"blt", BVOLLT,  BINOP},
    {"bne", BVOLNE,  BINOP},
};


static const struct t_op mop2[] = {
    {"N",   ONNULL,  UNOP},
    {"a",   BAND,    BBINOP},
    {"c",   OCOMB,   UNOP},
    {"e",   OEXIST,  UNOP},
    {"n",   ONULL,   UNOP},
    {"o",   BOR,     BBINOP},
    {"p",   OPRIM,   UNOP},
    {"v",   OBVOL,   UNOP},
};


struct exists_data {
    char **t_wp;
    struct t_op const *t_wp_op;
    struct bu_vls *message;
    struct ged *gedp;
    int no_op;
};
#define EXISTS_DATA_INIT_ZERO {NULL, NULL, NULL, NULL, 0}

static int oexpr(enum token, struct exists_data *);
static int aexpr(enum token, struct exists_data *);
static int nexpr(enum token, struct exists_data *);
static int primary(enum token, struct exists_data *);
static int binop(struct exists_data *);
static int isoperand(struct exists_data *);

int db_object_exists(struct exists_data *);
int db_object_exists_and_non_null(struct exists_data *);

#define VTOC(x) (const unsigned char *)((const struct t_op *)x)->op_text

static int
compare1(const void *va, const void *vb)
{
    const unsigned char *a = (unsigned char *)va;
    const unsigned char *b = VTOC(vb);

    return a[0] - b[0];
}


static int
compare3(const void *va, const void *vb)
{
    const char *a = (const char *)va;
    const char *b = (const char *)VTOC(vb);
    return bu_strcmp(a, b);
}


static const struct t_op *
findop(const char *s)
{
    if (s[0] == '-') {
	if (s[1] == '\0')
	    return NULL;
	if (s[2] == '\0')
	    return (const struct t_op *)bsearch(s + 1, mop2, __arraycount(mop2), sizeof(*mop2), compare1);
	else if (s[4] != '\0')
	    return NULL;
	else
	    return (const struct t_op *)bsearch(s + 1, mop4, __arraycount(mop4), sizeof(*mop4), compare3);
    } else {
	if (s[1] == '\0')
	    return (const struct t_op *)bsearch(s, cop, __arraycount(cop), sizeof(*cop), compare1);
	else if (BU_STR_EQUAL(s, cop2[0].op_text))
	    return cop2;
	else
	    return NULL;
    }
}


static enum token
t_lex(char *s, struct exists_data *ed)
{
    struct t_op const *op;

    ed->no_op = 0;
    if (s == NULL) {
	ed->t_wp_op = NULL;
	return EOI;
    }

    if ((op = findop(s)) != NULL) {
	if (!((op->op_type == UNOP && isoperand(ed)) ||
	      (op->op_num == LPAREN && *(ed->t_wp+1) == 0))) {
	    ed->t_wp_op = op;
	    return (enum token)op->op_num;
	}
    }
    if (strlen(*(ed->t_wp)) > 0 && !op && !ed->t_wp_op) {
	ed->t_wp_op = findop("-N");
	ed->no_op = 1;
	return (enum token)ed->t_wp_op->op_num;
    } else {
	ed->t_wp_op = NULL;
    }
    return OPERAND;
}


static int
oexpr(enum token n, struct exists_data *ed)
{
    int res;

    res = aexpr(n, ed);
    if (*(ed->t_wp) == NULL)
	return res;
    if (t_lex(*++(ed->t_wp), ed) == BOR) {
	ed->t_wp_op = NULL;
	return oexpr(t_lex(*++(ed->t_wp), ed), ed) || res;
    }
    (ed->t_wp)--;
    return res;
}


static int
aexpr(enum token n, struct exists_data *ed)
{
    int res;

    res = nexpr(n, ed);
    if (*(ed->t_wp) == NULL)
	return res;
    if (t_lex(*++(ed->t_wp), ed) == BAND) {
	ed->t_wp_op = NULL;
	return aexpr(t_lex(*++(ed->t_wp), ed), ed) && res;
    }
    (ed->t_wp)--;
    return res;
}


static int
nexpr(enum token n, struct exists_data *ed)
{

    if (n == UNOT)
	return !nexpr(t_lex(*++(ed->t_wp), ed), ed);
    return primary(n, ed);
}


static int
isoperand(struct exists_data *ed)
{
    struct t_op const *op;
    char *s, *t;

    if ((s  = *((ed->t_wp)+1)) == 0)
	return 1;
    if ((t = *((ed->t_wp)+2)) == 0)
	return 0;
    if ((op = findop(s)) != NULL)
	return op->op_type == BINOP && (t[0] != ')' || t[1] != '\0');
    return 0;
}


/* The code below starts the part that still needs reworking for the
 * new geometry based tokens/logic */
static int
primary(enum token n, struct exists_data *ed)
{
    enum token nn;
    int res;

    if (n == EOI)
	return 0;               /* missing expression */
    if (n == LPAREN) {
	ed->t_wp_op = NULL;
	if ((nn = t_lex(*++(ed->t_wp), ed)) == RPAREN)
	    return 0;       /* missing expression */
	res = oexpr(nn, ed);
	if (t_lex(*++(ed->t_wp), ed) != RPAREN) {
	    bu_vls_printf(ed->message , "closing paren expected");
	    return 0;
	}
	return res;
    }
    if (ed->t_wp_op && ed->t_wp_op->op_type == UNOP) {
	/* unary expression */
	if (!ed->no_op) {
	    if (*++(ed->t_wp) == NULL) {
		bu_vls_printf(ed->message , "argument expected");
		return 0;
	    }
	}
	switch (n) {
	    case OCOMB:
		bu_log("comb case");
		/*return is_comb();*/
	    case OEXIST:
		return db_object_exists(ed);
		/*return db_lookup();*/
	    case ONULL:
		bu_log("null case");
		/*return is_null();*/
	    case ONNULL:
		/* default case */
		return db_object_exists_and_non_null(ed);
	    case OPRIM:
		bu_log("primitive case");
		/*return is_prim();*/
	    case OBVOL:
		bu_log("bounding volume case");
		/*return has_vol();*/
	    default:
		/* not reached */
		return 0;
	}
    }

    if (t_lex(ed->t_wp[1], ed), ed->t_wp_op && ed->t_wp_op->op_type == BINOP) {
	return binop(ed);
    }

    return 0;
}


static int
binop(struct exists_data *ed)
{
    const char /**opnd1, */*opnd2;
    struct t_op const *op;

    /* opnd1 = *(ed->t_wp); */
    (void) t_lex(*++(ed->t_wp), ed);
    op = ed->t_wp_op;

    if ((opnd2 = *++(ed->t_wp)) == NULL) {
	bu_vls_printf(ed->message , "argument expected");
	return 0;
    }

    switch (op->op_num) {
	case EXTEQ:
	    bu_log("extern eq case");
	    /*bu_extern compare*/
	case EXTNE:
	    bu_log("extern neq case");
	    /*bu_extern compare*/
	case EXTLT:
	    bu_log("extern lt case");
	    /*bu_extern compare*/
	case EXTGT:
	    bu_log("extern gt case");
	    /*bu_extern compare*/
	case BVOLEQ:
	    bu_log("vol eq case");
	    /*return bbox_vol(opnd1) == bbox_vol(opnd2);*/
	case BVOLNE:
	    bu_log("vol neq case");
	    /*return bbox_vol(opnd1) != bbox_vol(opnd2);*/
	case BVOLGE:
	    bu_log("vol geq case");
	    /*return bbox_vol(opnd1) >= bbox_vol(opnd2);*/
	case BVOLGT:
	    bu_log("vol gt case");
	    /*return bbox_vol(opnd1) > bbox_vol(opnd2);*/
	case BVOLLE:
	    bu_log("vol leq case");
	    /*return bbox_vol(opnd1) <= bbox_vol(opnd2);*/
	case BVOLLT:
	    bu_log("vol lt case");
	    /*return bbox_vol(opnd1) < bbox_vol(opnd2);*/
	default:
	    return 0;
	    /* NOTREACHED */
    }
}


/* test functions */
int db_object_exists(struct exists_data *ed)
{
    struct directory *dp = NULL;
    dp = db_lookup(ed->gedp->ged_wdbp->dbip, *(ed->t_wp), LOOKUP_QUIET);
    if (dp) return 1;
    return 0;
}


int db_object_exists_and_non_null(struct exists_data *ed)
{
    int result;
    result = db_object_exists(ed);
    if (result) {
	/* db_lookup passes: todo - check for null database object */
	return result;
    } else {
	/* db_lookup fails - no go */
	return result;
    }
}


/**
 * Checks for the existence of a specified object.
 */
int
ged_exists(struct ged *gedp, int argc, const char *argv_orig[])
{
/* struct directory *dp;*/
    static const char *usage = "object";
    struct exists_data ed = EXISTS_DATA_INIT_ZERO;
    struct bu_vls message = BU_VLS_INIT_ZERO;
    int result;
    char **argv = bu_dup_argv(argc, argv_orig);

    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv_orig[0], usage);
	return GED_HELP;
    }
    /*
      if (argc != 2) {
      bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv_orig[0], usage);
      return GED_ERROR;
      }
    */

    ed.t_wp = &argv[1];
    ed.gedp = gedp;
    ed.t_wp_op = NULL;
    ed.message = &message;
    result = oexpr(t_lex(*(ed.t_wp), &ed), &ed);

    if (result)
	bu_vls_printf(gedp->ged_result_str, "1");
    else
	bu_vls_printf(gedp->ged_result_str, "0");

    if (bu_vls_strlen(ed.message) > 0) {
	bu_vls_printf(gedp->ged_result_str, "%s", bu_vls_addr(ed.message));
	bu_vls_free(&message);
	return GED_ERROR;
    }

    bu_vls_free(&message);
    if (*(ed.t_wp) != NULL && *++(ed.t_wp) != NULL) {
	return GED_ERROR;
    } else {
	return GED_OK;
    }
}


/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
