/*                          L E N S . C
 * BRL-CAD
 *
 * Copyright (c) 2008-2011 United States Government as represented by
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

/** @file proc-db/lens.c
 *
 * Lens Generator
 *
 * Program to create basic lenses.
 *
 * In lens making, there are 4 basic types of lenses that result
 * from the two basic surfaces (concave and convex):
 *
 * Plano-Convex (PCX)
 * Plano-Concave (PCV)
 * BiConvex or Double Convex (DCX)
 * BiConcave or Double Concave (DCV)
 *
 * This program takes the lensmaker's equation:
 *
 *  
 *               1            d (n - 1)   1    1
 *               - = (n - 1) (--------- - -- + --)
 *               f             n R1 R2    R2   R1
 *
 * and uses it to deduce the physical geometry needed
 * to represent simple lenses based on the inputs:
 *
 * Type (P or D), Diameter of lens,
 * focal length (+ for convex, - for concave),
 * n - the refractive index of the lens material, 
 * and d - the thickness of the lens
 * at the center the lens along the optical axis).
 * The latter two are set to the refractive index of
 * soda-lime glass (1.5) and to 1/5 of the diameter
 * of the lens if not specified.
 *
 *
 */

#include "common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "bu.h"
#include "vmath.h"
#include "bn.h"
#include "raytrace.h"
#include "wdb.h"

#define D2R(x) (x * DEG2RAD)
#define R2D(x) (x / DEG2RAD)
#define DEFAULT_LENS_FILENAME "lens.g"

void MakeP(struct rt_wdb (*file), char *prefix, fastf_t diameter, fastf_t focal_length, fastf_t ref_ind, fastf_t thickness)
{
    struct wmember lensglass, lens;
    struct bu_vls str;
    fastf_t sph_R, epa_H, epa_R, rcc_h;
    int lens_type;
    point_t origin;
    vect_t height;
    vect_t breadth;

    if (focal_length > 0) {
    	lens_type = 1;
    } else {
	lens_type = -1;
    }

    bu_vls_init(&str);


    sph_R = lens_type*focal_length*(ref_ind - 1);
    bu_log("sph_R = %f\n", sph_R);
    epa_R = diameter / 2;
    bu_log("epa_R = %f\n", epa_R);
    epa_H = sph_R - sqrt(sph_R*sph_R-epa_R*epa_R);
    bu_log("epa_H = %f\n", epa_H);
    rcc_h = thickness - lens_type * epa_H;
    bu_log("rcc_h = %f\n", rcc_h);

    BU_LIST_INIT(&lensglass.l);
    BU_LIST_INIT(&lens.l);

    if (epa_R > 0 && epa_H > 0) {
	if (rcc_h < 0) bu_log("Warning - specified thickness too thin for lens\n");

	if (rcc_h >= 0) {
	    VSET(origin, 0, 0, 0);
	    VSET(height, 0, -rcc_h, 0);
	    bu_vls_trunc(&str, 0);
	    bu_vls_printf(&str, "%s-cyl.s", prefix);
	    mk_rcc(file, bu_vls_addr(&str), origin, height, diameter/2);
	    (void)mk_addmember(bu_vls_addr(&str), &lensglass.l, NULL, WMOP_UNION);
	}

	VSET(origin, 0, -rcc_h, 0);
	VSET(height, 0, -1*lens_type*epa_H, 0);
	VSET(breadth, 0, 0, 1);
	bu_vls_trunc(&str, 0);
	bu_vls_printf(&str, "%s-epa.s", prefix);
	mk_epa(file, bu_vls_addr(&str), origin, height, breadth, epa_R, epa_R); 
	if (lens_type == 1) {       
	    (void)mk_addmember(bu_vls_addr(&str), &lensglass.l, NULL, WMOP_UNION);
	} else {
	    (void)mk_addmember(bu_vls_addr(&str), &lensglass.l, NULL, WMOP_SUBTRACT);
	}

	bu_vls_trunc(&str, 0);
	bu_vls_printf(&str, "%s.c", prefix);
	mk_lcomb(file, bu_vls_addr(&str), &lensglass, 0,  NULL, NULL, NULL, 0);
       
	(void)mk_addmember(bu_vls_addr(&str), &lens.l, NULL, WMOP_UNION);
	bu_vls_trunc(&str, 0);
	bu_vls_printf(&str, "%s.r", prefix);
	mk_lcomb(file, bu_vls_addr(&str), &lens, 1, "glass", "ri=1.5", NULL, 0);
    } else {
	bu_log("Error - specified parameters result in non-physical geometry");
    }
}

