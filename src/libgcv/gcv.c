/*                           G C V . C
 * BRL-CAD
 *
 * Copyright (c) 2015-2026 United States Government as represented by
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
 * Main header for LIBGCV.  Includes all available public headers.
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
#include "rt/resource.h"
#include "gcv/api.h"
#include "gcv/util.h"

#include "gcv_private.h"
#include "mime_validate.h"

static int
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

    int version = db_version(in_dbip);
    const struct bu_mime_trait values[] = {
	BU_MIME_TRAIT_STRING("version", version == 4 ? "4" : "5"),
	BU_MIME_TRAIT_STRING("encoding", "binary")
    };
    const struct bu_mime_traits observed = {values, sizeof(values) / sizeof(values[0])};
    if ((version != 4 && version != 5) ||
	!gcv_probe_file_traits_match(context, source_path, "model/vnd.brlcad", &observed, gcv_target_request_traits(context))) {
	bu_log("BRL-CAD input does not satisfy the requested MIME profile (version %d)\n", version);
	db_close(in_dbip);
	return 0;
    }
    struct rt_wdb *writer = wdb_dbopen(context->dbip, RT_WDB_TYPE_DB_INMEM);
    int status = writer && gcv_db_copy(context, writer, in_dbip);
    db_close(in_dbip);
    return status;
}


static int
_gcv_brlcad_write(struct gcv_context *context,
		  const struct gcv_opts *UNUSED(gcv_options), const void *UNUSED(options_data),
		  const char *dest_path)
{
    const char *requested = bu_mime_traits_get(gcv_target_traits(context), "version");
    int version = requested && BU_STR_EQUAL(requested, "4") ? 4 : 5;
    if (requested && !BU_STR_EQUAL(requested, "4") && !BU_STR_EQUAL(requested, "5"))
	return 0;
    struct db_i *dbip = NULL;
    struct rt_wdb *writer = NULL;
    int created = !bu_file_exists(dest_path, NULL);
    if (created) {
	writer = wdb_fopen_v(dest_path, version);
	if (writer) dbip = writer->dbip;
    } else {
	dbip = db_open(dest_path, DB_OPEN_READWRITE);
	if (dbip && (db_version(dbip) != version || db_dirbuild(dbip))) {
	    bu_log("Existing BRL-CAD output does not match requested version %d\n", version);
	    db_close(dbip);
	    return 0;
	}
	if (dbip) writer = wdb_dbopen(dbip, RT_WDB_TYPE_DB_DISK_APPEND_ONLY);
    }
    if (!writer) {
	if (dbip) db_close(dbip);
	bu_log("Cannot open BRL-CAD output %s\n", dest_path);
	return 0;
    }
    int status = gcv_db_copy(context, writer, context->dbip);
    db_close(dbip);
    if (!status && created)
	bu_file_delete(dest_path);
    return status;
}


static enum bu_mime_probe_status
_gcv_brlcad_probe(const struct bu_mime_probe_input *input, const char *UNUSED(type))
{
    const size_t header_size = 8;
    if (input->size < header_size)
	return input->complete ? BU_MIME_PROBE_NO_MATCH : BU_MIME_PROBE_INCOMPLETE;
    if (db5_header_is_valid(input->data))
	return BU_MIME_PROBE_SIGNATURE;
    if (input->size >= sizeof(struct ident) && input->data[0] == ID_IDENT &&
	!memcmp(input->data + offsetof(struct ident, i_version), ID_VERSION, sizeof(ID_VERSION)))
	return BU_MIME_PROBE_SIGNATURE;
    return BU_MIME_PROBE_NO_MATCH;
}


static int
_gcv_filter_register(struct bu_ptbl *filter_table, const struct gcv_filter *filter)
{
    const struct gcv_filter *const *entry;
    if (!filter_table || !filter || !filter->name || !filter->name[0] || !filter->filter_fn ||
	(!filter->create_opts_fn != !filter->free_opts_fn) ||
	(filter->filter_type != GCV_FILTER_FILTER && filter->filter_type != GCV_FILTER_READ &&
	 filter->filter_type != GCV_FILTER_WRITE))
	return 0;
    for (BU_PTBL_FOR(entry, (const struct gcv_filter *const *), filter_table))
	if (BU_STR_EQUAL((*entry)->name, filter->name))
	    return 0;
    bu_ptbl_ins(filter_table, (long *)filter);
    return 1;
}


static void
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


static void
_gcv_filter_options_free(const struct gcv_filter *filter, void *options_data)
{
    if (!filter || (!filter->create_opts_fn != !options_data))
	bu_bomb("missing arguments");

    if (filter->create_opts_fn)
	filter->free_opts_fn(options_data);
}


static int
_gcv_filter_options_process(const struct gcv_filter *filter, size_t argc,
			    const char * const *argv, void **options_data, struct bu_vls *messages)
{
    int ret_argc;
    struct bu_opt_desc *options_desc;
    const char **temp;
    int i;

    if (argc)
	temp = (const char **)bu_calloc(argc, sizeof(const char *), "temp");
    else
	temp = NULL;

    for (i = 0; (size_t)i < argc; ++i)
	temp[i] = argv[i];

    _gcv_filter_options_create(filter, &options_desc, options_data);
    ret_argc = argc ? bu_opt_parse(messages, argc, temp, options_desc) : 0;
    bu_free(options_desc, "options_desc");

    if (ret_argc) {
	if (ret_argc < 0)
	    bu_vls_strcat(messages, "Option parsing failed\n");
	else {
	    bu_vls_strcat(messages, "Unknown arguments: ");

	    for (i = 0; i < ret_argc - 1; ++i)
		bu_vls_printf(messages, "%s, ", temp[i]);

	    bu_vls_printf(messages, "%s\n", temp[i]);
	}

	bu_free((void *)temp, "temp");

	_gcv_filter_options_free(filter, *options_data);
	return 0;
    }

    bu_free((void *)temp, "temp");

    return 1;
}


static void
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


static void
_gcv_context_check(const struct gcv_context *context)
{
    if (!context)
	bu_bomb("null gcv_context");

    RT_CK_DBI(context->dbip);
    BU_CK_AVS(&context->messages);
}

static void
_gcv_plugins_load(struct bu_ptbl *filter_table, const char *path, struct gcv_context *c)
{
    void *info_val;
    const struct gcv_plugin *(*plugin_info)(void);
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

    {
	const struct gcv_plugin_v2 *(*modern_info)(void);
	info_val = bu_dlsym(dl_handle, "gcv_plugin_info_v2");
	modern_info = (const struct gcv_plugin_v2 *(*)(void))(intptr_t)info_val;
	if (modern_info) {
	    struct bu_vls messages = BU_VLS_INIT_ZERO;
	    if (!gcv_register_plugin(c, modern_info(), &messages)) {
		bu_log("Invalid plugin '%s': %s\n", path, bu_vls_cstr(&messages));
		bu_dlclose(dl_handle);
	    } else {
		bu_ptbl_ins(c->i->handles, (long *)dl_handle);
	    }
	    bu_vls_free(&messages);
	    return;
	}
	/* A missing optional modern entry point is not a loader error. */
	(void)bu_dlerror();
    }

    info_val = bu_dlsym(dl_handle, "gcv_plugin_info");
    plugin_info = (const struct gcv_plugin *(*)(void))(intptr_t)info_val;

    if (!plugin_info) {
	const char * const error_msg = bu_dlerror();

	if (error_msg)
	    bu_log("%s\n", error_msg);

	bu_log("Unable to load symbols from '%s' (skipping)\n", path);
	bu_log("Could not find 'gcv_plugin_info' symbol in plugin\n");
	bu_dlclose(dl_handle);
	return;
    }

    plugin = plugin_info();

    if (!plugin || !plugin->filters) {
	bu_log("Invalid plugin encountered from '%s' (skipping)\n", path);
	bu_dlclose(dl_handle);
	return;
    }

    {
	size_t checkpoint = BU_PTBL_LEN(filter_table);
	for (current = plugin->filters; *current; ++current) {
	    if (!_gcv_filter_register(filter_table, *current)) {
		bu_ptbl_trunc(filter_table, checkpoint);
		bu_log("Invalid or conflicting filters from '%s' (skipping)\n", path);
		bu_dlclose(dl_handle);
		return;
	    }
	}
    }
    bu_ptbl_ins(c->i->handles, (long *)dl_handle);
}


