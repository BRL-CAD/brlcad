/* 
 * tclWinTime.c --
 *
 *	Contains Windows specific versions of Tcl functions that
 *	obtain time values from the operating system.
 *
 * Copyright 1995-1998 by Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * RCS: @(#) $Id$
 */

#include "tclWinInt.h"

#define SECSPERDAY (60L * 60L * 24L)
#define SECSPERYEAR (SECSPERDAY * 365L)
#define SECSPER4YEAR (SECSPERYEAR * 4L + SECSPERDAY)

/*
 * The following arrays contain the day of year for the last day of
 * each month, where index 1 is January.
 */

static int normalDays[] = {
    -1, 30, 58, 89, 119, 150, 180, 211, 242, 272, 303, 333, 364
};

static int leapDays[] = {
    -1, 30, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334, 365
};

typedef struct ThreadSpecificData {
    char tzName[64];		/* Time zone name */
    struct tm tm;		/* time information */
} ThreadSpecificData;
static Tcl_ThreadDataKey dataKey;

/*
 * Calibration interval for the high-resolution timer, in msec
 */

static CONST unsigned long clockCalibrateWakeupInterval = 10000;
				/* FIXME: 10 s -- should be about 10 min! */

/*
 * Data for managing high-resolution timers.
 */

typedef struct TimeInfo {

    CRITICAL_SECTION cs;	/* Mutex guarding this structure */

    int initialized;		/* Flag == 1 if this structure is
				 * initialized. */

    int perfCounterAvailable;	/* Flag == 1 if the hardware has a
				 * performance counter */

    HANDLE calibrationThread;	/* Handle to the thread that keeps the
				 * virtual clock calibrated. */

    HANDLE readyEvent;		/* System event used to
				 * trigger the requesting thread
				 * when the clock calibration procedure
				 * is initialized for the first time */
    HANDLE exitEvent; 		/* Event to signal out of an exit handler
				 * to tell the calibration loop to
				 * terminate */


    /*
     * The following values are used for calculating virtual time.
     * Virtual time is always equal to:
     *    lastFileTime + (current perf counter - lastCounter) 
     *				* 10000000 / curCounterFreq
     * and lastFileTime and lastCounter are updated any time that
     * virtual time is returned to a caller.
     */

    ULARGE_INTEGER lastFileTime;
    LARGE_INTEGER lastCounter;
    LARGE_INTEGER curCounterFreq;

    /* 
     * The next two values are used only in the calibration thread, to track
     * the frequency of the performance counter.
     */

    LONGLONG lastPerfCounter;	/* Performance counter the last time
				 * that UpdateClockEachSecond was called */
    LONGLONG lastSysTime;	/* System clock at the last time
				 * that UpdateClockEachSecond was called */
    LONGLONG estPerfCounterFreq;
				/* Current estimate of the counter frequency
				 * using the system clock as the standard */

} TimeInfo;

static TimeInfo timeInfo = {
    { NULL },
    0,
    0,
    (HANDLE) NULL,
    (HANDLE) NULL,
    (HANDLE) NULL,
#ifdef HAVE_CAST_TO_UNION
    (ULARGE_INTEGER) (DWORDLONG) 0,
    (LARGE_INTEGER) (LONGLONG) 0,
    (LARGE_INTEGER) (LONGLONG) 0,
#else
    0,
    0,
    0,
#endif
    0,
    0,
    0
};

CONST static FILETIME posixEpoch = { 0xD53E8000, 0x019DB1DE };
    
/*
 * Declarations for functions defined later in this file.
 */

static struct tm *	ComputeGMT _ANSI_ARGS_((const time_t *tp));
static void		StopCalibration _ANSI_ARGS_(( ClientData ));
static DWORD WINAPI     CalibrationThread _ANSI_ARGS_(( LPVOID arg ));
static void 		UpdateTimeEachSecond _ANSI_ARGS_(( void ));

