/* C O D E C . H
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
/** @file icv/codec.h
 * Runtime image codec registration and MIME-based dispatch.
 */
#ifndef ICV_CODEC_H
#define ICV_CODEC_H

#include "common.h"
#include <stdio.h>
#include "bu/mime_runtime.h"
#include "icv/image.h"
#include "icv/anim.h"

__BEGIN_DECLS

#define ICV_PLUGIN_ABI_VERSION 7
struct icv_context;

enum icv_codec_operation {
    ICV_CODEC_READ = BU_MIME_READ,
    ICV_CODEC_READ_MEM = BU_MIME_READ_MEMORY,
    ICV_CODEC_WRITE = BU_MIME_WRITE,
    ICV_CODEC_WRITE_MEM = BU_MIME_WRITE_MEMORY,
    ICV_CODEC_METADATA = BU_MIME_METADATA,
    ICV_CODEC_READ_SEQUENCE = BU_MIME_READ_SEQUENCE,
    ICV_CODEC_WRITE_SEQUENCE = BU_MIME_WRITE_SEQUENCE
};

enum icv_metadata_color {
    ICV_METADATA_COLOR_UNKNOWN = 0,
    ICV_METADATA_COLOR_RGB,
    ICV_METADATA_COLOR_GRAY,
    ICV_METADATA_COLOR_CMYK
};

/** Properties of the encoded image, without pixel allocation. Zero fields
 * are unknown. Header inspection does not validate compressed image data. */
struct icv_image_metadata {
    size_t width;
    size_t height;
    size_t channels;
    enum icv_metadata_color color_space;
};

enum icv_metadata_status {
    ICV_METADATA_ERROR = -1,
    ICV_METADATA_UNKNOWN = 0,
    ICV_METADATA_MATCH,
    ICV_METADATA_INCOMPLETE
};

/** One owned row per codec/type/capability clause. */
struct icv_codec_info {
    const char *name;
    const char *mime_type;
    unsigned int operations;
    unsigned int priority;
    size_t raw_pixel_size;
    unsigned int capability_flags;
    struct bu_mime_traits traits;
};

/** One implementation may support several types and any subset of I/O
 * operations.  Each implemented callback follows the existing icv_read or
 * icv_write return/ownership conventions.  Memory writers allocate their
 * output with bu_malloc-compatible storage. Callbacks receive the selected
 * canonical MIME type, including when the caller requested an alias.
 * Input and metadata callbacks receive original caller assertions (possibly
 * NULL) and validate them against their actual input. The separate borrowed
 * observations collection contains independent contextual inspection facts
 * for this type and is valid for the callback duration (NULL when absent).
 * Use bu_mime_traits_match to supplement facts unavailable to the reader;
 * observations used for selection do not become additional assertions.
 * Writers receive the resolved output profile. */
struct icv_codec {
    const char *name;
    const struct bu_mime_capability *formats;
    size_t format_count;
    unsigned int priority;
    icv_image_t *(*read)(FILE *, size_t width, size_t height, const char *mime_type, const struct bu_mime_traits *traits, const struct bu_mime_traits *observations);
    icv_image_t *(*read_mem)(const unsigned char *, size_t size, size_t width, size_t height, const char *mime_type, const struct bu_mime_traits *traits, const struct bu_mime_traits *observations);
    int (*write)(icv_image_t *, FILE *, const char *mime_type, const struct bu_mime_traits *traits);
    int (*write_mem)(icv_image_t *, unsigned char **, size_t *, const char *mime_type, const struct bu_mime_traits *traits);
    enum bu_mime_probe_status (*probe)(const struct bu_mime_probe_input *, const char *mime_type);
    /** Fixed bytes per pixel for headerless data; zero for encoded formats.
     * All types advertised by this codec must share this layout. Capability
     * traits describe supported interpretations, not observations. Raw input
     * assertions require a declared interpretation or an inspected fact;
     * deferred layout matching also applies to size metadata. */
    size_t raw_pixel_size;
    /** Inspect only the supplied prefix. Return UNKNOWN when unsupported,
     * INCOMPLETE when more bytes are needed, ERROR for malformed headers. */
    enum icv_metadata_status (*metadata)(const struct bu_mime_probe_input *,
	const char *mime_type, struct icv_image_metadata *, const struct bu_mime_traits *traits, const struct bu_mime_traits *observations);
    /** Optional bounded variant inspector; use bu_mime_traits_add. */
    int (*inspect)(const struct bu_mime_probe_input *, const char *mime_type, struct bu_mime_traits *traits);
    /** Sequence operations use named files. Readers return a library-owned
     * animation created with icv_anim_create; callers destroy it normally.
     * Sequence readers receive original assertions plus observations and must
     * validate them and their supported profile against actual input.
     * Sequence writers receive the resolved output profile.
     * Reuse context for nested frame codec operations. Writers own opening
     * and closing their output and return zero on success. */
    icv_anim_t *(*read_sequence)(struct icv_context *, const char *filename,
	const char *mime_type, const struct bu_mime_traits *traits, const struct bu_mime_traits *observations);
    int (*write_sequence)(struct icv_context *, const icv_anim_t *, const char *filename,
	const char *mime_type, const struct bu_mime_traits *traits);
};

