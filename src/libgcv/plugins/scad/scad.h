/*                          S C A D . H
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
/** @file scad.h
 *
 * Shared declarations for the OpenSCAD (.scad) import plugin.
 *
 * The importer is split into three cooperating translation units that
 * all share the types declared here:
 *
 *   scad_parse.c  - lexer + recursive-descent parser (text -> AST)
 *   scad_eval.c   - evaluator (AST -> resolved CSG geometry tree)
 *   scad_geom.c   - geometry tree helpers + emission to a BRL-CAD .g
 *   scad_read.c   - libgcv plugin glue (options, recognition, entry)
 *
 * OpenSCAD is a declarative Constructive Solid Geometry language, which
 * maps very naturally onto BRL-CAD's native CSG representation:
 *
 *   cube        -> arb8           sphere       -> ell
 *   cylinder    -> tgc/trc        polyhedron   -> bot (solid)
 *   linear_extrude -> sketch+extrusion   rotate_extrude -> sketch+revolve
 *   translate/rotate/scale/mirror/multmatrix -> member arc matrices
 *   union/difference/intersection            -> boolean combinations
 *
 * See the plugin TODO file for documented fidelity losses.
 */

#ifndef LIBGCV_PLUGINS_SCAD_SCAD_H
#define LIBGCV_PLUGINS_SCAD_SCAD_H

#include "common.h"

#include <stddef.h>

#include "vmath.h"
#include "bu/vls.h"

__BEGIN_DECLS

struct rt_wdb;
struct gcv_opts;


/* ------------------------------------------------------------------ */
/* runtime values                                                     */
/* ------------------------------------------------------------------ */

enum scad_val_type {
    SCAD_VAL_UNDEF = 0,
    SCAD_VAL_NUM,
    SCAD_VAL_BOOL,
    SCAD_VAL_STR,
    SCAD_VAL_VEC,
    SCAD_VAL_RANGE
};

/* A value produced by evaluating an expression.  Values own their heap
 * storage (str, items) and are deep-copied; see scad_value_copy/free. */
struct scad_value {
    enum scad_val_type type;
    double num;			/* NUM (BOOL stored as 0.0/1.0)        */
    char *str;			/* STR                                  */
    struct scad_value *items;	/* VEC: vlen elements                   */
    size_t vlen;
    double range[3];		/* RANGE: begin, step, end              */
};


/* ------------------------------------------------------------------ */
/* expression AST                                                     */
/* ------------------------------------------------------------------ */

/* binary/unary operator codes */
#define SCAD_OP_ADD  1
#define SCAD_OP_SUB  2
#define SCAD_OP_MUL  3
#define SCAD_OP_DIV  4
#define SCAD_OP_MOD  5
#define SCAD_OP_NEG  6
#define SCAD_OP_NOT  7
#define SCAD_OP_AND  8
#define SCAD_OP_OR   9
#define SCAD_OP_EQ   10
#define SCAD_OP_NE   11
#define SCAD_OP_LT   12
#define SCAD_OP_LE   13
#define SCAD_OP_GT   14
#define SCAD_OP_GE   15

enum scad_expr_kind {
    EX_NUM, EX_BOOL, EX_STR, EX_UNDEF,
    EX_VECTOR,			/* [ items... ] (items may be generators) */
    EX_RANGE,			/* [a:c] or [a:b:c] (a,b,c)               */
    EX_IDENT,			/* name                                   */
    EX_UNARY,			/* op a                                   */
    EX_BINARY,			/* a op b                                 */
    EX_TERNARY,			/* a ? b : c                              */
    EX_INDEX,			/* a[b]                                   */
    EX_MEMBER,			/* a.name  (swizzle .x/.y/.z)             */
    EX_CALL,			/* name(args)                             */
    EX_LET,			/* let(assigns) a                         */
    EX_LC_FOR,			/* list-comp:  for(assigns) a             */
    EX_LC_IF,			/* list-comp:  if(a) b [else c]           */
    EX_LC_LET,			/* list-comp:  let(assigns) a             */
    EX_EACH			/* list-comp:  each a                     */
};

