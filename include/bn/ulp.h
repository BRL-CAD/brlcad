/*                        U L P . H
 * BRL-CAD
 *
 * Copyright (c) 2025 United States Government as represented by
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

/** @addtogroup bn_ulp
 *
 * @brief Library for performing comparisons and dynamically calculating
 * floating point limits including the Unit in the Last Place (ULP).
 *
 * @{ */
/** @file ulp.h */

#ifndef BN_ULP_H
#define BN_ULP_H

#include "common.h"
#include "bn/defines.h"

__BEGIN_DECLS

/** @name Machine epsilons @ { */
BN_EXPORT extern double bn_dbl_epsilon(void);
BN_EXPORT extern float bn_flt_epsilon(void);
/** @} */

/** @name Numeric range limits @ { */
BN_EXPORT extern double bn_dbl_min(void);
BN_EXPORT extern double bn_dbl_max(void);
BN_EXPORT extern float bn_flt_min(void);
BN_EXPORT extern float bn_flt_max(void);
/** @} */

/** @name Square-rooted limits @ { */
BN_EXPORT extern float bn_flt_min_sqrt(void);
BN_EXPORT extern float bn_flt_max_sqrt(void);
BN_EXPORT extern double bn_dbl_min_sqrt(void);
BN_EXPORT extern double bn_dbl_max_sqrt(void);
/** @} */

/**
 * @brief Calculate unit in the last place (ULP) for a given double.
 *
 * @param val the input double value.
 * @return distance to the next representable double precision value.
 */
BN_EXPORT extern double bn_ulp(double val);

__END_DECLS

#endif /* BN_ULP_H */

/** @} */

/*
 * Local Variables:
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */

