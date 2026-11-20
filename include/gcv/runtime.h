/*                       G C V _ A P I . H
 * BRL-CAD
 *
 * Copyright (c) 2008-2026 United States Government as represented by
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
/** @file gcv/runtime.h
 *
 * Main public API of the LIBGCV geometry conversion library.
 *
 */

#ifndef GCV_RUNTIME_H
#define GCV_RUNTIME_H

#include "common.h"
#include "vmath.h"

#include "bu/avs.h"
#include "bu/opt.h"
#include "bu/mime_runtime.h"
#include "bn/tol.h"
#include "rt/defines.h"
#include "rt/tol.h"
#include "gcv/defines.h"

__BEGIN_DECLS


/**
 * The big kahuna.
 */
struct gcv_context_internal;
struct gcv_context
{
    struct db_i *dbip;
    bu_avs_t messages;
    struct gcv_context_internal *i;  // Internal information
};


/**
 * Initialize a conversion context.
 */
GCV_EXPORT void
gcv_context_init(struct gcv_context *cxt);


/**
 * Release a conversion context, freeing all memory internally
 * allocated by the library.  Caller is responsible for freeing the
 * context pointer itself, if necessary.
 */
GCV_EXPORT void
gcv_context_destroy(struct gcv_context *cxt);

/** Return the context-owned MIME registry.  Callers may add definitions;
 * the returned registry must not be destroyed separately.  Operations on a
 * single GCV context, including plugin discovery, require caller serialization. */
GCV_EXPORT struct bu_mime_registry *gcv_mime_registry(struct gcv_context *context);

enum gcv_tessellation_algorithm {
    GCV_TESS_DEFAULT = 0,
    GCV_TESS_BOTTESS,
    GCV_TESS_MARCHING_CUBES
};


enum gcv_filter_type {
    GCV_FILTER_FILTER,
    GCV_FILTER_READ,
    GCV_FILTER_WRITE
};

/** Loss categories describe conversion intent, independently of format
 * observations and handler selection. Each reported loss has one category. */
enum gcv_loss_kind {
    GCV_LOSS_GEOMETRY = 1u << 0,
    GCV_LOSS_APPEARANCE = 1u << 1,
    GCV_LOSS_BEHAVIOR = 1u << 2,
    GCV_LOSS_METADATA = 1u << 3
};
#define GCV_LOSS_ALL (GCV_LOSS_GEOMETRY | GCV_LOSS_APPEARANCE | GCV_LOSS_BEHAVIOR | GCV_LOSS_METADATA)

enum gcv_conversion_status {
    GCV_CONVERSION_FAILED = 0,
    GCV_CONVERSION_COMPLETE = 1,
    GCV_CONVERSION_PARTIAL = 2
};

/** Owned by the result, independent of the plugin and context lifetime.
 * feature is a stable format-scoped identifier, e.g. gltf.skinning.
 * count is the number of affected features/objects, as documented by the
 * handler; it is not a count of database objects successfully written. */
struct gcv_conversion_loss {
    enum gcv_loss_kind kind;
    const char *feature;
    size_t count;
    const char *message;
};

/** Initialize with GCV_CONVERSION_RESULT_INIT_ZERO. Execution replaces a
 * previous result; free it with gcv_conversion_result_free(). COMPLETE
 * means no reported losses, not a guarantee that every handler has been
 * audited for every possible source feature. */
struct gcv_conversion_result {
    enum gcv_conversion_status status;
    struct gcv_conversion_loss *losses;
    size_t loss_count;
    struct bu_vls messages;
};
#define GCV_CONVERSION_RESULT_INIT_ZERO {GCV_CONVERSION_FAILED, NULL, 0, BU_VLS_INIT_ZERO}

/** Stable category spelling; NULL for invalid or combined categories. */
GCV_EXPORT const char *gcv_loss_kind_name(enum gcv_loss_kind kind);
/** bu_opt callback: OR one category name (or all) into an unsigned int
 * rejection mask. Use repeatable options for multiple categories. */
