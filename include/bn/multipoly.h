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
#include "bu/magic.h"
#include "bn/defines.h"

__BEGIN_DECLS

typedef struct bn_multipoly {
    uint32_t magic;
    int dgrs;
    int dgrt;
    double **cf;
}  bn_multipoly_t;

#define BN_CK_MULTIPOLY(_p) BU_CKMAG(_p, BN_MULTIPOLY_MAGIC, "struct bn_multipoly")

BN_EXPORT extern struct bn_multipoly *bn_multipoly_new(int dgrs, int dgrt);
BN_EXPORT extern void bn_multipoly_free(struct bn_multipoly *p);
BN_EXPORT extern struct bn_multipoly *bn_multipoly_grow(struct bn_multipoly *P, int dgrs, int dgrt);
BN_EXPORT extern struct bn_multipoly *bn_multipoly_set(struct bn_multipoly *P, int s, int t, double val);
BN_EXPORT extern struct bn_multipoly *bn_multipoly_add(const struct bn_multipoly *p1, const struct bn_multipoly *p2);
BN_EXPORT extern struct bn_multipoly *bn_multipoly_mul(const struct bn_multipoly *p1, const struct bn_multipoly *p2);

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
