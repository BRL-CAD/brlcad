/*                 T E S T _ M A T E R I A L . C
 * BRL-CAD
 *
 * Copyright (c) 2019-2021 United States Government as represented by
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

#include <stdio.h>
#include <bu.h>
#include <ged.h>

void
material_creation_test(struct ged *ged){
    //Create test material with whatever you want it called.
    char* material_name = "Material";
    char* object_name = "TestMaterial";
    const char *av[4] = {"material", "create", object_name, material_name};
    ged_exec(ged, 4, (const char **)av);
    printf("%s\n", bu_vls_cstr(ged->ged_result_str));
    const char* argv2[] = {"ls"};
    ged_exec(ged, 1, argv2);
}

void
property_set_test(struct ged *ged){
   char* object_name = "TestMaterial";
   char* property_group = "physicalProperties";
   char* property_name = "density";
   char* property_value = "12.5";
   const char* argv[] = {"material", "set", object_name, property_group, property_name, property_value};
   ged_exec(ged, 6, argv);
}

void
property_get_test(struct ged *ged){
    char* object_name = "TestMaterial";
    char* property_group = "physicalProperties";
    char* property_name = "density";
    const char* argv[] = {"material", "get", object_name, property_group, property_name};
    ged_exec(ged, 5, argv);
}

void
material_destroy_test(struct ged *ged){
    char* object_name = "TestMaterial";
    const char* argv[] = {"material", "destroy", object_name};
    ged_exec(ged, 3, argv);
}

void
material_import_test(struct ged *ged){
    char* file_name = "GQA_SAMPLE_DENSITIES";
    const char* argv[] = {"material", "import", "--id", file_name};
    ged_exec(ged, 4, argv);
}

int
main() {
    struct ged g;
    ged_init(&g);
    material_creation_test(&g);
    property_set_test(&g);
    property_get_test(&g);
    material_destroy_test(&g);
    material_import_test(&g);
    return 0;
}

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */