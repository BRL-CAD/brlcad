/*                         R U N . C
 * BRL-CAD
 *
 * Copyright (c) 2025 United States Government as represented by
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
/** @file run.c
 *
 * Orchestrator for the libbu IPC unit tests.
 *
 * Usage:  bu_ipc_test <path-to-ipc_worker>
 *
 * For each available transport (pipe, socket, tcp) this program:
 *   1. Creates a matched channel pair with bu_ipc_pair_prefer().
 *   2. Spawns ipc_worker, passing the child-end address via -I <addr>.
 *   3. Sends a request over the parent end and reads the reply.
 *   4. Verifies the round-trip result.
 *
 * Failure policy:
 *   - PIPE and SOCKET transports are expected to work everywhere.
 *     A failure on these transports is a hard test failure.
 *   - TCP transport may be unavailable in restricted CI environments.
 *     A failure to *create* the TCP channel is reported as a warning but
 *     does not fail the test.  However, if the channel is created
 *     successfully the full round-trip must succeed.
 *
 * Protocol (from ipc_worker.c):
 *   Request:  magic=0xDEADBEEF  value=<N>
 *   Reply:    magic=0xBEEFDEAD  value=<N*2>
 */

#include "common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bu.h"
#include "bu/ipc.h"


#define REQ_MAGIC  0xDEADBEEFU
#define REP_MAGIC  0xBEEFDEADU

#define TEST_VALUE 21u   /* worker returns 42 */


struct ipc_msg {
    unsigned int magic;
    unsigned int value;
};


/*
 * Run one round-trip test over the given transport.
 *
 * Returns:
 *   0  – pass
 *   1  – skip (transport unavailable; soft skip, not a failure)
 *  -1  – fail
 */
static int
run_transport_test(const char *worker_path,
		   bu_ipc_type_t transport,
		   const char *label)
{
    bu_ipc_chan_t *pe = NULL, *ce = NULL;
    int pair_ret;

    bu_log("  [%s] creating channel pair ...\n", label);
    pair_ret = bu_ipc_pair_prefer(&pe, &ce, transport);
    if (pair_ret != 0) {
	if (transport == BU_IPC_TCP) {
	    bu_log("  [%s] WARNING: channel creation failed (TCP may be blocked) — skipping\n",
		   label);
	    return 1;   /* soft skip */
	}
	bu_log("  [%s] FAIL: bu_ipc_pair_prefer() returned %d\n", label, pair_ret);
	return -1;
    }

    /* On Windows bu_process_create() sweeps fds 3..19 before exec(). Move
     * the child-end fd above that range so the child can inherit it.       */
    if (bu_ipc_move_high_fd(ce, 64) != 0) {
	bu_log("  [%s] FAIL: bu_ipc_move_high_fd() failed\n", label);
	bu_ipc_close(ce);
	bu_ipc_close(pe);
	return -1;
    }

    /* Save the address string before closing ce (which would free it). */
    char addr_buf[256];
    bu_strlcpy(addr_buf, bu_ipc_addr(ce), sizeof(addr_buf));

    /* Spawn the worker. */
    const char *argv[4];
    argv[0] = worker_path;
    argv[1] = "-I";
    argv[2] = addr_buf;
    argv[3] = NULL;

    /* Pass the IPC address via environment as well, as a fallback. */
    bu_setenv(BU_IPC_ADDR_ENVVAR, addr_buf, 1);

    struct bu_process *proc = NULL;
    bu_process_create(&proc, argv, BU_PROCESS_DEFAULT);

    bu_setenv(BU_IPC_ADDR_ENVVAR, "", 1);

    /* Parent closes its copy of the child end. */
    bu_ipc_close(ce);
    ce = NULL;

    if (!proc) {
	bu_log("  [%s] FAIL: failed to spawn worker '%s'\n", label, worker_path);
	bu_ipc_close(pe);
	return -1;
    }

    /* Send a request. */
    struct ipc_msg req;
    req.magic = REQ_MAGIC;
    req.value = TEST_VALUE;

    bu_ssize_t n = bu_ipc_write(pe, &req, sizeof(req));
    if (n != (bu_ssize_t)sizeof(req)) {
	bu_log("  [%s] FAIL: bu_ipc_write() returned %ld\n", label, (long)n);
	bu_process_wait_n(&proc, 5000000);
	bu_ipc_close(pe);
	return -1;
    }

    /* Read the reply. */
    struct ipc_msg rep;
    memset(&rep, 0, sizeof(rep));
    n = bu_ipc_read(pe, &rep, sizeof(rep));
    if (n != (bu_ssize_t)sizeof(rep)) {
	bu_log("  [%s] FAIL: bu_ipc_read() returned %ld\n", label, (long)n);
	bu_process_wait_n(&proc, 5000000);
	bu_ipc_close(pe);
	return -1;
    }

    /* Wait for worker to exit. */
    int status = bu_process_wait_n(&proc, 5000000 /* 5 s */);
    bu_ipc_close(pe);

    /* Verify reply. */
    if (rep.magic != REP_MAGIC) {
	bu_log("  [%s] FAIL: bad reply magic 0x%08X (expected 0x%08X)\n",
	       label, rep.magic, REP_MAGIC);
	return -1;
    }
    unsigned int expected = TEST_VALUE * 2u;
    if (rep.value != expected) {
	bu_log("  [%s] FAIL: bad reply value %u (expected %u)\n",
	       label, rep.value, expected);
	return -1;
    }
    if (status != 0) {
	bu_log("  [%s] FAIL: worker exited with status %d\n", label, status);
	return -1;
    }

    bu_log("  [%s] PASS: round-trip OK (sent %u, received %u)\n",
	   label, req.value, rep.value);
    return 0;
}


int
main(int argc, const char *argv[])
{
    bu_setprogname(argv[0]);

    if (argc < 2) {
	fprintf(stderr, "Usage: %s <path-to-ipc_worker>\n", argv[0]);
	return 1;
    }

    const char *worker_path = argv[1];
    if (!bu_file_exists(worker_path, NULL)) {
	fprintf(stderr, "%s: worker not found: %s\n", argv[0], worker_path);
	return 1;
    }

    bu_log("bu_ipc round-trip tests\n");
    bu_log("  worker: %s\n\n", worker_path);

    /* ------------------------------------------------------------------ */
    /* Test each transport in order                                         */
    /* ------------------------------------------------------------------ */

    struct {
	bu_ipc_type_t transport;
	const char   *label;
	int           required;  /* 0 = soft skip on pair failure, 1 = hard fail */
    } tests[] = {
	{ BU_IPC_PIPE,   "pipe",   1 },
	{ BU_IPC_SOCKET, "socket", 1 },
	{ BU_IPC_TCP,    "tcp",    0 },
    };

    int n_tests  = (int)(sizeof(tests) / sizeof(tests[0]));
    int n_pass   = 0;
    int n_skip   = 0;
    int n_fail   = 0;

    for (int i = 0; i < n_tests; i++) {
	int ret = run_transport_test(worker_path,
				     tests[i].transport,
				     tests[i].label);
	if (ret == 0) {
	    n_pass++;
	} else if (ret > 0) {
	    /* soft skip */
	    n_skip++;
	    if (tests[i].required) {
		bu_log("  [%s] required transport unavailable — FAIL\n",
		       tests[i].label);
		n_fail++;
	    }
	} else {
	    n_fail++;
	}
    }

    bu_log("\n--- Summary: %d passed, %d skipped, %d failed ---\n",
	   n_pass, n_skip, n_fail);

    return (n_fail > 0) ? 1 : 0;
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
