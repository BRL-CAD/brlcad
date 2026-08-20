/*                    G -  S T E P . C P P
 * BRL-CAD
 *
 * Copyright (c) 2013-2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 */

/** @file step/g-step/g-step.cpp
 *
 * AP203 configuration for the schema-neutral g-step plugin host.
 */

#include "STEPMechanicalExport.h"

extern "C" int
step_ap203_export_cli(int argc, char *argv[])
{
    static const STEPMechanicalExportConfig config = {
	"ap203",
	"'CONFIG_CONTROL_DESIGN'",
	"'CONFIGURATION CONTROLLED 3D DESIGNS OF MECHANICAL PARTS AND ASSEMBLIES'",
	"'BRL-CAD g-step AP203 exporter'",
	true,
	false
    };
    return STEPMechanicalExport(argc, argv, config);
}
