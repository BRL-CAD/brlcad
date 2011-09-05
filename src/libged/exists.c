/*                         E X I S T S . C
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
/** @file libged/exists.c
 *
 * The exist command.
 *
 * Based on public domein code from:
 * NetBSD: test.c,v 1.38 2011/08/29 14:51:19 joerg
 * 
 * test(1); version 7-like  --  author Erik Baalbergen
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
#include "bio.h"

#include "cmd.h"

#include "./ged_private.h"

/* New exists code from test - some rework to do before even initial
 * compilation trials, so commiting incremental stages turned off.*/
#if 0
enum token {
        EOI,
        OEXIST,
        OCOMB,
        ONULL,
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

static const struct t_op mop3[] = {
        {"beq", BVOLEQ,  BINOP},
        {"bge", BVOLGE,  BINOP},
        {"bgt", BVOLGT,  BINOP},
        {"ble", BVOLLE,  BINOP},
        {"blt", BVOLLT,  BINOP},
        {"bne", BVOLNE,  BINOP},
};

static const struct t_op mop2[] = {
        {"c",   OCOMB,   UNOP},
        {"e",   OEXIST,  UNOP},
        {"n",   ONULL,   UNOP},
        {"p",   OPRIM,   UNOP},
        {"v",   OBVOL,   UNOP},
};

static char **t_wp;
static struct t_op const *t_wp_op;

static int oexpr(enum token);
static int aexpr(enum token);
static int nexpr(enum token);
static int primary(enum token);
static int binop(void);
static int isoperand(void);

#define VTOC(x) (const unsigned char *)((const struct t_op *)x)->op_text

static int
compare1(const void *va, const void *vb)
{
        const unsigned char *a = va;
        const unsigned char *b = VTOC(vb);

        return a[0] - b[0];
}

static int
compare2(const void *va, const void *vb)
{
        const unsigned char *a = va;
        const unsigned char *b = VTOC(vb);
        int z = a[0] - b[0];

        return z ? z : (a[1] - b[1]);
}

static struct t_op const *
findop(const char *s)
{
        if (s[0] == '-') {
                if (s[1] == '\0')
                        return NULL;
                if (s[2] == '\0')
                        return bsearch(s + 1, mop2, __arraycount(mop2),
                            sizeof(*mop2), compare1);
                else if (s[3] != '\0')
                        return NULL;
                else
                        return bsearch(s + 1, mop3, __arraycount(mop3),
                            sizeof(*mop3), compare2);
        } else {
                if (s[1] == '\0')
                        return bsearch(s, cop, __arraycount(cop), sizeof(*cop),
                            compare1);
                else if (strcmp(s, cop2[0].op_text) == 0)
                        return cop2;
                else
                        return NULL;
        }
}


static enum token
t_lex(char *s)
{
        struct t_op const *op;

        if (s == NULL) {
                t_wp_op = NULL;
                return EOI;
        }

        if ((op = findop(s)) != NULL) {
                if (!((op->op_type == UNOP && isoperand()) ||
                    (op->op_num == LPAREN && *(t_wp+1) == 0))) {
                        t_wp_op = op;
                        return op->op_num;
                }
        }
        t_wp_op = NULL;
        return OPERAND;
}


static int
oexpr(enum token n)
{
        int res;

        res = aexpr(n);
        if (*t_wp == NULL)
                return res;
        if (t_lex(*++t_wp) == BOR)
                return oexpr(t_lex(*++t_wp)) || res;
        t_wp--;
        return res;
}

static int
aexpr(enum token n)
{
        int res;

        res = nexpr(n);
        if (*t_wp == NULL)
                return res;
        if (t_lex(*++t_wp) == BAND)
                return aexpr(t_lex(*++t_wp)) && res;
        t_wp--;
        return res;
}

static int
nexpr(enum token n)
{

        if (n == UNOT)
                return !nexpr(t_lex(*++t_wp));
        return primary(n);
}

static int
isoperand(void)
{
        struct t_op const *op;
        char *s, *t;

        if ((s  = *(t_wp+1)) == 0)
                return 1;
        if ((t = *(t_wp+2)) == 0)
                return 0;
        if ((op = findop(s)) != NULL) 
                return op->op_type == BINOP && (t[0] != ')' || t[1] != '\0');
        return 0;
}

/* The code below starts the part that still needs reworking for the
 * new geometry based tokens/logic */
static int
primary(enum token n)
{
        enum token nn;
        int res;

        if (n == EOI)
                return 0;               /* missing expression */
        if (n == LPAREN) {
                if ((nn = t_lex(*++t_wp)) == RPAREN)
                        return 0;       /* missing expression */
                res = oexpr(nn);
                if (t_lex(*++t_wp) != RPAREN)
                        syntax(NULL, "closing paren expected");
                return res;
        }
        if (t_wp_op && t_wp_op->op_type == UNOP) {
                /* unary expression */
                if (*++t_wp == NULL)
                        syntax(t_wp_op->op_text, "argument expected");
                switch (n) {
                case STREZ:
                        return strlen(*t_wp) == 0;
                case STRNZ:
                        return strlen(*t_wp) != 0;
                case FILTT:
                        return isatty((int)getn(*t_wp));
                default:
                        return filstat(*t_wp, n);
                }
        }

        if (t_lex(t_wp[1]), t_wp_op && t_wp_op->op_type == BINOP) {
                return binop();
        }

        return strlen(*t_wp) > 0;
}

static int
binop(void)
{
        const char *opnd1, *opnd2;
        struct t_op const *op;

        opnd1 = *t_wp;
        (void) t_lex(*++t_wp);
        op = t_wp_op;

        if ((opnd2 = *++t_wp) == NULL)
                syntax(op->op_text, "argument expected");

        switch (op->op_num) {
        case STREQ:
                return strcmp(opnd1, opnd2) == 0;
        case STRNE:
                return strcmp(opnd1, opnd2) != 0;
        case STRLT:
                return strcmp(opnd1, opnd2) < 0;
        case STRGT:
                return strcmp(opnd1, opnd2) > 0;
        case INTEQ:
                return getn(opnd1) == getn(opnd2);
        case INTNE:
                return getn(opnd1) != getn(opnd2);
        case INTGE:
                return getn(opnd1) >= getn(opnd2);
        case INTGT:
                return getn(opnd1) > getn(opnd2);
        case INTLE:
                return getn(opnd1) <= getn(opnd2);
        case INTLT:
                return getn(opnd1) < getn(opnd2);
        case FILNT:
                return newerf(opnd1, opnd2);
        case FILOT:
                return olderf(opnd1, opnd2);
        case FILEQ:
                return equalf(opnd1, opnd2);
        default:
                abort();
                /* NOTREACHED */
        }
}



#endif

/**
 * Checks for the existence of a specified object.
 */
int
ged_exists(struct ged *gedp, int argc, const char *argv[])
{
    struct directory *dp;
    static const char *usage = "object";

    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    if (argc != 2) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    dp = db_lookup(gedp->ged_wdbp->dbip, argv[1], LOOKUP_QUIET);
    if (dp == RT_DIR_NULL)
	bu_vls_printf(gedp->ged_result_str, "0");
    else
	bu_vls_printf(gedp->ged_result_str, "1");

    return GED_OK;
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
