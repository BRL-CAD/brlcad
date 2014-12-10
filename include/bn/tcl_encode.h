/*                        T C L _ E N C O D E . H
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
/* @file tcl_encode.h */
/** @addtogroup tcl */
/** @{ */

/**
 *  @brief Support routines for the math functions
 */

#ifndef BN_TCL_ENCODE_H
#define BN_TCL_ENCODE_H

#include "common.h"
#include "vmath.h"
#include "bn/defines.h"
#include "bu/vls.h"

__BEGIN_DECLS

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

#endif  /* BN_TCL_ENCODE_H */
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
