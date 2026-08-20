/*             S T E P _ P L U G I N _ B A D _ A B I . C P P
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 */

#include "step_plugin.h"

static const bu_plugin_manifest incompatible_manifest = {
    "step.test.incompatible",
    1,
    0,
    NULL,
    BU_PLUGIN_ABI_VERSION + 1,
    sizeof(bu_plugin_manifest)
};

BU_PLUGIN_DECLARE_MANIFEST(incompatible_manifest)
