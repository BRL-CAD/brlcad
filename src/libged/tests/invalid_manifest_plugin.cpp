/*          I N V A L I D _ M A N I F E S T _ P L U G I N . C P P
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License version 2.1 as
 * published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser
 * General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this file; see the file named COPYING for more information.
 */

#include "common.h"

#include "../include/plugin.h"


static int
invalid_manifest_probe(struct ged *, int, const char **)
{
    return 0;
}


static bu_plugin_cmd invalid_commands[] = {
    {"registry_invalid_manifest_probe", invalid_manifest_probe}
};


/* Deliberately invalid.  The readable schema manifest below proves libged
 * never registers metadata from a second inspection of a rejected module. */
static bu_plugin_manifest invalid_manifest = {
    "invalid-registry-fixture", 1, 1, invalid_commands,
    BU_PLUGIN_ABI_VERSION + 1, sizeof(bu_plugin_manifest)
};
BU_PLUGIN_DECLARE_MANIFEST(invalid_manifest)


static const struct bu_cmd_schema invalid_native_schema =
    BU_CMD_SCHEMA("registry_invalid_manifest_probe", "Must never be published",
	NULL, NULL, BU_CMD_PARSE_INTERSPERSED,
	BU_CMD_SCHEMA_META(NULL, NULL, NULL, NULL));

static const struct ged_cmd_schema invalid_schemas[] = {
    {"registry_invalid_manifest_probe", &invalid_native_schema, NULL, NULL}
};

static const struct ged_plugin_schema_manifest invalid_schema_manifest = {
    "invalid-registry-fixture", 1, 1, invalid_schemas,
    GED_PLUGIN_SCHEMA_ABI_VERSION, sizeof(struct ged_plugin_schema_manifest)
};

extern "C" BU_PLUGIN_EXPORT const struct ged_plugin_schema_manifest *
ged_plugin_schema_info(void)
{
    return &invalid_schema_manifest;
}

/*
 * Local Variables:
 * mode: C++
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
