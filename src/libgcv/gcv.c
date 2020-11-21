/*                           G C V . C
 * BRL-CAD
 *
 * Copyright (c) 2015-2020 United States Government as represented by
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
/** @file gcv.c
 *
 * Brief description
 *
 */


#include "common.h"
#include <string.h>

#include "vmath.h"
#include "bu/app.h"
#include "bu/debug.h"
#include "bu/dylib.h"
#include "bu/file.h"
#include "bu/log.h"
#include "bu/malloc.h"
#include "bu/mime.h"
#include "bu/str.h"
#include "rt/db4.h"
#include "rt/db5.h"
#include "rt/db_instance.h"
#include "rt/db_io.h"
#include "rt/debug.h"
#include "rt/wdb.h"
#include "rt/search.h"
#include "rt/global.h"
#include "gcv/api.h"


HIDDEN int
_gcv_brlcad_read(struct gcv_context *context,
		 const struct gcv_opts *UNUSED(gcv_options), const void *UNUSED(options_data),
		 const char *source_path)
{
    struct db_i * const in_dbip = db_open(source_path, DB_OPEN_READONLY);

    if (!in_dbip) {
	bu_log("db_open() failed for '%s'\n", source_path);
	return 0;
    }

    if (db_dirbuild(in_dbip)) {
	bu_log("db_dirbuild() failed for '%s'\n", source_path);
	db_close(in_dbip);
	return 0;
    }

    if (db_version(in_dbip) > 4) {
	if (db_dump(context->dbip->dbi_wdbp, in_dbip)) {
	    bu_log("db_dump() failed (from '%s' to context->dbip)\n", source_path);
	    db_close(in_dbip);
	    return 0;
	}

	db_close(in_dbip);
    } else {
	// For v4 .g files, use the original rather than an inmem (which is v5)
	db_close(context->dbip);
	context->dbip = in_dbip;
    }
    return 1;
}


HIDDEN int
_gcv_brlcad_write(struct gcv_context *context,
		  const struct gcv_opts *UNUSED(gcv_options), const void *UNUSED(options_data),
		  const char *dest_path)
{
    struct rt_wdb * const out_wdbp = wdb_fopen(dest_path);

    if (!out_wdbp) {
	bu_log("wdb_fopen() failed for '%s'\n", dest_path);
	return 0;
    }

    if (db_dump(out_wdbp, context->dbip)) {
	bu_log("db_dump() failed (from context->dbip to '%s')\n", dest_path);
	wdb_close(out_wdbp);
	return 0;
    }

    wdb_close(out_wdbp);
    return 1;
}

int
_gcv_brlcad_can_read(const char *data)
{
    union record record; /* GED database record */
    FILE *ifp = fopen(data, "rb");
    if (fread((char *)&record, sizeof record, 1, ifp) != 1) return 0;
    fclose(ifp);
    if (db5_header_is_valid((unsigned char *)&record)) return 1;
    return 0;
}

int
_gcv_brlcad_can_write(const char *data)
{
    int out_type = bu_file_mime_int(data);
    if (out_type == (int)BU_MIME_MODEL_VND_BRLCAD_PLUS_BINARY) return 1;
    return 0;
}

/* TODO - implement a BRL-CAD "valid file" test function...) */
static const struct gcv_filter _gcv_filter_brlcad_read =
{"BRL-CAD Reader", GCV_FILTER_READ, (int)BU_MIME_MODEL_VND_BRLCAD_PLUS_BINARY, BU_MIME_MODEL, _gcv_brlcad_can_read, NULL, NULL, _gcv_brlcad_read};


static const struct gcv_filter _gcv_filter_brlcad_write =
{"BRL-CAD Writer", GCV_FILTER_WRITE, (int)BU_MIME_MODEL_VND_BRLCAD_PLUS_BINARY, BU_MIME_MODEL, _gcv_brlcad_can_write, NULL, NULL, _gcv_brlcad_write};


HIDDEN void
_gcv_filter_register(struct bu_ptbl *filter_table,
		     const struct gcv_filter *filter)
{
    if (!filter_table || !filter)
	bu_bomb("missing argument");

    if (!filter->name)
	bu_bomb("null filter name");

    switch (filter->filter_type) {
	case GCV_FILTER_FILTER:
	    break;

	case GCV_FILTER_READ:
	case GCV_FILTER_WRITE:
	    /* One mime_type isn't going to cut it for things
	     * like GDAL or the asset import library... need to
	     * rethink this */
#if 0
	    if (filter->mime_type == BU_MIME_MODEL_AUTO
		|| filter->mime_type == BU_MIME_MODEL_UNKNOWN)
		bu_bomb("invalid mime_type");
#endif
	    break;

	default:
	    bu_bomb("unknown filter type");
    }

    if (!filter->create_opts_fn != !filter->free_opts_fn)
	bu_bomb("must have none or both of create_opts_fn and free_opts_fn");

    if (!filter->filter_fn)
	bu_bomb("null filter_fn");

    {
	const struct gcv_filter * const *entry;

	for (BU_PTBL_FOR(entry, (const struct gcv_filter * const *), filter_table))
	    if (!bu_strcmp((*entry)->name, filter->name))
		bu_bomb("duplicate filter name");
    }

    bu_ptbl_ins(filter_table, (long *)filter);
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
    const char **temp;
    int i;

    if (argc)
	temp = (const char **)bu_calloc(argc, sizeof(const char *), "temp");
    else
	temp = NULL;

    for (i = 0; (size_t)i < argc; ++i)
	temp[i] = argv[i];

    _gcv_filter_options_create(filter, &options_desc, options_data);
    BU_VLS_INIT(&messages);
    ret_argc = argc ? bu_opt_parse(&messages, argc, temp, options_desc) : 0;
    bu_free(options_desc, "options_desc");
    bu_log("%s", bu_vls_addr(&messages));
    bu_vls_free(&messages);

    if (ret_argc) {
	if (ret_argc == -1)
	    bu_log("fatal error in bu_opt_parse()\n");
	else {
	    bu_log("unknown arguments: ");

	    for (i = 0; i < ret_argc - 1; ++i)
		bu_log("%s, ", temp[i]);

	    bu_log("%s\n", temp[i]);
	}

	if (temp)
	    bu_free((void *)temp, "temp");

	_gcv_filter_options_free(filter, *options_data);
	return 0;
    }

    if (temp)
	bu_free((void *)temp, "temp");

    return 1;
}


