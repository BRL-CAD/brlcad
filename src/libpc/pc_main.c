/*                   	P C _ M A I N . C
 * BRL-CAD
 *
 * Copyright (c) 2008 United States Government as represented by
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
/** @file pc_main.c
 *
 * @brief External C Routines of Parametrics and Constraints API
 *
 * @author Dawn Thomas
 *
 *
 */

#include "common.h"

#include <stdlib.h>

#include "pc.h"

/**
 * 			PC_INIT_PCSET
 * initiates a pc_set object by calling the appropriate bu_list init macros
 *
 */
void
pc_init_pcset(struct pc_pc_set * pcsp)
{
    BU_GETSTRUCT(pcsp->ps,pc_param);
    BU_LIST_INIT(&(pcsp->ps->l));
    BU_GETSTRUCT(pcsp->cs,pc_constrnt);
    BU_LIST_INIT(&(pcsp->cs->l));
}

/**
 * 			PC_GETPARAMETER
 * allocates memory for the parameter if parameter storage accoring to
 * the integer argument
 *
 * 	0 : data storage by expression
 * 	anything else : data storage by structure
 *
 */
void
pc_getparameter(struct pc_param ** p, int n)
{
    BU_GETSTRUCT(*p,pc_param);
    bu_vls_init(&((*p)->name));
    if (n == PC_DB_BYEXPR)
	bu_vls_init(&((*p)->data.expression));
}

/**
 * 			PC_PUSHPARAM_EXPR
 * pushes a given parameter expression into the parameter list in the pc set
 *
 */
void
pc_pushparam_expr(struct pc_pc_set * pcsp,const char * name, const char * str)
{
    struct pc_param * par;
    pc_getparameter(&par,PC_DB_BYEXPR);
    bu_vls_strcat(&(par->name), name);
    par->ctype = PC_DB_BYEXPR;
    bu_vls_strcat(&(par->data.expression), str);
    PC_PCSET_PUSHP(pcsp, par);
}

/**
 * 			PC_PUSHPARAM_STRUCT
 * pushes a given parameter into the parameter list in the pc set
 *
 */
void
pc_pushparam_struct(struct pc_pc_set * pcsp,const char * name, int type, void * ptr)
{
    struct pc_param * par;
    /*va_list args;
    va_start(args,n);*/
    
    pc_getparameter(&par,PC_DB_BYSTRUCT);
    bu_vls_strcat(&(par->name), name);
    par->ctype = PC_DB_BYSTRUCT;
    
    switch (type) {
	case PC_DB_FASTF_T :
	    par->data.pval.valuep = (fastf_t *) ptr;
	    par->dtype = PC_DB_FASTF_T;
	    break;
	case PC_DB_POINT_T :
	    par->data.pval.pointp = (pointp_t) ptr;
	    par->dtype = PC_DB_POINT_T;
	    break;
	case PC_DB_VECTOR_T :
	    par->data.pval.vectorp = (vectp_t) ptr;
	    par->dtype = PC_DB_VECTOR_T;
	    break;
	default :
	    bu_log("!!! Unknown data structure for Variable : %s \n", name);
    }

    /** the acual push operation into the pc_pc_set */
    PC_PCSET_PUSHP(pcsp, par);
    
    /* End variable argument parsing 
    va_end(args);*/
}

/**
 * 			PC_GETCONSTRAINT
 * allocates memory for the parameter if parameter storage accoring to
 * the integer argument
 *
 * 	0 : data storage by expression
 * 	anything else : data storage by structure
 *
 */
void
pc_getconstraint(struct pc_constrnt ** c, int n)
{
    BU_GETSTRUCT(*c,pc_constrnt);
    bu_vls_init(&((*c)->name));
    /*if (n == 0) */
    bu_vls_init(&((*c)->expression));
}

/**
 * 			PC_PUSHCONSTRAINT
 * pushes a given constraint expression into the constraint list
 * in the pc set
 *
 */
void
pc_pushconstraint(struct pc_pc_set * pcsp, const char * str)
{
    struct pc_constrnt * con;
    pc_getconstraint(&con,0);
    bu_vls_strcat(&(con->name), str);
    PC_PCSET_PUSHC(pcsp, con);
}

/**
 * 			PC_FREE_PCSET
 * frees the parameter&constraint set (pc_pc_set) pointed to by the
 * pointer taken as an argument. It also frees the corresponding allocation
 * for bu_vls structs used for the storage of name and expression of
 * pc_param and pc_constrnt structures
 *
 */
void
pc_free_pcset(struct pc_pc_set * pcs) 
{
    struct pc_param * par;
    struct pc_constrnt * con;
    while (BU_LIST_WHILE(par,pc_param,&(pcs->ps->l))) { 
        bu_vls_free(&(par->name));
	if (par->ctype == PC_DB_BYEXPR)
	    bu_vls_free(&(par->data.expression));
        BU_LIST_DEQUEUE(&(par->l));
	bu_free(par, "free parameter");
    }
    bu_free(pcs->ps, "free parameter");
    while (BU_LIST_WHILE(con,pc_constrnt,&(pcs->cs->l))) {
        bu_vls_free(&(con->name));
	bu_vls_free(&(con->expression));
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
