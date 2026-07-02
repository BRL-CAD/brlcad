/*                      S C A D _ E V A L . C
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 *
 */
/** @file scad_eval.c
 *
 * Evaluator for the OpenSCAD AST.  Walks the parsed statement tree and
 * produces a resolved CSG geometry tree (struct scad_geom) that the
 * geometry back-end (scad_geom.c) writes to a BRL-CAD database.
 *
 * Implements the runtime value system, environments, the expression
 * evaluator (including list comprehensions and let-expressions), the
 * built-in math/list functions, user-defined modules and functions,
 * control flow (if/for/intersection_for/let), children()/$children,
 * the modifier characters, and include/use file resolution.
 *
 * Scoping note: for simplicity this evaluator uses dynamic scoping (a
 * new scope's parent is the enclosing runtime scope).  This makes the
 * OpenSCAD special variables ($fn/$fa/$fs) propagate down the call tree
 * as they should; the rare cases where it differs from OpenSCAD's
 * lexical scoping of ordinary variables are noted in TODO.scad.
 */

#include "common.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <ctype.h>
#include <stdint.h>

#include "bu/log.h"
#include "bu/malloc.h"
#include "bu/str.h"
#include "bu/file.h"
#include "bu/path.h"
#include "bu/vls.h"
#include "bn/mat.h"
#include "vmath.h"

#include "scad.h"

#ifndef M_PI
#  define M_PI 3.14159265358979323846
#endif

#define SCAD_MAX_DEPTH 20000	/* recursion guard for functions/modules */


/* ------------------------------------------------------------------ */
/* value helpers                                                      */
/* ------------------------------------------------------------------ */

static struct scad_value
v_undef(void)
{
    struct scad_value v;
    memset(&v, 0, sizeof(v));
    v.type = SCAD_VAL_UNDEF;
    return v;
}

static struct scad_value
v_num(double n)
{
    struct scad_value v = v_undef();
    v.type = SCAD_VAL_NUM;
    v.num = n;
    return v;
}

static struct scad_value
v_bool(int b)
{
    struct scad_value v = v_undef();
    v.type = SCAD_VAL_BOOL;
    v.num = b ? 1.0 : 0.0;
    return v;
}

static struct scad_value
v_str(const char *s)
{
    struct scad_value v = v_undef();
    v.type = SCAD_VAL_STR;
    v.str = bu_strdup(s ? s : "");
    return v;
}

static struct scad_value
v_vec(size_t n)
{
    struct scad_value v = v_undef();
    size_t i;
    v.type = SCAD_VAL_VEC;
    v.vlen = n;
    v.items = n ? (struct scad_value *)bu_calloc(n, sizeof(struct scad_value), "scad vec") : NULL;
    for (i = 0; i < n; i++) v.items[i] = v_undef();
    return v;
}

void
scad_value_free(struct scad_value *v)
{
    size_t i;
    if (!v) return;
    if (v->type == SCAD_VAL_STR && v->str) {
	bu_free(v->str, "scad val str");
	v->str = NULL;
    }
    if (v->type == SCAD_VAL_VEC && v->items) {
	for (i = 0; i < v->vlen; i++)
	    scad_value_free(&v->items[i]);
	bu_free(v->items, "scad val items");
	v->items = NULL;
    }
    v->type = SCAD_VAL_UNDEF;
}

struct scad_value
scad_value_copy(const struct scad_value *v)
{
    struct scad_value r;
    size_t i;
    if (!v) return v_undef();
    r = *v;
    if (v->type == SCAD_VAL_STR) {
	r.str = bu_strdup(v->str ? v->str : "");
    } else if (v->type == SCAD_VAL_VEC) {
	r.items = v->vlen ? (struct scad_value *)bu_calloc(v->vlen, sizeof(struct scad_value), "scad vec copy") : NULL;
	for (i = 0; i < v->vlen; i++)
	    r.items[i] = scad_value_copy(&v->items[i]);
    }
    return r;
}

/* numeric coercion; returns 0 for non-numeric */
static double
v_asnum(const struct scad_value *v)
{
    if (v && (v->type == SCAD_VAL_NUM || v->type == SCAD_VAL_BOOL))
	return v->num;
    return 0.0;
}

/* OpenSCAD truthiness: false, undef, 0, "", [] are false */
static int
v_true(const struct scad_value *v)
{
    if (!v) return 0;
    switch (v->type) {
	case SCAD_VAL_UNDEF: return 0;
	case SCAD_VAL_BOOL:
	case SCAD_VAL_NUM: return v->num != 0.0;
	case SCAD_VAL_STR: return v->str && v->str[0] != '\0';
	case SCAD_VAL_VEC: return v->vlen != 0;
	case SCAD_VAL_RANGE: return 1;
    }
    return 0;
}

/* extract up to 3 components into out[], filling the rest with fill.
 * a scalar broadcasts to all three. */
static void
v_to_vec3(const struct scad_value *v, vect_t out, double fill)
{
    out[0] = out[1] = out[2] = fill;
    if (!v) return;
    if (v->type == SCAD_VAL_NUM || v->type == SCAD_VAL_BOOL) {
	out[0] = out[1] = out[2] = v->num;
    } else if (v->type == SCAD_VAL_VEC) {
	size_t i;
	for (i = 0; i < v->vlen && i < 3; i++)
	    out[i] = v_asnum(&v->items[i]);
    }
}


/* ------------------------------------------------------------------ */
/* environments                                                       */
/* ------------------------------------------------------------------ */

struct binding {
    char *name;
    struct scad_value val;
    struct binding *next;
};

struct moddef {
    char *name;
    struct scad_param *params;
    struct scad_stmt **body;
    size_t nbody;
    struct moddef *next;
};

struct funcdef {
    char *name;
    struct scad_param *params;
    struct scad_expr *body;
    struct funcdef *next;
};

struct env {
    struct binding *vars;
    struct moddef *mods;
    struct funcdef *funcs;
    struct env *parent;
    /* children context for a module instantiation */
    int has_children;
    struct scad_stmt **children;
    size_t nchildren;
    struct env *children_env;
};

static struct env *
env_new(struct env *parent)
{
    struct env *e;
    BU_ALLOC(e, struct env);
    memset(e, 0, sizeof(*e));
    e->parent = parent;
    return e;
}

static void
env_free(struct env *e)
{
    struct binding *b;
    struct moddef *m;
    struct funcdef *f;
    if (!e) return;
    b = e->vars;
    while (b) {
	struct binding *nx = b->next;
	bu_free(b->name, "scad binding name");
	scad_value_free(&b->val);
	bu_free(b, "scad binding");
	b = nx;
    }
    m = e->mods;
    while (m) { struct moddef *nx = m->next; bu_free(m->name, "moddef name"); bu_free(m, "moddef"); m = nx; }
    f = e->funcs;
    while (f) { struct funcdef *nx = f->next; bu_free(f->name, "funcdef name"); bu_free(f, "funcdef"); f = nx; }
    bu_free(e, "scad env");
}

/* bind (copy) a value in the given scope, overwriting any prior */
static void
env_set(struct env *e, const char *name, struct scad_value val)
{
    struct binding *b;
    for (b = e->vars; b; b = b->next) {
	if (BU_STR_EQUAL(b->name, name)) {
	    scad_value_free(&b->val);
	    b->val = val;
	    return;
	}
    }
    BU_ALLOC(b, struct binding);
    b->name = bu_strdup(name);
    b->val = val;
    b->next = e->vars;
    e->vars = b;
}

static int
env_get(struct env *e, const char *name, struct scad_value *out)
{
    for (; e; e = e->parent) {
	struct binding *b;
	for (b = e->vars; b; b = b->next) {
	    if (BU_STR_EQUAL(b->name, name)) {
		*out = scad_value_copy(&b->val);
		return 1;
	    }
	}
    }
    return 0;
}

static double
env_num(struct env *e, const char *name, double dflt)
{
    struct scad_value v;
    double r = dflt;
    if (env_get(e, name, &v)) {
	if (v.type == SCAD_VAL_NUM || v.type == SCAD_VAL_BOOL) r = v.num;
	scad_value_free(&v);
    }
    return r;
}

static struct moddef *
env_find_mod(struct env *e, const char *name)
{
    for (; e; e = e->parent) {
	struct moddef *m;
	for (m = e->mods; m; m = m->next)
	    if (BU_STR_EQUAL(m->name, name)) return m;
    }
    return NULL;
}

static struct funcdef *
env_find_func(struct env *e, const char *name)
{
    for (; e; e = e->parent) {
	struct funcdef *f;
	for (f = e->funcs; f; f = f->next)
	    if (BU_STR_EQUAL(f->name, name)) return f;
    }
    return NULL;
}


/* ------------------------------------------------------------------ */
/* forward declarations                                               */
/* ------------------------------------------------------------------ */

static struct scad_value eval_expr(struct scad_state *st, struct env *env, struct scad_expr *e);
static double v_asnum2(struct scad_state *st, struct env *env, struct scad_expr *e);
static void exec_body(struct scad_state *st, struct env *env,
		      struct scad_stmt **body, size_t n, struct scad_geom *into);
static void exec_stmt(struct scad_state *st, struct env *env,
		      struct scad_stmt *s, struct scad_geom *into);


/* ------------------------------------------------------------------ */
/* built-in functions                                                 */
/* ------------------------------------------------------------------ */

/* deterministic PRNG for rands() so conversions are reproducible
 * (uint64_t: unsigned long is only 32-bit on LLP64/Windows) */
static uint64_t scad_rng_state = 0x2545F4914F6CDD1DULL;

