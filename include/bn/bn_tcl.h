/*                        B N _ T C L . H
 * BRL-CAD
 *
 * Copyright (c) 2004-2014 United States Government as represented by
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

/*----------------------------------------------------------------------*/
/* @file bn_tcl.h */
/** @addtogroup bntcl */
/** @{ */

/**
 * @brief
 * Tcl interfaces to all the LIBBN math routines.
 *
 */

#ifndef BN_TCL_H
#define BN_TCL_H

#include "common.h"
#include "vmath.h"
#include "bn/defines.h"
#include "bu/bu_tcl.h"

__BEGIN_DECLS

#include "tcl_encode.h"

/**
 *@brief
 * Add all the supported Tcl interfaces to LIBBN routines to
 * the list of commands known by the given interpreter.
 */
BN_EXPORT extern void bn_tcl_setup(Tcl_Interp *interp);

/**
 *@brief
 * Allows LIBBN to be dynamically loaded to a vanilla tclsh/wish with
 * "load /usr/brlcad/lib/libbn.so"
 *
 * The name of this function is specified by TCL.
 */
BN_EXPORT extern int Bn_Init(Tcl_Interp *interp);

BN_EXPORT extern void bn_tcl_mat_print(Tcl_Interp *interp, const char *title, const mat_t m);

__END_DECLS

#endif  /* BN_TCL_H */
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
