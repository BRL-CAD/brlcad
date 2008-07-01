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


__BEGIN_DECLS

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



PC_EXPORT PC_EXTERN(int pc_write_parameter_set,(struct pc_param_set ps, struct directory * dp, struct db_i * dbip ));

PC_EXPORT PC_EXTERN(int pc_generate_parameters,(struct pc_param_set * psp, struct directory * dp, struct db_i * dbip));

PC_EXPORT PC_EXTERN(int pc_mk_constraint,(struct rt_wdb *wdbp, const char *constraintname, int append_ok));


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