/*
 *----------------------------------------------------------------------
 *
 * TclpGetSeconds --
 *
 *	This procedure returns the number of seconds from the epoch.
 *	On most Unix systems the epoch is Midnight Jan 1, 1970 GMT.
 *
 * Results:
 *	Number of seconds from the epoch.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

unsigned long
TclpGetSeconds()
{
    Tcl_Time t;
    Tcl_GetTime( &t );
    return t.sec;
}

/*
 *----------------------------------------------------------------------
 *
 * TclpGetClicks --
 *
 *	This procedure returns a value that represents the highest
 *	resolution clock available on the system.  There are no
 *	guarantees on what the resolution will be.  In Tcl we will
 *	call this value a "click".  The start time is also system
 *	dependant.
 *
 * Results:
 *	Number of clicks from some start time.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

unsigned long
TclpGetClicks()
{
    /*
     * Use the Tcl_GetTime abstraction to get the time in microseconds,
     * as nearly as we can, and return it.
     */

    Tcl_Time now;		/* Current Tcl time */
    unsigned long retval;	/* Value to return */

    Tcl_GetTime( &now );
    retval = ( now.sec * 1000000 ) + now.usec;
    return retval;

}

/*
 *----------------------------------------------------------------------
 *
 * TclpGetTimeZone --
 *
 *	Determines the current timezone.  The method varies wildly
 *	between different Platform implementations, so its hidden in
 *	this function.
 *
 * Results:
 *	Minutes west of GMT.
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
    int timeZone;

    tzset();
    timeZone = _timezone / 60;

    return timeZone;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_GetTime --
 *
 *	Gets the current system time in seconds and microseconds
 *	since the beginning of the epoch: 00:00 UCT, January 1, 1970.
 *
 * Results:
 *	Returns the current time in timePtr.
 *
 * Side effects:
 *	On the first call, initializes a set of static variables to
 *	keep track of the base value of the performance counter, the
 *	corresponding wall clock (obtained through ftime) and the
 *	frequency of the performance counter.  Also spins a thread
 *	whose function is to wake up periodically and monitor these
 *	values, adjusting them as necessary to correct for drift
 *	in the performance counter's oscillator.
 *
 *----------------------------------------------------------------------
 */

