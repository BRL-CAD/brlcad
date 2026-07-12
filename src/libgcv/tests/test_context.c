/*                    T E S T _ C O N T E X T . C
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
/** @file test_context.c
 *
 * Test LIBGCV context lifecycle handling.
 */

#include "common.h"

#include <stdio.h>

#include "bu/app.h"
#include "bu/ptbl.h"
#include "gcv.h"


int
main(int argc, const char **argv)
{
    int failures = 0;

    (void)argc;

    bu_setprogname(argv[0]);

    for (int i = 0; i < 3; i++) {
	struct gcv_context context;
	const struct bu_ptbl *filters = NULL;

	gcv_context_init(&context);
	if (!context.dbip) {
	    fprintf(stderr, "gcv_context_init did not initialize dbip\n");
	    failures++;
	}
	if (!context.i) {
	    fprintf(stderr, "gcv_context_init did not initialize internals\n");
	    failures++;
	}

	filters = gcv_list_filters(&context);
	if (!filters || BU_PTBL_LEN(filters) < 2) {
	    fprintf(stderr, "gcv_list_filters did not return built-in filters\n");
	    failures++;
	}

	gcv_context_destroy(&context);
	if (context.dbip) {
	    fprintf(stderr, "gcv_context_destroy did not clear dbip\n");
	    failures++;
	}
	if (context.i) {
	    fprintf(stderr, "gcv_context_destroy did not clear internals\n");
	    failures++;
	}
    }

    return failures ? 1 : 0;
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
