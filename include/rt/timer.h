/*                        T I M E R . H
 * BRL-CAD
 *
 * Copyright (c) 1993-2020 United States Government as represented by
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
 * Provide timing information for RT.
 */

/**
 * Initialize global librt timer
 */
RT_EXPORT extern void rt_prep_timer(void);

/**
 * Reports on the passage of time, since rt_prep_timer() was called.  Explicit
 * return is number of CPU seconds.  String return is descriptive.  If "wall"
 * pointer is non-null, number of elapsed seconds per the wall clock are
 * returned.  Times returned will never be zero.
 */
RT_EXPORT extern double rt_get_timer(struct bu_vls *vp, double *elapsed);
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
