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

#include "bu.h"


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
    const char *output = NULL;
    FILE *fp = NULL;
    int result;

    if (argc != 2) {
	fprintf(stderr, "Usage: %s {file}\n", argv[0]);
	return 1;
    }

    if (argc > 1)
	output = argv[1];
    bu_file_delete(output);
    if (bu_file_exists(output, NULL))
	bu_exit(1, "ERROR: backtrace output [%s] already exists and couldn't be deleted\n", output);
    fp = fopen(output, "w");
    if (!fp)
	bu_exit(2, "ERROR: Unable to open backtrace output [%s]\n", output);

    bu_debug |= BU_DEBUG_BACKTRACE;

    result = go_deep(3, fp);
    fclose(fp);

    /* FIXME: need to make sure backtrace has some content */
    if (result != 1 || !bu_file_exists(output, NULL)) {
	printf("bu_backtrace: [FAILED]\n");
	return 1;
    }

    printf("bu_backtrace: [PASSED]\n");
    bu_file_delete(output);
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
