/*            M G E D _ T E S T _ I N T E R R U P T . C P P
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
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
/** @file mged_test_interrupt.cpp
 *
 * Headless unit tests for MGED's cooperative interrupt policy (Increment 1):
 *
 *   1. mged_interrupt_service  -- the mged_heartbeat rising-edge reactor:
 *      fires exactly once on a 0->1 transition, not while the flag stays
 *      raised, re-arms after the flag returns to 0, and gates the human-facing
 *      "stopping" log on whether anything is actually running.
 *   2. mged_interrupt_should_act -- the <Escape>/mged_interrupt guard: acts
 *      only when an in-process command is running or a subprocess exists.
 *
 * These functions encode the whole interrupt decision; the surrounding signal
 * handlers and Tcl glue are thin wrappers verified by inspection (async-signal
 * safety) and by the GUI manual checklist.
 */

#include "common.h"

#include <stdio.h>

#include "bu.h"

#include "mged_interrupt.h"

static int failures = 0;

static void
check(int cond, const char *msg)
{
    if (!cond) {
	printf("FAIL: %s\n", msg);
	failures++;
    } else {
	printf("ok:   %s\n", msg);
    }
}

int
main(int UNUSED(ac), char *av[])
{
    bu_setprogname(av[0]);

    /* ------------------------------------------------------------------ *
     * 1. mged_interrupt_service: rising-edge reactor.
     * ------------------------------------------------------------------ */

    /* No request pending: never reacts, never logs. */
    {
	int log = 7; /* sentinel: must be overwritten to 0 */
	check(mged_interrupt_service(0, 0, 0, 0, &log) == 0, "service: idle 0->0 does not react");
	check(log == 0, "service: idle 0->0 sets should_log=0");
    }

    /* 0->1 with an in-process command running: react + log. */
    {
	int log = 0;
	check(mged_interrupt_service(0, 1, 1, 0, &log) == 1, "service: 0->1 (cmd running) reacts");
	check(log == 1, "service: 0->1 (cmd running) logs");
    }

    /* 0->1 with a subprocess (rt) but no in-process command: react + log. */
    {
	int log = 0;
	check(mged_interrupt_service(0, 1, 0, 2, &log) == 1, "service: 0->1 (subproc) reacts");
	check(log == 1, "service: 0->1 (subproc) logs");
    }

    /* 0->1 with nothing running (flag raised while idle): still reacts once
     * (terminate is a no-op on empty ged_subp) but does NOT log. */
    {
	int log = 1; /* sentinel: must be overwritten to 0 */
	check(mged_interrupt_service(0, 1, 0, 0, &log) == 1, "service: 0->1 (idle) still reacts");
	check(log == 0, "service: 0->1 (idle) does not log");
    }

    /* 1->1: flag stays raised -- must NOT react again (react exactly once). */
    {
	int log = 9; /* sentinel: must be overwritten to 0 on a non-edge */
	check(mged_interrupt_service(1, 1, 1, 3, &log) == 0, "service: 1->1 does not re-react");
	check(log == 0, "service: 1->1 sets should_log=0");
    }

    /* 1->0: flag cleared -- no reaction, re-arms for the next edge. */
    {
	int log = 5;
	check(mged_interrupt_service(1, 0, 0, 0, &log) == 0, "service: 1->0 does not react");
	check(log == 0, "service: 1->0 sets should_log=0");
    }

    /* Full cycle: 0->1 (once) then 1->1 (silent) then re-arm 1->0 then 0->1. */
    {
	int prev = 0;
	int reacts = 0;
	int pendings[] = {1, 1, 1, 0, 1};  /* raise, hold, hold, clear, raise */
	for (int i = 0; i < 5; i++) {
	    if (mged_interrupt_service(prev, pendings[i], 1, 0, NULL))
		reacts++;
	    prev = pendings[i];
	}
	check(reacts == 2, "service: two distinct rising edges over a full cycle react twice");
    }

    /* should_log NULL is tolerated. */
    check(mged_interrupt_service(0, 1, 1, 0, NULL) == 1, "service: NULL should_log tolerated on edge");

    /* ------------------------------------------------------------------ *
     * 2. mged_interrupt_should_act: <Escape> / mged_interrupt guard.
     * ------------------------------------------------------------------ */
    check(mged_interrupt_should_act(0, 0) == 0, "should_act: idle + no subproc -> no-op");
    check(mged_interrupt_should_act(1, 0) != 0, "should_act: cmd running -> act");
    check(mged_interrupt_should_act(0, 1) != 0, "should_act: subproc present -> act");
    check(mged_interrupt_should_act(1, 5) != 0, "should_act: both -> act");

    printf("\n%s: %d failure(s)\n", av[0], failures);
    return (failures == 0) ? 0 : 1;
}

/*
 * Local Variables:
 * tab-width: 8
 * mode: C++
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