static double
scad_rng(void)
{
    /* xorshift64* */
    uint64_t x = scad_rng_state ? scad_rng_state : 0x9E3779B97F4A7C15ULL;
    x ^= x >> 12;
    x ^= x << 25;
    x ^= x >> 27;
    scad_rng_state = x;
    return (double)((x * 0x2545F4914F6CDD1DULL) >> 11) / (double)(1ULL << 53);
}

/* concatenate values (one level of flattening for vectors) */
static struct scad_value
builtin_concat(struct scad_value *args, size_t n)
{
    size_t total = 0, i, j, k = 0;
    struct scad_value r;
    for (i = 0; i < n; i++)
	total += (args[i].type == SCAD_VAL_VEC) ? args[i].vlen : 1;
    r = v_vec(total);
    for (i = 0; i < n; i++) {
	if (args[i].type == SCAD_VAL_VEC) {
	    for (j = 0; j < args[i].vlen; j++)
		r.items[k++] = scad_value_copy(&args[i].items[j]);
	} else {
	    r.items[k++] = scad_value_copy(&args[i]);
	}
    }
    return r;
}

static struct scad_value
builtin_lookup(struct scad_value *key, struct scad_value *tbl)
{
    /* linear interpolation lookup over [[k,v],...] */
    double k, lo_k = 0, lo_v = 0, hi_k = 0, hi_v = 0;
    int have_lo = 0, have_hi = 0;
    size_t i;
    if (!key || !tbl || tbl->type != SCAD_VAL_VEC) return v_undef();
    k = v_asnum(key);
    for (i = 0; i < tbl->vlen; i++) {
	struct scad_value *row = &tbl->items[i];
	double rk, rv;
	if (row->type != SCAD_VAL_VEC || row->vlen < 2) continue;
	rk = v_asnum(&row->items[0]);
	rv = v_asnum(&row->items[1]);
	if (rk == k) return v_num(rv);
	if (rk < k && (!have_lo || rk > lo_k)) { lo_k = rk; lo_v = rv; have_lo = 1; }
	if (rk > k && (!have_hi || rk < hi_k)) { hi_k = rk; hi_v = rv; have_hi = 1; }
    }
    if (have_lo && have_hi) {
	double t = (k - lo_k) / (hi_k - lo_k);
	return v_num(lo_v + t * (hi_v - lo_v));
    }
    if (have_lo) return v_num(lo_v);
    if (have_hi) return v_num(hi_v);
    return v_undef();
}

/* append a textual form of a value to a vls (used by str/echo) */
static void
value_to_str(struct bu_vls *out, const struct scad_value *v)
{
    size_t i;
    if (!v) { bu_vls_strcat(out, "undef"); return; }
    switch (v->type) {
	case SCAD_VAL_UNDEF: bu_vls_strcat(out, "undef"); break;
	case SCAD_VAL_BOOL: bu_vls_strcat(out, v->num ? "true" : "false"); break;
	case SCAD_VAL_NUM: bu_vls_printf(out, "%g", v->num); break;
	case SCAD_VAL_STR: bu_vls_strcat(out, v->str ? v->str : ""); break;
	case SCAD_VAL_RANGE: bu_vls_printf(out, "[%g:%g:%g]", v->range[0], v->range[1], v->range[2]); break;
	case SCAD_VAL_VEC:
	    bu_vls_putc(out, '[');
	    for (i = 0; i < v->vlen; i++) {
		if (i) bu_vls_strcat(out, ", ");
		value_to_str(out, &v->items[i]);
	    }
	    bu_vls_putc(out, ']');
	    break;
    }
}

/* the "1 arg is a number" math builtins */
static int
math1(const char *name, double x, double *out)
{
    if (BU_STR_EQUAL(name, "sin")) { *out = sin(x * DEG2RAD); return 1; }
    if (BU_STR_EQUAL(name, "cos")) { *out = cos(x * DEG2RAD); return 1; }
    if (BU_STR_EQUAL(name, "tan")) { *out = tan(x * DEG2RAD); return 1; }
    if (BU_STR_EQUAL(name, "asin")) { *out = asin(x) * RAD2DEG; return 1; }
    if (BU_STR_EQUAL(name, "acos")) { *out = acos(x) * RAD2DEG; return 1; }
    if (BU_STR_EQUAL(name, "atan")) { *out = atan(x) * RAD2DEG; return 1; }
    if (BU_STR_EQUAL(name, "abs")) { *out = fabs(x); return 1; }
    if (BU_STR_EQUAL(name, "sign")) { *out = (x > 0) - (x < 0); return 1; }
    if (BU_STR_EQUAL(name, "floor")) { *out = floor(x); return 1; }
    if (BU_STR_EQUAL(name, "ceil")) { *out = ceil(x); return 1; }
    if (BU_STR_EQUAL(name, "round")) { *out = (x < 0) ? -floor(-x + 0.5) : floor(x + 0.5); return 1; }
    if (BU_STR_EQUAL(name, "ln")) { *out = log(x); return 1; }
    if (BU_STR_EQUAL(name, "log")) { *out = log10(x); return 1; }
    if (BU_STR_EQUAL(name, "exp")) { *out = exp(x); return 1; }
    if (BU_STR_EQUAL(name, "sqrt")) { *out = sqrt(x); return 1; }
    return 0;
}

/* evaluate a built-in function; returns 1 if handled */
static int
eval_builtin_func(struct scad_state *st, const char *name,
		  struct scad_value *args, size_t n, struct scad_value *out)
{
    double d;

    if (n >= 1 && (args[0].type == SCAD_VAL_NUM || args[0].type == SCAD_VAL_BOOL) &&
	math1(name, args[0].num, &d)) {
	*out = v_num(d);
	return 1;
    }
    if (BU_STR_EQUAL(name, "atan2") && n >= 2) {
	*out = v_num(atan2(v_asnum(&args[0]), v_asnum(&args[1])) * RAD2DEG);
	return 1;
    }
    if (BU_STR_EQUAL(name, "pow") && n >= 2) {
	*out = v_num(pow(v_asnum(&args[0]), v_asnum(&args[1])));
	return 1;
    }
    if ((BU_STR_EQUAL(name, "min") || BU_STR_EQUAL(name, "max"))) {
	int ismin = BU_STR_EQUAL(name, "min");
	double acc = 0;
	int have = 0;
	size_t i;
	/* min(vector) or min(a,b,...) */
	if (n == 1 && args[0].type == SCAD_VAL_VEC) {
	    for (i = 0; i < args[0].vlen; i++) {
		double x = v_asnum(&args[0].items[i]);
		if (!have || (ismin ? x < acc : x > acc)) { acc = x; have = 1; }
	    }
	} else {
	    for (i = 0; i < n; i++) {
		double x = v_asnum(&args[i]);
		if (!have || (ismin ? x < acc : x > acc)) { acc = x; have = 1; }
	    }
	}
	*out = have ? v_num(acc) : v_undef();
	return 1;
    }
    if (BU_STR_EQUAL(name, "norm") && n >= 1 && args[0].type == SCAD_VAL_VEC) {
	double s = 0;
	size_t i;
	for (i = 0; i < args[0].vlen; i++) {
	    double x = v_asnum(&args[0].items[i]);
	    s += x * x;
	}
	*out = v_num(sqrt(s));
	return 1;
    }
    if (BU_STR_EQUAL(name, "cross") && n >= 2 &&
	args[0].type == SCAD_VAL_VEC && args[1].type == SCAD_VAL_VEC) {
	struct scad_value *a = &args[0], *b = &args[1];
	if (a->vlen >= 3 && b->vlen >= 3) {
	    double ax = v_asnum(&a->items[0]), ay = v_asnum(&a->items[1]), az = v_asnum(&a->items[2]);
	    double bx = v_asnum(&b->items[0]), by = v_asnum(&b->items[1]), bz = v_asnum(&b->items[2]);
	    struct scad_value r = v_vec(3);
	    r.items[0] = v_num(ay * bz - az * by);
	    r.items[1] = v_num(az * bx - ax * bz);
	    r.items[2] = v_num(ax * by - ay * bx);
	    *out = r;
	    return 1;
	}
	if (a->vlen >= 2 && b->vlen >= 2) {	/* 2D cross -> scalar */
	    *out = v_num(v_asnum(&a->items[0]) * v_asnum(&b->items[1]) -
			 v_asnum(&a->items[1]) * v_asnum(&b->items[0]));
	    return 1;
	}
    }
    if (BU_STR_EQUAL(name, "len") && n >= 1) {
	if (args[0].type == SCAD_VAL_VEC) { *out = v_num((double)args[0].vlen); return 1; }
	if (args[0].type == SCAD_VAL_STR) { *out = v_num((double)strlen(args[0].str)); return 1; }
	*out = v_undef();
	return 1;
    }
    if (BU_STR_EQUAL(name, "concat")) { *out = builtin_concat(args, n); return 1; }
    if (BU_STR_EQUAL(name, "lookup") && n >= 2) { *out = builtin_lookup(&args[0], &args[1]); return 1; }
    if (BU_STR_EQUAL(name, "str")) {
	struct bu_vls s = BU_VLS_INIT_ZERO;
	size_t i;
	for (i = 0; i < n; i++) value_to_str(&s, &args[i]);
	*out = v_str(bu_vls_cstr(&s));
	bu_vls_free(&s);
	return 1;
    }
    if (BU_STR_EQUAL(name, "chr")) {
	struct bu_vls s = BU_VLS_INIT_ZERO;
	size_t i;
	for (i = 0; i < n; i++) {
	    int c = (int)v_asnum(&args[i]);
	    if (c > 0 && c < 128) bu_vls_putc(&s, (char)c);
	}
	*out = v_str(bu_vls_cstr(&s));
	bu_vls_free(&s);
	return 1;
    }
    if (BU_STR_EQUAL(name, "ord") && n >= 1 && args[0].type == SCAD_VAL_STR) {
	*out = args[0].str[0] ? v_num((double)(unsigned char)args[0].str[0]) : v_undef();
	return 1;
    }
    if (BU_STR_EQUAL(name, "reverse") && n >= 1) {
	if (args[0].type == SCAD_VAL_VEC) {
	    size_t i, m = args[0].vlen;
	    struct scad_value r = v_vec(m);
	    for (i = 0; i < m; i++) r.items[i] = scad_value_copy(&args[0].items[m - 1 - i]);
	    *out = r;
	    return 1;
	}
	if (args[0].type == SCAD_VAL_STR) {
	    size_t i, m = strlen(args[0].str);
	    struct scad_value r = v_str(args[0].str);
	    for (i = 0; i < m; i++) r.str[i] = args[0].str[m - 1 - i];
	    *out = r;
	    return 1;
	}
    }
    if (BU_STR_EQUAL(name, "rands") && n >= 3) {
	double lo = v_asnum(&args[0]), hi = v_asnum(&args[1]);
	int cnt = (int)v_asnum(&args[2]);
	int i;
	struct scad_value r;
	if (cnt < 0) cnt = 0;
	if (n >= 4) scad_rng_state = (uint64_t)(v_asnum(&args[3]) * 2654435761.0) | 1ULL;
	r = v_vec((size_t)cnt);
	for (i = 0; i < cnt; i++) r.items[i] = v_num(lo + (hi - lo) * scad_rng());
	*out = r;
	return 1;
    }
    if (BU_STR_EQUAL(name, "is_undef")) { *out = v_bool(n < 1 || args[0].type == SCAD_VAL_UNDEF); return 1; }
    if (BU_STR_EQUAL(name, "is_bool")) { *out = v_bool(n >= 1 && args[0].type == SCAD_VAL_BOOL); return 1; }
    if (BU_STR_EQUAL(name, "is_num")) { *out = v_bool(n >= 1 && args[0].type == SCAD_VAL_NUM); return 1; }
    if (BU_STR_EQUAL(name, "is_string")) { *out = v_bool(n >= 1 && args[0].type == SCAD_VAL_STR); return 1; }
    if (BU_STR_EQUAL(name, "is_list")) { *out = v_bool(n >= 1 && args[0].type == SCAD_VAL_VEC); return 1; }

