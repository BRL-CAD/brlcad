/* M I M E . C P P
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

#include <algorithm>
#include <cstdio>
#include <exception>
#include <sys/stat.h>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "bu/str.h"
#include "bu/file.h"
#include "bu/malloc.h"
#include "gcv_private.h"
#include "../libbu/mime_select.h"

extern "C" int
gcv_invoke_filter(gcv_context *context, const gcv_filter *filter, const gcv_opts *options,
    const void *options_data, const char *target, bu_vls *messages)
{
    try {
	return filter->filter_fn(context, options, options_data, target);
    } catch (const std::exception &error) {
	bu_vls_printf(messages, "Conversion handler %s threw: %s\n", filter->name, error.what());
    } catch (...) {
	bu_vls_printf(messages, "Conversion handler %s threw an unknown exception\n", filter->name);
    }
    return 0;
}

struct gcv_owned_handler {
    std::string name;
    std::vector<bu_mime_capability> formats;
    struct gcv_filter filter;
    unsigned int priority;
    enum bu_mime_probe_status (*probe)(const bu_mime_probe_input *, const char *);
    int (*inspect)(const bu_mime_probe_input *, const char *, bu_mime_traits *);

    explicit gcv_owned_handler(const gcv_handler &source) :
	name(source.conversion->name),
	filter{name.c_str(), source.conversion->filter_type, BU_MIME_MODEL_UNKNOWN, NULL,
	    source.conversion->create_opts_fn, source.conversion->free_opts_fn, source.conversion->filter_fn},
	priority(source.priority), probe(source.probe), inspect(source.inspect)
    {}
    ~gcv_owned_handler() { for (auto &format : formats) bu_mime_capability_free(&format); }
};

struct gcv_handler_registry {
    std::vector<std::unique_ptr<gcv_owned_handler>> handlers;
};

extern "C" gcv_handler_registry *gcv_handlers_create(void) { return new gcv_handler_registry; }
extern "C" void gcv_handlers_destroy(gcv_handler_registry *registry) { delete registry; }

static int
gcv_registration_error(bu_vls *messages, const char *message)
{
    if (messages)
	bu_vls_printf(messages, "%s\n", message);
    return 0;
}

static unsigned int
gcv_operation(gcv_filter_type operation)
{
    return operation == GCV_FILTER_READ ? BU_MIME_READ : operation == GCV_FILTER_WRITE ? BU_MIME_WRITE : 0;
}

extern "C" int
gcv_register_plugin(gcv_context *context, const gcv_plugin_v2 *plugin, bu_vls *messages)
{
    if (!context || !context->i || !plugin || plugin->abi_version != GCV_PLUGIN_ABI_VERSION ||
	plugin->struct_size < sizeof(*plugin) || (plugin->definition_count && !plugin->definitions) ||
	!plugin->handler_count || !plugin->handlers)
	return gcv_registration_error(messages, "Invalid plugin descriptor");
    const bu_ptbl *filters = gcv_list_filters(context);
    std::vector<bu_mime_token> tokens;
    bool valid = true;
    for (size_t i = 0; i < plugin->definition_count; ++i) {
	bu_mime_token token = bu_mime_register(context->i->mime, &plugin->definitions[i]);
	if (!token) { valid = false; break; }
	tokens.push_back(token);
    }
    std::vector<std::unique_ptr<gcv_owned_handler>> prepared;
    for (size_t i = 0; valid && i < plugin->handler_count; ++i) {
	const gcv_handler &handler = plugin->handlers[i];
	const gcv_conversion *filter = handler.conversion;
	if (!filter || !filter->name || !filter->name[0] || !filter->filter_fn ||
	    (filter->filter_type != GCV_FILTER_READ && filter->filter_type != GCV_FILTER_WRITE && filter->filter_type != GCV_FILTER_FILTER) ||
	    (handler.format_count && !handler.formats) || (filter->filter_type != GCV_FILTER_FILTER && !handler.format_count) ||
	    (filter->filter_type == GCV_FILTER_FILTER && (handler.format_count || handler.probe || handler.inspect)) ||
	    handler.priority > BU_MIME_MAX_PRIORITY || (!filter->create_opts_fn != !filter->free_opts_fn)) {
	    valid = false;
	    break;
	}
	for (size_t j = 0; j < BU_PTBL_LEN(filters); ++j)
	    if (BU_STR_EQUAL(reinterpret_cast<const gcv_filter *>(BU_PTBL_GET(filters, j))->name, filter->name))
		valid = false;
	for (const auto &existing : prepared)
	    if (existing->name == filter->name)
		valid = false;
	if (!valid)
	    break;
	std::unique_ptr<gcv_owned_handler> owned(new gcv_owned_handler(handler));
	bu_vls canonical = BU_VLS_INIT_ZERO;
	for (size_t j = 0; j < handler.format_count; ++j) {
	    bu_mime_capability source = handler.formats[j];
	    if ((source.operations && source.operations != gcv_operation(filter->filter_type)) ||
		!bu_mime_canonical(context->i->mime, source.mime_type, &canonical)) {
		valid = false;
		break;
	    }
	    source.mime_type = bu_vls_cstr(&canonical);
	    bu_mime_capability copy = {};
	    if (!bu_mime_capability_copy(&copy, &source)) { valid = false; break; }
	    owned->formats.push_back(copy);
	}
	bu_vls_free(&canonical);
	prepared.push_back(std::move(owned));
    }
    if (!valid) {
	for (bu_mime_token token : tokens)
	    bu_mime_unregister(context->i->mime, token);
	return gcv_registration_error(messages, "Invalid or conflicting MIME/handler metadata");
    }
    for (auto &handler : prepared) {
	bu_ptbl_ins(&context->i->filters, reinterpret_cast<long *>(&handler->filter));
	context->i->handlers->handlers.push_back(std::move(handler));
    }
    return 1;
}

static const gcv_filter *
gcv_select_filter_impl(gcv_context *context, gcv_filter_type operation, const char *mime_type,
    const bu_mime_traits *traits, const char *handler_name, bu_mime_traits *selected_traits, bool *ambiguous_handlers, bu_vls *messages)
{
    if (!context || !context->i || !bu_mime_traits_valid(traits, 0))
	return NULL;
    const bu_ptbl *filters = gcv_list_filters(context);
    bu_vls canonical = BU_VLS_INIT_ZERO;
    if (!bu_mime_canonical(context->i->mime, mime_type, &canonical)) {
	gcv_registration_error(messages, "Unknown MIME type");
	bu_vls_free(&canonical);
	return NULL;
    }
    bu_mime_direction direction = operation == GCV_FILTER_WRITE ? BU_MIME_OUTPUT : BU_MIME_INPUT;
    const gcv_filter *selected = NULL;
    unsigned int best_priority = 0;
    bool ambiguous = false;
    for (size_t i = 0; i < BU_PTBL_LEN(filters); ++i) {
	const auto *filter = reinterpret_cast<const gcv_filter *>(BU_PTBL_GET(filters, i));
	if (filter->filter_type != operation || (handler_name && !BU_STR_EQUAL(handler_name, filter->name)))
	    continue;
	bool modern = false, supported = false;
	bu_mime_traits available = BU_MIME_TRAITS_INIT_ZERO;
	unsigned int priority = BU_MIME_DEFAULT_PRIORITY;
	for (const auto &handler : context->i->handlers->handlers) {
	    if (&handler->filter != filter)
		continue;
	    modern = true;
	    priority = handler->priority;
	    for (const auto &format : handler->formats)
		if (BU_STR_EQUAL(format.mime_type, bu_vls_cstr(&canonical)) && bu_mime_capability_resolve(&format, traits, direction, &available)) {
		    supported = true;
		    break;
		}
	    break;
	}
	if (!modern) {
	    const char *legacy = bu_mime_legacy_name(filter->mime_type, BU_MIME_MODEL);
	    bu_vls name = BU_VLS_INIT_ZERO;
	    bu_mime_capability capability = BU_MIME_CAPABILITY(legacy, NULL, 0, 0);
	    supported = legacy && bu_mime_canonical(context->i->mime, legacy, &name) &&
		BU_STR_EQUAL(bu_vls_cstr(&name), bu_vls_cstr(&canonical)) && bu_mime_capability_resolve(&capability, traits, direction, &available);
	    bu_vls_free(&name);
	}
	if (!supported) {
	    bu_mime_traits_free(&available);
	    continue;
	}
	if (!selected || priority > best_priority) {
	    selected = filter;
	    best_priority = priority;
	    ambiguous = false;
	    if (selected_traits)
		bu_mime_traits_copy(selected_traits, &available);
	} else if (priority == best_priority) {
	    ambiguous = true;
	}
	bu_mime_traits_free(&available);
    }
    if (ambiguous_handlers)
	*ambiguous_handlers = ambiguous;
    if (!selected || ambiguous) {
	if (messages) {
	    bu_vls_printf(messages, "%s for %s", ambiguous ? "Ambiguous handlers" : "No compatible handler", bu_vls_cstr(&canonical));
	    bu_mime_traits_describe(traits, messages);
	    bu_vls_putc(messages, '\n');
	    gcv_handler_info *info = NULL;
	    size_t count = 0;
	    if (gcv_list_handlers(context, bu_vls_cstr(&canonical), &info, &count))
		for (size_t i = 0; i < count; ++i)
		    if (info[i].operation == operation) {
			bu_vls_printf(messages, "  %s (priority %u)", info[i].name, info[i].priority);
			bu_mime_traits_describe(&info[i].traits, messages);
			bu_vls_putc(messages, '\n');
		    }
	    gcv_handler_list_free(info, count);
	}
	selected = NULL;
    }
    bu_vls_free(&canonical);
    return selected;
}

extern "C" const gcv_filter *
gcv_select_variant(gcv_context *context, gcv_filter_type operation, const char *mime_type,
    const bu_mime_traits *traits, const char *handler_name, bu_vls *messages)
{
    return gcv_select_filter_impl(context, operation, mime_type, traits, handler_name, NULL, NULL, messages);
}

extern "C" const gcv_filter *
gcv_select_filter(gcv_context *context, gcv_filter_type operation, const char *mime_type,
    const char *handler_name, bu_vls *messages)
{
    return gcv_select_variant(context, operation, mime_type, NULL, handler_name, messages);
}

static bu_mime_status
gcv_detection_status(const bu_mime_result *result)
{
    if (!result->count)
	return BU_MIME_UNKNOWN_RESULT;
    if (result->count > 1 && result->candidates[0].evidence == result->candidates[1].evidence &&
	result->candidates[0].priority == result->candidates[1].priority)
	return BU_MIME_AMBIGUOUS;
    return BU_MIME_MATCH;
}

static const bu_mime_traits *
gcv_observations(const bu_mime_result *result, const char *type)
{
    if (result)
	for (size_t i = 0; i < result->count; ++i)
	    if (BU_STR_EQUAL(result->candidates[i].name, type))
		return &result->candidates[i].traits;
    return NULL;
}

extern "C" bu_mime_status
gcv_detect(gcv_context *context, const char *filename, const bu_mime_probe_input *input,
    bu_mime_result **result, bu_vls *messages)
{
    if (!result)
	return BU_MIME_ERROR;
    *result = NULL;
    if (!context || !context->i || !input || (!input->data && input->size))
	return BU_MIME_ERROR;
    gcv_list_filters(context);
    bu_mime_probe_input bounded = *input;
    bounded.size = std::min(input->size, bu_mime_scan_limit(context->i->mime));
    bounded.complete = input->complete && bounded.size == input->size;
    const unsigned char empty = 0;
    if (!bounded.data)
	bounded.data = &empty;
    if (bu_mime_identify(context->i->mime, filename, &bounded, result) == BU_MIME_ERROR)
	return BU_MIME_ERROR;
    for (const auto &handler : context->i->handlers->handlers) {
	if ((!handler->probe && !handler->inspect) || handler->filter.filter_type != GCV_FILTER_READ)
	    continue;
	std::set<std::string> checked;
	for (const auto &format : handler->formats) {
	    const char *type = format.mime_type;
	    if (!checked.insert(type).second)
		continue;
	    if (handler->probe) {
		bu_mime_probe_status status = handler->probe(&bounded, type);
		if (status == BU_MIME_PROBE_INCOMPLETE) {
		    (*result)->incomplete = 1;
		    continue;
		}
		if (status == BU_MIME_PROBE_NO_MATCH)
		    continue;
		if (status != BU_MIME_PROBE_HEURISTIC && status != BU_MIME_PROBE_SIGNATURE)
		    goto failed;
		if (bu_mime_result_add(context->i->mime, *result, type,
		    status == BU_MIME_PROBE_SIGNATURE ? BU_MIME_EVIDENCE_SIGNATURE : BU_MIME_EVIDENCE_HEURISTIC,
		    BU_MIME_DEFAULT_PRIORITY) == BU_MIME_ERROR)
		    goto failed;
	    }
	    if (handler->inspect && gcv_observations(*result, type)) {
		bu_mime_traits traits = BU_MIME_TRAITS_INIT_ZERO;
		bool valid = handler->inspect(&bounded, type, &traits) && bu_mime_result_add_traits(*result, type, &traits);
		bu_mime_traits_free(&traits);
		if (!valid)
		    goto failed;
	    }
	}
    }
    return gcv_detection_status(*result);
failed:
    gcv_registration_error(messages, "File identification probe or trait inspector failed");
    bu_mime_result_free(*result);
    *result = NULL;
    return BU_MIME_ERROR;
}

extern "C" bu_mime_status
gcv_detect_file(gcv_context *context, const char *filename, bu_mime_result **result, bu_vls *messages)
{
    if (!result)
	return BU_MIME_ERROR;
    *result = NULL;
    if (!context || !context->i)
	return BU_MIME_ERROR;
    FILE *fp = filename ? fopen(filename, "rb") : NULL;
    if (!fp) {
	gcv_registration_error(messages, "Cannot open input for identification");
	return BU_MIME_ERROR;
    }
    struct stat info;
    if (fstat(fileno(fp), &info) || !S_ISREG(info.st_mode) || info.st_size < 0) {
	fclose(fp);
	gcv_registration_error(messages, "Automatic identification requires a regular input file");
	return BU_MIME_ERROR;
    }
    std::vector<unsigned char> prefix(bu_mime_scan_limit(context->i->mime));
    size_t size = fread(prefix.data(), 1, prefix.size(), fp);
    int complete = feof(fp);
    int error = ferror(fp);
    if (fclose(fp))
	error = 1;
    if (error) {
	gcv_registration_error(messages, "Cannot read input for identification");
	return BU_MIME_ERROR;
    }
    bu_mime_probe_input input = {prefix.data(), size, complete, static_cast<uint64_t>(info.st_size), 1};
    return gcv_detect(context, filename, &input, result, messages);
}

extern "C" const gcv_filter *
gcv_resolve_variant(gcv_context *context, gcv_filter_type operation, const char *filename, const char *format,
    const bu_mime_traits *requested, const char *handler_name, bu_vls *resolved_type,
    bu_mime_traits *resolved_traits, bu_vls *messages)
{
    if (resolved_type)
	bu_vls_trunc(resolved_type, 0);
    if (resolved_traits)
	bu_mime_traits_free(resolved_traits);
    if (!context || !context->i || !bu_mime_traits_valid(requested, 1))
	return NULL;
    gcv_list_filters(context);
    bu_mime_result *result = NULL;
    /* Directory datasets have no byte prefix. Filename associations are
     * hints; their selected readers validate the actual source. */
    if (operation == GCV_FILTER_READ && filename && !bu_file_directory(filename)) {
	if (gcv_detect_file(context, filename, &result, messages) == BU_MIME_ERROR)
	    return NULL;
    } else if (!format || !format[0]) {
	if (bu_mime_detect(context->i->mime, filename, NULL, 0, 0, &result) == BU_MIME_ERROR)
	    return NULL;
    }
    std::vector<std::string> types;
    if (format && format[0]) {
	bu_mime_result *names = NULL;
	if (bu_mime_resolve_candidates(context->i->mime, format, &names) != BU_MIME_ERROR && names)
	    for (size_t i = 0; i < names->count; ++i)
		types.emplace_back(names->candidates[i].name);
	bu_mime_result_free(names);
    } else if (result && result->count) {
	for (size_t i = 0; i < result->count; ++i) {
	    const auto &candidate = result->candidates[i];
	    if (candidate.evidence != result->candidates[0].evidence || candidate.priority != result->candidates[0].priority)
		break;
	    types.emplace_back(candidate.name);
	}
    }
    if (format && format[0]) mime_selection::prefer_observed(types, result);
    const gcv_filter *selected = NULL;
    std::string selected_type;
    bu_mime_traits selected_traits = BU_MIME_TRAITS_INIT_ZERO;
    bu_vls diagnostics = BU_VLS_INIT_ZERO;
    for (const auto &type : types) {
	bu_mime_traits traits = BU_MIME_TRAITS_INIT_ZERO;
	if (!bu_mime_traits_resolve(&traits, gcv_observations(result, type.c_str()), requested)) {
	    bu_vls_printf(&diagnostics, "Requested traits contradict observed %s properties\n", type.c_str());
	    continue;
	}
	bu_mime_traits available = BU_MIME_TRAITS_INIT_ZERO;
	bool ambiguous_handlers = false;
	const gcv_filter *filter = gcv_select_filter_impl(context, operation, type.c_str(), &traits, handler_name, &available, &ambiguous_handlers, &diagnostics);
	if (ambiguous_handlers || (filter && selected)) {
	    selected = NULL;
	    bu_mime_traits_free(&traits);
	    bu_mime_traits_free(&available);
	    gcv_registration_error(&diagnostics, "Ambiguous file types");
	    break;
	}
	if (filter) {
	    selected = filter;
	    selected_type = type;
	    bu_mime_traits_copy(&selected_traits, &available);
	}
	bu_mime_traits_free(&traits);
	bu_mime_traits_free(&available);
    }
    if (selected) {
	if (resolved_type)
	    bu_vls_strcpy(resolved_type, selected_type.c_str());
	if (resolved_traits)
	    bu_mime_traits_copy(resolved_traits, &selected_traits);
    } else if (messages) {
	bu_vls_printf(messages, "No compatible handler: %s\n", format ? format : filename ? filename : "unspecified format");
	bu_vls_vlscat(messages, &diagnostics);
	bu_mime_result_describe(result, messages);
    }
    bu_vls_free(&diagnostics);
    bu_mime_traits_free(&selected_traits);
    bu_mime_result_free(result);
    return selected;
}

