/*                G E D _ T E S T _ I N T E R R U P T . C P P
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
/** @file ged_test_interrupt.cpp
 *
 * tests for libged interrupt infrastructure:
 *
 *   1. Accessor round-trip: request -> pending()==1, clear -> 0.
 *   2. Cross-thread visibility: a second thread sets the flag; the main
 *      thread observes it by polling (models the worker/GUI split).
 *   3. Clear-at-entry: a stale flag is cleared when a top-level command
 *      starts, so its result does NOT carry GED_INTERRUPTED.
 *   4. Auto-tag: a command that ends with the flag set has GED_INTERRUPTED
 *      OR'd into its result by ged_exec.  Two variants exercise the PRE
 *      path and the POST-callback-error path (the latter guards against
 *      the auto-tag being placed before the callback error assignment,
 *      which would drop the bit).
 */

#include "common.h"

#include <atomic>
#include <chrono>
#include <thread>

#include <stdio.h>
#include <bu.h>
#include <ged.h>

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

/* Callback that requests an interrupt and reports success (PRE variant). */
static int
clbk_request_ok(int UNUSED(ac), const char **UNUSED(av), void *u1, void *UNUSED(u2))
{
    ged_interrupt_request((struct ged *)u1);
    return BRLCAD_OK;
}

/* Callback that requests an interrupt and reports failure (POST-error
 * variant).  A non-OK return makes ged_exec assign the callback's code to
 * ged_results->ret; the auto-tag must still survive that overwrite. */
static int
clbk_request_err(int UNUSED(ac), const char **UNUSED(av), void *u1, void *UNUSED(u2))
{
    ged_interrupt_request((struct ged *)u1);
    return BRLCAD_ERROR;
}

int
main(int ac, char *av[])
{
    struct ged *gedp;
    const char *ls[2] = {"ls", NULL};

    bu_setprogname(av[0]);

    if (ac != 2) {
	printf("Usage: %s file.g\n", av[0]);
	return 1;
    }
    if (!bu_file_exists(av[1], NULL)) {
	printf("ERROR: [%s] does not exist, expecting .g file\n", av[1]);
	return 2;
    }

    gedp = ged_open("db", av[1], 1);
    if (!gedp) {
	printf("ERROR: unable to open [%s]\n", av[1]);
	return 2;
    }

    /* 1. Accessor round-trip. */
    ged_interrupt_clear(gedp);
    check(ged_interrupt_pending(gedp) == 0, "clear -> pending()==0");
    ged_interrupt_request(gedp);
    check(ged_interrupt_pending(gedp) == 1, "request -> pending()==1");
    ged_interrupt_clear(gedp);
    check(ged_interrupt_pending(gedp) == 0, "clear -> pending()==0 (again)");

    /* NULL-safety of the accessors. */
    ged_interrupt_request(NULL);
    check(ged_interrupt_pending(NULL) == 0, "pending(NULL)==0");

    /* 2. Cross-thread visibility.  A helper thread raises the flag; the
     * main thread observes it by polling, exactly as a worker thread would
     * observe a Stop-button/signal-driven request.  
     * NOTE: This is an intentional cross-thread access of a volatile sig_atomic_t;
     * ThreadSanitizer, if enabled, will report it. */
    ged_interrupt_clear(gedp);
    {
	std::thread setter([&]() {
	    std::this_thread::sleep_for(std::chrono::milliseconds(5));
	    ged_interrupt_request(gedp);
	});
	int observed = 0;
	for (int i = 0; i < 10000 && !observed; i++) {
	    observed = ged_interrupt_pending(gedp);
	    if (!observed)
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}
	setter.join();
	check(observed == 1, "flag set on another thread is observed by poller");
    }
    ged_interrupt_clear(gedp);

    /* 3. Clear-at-entry: a stale flag must not leak into a fresh top-level
     * command's result.  Set the flag, run a benign command, and confirm the
     * result lacks GED_INTERRUPTED (ged_exec cleared it at depth 0). */
    ged_interrupt_request(gedp);
    {
	int r = ged_exec(gedp, 1, ls);
	check(!(r & GED_INTERRUPTED), "clear-at-entry: stale flag not tagged onto next command");
	check(ged_interrupt_pending(gedp) == 0, "clear-at-entry: flag is clear after command");
    }

    /* 4a. Auto-tag, PRE path: a PRE callback raises the flag (after the
     * entry clear), the command runs, and ged_exec tags the result. */
    ged_interrupt_clear(gedp);
    (void)ged_clbk_set(gedp, "ls", BU_CLBK_PRE, &clbk_request_ok, NULL);
    {
	int r = ged_exec(gedp, 1, ls);
	check((r & GED_INTERRUPTED) != 0, "auto-tag: PRE-set flag OR'd into result");
    }
    (void)ged_clbk_set(gedp, "ls", BU_CLBK_PRE, NULL, NULL); /* unregister */

    /* 4b. Auto-tag, POST-error path: a POST callback raises the flag and
     * returns non-OK, forcing ged_exec to assign its error code to the
     * result.  The auto-tag runs as the last mutation of ret, so
     * GED_INTERRUPTED must still be present.  This fails if the auto-tag is
     * placed before the callback-error assignment. */
    ged_interrupt_clear(gedp);
    (void)ged_clbk_set(gedp, "ls", BU_CLBK_POST, &clbk_request_err, NULL);
    {
	int r = ged_exec(gedp, 1, ls);
	check((r & GED_INTERRUPTED) != 0, "auto-tag: survives POST-callback-error overwrite");
	check((r & BRLCAD_ERROR) != 0, "auto-tag: POST error still reported as error");
    }
    (void)ged_clbk_set(gedp, "ls", BU_CLBK_POST, NULL, NULL); /* unregister */

    ged_close(gedp);

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