    if (st) st->dropped++;
    return 0;
}


/* ------------------------------------------------------------------ */
/* argument binding for user modules/functions                        */
/* ------------------------------------------------------------------ */

/* Evaluate call arguments and bind them to the given parameter list in
 * 'dest'.  Positional args fill params in order; named args override by
 * name.  Extra named args (e.g. $fn) are bound directly. */
static void
bind_params(struct scad_state *st, struct env *callenv, struct env *dest,
	    struct scad_param *params, struct scad_arg *args, size_t nargs)
{
    struct scad_param *p;
    size_t i, posidx = 0;

    /* defaults first */
    for (p = params; p; p = p->next) {
	if (p->def)
	    env_set(dest, p->name, eval_expr(st, callenv, p->def));
	else
	    env_set(dest, p->name, v_undef());
    }
    /* positional then named */
    for (i = 0; i < nargs; i++) {
	if (args[i].name) {
	    env_set(dest, args[i].name, eval_expr(st, callenv, args[i].value));
	} else {
	    /* find the posidx-th parameter */
	    struct scad_param *tp = params;
	    size_t k = 0;
	    while (tp && k < posidx) { tp = tp->next; k++; }
	    if (tp)
		env_set(dest, tp->name, eval_expr(st, callenv, args[i].value));
	    posidx++;
	}
    }
}


/* ------------------------------------------------------------------ */
/* expression evaluation                                              */
/* ------------------------------------------------------------------ */

static void
eval_range(struct scad_state *st, struct env *env, struct scad_expr *e, struct scad_value *out)
{
    double a = v_asnum2(st, env, e->a);
    double c = v_asnum2(st, env, e->c);
    double b = e->b ? v_asnum2(st, env, e->b) : 1.0;
    out->type = SCAD_VAL_RANGE;
    out->range[0] = a;
    out->range[1] = (e->b ? b : 1.0);
    out->range[2] = c;
}

/* helper: eval an expr and coerce to number (declared out of order) */
static double
v_asnum2(struct scad_state *st, struct env *env, struct scad_expr *e)
{
    struct scad_value v = eval_expr(st, env, e);
    double d = v_asnum(&v);
    scad_value_free(&v);
    return d;
}

/* flatten a list-comprehension generator element into 'acc' */
static void
eval_gen(struct scad_state *st, struct env *env, struct scad_expr *e, struct scad_value *acc)
{
    if (!e) return;
    switch (e->kind) {
	case EX_LC_FOR: {
	    /* iterate assigns (nested for multiple) */
	    struct scad_assign *as = e->assigns;
	    /* build nested loops recursively over the assign list */
	    /* helper via recursion on a copy of the chain */
	    /* We implement single/multi by recursing on 'as->next'. */
	    if (!as) { eval_gen(st, env, e->a, acc); return; }
	    {
		struct scad_value iter = eval_expr(st, env, as->value);
		struct env *le = env_new(env);
		/* temporarily detach the head to recurse on the rest */
		struct scad_expr inner;
		memset(&inner, 0, sizeof(inner));
		inner.kind = EX_LC_FOR;
		inner.assigns = as->next;
		inner.a = e->a;
		if (iter.type == SCAD_VAL_RANGE) {
		    double s = iter.range[0], step = iter.range[1], end = iter.range[2];
		    if (step == 0) step = 1;
		    if (step > 0)
			for (; s <= end + 1e-9; s += step) {
			    env_set(le, as->name, v_num(s));
			    if (as->next) eval_gen(st, le, &inner, acc);
			    else eval_gen(st, le, e->a, acc);
			}
		    else
			for (; s >= end - 1e-9; s += step) {
			    env_set(le, as->name, v_num(s));
			    if (as->next) eval_gen(st, le, &inner, acc);
			    else eval_gen(st, le, e->a, acc);
			}
		} else if (iter.type == SCAD_VAL_VEC) {
		    size_t i;
		    for (i = 0; i < iter.vlen; i++) {
			env_set(le, as->name, scad_value_copy(&iter.items[i]));
			if (as->next) eval_gen(st, le, &inner, acc);
			else eval_gen(st, le, e->a, acc);
		    }
		}
		scad_value_free(&iter);
		env_free(le);
	    }
	    return;
	}
	case EX_LC_IF: {
	    struct scad_value c = eval_expr(st, env, e->a);
	    int t = v_true(&c);
	    scad_value_free(&c);
	    if (t) eval_gen(st, env, e->b, acc);
	    else if (e->c) eval_gen(st, env, e->c, acc);
	    return;
	}
	case EX_LC_LET: {
	    struct env *le = env_new(env);
	    struct scad_assign *as;
	    for (as = e->assigns; as; as = as->next)
		env_set(le, as->name, eval_expr(st, le, as->value));
	    eval_gen(st, le, e->a, acc);
	    env_free(le);
	    return;
	}
	case EX_EACH: {
	    struct scad_value v = eval_expr(st, env, e->a);
	    if (v.type == SCAD_VAL_VEC) {
		size_t i;
		size_t base = acc->vlen;
		acc->items = (struct scad_value *)bu_realloc(acc->items,
			     (base + v.vlen) * sizeof(struct scad_value), "scad lc each");
		for (i = 0; i < v.vlen; i++)
		    acc->items[base + i] = scad_value_copy(&v.items[i]);
		acc->vlen = base + v.vlen;
	    } else if (v.type == SCAD_VAL_RANGE) {
		double s = v.range[0], step = v.range[1], end = v.range[2];
		if (step == 0) step = 1;
		for (; (step > 0) ? (s <= end + 1e-9) : (s >= end - 1e-9); s += step) {
		    acc->items = (struct scad_value *)bu_realloc(acc->items,
				 (acc->vlen + 1) * sizeof(struct scad_value), "scad lc each range");
		    acc->items[acc->vlen++] = v_num(s);
		}
	    } else if (v.type != SCAD_VAL_UNDEF) {
		/* each of a non-list yields the single element */
		acc->items = (struct scad_value *)bu_realloc(acc->items,
			     (acc->vlen + 1) * sizeof(struct scad_value), "scad lc each scalar");
		acc->items[acc->vlen++] = scad_value_copy(&v);
	    }
	    scad_value_free(&v);
	    return;
	}
	default: {
	    /* a plain element */
	    struct scad_value v = eval_expr(st, env, e);
	    acc->items = (struct scad_value *)bu_realloc(acc->items,
			 (acc->vlen + 1) * sizeof(struct scad_value), "scad lc elem");
	    acc->items[acc->vlen++] = v;
	    return;
	}
    }
}

static struct scad_value
eval_vector(struct scad_state *st, struct env *env, struct scad_expr *e)
{
    struct scad_value acc;
    size_t i;
    memset(&acc, 0, sizeof(acc));
    acc.type = SCAD_VAL_VEC;
    for (i = 0; i < e->nitems; i++)
	eval_gen(st, env, e->items[i], &acc);
    return acc;
}