GCV_EXPORT int gcv_opt_loss_policy(struct bu_vls *messages, size_t argc, const char **argv, void *destination);
/** Release owned data and reset to the zero initializer. */
GCV_EXPORT void gcv_conversion_result_free(struct gcv_conversion_result *result);
/** Append status, diagnostics and reported losses to messages. */
GCV_EXPORT void gcv_conversion_result_describe(const struct gcv_conversion_result *result, struct bu_vls *messages);

/** Report an omitted or approximated source feature during a callback.
 * Copies feature/message. Returns 1 when the current policy permits this
 * loss, 0 when rejected or called outside execution/with invalid arguments.
 * Check the return before writing affected output where possible. Rejection
 * also makes the final execution fail if a callback ignores the return.
 * Known losses remain in failed results. Generic execution does not roll
 * back database mutations or output files made before failure. */
GCV_EXPORT int gcv_report_loss(struct gcv_context *context, enum gcv_loss_kind kind,
    const char *feature, size_t count, const char *message);


/**
 * Conversion options.
 */
struct gcv_opts
{
    /* debug flags, applied via bitwise-OR
     * original flags are restored after conversion */
    int bu_debug_flag;
    uint32_t rt_debug_flag;
    uint32_t nmg_debug_flag;

    /* print debugging info if debug_mode == 1 */
    unsigned debug_mode;

    /* 0 is quiet, printing only errors */
    unsigned verbosity_level;

    /* max cpus; 0 for automatic */
    unsigned max_cpus;

    struct bn_tol calculational_tolerance;
    struct bg_tess_tol tessellation_tolerance;
    enum gcv_tessellation_algorithm tessellation_algorithm;

    /* conversion to units */
    fastf_t scale_factor;

    /* default name for nameless objects */
    const char *default_name;

    /* number of elements in object_names
     * if 0, convert all top-level objects */
    size_t num_objects;

    /* objects to convert */
    const char * const *object_names;

    /* Apply color randomization */
    unsigned randomize_colors;

    /** Bitmask of GCV_LOSS_* categories to reject. Zero (default) permits
     * reported losses and returns PARTIAL through gcv_execute_result(). */
    unsigned int reject_losses;
};


/**
 * Assign default option values.
 */
GCV_EXPORT void
gcv_opts_default(struct gcv_opts *gcv_options);


/** Opaque execution handle owned by its conversion context. */
struct gcv_filter;

/** MIME-independent execution callbacks. Registered handlers are copied
 * into context-owned execution handles. Options and callback return values
 * follow gcv_execute conventions. */
struct gcv_conversion {
    const char *name;
    enum gcv_filter_type filter_type;
    /** Optional allocation/free pair. The allocator sets both the option
     * descriptors and opaque option data for gcv_execute's parser. */
    void (*create_opts_fn)(struct bu_opt_desc **, void **);
    void (*free_opts_fn)(void *);
    /** Return 1 for complete or permitted partial conversion, 0 on failure.
     * Report omitted/approximated features using gcv_report_loss().
     * Options are always valid and
     * opaque data comes from create_opts_fn (NULL when absent). Target is
     * the input/output path for readers/writers, NULL for other filters.
     * Writers must treat context->dbip as read-only. Execution exceptions
     * become failed results; execution state and option data are cleaned up. */
    int (*filter_fn)(struct gcv_context *, const struct gcv_opts *, const void *, const char *);
};

/** Versioned runtime plugins advertise file types independently of their
 * implementation.  A filter can support any number of MIME types, including
 * types outside the model category. No generated MIME enum is part of this interface.
 * Higher priority is preferred among handlers for the same operation/type;
 * equally preferred handlers require an explicit handler name. */
#define GCV_PLUGIN_ABI_VERSION 5
/** Broad-format backends rank below dedicated readers and writers. */
#define GCV_FALLBACK_PRIORITY (BU_MIME_DEFAULT_PRIORITY / 2)
/** Non-conversion filters use no format clauses or probes. */
struct gcv_handler {
    const struct gcv_conversion *conversion;
    const struct bu_mime_capability *formats;
    size_t format_count;
    unsigned int priority;
    enum bu_mime_probe_status (*probe)(const struct bu_mime_probe_input *, const char *mime_type);
    /** Optional bounded trait inspector. Populate owned traits using libbu's
     * allocation API; return 1 on success, 0 on inspection error. */
    int (*inspect)(const struct bu_mime_probe_input *, const char *mime_type, struct bu_mime_traits *traits);
};

