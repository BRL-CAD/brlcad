/*                     D A T E T I M E . H
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

#ifndef BU_DATETIME_H
#define BU_DATETIME_H

#include "common.h"

#include <stddef.h> /* for size_t */
#include <stdlib.h> /* for getenv */

#include "bu/defines.h"
#include "bu/vls.h"

__BEGIN_DECLS

/** @addtogroup bu_datetime
 * @brief
 * Cross platform wrapper for timing functionality.
 */
/** @{ */
/** @file bu/datetime.h */

/**
 * Returns a microsecond-accurate wall-clock time counter.
 *
 * Example use:
 * @code
 * int64_t start = bu_gettime();
 * do_some_work_here();
 * double elapsed = bu_gettime() - start;
 * double seconds = elapsed / 1000000.0;
 * printf("time: %.2f\n", seconds);
 * @endcode
 *
 */
BU_EXPORT extern int64_t bu_gettime(void);

/**
 * Returns a high-resolution CPU (user+system) time counter for the
 * current process, in nanoseconds.
 *
 * Unlike bu_gettime(), this value is not a wall-clock or epoch timestamp.  It
 * measures CPU time consumed by the process across all threads.  Returns -1 if
 * a process CPU timer is not available on the platform.
 */
BU_EXPORT extern int64_t bu_timer_cpu(void);

/**
 * Returns a high-resolution CPU (user+system) time counter for the
 * current thread, in nanoseconds.
 *
 * This is useful for profiling small regions of threaded work without charging
 * scheduler preemption or other threads' work to the measured interval.
 * Returns -1 if a current-thread CPU timer is not available on the platform.
 */
BU_EXPORT extern int64_t bu_timer_cpu_thread(void);

/**
 * Evaluate the time_t input as UTC time in ISO format.
 *
 * The UTC time is written into the user-provided bu_vls struct and is
 * also returned and guaranteed to be a non-null result, returning a
 * static "NULL" UTC time if an error is encountered.
 */
BU_EXPORT void bu_utctime(struct bu_vls *utc_result, const int64_t time_val);

/** @} */

__END_DECLS

#endif  /* BU_DATETIME_H */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