void MakeD(struct rt_wdb (*file), char *prefix, fastf_t diameter, fastf_t focal_length, fastf_t ref_ind, fastf_t thickness)
{
    struct wmember lensglass, lens;
    struct bu_vls str;
    fastf_t sph_R, epa_H, epa_R, rcc_h;
    int lens_type;
    point_t origin;
    vect_t height;
    vect_t breadth;

    bu_vls_init(&str);

    if (focal_length > 0) {
    	lens_type = 1;
    } else {
	lens_type = -1;
    }


    sph_R = ((ref_ind - 1) * sqrt(focal_length * lens_type * focal_length * lens_type * ref_ind * ref_ind - thickness * focal_length * lens_type * ref_ind) + focal_length * lens_type * ref_ind * ref_ind - focal_length * lens_type * ref_ind)/ref_ind;
    bu_log("sph_R = %f\n", sph_R);
    epa_R = diameter / 2;
    bu_log("epa_R = %f\n", epa_R);
    epa_H = sph_R - sqrt(sph_R*sph_R-epa_R*epa_R);
    bu_log("epa_H = %f\n", epa_H);
    rcc_h = thickness - 2 * lens_type * epa_H;
    bu_log("rcc_h = %f\n", rcc_h);

    BU_LIST_INIT(&lensglass.l);
    BU_LIST_INIT(&lens.l);

    if (epa_R > 0 && epa_H > 0) {
	if (rcc_h < 0) bu_log("Warning - specified thickness too thin for lens\n");

	if (rcc_h >= 0) {
	    VSET(origin, 0, -rcc_h/2, 0);
	    VSET(height, 0, rcc_h, 0);
	    bu_vls_trunc(&str, 0);
	    bu_vls_printf(&str, "%s-cyl.s", prefix);
	    mk_rcc(file, bu_vls_addr(&str), origin, height, diameter/2);
	    (void)mk_addmember(bu_vls_addr(&str), &lensglass.l, NULL, WMOP_UNION);
	}

	VSET(origin, 0, -rcc_h/2, 0);
	VSET(height, 0, -1 * lens_type * epa_H, 0);
	VSET(breadth, 0, 0, 1);
	bu_vls_trunc(&str, 0);
	bu_vls_printf(&str, "%s-epa1.s", prefix);
	mk_epa(file, bu_vls_addr(&str), origin, height, breadth, epa_R, epa_R); 
	if (lens_type == 1) {
	    (void)mk_addmember(bu_vls_addr(&str), &lensglass.l, NULL, WMOP_UNION);
	} else {
	    (void)mk_addmember(bu_vls_addr(&str), &lensglass.l, NULL, WMOP_SUBTRACT);
	}
	VSET(origin, 0, rcc_h/2, 0);
	VSET(height, 0, lens_type * epa_H, 0);
	VSET(breadth, 0, 0, 1);
	bu_vls_trunc(&str, 0);
	bu_vls_printf(&str, "%s-epa2.s", prefix);
	mk_epa(file, bu_vls_addr(&str), origin, height, breadth, epa_R, epa_R); 
	if (lens_type == 1) {
	    (void)mk_addmember(bu_vls_addr(&str), &lensglass.l, NULL, WMOP_UNION);
	} else {
	    (void)mk_addmember(bu_vls_addr(&str), &lensglass.l, NULL, WMOP_SUBTRACT);
	}
       
	bu_vls_trunc(&str, 0);
	bu_vls_printf(&str, "%s.c", prefix);
	mk_lcomb(file, bu_vls_addr(&str), &lensglass, 0,  NULL, NULL, NULL, 0);
       
	(void)mk_addmember(bu_vls_addr(&str), &lens.l, NULL, WMOP_UNION);
	bu_vls_trunc(&str, 0);
	bu_vls_printf(&str, "%s.r", prefix);
	mk_lcomb(file, bu_vls_addr(&str), &lens, 1, "glass", "ri=1.5", NULL, 0);
    } else {
	bu_log("Error - specified parameters result in non-physical geometry");
    }
}