/** One owned row per handler/type/capability clause. */
struct gcv_handler_info {
    const char *name;
    const char *mime_type;
    enum gcv_filter_type operation;
    unsigned int priority;
    /** Handler-specific callbacks; shared registry recognition is separate. */
    int has_probe;
    unsigned int capability_flags;
    struct bu_mime_traits traits;
};

/** Copy capabilities, optionally filtered by MIME name/alias. Return 1 on
 * success, including an empty result. Snapshots survive context destruction. */
GCV_EXPORT int gcv_list_handlers(struct gcv_context *context, const char *mime_type,
    struct gcv_handler_info **info, size_t *count);
GCV_EXPORT void gcv_handler_list_free(struct gcv_handler_info *info, size_t count);

/** Export gcv_plugin_info_v2() returning this descriptor from a loadable
 * plugin.  Descriptors and MIME metadata are copied; executable callbacks
 * must remain loaded until context destruction.  Legacy plugin descriptors
 * remain supported. */
struct gcv_plugin_v2 {
    unsigned int abi_version;
    size_t struct_size;
    const struct bu_mime_definition *definitions;
    size_t definition_count;
    const struct gcv_handler *handlers;
    size_t handler_count;
};

/** Register an entire plugin as a unit. Return 1 on success, 0 on invalid
 * metadata or conflicts, appending a diagnostic to messages when supplied.
 * Also usable by statically linked plugins.  No library is loaded by this
 * call; the caller owns the lifetime of its executable callbacks. */
GCV_EXPORT int gcv_register_plugin(struct gcv_context *context,
				   const struct gcv_plugin_v2 *plugin, struct bu_vls *messages);

/** Select a handler by canonical MIME name or alias, independently of file
 * detection.  handler_name optionally selects an implementation explicitly.
 * Return NULL with a diagnostic for unknown types, unavailable operations,
 * or ambiguous handlers.  Returned filters live until context destruction. */
GCV_EXPORT const struct gcv_filter *gcv_select_filter(struct gcv_context *context,
						      enum gcv_filter_type operation, const char *mime_type,
						      const char *handler_name, struct bu_vls *messages);

/** Select with known or explicitly supplied format traits. An unknown
 * trait does not satisfy a constrained input handler. For output, each
 * requested trait must be advertised by a matching capability clause. */
GCV_EXPORT const struct gcv_filter *gcv_select_variant(struct gcv_context *context,
    enum gcv_filter_type operation, const char *mime_type, const struct bu_mime_traits *traits,
    const char *handler_name, struct bu_vls *messages);
/** Identify input and combine observed traits with explicit requests.
 * Directory inputs use registered filename associations or an explicit
 * format, with source validation left to the selected reader. Byte-prefix
 * identification remains limited to regular files.
 * Known contradictions fail. Optional resolved_traits is an owned copy;
 * initialize it to zero and release with bu_mime_traits_free. */
GCV_EXPORT const struct gcv_filter *gcv_resolve_variant(struct gcv_context *context,
    enum gcv_filter_type operation, const char *filename, const char *format,
    const struct bu_mime_traits *requested, const char *handler_name,
    struct bu_vls *resolved_type, struct bu_mime_traits *resolved_traits, struct bu_vls *messages);
/** Execute a context-owned compatible handle, making the selected traits
 * available to callbacks. Input files are inspected to reject known
 * contradictions before options or conversion callbacks run. */
GCV_EXPORT int gcv_execute_variant(struct gcv_context *context, const struct gcv_filter *filter,
    const char *mime_type, const struct bu_mime_traits *traits, const struct gcv_opts *options,
    size_t argc, const char *const *argv, const char *target);
