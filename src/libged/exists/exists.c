/*                         E X I S T S . C
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
/** @file libged/exists.c
 *
 * The exists command.
 *
 * Based on public domain code from:
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
#include "bu/malloc.h"
#include "rt/calc.h"
#include "rt/db5.h"
#include "rt/db_io.h"
#include "rt/directory.h"

#include "../ged_private.h"

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
    if (strlen(*(ed->t_wp)) > 0 && !op) {
	/* bare operand: if the next token is a binary operator, treat
	 * this as OPERAND so primary() can dispatch to binop() */
	char *next = *((ed->t_wp)+1);
	if (next != NULL) {
	    const struct t_op *next_op = findop(next);
	    if (next_op != NULL && next_op->op_type == BINOP) {
		ed->t_wp_op = NULL;
		return OPERAND;
	    }
	}
	/* otherwise treat as implicit -N (non-null existence check) */
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


/* ------------------------------------------------------------------ */
/* Object helper functions                                             */
/* ------------------------------------------------------------------ */

/* Look up an object by name.  Returns NULL if not found.             */
static struct directory *
exists_lookup(struct exists_data *ed, const char *name)
{
    return db_lookup(ed->gedp->dbip, name, LOOKUP_QUIET);
}


/* Get the raw external (on-disk) representation for an object.
 * Caller must call bu_free_external(&ext) when done.
 * Returns 0 on success, -1 on failure.                               */
static int
exists_get_external(struct exists_data *ed,
		    const char *name,
		    struct bu_external *ext)
{
    struct directory *dp = exists_lookup(ed, name);
    if (!dp) return -1;
    BU_EXTERNAL_INIT(ext);
    if (db_get_external(ext, dp, ed->gedp->dbip) < 0) {
	bu_free_external(ext);
	return -1;
    }
    return 0;
}


/* Parse the body of an external v5 object record into a bu_external that
 * wraps (does NOT own) the body bytes within the already-fetched ext.
 * Returns 0 on success, -1 if parsing fails.
 * The returned 'body' shares memory with 'ext'; do NOT bu_free_external(body).
 */
static int
exists_get_body(const struct bu_external *ext, struct bu_external *body)
{
    struct db5_raw_internal raw;
    if (db5_get_raw_internal_ptr(&raw, (const unsigned char *)ext->ext_buf) == NULL)
	return -1;
    /* raw.body is a bu_external that aliases into ext->ext_buf */
    body->ext_nbytes = raw.body.ext_nbytes;
    body->ext_buf    = raw.body.ext_buf;
    return 0;
}


/* Compute the bounding-box volume for a named object using rt_obj_bounds.
 * Returns 0 and sets *vol on success, -1 on failure.                 */
static int
exists_bbox_vol(struct exists_data *ed, const char *name, double *vol)
{
    point_t rpp_min, rpp_max;
    struct bu_vls msgs = BU_VLS_INIT_ZERO;
    const char *argv[2];

    VSETALL(rpp_min, MAX_FASTF);
    VREVERSE(rpp_max, rpp_min);

    argv[0] = name;
    argv[1] = NULL;

    if (rt_obj_bounds(&msgs, ed->gedp->dbip, 1, argv, 0, rpp_min, rpp_max) < 0) {
	bu_vls_free(&msgs);
	return -1;
    }
    bu_vls_free(&msgs);

    *vol = (rpp_max[0] - rpp_min[0])
	 * (rpp_max[1] - rpp_min[1])
	 * (rpp_max[2] - rpp_min[2]);
    return 0;
}


/* ------------------------------------------------------------------ */
/* Unary primary implementations                                       */
/* ------------------------------------------------------------------ */

/* -e: exists via db_lookup (no null check) */
static int
exists_obj_exists(struct exists_data *ed)
{
    return (exists_lookup(ed, *(ed->t_wp)) != NULL) ? 1 : 0;
}


/* -N (bare name): exists and has on-disk data (not a phony placeholder) */
static int
exists_obj_non_null(struct exists_data *ed)
{
    struct directory *dp = exists_lookup(ed, *(ed->t_wp));
    if (!dp) return 0;
    /* A phony address means the entry was added to the directory but
     * has not yet been written to disk - treat as null.              */
    if (dp->d_addr == RT_DIR_PHONY_ADDR) return 0;
    return 1;
}


