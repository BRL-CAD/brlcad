/* O B J _ M I M E . C P P
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 * See the file named COPYING for more information.
 */
#include "common.h"
#include <fstream>
#include <sstream>
#include <string>
#include "bu/log.h"
#include "bu/str.h"
#include "mime_obj.h"
#include "../mime_probe.h"
#include "obj_mime.h"

enum bu_mime_probe_status
obj_probe(const struct bu_mime_probe_input *input, const char *)
{
    if (!input->size) return input->complete ? BU_MIME_PROBE_NO_MATCH : BU_MIME_PROBE_INCOMPLETE;
    std::istringstream stream(std::string(reinterpret_cast<const char *>(input->data), input->size));
    const auto profile = mime_obj::inspect(stream, input->complete != 0);
    return profile.identified ? BU_MIME_PROBE_HEURISTIC :
	profile.incomplete ? BU_MIME_PROBE_INCOMPLETE : BU_MIME_PROBE_NO_MATCH;
}

int
obj_input_profile(struct gcv_context *context, const char *path, const struct bu_mime_traits *requested)
{
    std::ifstream stream(path, std::ios::binary);
    if (!stream.is_open()) return 0;
    mime_unicode::text_source source(stream);
    if (!BU_STR_EQUAL(source.width(), "8")) {
	bu_log("OBJ input requires 8-bit source text\n");
	return 0;
    }
    const bool charset = bu_mime_traits_get(requested, "encoding") || bu_mime_traits_get(requested, "text.ascii-only");
    const auto profile = mime_obj::observe(source, charset,
	bu_mime_traits_get(requested, "obj.material-library-reference") != NULL);
    bu_mime_traits observed = BU_MIME_TRAITS_INIT_ZERO;
    const bool valid = !stream.bad() && (!charset || profile.repertoire_known) && profile.append_traits(observed) &&
	gcv_probe_file_traits_match(context, path, mime_obj::type, &observed, requested);
    bu_mime_traits_free(&observed);
    if (!valid) bu_log("OBJ input does not satisfy the requested source text profile\n");
    return valid;
}