static const char *
_gcv_plugins_get_path(void)
{
    const char *pdir = bu_dir(NULL, 0, BU_DIR_LIBEXEC, LIBGCV_PLUGINS_DIRECTORY, NULL);
    const char *result = bu_strdup(pdir);
    return result;
}


static void
_gcv_plugins_load_all(struct bu_ptbl *filter_table, struct gcv_context *context)
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
	bu_vls_sprintf(&pattern, "*%s", LIBGCV_PLUGINS_SUFFIX);
	num_filenames = bu_file_list(plugins_path, bu_vls_addr(&pattern) , &filenames);

	for (i = 0; i < num_filenames; ++i)
	    if (!bu_file_directory(filenames[i])) {
		bu_vls_sprintf(&buffer, "%s%c%s", plugins_path, BU_DIR_SEPARATOR, filenames[i]);
		_gcv_plugins_load(filter_table, bu_vls_addr(&buffer), context);
	    }

	bu_vls_free(&buffer);
	bu_vls_free(&pattern);
	bu_argv_free(num_filenames, filenames);
    }

    bu_free((void *)plugins_path, "plugins_path");
}


void
gcv_context_init(struct gcv_context *context)
{
    context->dbip = db_create_inmem();
    BU_AVS_INIT(&context->messages);
    BU_GET(context->i, struct gcv_context_internal);
    BU_GET(context->i->handles, struct bu_ptbl);
    bu_ptbl_init(context->i->handles, 64, "handles init");
    bu_ptbl_init(&context->i->filters, 0, "GCV filters");
    context->i->filters_loaded = 0;
    context->i->target_mime = NULL;
    context->i->target_traits = NULL;
    context->i->target_request_traits = NULL;
    context->i->mime = bu_mime_registry_create(0);
    context->i->handlers = gcv_handlers_create();
}


