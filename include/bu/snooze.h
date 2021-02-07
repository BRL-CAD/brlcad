/*                        S N O O Z E . H
 * BRL-CAD
 *
 * Copyright (c) 2018-2021 United States Government as represented by
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
/** @file snooze.h
 *
 * Routines for suspending the current thread of execution.
 *
 */

#ifndef BU_SNOOZE_H
#define BU_SNOOZE_H 1

#include "common.h"

#include "bu/defines.h"

__BEGIN_DECLS

/**
 * Convert seconds to microseconds
 */
#define BU_SEC2USEC(sec) (1000000LL * (sec))


/**
 * suspend the current thread for at least the specified number of
 * microseconds.
 *
 * this is a portability replacement for usleep(3).
 *
 * returns 0 on success, non-zero on error
 */
BU_EXPORT extern int bu_snooze(int64_t useconds);

__END_DECLS

#endif /* BU_SNOOZE_H */

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