struct scad_expr;

/* a call argument: name==NULL for a positional argument */
struct scad_arg {
    char *name;
    struct scad_expr *value;
};

/* a name=expr binding used by let(), for(), and list comprehensions */
struct scad_assign {
    char *name;
    struct scad_expr *value;
    struct scad_assign *next;
};

struct scad_expr {
    enum scad_expr_kind kind;
    double num;			/* EX_NUM / EX_BOOL                       */
    char *str;			/* EX_STR                                 */
    char *name;			/* EX_IDENT / EX_CALL / EX_MEMBER         */
    int op;			/* EX_UNARY / EX_BINARY                   */
    struct scad_expr *a, *b, *c;	/* general operands                  */
    struct scad_expr **items;	/* EX_VECTOR elements                     */
    size_t nitems;
    struct scad_arg *args;	/* EX_CALL arguments                      */
    size_t nargs;
    struct scad_assign *assigns;	/* EX_LET / EX_LC_FOR / EX_LC_LET     */
};


/* ------------------------------------------------------------------ */
/* statement AST                                                      */
/* ------------------------------------------------------------------ */

enum scad_stmt_kind {
    ST_ASSIGN,			/* name = expr ;                          */
    ST_MODULE,			/* module name(params) body               */
    ST_FUNCTION,		/* function name(params) = expr ;         */
    ST_INSTANCE,		/* name(args) children   (module call)    */
    ST_IF,			/* if (expr) body [else elsebody]         */
    ST_FOR,			/* for (assigns) body                     */
    ST_INT_FOR,			/* intersection_for (assigns) body        */
    ST_LET,			/* let (assigns) body                     */
    ST_BLOCK,			/* { body }                               */
    ST_INCLUDE,			/* include <path>                         */
    ST_USE			/* use <path>                             */
};

/* a formal parameter of a module/function; def==NULL means required */
struct scad_param {
    char *name;
    struct scad_expr *def;
    struct scad_param *next;
};

struct scad_stmt {
    enum scad_stmt_kind kind;
    int modifier;		/* 0, '*', '!', '#', or '%'               */
    char *name;			/* call/def name, or include/use path     */
    struct scad_expr *expr;	/* ASSIGN value / FUNCTION body / IF cond */
    struct scad_arg *args;	/* ST_INSTANCE arguments                  */
    size_t nargs;
    struct scad_param *params;	/* ST_MODULE / ST_FUNCTION params         */
    struct scad_assign *assigns;	/* ST_FOR / ST_INT_FOR / ST_LET       */
    struct scad_stmt **body;	/* children / module body / then-branch   */
    size_t nbody;
    struct scad_stmt **elsebody;	/* ST_IF else-branch                  */
    size_t nelse;
};


/* ------------------------------------------------------------------ */
/* resolved geometry tree (output of evaluation)                      */
/* ------------------------------------------------------------------ */

enum scad_geom_kind {
    G_GROUP,			/* implicit union / union()               */
    G_DIFFERENCE,
    G_INTERSECTION,
    G_TRANSFORM,		/* 4x4 matrix over children               */
    G_COLOR,
    G_PRIM,
    G_LINEXT,			/* linear_extrude                         */
    G_ROTEXT,			/* rotate_extrude                         */
    G_HULL,
    G_MINKOWSKI,		/* unsupported (documented) - dropped     */
    G_RENDER			/* render() passthrough group             */
};

enum scad_prim_kind {
    PR_CUBE, PR_SPHERE, PR_CYLINDER, PR_POLYHEDRON,	/* 3D */
    PR_CIRCLE, PR_SQUARE, PR_POLYGON			/* 2D */
};

struct scad_geom {
    enum scad_geom_kind kind;
    struct scad_geom **kids;
    size_t nkids, kcap;