/** A loadable plugin exports icv_plugin_info() returning this descriptor.
 * Registration copies all descriptive data.  Callback code must remain
 * loaded until its context is destroyed. */
struct icv_plugin {
    unsigned int abi_version;
    size_t struct_size;
    const struct bu_mime_definition *definitions;
    size_t definition_count;
    const struct icv_codec *codecs;
    size_t codec_count;
};

/** Create a context with built-in codecs.  NULL registry creates an owned
 * registry; otherwise the caller's registry is borrowed and must outlive
 * this context.  Context operations require caller serialization. */
ICV_EXPORT struct icv_context *icv_context_create(struct bu_mime_registry *registry);
ICV_EXPORT void icv_context_destroy(struct icv_context *context);
/** Limit bytes buffered for nonseekable automatic reads. Zero (the default)
 * permits unlimited temporary-file storage. Identification always uses the registry
 * scan limit; a selected decoder is required before buffering begins. */
ICV_EXPORT void icv_context_set_spool_limit(struct icv_context *context, size_t bytes);
/** Borrow the context's registry; do not destroy it separately. */
ICV_EXPORT struct bu_mime_registry *icv_mime_registry(struct icv_context *context);
/** Copy registered capabilities, optionally filtered by MIME name/alias.
 * Return 1 on success. The snapshot survives context destruction. */
ICV_EXPORT int icv_list_codecs(struct icv_context *context, const char *mime_type,
    struct icv_codec_info **info, size_t *count);
ICV_EXPORT void icv_codec_list_free(struct icv_codec_info *info, size_t count);
/** Return whether a codec implements all requested operation bits for a
 * type, possibly in different clauses. This query does not assert that all
 * variants are supported; inspect clause traits with icv_list_codecs. */
ICV_EXPORT int icv_supports(struct icv_context *context, const char *mime_type,
    unsigned int operations);
/** Select one operation without decoding or creating output. Detection may
 * contain several candidate types; only equally strong candidates with a
 * compatible codec participate. Type/extension and codec overrides may be
 * NULL. Outputs are optional, cleared on failure, and owned by the caller.
 * Initialize resolved_traits to zero before use. Selection verifies the
 * advertised contract, not the validity of the entire document. */
ICV_EXPORT int icv_select_variant(struct icv_context *context,
    enum icv_codec_operation operation, const char *mime_type, const char *codec_name,
    const struct bu_mime_result *detected, const struct bu_mime_traits *requested,
    struct bu_vls *resolved_type, struct bu_vls *resolved_codec,
    struct bu_mime_traits *resolved_traits, struct bu_vls *messages);
/** File convenience query. Reads a bounded prefix for READ, READ_SEQUENCE and METADATA;
 * output selection uses only the filename. Memory operations use
 * icv_select_variant with the result of icv_detect instead. */
ICV_EXPORT int icv_resolve_variant(struct icv_context *context,
    enum icv_codec_operation operation, const char *filename,
    const char *mime_type, const char *codec_name, const struct bu_mime_traits *requested,
    struct bu_vls *resolved_type, struct bu_vls *resolved_codec,
    struct bu_mime_traits *resolved_traits, struct bu_vls *messages);
/** Inspect encoded properties using the selected codec. Output is cleared
 * unless MATCH is returned. A NULL type requests content identification.
 * The file variant reads at most the registry scan limit from a regular file. */
ICV_EXPORT enum icv_metadata_status icv_metadata(struct icv_context *context,
    const struct bu_mime_probe_input *input, const char *mime_type,
    const char *codec_name, struct icv_image_metadata *metadata, struct bu_vls *messages);
ICV_EXPORT enum icv_metadata_status icv_metadata_file(struct icv_context *context,
    const char *filename, const char *mime_type, const char *codec_name,
    struct icv_image_metadata *metadata, struct bu_vls *messages);
/** Metadata selection with the same trait assertions as variant decoding. */
ICV_EXPORT enum icv_metadata_status icv_metadata_variant(struct icv_context *context,
    const struct bu_mime_probe_input *input, const char *mime_type,
    const char *codec_name, const struct bu_mime_traits *requested,
    struct icv_image_metadata *metadata, struct bu_vls *messages);
ICV_EXPORT enum icv_metadata_status icv_metadata_file_variant(struct icv_context *context,
    const char *filename, const char *mime_type, const char *codec_name,
    const struct bu_mime_traits *requested, struct icv_image_metadata *metadata,
    struct bu_vls *messages);
/** Inspect without decoding, including the context's structural probes. */
ICV_EXPORT enum bu_mime_status icv_detect(struct icv_context *context,
    const char *filename, const struct bu_mime_probe_input *input,
    struct bu_mime_result **result, struct bu_vls *messages);
ICV_EXPORT enum bu_mime_status icv_detect_file(struct icv_context *context,
    const char *filename, struct bu_mime_result **result, struct bu_vls *messages);
