/*                    A P 2 4 2   E X P O R T . C P P
 * BRL-CAD
 *
 * Copyright (c) 2013-2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 */

/** @file step/ap242/Export.cpp
 *
 * AP242 configuration for the schema-neutral g-step plugin host.
 */

#include "STEPMechanicalExport.h"

#ifndef STEP_PLUGIN_SCHEMA
#  define STEP_PLUGIN_SCHEMA "ap242e4"
#endif

extern "C" int
step_ap242_export_cli(int argc, char *argv[])
{
    static const STEPMechanicalExportConfig config = {
	STEP_PLUGIN_SCHEMA,
	"'AP242_MANAGED_MODEL_BASED_3D_ENGINEERING_MIM_LF'",
	"'managed model based 3D engineering'",
	"'BRL-CAD g-step AP242 exporter'",
	false,
	true
    };
    return STEPMechanicalExport(argc, argv, config);
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