/** Execute with an owned structured result. Validates the context-owned
 * handle and, for format operations, the MIME/trait contract. MIME may be
 * NULL to resolve from the target. Non-conversion filters take no MIME,
 * traits or target. Diagnostics and losses survive context destruction.
 * The return value equals result->status; result must be initialized and
 * non-NULL. Boolean execution APIs return 1 for permitted partial results
 * and log their losses; use this API to distinguish them from COMPLETE.
 * Nested executions have independent results. */
GCV_EXPORT enum gcv_conversion_status gcv_execute_result(struct gcv_context *context,
    const struct gcv_filter *filter, const char *mime_type, const struct bu_mime_traits *traits,
    const struct gcv_opts *options, size_t argc, const char *const *argv, const char *target,
    struct gcv_conversion_result *result);
/** Borrow selected traits during execution; NULL outside execution. */
GCV_EXPORT const struct bu_mime_traits *gcv_target_traits(struct gcv_context *context);
/** Borrow an execution-owned copy of the caller's original assertions,
 * before detection facts or capability defaults are added. NULL when no
 * traits were supplied or outside execution. An explicitly supplied empty
 * collection remains distinguishable from NULL. Nested conversions restore
 * the parent request. Readers validate this view against independent file
 * facts; observations from other plugins are not extra caller assertions.
 * Writers with legacy backend options may use this view
 * to distinguish explicit constraints from provisional variant defaults. */
GCV_EXPORT const struct bu_mime_traits *gcv_target_request_traits(struct gcv_context *context);


/** Inspect a supplied prefix, incorporating registered structural probes.
 * Metadata and rules share the same result/evidence policy as libbu.
 * Callbacks run without registry locks. */
GCV_EXPORT enum bu_mime_status gcv_detect(struct gcv_context *context,
					  const char *filename, const struct bu_mime_probe_input *input,
					  struct bu_mime_result **result, struct bu_vls *messages);
/** Inspect a regular file with the same bounded rules and probes. */
GCV_EXPORT enum bu_mime_status gcv_detect_file(struct gcv_context *context,
    const char *filename, struct bu_mime_result **result, struct bu_vls *messages);
/** Resolve an explicit MIME name/alias/extension, or detect input from its
 * contents and filename.  Output detection uses the filename only.
 * A handler name optionally chooses an implementation. */
GCV_EXPORT const struct gcv_filter *gcv_resolve_filter(struct gcv_context *context,
						       enum gcv_filter_type operation, const char *filename, const char *format,
						       const char *handler_name, struct bu_vls *resolved_type, struct bu_vls *messages);

/** Borrow the requested canonical type during gcv_execute_format(), or
 * return NULL when invoked through the legacy execution interface. */
GCV_EXPORT const char *gcv_target_mime(struct gcv_context *context);
/** Execute with an explicit MIME type available to multi-format writers.
 * Reject handles outside this context and types not advertised by the
 * selected handler before options or execution callbacks run. */
GCV_EXPORT int gcv_execute_format(struct gcv_context *context, const struct gcv_filter *filter,
				  const char *mime_type, const struct gcv_opts *options, size_t argc,
				  const char *const *argv, const char *target);

/*
 * Return a pointer to a bu_ptbl listing all registered filters as
 * const struct gcv_filter pointers.
 */
GCV_EXPORT const struct bu_ptbl *gcv_list_filters(struct gcv_context *context);

/**
 * Perform a filtering operation on a gcv_context.
 *
 * If 'gcv_options' is NULL, defaults will be used as set by gcv_opts_default().
 * The parameters 'argc' and 'argv' are used for option processing as specified by
 * the filter.
 *
 * Returns 1 for complete or permitted partial conversion, 0 on failure.
 * Reported losses are logged. Use gcv_execute_result() for structured results.
 */
GCV_EXPORT int
gcv_execute(struct gcv_context *context, const struct gcv_filter *filter, const struct gcv_opts *gcv_options, size_t argc, const char * const *argv, const char *target);


__END_DECLS

#endif /* GCV_RUNTIME_H */

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
