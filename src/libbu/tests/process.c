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

// Tests requiring non-blocking reading behavior - THEY WILL INF LOOP IN CURRENT FORM
#define BU_PROCESS_ASYNC 0

#define PROCESS_ERROR 100
#define PROCESS_FAIL  1
#define PROCESS_PASS  0

void gettime_delay(long ms) {
    int64_t start_time = bu_gettime();
    while ((bu_gettime() - start_time) < ms)	;
}

/* tests:   single stdout and stderr reads - BLOCKING READS
 *  bu_process_read() [stdout and stderr]
 * also relies on:
 *  bu_process_create()
 *  bu_process_wait()
 */
int _test_read(const char* cmd) {
    struct bu_process* p;
    const char* run_av[3] = {cmd, "output", NULL};
    char line[100] = {0};

    bu_process_create(&p, (const char**)run_av, BU_PROCESS_DEFAULT);

    if (bu_process_read_n(p, BU_PROCESS_STDOUT, 100, (char *)line) <= 0) {
	fprintf(stderr, "bu_process_test[\"read\"] stdin read failed\n");
	return PROCESS_FAIL;
    }
    if (bu_process_read_n(p, BU_PROCESS_STDERR, 100, (char *)line) <= 0) {
	fprintf(stderr, "bu_process_test[\"read\"] stdout read failed\n");
	return PROCESS_FAIL;
    }

    if (bu_process_wait_n(p, 0)) {
	fprintf(stderr, "bu_process_test[\"read\"] - wait failed\n");
	return PROCESS_FAIL;
    }

    return PROCESS_PASS;
}

/* tests:   reads with lots of output; equal distribution - BLOCKING READS
 *  bu_process_read() [stdout and stderr]
 * also relies on:
 *  bu_process_create()
 *  bu_process_wait()
 */
int _test_read_flood(const char* cmd) {
    struct bu_process* p;
    const char* run_av[3] = {cmd, "flood", NULL};
    char line[100] = {0};
    int stdout_done = 0, stderr_done = 0;   // NOTE: this would be better checked with 'alive' function

    bu_process_create(&p, (const char**)run_av, BU_PROCESS_DEFAULT);

    while (!stdout_done || !stderr_done) {
	if (!stdout_done && bu_process_read_n(p, BU_PROCESS_STDOUT, 100, (char *)line) <= 0)
	    stdout_done = 1;

	if (!stderr_done && bu_process_read_n(p, BU_PROCESS_STDERR, 100, (char *)line) <= 0)
	    stderr_done = 1;
    }

    if (bu_process_wait_n(p, 0)) {
	fprintf(stderr, "bu_process_test[\"read\"] - wait failed\n");
	return PROCESS_FAIL;
    }

    return PROCESS_PASS;
}

/* tests:   basic process execution and wait
 *  bu_process_create()
 *  bu_process_wait()
 */
int _test_exec_wait(const char* cmd) {
    struct bu_process* p;
    const char* run_av[3] = {cmd, "basic", NULL};

    bu_process_create(&p, (const char**)run_av, BU_PROCESS_DEFAULT);

    // TODO: add tests for timeout
    if (bu_process_wait_n(p, 0)) {
	fprintf(stderr, "bu_process_test[\"exec_wait\"] - wait failed\n");
	return PROCESS_FAIL;
    }

    return PROCESS_PASS;
}

/* tests:   process creation with options
 *  bu_process_create() [bu_process_create_opts]
 * also relies on
 *  bu_process_read()
 *  bu_process_wait()
 */
int _test_create_opts(const char* cmd) {
    struct bu_process* p;
    const char* run_av[3] = {cmd, "output", NULL};
    char line[100] = {0};

    /*** out = stderr ***/
    bu_process_create(&p, (const char**)run_av, BU_PROCESS_OUT_EQ_ERR);

    /* read from stdout, but should get text from stderr*/
    if (bu_process_read_n(p, BU_PROCESS_STDOUT, 100, (char *)line) <= 0) {
	fprintf(stderr, "bu_process_test[\"read\"] stdin read failed\n");
	return PROCESS_FAIL;
    }
    char expected[19] = "Howdy from stderr!";   // intentionally ignore newline chars if any
    if (bu_strncmp(line, expected, 18)) {
        fprintf(stderr,
                "bu_process_test[\"create_opts\"] - OUT_EQ_ERR fail\n  Expected: %s\n  Got: %s\n",
                expected, line);
	return PROCESS_FAIL;
    }

    if (bu_process_wait_n(p, 0)) {
	fprintf(stderr, "bu_process_test[\"exec_wait\"] - wait failed\n");
	return PROCESS_FAIL;
    }

    /*** TODO: (Windows) Hide Window? ***/

    return PROCESS_PASS;
}