void
gcv_context_destroy(struct gcv_context *context)
{
    _gcv_context_check(context);
    bu_ptbl_free(&context->i->filters);
    gcv_handlers_destroy(context->i->handlers);
    bu_mime_registry_destroy(context->i->mime);
    context->i->mime = NULL;
    for (size_t i = 0; i < BU_PTBL_LEN(context->i->handles); i++) {
	void *dl_handle = BU_PTBL_GET(context->i->handles, i);
	bu_dlclose(dl_handle);
    }
    bu_ptbl_free(context->i->handles);
    BU_PUT(context->i->handles, struct bu_ptbl);
    context->i->handles = NULL;

    bu_avs_free(&context->messages);
    db_close(context->dbip);
    context->dbip = NULL;

    BU_PUT(context->i, struct gcv_context_internal);
    context->i = NULL;
}

struct bu_mime_registry *
gcv_mime_registry(struct gcv_context *context)
{
    _gcv_context_check(context);
    return context->i->mime;
}

const char *
gcv_target_mime(struct gcv_context *context)
{
    _gcv_context_check(context);
    return context->i->target_mime;
}

int
gcv_execute_format(struct gcv_context *context, const struct gcv_filter *filter,
		   const char *mime_type, const struct gcv_opts *options,
		   size_t argc, const char *const *argv, const char *target)
{
    return gcv_execute_variant(context, filter, mime_type, NULL, options, argc, argv, target);
}