static int
val_equal(const struct scad_value *a, const struct scad_value *b)
{
    size_t i;
    if (a->type != b->type)
	return 0;		/* unlike types never compare equal */
    switch (a->type) {
	case SCAD_VAL_UNDEF: return 1;
	case SCAD_VAL_NUM:
	case SCAD_VAL_BOOL: return a->num == b->num;
	case SCAD_VAL_STR: return BU_STR_EQUAL(a->str, b->str);
	case SCAD_VAL_VEC:
	    if (a->vlen != b->vlen) return 0;
	    for (i = 0; i < a->vlen; i++)
		if (!val_equal(&a->items[i], &b->items[i])) return 0;
	    return 1;
	default: return 0;
    }
}

static struct scad_value
eval_binary(struct scad_state *st, struct env *env, struct scad_expr *e)
{
    struct scad_value a, b, r;
    size_t i;

    /* short-circuit logicals */
    if (e->op == SCAD_OP_AND) {
	a = eval_expr(st, env, e->a);
	if (!v_true(&a)) { scad_value_free(&a); return v_bool(0); }
	scad_value_free(&a);
	b = eval_expr(st, env, e->b);
	r = v_bool(v_true(&b));
	scad_value_free(&b);
	return r;
    }
    if (e->op == SCAD_OP_OR) {
	a = eval_expr(st, env, e->a);
	if (v_true(&a)) { scad_value_free(&a); return v_bool(1); }
	scad_value_free(&a);
	b = eval_expr(st, env, e->b);
	r = v_bool(v_true(&b));
	scad_value_free(&b);
	return r;
    }

    a = eval_expr(st, env, e->a);
    b = eval_expr(st, env, e->b);
    r = v_undef();

    switch (e->op) {
	case SCAD_OP_EQ: r = v_bool(val_equal(&a, &b)); break;
	case SCAD_OP_NE: r = v_bool(!val_equal(&a, &b)); break;
	case SCAD_OP_LT: r = v_bool(v_asnum(&a) < v_asnum(&b)); break;
	case SCAD_OP_LE: r = v_bool(v_asnum(&a) <= v_asnum(&b)); break;
	case SCAD_OP_GT: r = v_bool(v_asnum(&a) > v_asnum(&b)); break;
	case SCAD_OP_GE: r = v_bool(v_asnum(&a) >= v_asnum(&b)); break;
	case SCAD_OP_ADD:
	case SCAD_OP_SUB:
	    if (a.type == SCAD_VAL_VEC && b.type == SCAD_VAL_VEC) {
		size_t m = a.vlen < b.vlen ? a.vlen : b.vlen;
		r = v_vec(m);
		for (i = 0; i < m; i++)
		    r.items[i] = v_num(e->op == SCAD_OP_ADD
			? v_asnum(&a.items[i]) + v_asnum(&b.items[i])
			: v_asnum(&a.items[i]) - v_asnum(&b.items[i]));
	    } else {
		r = v_num(e->op == SCAD_OP_ADD ? v_asnum(&a) + v_asnum(&b)
					       : v_asnum(&a) - v_asnum(&b));
	    }
	    break;
	case SCAD_OP_MUL:
	    if (a.type == SCAD_VAL_VEC && b.type == SCAD_VAL_VEC) {
		/* dot product */
		double s = 0;
		size_t m = a.vlen < b.vlen ? a.vlen : b.vlen;
		for (i = 0; i < m; i++) s += v_asnum(&a.items[i]) * v_asnum(&b.items[i]);
		r = v_num(s);
	    } else if (a.type == SCAD_VAL_VEC) {
		double s = v_asnum(&b);
		r = v_vec(a.vlen);
		for (i = 0; i < a.vlen; i++) r.items[i] = v_num(v_asnum(&a.items[i]) * s);
	    } else if (b.type == SCAD_VAL_VEC) {
		double s = v_asnum(&a);
		r = v_vec(b.vlen);
		for (i = 0; i < b.vlen; i++) r.items[i] = v_num(v_asnum(&b.items[i]) * s);
	    } else {
		r = v_num(v_asnum(&a) * v_asnum(&b));
	    }
	    break;
	case SCAD_OP_DIV:
	    if (a.type == SCAD_VAL_VEC) {
		double s = v_asnum(&b);
		r = v_vec(a.vlen);
		for (i = 0; i < a.vlen; i++) r.items[i] = v_num(v_asnum(&a.items[i]) / s);
	    } else if (b.type == SCAD_VAL_VEC) {
		r = v_undef();		/* scalar / vector is undefined */
	    } else {
		r = v_num(v_asnum(&a) / v_asnum(&b));
	    }
	    break;
	case SCAD_OP_MOD:
	    r = v_num(fmod(v_asnum(&a), v_asnum(&b)));
	    break;
	default: break;
    }
    scad_value_free(&a);
    scad_value_free(&b);
    return r;
}

/* call a user function (recursive) */
static struct scad_value
eval_user_func(struct scad_state *st, struct env *callenv, struct funcdef *fd,
	       struct scad_expr *call)
{
    struct env *fe;
    struct scad_value r;
    if (++st->eval_depth > SCAD_MAX_DEPTH) {
	st->eval_depth--;
	bu_log("scad: recursion limit exceeded in function %s\n", fd->name);
	st->aborted = 1;
	return v_undef();
    }
    fe = env_new(callenv);
    bind_params(st, callenv, fe, fd->params, call->args, call->nargs);
    r = eval_expr(st, fe, fd->body);
    env_free(fe);
    st->eval_depth--;
    return r;
}

static struct scad_value
eval_call(struct scad_state *st, struct env *env, struct scad_expr *e)
{
    struct funcdef *fd = env_find_func(env, e->name);
    struct scad_value out = v_undef();
    struct scad_value *argv;
    size_t i;

    if (fd)
	return eval_user_func(st, env, fd, e);

    /* built-in: evaluate args into an array */
    argv = e->nargs ? (struct scad_value *)bu_calloc(e->nargs, sizeof(struct scad_value), "scad argv") : NULL;
    for (i = 0; i < e->nargs; i++)
	argv[i] = eval_expr(st, env, e->args[i].value);

    if (!eval_builtin_func(st, e->name, argv, e->nargs, &out)) {
	static int warned = 0;
	if (warned < 20) {
	    bu_log("scad: unknown function '%s' -> undef\n", e->name);
	    warned++;
	}
	out = v_undef();
    }

    for (i = 0; i < e->nargs; i++) scad_value_free(&argv[i]);
    if (argv) bu_free(argv, "scad argv");
    return out;
}

static struct scad_value
eval_expr(struct scad_state *st, struct env *env, struct scad_expr *e)
{
    if (!e || st->aborted) return v_undef();

    switch (e->kind) {
	case EX_NUM: return v_num(e->num);
	case EX_BOOL: return v_bool((int)e->num);
	case EX_STR: return v_str(e->str);
	case EX_UNDEF: return v_undef();
	case EX_VECTOR: return eval_vector(st, env, e);
	case EX_RANGE: {
	    struct scad_value r = v_undef();
	    eval_range(st, env, e, &r);
	    return r;
	}
	case EX_IDENT: {
	    struct scad_value v;
	    if (BU_STR_EQUAL(e->name, "PI")) return v_num(M_PI);
	    if (BU_STR_EQUAL(e->name, "undef")) return v_undef();
	    if (env_get(env, e->name, &v)) return v;
	    return v_undef();
	}
	case EX_UNARY: {
	    struct scad_value a = eval_expr(st, env, e->a);
	    struct scad_value r;
	    if (e->op == SCAD_OP_NOT) {
		r = v_bool(!v_true(&a));
	    } else {	/* NEG */
		if (a.type == SCAD_VAL_VEC) {
		    size_t i;
		    r = v_vec(a.vlen);
		    for (i = 0; i < a.vlen; i++) r.items[i] = v_num(-v_asnum(&a.items[i]));
		} else {
		    r = v_num(-v_asnum(&a));
		}
	    }
	    scad_value_free(&a);
	    return r;
	}
	case EX_BINARY: return eval_binary(st, env, e);
	case EX_TERNARY: {
	    struct scad_value c = eval_expr(st, env, e->a);
	    int t = v_true(&c);
	    scad_value_free(&c);
	    return eval_expr(st, env, t ? e->b : e->c);
	}
	case EX_INDEX: {
	    struct scad_value a = eval_expr(st, env, e->a);
	    struct scad_value idx = eval_expr(st, env, e->b);
	    struct scad_value r = v_undef();
	    long k = (long)v_asnum(&idx);
	    if (a.type == SCAD_VAL_VEC && k >= 0 && (size_t)k < a.vlen)
		r = scad_value_copy(&a.items[k]);
	    else if (a.type == SCAD_VAL_STR && k >= 0 && (size_t)k < strlen(a.str)) {
		char buf[2]; buf[0] = a.str[k]; buf[1] = '\0';
		r = v_str(buf);
	    }
	    scad_value_free(&a);
	    scad_value_free(&idx);
	    return r;
	}
	case EX_MEMBER: {
	    struct scad_value a = eval_expr(st, env, e->a);
	    struct scad_value r = v_undef();
	    int k = -1;
	    if (BU_STR_EQUAL(e->name, "x")) k = 0;
	    else if (BU_STR_EQUAL(e->name, "y")) k = 1;
	    else if (BU_STR_EQUAL(e->name, "z")) k = 2;
	    if (a.type == SCAD_VAL_VEC && k >= 0 && (size_t)k < a.vlen)
		r = scad_value_copy(&a.items[k]);
	    scad_value_free(&a);
	    return r;
	}
	case EX_CALL: return eval_call(st, env, e);
	case EX_LET: {
	    struct env *le = env_new(env);
	    struct scad_assign *as;
	    struct scad_value r;
	    for (as = e->assigns; as; as = as->next)
		env_set(le, as->name, eval_expr(st, le, as->value));
	    r = eval_expr(st, le, e->a);
	    env_free(le);
	    return r;
	}
	default:
	    return v_undef();
    }
}


