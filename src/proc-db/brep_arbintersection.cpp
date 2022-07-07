/*                 B R E P _ S I M P L E . C P P
 * BRL-CAD
 *
 * Copyright (c) 2004-2022 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file proc-db/brep_arbintersection.cpp
 *
 *  Creates some brep objects using arb8 interserction
 * 
 *  All test datas are defined in brep_arbintersection.h
 */

#include "common.h"

#include "twistedcube.h"

#include "bio.h"

#include "vmath.h"		/* BRL-CAD Vector macros */

#include "bu/app.h"
#include "wdb.h"
#include "brep.h"
#include "brep_arbintersection.h"
#include "rt/geom.h"
#include <string>

struct rt_wdb* outfp;
const char* db_name = "brep_arbintersection.g";

static void
printusage(void) {
    fprintf(stderr,"Usage: brep_arbintersection (takes no arguments)\n");
}

int
com_add(
    struct rt_wdb* wdbp,
    const char* combname,
    int size,
    std::string* names,
    const char* shadername,
    const char* shaderargs,
    const unsigned char* rgb)
{
    struct bu_list head;
    BU_LIST_INIT(&head);
    for (int i = 0; i < size; i++)
    {
        if (mk_addmember(names[i].data(), &head, NULL, WMOP_UNION) == WMEMBER_NULL)
            return -2;
    }
    return mk_comb(wdbp, combname, &head, 1, shadername, shaderargs,
        rgb, 0, 0, 0, 0, 0, 0, 0);
}

int test_UNION(rt_wdb* out, int arb_num, rt_arb_internal** arbs, ON_3dPoint points[][8], char* namehead)
{
    std::string head = namehead;
    std::string* names = new std::string[arb_num];for (int i = 0; i < arb_num; i++) {
        BU_ALLOC(arbs[i], struct rt_arb_internal);
        arbs[i]->magic = RT_ARB_INTERNAL_MAGIC;
        for (int j = 0; j < 8; j++) {
            VMOVE(arbs[i]->pt[j], points[i][j]);
        }
        std::string str= std::to_string(i);
        names[i] = head + std::to_string(i);
        wdb_export(out, names[i].data(), (void*)arbs[i], ID_ARB8, mk_conv2mm);
    }


    unsigned char rgb[] = { 255, 255, 255 };
    std::string com_name = (head + ".r");
    com_add(out, com_name.data(), arb_num, names, "plastic", "", rgb);

    ON_Brep* brep = ON_Brep::New();
    struct rt_db_internal brep_db_internal;

    struct db_i* dbip = db_open(db_name, DB_OPEN_READONLY);
    if (!dbip) {
        bu_exit(1, "Unable to open brep_arbintersection.g geometry database file\n");
    }
    db_dirbuild(dbip);
    struct directory* dirp;
    if ((dirp = db_lookup(dbip, com_name.data(), 0)) != RT_DIR_NULL) {
        struct rt_db_internal ip;
        mat_t mat;
        MAT_IDN(mat);
        if (rt_db_get_internal(&ip, dirp, dbip, mat, &rt_uniresource) >= 0) {

        }
        else {
            fprintf(stderr, "problem getting internal object rep\n");
        }

        struct rt_db_internal intern_res;
        std::string brep_name = ("brep." + head);


        int ret = brep_conversion(&ip, &brep_db_internal, dbip);
        bu_log("ret = %d.\n", ret);
        if (ret == -1) {
            bu_log("-1.\n");
        }
        else if (ret == -2) {
            bu_log("-2.\n");
        }
        else {
            brep = ((struct rt_brep_internal*)brep_db_internal.idb_ptr)->brep;
            ret = mk_brep(outfp, brep_name.data(), (void*)brep);
        }

        rt_db_free_internal(&brep_db_internal);
        return BRLCAD_OK;
    }
}

int
main(int argc, char** argv)
{
    ON_TextLog error_log;
    const char* id_name = "B-Rep Example";
    const char* geom_name = "cube.s";

    bu_setprogname(argv[0]);

    if (BU_STR_EQUAL(argv[1], "-h") || BU_STR_EQUAL(argv[1], "-?")) {
    	printusage();
    	return 0;
    }
    if (argc > 1) {
    	printusage();
	return 1;
    }

    ON::Begin();

    outfp = wdb_fopen(db_name);
    mk_id(outfp, id_name);

    test_UNION(outfp, arb_1_num, arb_1, arb_1_v, "arb_1");
    test_UNION(outfp, arb_2_num, arb_2, arb_2_v, "arb_2");
    test_UNION(outfp, arb_3_num, arb_3, arb_3_v, "arb_3");
    test_UNION(outfp, arb_4_num, arb_4, arb_4_v, "arb_4");
    test_UNION(outfp, arb_5_num, arb_5, arb_5_v, "arb_5");

    wdb_close(outfp);
    


    ON::End();

    return 0;
}


/*
 * Local Variables:
 * tab-width: 8
 * mode: C++
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