const struct bu_ptbl *
gcv_list_filters(struct gcv_context *context)
{
    struct bu_ptbl *filter_table;
    _gcv_context_check(context);
    filter_table = &context->i->filters;

    /* Filter pointers and their shared-library handles share one owner. */
    if (!context->i->filters_loaded) {
	static const struct bu_mime_trait v5[] = {BU_MIME_TRAIT_STRING("version", "5")};
	static const struct bu_mime_trait v4[] = {BU_MIME_TRAIT_STRING("version", "4")};
	static const struct bu_mime_capability types[] = {
	    BU_MIME_CAPABILITY("model/vnd.brlcad", v5, 1, 0),
	    BU_MIME_CAPABILITY("model/vnd.brlcad", v4, 1, 0)};
	static const struct bu_mime_capability inputs[] = {
	    {"model/vnd.brlcad", v5, 1, 0, BU_MIME_DEFER_INPUT_VALIDATION},
	    {"model/vnd.brlcad", v4, 1, 0, BU_MIME_DEFER_INPUT_VALIDATION}};
	const struct gcv_conversion reader = {"BRL-CAD Reader", GCV_FILTER_READ, NULL, NULL, _gcv_brlcad_read};
	const struct gcv_conversion writer = {"BRL-CAD Writer", GCV_FILTER_WRITE, NULL, NULL, _gcv_brlcad_write};
	const struct gcv_handler handlers[] = {
	    {&reader, inputs, 2, BU_MIME_DEFAULT_PRIORITY, _gcv_brlcad_probe, NULL},
	    {&writer, types, 2, BU_MIME_DEFAULT_PRIORITY, NULL, NULL}};
	const struct gcv_plugin_v2 builtins = {GCV_PLUGIN_ABI_VERSION,
					       sizeof(struct gcv_plugin_v2), NULL, 0, handlers, 2};
	context->i->filters_loaded = 1;
	if (!gcv_register_plugin(context, &builtins, NULL))
	    bu_bomb("Invalid built-in GCV handlers");
	_gcv_plugins_load_all(filter_table, context);
    }

    return filter_table;
}