/* ------------------------------------------------------------------ */
/* transform matrix builders                                          */
/* ------------------------------------------------------------------ */

static void
mat_translate(mat_t m, double x, double y, double z)
{
    MAT_IDN(m);
    m[MDX] = x; m[MDY] = y; m[MDZ] = z;
}

static void
mat_scale3(mat_t m, double x, double y, double z)
{
    MAT_IDN(m);
    m[0] = x; m[5] = y; m[10] = z;
}

static void
mat_rot_axis(mat_t m, char axis, double deg)
{
    double c = cos(deg * DEG2RAD), s = sin(deg * DEG2RAD);
    MAT_IDN(m);
    switch (axis) {
	case 'x': m[5] = c; m[6] = -s; m[9] = s; m[10] = c; break;
	case 'y': m[0] = c; m[2] = s; m[8] = -s; m[10] = c; break;
	case 'z': m[0] = c; m[1] = -s; m[4] = s; m[5] = c; break;
	default: break;
    }
}

/* rotate([x,y,z]) = Rz*Ry*Rx */
static void
mat_rot_xyz(mat_t m, double x, double y, double z)
{
    mat_t rx, ry, rz, tmp;
    mat_rot_axis(rx, 'x', x);
    mat_rot_axis(ry, 'y', y);
    mat_rot_axis(rz, 'z', z);
    bn_mat_mul(tmp, rz, ry);
    bn_mat_mul(m, tmp, rx);
}

/* rotate(a, [x,y,z]) about arbitrary axis (Rodrigues) */
static void
mat_rot_arbitrary(mat_t m, double deg, double x, double y, double z)
{
    double len = sqrt(x * x + y * y + z * z);
    double c, s, t;
    if (len < VDIVIDE_TOL) { MAT_IDN(m); return; }
    x /= len; y /= len; z /= len;
    c = cos(deg * DEG2RAD); s = sin(deg * DEG2RAD); t = 1.0 - c;
    MAT_IDN(m);
    m[0] = t * x * x + c;      m[1] = t * x * y - s * z;  m[2] = t * x * z + s * y;
    m[4] = t * x * y + s * z;  m[5] = t * y * y + c;      m[6] = t * y * z - s * x;
    m[8] = t * x * z - s * y;  m[9] = t * y * z + s * x;  m[10] = t * z * z + c;
}

/* mirror([x,y,z]) Householder reflection */
static void
mat_mirror(mat_t m, double x, double y, double z)
{
    double len = sqrt(x * x + y * y + z * z);
    if (len < VDIVIDE_TOL) { MAT_IDN(m); return; }
    x /= len; y /= len; z /= len;
    MAT_IDN(m);
    m[0] = 1 - 2 * x * x; m[1] = -2 * x * y;    m[2] = -2 * x * z;
    m[4] = -2 * x * y;    m[5] = 1 - 2 * y * y; m[6] = -2 * y * z;
    m[8] = -2 * x * z;    m[9] = -2 * y * z;    m[10] = 1 - 2 * z * z;
}

/* multmatrix(m): m is a vector of rows */
static void
mat_from_value(mat_t out, const struct scad_value *v)
{
    size_t r, cc;
    MAT_IDN(out);
    if (!v || v->type != SCAD_VAL_VEC) return;
    for (r = 0; r < v->vlen && r < 4; r++) {
	struct scad_value *row = &v->items[r];
	if (row->type != SCAD_VAL_VEC) continue;
	for (cc = 0; cc < row->vlen && cc < 4; cc++)
	    out[r * 4 + cc] = v_asnum(&row->items[cc]);
    }
    /* force affine bottom row */
    out[12] = out[13] = out[14] = 0.0;
    out[15] = 1.0;
}


/* ------------------------------------------------------------------ */
/* geometry-producing builtins                                        */
/* ------------------------------------------------------------------ */

/* fetch a named arg value from a builtin's evaluated argset */
struct argset {
    struct scad_value *pos;	/* positional in order */
    size_t npos;
    char **names;		/* named keys */
    struct scad_value *nvals;
    size_t nnamed;
};

static struct scad_value *
arg_named(struct argset *as, const char *name)
{
    size_t i;
    for (i = 0; i < as->nnamed; i++)
	if (BU_STR_EQUAL(as->names[i], name)) return &as->nvals[i];
    return NULL;
}

/* positional-or-named lookup mirroring OpenSCAD binding */
static struct scad_value *
arg_get(struct argset *as, size_t posidx, const char *name)
{
    struct scad_value *v = name ? arg_named(as, name) : NULL;
    if (v) return v;
    if (posidx < as->npos) return &as->pos[posidx];
    return NULL;
}

static void
argset_build(struct scad_state *st, struct env *env, struct scad_arg *args,
	     size_t nargs, struct argset *as)
{
    size_t i, p = 0, m = 0;
    memset(as, 0, sizeof(*as));
    if (!nargs) return;
    as->pos = (struct scad_value *)bu_calloc(nargs, sizeof(struct scad_value), "argset pos");
    as->nvals = (struct scad_value *)bu_calloc(nargs, sizeof(struct scad_value), "argset nvals");
    as->names = (char **)bu_calloc(nargs, sizeof(char *), "argset names");
    for (i = 0; i < nargs; i++) {
	struct scad_value v = eval_expr(st, env, args[i].value);
	if (args[i].name) {
	    as->names[m] = args[i].name;
	    as->nvals[m] = v;
	    m++;
	} else {
	    as->pos[p++] = v;
	}
    }
    as->npos = p;
    as->nnamed = m;
}

static void
argset_free(struct argset *as)
{
    size_t i;
    for (i = 0; i < as->npos; i++) scad_value_free(&as->pos[i]);
    for (i = 0; i < as->nnamed; i++) scad_value_free(&as->nvals[i]);
    if (as->pos) bu_free(as->pos, "argset pos");
    if (as->nvals) bu_free(as->nvals, "argset nvals");
    if (as->names) bu_free(as->names, "argset names");
}

/* read $fn/$fa/$fs applying option fallbacks */
static void
read_fragments(struct scad_state *st, struct env *env, int *fn, double *fa, double *fs)
{
    double f = env_num(env, "$fn", 0.0);
    *fn = (int)f;
    *fa = env_num(env, "$fa", 12.0);
    *fs = env_num(env, "$fs", 2.0);
    if (*fn == 0 && st->opts) {
	if (st->opts->fn > 0) *fn = st->opts->fn;
    }
    if (st->opts) {
	if (st->opts->fa > 0.0 && env_num(env, "$fa", -1.0) < 0.0) *fa = st->opts->fa;
	if (st->opts->fs > 0.0 && env_num(env, "$fs", -1.0) < 0.0) *fs = st->opts->fs;
    }
}

/* build the polygon/polyhedron point + face arrays from values */
static void
geom_load_faces(struct scad_geom *g, struct scad_value *pts, struct scad_value *faces, int is2d)
{
    size_t i, j, total = 0;
    if (!pts || pts->type != SCAD_VAL_VEC) return;
    g->npts = pts->vlen;
    g->pts = (double *)bu_calloc(g->npts ? g->npts * 3 : 1, sizeof(double), "scad pts");
    for (i = 0; i < g->npts; i++) {
	vect_t p;
	v_to_vec3(&pts->items[i], p, 0.0);
	g->pts[i * 3 + 0] = p[0];
	g->pts[i * 3 + 1] = p[1];
	g->pts[i * 3 + 2] = is2d ? 0.0 : p[2];
    }
    if (!faces || faces->type != SCAD_VAL_VEC) {
	/* polygon default path: single loop over all points in order */
	if (is2d && g->npts >= 3) {
	    g->nfaces = 1;
	    g->flen = (size_t *)bu_calloc(1, sizeof(size_t), "scad flen");
	    g->flen[0] = g->npts;
	    g->fidx = (int *)bu_calloc(g->npts, sizeof(int), "scad fidx");
	    for (i = 0; i < g->npts; i++) g->fidx[i] = (int)i;
	}
	return;
    }
    g->nfaces = faces->vlen;
    g->flen = (size_t *)bu_calloc(g->nfaces ? g->nfaces : 1, sizeof(size_t), "scad flen");
    for (i = 0; i < g->nfaces; i++) {
	struct scad_value *f = &faces->items[i];
	g->flen[i] = (f->type == SCAD_VAL_VEC) ? f->vlen : 0;
	total += g->flen[i];
    }
    g->fidx = (int *)bu_calloc(total ? total : 1, sizeof(int), "scad fidx");
    {
	size_t k = 0;
	for (i = 0; i < g->nfaces; i++) {
	    struct scad_value *f = &faces->items[i];
	    if (f->type != SCAD_VAL_VEC) continue;
	    for (j = 0; j < f->vlen; j++)
		g->fidx[k++] = (int)v_asnum(&f->items[j]);
	}
    }
}

