/* M I M E _ P L U G I N . C
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

#include "gcv/runtime.h"

#ifdef BU_MIME_TYPES_H
#  error "Runtime geometry plugins must not depend on generated MIME enums"
#endif

static int
mime_fixture_read(struct gcv_context *context,
		  const struct gcv_opts *UNUSED(options),
		  const void *UNUSED(options_data), const char *UNUSED(target))
{
    bu_avs_add(&context->messages, "mime-handler", "executed");
    return 1;
}

COMPILER_DLLEXPORT const struct gcv_plugin_v2 *
gcv_plugin_info_v2(void)
{
    static const unsigned char signature[] = "MIMETEST";
    static const struct bu_mime_test test = {0, 0, signature, NULL, sizeof(signature) - 1};
    static const struct bu_mime_rule rule = {&test, 1, BU_MIME_EVIDENCE_SIGNATURE, BU_MIME_DEFAULT_PRIORITY};
    static const char *extensions[] = {"mimetest"};
    static const char * aliases[] = {"application/x-mimetest"};
    static const struct bu_mime_definition definitions[] = {
	{BU_MIME_ABI_VERSION, sizeof(struct bu_mime_definition),
	 "application/vnd.example.mimetest", "Runtime MIME fixture data",
	 aliases, 1, extensions, 1, &rule, 1},
	{BU_MIME_ABI_VERSION, sizeof(struct bu_mime_definition),
	 "model/vnd.example.mimetest", "Runtime model fixture data",
	 NULL, 0, NULL, 0, NULL, 0}
    };
    static const struct bu_mime_capability types[] = {
        BU_MIME_CAPABILITY("application/vnd.example.mimetest", NULL, 0, 0),
        BU_MIME_CAPABILITY("model/vnd.example.mimetest", NULL, 0, 0),
    };
    static const struct gcv_conversion conversion = {"Runtime MIME fixture", GCV_FILTER_READ, NULL, NULL, mime_fixture_read};
    static const struct gcv_handler handler = {&conversion, types, 2, BU_MIME_DEFAULT_PRIORITY, NULL, NULL};
    static const struct gcv_plugin_v2 plugin = {
	GCV_PLUGIN_ABI_VERSION, sizeof(struct gcv_plugin_v2), definitions, 2, &handler, 1
    };
    return &plugin;
}