extern "C" const gcv_filter *
gcv_resolve_filter(gcv_context *context, gcv_filter_type operation, const char *filename, const char *format,
    const char *handler_name, bu_vls *resolved_type, bu_vls *messages)
{
    return gcv_resolve_variant(context, operation, filename, format, NULL, handler_name, resolved_type, NULL, messages);
}

extern "C" const bu_mime_traits *
gcv_target_traits(gcv_context *context)
{
    return context && context->i ? context->i->target_traits : NULL;
}

extern "C" const bu_mime_traits *
gcv_target_request_traits(gcv_context *context)
{
    return context && context->i ? context->i->target_request_traits : NULL;
}

extern "C" enum gcv_conversion_status
gcv_execute_result(gcv_context *context, const gcv_filter *filter, const char *mime_type,
    const bu_mime_traits *requested, const gcv_opts *options, size_t argc, const char *const *argv,
    const char *target, gcv_conversion_result *result)
{
    if (!result)
	return GCV_CONVERSION_FAILED;
    if (context && context->i && context->i->active_result == result) {
	result->status = GCV_CONVERSION_FAILED;
	bu_vls_strcat(&result->messages, "Nested execution requires an independent result\n");
	return result->status;
    }
    gcv_conversion_result_free(result);
    if (!context || !context->i || !filter || (argc && !argv)) {
	bu_vls_strcat(&result->messages, "Invalid conversion execution arguments\n");
	return result->status;
    }
    const bu_ptbl *filters = gcv_list_filters(context);
    bool owned = false;
    for (size_t i = 0; i < BU_PTBL_LEN(filters); ++i)
	if (BU_PTBL_GET(filters, i) == reinterpret_cast<const long *>(filter))
	    owned = true;
    if (!owned) {
	bu_vls_strcat(&result->messages, "Conversion handle does not belong to this context\n");
	return result->status;
    }
    if (filter->filter_type == GCV_FILTER_FILTER) {
	if (mime_type || requested || target) {
	    bu_vls_strcat(&result->messages, "Non-conversion filters take no MIME, traits or target\n");
	    return result->status;
	}
	result->status = gcv_execute_internal(context, filter, options, argc, argv, NULL, result);
	return result->status;
    }
    if (!target) {
	bu_vls_strcat(&result->messages, "A conversion target is required\n");
	return result->status;
    }
    bu_vls canonical = BU_VLS_INIT_ZERO;
    bu_mime_traits traits = BU_MIME_TRAITS_INIT_ZERO;
    bu_mime_traits assertions = BU_MIME_TRAITS_INIT_ZERO;
    if (!bu_mime_traits_copy(&assertions, requested)) {
	bu_vls_strcat(&result->messages, "Invalid conversion trait assertions\n");
	return result->status;
    }
    const gcv_filter *selected = gcv_resolve_variant(context, filter->filter_type,
	target, mime_type, requested ? &assertions : NULL, filter->name, &canonical, &traits, &result->messages);
    if (selected != filter) {
	bu_vls_free(&canonical);
	bu_mime_traits_free(&traits);
	bu_mime_traits_free(&assertions);
	return result->status;
    }
    const char *previous_type = context->i->target_mime;
    const bu_mime_traits *previous_traits = context->i->target_traits;
    const bu_mime_traits *previous_request = context->i->target_request_traits;
    context->i->target_mime = bu_vls_cstr(&canonical);
    context->i->target_traits = &traits;
    context->i->target_request_traits = requested ? &assertions : NULL;
    result->status = gcv_execute_internal(context, filter, options, argc, argv, target, result);
    context->i->target_mime = previous_type;
    context->i->target_traits = previous_traits;
    context->i->target_request_traits = previous_request;
    bu_vls_free(&canonical);
    bu_mime_traits_free(&traits);
    bu_mime_traits_free(&assertions);
    return result->status;
}

