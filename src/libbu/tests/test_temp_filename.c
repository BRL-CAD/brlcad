/*            T E S T _ T E M P _ F I L E N A M E . C
 * BRL-CAD
 *
 * Copyright (c) 2014-2026 United States Government as represented by
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

#include "common.h"


static void
temp_filename_thread(int cpu, void* ptr)
{
    // unroll ptr into names arr
    char** names = (char**)ptr;

    // generate name
    const char* buf = bu_temp_file_name(NULL, 0);

    // add to arr
    size_t len = strlen(buf);
    names[cpu - 1] = (char*)bu_malloc(len * sizeof(char), "alloc name");
    bu_strlcpy(names[cpu - 1], buf, len);
}


int
main(int argc, char *argv[])
{
    // Normally this file is part of bu_test, so only set this if it
    // looks like the program name is still unset.
    if (bu_getprogname()[0] == '\0')
	bu_setprogname(argv[0]);

    /* group tests so failures are easier to pinpoint */
    char* curr = NULL;
    for (int test = 1; test < argc; test++) {
	curr = argv[test];

	if (!bu_strcmp(curr, "single")) {
	    // test with different configurations - all thread and
	    // proc ID's should be the same

	    // no prefix - buffer + no-buffer should match
	    char plain[25] = {0};
	    bu_temp_file_name(plain, 25);
	    const char* non_alloc = bu_temp_file_name(NULL, 0);
	    // validate
	    if (bu_strncmp(plain, non_alloc, strlen(plain))) /* should match */
		bu_exit(EXIT_FAILURE, "temp_filename failure: expected match, got [%s] | [%s]\n", plain, non_alloc);

	    // prefix - buffer + no-buffer should match
	    char prefix[25] = "prefix";
	    bu_temp_file_name(prefix, 25);
	    const char* prefix_non_alloc = bu_temp_file_name(prefix, 0);
	    // validate
	    if (bu_strncmp(prefix, prefix_non_alloc, strlen(prefix))) /* should match */
		bu_exit(EXIT_FAILURE, "temp_filename failure: expected match, got [%s] | [%s]\n", prefix, prefix_non_alloc);

	    // validate prefix != generic
	    if (!bu_strncmp(plain, prefix, strlen(plain))) /* should not match */
		bu_exit(EXIT_FAILURE, "temp_filename failure: expected non-matching names\n");

	    // undersized buffer - should create 4 chars + null termination
	    char under_size[5] = {0};
	    bu_temp_file_name(under_size, 5);
	    // validate
	    if (strlen(under_size) != 4)    /* buffer size -1 */
		bu_exit(EXIT_FAILURE, "temp_filename failure: expected size [%d], got [%ld]]\n", 4, strlen(under_size));

	} else if (!bu_strcmp(curr, "parallel")) {
	    // test parallel / threaded -
	    uint8_t threads = bu_avail_cpus();

	    /* create names array. passed to each thread to ensure
	     * each name is created uniquely */
	    char** names = (char**)bu_malloc(threads * sizeof(char*), "alloc names arr");
	    /* individual names are allocated in each thread */
	    /* for (int i = 0; i < threads; i++)
	     * names[i] = (char*)bu_malloc(25 * sizeof(char), "zero alloc name");
	     */

	    bu_parallel(temp_filename_thread, threads, &names);

	    // check names are all unique, free memory as we go
	    int failed = 0;
	    for (int i = 0; i < threads; i++) {
		for (int j = i + 1; j < threads; j++) {
		    /* keep going even if we've failed to make sure
		     * memory is released.
		     */
		    if (failed || !bu_strncmp(names[i], names[j], 25))
			failed = 1;
		}
		bu_free(names[i], "free name");
	    }

	    /* cleanup */
	    bu_free(names, "free names");

	    if (failed)
		bu_exit(EXIT_FAILURE, "temp_filename failure: parallel did not create unique names\n");
	} else {
	    bu_exit(EXIT_FAILURE, "temp_filename failure: unknown test mode [%s]\n", curr);
	}
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
 * ex: shiftwidth=4 tabstop=8 cino=N-s
 */
