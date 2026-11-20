/* M I M E _ F U Z Z . C P P
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

#include <cstddef>
#include <cstdint>

#include "bu/app.h"
#include "bu/exit.h"
#include "gcv/api.h"
#include "icv/codec.h"

namespace {
class ProbeContext {
public:
    struct gcv_context value;
    struct icv_context *images;
    ProbeContext() { gcv_context_init(&value); gcv_list_filters(&value); images = icv_context_create(NULL); }
    ~ProbeContext() { icv_context_destroy(images); gcv_context_destroy(&value); }
};

ProbeContext &
probe_context()
{
    static ProbeContext context;
    return context;
}

}

extern "C" int
LLVMFuzzerInitialize(int *, char ***argv)
{
    bu_setprogname((*argv)[0]);
    /* Load instrumented modules before the fuzz engine takes its coverage
     * snapshot. Their installation path is relative to the executable. */
    auto &context = probe_context();
    if (!gcv_select_filter(&context.value, GCV_FILTER_READ, "model/obj", "OBJ Reader", NULL))
	bu_bomb("Native OBJ reader is required by the MIME fuzz harness");
    return 0;
}

extern "C" int
LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    auto &context = probe_context();
    bu_mime_registry *registry = gcv_mime_registry(&context.value);
    bu_mime_token token = 0;
    const size_t control_bytes = 4;
    if (size > control_bytes) {
	size_t offset = (static_cast<size_t>(data[0]) << 8) | data[1];
	size_t length = size - control_bytes;
	if (length > control_bytes)
	    length = control_bytes;
	bu_mime_test test = {offset, offset + data[2], data + control_bytes, NULL, length};
	bu_mime_rule rule = {&test, 1, BU_MIME_EVIDENCE_SIGNATURE,
	    static_cast<unsigned int>(data[3] % (BU_MIME_MAX_PRIORITY + 1))};
	bu_mime_definition definition = {BU_MIME_ABI_VERSION, sizeof(bu_mime_definition),
	    "application/vnd.example.fuzz", NULL, NULL, 0, NULL, 0, &rule, 1};
	token = bu_mime_register(registry, &definition);
    }
    for (int complete = 0; complete <= 1; ++complete) {
	bu_mime_probe_input input = {data, size, complete, size, 1};
	bu_mime_result *result = NULL;
	if (gcv_detect(&context.value, "input.payload", &input, &result, NULL) == BU_MIME_ERROR || !result)
	    bu_bomb("MIME detection failed for valid bounded input");
	for (size_t i = 0; i < result->count; ++i)
	    if (!result->candidates[i].name || result->candidates[i].priority > BU_MIME_MAX_PRIORITY ||
		!bu_mime_traits_valid(&result->candidates[i].traits, 0))
		bu_bomb("MIME detector returned an invalid candidate");
	bu_mime_result_free(result);
	static const char *image_types[] = {"image/png", "image/jpeg", "image/x-portable-pixmap"};
	for (const char *type : image_types) {
	    icv_image_metadata metadata = {};
	    icv_metadata_status status = icv_metadata(context.images, &input, type, NULL, &metadata, NULL);
	    if (status == ICV_METADATA_MATCH) {
		if (!metadata.width || !metadata.height || !metadata.channels)
		    bu_bomb("Image metadata omitted required header properties");
	    } else if (metadata.width || metadata.height || metadata.channels) {
		bu_bomb("Failed image metadata query retained partial properties");
	    }
	}
    }
    if (token && !bu_mime_unregister(registry, token))
	bu_bomb("MIME contribution could not be removed");
    return 0;
}
