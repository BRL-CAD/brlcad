/*                          S P R I N G . C
 * BRL-CAD
 *
 * Copyright (c) 2009 United States Government as represented by
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

/** @file spring.c
 *
 * Spring Generator
 *
 * Program to create springs using the pipe primitive.
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
#define DEFAULT_LENS_FILENAME "spring.g"

void MakeSpring(struct rt_wdb (*file), char *prefix, fastf_t mean_outer_diameter, fastf_t wire_diameter, fastf_t helix_angle, fastf_t pitch, int nt, int end_type)
{
    struct bu_list head;
    int i;
    fastf_t pipe_bend;
    point_t origin, pnt1, pnt2, pnt3, pnt4;

    VSET(origin, 0, 0 ,0);

    pipe_bend = mean_outer_diameter/2;
    mk_pipe_init(&head);
    
    VSET(pnt1, mean_outer_diameter/2 , 0, 0);
    VSET(pnt2, mean_outer_diameter, 0, pitch/4);
    VSET(pnt3, mean_outer_diameter, mean_outer_diameter, pitch/2);
    VSET(pnt4, 0, mean_outer_diameter, pitch*3/4);
    mk_add_pipe_pt(&head, pnt1, wire_diameter, 0.0, pipe_bend);
    mk_add_pipe_pt(&head, pnt2, wire_diameter, 0.0, pipe_bend);
    mk_add_pipe_pt(&head, pnt3, wire_diameter, 0.0, pipe_bend);
    mk_add_pipe_pt(&head, pnt4, wire_diameter, 0.0, pipe_bend);

    for (i = 1; i < nt - 1; i++) {
    	VSET(pnt1, 0 , 0, i*pitch);
    	VSET(pnt2, mean_outer_diameter, 0, i*pitch+pitch/4);
    	VSET(pnt3, mean_outer_diameter, mean_outer_diameter, i*pitch + pitch/2);
    	VSET(pnt4, 0, mean_outer_diameter, i*pitch + pitch*3/4);
    	mk_add_pipe_pt(&head, pnt1, wire_diameter, 0.0, pipe_bend);
   	mk_add_pipe_pt(&head, pnt2, wire_diameter, 0.0, pipe_bend);
    	mk_add_pipe_pt(&head, pnt3, wire_diameter, 0.0, pipe_bend);
    	mk_add_pipe_pt(&head, pnt4, wire_diameter, 0.0, pipe_bend);
    }

    VSET(pnt1, 0 , 0, (nt-1)*pitch);
    VSET(pnt2, mean_outer_diameter, 0, (nt-1)*pitch + pitch/4);
    VSET(pnt3, mean_outer_diameter, mean_outer_diameter, (nt-1)*pitch + pitch/2);
    VSET(pnt4, mean_outer_diameter/2, mean_outer_diameter, (nt-1)*pitch + pitch*3/4);
    mk_add_pipe_pt(&head, pnt1, wire_diameter, 0.0, pipe_bend);
    mk_add_pipe_pt(&head, pnt2, wire_diameter, 0.0, pipe_bend);
    mk_add_pipe_pt(&head, pnt3, wire_diameter, 0.0, pipe_bend);
    mk_add_pipe_pt(&head, pnt4, wire_diameter, 0.0, pipe_bend);


    mk_pipe(file, prefix, &head);

    mk_pipe_free(&head);
}

/* Process command line arguments 
int ReadArgs(int argc, char **argv, int *lens_1side_2side, fastf_t *ref_ind, fastf_t *diameter, fastf_t *thickness, fastf_t *focal_length)
{
    int c = 0;
    char *options="T:r:d:t:f:";
    int ltype;
    float refractive, diam, thick, focal;

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
    return(bu_optind);
}
*/


int main(int ac, char *av[])
{
    struct rt_wdb *db_fp;
    struct bu_vls spring_type;
    struct bu_vls name;
    struct bu_vls str;
    fastf_t mean_outer_diameter, wire_diameter;
    fastf_t helix_angle, pitch;
    int nt; /* Number of turns */
    fastf_t spring_index;
    int end_type;
    
    bu_vls_init(&str);
    bu_vls_init(&spring_type);
    bu_vls_init(&name);
    bu_vls_trunc(&spring_type, 0);
    bu_vls_trunc(&name, 0);

    mean_outer_diameter = 2000;
    wire_diameter = 250;
    spring_index = mean_outer_diameter/wire_diameter; 
    helix_angle = 10;
    pitch = 300;
    nt = 60;
    end_type = 0;    

    bu_vls_printf(&spring_type, "hc");
    bu_vls_printf(&name, "spring_%s_%.1f_%.1f_%.1f_%.1f_%d", bu_vls_addr(&spring_type), mean_outer_diameter, spring_index, helix_angle, pitch, nt);
    
    /* Process arguments  
    ReadArgs(ac, av, &lens_1side_2side, &ref_ind, &diameter, &thickness, &focal_length);
	*/
    /* Create file name if supplied, else use "string.g" */
    if (av[bu_optind]) {
	if (!bu_file_exists(av[bu_optind])) {
	    db_fp = wdb_fopen( av[bu_optind] );
	} else {
	    bu_exit(-1,"Error - refusing to overwrite pre-existing file %s",av[bu_optind]);
	}
    }
    if (!av[bu_optind]) {
	if (!bu_file_exists(DEFAULT_LENS_FILENAME)) {
	    db_fp = wdb_fopen(DEFAULT_LENS_FILENAME);
	} else {
	    bu_exit(-1,"Error - no filename supplied and lens.g exists.");
	}
    }
 
    bu_log("Making spring...\n");
    MakeSpring(db_fp, bu_vls_addr(&name), mean_outer_diameter, wire_diameter, helix_angle, pitch, nt, end_type);

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
