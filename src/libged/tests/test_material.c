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

#include "bu.h"
#include "ged.h"


void
material_creation_test(struct ged *ged)
{
    //Create test material with whatever you want it called.
    printf("Testing creating a material using the material command...");
    char* material_name = "Material";
    char* object_name = "TestMaterial";
    const char *av[4] = {"material", "create", object_name, material_name};
    ged_exec(ged, 4, (const char **)av);
    const char* argv2[] = {"ls"};
    ged_exec(ged, 1, argv2);
    printf("%s\n\n", bu_vls_cstr(ged->ged_result_str));
    printf("Testing when there is not enough arguments: ");
    const char *av2[4] = {"material", "create", object_name};
    ged_exec(ged, 3, (const char **)av2);
    printf("%s\n", bu_vls_cstr(ged->ged_result_str));
}


void
property_set_test(struct ged *ged)
{
    printf("Testing setting a property in a material object...\n");
    char* object_name = "TestMaterial";
    char* property_group = "physical";
    char* property_name = "density";
    char* property_value = "12.5";
    const char* argv[] = {"material", "set", object_name, property_group, property_name, property_value};
    ged_exec(ged, 6, argv);
    printf("Testing when there is not enough arguments (property group is not inputted):\n");
    const char* argv2[] = {"material", "set", object_name, property_name, property_value};
    ged_exec(ged, 5, argv2);
    printf("%s\n\n", bu_vls_cstr(ged->ged_result_str));
    printf("Testing when property group is incorrect:\n");
    char* incorrect_property_group = "opticalProperties";
    const char* argv3[] = {"material", "set", object_name, incorrect_property_group, property_name, property_value};
    ged_exec(ged, 6, argv3);
    printf("%s\n\n", bu_vls_cstr(ged->ged_result_str));
}


void
property_get_test(struct ged *ged) {
    printf("Testing retrieving a material property...\n");
    char* object_name = "TestMaterial";
    char* property_group = "physical";
    char* property_name = "density";
    const char* argv[] = {"material", "get", object_name, property_group, property_name};
    ged_exec(ged, 5, argv);
    printf("Should be 12.5: %s\n\n", bu_vls_cstr(ged->ged_result_str));
    printf("Testing when the property does not exist:\n");
    char* nonexistant_property = "meltingPoint";
    const char* argv2[] = {"material", "get", object_name, property_group, nonexistant_property};
    ged_exec(ged, 5, argv2);
    printf("%s\n\n", bu_vls_cstr(ged->ged_result_str));
}


void
material_destroy_test(struct ged *ged) {
    printf("Testing destroying a material object...\n");
    char* object_name = "TestMaterial";
    const char* argv[] = {"material", "destroy", object_name};
    ged_exec(ged, 3, argv);
    printf("ls should be empty: ");
    const char* argv2[] = {"ls"};
    ged_exec(ged, 1, argv2);
    printf("%s\n\n", bu_vls_cstr(ged->ged_result_str));
}


void
material_import_test(struct ged *ged)
{
    printf("Testing importing a density table...\n");
    char* file_name = "TEST_DENSITIES";
    const char* argv[] = {"material", "import", "--id", file_name};
    ged_exec(ged, 4, argv);
    const char* argv2[] = {"ls"};
    ged_exec(ged, 1, argv2);
    printf("%s\n", bu_vls_cstr(ged->ged_result_str));
    printf("Testing when file does not exist:\n");
    char* fake_file = "EmptyFile.txt";
    const char* argv3[] = {"material", "import", "--id", fake_file};
    ged_exec(ged, 4, argv3);
    printf("%s\n", bu_vls_cstr(ged->ged_result_str));
}


int
main(int ac, char* av[])
{
    struct ged *g;
    if (ac != 2) {
	printf("Usage: %s file.g\n", av[0]);
	return 1;
    }
    bu_setprogname(av[0]);
    g = ged_open("db", av[1], 0);

    /* FIXME: needs to actually return an error when tests fail. */
    material_creation_test(g);
    property_set_test(g);
    property_get_test(g);
    material_destroy_test(g);
    material_import_test(g);

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