HIDDEN void
_gcv_opts_check(const struct gcv_opts *gcv_options)
{
    if (!gcv_options)
	bu_bomb("null gcv_options");

    BN_CK_TOL(&gcv_options->calculational_tolerance);
    BG_CK_TESS_TOL(&gcv_options->tessellation_tolerance);

    if (gcv_options->debug_mode != 0 && gcv_options->debug_mode != 1)
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

HIDDEN void
_gcv_plugins_load(struct bu_ptbl *filter_table, const char *path)
{
    void *info_val;
    const struct gcv_plugin *(*plugin_info)();
    const struct gcv_plugin *plugin;
    const struct gcv_filter * const *current;
    void *dl_handle;

    if (!(dl_handle = bu_dlopen(path, BU_RTLD_NOW))) {
	const char * const error_msg = bu_dlerror();

	if (error_msg)
	    bu_log("%s\n", error_msg);

	bu_log("Unable to dynamically load '%s' (skipping)\n", path);
	return;
    }

    info_val = bu_dlsym(dl_handle, "gcv_plugin_info");
    plugin_info = (const struct gcv_plugin *(*)())(intptr_t)info_val;

    if (!plugin_info) {
	const char * const error_msg = bu_dlerror();

	if (error_msg)
	    bu_log("%s\n", error_msg);

	bu_log("Unable to load symbols from '%s' (skipping)\n", path);
	bu_log("Could not find 'gcv_plugin_info' symbol in plugin\n");
	return;
    }

    plugin = plugin_info();

    if (!plugin || !plugin->filters) {
	bu_log("Invalid plugin encountered from '%s' (skipping)\n", path);
	return;
    }

    for (current = plugin->filters; *current; ++current)
	_gcv_filter_register(filter_table, *current);
}


HIDDEN const char *
_gcv_plugins_get_path(void)
{
    const char *pdir = bu_dir(NULL, 0, BU_DIR_LIBEXEC, LIBGCV_PLUGINS_DIRECTORY, NULL);
    const char *result = bu_strdup(pdir);
    return result;
}


HIDDEN void
_gcv_plugins_load_all(struct bu_ptbl *filter_table)
{
    const char * const plugins_path = _gcv_plugins_get_path();

    if (!plugins_path) {
	bu_log("could not find LibGCV plugins directory\n");
	return;
    }

    {
	char **filenames;
	size_t i;
	struct bu_vls buffer = BU_VLS_INIT_ZERO;
	size_t num_filenames = 0;
	struct bu_vls pattern = BU_VLS_INIT_ZERO;

	/* LIBGCV_PLUGINS_SUFFIX is filename extension on libraries, defined via cppflag */
	bu_vls_sprintf(&pattern, "*%s", LIBGCV_PLUGIN_SUFFIX);
	num_filenames = bu_file_list(plugins_path, bu_vls_addr(&pattern) , &filenames);

	for (i = 0; i < num_filenames; ++i)
	    if (!bu_file_directory(filenames[i])) {
		bu_vls_sprintf(&buffer, "%s%c%s", plugins_path, BU_DIR_SEPARATOR, filenames[i]);
		_gcv_plugins_load(filter_table, bu_vls_addr(&buffer));
	    }

	bu_vls_free(&buffer);
	bu_vls_free(&pattern);
	bu_argv_free(num_filenames, filenames);
    }
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
	bu_ptbl_init(&filter_table, 64, "filter_table");

	_gcv_filter_register(&filter_table, &_gcv_filter_brlcad_read);
	_gcv_filter_register(&filter_table, &_gcv_filter_brlcad_write);

	_gcv_plugins_load_all(&filter_table);
    }

    return &filter_table;
}


void
gcv_opts_default(struct gcv_opts *gcv_options)
{
    const struct bg_tess_tol default_tessellation_tolerance =
    {BG_TESS_TOL_MAGIC, 0.0, 1.0e-2, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0};

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
    const uint32_t rt_debug_orig = rt_debug;
    const uint32_t nmg_debug_orig = nmg_debug;
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
    rt_debug |= gcv_options->rt_debug_flag;
    nmg_debug |= gcv_options->nmg_debug_flag;

    dbi_read_only_orig = context->dbip->dbi_read_only;

    if (filter->filter_type == GCV_FILTER_WRITE)
	context->dbip->dbi_read_only = 1;

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

    bu_debug = bu_debug_orig;
    rt_debug = rt_debug_orig;
    nmg_debug = nmg_debug_orig;
    context->dbip->dbi_read_only = dbi_read_only_orig;

    _gcv_filter_options_free(filter, options_data);

    if (result != 0 && result != 1)
	bu_bomb("invalid result");

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
