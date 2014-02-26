/*                         T I M E . H
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

/** @file time.h
 *
 */
#ifndef BU_TIME_H
#define BU_TIME_H

#include "common.h"

#include <stdio.h> /* For FILE */
#include <sys/types.h> /* for off_t */
#include <stddef.h> /* for size_t */
#include <stdlib.h> /* for getenv */

#ifdef HAVE_STDINT_H
#  include <stdint.h> /* for [u]int[16|32|64]_t */
#endif

#include "bu/defines.h"
#include "bu/vls.h"

/** @addtogroup bu_time */
/** @{ */

/** @file libbu/timer.c
 * Return microsecond accuracy time information.
 */
BU_EXPORT extern int64_t bu_gettime(void);

/**
 * Evaluate the time_t input as UTC time in ISO format.
 *
 * The UTC time is written into the user-provided bu_vls struct and is
 * also returned and guaranteed to be a non-null result, returning a
 * static "NULL" UTC time if an error is encountered.
 */
BU_EXPORT void bu_utctime(struct bu_vls *utc_result, const int64_t time_val);

/** @} */

#endif  /* BU_TIME_H */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
