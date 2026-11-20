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

#include "bu/app.h"
#include "bu/ptbl.h"
#include "bu/log.h"
#include "bu/mime.h"
#include "gcv.h"


int
main(int argc, const char **argv)
{
    int failures = 0;
    const int context_iterations = 3;

    (void)argc;

    bu_setprogname(argv[0]);

    for (int i = 0; i < context_iterations; i++) {
	struct gcv_context context;
	const struct bu_ptbl *filters = NULL;

	gcv_context_init(&context);
	if (!context.dbip) {
	    bu_log("gcv_context_init did not initialize dbip\n");
	    failures++;
	}
	if (!context.i) {
	    bu_log("gcv_context_init did not initialize internals\n");
	    failures++;
	}

	filters = gcv_list_filters(&context);
	if (!filters || BU_PTBL_LEN(filters) < 2) {
	    bu_log("gcv_list_filters did not return built-in filters\n");
	    failures++;
	}

	{
	    struct gcv_context other;
	    const struct bu_ptbl *other_filters;
	    struct bu_mime_definition definition = {0};
	    struct bu_vls canonical = BU_VLS_INIT_ZERO;
	    definition.abi_version = BU_MIME_ABI_VERSION;
	    definition.struct_size = sizeof(definition);
	    definition.name = "model/vnd.example.context";
	    if (!bu_mime_register(gcv_mime_registry(&context), &definition))
		failures++;
	    gcv_context_init(&other);
	    other_filters = gcv_list_filters(&other);
	    if (other_filters == filters || gcv_mime_registry(&context) == gcv_mime_registry(&other)) {
		bu_log("GCV contexts unexpectedly share mutable registries\n");
		failures++;
	    }
	    if (bu_mime_canonical(gcv_mime_registry(&other), definition.name, &canonical))
		failures++;
	    bu_vls_free(&canonical);
	    gcv_context_destroy(&other);
	    /* Discovery in another context must not invalidate our filters. */
	    for (size_t j = 0; j < BU_PTBL_LEN(filters); ++j) {
		const struct gcv_filter *filter = (const struct gcv_filter *)BU_PTBL_GET(filters, j);
		if (!filter->name || !filter->name[0] || !filter->filter_fn)
		    failures++;
	    }
	}

	gcv_context_destroy(&context);
	if (context.dbip) {
	    bu_log("gcv_context_destroy did not clear dbip\n");
	    failures++;
	}
	if (context.i) {
	    bu_log("gcv_context_destroy did not clear internals\n");
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
