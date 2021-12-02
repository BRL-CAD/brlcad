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
    const char* argv[] = {"material", "create", object_name, material_name};
    ged_exec(ged, 4, argv);
}

void
property_set_test(){
   
}

void
property_get_test(){
}

void
material_destroy_test(){
}

void
material_import_test(){

}

int
main() {
    struct ged g;
    ged_init(&g);

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