/*                         S O R T . H
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

/** @file sort.h
 *
 */
#ifndef BU_SORT_H
#define BU_SORT_H

#include "common.h"

#include <stddef.h> /* for size_t */

#include "bu/defines.h"

__BEGIN_DECLS

/** @addtogroup sort */
/** @{ */

/** @file libbu/sort.c
 * platform-independent re-entrant version of qsort, where the first argument
 * is the array to sort, the second the number of elements inside the array, the
 * third the size of one element, the fourth the comparison-function and the
 * fifth a variable which is handed as a third argument to the comparison-function.
 */
BU_EXPORT extern void bu_sort(void *array, size_t nummemb, size_t sizememb,
            int (*compare)(const void *, const void *, void *), void *context);


/** @} */

__END_DECLS

#endif  /* BU_SORT_H */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