    mat_t mat;			/* G_TRANSFORM                            */

    double rgba[4];		/* G_COLOR                                */

    /* G_PRIM ----------------------------------------------------- */
    enum scad_prim_kind prim;
    int is2d;
    int center;
    vect_t size;		/* cube / square (z unused for square)    */
    double r1, r2, h;		/* sphere r=r1; cylinder r1/r2,h; circle  */
    int fn;			/* resolved $fn (0 => smooth)             */
    double fa, fs;		/* resolved $fa / $fs                     */

    /* polyhedron (3D) / polygon (2D): pts is npts*3 (z=0 for 2D)    */
    double *pts;
    size_t npts;
    int *fidx;			/* concatenated face vertex indices       */
    size_t *flen;		/* per-face index count                   */
    size_t nfaces;

    /* extrude parameters --------------------------------------- */
    double ext_height;
    int ext_center;
    double ext_twist;		/* degrees                                */
    double ext_scale[2];
    int ext_slices;
    double ext_angle;		/* rotate_extrude sweep, degrees          */
};


/* ------------------------------------------------------------------ */
/* plugin options and shared importer state                           */
/* ------------------------------------------------------------------ */

struct scad_read_options {
    char *units;		/* model units (default mm)               */
    int fn;			/* fallback $fn when file leaves it 0     */
    fastf_t fa;			/* fallback $fa                           */
    fastf_t fs;			/* fallback $fs                           */
    int facetize;		/* force faceted (bot) output for curved  */
    int facet_max;		/* max $fn kept as a faceted prism/gon    */
};

struct scad_state {
    struct rt_wdb *wdbp;
    const struct gcv_opts *gcv_options;
    const struct scad_read_options *opts;

    char *base_dir;		/* directory of the top-level file        */
    const char *input_file;

    unsigned long name_count;	/* unique BRL-CAD object name counter     */
    int dropped;		/* count of skipped/unsupported constructs */

    struct scad_geom *root;	/* G_GROUP of top-level instances         */
    struct scad_geom *root_only;	/* subtree selected by a '!' modifier */

    int eval_depth;		/* recursion guard                        */
    int include_depth;
    int aborted;		/* fatal evaluation error                 */

    struct bu_vls msg;		/* scratch message buffer                 */
};


/* ------------------------------------------------------------------ */
/* cross-module entry points                                          */
/* ------------------------------------------------------------------ */

/* scad_parse.c : parse a buffer into a heap array of statements.
 * Returns the statement array (caller frees via scad_stmts_free) and
 * sets *nout.  Returns NULL on a hard parse error. */
extern struct scad_stmt **scad_parse(struct scad_state *st, const char *text,
				     const char *fname, size_t *nout);
extern void scad_stmts_free(struct scad_stmt **stmts, size_t n);

/* scad_eval.c : read a whole file into a NUL-terminated heap buffer. */
extern char *scad_read_file(const char *path);

/* scad_eval.c : evaluate a parsed program, populating st->root. */
extern int scad_run(struct scad_state *st, struct scad_stmt **prog, size_t n);

/* scad_eval.c : value helpers (also used while building geometry). */
extern void scad_value_free(struct scad_value *v);
extern struct scad_value scad_value_copy(const struct scad_value *v);

/* scad_geom.c : geometry tree helpers. */
extern struct scad_geom *scad_geom_new(enum scad_geom_kind kind);
extern void scad_geom_add(struct scad_geom *parent, struct scad_geom *child);
extern void scad_geom_free(struct scad_geom *g);

/* scad_geom.c : write the resolved geometry tree into st->wdbp. */
extern int scad_emit(struct scad_state *st, struct scad_geom *root);

/* scad_geom.c : OpenSCAD fragment count for a circle of radius r. */
extern int scad_fragments(double r, int fn, double fa, double fs);

__END_DECLS

#endif /* LIBGCV_PLUGINS_SCAD_SCAD_H */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
