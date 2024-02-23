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

#define PROCESS_ERROR 100
#define PROCESS_FAIL  1
#define PROCESS_PASS  0

/* tests:
 *  bu_process_read() [stdout and stderr]
 * also relies on:
 *  bu_process_exec()
 *  bu_process_wait()
 */
int _test_read(const char* cmd) {
    struct bu_process* p;
    const char* run_av[3] = {cmd, "basic", NULL};
    int count = 0;
    int aborted = 0;
    char line[100] = {0};

    bu_process_exec(&p, cmd, 2, (const char**)run_av, 0, 0);

    if (bu_process_read((char *)line, &count, p, BU_PROCESS_STDOUT, 100) <= 0) {
	fprintf(stderr, "bu_process_test[\"basic\"] stdin read failed\n");
	return PROCESS_FAIL;
    }
    if (bu_process_read((char *)line, &count, p, BU_PROCESS_STDERR, 100) <= 0) {
	fprintf(stderr, "bu_process_test[\"basic\"] stdout read failed\n");
	return PROCESS_FAIL;
    }

    if (bu_process_wait(&aborted, p, 0)) {
	fprintf(stderr, "bu_process_test[\"basic\"] - wait failed\n");
	return PROCESS_FAIL;
    }

    return PROCESS_PASS;
}

/* tests:
 *  bu_process_exec()
 *  bu_process_wait()
 */
int _test_exec_wait(const char* cmd) {
    struct bu_process* p;
    const char* run_av[3] = {cmd, "basic", NULL};

    // NOTE: add tests to check options flags
    bu_process_exec(&p, cmd, 2, (const char**)run_av, 0, 0);

    if (bu_process_wait(NULL, p, 0)) {
	fprintf(stderr, "bu_process_test[\"basic\"] - wait failed\n");
	return PROCESS_FAIL;
    }

    return PROCESS_PASS;
}

/* tests:
 *  bu_process_id()
 *  bu_process_pid()
 * also relies on
 *  bu_process_exec()
 *  bu_process_wait()
 */
int _test_ids(const char* cmd) {
    struct bu_process* p = NULL;
    const char* run_av[3] = {cmd, "basic", NULL};

    bu_process_exec(&p, cmd, 2, (const char**)run_av, 0, 0);

    int curr_id = bu_process_id();
    int process_id = bu_process_pid(p);

    if (curr_id <= 0) {
	fprintf(stderr, "bu_process_test[\"ids\"] bu_process_id() should not be 0");
	return PROCESS_FAIL;
    }
    if (process_id <= 0) {
	fprintf(stderr, "bu_process_test[\"ids\"] bu_process_pid() should not be 0");
	return PROCESS_FAIL;
    }
    if (curr_id == process_id) {
	fprintf(stderr, "bu_process_test[\"ids\"] bu_process_pid() should not equal bu_process_id()");
	return PROCESS_FAIL;
    }

    if (bu_process_wait(NULL, p, 0)) {
	fprintf(stderr, "bu_process_test[\"ids\"] - wait failed\n");
	return PROCESS_FAIL;
    }

    return PROCESS_PASS;
}

/* tests:
 *  bu_process_open(),
 *  bu_process_close(),
 *  bu_process_fileno(),
 *  bu_process_pending()
 * also relies on:
 *  bu_process_exec(),
 *  bu_process_wait()
 */
int _test_streams(const char* cmd) {
    struct bu_process* p = NULL;
    const char* run_av[3] = {cmd, "echo", NULL};

    bu_process_exec(&p, cmd, 2, (const char**)run_av, 0, 0);

    FILE* f_in = bu_process_open(p, BU_PROCESS_STDIN);

    // send a test line through stdin
    char line[10] = "echo_test";
    fputs(line, f_in);
    // subprocess is using cin.get() -> need to send newline and flush so it'll move on
    fputs("\n", f_in);	fflush(f_in);

    // subprocess should echo on stdout and stderr
    char out_read[10], err_read[10];
    FILE* f_out = bu_process_open(p, BU_PROCESS_STDOUT);
    FILE* f_err = bu_process_open(p, BU_PROCESS_STDERR);
    int fd_out = bu_process_fileno(p, BU_PROCESS_STDOUT);
    int fd_err = bu_process_fileno(p, BU_PROCESS_STDERR);

    // give up to 5 seconds for process_pending to get the echo
    int64_t start = bu_gettime();
    while (!bu_process_pending(fd_out) && !bu_process_pending(fd_out)) {
	if ((bu_gettime() - start) > BU_SEC2USEC(5)) {
	    fprintf(stderr, "bu_process_test[\"streams\"] - process_pending check failed\n");
	    return PROCESS_FAIL;
	}
    }

    if (bu_process_pending(fd_out)) {
	fgets(out_read, 10, f_out);
    } else {
	fprintf(stderr, "bu_process_test[\"streams\"] - expected pending data on stdout\n");
	return PROCESS_FAIL;
    }

    if (bu_process_pending(fd_err)) {
	fgets(err_read, 10, f_err);
    } else {
	fprintf(stderr, "bu_process_test[\"streams\"] - expected pending data on stderr\n");
	return PROCESS_FAIL;
    }

    // verify echo's were correct
    if (bu_strncmp(out_read, err_read, 10) || bu_strncmp(out_read, line, 10)) {
	fprintf(stderr, "bu_process_test[\"streams\"] - bad echo data\n");
	return PROCESS_FAIL;
    }

    bu_process_close(p, BU_PROCESS_STDIN);
    bu_process_close(p, BU_PROCESS_STDOUT);
    bu_process_close(p, BU_PROCESS_STDERR);

    if (bu_process_wait(NULL, p, 0) == -1) {
	fprintf(stderr, "bu_process_test[\"streams\"] - wait failed\n");
	return PROCESS_FAIL;
    }

    return PROCESS_PASS;
}


