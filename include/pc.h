/*                            P C . H
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
/** @addtogroup libpc */
/** @{ */
/** @file pc.h
 *
 * Structures required for implementing Parametrics and constraints
 *
 */
#ifndef __PC_H__
#define __PC_H__

#define PC_MAX_STACK_SIZE 1000

#include "bu.h"
#include "bn.h"
#include "raytrace.h"

#ifndef PC_EXPORT
#  if defined(_WIN32) && !defined(__CYGWIN__) && defined(BRLCAD_DLL)
#    ifdef PC_EXPORT_DLL
#      define PC_EXPORT __declspec(dllexport)
#    else
#      define PC_EXPORT __declspec(dllimport)
#    endif
#  else
#    define PC_EXPORT
#  endif
#endif

/*
 * Macros for providing function prototypes, regardless of whether the
 * compiler understands them or not.  It is vital that the argument
 * list given for "args" be enclosed in parens.
 */

#if __STDC__ || USE_PROTOTYPES
#  define PC_EXTERN(type_and_name, args)       extern type_and_name args
#  define PC_ARGS(args)                        args
#else
#  define PC_EXTERN(type_and_name, args)       extern type_and_name()
#  define PC_ARGS(args)                        ()
#endif

#define PC_INIT_PCSET(_pcs) \
	BU_GETSTRUCT(_pcs.ps,pc_param); \
	BU_LIST_INIT(&(_pcs.ps->l)); \
	BU_GETSTRUCT(_pcs.cs,pc_constrnt); \
	BU_LIST_INIT(&(_pcs.cs->l));
#define PC_GETPARAMETER(_par) \
	BU_GETSTRUCT(_par,pc_param); \
	bu_vls_init(&(_par->name)); \
	bu_vls_init(&(_par->data.expression));
#define PC_GETCONSTRAINT(_con) \
	BU_GETSTRUCT(_con,pc_constrnt); \
	bu_vls_init(&(_con->name)); \
	bu_vls_init(&(_con->expression));
#define PC_PCSET_PUSHP(_pcsp,_par) \
	BU_LIST_PUSH(&(_pcsp->ps->l),&(_par->l));
#define PC_PCSET_PUSHC(_pcsp,_con) \
	BU_LIST_PUSH(&(_pcsp->cs->l),&(_con->l));


#ifdef __cplusplus
extern "C" {
#endif

/* functions defined in pc_main.c */

PC_EXPORT PC_EXTERN(int pc_mk_constraint, (struct rt_wdb *wdbp, const char *constraintname, int append_ok));
PC_EXPORT PC_EXTERN(void pc_free_pcset, (struct pc_pc_set * pcs));
PC_EXPORT PC_EXTERN(void pc_pushparameter, (struct pc_pc_set * pcs, const char * str));
PC_EXPORT PC_EXTERN(void pc_pushconstraint, (struct pc_pc_set * pcs, const char * str));

#ifdef __cplusplus
}
#endif

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