extern "C" int
gcv_execute_variant(gcv_context *context, const gcv_filter *filter, const char *mime_type,
    const bu_mime_traits *requested, const gcv_opts *options, size_t argc, const char *const *argv, const char *target)
{
    gcv_conversion_result result = GCV_CONVERSION_RESULT_INIT_ZERO;
    gcv_conversion_status status = gcv_execute_result(context, filter, mime_type, requested,
	options, argc, argv, target, &result);
    gcv_conversion_result_log(&result);
    gcv_conversion_result_free(&result);
    return status != GCV_CONVERSION_FAILED;
}

extern "C" void
gcv_handler_list_free(gcv_handler_info *info, size_t count)
{
    for (size_t i = 0; i < count; ++i) {
	bu_free(const_cast<char *>(info[i].name), "handler name");
	bu_free(const_cast<char *>(info[i].mime_type), "handler MIME type");
	bu_mime_traits_free(&info[i].traits);
    }
    bu_free(info, "handler metadata");
}

extern "C" int
gcv_list_handlers(gcv_context *context, const char *mime_type, gcv_handler_info **info, size_t *count)
{
    if (!info || !count)
	return 0;
    *info = NULL;
    *count = 0;
    if (!context || !context->i)
	return 0;
    const bu_ptbl *filters = gcv_list_filters(context);
    bu_vls canonical = BU_VLS_INIT_ZERO;
    if (mime_type && !bu_mime_canonical(context->i->mime, mime_type, &canonical)) {
	bu_vls_free(&canonical);
	return 1;
    }
    std::vector<gcv_handler_info> rows;
    for (size_t i = 0; i < BU_PTBL_LEN(filters); ++i) {
	const auto *filter = reinterpret_cast<const gcv_filter *>(BU_PTBL_GET(filters, i));
	bool modern = false;
	for (const auto &handler : context->i->handlers->handlers) {
	    if (&handler->filter != filter)
		continue;
	    modern = true;
	    for (const auto &format : handler->formats)
		if (!mime_type || BU_STR_EQUAL(format.mime_type, bu_vls_cstr(&canonical)))
		    rows.push_back({handler->name.c_str(), format.mime_type, filter->filter_type,
			handler->priority, handler->probe != NULL || handler->inspect != NULL, format.flags, {format.traits, format.trait_count}});
	    break;
	}
	if (!modern) {
	    const char *type = bu_mime_legacy_name(filter->mime_type, BU_MIME_MODEL);
	    if (type && (!mime_type || BU_STR_EQUAL(type, bu_vls_cstr(&canonical))))
		rows.push_back({filter->name, type, filter->filter_type, BU_MIME_DEFAULT_PRIORITY, 0, 0, BU_MIME_TRAITS_INIT_ZERO});
	}
    }
    bu_vls_free(&canonical);
    std::stable_sort(rows.begin(), rows.end(), [](const gcv_handler_info &a, const gcv_handler_info &b) {
	int order = bu_strcmp(a.mime_type, b.mime_type);
	return order ? order < 0 : bu_strcmp(a.name, b.name) < 0;
    });
    if (rows.empty())
	return 1;
    *info = static_cast<gcv_handler_info *>(bu_calloc(rows.size(), sizeof(gcv_handler_info), "handler metadata"));
    *count = rows.size();
    for (size_t i = 0; i < rows.size(); ++i) {
	(*info)[i] = rows[i];
	(*info)[i].name = bu_strdup(rows[i].name);
	(*info)[i].mime_type = bu_strdup(rows[i].mime_type);
	(*info)[i].traits = {NULL, 0};
	bu_mime_traits_copy(&(*info)[i].traits, &rows[i].traits);
    }
    return 1;
}
