/*                   B O O L E A N I Z E . C
 * BRL-CAD
 *
 * Copyright (c) 2018 United States Government as represented by
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

#include "common.h"

#ifdef HAVE_SYS_STAT_H
#  include <sys/stat.h>
#endif

#include "bu.h"


/* should be at least a few lines with some words */
#define BACKTRACE_MINIMUM 64


static int
go_deeper(int depth, FILE *fp)
{
    if (depth > 1)
	return go_deeper(--depth, fp);

    return bu_backtrace(fp);
}


static int
go_deep(int depth, FILE *fp)
{
    return go_deeper(depth, fp);
}


int
main(int argc, char *argv[])
{
    char *buffer = NULL;
    const char *output = NULL;
    FILE *fp = NULL;
    int result;
    size_t size = 0;

    if (argc > 2) {
	fprintf(stderr, "Usage: %s [file]\n", argv[0]);
	return 1;
    }

    if (argc > 1)
	output = argv[1];
    bu_file_delete(output);
    if (bu_file_exists(output, NULL))
	bu_exit(1, "ERROR: backtrace output [%s] already exists and couldn't be deleted\n", output);
    if (!output) {
	buffer = (char *)bu_calloc(MAXPATHLEN, MAXPATHLEN, "memory buffer");
	fp = fmemopen(buffer, MAXPATHLEN * MAXPATHLEN, "w");
	output = "IN_MEMORY_BUFFER";
	fprintf(fp, "this is a test\n");
    } else {
	fp = fopen(output, "w");
    }

    if (!fp)
	bu_exit(2, "ERROR: Unable to open backtrace output [%s]\n", output);

    bu_debug |= BU_DEBUG_BACKTRACE;

    result = go_deep(3, fp);
    fclose(fp);

    /* display the backtrace */
    printf("BEGIN Backtrace {\n");
    if (buffer) {
	printf("%s\n", buffer);
	size = strlen(buffer);
    } else {
	char fgetsbuf[MAXPATHLEN] = {0};
	fp = fopen(output, "r");
	while(bu_fgets(fgetsbuf, MAXPATHLEN, fp)) {
	    printf("%s", fgetsbuf);
	    size += strlen(fgetsbuf);
	}
	fclose(fp);
    }
    printf("} END Backtrace\n");

    /* check the backtrace */
    if (result != 1) {
	printf("bu_backtrace: [FAILED] returned error [%d]\n", result);
	return 1;
    }
    if (!buffer) {
	if (!bu_file_exists(output, NULL)) {
	    printf("bu_backtrace: [FAILED] expecting file\n");
	    return 2;
	}
    }

    if (size < (size_t)BACKTRACE_MINIMUM) {
	printf("bu_backtrace: [FAILED] short trace (%zu < %zu bytes)\n", size, (size_t)BACKTRACE_MINIMUM);
	return 4;
    }
    /* TODO: check trace for main+go_deep+go_deeper */

    bu_free(buffer, "memory buffer");
    printf("bu_backtrace: [PASSED]\n");

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
