/*                       A P 2 1 4   E X P O R T . C P P
 * BRL-CAD
 *
 * Copyright (c) 2013-2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 */

/** @file step/ap214/Export.cpp
 *
 * AP214 configuration for the schema-neutral g-step plugin host.
 */

#include "STEPMechanicalExport.h"

extern "C" int
step_ap214_export_cli(int argc, char *argv[])
{
    static const STEPMechanicalExportConfig config = {
	"ap214",
	"'AUTOMOTIVE_DESIGN {1 2 10303 214 0 1 1 1}'",
	"'Core Data for Automotive Mechanical Design Process'",
	"'BRL-CAD g-step AP214 exporter'",
	false,
	true
    };
    return STEPMechanicalExport(argc, argv, config);
}
