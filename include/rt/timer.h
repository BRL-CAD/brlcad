/*                        T I M E R . H
 * BRL-CAD
 *
 * Copyright (c) 1993-2016 United States Government as represented by
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
/** @file rt/timer.h */

#ifndef RT_TIMER_H
#define RT_TIMER_H

#include "common.h"
#include "vmath.h"
#include "bu/vls.h"
#include "rt/defines.h"

__BEGIN_DECLS

/* Start the global timer */
/** @addtogroup rt_timer */
/** @{ */
/**
 * To provide timing information for RT.
 * THIS VERSION FOR Denelcor HEP/UPX (System III-like)
 */

/**
*
* To provide timing information for RT when running on 4.2 BSD UNIX.
*
*/

 /**
 *
 * To provide timing information on Microsoft Windows NT.
 */
 /**
 *
 * To provide timing information for RT.  This version for any non-BSD
 * UNIX system, including System III, Vr1, Vr2.  Version 6 & 7 should
 * also be able to use this (untested).  The time() and times()
 * sys-calls are used for all timing.
 *
 */


RT_EXPORT extern void rt_prep_timer(void);
/* Read global timer, return time + str */
/**
 * Reports on the passage of time, since rt_prep_timer() was called.
 * Explicit return is number of CPU seconds.  String return is
 * descriptive.  If "elapsed" pointer is non-null, number of elapsed
 * seconds are returned.  Times returned will never be zero.
 */
RT_EXPORT extern double rt_get_timer(struct bu_vls *vp,
                                     double *elapsed);
/* Return CPU time, text, & wall clock time off the global timer */
/**
 * Compatibility routine
 */
RT_EXPORT extern double rt_read_timer(char *str, int len);

/** @} */

__END_DECLS

#endif /* RT_TIMER_H */

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