/* -n: exists but IS a phony/null directory entry */
static int
exists_obj_null(struct exists_data *ed)
{
    struct directory *dp = exists_lookup(ed, *(ed->t_wp));
    if (!dp) return 0;
    return (dp->d_addr == RT_DIR_PHONY_ADDR) ? 1 : 0;
}


/* -c: exists and is a combination */
static int
exists_obj_comb(struct exists_data *ed)
{
    struct directory *dp = exists_lookup(ed, *(ed->t_wp));
    if (!dp) return 0;
    return (dp->d_flags & RT_DIR_COMB) ? 1 : 0;
}


/* -p: exists and is a geometric primitive (solid) */
static int
exists_obj_prim(struct exists_data *ed)
{
    struct directory *dp = exists_lookup(ed, *(ed->t_wp));
    if (!dp) return 0;
    if (!(dp->d_flags & RT_DIR_SOLID)) return 0;
    if (dp->d_flags & RT_DIR_NON_GEOM) return 0;
    return 1;
}


/* -v: exists and bounding box has non-zero volume */
static int
exists_obj_vol(struct exists_data *ed)
{
    double vol = 0.0;
    if (exists_lookup(ed, *(ed->t_wp)) == NULL) return 0;
    if (exists_bbox_vol(ed, *(ed->t_wp), &vol) < 0) return 0;
    return !NEAR_ZERO(vol, SMALL_FASTF) ? 1 : 0;
}


/* ------------------------------------------------------------------ */
/* Grammar engine                                                      */
/* ------------------------------------------------------------------ */

static int
primary(enum token n, struct exists_data *ed)
{
    enum token nn;
    int res;

    if (n == EOI)
	return 0;               /* missing expression */
    if (n == LPAREN) {
	ed->t_wp_op = NULL;
	if ((nn = t_lex(*++(ed->t_wp), ed)) == RPAREN) {
	    bu_vls_printf(ed->message, "missing expression inside ()");
	    return 0;
	}
	res = oexpr(nn, ed);
	if (t_lex(*++(ed->t_wp), ed) != RPAREN) {
	    bu_vls_printf(ed->message, "closing paren expected");
	    return 0;
	}
	return res;
    }
    if (ed->t_wp_op && ed->t_wp_op->op_type == UNOP) {
	/* unary expression */
	if (!ed->no_op) {
	    if (*++(ed->t_wp) == NULL) {
		bu_vls_printf(ed->message, "argument expected");
		return 0;
	    }
	}
	switch (n) {
	    case OCOMB:  return exists_obj_comb(ed);
	    case OEXIST: return exists_obj_exists(ed);
	    case ONULL:  return exists_obj_null(ed);
	    case ONNULL: return exists_obj_non_null(ed);
	    case OPRIM:  return exists_obj_prim(ed);
	    case OBVOL:  return exists_obj_vol(ed);
	    default:
		/* not reached */
		return 0;
	}
    }

    /* bare operand: may be first arg of a binary expression */
    if (t_lex(ed->t_wp[1], ed), ed->t_wp_op && ed->t_wp_op->op_type == BINOP) {
	return binop(ed);
    }

    return 0;
}


