/*                        T I M E R . C P P
 * BRL-CAD
 *
 * Copyright (c) 1985-2021 United States Government as represented by
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

#include "common.h"
#include "bio.h"
#include <ctime>
#include <chrono>
#include <sstream>
#include <iomanip>
#include "bu/log.h"
#include "bu/str.h"
#include "bu/vls.h"
#include "rt/timer.h"

#ifdef HAVE_GETPROCESSTIMES
static double  time_cpu;	/* Time at which timing started */
#else
static std::clock_t time_cpu;	/* Time at which timing started */
#endif
static std::chrono::steady_clock::time_point time_wall;

void
rt_prep_timer(void)
{
#ifdef HAVE_GETPROCESSTIMES
    FILETIME a,b,c,d;
    time_cpu = 0.0;
    if (!GetProcessTimes(GetCurrentProcess(),&a,&b,&c,&d)) {
	bu_log("Warning - could not initialize RT timer!\n");
	return;
    }
    /* https://stackoverflow.com/a/17440673 */
    time_cpu = (double)(d.dwLowDateTime | ((unsigned long long)d.dwHighDateTime << 32)) * 0.0000001;
#else
    time_cpu = std::clock();
#endif
    time_wall = std::chrono::steady_clock::now();
}

double
rt_get_timer(struct bu_vls *vp, double *elapsed)
{

    double user_cpu_secs;
    double elapsed_secs;

#ifdef HAVE_GETPROCESSTIMES
    FILETIME a,b,c,d;
    double time1 = DBL_MAX;
    if (!GetProcessTimes(GetCurrentProcess(),&a,&b,&c,&d)) {
	bu_log("Warning - could not initialize RT timer!\n");
	return DBL_MAX;
    }
    /* https://stackoverflow.com/a/17440673 */
    time1 = (double)(d.dwLowDateTime | ((unsigned long long)d.dwHighDateTime << 32)) * 0.0000001;
    user_cpu_secs = time1 - time_cpu;
#else
    std::clock_t time1 = std::clock();
    user_cpu_secs = (double)(time1 - time_cpu)/CLOCKS_PER_SEC;
#endif

    std::chrono::steady_clock::time_point wall_now = std::chrono::steady_clock::now();
    elapsed_secs = std::chrono::duration_cast<std::chrono::microseconds>(wall_now - time_wall).count();
    elapsed_secs = elapsed_secs / 1e6;

    if (elapsed)  *elapsed = elapsed_secs;

    if (vp) {
	std::stringstream ss;
	ss << std::fixed << std::setprecision(5) << elapsed_secs;
	std::string sstr = ss.str();
	bu_vls_printf(vp, "cpu = %g sec, elapsed = %s sec\n", user_cpu_secs, sstr.c_str());
    }

    return user_cpu_secs;
}

double
rt_read_timer(char *str, int len)
{
    struct bu_vls vls = BU_VLS_INIT_ZERO;
    double cpu;
    int todo;

    if (!str)
	return rt_get_timer((struct bu_vls *)0, (double *)0);

    cpu = rt_get_timer(&vls, (double *)0);
    todo = bu_vls_strlen(&vls);

    /* Whatever the vls length is, we're not writing more
     * content than the str length supplied by the caller */
    todo = (todo > len) ? len : todo;

    bu_strlcpy(str, bu_vls_addr(&vls), todo);

    return cpu;
}


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
