/*                     I N T E R R U P T . C
 * BRL-CAD
 *
 * Copyright (c) 2009-2014 United States Government as represented by
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

/* FIXME:  Should use sigaction(2) instead of BSD signal semantics for
 * conformance, portability, and safety. */
#if defined(C99_POSIX_USE_BSD)
/* defining _BSD_SOURCE should ensure BSD signal semantics as well
 * as sig_t for glibc on Linux according to 'man signal(2)'
 */
#if !defined(_BSD_SOURCE)
#define _BSD_SOURCE
#endif
#endif

#include "common.h"

#include <signal.h>

#include "bu.h"

/* wrap for hack above */
#if !defined(C99_POSIX_USE_BSD)
/* orig code: */
#ifndef HAVE_SIG_T
typedef void (*sig_t)(int);
#endif
#endif

/* hard-coded maximum signal number we can defer due to array we're
 * using for quick O(1) access in a single container for all signals.
 */
#define _BU_MAX_SIGNUM 128

/* keeps track of whether signal processing is put on hold */
volatile sig_atomic_t interrupt_defer_signal[_BU_MAX_SIGNUM] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

/* keeps track of whether a signal was received while on hold */
volatile sig_atomic_t interrupt_signal_pending[_BU_MAX_SIGNUM] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

/* keeps track of the installed signal handler that is suspended */
volatile sig_t interrupt_signal_func[_BU_MAX_SIGNUM] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};


/* temporary signal handler to detect when a signal that has been
 * suspended is raised.
 */
static void
interrupt_suspend_signal_handler(int signum)
{
    if (interrupt_defer_signal[signum])
	interrupt_signal_pending[signum]++;
}


/**
 * Defer signal processing for critical sections.
 *
 * Signal processing for a given 'signum' signal is put on hold until
 * interrupt_restore_signal() is called.  If a signal is received while
 * suspended, it will be raised when/if the signal is restored.
 *
 * Returns non-zero on error (with perror set if signal() failure).
 * Returns 1 if already suspended.
 * Returns 2 if signal failure.
 *
 * This comment should be moved to bu.h if this HIDDEN function is
 * publicly exposed.
 */
HIDDEN int
interrupt_suspend_signal(int signum)
{
    BU_ASSERT(signum < _BU_MAX_SIGNUM && "signal number out of range");

    if (interrupt_signal_func[signum] == interrupt_suspend_signal_handler) {
	return 1;
    }


#if !defined(BRLCAD_USE_SIGACTION)
    interrupt_signal_func[signum] = signal(signum, interrupt_suspend_signal_handler);
#else
    interrupt_signal_func[signum] = signal(signum, interrupt_suspend_signal_handler);
#endif

    if (interrupt_signal_func[signum] == SIG_ERR) {
	interrupt_signal_func[signum] = (sig_t)0;
	return 2;
    }
    interrupt_signal_pending[signum] = 0;
    interrupt_defer_signal[signum]++;

    return 0;
}


/**
 * Restore signal processing for a given suspended signal.
 *
 * If a signal was raised since interrupt_suspend_signal() was called, the
 * previously installed signal handler will be immediately called
 * albeit only once even if multiple signals were received.
 *
 * Returns non-zero on error (with perror set if signal() failure).
 * Returns 1 if unexpected suspend state.
 * Returns 2 if signal failure.
 *
 * This comment should be moved to bu.h if this HIDDEN function is
 * publicly exposed.
 */
HIDDEN int
interrupt_restore_signal(int signum)
{
    BU_ASSERT(signum < _BU_MAX_SIGNUM && "signal number out of range");

    /* must be before the test to avoid a race condition */
    interrupt_defer_signal[signum]--;

    if (interrupt_defer_signal[signum] == 0 && interrupt_signal_pending[signum] != 0) {
	sig_t ret;

	if (interrupt_signal_func[signum] != interrupt_suspend_signal_handler) {
	    /* unexpected state, how did we get here? */
	    return 1;
	}

#if !defined(BRLCAD_USE_SIGACTION)
	ret = signal(signum, interrupt_signal_func[signum]);
#else
	ret = signal(signum, interrupt_signal_func[signum]);
#endif

	interrupt_signal_func[signum] = (sig_t)0;
	interrupt_signal_pending[signum] = 0;

	if (ret == SIG_ERR) {
	    return 2;
	}
	raise(signum);
    }

    return 0;
}


int
bu_suspend_interrupts()
{
    int ret = 0;

#ifdef SIGINT
    ret += interrupt_suspend_signal(SIGINT);
#endif
#ifdef SIGHUP
    ret += interrupt_suspend_signal(SIGHUP);
#endif
#ifdef SIGQUIT
    ret += interrupt_suspend_signal(SIGQUIT);
#endif
#ifdef SIGTSTP
    ret += interrupt_suspend_signal(SIGTSTP);
#endif

    /* should do something sensible on Windows here */

    if (ret > 0)
	return 1;
    return 0;
}


int
bu_restore_interrupts()
{
    int ret = 0;

#ifdef SIGINT
    ret += interrupt_restore_signal(SIGINT);
#endif
#ifdef SIGHUP
    ret += interrupt_restore_signal(SIGHUP);
#endif
#ifdef SIGQUIT
    ret += interrupt_restore_signal(SIGQUIT);
#endif
#ifdef SIGTSTP
    ret += interrupt_restore_signal(SIGTSTP);
#endif

    /* should do something sensible on Windows here */

    if (ret > 0)
	return 1;
    return 0;
}


/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
