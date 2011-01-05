/*                           T I M E R . C
 * BRL-CAD
 *
 * Copyright (c) 2011 United States Government as represented by
 * the U.S. Army Research Laboratory.
 * Copyright (c) Tim Riker
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

#include <time.h>
#if !defined(_WIN32)
#  ifdef HAVE_SYS_TIME_H
#     include <sys/time.h>
#  endif
#  ifdef HAVE_SYS_TYPES_H
#     include <sys/types.h>
#  endif
#  ifdef HAVE_SCHED_H
#    include <sched.h>
#  endif
static int64_t lastTime = 0;
#else /* !defined(_WIN32) */
#include <windows.h>
#  include <mmsystem.h>
static unsigned long int lastTime = 0;
static LARGE_INTEGER     qpcLastTime;
static LONGLONG          qpcFrequency = 0;
static LONGLONG          qpcLastCalibration;
static DWORD             timeLastCalibration;
#endif /* !defined(_WIN32) */

static int64_t currentTime = 0;

#include "bu.h"

#if !defined(_WIN32)
static inline int64_t getEpochMicroseconds()
{
	struct timeval nowTime;
	gettimeofday(&nowTime, NULL);
	return ((int64_t)nowTime.tv_sec * (int64_t)1000000
		+ (int64_t)nowTime.tv_usec);
}
#endif

int64_t bu_gettime(void)
{
#if !defined(_WIN32)
	if (lastTime == 0) {
		lastTime = getEpochMicroseconds();
		currentTime += lastTime; /* sync with system clock */
	}
	else {
		const int64_t nowTime = getEpochMicroseconds();

		const int64_t diff = (nowTime - lastTime);

		if (diff > 0) {
			/* add to currentTime */
			currentTime += diff;
		}
		else if (diff < 0) {
			/* eh, how'd we go back in time? */
			bu_log("WARNING: went back in time %lli microseconds\n", (long long int)diff);
		}

		lastTime = nowTime;
	}

#else /* !defined(_WIN32) */
	{
	static int inited = 0;
	if (!inited) {
		inited = 1;
		/*
		InitializeCriticalSection(&timer_critical);
		*/
	}

	if (qpcFrequency != 0) {
		/* main timer is qpc */
		LARGE_INTEGER now;
		QueryPerformanceCounter(&now);
		{
		LONGLONG diff     = now.QuadPart - qpcLastTime.QuadPart;
		LONGLONG clkSpent = now.QuadPart - qpcLastCalibration;
		qpcLastTime = now;

		if (clkSpent > qpcFrequency) {
			/* Recalibrate Frequency */
			DWORD tgt	   = timeGetTime();
			DWORD deltaTgt      = tgt - timeLastCalibration;
			timeLastCalibration = tgt;
			qpcLastCalibration  = now.QuadPart;
			if (deltaTgt > 0) {
				LONGLONG oldqpcfreq = qpcFrequency;
				qpcFrequency	= (clkSpent * 1000) / deltaTgt;
				if (qpcFrequency != oldqpcfreq)
					bu_log("Recalibrated QPC frequency.  Old: %f ; New: %f\n",
							(double)oldqpcfreq, (double)qpcFrequency);
			}
		}

		currentTime += (int64_t)((double) diff / (double) qpcFrequency);
		}
	}
	else {
		static int sane = 1;
		LARGE_INTEGER freq;

		/* should only get into here once on app start */
		if (!sane) {
			bu_log("Sanity check failure in bu_gettime()\n");
		}
		sane = 0;

		/* make sure we're at our best timer resolution possible */
		timeBeginPeriod(1);

		if (QueryPerformanceFrequency(&freq)) {
			QueryPerformanceCounter(&qpcLastTime);
			qpcFrequency	= freq.QuadPart;
			logDebugMessage(4,"Actual reported QPC Frequency: %f\n", (double)qpcFrequency);
			qpcLastCalibration  = qpcLastTime.QuadPart;
			timeLastCalibration = timeGetTime();
			currentTime += (int64_t)(1.0e3 * (double)timeLastCalibration); /* sync with system clock */
		}
		else {
			/*
			logDebugMessage(1,"QueryPerformanceFrequency failed with error %d\n", GetLastError());
*/
			lastTime = (unsigned long int)timeGetTime();
			currentTime += (int64_t)(1.0e3 * (double)lastTime); /* sync with system clock */
		}
	}

	/*
	UNLOCK_TIMER_MUTEX
	*/
	}
#endif /* !defined(_WIN32) */

		return currentTime;
}

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