/** Infer dimensions of fixed-layout raw data. Named dimensions may be
 * queried without data (data_size == 0). Encoded formats are rejected. */
ICV_EXPORT int icv_image_size_with_context(struct icv_context *context,
    const char *label, size_t dpi, size_t data_size, const char *mime_type,
    size_t *width, size_t *height);
/** Register as a unit, returning 1 on success and 0 with a diagnostic on
 * invalid metadata or conflicts.  Statically linked plugins may use this. */
ICV_EXPORT int icv_register_plugin(struct icv_context *context,
	const struct icv_plugin *plugin, struct bu_vls *messages);
/** Load an explicit shared library.  The context keeps it loaded until
 * destruction.  Return 1 on success, 0 with a diagnostic otherwise. */
ICV_EXPORT int icv_load_plugin(struct icv_context *context,
	const char *path, struct bu_vls *messages);

/** MIME type and codec_name may be NULL for automatic selection.  Detection
 * uses content before filename evidence.  Ambiguity, unrecognized input,
 * unavailable codecs, and I/O errors are reported through messages.
 * Raw images require appropriate dimensions. When both dimensions are zero,
 * fixed-layout raw codecs may infer dimensions from a regular file's size.
 * A NULL context uses built-in codecs for the duration of this call; reuse
 * an explicit context for batch operations or plugin codecs. */
ICV_EXPORT icv_image_t *icv_read_with_context(struct icv_context *context,
	const char *filename, const char *mime_type, const char *codec_name,
	size_t width, size_t height, struct bu_vls *messages);
ICV_EXPORT icv_image_t *icv_read_mem_with_context(struct icv_context *context,
	const unsigned char *buffer, size_t size, const char *mime_type,
	const char *codec_name, size_t width, size_t height, struct bu_vls *messages);
/** Output type is explicit or inferred from filename.  Memory output
 * requires an explicit type.  Return 0 on success, BRLCAD_ERROR otherwise. */
ICV_EXPORT int icv_write_with_context(struct icv_context *context,
	icv_image_t *image, const char *filename, const char *mime_type,
	const char *codec_name, struct bu_vls *messages);
ICV_EXPORT int icv_write_mem_with_context(struct icv_context *context,
	icv_image_t *image, unsigned char **buffer, size_t *size,
	const char *mime_type, const char *codec_name, struct bu_vls *messages);

/** Trait-aware I/O. Known input facts must agree with explicit requests.
 * Missing facts may be asserted by the caller; unknown facts never satisfy
 * restricted codecs automatically. Output requests must be advertised by
 * a matching capability clause. Readers receive original caller assertions;
 * writers receive the resolved output profile. Selection queries retain
 * observed/resolved traits independently of callback assertions. */
ICV_EXPORT icv_image_t *icv_read_variant(struct icv_context *context,
    const char *filename, const char *mime_type, const char *codec_name,
    const struct bu_mime_traits *traits, size_t width, size_t height, struct bu_vls *messages);
ICV_EXPORT icv_image_t *icv_read_mem_variant(struct icv_context *context,
    const unsigned char *buffer, size_t size, const char *mime_type, const char *codec_name,
    const struct bu_mime_traits *traits, size_t width, size_t height, struct bu_vls *messages);
ICV_EXPORT int icv_write_variant(struct icv_context *context, icv_image_t *image,
    const char *filename, const char *mime_type, const char *codec_name,
    const struct bu_mime_traits *traits, struct bu_vls *messages);
ICV_EXPORT int icv_write_mem_variant(struct icv_context *context, icv_image_t *image,
    unsigned char **buffer, size_t *size, const char *mime_type, const char *codec_name,
    const struct bu_mime_traits *traits, struct bu_vls *messages);

/** Sequence I/O through the same registry, selection and plugin context as
 * still images. A named file is required; memory and standard-stream I/O
 * are not sequence operations. NULL context creates temporary built-ins.
 * Input type may be NULL for detection; output type may be NULL for filename
 * selection. Explicit MIME/codec/trait requests must match the selected
 * operation. Rejected output selections do not open the destination.
 * Returned animations retain their source MIME identity independently of
 * context lifetime. Return NULL on read failure, zero on write success and
 * BRLCAD_ERROR on write failure. */
ICV_EXPORT icv_anim_t *icv_anim_read_variant(struct icv_context *context,
    const char *filename, const char *mime_type, const char *codec_name,
    const struct bu_mime_traits *traits, struct bu_vls *messages);
ICV_EXPORT int icv_anim_write_variant(struct icv_context *context, const icv_anim_t *animation,
    const char *filename, const char *mime_type, const char *codec_name,
    const struct bu_mime_traits *traits, struct bu_vls *messages);

/** Write an 8-bit color-mapped Sun rasterfile. A nonzero dither value
 * enables palette dithering. Pass NULL filename for standard output.
 * For ordinary output, use icv_write_with_context with image/x-sun-raster.
 * Return 0 on success, BRLCAD_ERROR on failure. */
ICV_EXPORT int icv_write_sun(icv_image_t *image, const char *filename, int dither);

__END_DECLS
#endif
