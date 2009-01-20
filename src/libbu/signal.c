/*                        S I G N A L . C
 * BRL-CAD
 *
 * Copyright (c) 2009 United States Government as represented by
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
/** @file signal.c
 *
 * Routines for managing signals.  In particular, provide a common
 * means to temporarily buffer processing a signal during critical
 * write operations.
 *
 */

#include "common.h"

#include <signal.h>
#include <assert.h>


/* hard-coded maximum signal number we can defer due to array we're
 * using for quick O(1) access in a single container for all signals.
 */
#define _BU_MAX_SIGNUM 128

/* keeps track of whether signal processing is put on hold */
volatile sig_atomic_t _bu_defer_signal = 0;

/* keeps track of whether a signal was received while on hold */
volatile sig_atomic_t _bu_signal_pending[_BU_MAX_SIGNUM] = {
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
volatile sig_t _bu_signal_func[_BU_MAX_SIGNUM] = {
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
_bu_suspend_signal_handler(int signum)
{
    if (_bu_defer_signal)
	_bu_signal_pending[signum]++;
}


int
bu_suspend_signal(int signum)
{
    assert(signum < _BU_MAX_SIGNUM && "signal number out of range");

    _bu_signal_func[signum] = signal(signum, _bu_suspend_signal_handler);
    if (_bu_signal_func[signum] == SIG_ERR) {
	_bu_signal_func[signum] = (sig_t)0;
	return 1;
    }
    _bu_signal_pending[signum] = 0;
    _bu_defer_signal++;

    return 0;
}


int
bu_restore_signal(int signum)
{
    assert(signum < _BU_MAX_SIGNUM && "signal number out of range");

    /* must be before the test to avoid a race condition */
    _bu_defer_signal--;

    if (_bu_defer_signal == 0 && _bu_signal_pending[signum] != 0) {
	sig_t ret = signal(signum, _bu_signal_func[signum]);
	_bu_signal_func[signum] = (sig_t)0;
	_bu_signal_pending[signum] = 0;

	if (ret == SIG_ERR) {
	    return 1;
	}
	raise(signum);
    }

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
