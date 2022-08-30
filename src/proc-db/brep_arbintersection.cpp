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

void
pointShift(point_t* point, ON_3dPoint shift) {
    (*point)[0] += shift.x;
    (*point)[1] += shift.y;
    (*point)[2] += shift.z;
}

int
create_arb8_matrix()
{
    // caculate vertex positions of arb8s
    struct ON_3dPoint shift(0, 0, 0);   //each test case have a 4*4*4 space
    double shift_space = 4.0;
    for (int i = 0; i < 5; i++) {
        shift.x = i * shift_space;
        for (int j = 0; j < 5; j++) {
            shift.y = j * shift_space;
            for (int k = 0; k < 5; k++) {
                shift.z = k * shift_space;
                int case_id = i * 25 + j * 5 + k;
                rt_arb_internal* arb_0;
                rt_arb_internal* arb_1;
                BU_ALLOC(arb_0, struct rt_arb_internal);
                BU_ALLOC(arb_1, struct rt_arb_internal);
                arb_0->magic = RT_ARB_INTERNAL_MAGIC;
                arb_1->magic = RT_ARB_INTERNAL_MAGIC;
                arb_1->pt[0][0] = arb_1->pt[1][0] = arb_1->pt[4][0] = arb_1->pt[5][0] = v_arb_pos[i][0];
                arb_1->pt[2][0] = arb_1->pt[3][0] = arb_1->pt[6][0] = arb_1->pt[7][0] = v_arb_pos[i][1];
                arb_1->pt[0][1] = arb_1->pt[3][1] = arb_1->pt[4][1] = arb_1->pt[7][1] = v_arb_pos[j][0];
                arb_1->pt[1][1] = arb_1->pt[2][1] = arb_1->pt[5][1] = arb_1->pt[6][1] = v_arb_pos[j][1];
                arb_1->pt[0][2] = arb_1->pt[1][2] = arb_1->pt[2][2] = arb_1->pt[3][2] = v_arb_pos[k][0];
                arb_1->pt[4][2] = arb_1->pt[5][2] = arb_1->pt[6][2] = arb_1->pt[7][2] = v_arb_pos[k][1];

                for (int t = 0; t < 8; t++) {
                    VMOVE(arb_0->pt[t], ps_arb_0[t]);
                    pointShift(&(arb_0->pt[t]), shift);
                    pointShift(&(arb_1->pt[t]), shift);
                }

                // create unions
                std::string name_0 = "arb_" + std::to_string(i) + "_" + std::to_string(j) + "_" + std::to_string(k) + "_0";
                std::string name_1 = "arb_" + std::to_string(i) + "_" + std::to_string(j) + "_" + std::to_string(k) + "_1";
                std::string name_inter = "inter_" + std::to_string(i) + "_" + std::to_string(j) + "_" + std::to_string(k) + ".r";
                std::string name_sub = "sub_" + std::to_string(i) + "_" + std::to_string(j) + "_" + std::to_string(k) + ".r";
                std::string name_un = "un_" + std::to_string(i) + "_" + std::to_string(j) + "_" + std::to_string(k) + ".r";
                wdb_export(outfp, name_0.data(), (void*)arb_0, ID_ARB8, mk_conv2mm);
                wdb_export(outfp, name_1.data(), (void*)arb_1, ID_ARB8, mk_conv2mm);

                // three boolean operations: intersection, subtraction, and union
                struct bu_list inter;
                struct bu_list sub;
                struct bu_list un;
                BU_LIST_INIT(&inter);
                BU_LIST_INIT(&sub);
                BU_LIST_INIT(&un);

                if (mk_addmember(name_0.data(), &inter, NULL, WMOP_UNION) == WMEMBER_NULL)
                    return -2;
                if (mk_addmember(name_1.data(), &inter, NULL, WMOP_INTERSECT) == WMEMBER_NULL)
                    return -2;

                if (mk_addmember(name_0.data(), &sub, NULL, WMOP_UNION) == WMEMBER_NULL)
                    return -2;
                if (mk_addmember(name_1.data(), &sub, NULL, WMOP_SUBTRACT) == WMEMBER_NULL)
                    return -2;

                if (mk_addmember(name_0.data(), &un, NULL, WMOP_UNION) == WMEMBER_NULL)
                    return -2;
                if (mk_addmember(name_1.data(), &un, NULL, WMOP_UNION) == WMEMBER_NULL)
                    return -2;

                unsigned char rgb[] = { 255, 255, 255 };
                mk_comb(outfp, name_inter.data(), &inter, 1, "plastic", "", rgb, 0, 0, 0, 0, 0, 0, 0);
                mk_comb(outfp, name_sub.data(), &sub, 1, "plastic", "", rgb, 0, 0, 0, 0, 0, 0, 0);
                mk_comb(outfp, name_un.data(), &un, 1, "plastic", "", rgb, 0, 0, 0, 0, 0, 0, 0);
            }
        }
    }

    struct db_i* dbip = db_open(db_name, DB_OPEN_READONLY);
    if (!dbip) {
        bu_exit(1, "Unable to open brep_arbintersection.g geometry database file\n");
    }
    db_dirbuild(dbip);
    for (int i = 0; i < 125; i++) {
        ON_Brep* brep = ON_Brep::New();
        struct rt_db_internal brep_db_internal;
        std::string names[3] = {
            std::string("inter_" + std::to_string(i / 25) + "_" + std::to_string(i % 25 / 5) + "_" + std::to_string(i % 5) + ".r").data(),
            std::string("sub_" + std::to_string(i / 25) + "_" + std::to_string(i % 25 / 5) + "_" + std::to_string(i % 5) + ".r").data(),
            std::string("un_" + std::to_string(i / 25) + "_" + std::to_string(i % 25 / 5) + "_" + std::to_string(i % 5) + ".r").data()
        };

        struct directory* dirp;

        for (auto str : names) {

            if ((dirp = db_lookup(dbip, str.data(), 0)) != RT_DIR_NULL) {
                struct rt_db_internal ip;
                mat_t mat;
                MAT_IDN(mat);
                if (rt_db_get_internal(&ip, dirp, dbip, mat, &rt_uniresource) >= 0) {

                }

                struct rt_db_internal intern_res;

                std::string brep_name = ("brep.." + std::to_string(i / 25) + "_" + std::to_string(i % 25 / 5) + "_" + std::to_string(i % 5));
                brep_name.insert(5, 1, str.data()[0]);

                int ret = brep_conversion(&ip, &brep_db_internal, dbip);
                if (!ret && ret != -2) {
                    brep = ((struct rt_brep_internal*)brep_db_internal.idb_ptr)->brep;
                    ret = mk_brep(outfp, brep_name.data(), (void*)brep);
                }

                rt_db_free_internal(&brep_db_internal);
            }
        }
    }
}

