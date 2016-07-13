/*                            P C . H
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
/** @addtogroup libpc */
/** @{ */
/** @file pc.h
 *
 * Structures required for implementing Parametrics and constraints
 *
 */
#ifndef PC_H
#define PC_H

#include "common.h"

#include "bu/list.h"
#include "bu/vls.h"
#include "bn.h"

__BEGIN_DECLS

#ifndef PC_EXPORT
#  if defined(PC_DLL_EXPORTS) && defined(PC_DLL_IMPORTS)
#    error "Only PC_DLL_EXPORTS or PC_DLL_IMPORTS can be defined, not both."
#  elif defined(PC_DLL_EXPORTS)
#    define PC_EXPORT __declspec(dllexport)
#  elif defined(PC_DLL_IMPORTS)
#    define PC_EXPORT __declspec(dllimport)
#  else
#    define PC_EXPORT
#  endif
#endif

/*
 * Macros for providing function prototypes, regardless of whether the
 * compiler understands them or not.  It is vital that the argument
 * list given for "args" be enclosed in parens.
 */

#define PC_PCSET_PUSHP(_pcsp,_par) \
	BU_LIST_PUSH(&(_pcsp->ps->l),&(_par->l));
#define PC_PCSET_PUSHC(_pcsp,_con) \
	BU_LIST_PUSH(&(_pcsp->cs->l),&(_con->l));

/** Definitions for data storage and transfer type information */
#define PC_DB_FASTF_T 1
#define PC_DB_POINT_T 2
#define PC_DB_VECTOR_T 3

#define PC_DB_BYSTRUCT 1
#define PC_DB_BYEXPR 2

#ifdef __cplusplus
extern "C" {
#endif

/**
 * A composite set of parameters constraints with respect to those
 * parameters. Used for declaration by each geometry object
 */
struct pc_param {
    struct bu_list l;
    struct bu_vls name; /** Name of the parameter */

    /** @todo convert to enum after pc becomes a dependency of rt */
    int ctype;	/** Container type used for storing data */
    int dtype; 	/** Data type of the data pointed to */

    union {		/** The Actual data / pointer */
	struct bu_vls expression;
	void * ptr;
    } data;
};

struct pc_constraint_fp {
	int nargs;
	int dimension;
	int (*fp) (double ** a);
};

struct pc_constrnt {
    struct bu_list l;
    struct bu_vls name;
    int ctype;
    union {
	struct bu_vls expression;
	struct pc_constraint_fp cf;
    } data;
    const char **args;
};

struct pc_pc_set {
    struct pc_param * ps;
    struct pc_constrnt * cs;
};

/* functions defined in pc_main.c */

PC_EXPORT extern void pc_init_pcset(struct pc_pc_set * pcs);
PC_EXPORT extern void pc_free_pcset(struct pc_pc_set * pcs);
PC_EXPORT extern void pc_free_constraint(struct pc_constrnt * c);
PC_EXPORT extern void pc_getparameter(struct pc_param **p, int t);
PC_EXPORT extern void pc_pushparam_expr(struct pc_pc_set * pcsp, const char *name, const char *str);
PC_EXPORT extern void pc_pushparam_struct(struct pc_pc_set * pcs, const char * name, int type, void * ptr);
PC_EXPORT extern void pc_getconstraint_expr(struct pc_constrnt ** c);
PC_EXPORT extern void pc_getconstraint_struct(struct pc_constrnt **c, int nargs);
PC_EXPORT extern void pc_pushconstraint_expr(struct pc_pc_set * pcsp, const char * name, const char * str);
PC_EXPORT extern void pc_pushconstraint_struct(struct pc_pc_set * pcs, const char * name, int nargs, int dimension, int (*fp) (double ** args), const char ** args);
PC_EXPORT extern void pc_pushconstraint_struct1(struct pc_pc_set * pcs, struct pc_constrnt * c);
PC_EXPORT extern void pc_pushconstraint(struct pc_pc_set * pcs, const char * str);

/* constraint eval functions defined in pc_constraints.c */

PC_EXPORT extern int pc_isperpendicular(double ** v);
PC_EXPORT extern void pc_mk_isperpendicular(struct pc_constrnt ** c, const char * name, const char **args);
PC_EXPORT extern int pc_ismodpositive(double ** v);
PC_EXPORT extern void pc_mk_ismodpositive(struct pc_constrnt ** c, const char * name, const char **args);

#ifdef __cplusplus
}
#endif

__END_DECLS
#endif
/** @} */
/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