/* tests:   current process id, and running subprocess id
 *  bu_process_id()
 *  bu_process_pid()
 * also relies on
 *  bu_process_create()
 *  bu_process_wait()
 */
int _test_ids(const char* cmd) {
    struct bu_process* p = NULL;
    const char* run_av[3] = {cmd, "basic", NULL};

    bu_process_create(&p, (const char**)run_av, BU_PROCESS_DEFAULT);

    int curr_id = bu_pid();
    int process_id = bu_process_pid(p);

    if (curr_id <= 0) {
	fprintf(stderr, "bu_process_test[\"ids\"] bu_pid() should not be 0");
	return PROCESS_FAIL;
    }
    if (process_id <= 0) {
	fprintf(stderr, "bu_process_test[\"ids\"] bu_process_pid() should not be 0");
	return PROCESS_FAIL;
    }
    if (curr_id == process_id) {
	fprintf(stderr, "bu_process_test[\"ids\"] bu_process_pid() should not equal bu_pid()");
	return PROCESS_FAIL;
    }

    if (bu_process_wait_n(p, 0)) {
	fprintf(stderr, "bu_process_test[\"ids\"] - wait failed\n");
	return PROCESS_FAIL;
    }

    return PROCESS_PASS;
}

/* tests:   open/close stdin, stdout, stderr; test pending information on stream using fileno
 *  bu_process_open(),
 *  bu_process_close(),
 *  bu_process_fileno(),
 *  bu_process_pending()
 * also relies on:
 *  bu_process_create(),
 *  bu_process_wait()
 */
int _test_streams(const char* cmd) {
    struct bu_process* p = NULL;
    const char* run_av[3] = {cmd, "echo", NULL};

    bu_process_create(&p, (const char**)run_av, BU_PROCESS_DEFAULT);

    FILE* f_in = bu_process_file_open(p, BU_PROCESS_STDIN);

    // send a test line through stdin
    char line[10] = "echo_test";
    fputs(line, f_in);
    // subprocess is using cin.get() -> need to send newline and flush so it'll move on
    fputs("\n", f_in);	fflush(f_in);

    // subprocess should echo on stdout and stderr
    char out_read[10], err_read[10];
    FILE* f_out = bu_process_file_open(p, BU_PROCESS_STDOUT);
    FILE* f_err = bu_process_file_open(p, BU_PROCESS_STDERR);
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

    bu_process_file_close(p, BU_PROCESS_STDIN);
    bu_process_file_close(p, BU_PROCESS_STDOUT);
    bu_process_file_close(p, BU_PROCESS_STDERR);

    if (bu_process_wait_n(p, 0) == -1) {
	fprintf(stderr, "bu_process_test[\"streams\"] - wait failed\n");
	return PROCESS_FAIL;
    }

    return PROCESS_PASS;
}


/* tests:   forcefully terminate a running process
 *  bu_pid_terminate()
 * also relies on:
 *  bu_process_create()
 *  bu_process_pid()
 *  bu_process_wait()
 */
int _test_abort(const char* cmd) {
    struct bu_process* p = NULL;
    const char* run_av[3] = {cmd, "echo", NULL};	// 'echo' test has inf wait on cin.get()
    bu_process_create(&p, (const char**)run_av, BU_PROCESS_DEFAULT);

    gettime_delay(200); /* give process a moment to start before trying to kill it */

    if (!bu_pid_terminate(bu_process_pid(p))) {
	fprintf(stderr, "bu_process_test[\"terminate\"] - subprocess could not be terminated\n");
	return PROCESS_FAIL;
    }

    int wait_status = bu_process_wait_n(p, 0);
    if (wait_status != ERROR_PROCESS_ABORTED) {
        fprintf(stderr, "bu_process_test[\"terminate\"] - wait should have reported abort code\n");
	return PROCESS_FAIL;
    }

    return PROCESS_PASS;
}


/* tests:   process args storage and retrieval
 *  bu_process_args()
 * also relies on:
 *  bu_process_create()
 *  bu_process_wait()
 */
