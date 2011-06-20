/*                   	P C _ M A I N . C
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
/** @addtogroup pcdbio */
/** @{ */
/** @file libpc/pc_main.c
 *
 * @brief External C Routines of Parametrics and Constraints API
 *
 */

#include "common.h"

#include <stdlib.h>

#include "pc.h"

/**
 * PC_INIT_PCSET
 * initiates a pc_set object by calling the appropriate bu_list init macros
 *
 */
void
pc_init_pcset(struct pc_pc_set *pcsp)
{
    BU_GETSTRUCT(pcsp->ps, pc_param);
    BU_LIST_INIT(&(pcsp->ps->l));
    BU_GETSTRUCT(pcsp->cs, pc_constrnt);
    BU_LIST_INIT(&(pcsp->cs->l));
}


/**
 * PC_GETPARAMETER
 * allocates memory for the parameter if parameter storage accoring to
 * the integer argument
 *
 * 0 : data storage by expression
 * anything else : data storage by structure
 *
 */
void
pc_getparameter(struct pc_param **p, int n)
{
    BU_GETSTRUCT(*p, pc_param);
    bu_vls_init(&((*p)->name));
    if (n == PC_DB_BYEXPR)
	bu_vls_init(&((*p)->data.expression));
}


/**
 * PC_PUSHPARAM_EXPR
 * pushes a given parameter expression into the parameter list in the pc set
 *
 */
void
pc_pushparam_expr(struct pc_pc_set *pcsp, const char *name, const char *str)
{
    struct pc_param *par;
    pc_getparameter(&par, PC_DB_BYEXPR);
    bu_vls_strcat(&(par->name), name);
    par->ctype = PC_DB_BYEXPR;
    bu_vls_strcat(&(par->data.expression), str);
    PC_PCSET_PUSHP(pcsp, par);
}


/**
 * PC_PUSHPARAM_STRUCT
 * pushes a given parameter into the parameter list in the pc set
 *
 */
void
pc_pushparam_struct(struct pc_pc_set *pcsp, const char *name, int type, void *ptr)
{
    struct pc_param *par;
    
    pc_getparameter(&par, PC_DB_BYSTRUCT);
    bu_vls_strcat(&(par->name), name);
    par->ctype = PC_DB_BYSTRUCT;
    par->data.ptr = ptr;
    
    switch (type) {
	case PC_DB_FASTF_T :
	    par->dtype = PC_DB_FASTF_T;
	    break;
	case PC_DB_POINT_T :
	    par->dtype = PC_DB_POINT_T;
	    break;
	case PC_DB_VECTOR_T :
	    par->dtype = PC_DB_VECTOR_T;
	    break;
	default :
	    bu_log("!!! Unknown data structure for Variable : %s \n", name);
    }

    /** the acual push operation into the pc_pc_set */
    PC_PCSET_PUSHP(pcsp, par);
}


/**
 * PC_GETCONSTRAINT_EXPR
 * allocate storage for a constraint by expression
 *
 */
void
pc_getconstraint_expr(struct pc_constrnt **c)
{
    BU_GETSTRUCT(*c, pc_constrnt);
    bu_vls_init(&((*c)->name));
    bu_vls_init(&((*c)->data.expression));
    (*c)->ctype = PC_DB_BYEXPR;
}


/**
 * PC_GETCONSTRAINT_STRUCT
 * allocate storage for a pc_constrnt object which uses data structure for
 * storage
 *
 */

void
pc_getconstraint_struct(struct pc_constrnt **c, int nargs)
{
    BU_GETSTRUCT(*c, pc_constrnt);
    bu_vls_init(&((*c)->name));
    (*c)->args = (const char **) malloc(nargs *sizeof(char *));
    (*c)->ctype = PC_DB_BYSTRUCT;
}


/**
 * PC_PUSHCONSTRAINT_EXPR
 * pushes a given parameter expression into the parameter list in the pc set
 *
 */
void
pc_pushconstraint_expr(struct pc_pc_set *pcsp, const char *name, const char *str)
{
    struct pc_constrnt *con;
    pc_getconstraint_expr(&con);
    bu_vls_strcat(&(con->name), name);
    bu_vls_strcat(&(con->data.expression), str);
    PC_PCSET_PUSHC(pcsp, con);
}


/**
 * PC_PUSHCONSTRAINT_STRUCT
 * pushes a given parameter into the parameter list in the pc set
 * @todo nargs and dimension should be contained withing a datastructure by
 * the constraint evaluation struct (fp) .But due to lack of functors in C
 * this might as well need to exist as an independent struct especially in
 * the case of complex constraint functions
 */
void
pc_pushconstraint_struct(struct pc_pc_set *pcsp, const char *name, int nargs, int dimension, int (*fp) (double **args), const char **args)
{
    struct pc_constrnt *con;
    int i;
    
    pc_getconstraint_struct(&con, nargs);
    bu_vls_strcat(&(con->name), name);
    for (i =0; i < nargs; i++)
	con->args[i] = args[i];
    con->data.cf.nargs = nargs;
    con->data.cf.dimension = dimension;
    con->data.cf.fp = fp;

    /** the acual push operation into the pc_pc_set */
    PC_PCSET_PUSHC(pcsp, con);
}


void
pc_pushconstraint_struct1(struct pc_pc_set *pcsp, struct pc_constrnt *con)
{
    PC_PCSET_PUSHC(pcsp, con);
}


/**
 * PC_FREE_CONSTRAINT
 * frees the memory allocation for a pc_constrnt object.
 *
 */

void
pc_free_constraint(struct pc_constrnt *c)
{
    bu_vls_free(&(c->name));
    if (c->ctype == PC_DB_BYEXPR)
        bu_vls_free(&(c->data.expression));
    if (c->ctype == PC_DB_BYSTRUCT) 
        free(c->args);
}


/**
 * PC_FREE_PCSET
 * frees the parameter&constraint set (pc_pc_set) pointed to by the
 * pointer taken as an argument. It also frees the corresponding allocation
 * for bu_vls structs used for the storage of name and expression of
 * pc_param and pc_constrnt structures
 *
 */
void
pc_free_pcset(struct pc_pc_set *pcs) 
{
    struct pc_param *par;
    struct pc_constrnt *con;
    while (BU_LIST_WHILE(par, pc_param, &(pcs->ps->l))) { 
        bu_vls_free(&(par->name));
	if (par->ctype == PC_DB_BYEXPR)
	    bu_vls_free(&(par->data.expression));
        BU_LIST_DEQUEUE(&(par->l));
	bu_free(par, "free parameter");
    }
    bu_free(pcs->ps, "free parameter");
    while (BU_LIST_WHILE(con, pc_constrnt, &(pcs->cs->l))) {
	pc_free_constraint(con);
        BU_LIST_DEQUEUE(&(con->l));
        bu_free(con, "free constraint");
    }
    bu_free(pcs->cs, "free constraint");
}


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