/* Process command line arguments */
int ReadArgs(int argc, char **argv, int *lens_1side_2side, fastf_t *ref_ind, fastf_t *diameter, fastf_t *thickness, fastf_t *focal_length)
{
    int c = 0;
    char *options="T:r:d:t:f:";
    int ltype;
    float refractive, diam, thick, focal;

    /* don't report errors */
    bu_opterr = 0;

    while ((c=bu_getopt(argc, argv, options)) != -1) {
	switch (c) {
	    case 'T' :
		sscanf(bu_optarg, "%d", &ltype);
		*lens_1side_2side = ltype;
		break;
	    case 'r':
		sscanf(bu_optarg, "%f", &refractive);
		*ref_ind = refractive;
		break;
	    case 'd':
		sscanf(bu_optarg, "%f", &diam);
		*diameter = diam;
		break;
	    case 't':
		sscanf(bu_optarg, "%f", &thick);
		*thickness = thick;
		break;
	    case 'f':
		sscanf(bu_optarg, "%f", &focal);
		*focal_length = focal;
		break;
	    default:
		bu_log("%s: illegal option -- %c\n", bu_getprogname(), c);
		bu_exit(EXIT_SUCCESS, NULL);
	}
    }
    return bu_optind;
}



int main(int ac, char *av[])
{
    struct rt_wdb *db_fp = NULL;
    struct bu_vls lens_type;
    struct bu_vls name;
    struct bu_vls str;
    int lens_1side_2side;
    fastf_t ref_ind, thickness, diameter, focal_length;

    bu_vls_init(&str);
    bu_vls_init(&lens_type);
    bu_vls_init(&name);
    bu_vls_trunc(&lens_type, 0);
    bu_vls_trunc(&name, 0);

    ref_ind = 1.5;
    diameter = 200;
    thickness = diameter/5;
    focal_length = 600;
    lens_1side_2side = 2;
    bu_vls_printf(&lens_type, "DCX");
    bu_vls_printf(&name, "lens_%s_f%.1f_d%.1f", bu_vls_addr(&lens_type), focal_length, diameter);
    
    /* Process arguments */ 
    ReadArgs(ac, av, &lens_1side_2side, &ref_ind, &diameter, &thickness, &focal_length);

    /* Create file name if supplied, else use "lens.g" */
    if (av[bu_optind]) {
	if (!bu_file_exists(av[bu_optind])) {
	    db_fp = wdb_fopen(av[bu_optind]);
	} else {
	    bu_exit(-1, "Error - refusing to overwrite pre-existing file %s", av[bu_optind]);
	}
    }
    if (!av[bu_optind]) {
	if (!bu_file_exists(DEFAULT_LENS_FILENAME)) {
	    db_fp = wdb_fopen(DEFAULT_LENS_FILENAME);
	} else {
	    bu_exit(-1, "Error - no filename supplied and lens.g exists.");
	}
    }
    /* Make the requested lens*/
    if (lens_1side_2side == 1 && focal_length > 0) {
	bu_log("Making Plano-Convex lens...\n");
	bu_vls_trunc(&lens_type, 0);
	bu_vls_trunc(&name, 0);
	bu_vls_printf(&lens_type, "PCX");
	bu_vls_printf(&name, "lens_%s_f%.1f_d%.1f", bu_vls_addr(&lens_type), focal_length, diameter);
	MakeP(db_fp, bu_vls_addr(&name), diameter, focal_length, ref_ind, thickness);
    }
    if (lens_1side_2side == 1 && focal_length < 0) { 
	bu_log("Making Plano-Concave lens...\n");
	bu_vls_trunc(&lens_type, 0);
	bu_vls_trunc(&name, 0);
	bu_vls_printf(&lens_type, "PCV");
	bu_vls_printf(&name, "lens_%s_f%.1f_d%.1f", bu_vls_addr(&lens_type), focal_length, diameter);
	MakeP(db_fp, bu_vls_addr(&name), diameter, focal_length, ref_ind, thickness);
    }

    if (lens_1side_2side == 2 && focal_length > 0) {
	bu_log("Making BiConvex lens...\n");
	bu_vls_trunc(&lens_type, 0);
	bu_vls_trunc(&name, 0);
	bu_vls_printf(&lens_type, "DCX");
	bu_vls_printf(&name, "lens_%s_f%.1f_d%.1f", bu_vls_addr(&lens_type), focal_length, diameter);
	MakeD(db_fp, bu_vls_addr(&name), diameter, focal_length, ref_ind, thickness);
    }
    if (lens_1side_2side == 2 && focal_length < 0) {
	bu_log("Making BiConcave lens...\n");
	bu_vls_trunc(&lens_type, 0);
	bu_vls_trunc(&name, 0);
	bu_vls_printf(&lens_type, "DCV");
	bu_vls_printf(&name, "lens_%s_f%.1f_d%.1f", bu_vls_addr(&lens_type), focal_length, diameter);
	MakeD(db_fp, bu_vls_addr(&name), diameter, focal_length, ref_ind, thickness);
    }

    /* Close database */
    wdb_close(db_fp);

    return 0;
}


/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
