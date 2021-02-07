/*                       B O O L E A N . H
 * BRL-CAD
 *
 * Copyright (c) 2007-2021 United States Government as represented by
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
/** @addtogroup brep_boolean
 * @brief
 * Boolean Operations for  Non-Uniform Rational
 * B-Spline (NURBS) Boundary Representations.
 */
#ifndef BREP_BOOLEAN_H
#define BREP_BOOLEAN_H

#include "common.h"
#include "brep/defines.h"

/** @{ */
/** @file brep/boolean.h */

#ifdef __cplusplus

extern "C++" {

enum op_type {
    BOOLEAN_UNION = 0,
    BOOLEAN_INTERSECT = 1,
    BOOLEAN_DIFF = 2,
    BOOLEAN_XOR = 3
};

/**
 * Evaluate NURBS boolean operations.
 *
 * @param brepO [out]
 * @param brepA [in]
 * @param brepB [in]
 * @param operation [in]
 */
extern BREP_EXPORT int
ON_Boolean(ON_Brep *brepO, const ON_Brep *brepA, const ON_Brep *brepB, op_type operation);

} /* extern C++ */
#endif

#endif  /* BREP_BOOLEAN_H */
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
