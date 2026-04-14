/*                    I P C _ W O R K E R . C
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
/** @file ipc_worker.c
 *
 * Child process for the libbu IPC unit test.
 *
 * Usage:  ipc_worker -I <addr>
 *
 * Reads a fixed-size request from the IPC channel, computes a simple
 * reply, and writes it back.  The parent (run.c) verifies the round-trip.
 *
 * Protocol (all values little-endian uint32_t):
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


struct ipc_msg {
    unsigned int magic;
    unsigned int value;
};


int
main(int argc, const char *argv[])
{
    bu_setprogname(argv[0]);

    const char *addr = NULL;

    /* Parse -I <addr> */
    for (int i = 1; i < argc; i++) {
	if (BU_STR_EQUAL(argv[i], "-I") && i + 1 < argc) {
	    addr = argv[++i];
	} else {
	    fprintf(stderr, "ipc_worker: unknown arg '%s'\n", argv[i]);
	    return 1;
	}
    }

    if (!addr) {
	fprintf(stderr, "ipc_worker: missing -I <addr>\n");
	return 1;
    }

    bu_ipc_chan_t *chan = bu_ipc_connect(addr);
    if (!chan) {
	fprintf(stderr, "ipc_worker: bu_ipc_connect('%s') failed\n", addr);
	return 1;
    }

    struct ipc_msg req;
    bu_ssize_t n = bu_ipc_read(chan, &req, sizeof(req));
    if (n != (bu_ssize_t)sizeof(req)) {
	fprintf(stderr, "ipc_worker: short read (%ld)\n", (long)n);
	bu_ipc_close(chan);
	return 1;
    }

    if (req.magic != REQ_MAGIC) {
	fprintf(stderr, "ipc_worker: bad request magic 0x%08X\n", req.magic);
	bu_ipc_close(chan);
	return 1;
    }

    struct ipc_msg rep;
    rep.magic = REP_MAGIC;
    rep.value = req.value * 2u;

    n = bu_ipc_write(chan, &rep, sizeof(rep));
    if (n != (bu_ssize_t)sizeof(rep)) {
	fprintf(stderr, "ipc_worker: short write (%ld)\n", (long)n);
	bu_ipc_close(chan);
	return 1;
    }

    bu_ipc_close(chan);
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