void
Tcl_GetTime(timePtr)
    Tcl_Time *timePtr;		/* Location to store time information. */
{
	
    struct timeb t;

    int useFtime = 1;		/* Flag == TRUE if we need to fall back
				 * on ftime rather than using the perf
				 * counter */

    /* Initialize static storage on the first trip through. */

    /*
     * Note: Outer check for 'initialized' is a performance win
     * since it avoids an extra mutex lock in the common case.
     */

    if ( !timeInfo.initialized ) { 
	TclpInitLock();
	if ( !timeInfo.initialized ) {
	    timeInfo.perfCounterAvailable
		= QueryPerformanceFrequency( &timeInfo.curCounterFreq );

	    /*
	     * Some hardware abstraction layers use the CPU clock
	     * in place of the real-time clock as a performance counter
	     * reference.  This results in:
	     *    - inconsistent results among the processors on
	     *      multi-processor systems.
	     *    - unpredictable changes in performance counter frequency
	     *      on "gearshift" processors such as Transmeta and
	     *      SpeedStep.
	     *
	     * There seems to be no way to test whether the performance
	     * counter is reliable, but a useful heuristic is that
	     * if its frequency is 1.193182 MHz or 3.579545 MHz, it's
	     * derived from a colorburst crystal and is therefore
	     * the RTC rather than the TSC.
	     *
	     * A sloppier but serviceable heuristic is that the RTC crystal
	     * is normally less than 15 MHz while the TSC crystal is
	     * virtually assured to be greater than 100 MHz.  Since Win98SE
	     * appears to fiddle with the definition of the perf counter
	     * frequency (perhaps in an attempt to calibrate the clock?)
	     * we use the latter rule rather than an exact match.
	     */

	    if ( timeInfo.perfCounterAvailable
		 /* The following lines would do an exact match on
		  * crystal frequency:
		  * && timeInfo.curCounterFreq.QuadPart != (LONGLONG) 1193182
		  * && timeInfo.curCounterFreq.QuadPart != (LONGLONG) 3579545
		  */
		 && timeInfo.curCounterFreq.QuadPart > (LONGLONG) 15000000 ) {
		timeInfo.perfCounterAvailable = FALSE;
	    }

	    /*
	     * If the performance counter is available, start a thread to
	     * calibrate it.
	     */

	    if ( timeInfo.perfCounterAvailable ) {
		DWORD id;
		InitializeCriticalSection( &timeInfo.cs );
		timeInfo.readyEvent = CreateEvent( NULL, FALSE, FALSE, NULL );
		timeInfo.exitEvent = CreateEvent( NULL, FALSE, FALSE, NULL );
		timeInfo.calibrationThread = CreateThread( NULL,
							   256,
							   CalibrationThread,
							   (LPVOID) NULL,
							   0,
							   &id );
		SetThreadPriority( timeInfo.calibrationThread,
				   THREAD_PRIORITY_HIGHEST );

		/*
		 * Wait for the thread just launched to start running,
		 * and create an exit handler that kills it so that it
		 * doesn't outlive unloading tclXX.dll
		 */

		WaitForSingleObject( timeInfo.readyEvent, INFINITE );
		CloseHandle( timeInfo.readyEvent );
		Tcl_CreateExitHandler( StopCalibration, (ClientData) NULL );
	    }
	    timeInfo.initialized = TRUE;
	}
	TclpInitUnlock();
    }

    if ( timeInfo.perfCounterAvailable ) {
	
	/*
	 * Query the performance counter and use it to calculate the
	 * current time.
	 */

	LARGE_INTEGER curCounter;
				/* Current performance counter */

	LONGLONG curFileTime;
				/* Current estimated time, expressed
				 * as 100-ns ticks since the Windows epoch */

	static LARGE_INTEGER posixEpoch;
				/* Posix epoch expressed as 100-ns ticks
				 * since the windows epoch */

	LONGLONG usecSincePosixEpoch;
				/* Current microseconds since Posix epoch */

	posixEpoch.LowPart = 0xD53E8000;
	posixEpoch.HighPart = 0x019DB1DE;

	EnterCriticalSection( &timeInfo.cs );

	QueryPerformanceCounter( &curCounter );
	/* 
	 * If it appears to be more than 1.1 seconds since the last trip
	 * through the calibration loop, the performance counter may
	 * have jumped. Discard it. See MSDN Knowledge Base article
	 * Q274323 for a description of the hardware problem that makes
	 * this test necessary.
	 */
	if ( curCounter.QuadPart - timeInfo.lastPerfCounter
	     < 11 * timeInfo.estPerfCounterFreq / 10 ) {
	    
	    curFileTime = timeInfo.lastFileTime.QuadPart
		+ ( ( curCounter.QuadPart - timeInfo.lastCounter.QuadPart )
		    * 10000000 / timeInfo.curCounterFreq.QuadPart );
	    timeInfo.lastFileTime.QuadPart = curFileTime;
	    timeInfo.lastCounter.QuadPart = curCounter.QuadPart;
	    usecSincePosixEpoch = ( curFileTime - posixEpoch.QuadPart ) / 10;
	    timePtr->sec = (time_t) ( usecSincePosixEpoch / 1000000 );
	    timePtr->usec = (unsigned long ) ( usecSincePosixEpoch % 1000000 );
	    useFtime = 0;
	}	    

	LeaveCriticalSection( &timeInfo.cs );
    }
	
    if ( useFtime ) {
	
	/* High resolution timer is not available.  Just use ftime */

	ftime(&t);
	timePtr->sec = t.time;
	timePtr->usec = t.millitm * 1000;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * StopCalibration --
 *
 *	Turns off the calibration thread in preparation for exiting the
 *	process.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Sets the 'exitEvent' event in the 'timeInfo' structure to ask
 *	the thread in question to exit, and waits for it to do so.
 *
 *----------------------------------------------------------------------
 */

static void
StopCalibration( ClientData unused )
				/* Client data is unused */
{
    SetEvent( timeInfo.exitEvent );
    WaitForSingleObject( timeInfo.calibrationThread, INFINITE );
    CloseHandle( timeInfo.exitEvent );
    CloseHandle( timeInfo.calibrationThread );
}

/*
 *----------------------------------------------------------------------
 *
 * TclpGetTZName --
 *
 *	Gets the current timezone string.
 *
 * Results:
 *	Returns a pointer to a static string, or NULL on failure.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

char *
TclpGetTZName(int dst)
{
    int len;
    char *zone, *p;
    TIME_ZONE_INFORMATION tz;
    Tcl_Encoding encoding;
    ThreadSpecificData *tsdPtr = TCL_TSD_INIT(&dataKey);
    char *name = tsdPtr->tzName;

    /*
     * tzset() under Borland doesn't seem to set up tzname[] at all.
     * tzset() under MSVC has the following weird observed behavior:
     *	 First time we call "clock format [clock seconds] -format %Z -gmt 1"
     *	 we get "GMT", but on all subsequent calls we get the current time 
     *	 zone string, even though env(TZ) is GMT and the variable _timezone 
     *   is 0.
     */

    name[0] = '\0';

    zone = getenv("TZ");
    if (zone != NULL) {
	/*
	 * TZ is of form "NST-4:30NDT", where "NST" would be the
	 * name of the standard time zone for this area, "-4:30" is
	 * the offset from GMT in hours, and "NDT is the name of 
	 * the daylight savings time zone in this area.  The offset 
	 * and DST strings are optional.
	 */

	len = strlen(zone);
	if (len > 3) {
	    len = 3;
	}
	if (dst != 0) {
	    /*
	     * Skip the offset string and get the DST string.
	     */

	    p = zone + len;
	    p += strspn(p, "+-:0123456789");
	    if (*p != '\0') {
		zone = p;
		len = strlen(zone);
		if (len > 3) {
		    len = 3;
		}
	    }
	}
	Tcl_ExternalToUtf(NULL, NULL, zone, len, 0, NULL, name,
		sizeof(tsdPtr->tzName), NULL, NULL, NULL);
    }
    if (name[0] == '\0') {
	if (GetTimeZoneInformation(&tz) == TIME_ZONE_ID_UNKNOWN) {
	    /*
	     * MSDN: On NT this is returned if DST is not used in
	     * the current TZ
	     */
	    dst = 0;
	}
	encoding = Tcl_GetEncoding(NULL, "unicode");
	Tcl_ExternalToUtf(NULL, encoding, 
		(char *) ((dst) ? tz.DaylightName : tz.StandardName), -1, 
		0, NULL, name, sizeof(tsdPtr->tzName), NULL, NULL, NULL);
	Tcl_FreeEncoding(encoding);
    } 
    return name;
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
TclpGetDate(t, useGMT)
    TclpTime_t t;
    int useGMT;
{
    const time_t *tp = (const time_t *) t;
    struct tm *tmPtr;
    long time;

    if (!useGMT) {
	tzset();

	/*
	 * If we are in the valid range, let the C run-time library
	 * handle it.  Otherwise we need to fake it.  Note that this
	 * algorithm ignores daylight savings time before the epoch.
	 */

	if (*tp >= 0) {
	    return localtime(tp);
	}

	time = *tp - _timezone;
	
	/*
	 * If we aren't near to overflowing the long, just add the bias and
	 * use the normal calculation.  Otherwise we will need to adjust
	 * the result at the end.
	 */

	if (*tp < (LONG_MAX - 2 * SECSPERDAY)
		&& *tp > (LONG_MIN + 2 * SECSPERDAY)) {
	    tmPtr = ComputeGMT(&time);
	} else {
	    tmPtr = ComputeGMT(tp);

	    tzset();

	    /*
	     * Add the bias directly to the tm structure to avoid overflow.
	     * Propagate seconds overflow into minutes, hours and days.
	     */

	    time = tmPtr->tm_sec - _timezone;
	    tmPtr->tm_sec = (int)(time % 60);
	    if (tmPtr->tm_sec < 0) {
		tmPtr->tm_sec += 60;
		time -= 60;
	    }
    
	    time = tmPtr->tm_min + time/60;
	    tmPtr->tm_min = (int)(time % 60);
	    if (tmPtr->tm_min < 0) {
		tmPtr->tm_min += 60;
		time -= 60;
	    }

	    time = tmPtr->tm_hour + time/60;
	    tmPtr->tm_hour = (int)(time % 24);
	    if (tmPtr->tm_hour < 0) {
		tmPtr->tm_hour += 24;
		time -= 24;
	    }

	    time /= 24;
	    tmPtr->tm_mday += time;
	    tmPtr->tm_yday += time;
	    tmPtr->tm_wday = (tmPtr->tm_wday + time) % 7;
	}
    } else {
	tmPtr = ComputeGMT(tp);
    }
    return tmPtr;
}

/*
 *----------------------------------------------------------------------
 *
 * ComputeGMT --
 *
 *	This function computes GMT given the number of seconds since
 *	the epoch (midnight Jan 1 1970).
 *
 * Results:
 *	Returns a (per thread) statically allocated struct tm.
 *
 * Side effects:
 *	Updates the values of the static struct tm.
 *
 *----------------------------------------------------------------------
 */

static struct tm *
ComputeGMT(tp)
    const time_t *tp;
{
    struct tm *tmPtr;
    long tmp, rem;
    int isLeap;
    int *days;
    ThreadSpecificData *tsdPtr = TCL_TSD_INIT(&dataKey);

    tmPtr = &tsdPtr->tm;

    /*
     * Compute the 4 year span containing the specified time.
     */

    tmp = *tp / SECSPER4YEAR;
    rem = *tp % SECSPER4YEAR;

    /*
     * Correct for weird mod semantics so the remainder is always positive.
     */

    if (rem < 0) {
	tmp--;
	rem += SECSPER4YEAR;
    }

    /*
     * Compute the year after 1900 by taking the 4 year span and adjusting
     * for the remainder.  This works because 2000 is a leap year, and
     * 1900/2100 are out of the range.
     */

    tmp = (tmp * 4) + 70;
    isLeap = 0;
    if (rem >= SECSPERYEAR) {			  /* 1971, etc. */
	tmp++;
	rem -= SECSPERYEAR;
	if (rem >= SECSPERYEAR) {		  /* 1972, etc. */
	    tmp++;
	    rem -= SECSPERYEAR;
	    if (rem >= SECSPERYEAR + SECSPERDAY) { /* 1973, etc. */
		tmp++;
		rem -= SECSPERYEAR + SECSPERDAY;
	    } else {
		isLeap = 1;
	    }
	}
    }
    tmPtr->tm_year = tmp;

    /*
     * Compute the day of year and leave the seconds in the current day in
     * the remainder.
     */

    tmPtr->tm_yday = rem / SECSPERDAY;
    rem %= SECSPERDAY;
    
    /*
     * Compute the time of day.
     */

    tmPtr->tm_hour = rem / 3600;
    rem %= 3600;
    tmPtr->tm_min = rem / 60;
    tmPtr->tm_sec = rem % 60;

    /*
     * Compute the month and day of month.
     */

    days = (isLeap) ? leapDays : normalDays;
    for (tmp = 1; days[tmp] < tmPtr->tm_yday; tmp++) {
    }
    tmPtr->tm_mon = --tmp;
    tmPtr->tm_mday = tmPtr->tm_yday - days[tmp];

    /*
     * Compute day of week.  Epoch started on a Thursday.
     */

    tmPtr->tm_wday = (*tp / SECSPERDAY) + 4;
    if ((*tp % SECSPERDAY) < 0) {
	tmPtr->tm_wday--;
    }
    tmPtr->tm_wday %= 7;
    if (tmPtr->tm_wday < 0) {
	tmPtr->tm_wday += 7;
    }

    return tmPtr;
}

/*
 *----------------------------------------------------------------------
 *
 * CalibrationThread --
 *
 *	Thread that manages calibration of the hi-resolution time
 *	derived from the performance counter, to keep it synchronized
 *	with the system clock.
 *
 * Parameters:
 *	arg -- Client data from the CreateThread call.  This parameter
 *             points to the static TimeInfo structure.
 *
 * Return value:
 *	None.  This thread embeds an infinite loop.
 *
 * Side effects:
 *	At an interval of clockCalibrateWakeupInterval ms, this thread
 *	performs virtual time discipline.
 *
 * Note: When this thread is entered, TclpInitLock has been called
 * to safeguard the static storage.  There is therefore no synchronization
 * in the body of this procedure.
 *
 *----------------------------------------------------------------------
 */

static DWORD WINAPI
CalibrationThread( LPVOID arg )
{
    FILETIME curFileTime;
    DWORD waitResult;

    /* Get initial system time and performance counter */

    GetSystemTimeAsFileTime( &curFileTime );
    QueryPerformanceCounter( &timeInfo.lastCounter );
    QueryPerformanceFrequency( &timeInfo.curCounterFreq );
    timeInfo.lastFileTime.LowPart = curFileTime.dwLowDateTime;
    timeInfo.lastFileTime.HighPart = curFileTime.dwHighDateTime;

    /* Initialize the working storage for the calibration callback */

    timeInfo.lastPerfCounter = timeInfo.lastCounter.QuadPart;
    timeInfo.estPerfCounterFreq = timeInfo.curCounterFreq.QuadPart;

    /*
     * Wake up the calling thread.  When it wakes up, it will release the
     * initialization lock.
     */

    SetEvent( timeInfo.readyEvent );

    /* Run the calibration once a second */

    for ( ; ; ) {

	/* If the exitEvent is set, break out of the loop. */

	waitResult = WaitForSingleObjectEx(timeInfo.exitEvent, 1000, FALSE);
	if ( waitResult == WAIT_OBJECT_0 ) {
	    break;
	}
	UpdateTimeEachSecond();
    }

    /* lint */
    return (DWORD) 0;
}

/*
 *----------------------------------------------------------------------
 *
 * UpdateTimeEachSecond --
 *
 *	Callback from the waitable timer in the clock calibration thread
 *	that updates system time.
 *
 * Parameters:
 *	info -- Pointer to the static TimeInfo structure
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Performs virtual time calibration discipline.
 *
 *----------------------------------------------------------------------
 */

static void
UpdateTimeEachSecond()
{

    LARGE_INTEGER curPerfCounter;
				/* Current value returned from
				 * QueryPerformanceCounter */

    LONGLONG perfCounterDiff;	/* Difference between the current value
				 * and the value of 1 second ago */

    FILETIME curSysTime;	/* Current system time */

    LARGE_INTEGER curFileTime;	/* File time at the time this callback
				 * was scheduled. */

    LONGLONG fileTimeDiff;	/* Elapsed time on the system clock
				 * since the last time this procedure
				 * was called */

    LONGLONG instantFreq;	/* Instantaneous estimate of the
				 * performance counter frequency */

    LONGLONG delta;		/* Increment to add to the estimated
				 * performance counter frequency in the
				 * loop filter */

    LONGLONG fuzz;		/* Tolerance for the perf counter frequency */

    LONGLONG lowBound;		/* Lower bound for the frequency assuming
				 * 1000 ppm tolerance */

    LONGLONG hiBound;		/* Upper bound for the frequency */

    /*
     * Get current performance counter and system time.
     */

    QueryPerformanceCounter( &curPerfCounter );
    GetSystemTimeAsFileTime( &curSysTime );
    curFileTime.LowPart = curSysTime.dwLowDateTime;
    curFileTime.HighPart = curSysTime.dwHighDateTime;

    EnterCriticalSection( &timeInfo.cs );

    /*
     * Find out how many ticks of the performance counter and the
     * system clock have elapsed since we got into this procedure.
     * Estimate the current frequency.
     */

    perfCounterDiff = curPerfCounter.QuadPart - timeInfo.lastPerfCounter;
    timeInfo.lastPerfCounter = curPerfCounter.QuadPart;
    fileTimeDiff = curFileTime.QuadPart - timeInfo.lastSysTime;
    timeInfo.lastSysTime = curFileTime.QuadPart;
    instantFreq = ( 10000000 * perfCounterDiff / fileTimeDiff );

    /*
     * Consider this a timing glitch if instant frequency varies
     * significantly from the current estimate.
     */

    fuzz = timeInfo.estPerfCounterFreq >> 10;
    lowBound = timeInfo.estPerfCounterFreq - fuzz;
    hiBound = timeInfo.estPerfCounterFreq + fuzz;
    if ( instantFreq < lowBound || instantFreq > hiBound ) {
	LeaveCriticalSection( &timeInfo.cs );
	return;
    }

    /*
     * Update the current estimate of performance counter frequency.
     * This code is equivalent to the loop filter of a phase locked
     * loop.
     */

    delta = ( instantFreq - timeInfo.estPerfCounterFreq ) >> 6;
    timeInfo.estPerfCounterFreq += delta;

    /*
     * Update the current virtual time.
     */

    timeInfo.lastFileTime.QuadPart
	+= ( ( curPerfCounter.QuadPart - timeInfo.lastCounter.QuadPart )
	     * 10000000 / timeInfo.curCounterFreq.QuadPart );
    timeInfo.lastCounter.QuadPart = curPerfCounter.QuadPart;

    delta = curFileTime.QuadPart - timeInfo.lastFileTime.QuadPart;
    if ( delta > 10000000 || delta < -10000000 ) {

	/*
	 * If the virtual time slip exceeds one second, then adjusting
	 * the counter frequency is hopeless (it'll take over fifteen
	 * minutes to line up with the system clock).  The most likely
	 * cause of this large a slip is a sudden change to the system
	 * clock, perhaps because it was being corrected by wristwatch
	 * and eyeball.  Accept the system time, and set the performance
	 * counter frequency to the current estimate.
	 */

	timeInfo.lastFileTime.QuadPart = curFileTime.QuadPart;
	timeInfo.curCounterFreq.QuadPart = timeInfo.estPerfCounterFreq;

    } else {

	/*
	 * Compute a counter frequency that will cause virtual time to line
	 * up with system time one second from now, assuming that the
	 * performance counter continues to tick at timeInfo.estPerfCounterFreq.
	 */
	
	timeInfo.curCounterFreq.QuadPart
	    = 10000000 * timeInfo.estPerfCounterFreq / ( delta + 10000000 );

	/*
	 * Limit frequency excursions to 1000 ppm from estimate
	 */
	
	if ( timeInfo.curCounterFreq.QuadPart < lowBound ) {
	    timeInfo.curCounterFreq.QuadPart = lowBound;
	} else if ( timeInfo.curCounterFreq.QuadPart > hiBound ) {
	    timeInfo.curCounterFreq.QuadPart = hiBound;
	}
    }

    LeaveCriticalSection( &timeInfo.cs );

}
