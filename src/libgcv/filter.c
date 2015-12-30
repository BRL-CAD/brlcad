/*                        F I L T E R . C
 * BRL-CAD
 *
 * Copyright (c) 2015 United States Government as represented by
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
/** @file filter.c
 *
 * Brief description
 *
 */


#include "common.h"

#include "gcv/api.h"

#include "bu/debug.h"

#include <string.h>


HIDDEN void
_gcv_filter_register(struct bu_ptbl *table, const struct gcv_filter *filter)
{
    if (!table || !filter)
	bu_bomb("missing argument");

    if (!filter->name)
	bu_bomb("null filter name");

    switch (filter->filter_type) {
	case GCV_FILTER_FILTER:
	    if (filter->mime_type != MIME_MODEL_UNKNOWN)
		bu_bomb("invalid mime_type");

	    break;

	case GCV_FILTER_READ:
	case GCV_FILTER_WRITE:
	    if (filter->mime_type == MIME_MODEL_AUTO
		|| filter->mime_type == MIME_MODEL_UNKNOWN)
		bu_bomb("invalid mime_type");

	    break;

	default:
	    bu_bomb("unknown filter type");
    }

    if (!filter->create_opts_fn != !filter->free_opts_fn)
	bu_bomb("must have none or both of create_opts_fn and free_opts_fn");

    if (!filter->filter_fn)
	bu_bomb("null filter_fn");

    if (bu_ptbl_ins_unique(table, (long *)filter) != -1)
	bu_bomb("filter already registered");
}


HIDDEN void
_gcv_filter_options_create(const struct gcv_filter *filter,
			   struct bu_opt_desc **options_desc, void **options_data)
{
    if (!filter || !options_desc || !options_data)
	bu_bomb("missing arguments");

    if (filter->create_opts_fn) {
	*options_desc = NULL;
	*options_data = NULL;

	filter->create_opts_fn(options_desc, options_data);

	if (!*options_desc || !*options_data)
	    bu_bomb("filter->create_opts_fn() set null result");
    } else {
	*options_desc = (struct bu_opt_desc *)bu_malloc(sizeof(struct bu_opt_desc),
			"options_desc");
	BU_OPT_NULL(**options_desc);
	*options_data = NULL;
    }
}


HIDDEN void
_gcv_filter_options_free(const struct gcv_filter *filter, void *options_data)
{
    if (!filter || (!filter->create_opts_fn != !options_data))
	bu_bomb("missing arguments");

    if (filter->create_opts_fn)
	filter->free_opts_fn(options_data);
}


HIDDEN int
_gcv_filter_options_process(const struct gcv_filter *filter, size_t argc,
			    const char * const *argv, void **options_data)
{
    int ret_argc;
    struct bu_opt_desc *options_desc;
    struct bu_vls messages;

    _gcv_filter_options_create(filter, &options_desc, options_data);

    BU_VLS_INIT(&messages);
    ret_argc = argc ? bu_opt_parse(&messages, argc, (const char **)argv,
				   options_desc) : argc;
    bu_log("%s", bu_vls_addr(&messages));
    bu_vls_free(&messages);

    bu_free(options_desc, "options_desc");

    if (ret_argc) {
	if (ret_argc == -1)
	    bu_log("fatal error in bu_opt_parse()\n");
	else {
	    int i;

	    bu_log("unknown arguments: ");

	    for (i = 0; i < ret_argc - 1; ++i)
		bu_log("%s, ", argv[i]);

	    bu_log("%s\n", argv[i]);
	}

	_gcv_filter_options_free(filter, *options_data);
	return 0;
    }

    return 1;
}


HIDDEN void
_gcv_opts_check(const struct gcv_opts *gcv_options)
{
    if (!gcv_options)
	bu_bomb("null gcv_options");

    BN_CK_TOL(&gcv_options->calculational_tolerance);
    RT_CK_TESS_TOL(&gcv_options->tessellation_tolerance);

    if (gcv_options->debug_mode > 1)
	bu_bomb("invalid gcv_opts.debug_mode");

    if (gcv_options->scale_factor <= 0.0)
	bu_bomb("invalid gcv_opts.scale_factor");

    if (!gcv_options->default_name)
	bu_bomb("invalid gcv_opts.default_name");

    if (!gcv_options->num_objects != !gcv_options->object_names)
	bu_bomb("must have none or both of num_objects and object_names");
}


HIDDEN void
_gcv_context_check(const struct gcv_context *context)
{
    if (!context)
	bu_bomb("null gcv_context");

    RT_CK_DBI(context->dbip);
    BU_CK_AVS(&context->messages);
}


void
gcv_context_init(struct gcv_context *context)
{
    context->dbip = db_create_inmem();
    BU_AVS_INIT(&context->messages);
}