/* named color table (subset of the CSS/OpenSCAD names) */
static int
color_lookup(const char *name, double rgb[3])
{
    struct { const char *n; double r, g, b; } tbl[] = {
	{"red", 1, 0, 0}, {"green", 0, 0.5, 0}, {"lime", 0, 1, 0},
	{"blue", 0, 0, 1}, {"yellow", 1, 1, 0}, {"cyan", 0, 1, 1},
	{"magenta", 1, 0, 1}, {"white", 1, 1, 1}, {"black", 0, 0, 0},
	{"gray", 0.5, 0.5, 0.5}, {"grey", 0.5, 0.5, 0.5},
	{"darkgray", 0.66, 0.66, 0.66}, {"darkgrey", 0.66, 0.66, 0.66},
	{"lightgray", 0.83, 0.83, 0.83}, {"lightgrey", 0.83, 0.83, 0.83},
	{"orange", 1, 0.65, 0}, {"purple", 0.5, 0, 0.5},
	{"brown", 0.65, 0.16, 0.16}, {"pink", 1, 0.75, 0.80},
	{"steelblue", 0.27, 0.51, 0.71}, {"silver", 0.75, 0.75, 0.75},
	{"gold", 1, 0.84, 0}, {"navy", 0, 0, 0.5}, {"teal", 0, 0.5, 0.5},
	{NULL, 0, 0, 0}
    };
    int i;
    for (i = 0; tbl[i].n; i++) {
	if (BU_STR_EQUIV(tbl[i].n, name)) {
	    rgb[0] = tbl[i].r; rgb[1] = tbl[i].g; rgb[2] = tbl[i].b;
	    return 1;
	}
    }
    return 0;
}

static int
hexpair(const char *s)
{
    int hi = isdigit((unsigned char)s[0]) ? s[0] - '0' : tolower(s[0]) - 'a' + 10;
    int lo = isdigit((unsigned char)s[1]) ? s[1] - '0' : tolower(s[1]) - 'a' + 10;
    return hi * 16 + lo;
}


/* forward: instantiate the children passed to a user module */
static void exec_children(struct scad_state *st, struct env *env,
			  struct scad_geom *into, long which);


/* dispatch a builtin module instantiation.  Returns 1 if the name was a
 * recognized builtin (whether or not it produced geometry). */
static int
exec_builtin(struct scad_state *st, struct env *env, const char *name,
	     struct scad_arg *args, size_t nargs,
	     struct scad_stmt **body, size_t nbody, struct scad_geom *into)
{
    struct argset as;
    struct scad_geom *g = NULL;
    int handled = 1;

    /* ---- operators that wrap children with a transform ---------- */
    if (BU_STR_EQUAL(name, "translate") || BU_STR_EQUAL(name, "rotate") ||
	BU_STR_EQUAL(name, "scale") || BU_STR_EQUAL(name, "mirror") ||
	BU_STR_EQUAL(name, "multmatrix") || BU_STR_EQUAL(name, "resize")) {
	vect_t v;
	argset_build(st, env, args, nargs, &as);
	g = scad_geom_new(G_TRANSFORM);
	MAT_IDN(g->mat);
	if (BU_STR_EQUAL(name, "translate")) {
	    v_to_vec3(arg_get(&as, 0, "v"), v, 0.0);
	    mat_translate(g->mat, v[0], v[1], v[2]);
	} else if (BU_STR_EQUAL(name, "scale")) {
	    v_to_vec3(arg_get(&as, 0, "v"), v, 1.0);
	    mat_scale3(g->mat, v[0], v[1], v[2]);
	} else if (BU_STR_EQUAL(name, "mirror")) {
	    v_to_vec3(arg_get(&as, 0, "v"), v, 0.0);
	    mat_mirror(g->mat, v[0], v[1], v[2]);
	} else if (BU_STR_EQUAL(name, "multmatrix")) {
	    mat_from_value(g->mat, arg_get(&as, 0, "m"));
	} else if (BU_STR_EQUAL(name, "resize")) {
	    /* resize needs the child bounding box; unsupported -> identity */
	    st->dropped++;
	} else {	/* rotate */
	    struct scad_value *a0 = arg_get(&as, 0, "a");
	    struct scad_value *ax = arg_get(&as, 1, "v");
	    if (a0 && a0->type == SCAD_VAL_VEC) {
		vect_t r;
		v_to_vec3(a0, r, 0.0);
		mat_rot_xyz(g->mat, r[0], r[1], r[2]);
	    } else if (ax && ax->type == SCAD_VAL_VEC) {
		vect_t r;
		v_to_vec3(ax, r, 0.0);
		mat_rot_arbitrary(g->mat, a0 ? v_asnum(a0) : 0.0, r[0], r[1], r[2]);
	    } else {
		mat_rot_axis(g->mat, 'z', a0 ? v_asnum(a0) : 0.0);
	    }
	}
	argset_free(&as);
	exec_body(st, env, body, nbody, g);
	scad_geom_add(into, g);
	return 1;
    }

    if (BU_STR_EQUAL(name, "color")) {
	struct scad_value *c;
	argset_build(st, env, args, nargs, &as);
	g = scad_geom_new(G_COLOR);
	g->rgba[0] = g->rgba[1] = g->rgba[2] = 0.5;
	g->rgba[3] = 1.0;
	c = arg_get(&as, 0, "c");
	if (c && c->type == SCAD_VAL_STR) {
	    double rgb[3];
	    if (c->str[0] == '#') {
		const char *h = c->str + 1;
		size_t L = strlen(h);
		if (L >= 6) {
		    g->rgba[0] = hexpair(h) / 255.0;
		    g->rgba[1] = hexpair(h + 2) / 255.0;
		    g->rgba[2] = hexpair(h + 4) / 255.0;
		    if (L >= 8) g->rgba[3] = hexpair(h + 6) / 255.0;
		}
	    } else if (color_lookup(c->str, rgb)) {
		g->rgba[0] = rgb[0]; g->rgba[1] = rgb[1]; g->rgba[2] = rgb[2];
	    }
	} else if (c && c->type == SCAD_VAL_VEC) {
	    vect_t rc;
	    v_to_vec3(c, rc, 1.0);
	    g->rgba[0] = rc[0]; g->rgba[1] = rc[1]; g->rgba[2] = rc[2];
	    if (c->vlen >= 4) g->rgba[3] = v_asnum(&c->items[3]);
	}
	{
	    struct scad_value *al = arg_get(&as, 1, "alpha");
	    if (al && (al->type == SCAD_VAL_NUM)) g->rgba[3] = al->num;
	}
	argset_free(&as);
	exec_body(st, env, body, nbody, g);
	scad_geom_add(into, g);
	return 1;
    }

    /* ---- boolean / grouping operators --------------------------- */
    if (BU_STR_EQUAL(name, "union") || BU_STR_EQUAL(name, "group") ||
	BU_STR_EQUAL(name, "render") || BU_STR_EQUAL(name, "assign")) {
	g = scad_geom_new(BU_STR_EQUAL(name, "render") ? G_RENDER : G_GROUP);
	if (BU_STR_EQUAL(name, "assign")) {
	    /* deprecated: named args become local bindings */
	    struct env *ae = env_new(env);
	    size_t i;
	    for (i = 0; i < nargs; i++)
		if (args[i].name)
		    env_set(ae, args[i].name, eval_expr(st, env, args[i].value));
	    exec_body(st, ae, body, nbody, g);
	    env_free(ae);
	} else {
	    exec_body(st, env, body, nbody, g);
	}
	scad_geom_add(into, g);
	return 1;
    }
    if (BU_STR_EQUAL(name, "difference")) {
	g = scad_geom_new(G_DIFFERENCE);
	exec_body(st, env, body, nbody, g);
	scad_geom_add(into, g);
	return 1;
    }
    if (BU_STR_EQUAL(name, "intersection")) {
	g = scad_geom_new(G_INTERSECTION);
	exec_body(st, env, body, nbody, g);
	scad_geom_add(into, g);
	return 1;
    }
    if (BU_STR_EQUAL(name, "hull")) {
	g = scad_geom_new(G_HULL);
	exec_body(st, env, body, nbody, g);
	scad_geom_add(into, g);
	return 1;
    }
    if (BU_STR_EQUAL(name, "minkowski")) {
	/* unsupported: keep only the first child as an approximation */
	g = scad_geom_new(G_GROUP);
	exec_body(st, env, body, nbody, g);
	if (g->nkids > 1) {
	    size_t i;
	    for (i = 1; i < g->nkids; i++) scad_geom_free(g->kids[i]);
	    g->nkids = 1;
	}
	bu_log("scad: minkowski() is not supported; using base child only\n");
	st->dropped++;
	scad_geom_add(into, g);
	return 1;
    }

    /* ---- extrusions --------------------------------------------- */
    if (BU_STR_EQUAL(name, "linear_extrude")) {
	struct scad_value *sc;
	argset_build(st, env, args, nargs, &as);
	g = scad_geom_new(G_LINEXT);
	g->ext_height = v_asnum(arg_get(&as, 0, "height"));
	{
	    struct scad_value *ce = arg_get(&as, 1, "center");
	    g->ext_center = ce ? v_true(ce) : 0;
	}
	g->ext_twist = v_asnum(arg_named(&as, "twist"));
	g->ext_slices = (int)v_asnum(arg_named(&as, "slices"));
	g->ext_scale[0] = g->ext_scale[1] = 1.0;
	sc = arg_named(&as, "scale");
	if (sc) {
	    if (sc->type == SCAD_VAL_VEC) {
		if (sc->vlen > 0) g->ext_scale[0] = v_asnum(&sc->items[0]);
		g->ext_scale[1] = sc->vlen > 1 ? v_asnum(&sc->items[1]) : g->ext_scale[0];
	    } else {
		g->ext_scale[0] = g->ext_scale[1] = v_asnum(sc);
	    }
	}
	read_fragments(st, env, &g->fn, &g->fa, &g->fs);
	argset_free(&as);
	exec_body(st, env, body, nbody, g);
	scad_geom_add(into, g);
	return 1;
    }
    if (BU_STR_EQUAL(name, "rotate_extrude")) {
	argset_build(st, env, args, nargs, &as);
	g = scad_geom_new(G_ROTEXT);
	{
	    struct scad_value *a0 = arg_get(&as, 0, "angle");
	    g->ext_angle = a0 ? v_asnum(a0) : 360.0;
	}
	read_fragments(st, env, &g->fn, &g->fa, &g->fs);
	argset_free(&as);
	exec_body(st, env, body, nbody, g);
	scad_geom_add(into, g);
	return 1;
    }