/* tests:
 *  bu_terminate()
 * also relies on:
 *  bu_process_exec()
 *  bu_process_pid()
 *  bu_process_wait()
 */
int _test_abort(const char* cmd) {
    struct bu_process* p = NULL;
    const char* run_av[3] = {cmd, "echo", NULL};	// 'echo' test has inf wait on cin.get()
    int aborted = 0;
    bu_process_exec(&p, cmd, 2, (const char**)run_av, 0, 0);

    if (!bu_terminate(bu_process_pid(p))) {
	fprintf(stderr, "bu_process_test[\"terminate\"] - subprocess could not be terminated\n");
	return PROCESS_FAIL;
    }

    if (!bu_process_wait(&aborted, p, 0)) {
        fprintf(stderr, "bu_process_test[\"terminate\"] - wait should have reported abort code\n");
	return PROCESS_FAIL;
    }

    if (!aborted) {
        fprintf(stderr, "bu_process_test[\"terminate\"] - bu_process didn't correctly report an aborted subprocess\n");
        return PROCESS_FAIL;
    }

    return PROCESS_PASS;
}


/* tests:
 *  bu_process_args()
 * also relies on:
 *  bu_process_exec()
 *  bu_process_wait()
 */
int _test_args(const char* cmd) {
    struct bu_process* p;
    const char* run_av[3] = {cmd, "basic", NULL};
    int ck_argc;
    const char* ck_cmd;
    const char* const* ck_argv;

    bu_process_exec(&p, cmd, 2, (const char**)run_av, 0, 0);

    ck_argc = bu_process_args(&ck_cmd, &ck_argv, p);

    if (ck_argc != 2) {
	fprintf(stderr, "bu_process_test[\"argv\"] - ck_argc got (%d), expected (%d)\n", ck_argc, 2);
	return PROCESS_FAIL;
    }

    if (bu_strncmp(cmd, ck_cmd, 100)) {
	fprintf(stderr, "bu_process_test[\"argv\"] - ck_cmd got (%s), expected (%s)\n", ck_cmd, cmd);
	return PROCESS_FAIL;
    }

    for (int i = 0; i < ck_argc; i++) {
	if (bu_strncmp(run_av[i], ck_argv[i], 100)) {
	    fprintf(stderr, "bu_process_test[\"argv\"] - ck_argv idx (%d) got (%s), expected (%s)\n", i, ck_cmd, run_av[i]);
	    return PROCESS_FAIL;
	}
    }

    if (bu_process_wait(NULL, p, 0)) {
        fprintf(stderr, "bu_process_test[\"argv\"] - wait failed\n");
	return PROCESS_FAIL;
    }

    return PROCESS_PASS;
}

typedef struct {
    const char* name;
    int (*func)(const char*);
} ProcessTest;

ProcessTest tests[] = {
    {"exec_wait", _test_exec_wait},
    {"read", _test_read},
    {"ids", _test_ids},
    {"streams", _test_streams},
    {"abort", _test_abort},
    {"args", _test_args},
};

int run_test(char* av[]) {
    int failed = 0;

    int test_all = (BU_STR_EQUAL(av[2], "all")) ? 1 : 0;

    for (size_t i = 0; i < sizeof(tests) / sizeof(tests[0]); i++) {
	if (test_all || BU_STR_EQUAL(av[2], tests[i].name)) {
	    // echo what we're about to run
	    fprintf(stdout, "Running: \"%s %s\"\n", av[1], tests[i].name);

	    failed += tests[i].func(av[1]);
	}
    }

    return failed;
}

int
main(int ac, char *av[])
{
    // Normally this file is part of bu_test, so only set this if it looks like
    // the program name is still unset.
    if (bu_getprogname()[0] == '\0')
	bu_setprogname(av[0]);

    // check usage
    if (ac != 3) {
	const char* usage = "subprocess_exe testname";
	fprintf(stderr, "Usage: %s %s\n", av[0], usage);
	return PROCESS_ERROR;
    }
    // ensure subprocess_executable exists
    if (!bu_file_exists(av[1], NULL)) {
	fprintf(stderr, "Program %s not found, cannot run test\n", av[1]);
	return PROCESS_ERROR;
    }

    return run_test(av);
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