int _test_args(const char* cmd) {
    struct bu_process* p;
    const char* run_av[3] = {cmd, "basic", NULL};
    int ck_argc;
    const char* ck_cmd;
    const char* const* ck_argv;

    bu_process_create(&p, (const char**)run_av, BU_PROCESS_DEFAULT);

    ck_argc = bu_process_args_n(p, &ck_cmd, &ck_argv);

    if (ck_argc != 2) {
	fprintf(stderr, "bu_process_test[\"args\"] - ck_argc got (%d), expected (%d)\n", ck_argc, 2);
	return PROCESS_FAIL;
    }

    if (bu_strncmp(cmd, ck_cmd, 100)) {
	fprintf(stderr, "bu_process_test[\"args\"] - ck_cmd got (%s), expected (%s)\n", ck_cmd, cmd);
	return PROCESS_FAIL;
    }

    for (int i = 0; i < ck_argc; i++) {
	if (bu_strncmp(run_av[i], ck_argv[i], 100)) {
	    fprintf(stderr, "bu_process_test[\"args\"] - ck_argv idx (%d) got (%s), expected (%s)\n", i, ck_cmd, run_av[i]);
	    return PROCESS_FAIL;
	}
    }

    if (bu_process_wait_n(p, 0)) {
        fprintf(stderr, "bu_process_test[\"args\"] - wait failed\n");
	return PROCESS_FAIL;
    }

    return PROCESS_PASS;
}

/* tests:   process reports alive
 *  bu_process_alive()
 *  bu_pid_alive()
 * also relies on:
 *  bu_process_pid()
 *  bu_process_create()
 *  bu_process_wait()
 */
int _test_alive(const char* cmd) {
    struct bu_process* p;
    const char* run_av[3] = {cmd, "alive", NULL};
    int pid = -1;

    /* test alive status with wait */
    bu_process_create(&p, (const char**)run_av, BU_PROCESS_DEFAULT);
    pid = bu_process_pid(p);

    if (!bu_process_alive(p)) {	// should be alive
	fprintf(stderr, "bu_process_test[\"alive\"] alive check (p) failed\n");
	return PROCESS_FAIL;
    }
    if (!bu_pid_alive(pid)) {	// should be alive
	fprintf(stderr, "bu_process_test[\"alive\"] alive check (pid) failed\n");
	return PROCESS_FAIL;
    }

    if (bu_process_wait_n(p, 0)) {
	fprintf(stderr, "bu_process_test[\"alive\"] - wait failed\n");
	return PROCESS_FAIL;
    }

    if (bu_process_alive(p)) {	// should be dead
	fprintf(stderr, "bu_process_test[\"alive\"] alive check (p) failed after wait\n");
	return PROCESS_FAIL;
    }
    if (bu_pid_alive(pid)) {	// should be dead
	fprintf(stderr, "bu_process_test[\"alive\"] alive check (pid) failed after wait\n");
	return PROCESS_FAIL;
    }


    /* test alive status with termination */
    bu_process_create(&p, (const char**)run_av, BU_PROCESS_DEFAULT);
    pid = bu_process_pid(p);

    if (!bu_process_alive(p)) {	// should be alive
	fprintf(stderr, "bu_process_test[\"alive\"] alive1 check failed\n");
	return PROCESS_FAIL;
    }
    if (!bu_pid_alive(pid)) {	// should be alive
	fprintf(stderr, "bu_process_test[\"alive\"] alive1 check (pid) failed\n");
	return PROCESS_FAIL;
    }

    gettime_delay(200); /* give process a moment to start before trying to kill it */
    if (!bu_pid_terminate(bu_process_pid(p))) {
	fprintf(stderr, "bu_process_test[\"alive\"] - terminate failed\n");
	return PROCESS_FAIL;
    }

    (void)bu_process_wait_n(p, 0);      // reap terminated process

    if (bu_process_alive(p)) {	// should be dead
	fprintf(stderr, "bu_process_test[\"alive\"] alive check (p) failed after terminate\n");
	return PROCESS_FAIL;
    }
    if (bu_pid_alive(pid)) {	// should be dead
	fprintf(stderr, "bu_process_test[\"alive\"] alive1 check (pid) failed after terminate\n");
	return PROCESS_FAIL;
    }

    return PROCESS_PASS;
}

#if BU_PROCESS_ASYNC
/* tests:   reads with lots of output; equal stdout and stderr distribution - ASYNC READS
 *  bu_process_create() ['async' option]
 *  bu_process_read() [stdout and stderr]
 * also relies on:
 *  bu_process_alive()
 *  bu_terminate()
 *  bu_process_pid()
 *  bu_process_wait()
 */