    /* ---- 3D primitives ------------------------------------------ */
    if (BU_STR_EQUAL(name, "cube")) {
	struct scad_value *sz, *ce;
	argset_build(st, env, args, nargs, &as);
	g = scad_geom_new(G_PRIM);
	g->prim = PR_CUBE;
	sz = arg_get(&as, 0, "size");
	if (sz) v_to_vec3(sz, g->size, 1.0); else VSET(g->size, 1, 1, 1);
	ce = arg_get(&as, 1, "center");
	g->center = ce ? v_true(ce) : 0;
	argset_free(&as);
	scad_geom_add(into, g);
	return 1;
    }
    if (BU_STR_EQUAL(name, "sphere")) {
	struct scad_value *r, *dv;
	argset_build(st, env, args, nargs, &as);
	g = scad_geom_new(G_PRIM);
	g->prim = PR_SPHERE;
	r = arg_get(&as, 0, "r");
	dv = arg_named(&as, "d");
	g->r1 = dv ? v_asnum(dv) / 2.0 : (r ? v_asnum(r) : 1.0);
	read_fragments(st, env, &g->fn, &g->fa, &g->fs);
	argset_free(&as);
	scad_geom_add(into, g);
	return 1;
    }
    if (BU_STR_EQUAL(name, "cylinder")) {
	struct scad_value *hv, *r, *r1, *r2, *dd, *d1, *d2, *ce;
	argset_build(st, env, args, nargs, &as);
	g = scad_geom_new(G_PRIM);
	g->prim = PR_CYLINDER;
	hv = arg_get(&as, 0, "h");
	g->h = hv ? v_asnum(hv) : 1.0;
	r = arg_named(&as, "r");
	r1 = arg_get(&as, 1, "r1");
	r2 = arg_get(&as, 2, "r2");
	dd = arg_named(&as, "d");
	d1 = arg_named(&as, "d1");
	d2 = arg_named(&as, "d2");
	g->r1 = g->r2 = 1.0;
	if (r) { g->r1 = g->r2 = v_asnum(r); }
	if (dd) { g->r1 = g->r2 = v_asnum(dd) / 2.0; }
	if (r1) g->r1 = v_asnum(r1);
	if (r2) g->r2 = v_asnum(r2);
	if (d1) g->r1 = v_asnum(d1) / 2.0;
	if (d2) g->r2 = v_asnum(d2) / 2.0;
	ce = arg_get(&as, 3, "center");	/* center is positional index 3 */
	g->center = ce ? v_true(ce) : 0;
	read_fragments(st, env, &g->fn, &g->fa, &g->fs);
	argset_free(&as);
	scad_geom_add(into, g);
	return 1;
    }
    if (BU_STR_EQUAL(name, "polyhedron")) {
	argset_build(st, env, args, nargs, &as);
	g = scad_geom_new(G_PRIM);
	g->prim = PR_POLYHEDRON;
	{
	    struct scad_value *pts = arg_get(&as, 0, "points");
	    struct scad_value *fac = arg_get(&as, 1, "faces");
	    if (!fac) fac = arg_named(&as, "triangles");	/* legacy */
	    geom_load_faces(g, pts, fac, 0);
	}
	argset_free(&as);
	scad_geom_add(into, g);
	return 1;
    }

    /* ---- 2D primitives (only meaningful inside an extrude) ------- */
    if (BU_STR_EQUAL(name, "square")) {
	struct scad_value *sz, *ce;
	argset_build(st, env, args, nargs, &as);
	g = scad_geom_new(G_PRIM);
	g->prim = PR_SQUARE;
	g->is2d = 1;
	sz = arg_get(&as, 0, "size");
	if (sz) v_to_vec3(sz, g->size, 1.0); else VSET(g->size, 1, 1, 0);
	ce = arg_get(&as, 1, "center");
	g->center = ce ? v_true(ce) : 0;
	argset_free(&as);
	scad_geom_add(into, g);
	return 1;
    }
    if (BU_STR_EQUAL(name, "circle")) {
	struct scad_value *r, *dv;
	argset_build(st, env, args, nargs, &as);
	g = scad_geom_new(G_PRIM);
	g->prim = PR_CIRCLE;
	g->is2d = 1;
	r = arg_get(&as, 0, "r");
	dv = arg_named(&as, "d");
	g->r1 = dv ? v_asnum(dv) / 2.0 : (r ? v_asnum(r) : 1.0);
	read_fragments(st, env, &g->fn, &g->fa, &g->fs);
	argset_free(&as);
	scad_geom_add(into, g);
	return 1;
    }
    if (BU_STR_EQUAL(name, "polygon")) {
	argset_build(st, env, args, nargs, &as);
	g = scad_geom_new(G_PRIM);
	g->prim = PR_POLYGON;
	g->is2d = 1;
	{
	    struct scad_value *pts = arg_get(&as, 0, "points");
	    struct scad_value *paths = arg_get(&as, 1, "paths");
	    geom_load_faces(g, pts, (paths && paths->type == SCAD_VAL_VEC) ? paths : NULL, 1);
	}
	argset_free(&as);
	scad_geom_add(into, g);
	return 1;
    }

    /* ---- non-geometry builtins ---------------------------------- */
    if (BU_STR_EQUAL(name, "children")) {
	long which = -1;
	if (nargs >= 1) {
	    struct scad_value idx = eval_expr(st, env, args[0].value);
	    if (idx.type == SCAD_VAL_NUM) which = (long)idx.num;
	    scad_value_free(&idx);
	}
	exec_children(st, env, into, which);
	return 1;
    }
    if (BU_STR_EQUAL(name, "echo")) {
	struct bu_vls s = BU_VLS_INIT_ZERO;
	size_t i;
	bu_vls_strcat(&s, "ECHO: ");
	for (i = 0; i < nargs; i++) {
	    struct scad_value v = eval_expr(st, env, args[i].value);
	    if (i) bu_vls_strcat(&s, ", ");
	    if (args[i].name) bu_vls_printf(&s, "%s = ", args[i].name);
	    value_to_str(&s, &v);
	    scad_value_free(&v);
	}
	bu_log("scad: %s\n", bu_vls_cstr(&s));
	bu_vls_free(&s);
	/* echo may still take children */
	exec_body(st, env, body, nbody, into);
	return 1;
    }
    if (BU_STR_EQUAL(name, "assert")) {
	if (nargs >= 1) {
	    struct scad_value c = eval_expr(st, env, args[0].value);
	    if (!v_true(&c))
		bu_log("scad: assertion failed\n");
	    scad_value_free(&c);
	}
	exec_body(st, env, body, nbody, into);
	return 1;
    }

    /* unsupported geometry-changing builtins: pass children through */
    if (BU_STR_EQUAL(name, "projection") || BU_STR_EQUAL(name, "offset") ||
	BU_STR_EQUAL(name, "import") || BU_STR_EQUAL(name, "surface") ||
	BU_STR_EQUAL(name, "text")) {
	bu_log("scad: %s() is not supported and was skipped\n", name);
	st->dropped++;
	return 1;
    }

    handled = 0;
    return handled;
}


/* ------------------------------------------------------------------ */
/* statement execution                                                */
/* ------------------------------------------------------------------ */

/* execute the children captured by the enclosing module instantiation */
static void
exec_children(struct scad_state *st, struct env *env, struct scad_geom *into, long which)
{
    struct env *e = env;
    for (; e; e = e->parent)
	if (e->has_children) break;
    if (!e) return;
    if (which < 0) {
	exec_body(st, e->children_env, e->children, e->nchildren, into);
    } else if ((size_t)which < e->nchildren) {
	exec_stmt(st, e->children_env, e->children[which], into);
    }
}

/* invoke a user-defined module */
static void
exec_user_module(struct scad_state *st, struct env *callenv, struct moddef *md,
		 struct scad_stmt *call, struct scad_geom *into)
{
    struct env *me;
    struct scad_geom *mg;

    if (++st->eval_depth > SCAD_MAX_DEPTH) {
	st->eval_depth--;
	bu_log("scad: recursion limit exceeded in module %s\n", md->name);
	st->aborted = 1;
	return;
    }
    me = env_new(callenv);
    bind_params(st, callenv, me, md->params, call->args, call->nargs);
    /* attach children context */
    me->has_children = 1;
    me->children = call->body;
    me->nchildren = call->nbody;
    me->children_env = callenv;
    env_set(me, "$children", v_num((double)call->nbody));

    mg = scad_geom_new(G_GROUP);
    exec_body(st, me, md->body, md->nbody, mg);
    scad_geom_add(into, mg);

    env_free(me);
    st->eval_depth--;
}

static void
exec_instance(struct scad_state *st, struct env *env, struct scad_stmt *s,
	      struct scad_geom *into)
{
    struct moddef *md;
    struct env *ienv;
    size_t i;

    if (s->modifier == '*' || s->modifier == '%') {
	/* '*' disables; '%' is preview-only (excluded from CSG) */
	return;
    }

