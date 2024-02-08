/*                       P R O C E S S . C
 * BRL-CAD
 *
 * Copyright (c) 2013-2024 United States Government as represented by
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
/** @file bu_process.c
 *
 * Minimal test of the libbu subprocess API
 *
 */

#include "common.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "bu.h"

static void
exit_info(int retcode)
{
    if ( WIFEXITED(retcode) ) {
	int rc = WEXITSTATUS(retcode);
	bu_log("Normal exit, return code %d\n", rc);
    } else if (WIFSIGNALED(retcode) ) {
	int signum = WTERMSIG(retcode);
	printf("Exited due to receiving signal %d\n", signum);
    } else if (WIFSTOPPED(retcode) ) {
	int signum = WSTOPSIG(retcode);
	printf("Stopped due to receiving signal %d\n", signum);
    }
}


int
main(int ac, char *av[])
{
    struct bu_process *p = NULL;

    // Normally this file is part of bu_test, so only set this if it looks like
    // the program name is still unset.
    if (bu_getprogname()[0] == '\0')
	bu_setprogname(av[0]);

    if (ac != 3) {
	fprintf(stderr, "Usage: %s subprocess testname\n", av[0]);
	return 1;
    }
    fprintf(stdout, "%s\n%s\n", av[1], av[2]);
    if (!bu_file_exists(av[1], NULL)) {
	fprintf(stderr, "Program %s not found, cannot run test\n", av[1]);
	return 1;
    }

    if (BU_STR_EQUAL(av[2], "basic")) {
	const char *pav[3];
	int count = 0;
	int aborted = 0;
	char line[101] = {0};
	pav[0] = av[1];
	pav[1] = av[2];
	pav[2] = NULL;
	bu_process_exec(&p, av[1], 2, (const char **)pav, 0, 0);

	if (bu_process_read((char *)line, &count, p, BU_PROCESS_STDOUT, 100) <= 0) {
	    fprintf(stderr, "bu_process_test stdin read failed\n");
	    return 1;
	} else {
	    fprintf(stdout, "got: %s\n", line);
	}
	int ret = bu_process_wait(&aborted, p, 120);
	if (ret == -1) {
	    fprintf(stderr, "bu_process_test - wait failed\n");
	    exit_info(ret);
	    return 1;
	}
	if (aborted) {
	    fprintf(stderr, "bu_process_test - process unexpectedly aborted\n");
	    exit_info(ret);
	    return 1;
	}
	exit_info(ret);
    }

    if (BU_STR_EQUAL(av[2], "abort")) {
	const char *pav[3];
	int aborted = 0;
	pav[0] = av[1];
	pav[1] = av[2];
	pav[2] = NULL;
	bu_process_exec(&p, av[1], 2, (const char **)pav, 0, 0);

	fprintf(stderr, "terminating %d\n", bu_process_pid(p));

	bu_terminate(bu_process_pid(p));

	int ret = bu_process_wait(&aborted, p, 1);
	if (!aborted) {
	    fprintf(stderr, "bu_process_test - bu_process didn't correctly report an aborted subprocess\n");
	    exit_info(ret);
	    return 1;
	}
	exit_info(ret);
    }

    if (BU_STR_EQUAL(av[2], "error")) {
	const char *pav[3];
	int aborted = 0;
	pav[0] = av[1];
	pav[1] = av[2];
	pav[2] = NULL;
	bu_process_exec(&p, av[1], 2, (const char **)pav, 0, 0);

	int ret = bu_process_wait(&aborted, p, 120);
	if (WEXITSTATUS(ret) != 2) {
	    fprintf(stderr, "bu_process_test - unexpected return code %d\n", WEXITSTATUS(ret));
	    exit_info(ret);
	    return 1;
	}
	if (aborted) {
	    fprintf(stderr, "bu_process_test - process unexpectedly aborted\n");
	    exit_info(ret);
	    return 1;
	}
	exit_info(ret);
    }


    return 0;
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
