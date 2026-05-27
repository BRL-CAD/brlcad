/*             T E S T _ S U B P R O C E S S . C P P
 * BRL-CAD
 *
 * Copyright (c) 2013-2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following
 * disclaimer in the documentation and/or other materials provided
 * with the distribution.
 *
 * 3. The name of the author may not be used to endorse or promote
 * products derived from this software without specific prior written
 * permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
/** @file test_subprocess.cpp
 *
 * Program run by the process unit tests.
 *
 */

#include "common.h"

#include <chrono>
#include <thread>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <iostream>
#include <time.h>

#include "bu.h"


const char* lorem_25 = "sagittis id consectetur purus ut faucibus pulvinar elementum integer enim neque volutpat ac tincidunt vitae semper quis lectus nulla at volutpat diam ut venenatis tellus";


int
main(int argc, const char *argv[])
{
    if (argc > 2) {
	fprintf(stderr, "Usage: %s\n", argv[0]);
	return 1;
    }

    bu_setprogname(argv[0]);

    if (BU_STR_EQUAL(argv[1], "basic")) {
	// just return
	return 0;
    }

    if (BU_STR_EQUAL(argv[1], "output")) {
	// single write to stdout and stderr
	fprintf(stdout, "Howdy from stdout!\n");
	fprintf(stderr, "Howdy from stderr!\n");
	return 0;
    }

    if (BU_STR_EQUAL(argv[1], "echo")) {
	// echo incoming line on stdout and stderr
	char line[25];
	std::cin.get(line, 25);

	fprintf(stdout, "%s", line);
	fprintf(stderr, "%s", line);
	return 0;
    }

    if (BU_STR_EQUAL(argv[1], "flood")) {
	// 1000 writes to stdout and stderr
	const int STDOUT_WRITE_CNT = 1000;

	for (int i = 0; i < STDOUT_WRITE_CNT; i++) {
	    fprintf(stdout, "out: %s[%d]\n", lorem_25, i);
	    fprintf(stderr, "err: %s[%d]\n", lorem_25, i);
	}

	return 0;
    }

    if (BU_STR_EQUAL(argv[1], "flood_unbal")) {
	// 1000 writes to stdout, 100 writes to stderr
	const int STDOUT_WRITE_CNT = 1000;

	for (int i = 0; i < STDOUT_WRITE_CNT; i++) {
	    fprintf(stdout, "out: %s[%d]\n", lorem_25, i);

	    if ((i % 10) == 0)
		fprintf(stderr, "err: %s[%d]\n", lorem_25, i);
	}

	return 0;
    }

    if (BU_STR_EQUAL(argv[1], "alive")) {
	// delay 100ms to give time for an alive check
	// cross-plat sleep function: https://stackoverflow.com/a/11276503
	std::this_thread::sleep_for(std::chrono::milliseconds(100));

	return 0;
    }

    if (BU_STR_EQUAL(argv[1], "timeout")) {
	// long, but reasonable sleep to allow for time checks
	// cross-plat sleep function: https://stackoverflow.com/a/11276503
	std::this_thread::sleep_for(std::chrono::milliseconds(10000));

	return 0;
    }

    return 1;
}


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8 cino=N-s