    /* bind any $-prefixed named args so they propagate to children */
    ienv = env;
    for (i = 0; i < s->nargs; i++) {
	if (s->args[i].name && s->args[i].name[0] == '$') {
	    if (ienv == env) ienv = env_new(env);
	    env_set(ienv, s->args[i].name, eval_expr(st, env, s->args[i].value));
	}
    }

    md = env_find_mod(env, s->name);
    if (md) {
	struct scad_geom *before = into;
	size_t nk = into->nkids;
	exec_user_module(st, ienv, md, s, into);
	if (s->modifier == '!' && before->nkids > nk)
	    st->root_only = before->kids[nk];
    } else {
	size_t nk = into->nkids;
	if (!exec_builtin(st, ienv, s->name, s->args, s->nargs, s->body, s->nbody, into)) {
	    bu_log("scad: unknown module '%s' skipped\n", s->name);
	    st->dropped++;
	} else if (s->modifier == '!' && into->nkids > nk) {
	    st->root_only = into->kids[nk];
	}
    }

    if (ienv != env) env_free(ienv);
}

static void
exec_for(struct scad_state *st, struct env *env, struct scad_stmt *s,
	 struct scad_geom *into, struct scad_assign *as, struct scad_geom *g)
{
    /* nested iteration over the assign list (cartesian product) */
    struct scad_value iter;
    struct env *le;
    if (!as) {
	exec_body(st, env, s->body, s->nbody, g);
	return;
    }
    le = env_new(env);
    iter = eval_expr(st, env, as->value);
    if (iter.type == SCAD_VAL_RANGE) {
	double x = iter.range[0], step = iter.range[1], end = iter.range[2];
	if (step == 0) step = 1;
	for (; (step > 0) ? (x <= end + 1e-9) : (x >= end - 1e-9); x += step) {
	    env_set(le, as->name, v_num(x));
	    exec_for(st, le, s, into, as->next, g);
	}
    } else if (iter.type == SCAD_VAL_VEC) {
	size_t i;
	for (i = 0; i < iter.vlen; i++) {
	    env_set(le, as->name, scad_value_copy(&iter.items[i]));
	    exec_for(st, le, s, into, as->next, g);
	}
    } else if (iter.type == SCAD_VAL_STR) {
	size_t i, m = strlen(iter.str);
	for (i = 0; i < m; i++) {
	    char buf[2]; buf[0] = iter.str[i]; buf[1] = '\0';
	    env_set(le, as->name, v_str(buf));
	    exec_for(st, le, s, into, as->next, g);
	}
    }
    scad_value_free(&iter);
    env_free(le);
}

static void
exec_stmt(struct scad_state *st, struct env *env, struct scad_stmt *s,
	  struct scad_geom *into)
{
    if (!s || st->aborted) return;

    switch (s->kind) {
	case ST_ASSIGN:
	    env_set(env, s->name, eval_expr(st, env, s->expr));
	    break;
	case ST_MODULE:
	case ST_FUNCTION:
	    /* handled by the hoisting pass in exec_body */
	    break;
	case ST_INSTANCE:
	    exec_instance(st, env, s, into);
	    break;
	case ST_IF: {
	    struct scad_value c = eval_expr(st, env, s->expr);
	    int t = v_true(&c);
	    struct scad_geom *g = scad_geom_new(G_GROUP);
	    scad_value_free(&c);
	    if (t) exec_body(st, env, s->body, s->nbody, g);
	    else exec_body(st, env, s->elsebody, s->nelse, g);
	    scad_geom_add(into, g);
	    break;
	}
	case ST_FOR:
	case ST_INT_FOR: {
	    struct scad_geom *g = scad_geom_new(s->kind == ST_INT_FOR ? G_INTERSECTION : G_GROUP);
	    exec_for(st, env, s, into, s->assigns, g);
	    scad_geom_add(into, g);
	    break;
	}
	case ST_LET: {
	    struct env *le = env_new(env);
	    struct scad_assign *a;
	    struct scad_geom *g = scad_geom_new(G_GROUP);
	    for (a = s->assigns; a; a = a->next)
		env_set(le, a->name, eval_expr(st, le, a->value));
	    exec_body(st, le, s->body, s->nbody, g);
	    scad_geom_add(into, g);
	    env_free(le);
	    break;
	}
	case ST_BLOCK: {
	    struct scad_geom *g = scad_geom_new(G_GROUP);
	    exec_body(st, env, s->body, s->nbody, g);
	    scad_geom_add(into, g);
	    break;
	}
	case ST_INCLUDE:
	case ST_USE:
	    /* handled in exec_body (needs file IO) */
	    break;
    }
}


/* ------------------------------------------------------------------ */
/* include / use                                                      */
/* ------------------------------------------------------------------ */

char *
scad_read_file(const char *path)
{
    FILE *fp = fopen(path, "rb");
    long sz;
    char *buf;
    size_t rd;
    if (!fp) return NULL;
    fseek(fp, 0, SEEK_END);
    sz = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    if (sz < 0) { fclose(fp); return NULL; }
    buf = (char *)bu_malloc((size_t)sz + 1, "scad file");
    rd = fread(buf, 1, (size_t)sz, fp);
    buf[rd] = '\0';
    fclose(fp);
    return buf;
}

static void hoist_defs(struct env *env, struct scad_stmt **body, size_t n);

static void
exec_include(struct scad_state *st, struct env *env, struct scad_stmt *s,
	     struct scad_geom *into, int use_only)
{
    struct bu_vls path = BU_VLS_INIT_ZERO;
    char *text;
    struct scad_stmt **prog;
    size_t nprog = 0, i;

    if (st->include_depth > 30) {
	bu_log("scad: include/use nesting too deep, skipping %s\n", s->name);
	return;
    }

    if (st->base_dir && s->name[0] != '/' &&
	!(isalpha((unsigned char)s->name[0]) && s->name[1] == ':'))
	bu_vls_printf(&path, "%s/%s", st->base_dir, s->name);
    else
	bu_vls_strcat(&path, s->name);

    if (!bu_file_exists(bu_vls_cstr(&path), NULL)) {
	bu_log("scad: cannot find %s '%s'\n", use_only ? "use" : "include", bu_vls_cstr(&path));
	st->dropped++;
	bu_vls_free(&path);
	return;
    }

    text = scad_read_file(bu_vls_cstr(&path));
    if (!text) { bu_vls_free(&path); return; }

    st->include_depth++;
    prog = scad_parse(st, text, bu_vls_cstr(&path), &nprog);
    bu_free(text, "scad file");

    if (prog) {
	if (use_only) {
	    /* import only module/function definitions */
	    hoist_defs(env, prog, nprog);
	} else {
	    /* full include: definitions + top-level geometry */
	    hoist_defs(env, prog, nprog);
	    for (i = 0; i < nprog; i++) {
		enum scad_stmt_kind k = prog[i]->kind;
		if (k == ST_INCLUDE)
		    exec_include(st, env, prog[i], into, 0);
		else if (k == ST_USE)
		    exec_include(st, env, prog[i], into, 1);
		else if (k != ST_MODULE && k != ST_FUNCTION)
		    exec_stmt(st, env, prog[i], into);
	    }
	}
	/* NOTE: prog nodes are referenced by hoisted moddefs (body pointers),
	 * so we intentionally leak the parsed included AST for the lifetime of
	 * the conversion rather than freeing it here. */
    }
    st->include_depth--;
    bu_vls_free(&path);
}


/* ------------------------------------------------------------------ */
/* body execution with definition hoisting                            */
/* ------------------------------------------------------------------ */

static void
hoist_defs(struct env *env, struct scad_stmt **body, size_t n)
{
    size_t i;
    for (i = 0; i < n; i++) {
	struct scad_stmt *s = body[i];
	if (s->kind == ST_MODULE) {
	    struct moddef *m;
	    BU_ALLOC(m, struct moddef);
	    m->name = bu_strdup(s->name);
	    m->params = s->params;
	    m->body = s->body;
	    m->nbody = s->nbody;
	    m->next = env->mods;
	    env->mods = m;
	} else if (s->kind == ST_FUNCTION) {
	    struct funcdef *f;
	    BU_ALLOC(f, struct funcdef);
	    f->name = bu_strdup(s->name);
	    f->params = s->params;
	    f->body = s->expr;
	    f->next = env->funcs;
	    env->funcs = f;
	}
    }
}

static void
exec_body(struct scad_state *st, struct env *env,
	  struct scad_stmt **body, size_t n, struct scad_geom *into)
{
    size_t i;
    if (st->aborted) return;
    hoist_defs(env, body, n);
    for (i = 0; i < n && !st->aborted; i++) {
	struct scad_stmt *s = body[i];
	if (s->kind == ST_MODULE || s->kind == ST_FUNCTION)
	    continue;
	if (s->kind == ST_INCLUDE)
	    exec_include(st, env, s, into, 0);
	else if (s->kind == ST_USE)
	    exec_include(st, env, s, into, 1);
	else
	    exec_stmt(st, env, s, into);
    }
}


/* ------------------------------------------------------------------ */
/* entry point                                                        */
/* ------------------------------------------------------------------ */

int
scad_run(struct scad_state *st, struct scad_stmt **prog, size_t n)
{
    struct env *g = env_new(NULL);

    /* special-variable defaults */
    env_set(g, "$fn", v_num(0));
    env_set(g, "$fa", v_num(12));
    env_set(g, "$fs", v_num(2));
    env_set(g, "$t", v_num(0));
    env_set(g, "$preview", v_bool(0));
    env_set(g, "$children", v_num(0));

    st->root = scad_geom_new(G_GROUP);
    st->root_only = NULL;

    exec_body(st, g, prog, n, st->root);

    env_free(g);
    return st->aborted ? -1 : 0;
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
