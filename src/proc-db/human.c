/*                          H U M A N . C
 * BRL-CAD
 *
 * Copyright (c) 2008-2009 United States Government as represented by
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
/** @file human.c
 *
 * Generator for human models based on height
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

#define DEFAULT_HEIGHT_INCHES 68 
#define DEFAULT_FILENAME "human.g"

/**
 * Display input parameters when debugging
 */
void print_human_info(char *name)
{
    bu_log("%s\n", name);
}

/**
 * Help message printed when -h option is supplied
 */
void show_help(const char *name, const char *optstr)
{
    struct bu_vls str;
    const char *cp = optstr;

    bu_vls_init(&str);
    while (cp && *cp != '\0') {
	if (*cp == ':') {
	    cp++;
	    continue;
	}
	bu_vls_strncat(&str, cp, 1);
	cp++;
    }

    bu_log("usage: %s [%s]\n", name, bu_vls_addr(&str));
    bu_log("options:\n"
	   "\t-h\n\t\tShow help\n\n"
	);

    bu_vls_free(&str);
    return;
}

/* Process command line arguments */
int read_args(int argc, char **argv, fastf_t *standing_height, fastf_t *l_foot_bend_angle, fastf_t *l_knee_bend_angle, fastf_t *l_pelvis_bend_angle_x, fastf_t *l_pelvis_bend_angle_y, fastf_t *l_elbow_bend_angle,  fastf_t *l_shoulder_bend_angle_x, fastf_t *l_shoulder_bend_angle_y,  fastf_t *r_foot_bend_angle, fastf_t *r_knee_bend_angle, fastf_t *r_pelvis_bend_angle_x, fastf_t *r_pelvis_bend_angle_y, fastf_t *r_elbow_bend_angle,  fastf_t *r_shoulder_bend_angle_x, fastf_t *r_shoulder_bend_angle_y, fastf_t *neck_bend_angle_x, fastf_t *neck_bend_angle_y)
{
    int c = 0;
    char *options="H:f:F:k:K:p:P:e:E:s:S:n:";
    float height;

    /* don't report errors */
    bu_opterr = 0;

    while ((c=bu_getopt(argc, argv, options)) != -1) {
	switch (c) {
	    case 'H':
		sscanf(bu_optarg, "%f", &height);
		*standing_height = height;
		break;
	    default:
		bu_log("%s: illegal option -- %c\n", bu_getprogname(), c);
	    case 'h':
		show_help(*argv, options);
		bu_exit(EXIT_SUCCESS, NULL);
	}
    }
    return(bu_optind);
}


int main(int ac, char *av[])
{
    struct rt_wdb *db_fp;
    struct wmember human;
    
    struct bu_vls name;
    struct bu_vls str;
    
    fastf_t standing_height = DEFAULT_HEIGHT_INCHES;
    fastf_t l_foot_bend_angle = 0;
    fastf_t l_knee_bend_angle = 0;
    fastf_t l_pelvis_bend_angle_x = 0;
    fastf_t l_pelvis_bend_angle_y = 0;
    fastf_t l_elbow_bend_angle = 0;
    fastf_t l_shoulder_bend_angle_x = 0;
    fastf_t l_shoulder_bend_angle_y = 0;
    fastf_t r_foot_bend_angle = 0;
    fastf_t r_knee_bend_angle = 0;
    fastf_t r_pelvis_bend_angle_x = 0;
    fastf_t r_pelvis_bend_angle_y = 0;
    fastf_t r_elbow_bend_angle = 0;
    fastf_t r_shoulder_bend_angle_x = 0;
    fastf_t r_shoulder_bend_angle_y = 0;
    fastf_t neck_bend_angle_x = 0; 
    fastf_t neck_bend_angle_y = 0; 
    
    bu_vls_init(&name);
    bu_vls_trunc(&name, 0);

    bu_vls_init(&str);
    bu_vls_trunc(&str, 0);
    
    /* Process arguments */
    read_args(ac, av, &standing_height, &l_foot_bend_angle, &l_knee_bend_angle, &l_pelvis_bend_angle_x, &l_pelvis_bend_angle_y, &l_elbow_bend_angle, &l_shoulder_bend_angle_x, &l_shoulder_bend_angle_y, &r_foot_bend_angle, &r_knee_bend_angle, &r_pelvis_bend_angle_x, &r_pelvis_bend_angle_y, &r_elbow_bend_angle, &r_shoulder_bend_angle_x, &r_shoulder_bend_angle_y, &neck_bend_angle_x, &neck_bend_angle_y);

    db_fp = wdb_fopen(DEFAULT_FILENAME);
    
    /* Final top level assembly of human */
    BU_LIST_INIT(&human.l);
    bu_vls_trunc(&str, 0);
    bu_vls_printf(&str, "%s_shoulders", bu_vls_addr(&name));
    (void)mk_addmember(bu_vls_addr(&str), &human.l, NULL, WMOP_UNION);
    bu_vls_trunc(&str, 0);
    bu_vls_printf(&str, "%s_thorax", bu_vls_addr(&name));
    (void)mk_addmember(bu_vls_addr(&str), &human.l, NULL, WMOP_UNION);
    bu_vls_trunc(&str, 0);
    bu_vls_printf(&str, "%s_abdomen", bu_vls_addr(&name));
    (void)mk_addmember(bu_vls_addr(&str), &human.l, NULL, WMOP_UNION);
    bu_vls_trunc(&str, 0);
    bu_vls_printf(&str, "%s_pelvis", bu_vls_addr(&name));
    (void)mk_addmember(bu_vls_addr(&str), &human.l, NULL, WMOP_UNION);
    mk_lcomb(db_fp, bu_vls_addr(&name), &human, 0,  NULL, NULL, NULL, 0);

    /* Close database */
    wdb_close(db_fp);

    bu_vls_free(&name);
    bu_vls_free(&str);

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