const struct gcv_filter *
find_filter(enum gcv_filter_type filter_type, bu_mime_model_t mime_type,
	    const char *data, struct gcv_context *context)
{
    const char *name = bu_mime_legacy_name(mime_type, BU_MIME_MODEL);
    const struct gcv_filter *const *entry;
    const struct bu_ptbl *filters;
    if (mime_type == BU_MIME_MODEL_VND_GDAL || mime_type == BU_MIME_MODEL_VND_ASSETIMPORT) {
	const char *backend = mime_type == BU_MIME_MODEL_VND_GDAL ? "GDAL Reader" :
	    (filter_type == GCV_FILTER_WRITE ? "Assetimport Writer" : "Assetimport Reader");
	filters = gcv_list_filters(context);
	for (BU_PTBL_FOR(entry, (const struct gcv_filter *const *), filters))
	    if ((*entry)->filter_type == filter_type && BU_STR_EQUAL((*entry)->name, backend))
		return *entry;
	return NULL;
    }
    if (name)
	return gcv_select_filter(context, filter_type, name, NULL, NULL);
    filters = gcv_list_filters(context);
    for (BU_PTBL_FOR(entry, (const struct gcv_filter *const *), filters)) {
	if ((*entry)->filter_type == filter_type &&
	    (*entry)->mime_type == BU_MIME_MODEL_AUTO &&
	    (*entry)->data_supported && data && (*entry)->data_supported(data))
	    return *entry;
    }
    return NULL;
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


const char *
gcv_loss_kind_name(enum gcv_loss_kind kind)
{
    switch (kind) {
	case GCV_LOSS_GEOMETRY: return "geometry";
	case GCV_LOSS_APPEARANCE: return "appearance";
	case GCV_LOSS_BEHAVIOR: return "behavior";
	case GCV_LOSS_METADATA: return "metadata";
	default: return NULL;
    }
}

int
gcv_opt_loss_policy(struct bu_vls *messages, size_t argc, const char **argv, void *destination)
{
    static const enum gcv_loss_kind kinds[] = {
	GCV_LOSS_GEOMETRY, GCV_LOSS_APPEARANCE, GCV_LOSS_BEHAVIOR, GCV_LOSS_METADATA
    };
    unsigned int *policy = (unsigned int *)destination;
    BU_OPT_CHECK_ARGV0(messages, argc, argv, "conversion loss category");
    if (!policy) return -1;
    if (BU_STR_EQUAL(argv[0], "all")) {
	*policy |= GCV_LOSS_ALL;
	return 1;
    }
    for (size_t i = 0; i < sizeof(kinds) / sizeof(kinds[0]); ++i) {
	if (BU_STR_EQUAL(argv[0], gcv_loss_kind_name(kinds[i]))) {
	    *policy |= kinds[i];
	    return 1;
	}
    }
    if (messages)
	bu_vls_printf(messages, "Unknown loss category '%s': expected geometry, appearance, behavior, metadata or all\n", argv[0]);
    return -1;
}

void
gcv_conversion_result_free(struct gcv_conversion_result *result)
{
    const struct gcv_conversion_result empty = GCV_CONVERSION_RESULT_INIT_ZERO;
    if (!result) return;
    for (size_t i = 0; i < result->loss_count; ++i) {
	bu_free((void *)result->losses[i].feature, "conversion loss feature");
	bu_free((void *)result->losses[i].message, "conversion loss message");
    }
    bu_free(result->losses, "conversion losses");
    bu_vls_free(&result->messages);
    *result = empty;
}

void
gcv_conversion_result_describe(const struct gcv_conversion_result *result, struct bu_vls *messages)
{
    if (!result || !messages) return;
    const char *status = result->status == GCV_CONVERSION_COMPLETE ? "Complete" :
	result->status == GCV_CONVERSION_PARTIAL ? "Partial" : "Failed";
    bu_vls_printf(messages, "%s conversion\n", status);
    bu_vls_vlscat(messages, &result->messages);
    for (size_t i = 0; i < result->loss_count; ++i) {
	const struct gcv_conversion_loss *loss = &result->losses[i];
	bu_vls_printf(messages, "  %s: %s (%zu): %s\n", gcv_loss_kind_name(loss->kind),
	    loss->feature, loss->count, loss->message);
    }
}

int
gcv_report_loss(struct gcv_context *context, enum gcv_loss_kind kind,
    const char *feature, size_t count, const char *message)
{
    if (!context || !context->i || !context->i->active_result ||
	!gcv_loss_kind_name(kind) || !feature || !feature[0] || !count || !message || !message[0])
	return 0;
    struct gcv_conversion_result *result = context->i->active_result;
    if (result->loss_count >= SIZE_MAX / sizeof(struct gcv_conversion_loss))
	return 0;
    char *feature_copy = bu_strdup(feature);
    char *message_copy = bu_strdup(message);
    result->losses = (struct gcv_conversion_loss *)bu_realloc(result->losses,
	(result->loss_count + 1) * sizeof(struct gcv_conversion_loss), "conversion losses");
    struct gcv_conversion_loss *loss = &result->losses[result->loss_count++];
    loss->kind = kind;
    loss->feature = feature_copy;
    loss->count = count;
    loss->message = message_copy;
    return !(context->i->reject_losses & kind);
}

enum gcv_conversion_status
gcv_execute_internal(struct gcv_context *context, const struct gcv_filter *filter,
	    const struct gcv_opts *gcv_options, size_t argc, const char * const *argv,
	    const char *target, struct gcv_conversion_result *report)
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

    if (gcv_options->reject_losses & ~GCV_LOSS_ALL) {
	bu_vls_strcat(&report->messages, "Invalid conversion loss policy\n");
	return GCV_CONVERSION_FAILED;
    }
    if (!_gcv_filter_options_process(filter, argc, argv, &options_data, &report->messages))
	return GCV_CONVERSION_FAILED;

    struct gcv_conversion_result *previous_result = context->i->active_result;
    unsigned int previous_policy = context->i->reject_losses;
    context->i->active_result = report;
    context->i->reject_losses = gcv_options->reject_losses;

    bu_debug |= gcv_options->bu_debug_flag;
    rt_debug |= gcv_options->rt_debug_flag;
    nmg_debug |= gcv_options->nmg_debug_flag;

    dbi_read_only_orig = context->dbip->dbi_read_only;

    if (filter->filter_type == GCV_FILTER_WRITE)
	context->dbip->dbi_read_only = 1;

    if (!gcv_options->num_objects && filter->filter_type != GCV_FILTER_READ) {
	size_t num_objects;
	struct directory **toplevel_dirs;

	db_update_nref(context->dbip);
	num_objects = db_ls(context->dbip, DB_LS_TOPS, NULL, &toplevel_dirs);

	if (num_objects) {
	    struct gcv_opts temp_options = *gcv_options;
	    char **object_names = db_dpv_to_argv(toplevel_dirs);
	    bu_free(toplevel_dirs, "toplevel_dirs");

	    temp_options.num_objects = num_objects;
	    temp_options.object_names = (const char * const *)object_names;
	    result = gcv_invoke_filter(context, filter, &temp_options, options_data, target, &report->messages);
	    bu_free(object_names, "object_names");
	} else
	    result = gcv_invoke_filter(context, filter, gcv_options, options_data, target, &report->messages);
    } else
	result = gcv_invoke_filter(context, filter, gcv_options, options_data, target, &report->messages);

    bu_debug = bu_debug_orig;
    rt_debug = rt_debug_orig;
    nmg_debug = nmg_debug_orig;
    context->dbip->dbi_read_only = dbi_read_only_orig;
    context->i->active_result = previous_result;
    context->i->reject_losses = previous_policy;

    _gcv_filter_options_free(filter, options_data);

    if (result != 0 && result != 1)
	bu_vls_printf(&report->messages, "Conversion handler %s returned invalid status %d\n", filter->name, result);
    if (result != 1) {
	bu_vls_printf(&report->messages, "Conversion handler %s failed\n", filter->name);
	return GCV_CONVERSION_FAILED;
    }
    for (size_t i = 0; i < report->loss_count; ++i)
	if (report->losses[i].kind & gcv_options->reject_losses) {
	    bu_vls_strcat(&report->messages, "Conversion rejected by the requested loss policy\n");
	    return GCV_CONVERSION_FAILED;
	}
    report->status = report->loss_count ? GCV_CONVERSION_PARTIAL : GCV_CONVERSION_COMPLETE;
    return report->status;
}

void
gcv_conversion_result_log(const struct gcv_conversion_result *result)
{
    if (result->status != GCV_CONVERSION_COMPLETE || bu_vls_strlen(&result->messages)) {
	struct bu_vls messages = BU_VLS_INIT_ZERO;
	gcv_conversion_result_describe(result, &messages);
	bu_log("%s", bu_vls_cstr(&messages));
	bu_vls_free(&messages);
    }
}

int
gcv_execute(struct gcv_context *context, const struct gcv_filter *filter,
    const struct gcv_opts *options, size_t argc, const char *const *argv, const char *target)
{
    struct gcv_conversion_result result = GCV_CONVERSION_RESULT_INIT_ZERO;
    enum gcv_conversion_status status = gcv_execute_internal(context, filter, options, argc, argv, target, &result);
    gcv_conversion_result_log(&result);
    gcv_conversion_result_free(&result);
    return status != GCV_CONVERSION_FAILED;
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
