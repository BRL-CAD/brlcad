/*                    T E S T _ P L Y _ W R I T E . C
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

#include "common.h"

#include "bu/app.h"
#include "bu/log.h"
#include "bu/malloc.h"
#include "bu/opt.h"
#include "bu/str.h"
#include "gcv.h"

static const char *
option_string(const struct bu_opt_desc *options, const char *shortopt)
{
    for (const struct bu_opt_desc *option = options; option->shortopt; ++option)
	if (BU_STR_EQUAL(option->shortopt, shortopt))
	    return *(const char * const *)option->set_var;
    return NULL;
}

static int
check_options(const struct gcv_filter *writer, int long_options)
{
    struct bu_opt_desc *options = NULL;
    void *data = NULL;
    struct bu_vls messages = BU_VLS_INIT_ZERO;
    const char *arguments[] = {"-o", "mesh.ply", "-x", "1", "-X", "2"};
    const char *long_arguments[] = {"--output_file", "mesh.ply", "--rt_debug", "1", "--nmg_debug", "2"};
    writer->create_opts_fn(&options, &data);
    int valid = options && data;
    if (valid) {
	valid = bu_opt_parse(&messages, sizeof(arguments) / sizeof(arguments[0]),
	    long_options ? long_arguments : arguments, options) == 0;
	const char *output = option_string(options, "o");
	const char *rt_flags = option_string(options, "x");
	const char *nmg_flags = option_string(options, "X");
	valid = valid && output && BU_STR_EQUAL(output, "mesh.ply") &&
	    rt_flags && BU_STR_EQUAL(rt_flags, "1") && nmg_flags && BU_STR_EQUAL(nmg_flags, "2");
    }
    if (!valid)
	bu_log("PLY debug options changed the output path or lost their values: %s\n", bu_vls_cstr(&messages));
    bu_vls_free(&messages);
    bu_free(options, "PLY option descriptions");
    writer->free_opts_fn(data);
    return valid;
}

int
main(int argc, const char **argv)
{
    struct gcv_context context;
    (void)argc;
    bu_setprogname(argv[0]);
    gcv_context_init(&context);
    const struct gcv_filter *writer = find_filter(GCV_FILTER_WRITE, BU_MIME_MODEL_PLY, NULL, &context);
    int valid = writer && writer->create_opts_fn && writer->free_opts_fn;
    if (valid)
	valid = check_options(writer, 0) && check_options(writer, 1);
    gcv_context_destroy(&context);
    return valid ? 0 : 1;
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
