
#include "common.h"

#include <iostream>

#include "bu/app.h"
#include "bu/vls.h"
#include "bu/file.h"
#include "bu/str.h"
#include "rt/nongeom.h"
#include "material/materialX.h"

// this is a unit test for the rt_material_internal function in material/material.cpp

int main()
{
    
    // construct new rt_material_internal object and assign a node definition to its optical properties
    struct rt_material_internal material_ip = { RT_MATERIAL_MAGIC, BU_VLS_INIT_ZERO, BU_VLS_INIT_ZERO, BU_VLS_INIT_ZERO,
        BU_AVS_INIT_ZERO, BU_AVS_INIT_ZERO, BU_AVS_INIT_ZERO, BU_AVS_INIT_ZERO };
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
