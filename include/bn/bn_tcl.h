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
/** @addtogroup bn_tcl
 *
 * @brief
 * Tcl interfaces to all the LIBBN math routines.
 *
 */
/** @{ */
/** @file bn_tcl.h */

#ifndef BN_TCL_H
#define BN_TCL_H

#include "common.h"
#include "vmath.h"
#include "bn/defines.h"
#include "bu/bu_tcl.h"

__BEGIN_DECLS

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

/**
 *  @brief Support routines for the math functions
 */

/* XXX Really need a decode_array function that uses atof(),
 * XXX so that junk like leading { and commas between inputs
 * XXX don't spoil the conversion.
 */

/* The presence of Tcl_Interp as an arg prevents giving arg list */

BN_EXPORT extern int bn_decode_mat(mat_t m,
				   const char *str);
BN_EXPORT extern int bn_decode_quat(quat_t q,
				    const char *str);
BN_EXPORT extern int bn_decode_vect(vect_t v,
				    const char *str);
BN_EXPORT extern int bn_decode_hvect(hvect_t v,
				     const char *str);
BN_EXPORT extern void bn_encode_mat(struct bu_vls *vp,
				    const mat_t m);
BN_EXPORT extern void bn_encode_quat(struct bu_vls *vp,
				     const quat_t q);
BN_EXPORT extern void bn_encode_vect(struct bu_vls *vp,
				     const vect_t v);
BN_EXPORT extern void bn_encode_hvect(struct bu_vls *vp,
				      const hvect_t v);



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
