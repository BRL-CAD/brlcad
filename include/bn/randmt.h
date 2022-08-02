/*                        R A N D M T . H
 * BRL-CAD
 *
 * Copyright (c) 2004-2022 United States Government as represented by
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
/** @addtogroup bn_rmt
 *
 * @brief
 * Mersenne Twister random number generation as defined by MT19937.
 *
 * Generates one pseudorandom real number (double) which is uniformly
 * distributed on [0, 1]-interval, for each call.
 *
 * @par Usage:
 @code
 double d;

 bn_randmt_seed(integer_seed);

 while (NEED_MORE_RAND_NUMBERS) {
 d = bn_randmt();
 }
 @endcode
 *
 */
/** @{ */
/** @file randmt.h */

#ifndef BN_RANDMT_H
#define BN_RANDMT_H

#include "common.h"
#include "bn/defines.h"

__BEGIN_DECLS

BN_EXPORT extern double bn_randmt(void);
BN_EXPORT extern void bn_randmt_seed(unsigned long seed);

__END_DECLS

#endif  /* BN_RANDMT_H */
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