static int
binop(struct exists_data *ed)
{
    const char *opnd1;
    const char *opnd2;
    struct t_op const *op;

    opnd1 = *(ed->t_wp);
    (void) t_lex(*++(ed->t_wp), ed);
    op = ed->t_wp_op;

    opnd2 = *++(ed->t_wp);
    if (opnd2 == NULL) {
	bu_vls_printf(ed->message, "argument expected after '%s'", op->op_text);
	return 0;
    }

    switch (op->op_num) {
	case EXTEQ:
	case EXTNE:
	case EXTLT:
	case EXTGT: {
	    /* Compare the geometry-body of the serialised on-disk records.
	     * Using the body (not the full external) means name differences
	     * do not affect equality; two objects with the same geometry
	     * but different names compare as equal.                        */
	    struct bu_external ext1, ext2;
	    struct bu_external body1, body2;
	    int ok1, ok2, result = 0;

	    ok1 = exists_get_external(ed, opnd1, &ext1);
	    ok2 = exists_get_external(ed, opnd2, &ext2);

	    if (ok1 < 0 || ok2 < 0) {
		if (ok1 == 0) bu_free_external(&ext1);
		if (ok2 == 0) bu_free_external(&ext2);
		return 0;
	    }

	    if (exists_get_body(&ext1, &body1) < 0 ||
		exists_get_body(&ext2, &body2) < 0)
	    {
		bu_free_external(&ext1);
		bu_free_external(&ext2);
		return 0;
	    }

	    switch (op->op_num) {
		case EXTEQ:
		    result = (body1.ext_nbytes == body2.ext_nbytes
			     && memcmp(body1.ext_buf, body2.ext_buf,
				       body1.ext_nbytes) == 0) ? 1 : 0;
		    break;
		case EXTNE:
		    result = (body1.ext_nbytes != body2.ext_nbytes
			     || memcmp(body1.ext_buf, body2.ext_buf,
				       body1.ext_nbytes) != 0) ? 1 : 0;
		    break;
		case EXTLT:
		    result = (body1.ext_nbytes < body2.ext_nbytes) ? 1 : 0;
		    break;
		case EXTGT:
		    result = (body1.ext_nbytes > body2.ext_nbytes) ? 1 : 0;
		    break;
		default:
		    break;
	    }

	    bu_free_external(&ext1);
	    bu_free_external(&ext2);
	    return result;
	}

	case BVOLEQ:
	case BVOLNE:
	case BVOLGE:
	case BVOLGT:
	case BVOLLE:
	case BVOLLT: {
	    /* Compare bounding-box volumes */
	    double vol1 = 0.0, vol2 = 0.0;

	    if (exists_lookup(ed, opnd1) == NULL ||
		exists_lookup(ed, opnd2) == NULL)
		return 0;
	    if (exists_bbox_vol(ed, opnd1, &vol1) < 0) return 0;
	    if (exists_bbox_vol(ed, opnd2, &vol2) < 0) return 0;

	    switch (op->op_num) {
		case BVOLEQ: return NEAR_EQUAL(vol1, vol2, SMALL_FASTF) ? 1 : 0;
		case BVOLNE: return !NEAR_EQUAL(vol1, vol2, SMALL_FASTF) ? 1 : 0;
		case BVOLGT: return (vol1 > vol2 + SMALL_FASTF) ? 1 : 0;
		case BVOLGE: return (vol1 >= vol2 - SMALL_FASTF) ? 1 : 0;
		case BVOLLT: return (vol1 < vol2 - SMALL_FASTF) ? 1 : 0;
		case BVOLLE: return (vol1 <= vol2 + SMALL_FASTF) ? 1 : 0;
		default: return 0;
	    }
	}

	default:
	    return 0;
    }
}


/**
 * Checks for the existence of a specified object.
 */
int
ged_exists_core(struct ged *gedp, int argc, const char *argv_orig[])
{
    static const char *usage = "expression [expression]...";
    struct exists_data ed = EXISTS_DATA_INIT_ZERO;
    struct bu_vls message = BU_VLS_INIT_ZERO;
    int result;
    char **argv;

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv_orig[0], usage);
	return GED_HELP;
    }

    argv = bu_argv_dup(argc, argv_orig);

    ed.t_wp = &argv[1];
    ed.gedp = gedp;
    ed.t_wp_op = NULL;
    ed.message = &message;
    result = oexpr(t_lex(*(ed.t_wp), &ed), &ed);

    /* check for leftover tokens - indicates a malformed expression */
    if (bu_vls_strlen(&message) == 0
	&& *(ed.t_wp) != NULL && *++(ed.t_wp) != NULL)
    {
	bu_vls_printf(&message, "unexpected token '%s'", *(ed.t_wp));
    }

    bu_argv_free(argc, argv);

    if (bu_vls_strlen(&message) > 0) {
	bu_vls_printf(gedp->ged_result_str, "%s", bu_vls_addr(&message));
	bu_vls_free(&message);
	return BRLCAD_ERROR;
    }

    bu_vls_free(&message);
    bu_vls_printf(gedp->ged_result_str, "%d", result);
    return BRLCAD_OK;
}


#include "../include/plugin.h"

#define GED_EXISTS_COMMANDS(X, XID) \
    X(exists, ged_exists_core, GED_CMD_DEFAULT) \

GED_DECLARE_COMMAND_SET(GED_EXISTS_COMMANDS)
GED_DECLARE_PLUGIN_MANIFEST("libged_exists", 1, GED_EXISTS_COMMANDS)

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