int
create_sph_matrix(float sph0_r = 2.0f, float sph1_r = 1.0f)
{
    caculate_ell_pos(sph0_r, sph1_r);
    // caculate vertex positions of arb8s
    struct ON_3dPoint shift(0, 0, 0);   //each test case have a 4*4*4 space
    float shift_space = (sph0_r + sph1_r) * 2;
    std::cout << shift_space << std::endl;
    for (int i = 0; i < 5; i++) {
        shift.x = i * shift_space;
        for (int j = 0; j < 5; j++) {
            shift.y = j * shift_space;
            for (int k = 0; k < 5; k++) {
                shift.z = k * shift_space;
                int case_id = i * 25 + j * 5 + k;
                rt_ell_internal* ell_0;
                rt_ell_internal* ell_1;
                BU_ALLOC(ell_0, struct rt_ell_internal);
                BU_ALLOC(ell_1, struct rt_ell_internal);

                ell_0->magic = RT_ELL_INTERNAL_MAGIC;
                ell_1->magic = RT_ELL_INTERNAL_MAGIC;
                VSET(ell_0->v, shift[X], shift[Y], shift[Z]);
                VSET(ell_0->a, sph0_r, 0.0, 0.0);	/* A */
                VSET(ell_0->b, 0.0, sph0_r, 0.0);	/* B */
                VSET(ell_0->c, 0.0, 0.0, sph0_r);	/* C */

                VSET(ell_1->v, shift[X] + v_ell_pos[i], shift[Y] + v_ell_pos[j], shift[Z] + v_ell_pos[k]);
                VSET(ell_1->a, sph1_r, 0.0, 0.0);	/* A */
                VSET(ell_1->b, 0.0, sph1_r, 0.0);	/* B */
                VSET(ell_1->c, 0.0, 0.0, sph1_r);	/* C */

                // create unions
                std::string name_0 = "ell_" + std::to_string(i) + "_" + std::to_string(j) + "_" + std::to_string(k) + "_0";
                std::string name_1 = "ell_" + std::to_string(i) + "_" + std::to_string(j) + "_" + std::to_string(k) + "_1";
                std::string name_inter = "inter_" + std::to_string(i) + "_" + std::to_string(j) + "_" + std::to_string(k) + ".r";
                std::string name_sub = "sub_" + std::to_string(i) + "_" + std::to_string(j) + "_" + std::to_string(k) + ".r";
                std::string name_un = "un_" + std::to_string(i) + "_" + std::to_string(j) + "_" + std::to_string(k) + ".r";
                wdb_export(outfp, name_0.data(), (void*)ell_0, ID_ELL, mk_conv2mm);
                wdb_export(outfp, name_1.data(), (void*)ell_1, ID_ELL, mk_conv2mm);

                // three boolean operations: intersection, subtraction, and union
                struct bu_list inter;
                struct bu_list sub;
                struct bu_list un;
                BU_LIST_INIT(&inter);
                BU_LIST_INIT(&sub);
                BU_LIST_INIT(&un);

                if (mk_addmember(name_0.data(), &inter, NULL, WMOP_UNION) == WMEMBER_NULL)
                    return -2;
                if (mk_addmember(name_1.data(), &inter, NULL, WMOP_INTERSECT) == WMEMBER_NULL)
                    return -2;

                if (mk_addmember(name_0.data(), &sub, NULL, WMOP_UNION) == WMEMBER_NULL)
                    return -2;
                if (mk_addmember(name_1.data(), &sub, NULL, WMOP_SUBTRACT) == WMEMBER_NULL)
                    return -2;

                if (mk_addmember(name_0.data(), &un, NULL, WMOP_UNION) == WMEMBER_NULL)
                    return -2;
                if (mk_addmember(name_1.data(), &un, NULL, WMOP_UNION) == WMEMBER_NULL)
                    return -2;

                unsigned char rgb[] = { 255, 255, 255 };
                mk_comb(outfp, name_inter.data(), &inter, 1, "plastic", "", rgb, 0, 0, 0, 0, 0, 0, 0);
                mk_comb(outfp, name_sub.data(), &sub, 1, "plastic", "", rgb, 0, 0, 0, 0, 0, 0, 0);
                mk_comb(outfp, name_un.data(), &un, 1, "plastic", "", rgb, 0, 0, 0, 0, 0, 0, 0);
            }
        }
    }

    struct db_i* dbip = db_open(db_name, DB_OPEN_READONLY);
    if (!dbip) {
        bu_exit(1, "Unable to open brep_arbintersection.g geometry database file\n");
    }
    db_dirbuild(dbip);
    for (int i = 0; i < 125; i++) {
        ON_Brep* brep = ON_Brep::New();
        struct rt_db_internal brep_db_internal;
        std::string names[3] = {
            std::string("inter_" + std::to_string(i / 25) + "_" + std::to_string(i % 25 / 5) + "_" + std::to_string(i % 5) + ".r").data(),
            std::string("sub_" + std::to_string(i / 25) + "_" + std::to_string(i % 25 / 5) + "_" + std::to_string(i % 5) + ".r").data(),
            std::string("un_" + std::to_string(i / 25) + "_" + std::to_string(i % 25 / 5) + "_" + std::to_string(i % 5) + ".r").data()
        };

        struct directory* dirp;

        for (auto str : names) {

            if ((dirp = db_lookup(dbip, str.data(), 0)) != RT_DIR_NULL) {
                struct rt_db_internal ip;
                mat_t mat;
                MAT_IDN(mat);
                if (rt_db_get_internal(&ip, dirp, dbip, mat, &rt_uniresource) >= 0) {

                }

                struct rt_db_internal intern_res;

                std::string brep_name = ("brep.." + std::to_string(i / 25) + "_" + std::to_string(i % 25 / 5) + "_" + std::to_string(i % 5));
                brep_name.insert(5, 1, str.data()[0]);

                int ret = brep_conversion(&ip, &brep_db_internal, dbip);
                if (!ret && ret != -2) {
                    brep = ((struct rt_brep_internal*)brep_db_internal.idb_ptr)->brep;
                    ret = mk_brep(outfp, brep_name.data(), (void*)brep);
                }

                rt_db_free_internal(&brep_db_internal);
            }
        }
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
    create_sph_matrix();

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
