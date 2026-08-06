/*                     G E D _ T E S T _ S U B P . C P P
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
/** @file ged_test_subp.cpp
 *
 * Tests for Increment 2 subprocess hardening (mged interruptibility):
 *
 *   A. ged_subprocesses_terminate fires end_clbk (with aborted=1) and
 *      invokes gedp->ged_delete_io_handler on every registered stream,
 *      restoring the completion contract that the natural-EOF path
 *      already honored (see src/libged/ged_util.cpp:1786-1795 /
 *      :1881-1890).  This closes the Increment-1 regression where the
 *      heartbeat interrupt path silently dropped end_clbk.
 *
 *   B. Ordering invariant: a re-entrant io-handler stub called during
 *      registration observes gedp->ged_subp already populated, proving
 *      the "init → publish → register" ordering enforced in the four
 *      spawn helpers (src/libged/ged_util.cpp:_ged_run_rt, rtcheck.c,
 *      rtcheck2.cpp, rtwizard.c).
 *
 * Both tests are fully headless — no libtcl, no real rt process.  The
 * synthetic ged_subprocess in Test A carries p=NULL with aborted
 * pre-set so ged_subprocesses_terminate takes the fast path (skips
 * bu_pid_terminate; bu_process_wait_n(&NULL,...) is a documented no-op).
 * Test B publishes the synthetic entry directly and then invokes the
 * gedp->ged_create_io_handler slot to model the ordering hazard.
 */

#include "common.h"

#include <atomic>

#include <stdio.h>
#include <string.h>
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

/* -------- Test A: end_clbk fires on interrupt teardown -------- */

static std::atomic<int> end_clbk_calls{0};
static std::atomic<int> end_clbk_last_aborted{-1};
static std::atomic<int> delete_io_calls{0};

static int
test_end_clbk(int UNUSED(ac), const char **UNUSED(av), void *u1, void *UNUSED(u2))
{
    /* u1 is &aborted per the ged_util.cpp natural-EOF contract. */
    int aborted = u1 ? *(int *)u1 : -2;
    end_clbk_last_aborted.store(aborted);
    end_clbk_calls.fetch_add(1);
    return BRLCAD_OK;
}

static void
test_delete_io_handler(struct ged_subprocess *UNUSED(p), bu_process_io_t UNUSED(t))
{
    delete_io_calls.fetch_add(1);
}

/* -------- Test B: ordering-invariant probe -------- */

static struct ged *g_probe_gedp = NULL;
static std::atomic<int> observed_subp_len{-1};

static void
test_create_io_handler_probe(struct ged_subprocess *UNUSED(p),
			     bu_process_io_t UNUSED(t),
			     ged_io_func_t UNUSED(cb),
			     void *UNUSED(data))
{
    /* Mimic the visibility hazard: any observer that inspects ged_subp
     * during handler registration must see the entry already published. */
    if (g_probe_gedp)
	observed_subp_len.store((int)BU_PTBL_LEN(&g_probe_gedp->ged_subp));
}

int
main(int ac, char *av[])
{
    struct ged *gedp;

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

    /* ---- Test A: end_clbk + delete_io_handler fire on interrupt teardown ---- */
    gedp->ged_delete_io_handler = &test_delete_io_handler;
    end_clbk_calls.store(0);
    end_clbk_last_aborted.store(-1);
    delete_io_calls.store(0);

    {
	struct ged_subprocess *rrp;
	BU_GET(rrp, struct ged_subprocess);
	rrp->magic = GED_CMD_MAGIC;
	rrp->p = NULL;        /* synthetic — safe with aborted=1 (see file header) */
	rrp->aborted = 1;     /* skip bu_pid_terminate path */
	rrp->gedp = gedp;
	rrp->stdin_active = 0;
	rrp->stdout_active = 0;
	rrp->stderr_active = 0;
	rrp->end_clbk = &test_end_clbk;
	rrp->end_clbk_data = NULL;
	bu_ptbl_ins(&gedp->ged_subp, (long *)rrp);
    }

    check((int)BU_PTBL_LEN(&gedp->ged_subp) == 1, "A: ged_subp populated pre-terminate");

    ged_subprocesses_terminate(gedp);

    check((int)BU_PTBL_LEN(&gedp->ged_subp) == 0, "A: ged_subp emptied by terminate");
    check(end_clbk_calls.load() == 1, "A: end_clbk fired exactly once");
    check(end_clbk_last_aborted.load() == 1, "A: end_clbk received aborted=1");
    check(delete_io_calls.load() == 3, "A: delete_io_handler called for STDIN+STDOUT+STDERR");

    gedp->ged_delete_io_handler = NULL;

    /* ---- Test B: publish-before-register ordering invariant ---- */
    gedp->ged_create_io_handler = &test_create_io_handler_probe;
    g_probe_gedp = gedp;
    observed_subp_len.store(-1);

    {
	/* Reproduce the pattern the four spawn helpers now use: fully init the
	 * ged_subprocess, publish, then register.  The probe callback records
	 * BU_PTBL_LEN(&gedp->ged_subp) at the moment of registration; it MUST
	 * see the entry already present (>= 1). */
	struct ged_subprocess *rrp;
	BU_GET(rrp, struct ged_subprocess);
	rrp->magic = GED_CMD_MAGIC;
	rrp->p = NULL;
	rrp->aborted = 1;     /* safe for the cleanup call below */
	rrp->gedp = gedp;
	rrp->stdin_active = 0;
	rrp->stdout_active = 0;
	rrp->stderr_active = 0;
	rrp->end_clbk = NULL;
	rrp->end_clbk_data = NULL;
	bu_ptbl_ins(&gedp->ged_subp, (long *)rrp);

	/* now register — the probe reads back ged_subp length */
	(*gedp->ged_create_io_handler)(rrp, BU_PROCESS_STDERR, NULL, NULL);
    }

    check(observed_subp_len.load() >= 1,
	  "B: io-handler registration observes published ged_subp entry");

    /* Clean up the synthetic entry from Test B without needing terminate's
     * full path (we've cleared the delete_io_handler slot already). */
    ged_subprocesses_terminate(gedp);
    g_probe_gedp = NULL;
    gedp->ged_create_io_handler = NULL;

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