void
gcv_context_destroy(struct gcv_context *context)
{
    _gcv_context_check(context);

    db_close(context->dbip);
    bu_avs_free(&context->messages);
}


const struct bu_ptbl *
gcv_list_filters(void)
{
    static struct bu_ptbl filter_table = BU_PTBL_INIT_ZERO;

    if (!BU_PTBL_LEN(&filter_table)) {
#define REGISTER_FILTER(name) \
	do { \
	    extern const struct gcv_filter (name); \
	    _gcv_filter_register(&filter_table, &(name)); \
	} while (0)

	REGISTER_FILTER(gcv_conv_brlcad_read);
	REGISTER_FILTER(gcv_conv_brlcad_write);
	REGISTER_FILTER(gcv_conv_fastgen4_read);
	REGISTER_FILTER(gcv_conv_fastgen4_write);
	REGISTER_FILTER(gcv_conv_obj_read);
	REGISTER_FILTER(gcv_conv_obj_write);
	REGISTER_FILTER(gcv_conv_stl_read);
	REGISTER_FILTER(gcv_conv_stl_write);
	REGISTER_FILTER(gcv_conv_vrml_write);

#undef REGISTER_FILTER
    }

    return &filter_table;
}


void
gcv_opts_default(struct gcv_opts *gcv_options)
{
    const struct rt_tess_tol default_tessellation_tolerance =
    {RT_TESS_TOL_MAGIC, 0.0, 1.0e-2, 0.0};

    memset(gcv_options, 0, sizeof(*gcv_options));

    gcv_options->scale_factor = 1.0;
    gcv_options->default_name = "unnamed";

    BN_TOL_INIT(&gcv_options->calculational_tolerance);
    rt_tol_default(&gcv_options->calculational_tolerance);
    gcv_options->tessellation_tolerance = default_tessellation_tolerance;
}


int
gcv_execute(struct gcv_context *context, const struct gcv_filter *filter,
	    const struct gcv_opts *gcv_options, size_t argc, const char * const *argv,
	    const char *target)
{
    const int bu_debug_orig = bu_debug;
    const uint32_t rt_debug_orig = RTG.debug;
    const uint32_t nmg_debug_orig = RTG.NMG_debug;
    int dbi_read_only_orig;

    int result;
    struct gcv_opts default_opts;
    void *options_data;

    _gcv_context_check(context);

    if (!filter)
	bu_bomb("missing filter");

    switch (filter->filter_type) {
	case GCV_FILTER_FILTER:
	    if (target)
		bu_bomb("target specified for non-conversion filter");

	    break;

	case GCV_FILTER_READ:
	case GCV_FILTER_WRITE:
	    if (!target)
		bu_bomb("no target specified for conversion filter");

	    break;

	default:
	    bu_bomb("unknown filter type");
    }

    if (!gcv_options) {
	gcv_opts_default(&default_opts);
	gcv_options = &default_opts;
    }

    _gcv_opts_check(gcv_options);

    if (!_gcv_filter_options_process(filter, argc, argv, &options_data))
	return 0;

    bu_debug |= gcv_options->bu_debug_flag;
    RTG.debug |= gcv_options->rt_debug_flag;
    RTG.NMG_debug |= gcv_options->nmg_debug_flag;

    if (filter->filter_type == GCV_FILTER_WRITE) {
	dbi_read_only_orig = context->dbip->dbi_read_only;
	context->dbip->dbi_read_only = 1;
    }

    if (!gcv_options->num_objects && filter->filter_type != GCV_FILTER_READ) {
	size_t num_objects;
	struct directory **toplevel_dirs;

	db_update_nref(context->dbip, &rt_uniresource);
	num_objects = db_ls(context->dbip, DB_LS_TOPS, NULL, &toplevel_dirs);

	if (num_objects) {
	    struct gcv_opts temp_options = *gcv_options;
	    char **object_names = db_dpv_to_argv(toplevel_dirs);
	    bu_free(toplevel_dirs, "toplevel_dirs");

	    temp_options.num_objects = num_objects;
	    temp_options.object_names = (const char * const *)object_names;
	    result = filter->filter_fn(context, &temp_options, options_data, target);
	    bu_free(object_names, "object_names");
	} else
	    result = filter->filter_fn(context, gcv_options, options_data, target);
    } else
	result = filter->filter_fn(context, gcv_options, options_data, target);

    if (filter->filter_type == GCV_FILTER_WRITE)
	context->dbip->dbi_read_only = dbi_read_only_orig;

    bu_debug = bu_debug_orig;
    RTG.debug = rt_debug_orig;
    RTG.NMG_debug = nmg_debug_orig;

    _gcv_filter_options_free(filter, options_data);

    return result;
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
