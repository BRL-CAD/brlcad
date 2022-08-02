/*              M A T E R I A L X _ T E S T . C P P
 * BRL-CAD
 *
 * Copyright (c) 2021-2022 United States Government as represented by
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

#include <iostream>

#include "bu/app.h"
#include "bu/vls.h"
#include "bu/file.h"
#include "bu/str.h"
#include "rt/nongeom.h"
#include "material/materialX.h"


// this is a unit test for the rt_material_internal function in
// material/material.cpp
int
main(int UNUSED(argc), char **argv)
{
    bu_setprogname(argv[0]);
    // construct new rt_material_internal object and assign a node
    // definition to its optical properties
    struct rt_material_internal material_ip = {
	RT_MATERIAL_MAGIC, BU_VLS_INIT_ZERO, BU_VLS_INIT_ZERO, BU_VLS_INIT_ZERO,
        BU_AVS_INIT_ZERO, BU_AVS_INIT_ZERO, BU_AVS_INIT_ZERO, BU_AVS_INIT_ZERO
    };
    bu_avs_add(&material_ip.opticalProperties, "MTLX", "ND_standard_surface_surfaceshader");

    // call the function for testing
    struct bu_vls vp = BU_VLS_INIT_ZERO;
    rt_material_mtlx_to_osl(material_ip, &vp);

    // check that the resulting file path is not null
    if (BU_STR_EMPTY(bu_vls_cstr(&vp))) {
        std::cout << "MATERIALX UNIT TEST FAILED: FILEPATH IS NULL" << std::endl;
        return 1;
    }

    // check that the file exists
    if (bu_file_exists(bu_vls_cstr(&vp), NULL) <= 0) {
        std::cout << "MATERIALX UNIT TEST FAILED: FILE DOES NOT EXIST" << std::endl;
        return 1;
    }

    std::cout << "MATERIALX UNIT TEST PASSED" << std::endl;
    std::cout << ".OSL IS LOCATED AT: " << vp.vls_str << std::endl;
    return 0;
}


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
