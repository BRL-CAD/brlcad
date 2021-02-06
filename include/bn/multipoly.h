/*                        M U L T I P O L Y . H
 * BRL-CAD
 *
 * Copyright (c) 2004-2021 United States Government as represented by
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
    int dgrs;
    int dgrt;
    double **cf;
}  bn_multipoly_t;

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
