/* 
 * tclUnixTime.c --
 *
 *	Contains Unix specific versions of Tcl functions that
 *	obtain time values from the operating system.
 *
 * Copyright (c) 1995 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * RCS: @(#) $Id$
 */

#include "tclInt.h"
#include "tclPort.h"

/*
 *-----------------------------------------------------------------------------
 *
 * TclpGetSeconds --
 *
 *	This procedure returns the number of seconds from the epoch.  On
 *	most Unix systems the epoch is Midnight Jan 1, 1970 GMT.
 *
 * Results:
 *	Number of seconds from the epoch.
 *
 * Side effects:
 *	None.
 *
 *-----------------------------------------------------------------------------
 */

unsigned long
TclpGetSeconds()
{
    return time((time_t *) NULL);
}

/*
 *-----------------------------------------------------------------------------
 *
 * TclpGetClicks --
 *
 *	This procedure returns a value that represents the highest resolution
 *	clock available on the system.  There are no garantees on what the
 *	resolution will be.  In Tcl we will call this value a "click".  The
 *	start time is also system dependant.
 *
 * Results:
 *	Number of clicks from some start time.
 *
 * Side effects:
 *	None.
 *
 *-----------------------------------------------------------------------------
 */

unsigned long
TclpGetClicks()
{
    unsigned long now;
#ifdef NO_GETTOD
    struct tms dummy;
#else
    struct timeval date;
    struct timezone tz;
#endif

#ifdef NO_GETTOD
    now = (unsigned long) times(&dummy);
#else
    gettimeofday(&date, &tz);
    now = date.tv_sec*1000000 + date.tv_usec;
#endif

    return now;
}

/*
 *----------------------------------------------------------------------
 *
 * TclpGetTimeZone --
 *
 *	Determines the current timezone.  The method varies wildly
 *	between different platform implementations, so its hidden in
 *	this function.
 *
 * Results:
 *	The return value is the local time zone, measured in
 *	minutes away from GMT (-ve for east, +ve for west).
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
TclpGetTimeZone (currentTime)
    unsigned long  currentTime;
{
    /*
     * Determine how a timezone is obtained from "struct tm".  If there is no
     * time zone in this struct (very lame) then use the timezone variable.
     * This is done in a way to make the timezone variable the method of last
     * resort, as some systems have it in addition to a field in "struct tm".
     * The gettimeofday system call can also be used to determine the time
     * zone.
     */
    
#if defined(HAVE_TM_TZADJ)
#   define TCL_GOT_TIMEZONE
    time_t      curTime = (time_t) currentTime;
    struct tm  *timeDataPtr = localtime(&curTime);
    int         timeZone;

    timeZone = timeDataPtr->tm_tzadj  / 60;
    if (timeDataPtr->tm_isdst) {
        timeZone += 60;
    }
    
    return timeZone;
#endif

#if defined(HAVE_TM_GMTOFF) && !defined (TCL_GOT_TIMEZONE)
#   define TCL_GOT_TIMEZONE
    time_t     curTime = (time_t) currentTime;
    struct tm *timeDataPtr = localtime(&curTime);
    int        timeZone;

    timeZone = -(timeDataPtr->tm_gmtoff / 60);
    if (timeDataPtr->tm_isdst) {
        timeZone += 60;
    }
    
    return timeZone;
#endif

#if defined(USE_DELTA_FOR_TZ)
#define TCL_GOT_TIMEZONE 1
    /*
     * This hack replaces using global var timezone or gettimeofday
     * in situations where they are buggy such as on AIX when libbsd.a
     * is linked in.
     */

    int timeZone;
    time_t tt;
    struct tm *stm;
    tt = 849268800L;      /*    1996-11-29 12:00:00  GMT */
    stm = localtime(&tt); /* eg 1996-11-29  6:00:00  CST6CDT */
    /* The calculation below assumes a max of +12 or -12 hours from GMT */
    timeZone = (12 - stm->tm_hour)*60 + (0 - stm->tm_min);
    return timeZone;  /* eg +360 for CST6CDT */
#endif

    /*
     * Must prefer timezone variable over gettimeofday, as gettimeofday does
     * not return timezone information on many systems that have moved this
     * information outside of the kernel.
     */
    
#if defined(HAVE_TIMEZONE_VAR) && !defined (TCL_GOT_TIMEZONE)
#   define TCL_GOT_TIMEZONE
    static int setTZ = 0;
#ifdef TCL_THREADS
    static Tcl_Mutex tzMutex;
#endif
    int        timeZone;

    Tcl_MutexLock(&tzMutex);
    if (!setTZ) {
        tzset();
        setTZ = 1;
    }
    Tcl_MutexUnlock(&tzMutex);

    /*
     * Note: this is not a typo in "timezone" below!  See tzset
     * documentation for details.
     */

    timeZone = timezone / 60;

    return timeZone;
#endif

#if !defined(NO_GETTOD) && !defined (TCL_GOT_TIMEZONE)
#   define TCL_GOT_TIMEZONE
    struct timeval  tv;
    struct timezone tz;
    int timeZone;

    gettimeofday(&tv, &tz);
    timeZone = tz.tz_minuteswest;
    if (tz.tz_dsttime) {
        timeZone += 60;
    }
    
    return timeZone;
#endif

#ifndef TCL_GOT_TIMEZONE
    /*
     * Cause compile error, we don't know how to get timezone.
     */
    error: autoconf did not figure out how to determine the timezone. 
#endif

}

/*
 *----------------------------------------------------------------------
 *
 * TclpGetTime --
 *
 *	Gets the current system time in seconds and microseconds
 *	since the beginning of the epoch: 00:00 UCT, January 1, 1970.
 *
 * Results:
 *	Returns the current time in timePtr.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

void
TclpGetTime(timePtr)
    Tcl_Time *timePtr;		/* Location to store time information. */
{
    struct timeval tv;
    struct timezone tz;
    
    (void) gettimeofday(&tv, &tz);
    timePtr->sec = tv.tv_sec;
    timePtr->usec = tv.tv_usec;
}

/*
 *----------------------------------------------------------------------
 *
 * TclpGetDate --
 *
 *	This function converts between seconds and struct tm.  If
 *	useGMT is true, then the returned date will be in Greenwich
 *	Mean Time (GMT).  Otherwise, it will be in the local time zone.
 *
 * Results:
 *	Returns a static tm structure.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

struct tm *
TclpGetDate(time, useGMT)
    TclpTime_t time;
    int useGMT;
{
    CONST time_t *tp = (CONST time_t *)time;

    if (useGMT) {
	return gmtime(tp);
    } else {
	return localtime(tp);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TclpStrftime --
 *
 *	On Unix, we can safely call the native strftime implementation.
 *
 * Results:
 *	The normal strftime result.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

size_t
TclpStrftime(s, maxsize, format, t)
    char *s;
    size_t maxsize;
    CONST char *format;
    CONST struct tm *t;
{
    return strftime(s, maxsize, format, t);
}
