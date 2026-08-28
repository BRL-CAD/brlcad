/*                        M U L T I P O L Y . H
 * BRL-CAD
 *
 * Copyright (c) 2004-2026 United States Government as represented by
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
/** @addtogroup bn_poly
 *
 * @brief Polynomial data type
 *
 */
/** @{ */
/* @file multipoly.h */

#ifndef BN_MULTIPOLY_H
#define BN_MULTIPOLY_H

#include "common.h"
#include "bn/defines.h"

__BEGIN_DECLS

typedef struct bn_multipoly {
    uint32_t magic;
    /**< Number of coefficients in the first polynomial variable. */
    int dgrs;
    /**< Number of coefficients in the second polynomial variable. */
    int dgrt;
    /**< Coefficient of S^s * T^t is cf[s][t]. */
    double **cf;
}  bn_multipoly_t;

/**
 * Allocate a zero-valued bivariate polynomial with the specified
 * coefficient counts.  Both counts must be positive.
 */
BN_EXPORT extern struct bn_multipoly *bn_multipoly_new(int dgrs, int dgrt);

/** Release a bivariate polynomial allocated by bn_multipoly_new(). */
BN_EXPORT extern void bn_multipoly_free(struct bn_multipoly *poly);

/** Grow a polynomial to at least the specified coefficient counts. */
BN_EXPORT extern struct bn_multipoly *bn_multipoly_grow(struct bn_multipoly *poly,
							 int dgrs,
							 int dgrt);

/** Set the coefficient of S^s * T^t, growing the polynomial as needed. */
BN_EXPORT extern struct bn_multipoly *bn_multipoly_set(struct bn_multipoly *poly,
							int s,
							int t,
							double value);

/** Return the sum of two bivariate polynomials. */
BN_EXPORT extern struct bn_multipoly *bn_multipoly_add(const struct bn_multipoly *p1,
							const struct bn_multipoly *p2);

/** Return the product of two bivariate polynomials. */
BN_EXPORT extern struct bn_multipoly *bn_multipoly_mul(const struct bn_multipoly *p1,
							const struct bn_multipoly *p2);

__END_DECLS

#endif  /* BN_MULTIPOLY_H */
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
