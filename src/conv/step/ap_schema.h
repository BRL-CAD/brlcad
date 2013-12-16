/*                   A P _ S C H E M A . H
 * BRL-CAD
 *
 * Copyright (c) 2013 United States Government as represented by
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
/** @file step/ap_schema.h
 *
 * Definition based switching for current STEP schema
 *
 */

#ifndef CONV_STEP_AP_SCHEMA_H
#define CONV_STEP_AP_SCHEMA_H

#include "common.h"

#ifdef AP203
#  define SCHEMA_NAMESPACE config_control_design
#  include <SdaiCONFIG_CONTROL_DESIGN.h>
#endif

#ifdef AP203e2
#  include <SdaiAP203_CONFIGURATION_CONTROLLED_3D_DESIGN_OF_MECHANICAL_PARTS_AND_ASSEMBLIES_MIM_LF.h>
#  define SCHEMA_NAMESPACE ap203_configuration_controlled_3d_design_of_mechanical_parts_and_assemblies_mim_lf
#endif

#ifdef AP214e3
#  include <SdaiAUTOMOTIVE_DESIGN.h>
#  define SCHEMA_NAMESPACE automotive_design
#endif

#endif /* CONV_STEP_AP_SCHEMA_H */

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
