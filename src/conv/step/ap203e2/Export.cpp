/*                    A P 2 0 3 E 2   E X P O R T . C P P
 * BRL-CAD
 *
 * Copyright (c) 2013-2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 */

/** @file step/ap203e2/Export.cpp
 *
 * AP203 edition 2 configuration for the schema-neutral g-step plugin host.
 */

#include "STEPMechanicalExport.h"

extern "C" int
step_ap203e2_export_cli(int argc, char *argv[])
{
    static const STEPMechanicalExportConfig config = {
	"ap203e2",
	"'AP203_CONFIGURATION_CONTROLLED_3D_DESIGN_OF_MECHANICAL_PARTS_AND_ASSEMBLIES_MIM_LF'",
	"'configuration controlled 3D designs of mechanical parts and assemblies'",
	"'BRL-CAD g-step AP203e2 exporter'",
	true,
	true
    };
    return STEPMechanicalExport(argc, argv, config);
}
