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
	BU_GETSTRUCT(_par,pc_param);
#define PC_GETCONSTRAINT(_con) \
	BU_GETSTRUCT(_con,pc_constrnt);
#define PC_PCSET_PUSHP(_pcs,_par) \
	BU_LIST_PUSH(&(_pcs.ps->l),&(_par->l));
#define PC_PCSET_PUSHC(_pcs,_con) \
	BU_LIST_PUSH(&(_pcs.cs->l),&(_con->l));
#define PC_FREE_PCSET(_pcs) \
	bu_free(_pcs.ps); \
	bu_free(_pcs.cs);

#ifdef __cplusplus
extern "C" {
#endif

PC_EXPORT PC_EXTERN(int pc_mk_constraint,(struct rt_wdb *wdbp, const char *constraintname, int append_ok));

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