int _test_async_balanced(const char* cmd) {
    struct bu_process* p;
    const char* run_av[3] = {cmd, "flood", NULL};
    int count = 0;
    char line[100] = {0};
    int timed_out = 0;

    // subprocess_option_enable_async = 0x4
    bu_process_create(&p, cmd, 2, (const char**)run_av, 0x4);

    // read for up to 2 seconds
    int64_t start_time = bu_gettime();
    while (bu_process_alive(p)) {
	(void)bu_process_read((char *)line, &count, p, BU_PROCESS_STDOUT, 100);
	(void)bu_process_read((char *)line, &count, p, BU_PROCESS_STDERR, 100);

	if ((bu_gettime() - start_time) > BU_SEC2USEC(2))
	    timed_out = 1;
    }

    if (timed_out) {
	bu_terminate(bu_process_pid(p));
	fprintf(stderr, "bu_process_test[\"async_bal\"] - reading failed\n");
	return PROCESS_FAIL;
    }

    if (bu_process_wait(NULL, p, 0)) {
	fprintf(stderr, "bu_process_test[\"async_bal\"] - wait failed\n");
	return PROCESS_FAIL;
    }

    return PROCESS_PASS;
}

/* tests:   lots of outout; unequal stdout and stderr distribution - ASYNC READS
 *  bu_process_create() ['async' option]
 *  bu_process_read() [stdout and stderr]
 * also relies on:
 *  bu_process_alive()
 *  bu_terminate()
 *  bu_process_pid()
 *  bu_process_wait()
 */
int _test_async_unbalanced(const char* cmd) {
    struct bu_process* p;
    const char* run_av[3] = {cmd, "flood_unbal", NULL};	    // NOTE: writes to stdout 10x stderr
    int count = 0;
    char line[100] = {0};
    int timed_out = 0;

    // subprocess_option_enable_async = 0x4
    bu_process_create(&p, cmd, 2, (const char**)run_av, 0x4);

    // read for up to 2 seconds
    int64_t start_time = bu_gettime();
    while (bu_process_alive(p)) {
	(void)bu_process_read((char *)line, &count, p, BU_PROCESS_STDOUT, 100);
	(void)bu_process_read((char *)line, &count, p, BU_PROCESS_STDERR, 100);

	if ((bu_gettime() - start_time) > BU_SEC2USEC(2))
	    timed_out = 1;
    }

    if (timed_out) {
	bu_terminate(bu_process_pid(p));
	fprintf(stderr, "bu_process_test[\"async_unbal\"] - reading failed\n");
	return PROCESS_FAIL;
    }

    if (bu_process_wait(NULL, p, 0)) {
	fprintf(stderr, "bu_process_test[\"async_unbal\"] - wait failed\n");
	return PROCESS_FAIL;
    }

    return PROCESS_PASS;
}
#endif

typedef struct {
    const char* name;
    int (*func)(const char*);
} ProcessTest;

ProcessTest tests[] = {
    {"exec_wait", _test_exec_wait},
    {"create_opts", _test_create_opts},
    {"read", _test_read},
    {"read_flood", _test_read_flood},
    {"ids", _test_ids},
    {"streams", _test_streams},
    {"abort", _test_abort},
    {"args", _test_args},
    {"alive", _test_alive},
#if BU_PROCESS_ASYNC
    {"async_bal", _test_async_balanced},
    {"async_unbal", _test_async_unbalanced},
#endif
};

int run_test(char* av[]) {
    int failed = 0;
    int run = 0;

    int test_all = (BU_STR_EQUAL(av[2], "all")) ? 1 : 0;

    for (size_t i = 0; i < sizeof(tests) / sizeof(tests[0]); i++) {
	if (test_all || BU_STR_EQUAL(av[2], tests[i].name)) {
	    // echo what we're about to run
	    fprintf(stdout, "Running [%s] with \"%s\"\n", tests[i].name, av[1]);

	    failed += tests[i].func(av[1]);
	    run = 1;
	}
    }

    if (!run) {
	// invalid test was supplied, print available
	fprintf(stderr, "ERROR: testname '%s' unknown\nAvailable tests:\n", av[2]);
	for (size_t i = 0; i < sizeof(tests) / sizeof(tests[0]); i++)
	    fprintf(stderr, "    %s\n", tests[i].name);
	failed = -1;
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
