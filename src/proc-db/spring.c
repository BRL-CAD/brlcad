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
#define DEFAULT_COIL_FILENAME "spring.g"

void add_cap(){
}

fastf_t cap_squared(struct bu_list *head, struct wmember *spring, fastf_t mean_outer_diameter, fastf_t wire_diameter, fastf_t helix_angle, fastf_t pitch, fastf_t starting_pitch, int is_start)
{
    fastf_t pipe_bend, coil_radius;
    point_t origin, height, pnt1, pnt2, pnt4, pnt6, pnt8;
    struct bu_vls str;
  
    bu_vls_init(&str); 
    
    coil_radius = mean_outer_diameter/2 - wire_diameter/2;
    pipe_bend = coil_radius; 
       
    if (is_start == 1) {
	VSET(pnt1, 0, -coil_radius, starting_pitch);
	VSET(pnt2, coil_radius , -coil_radius, starting_pitch);
    	VSET(pnt4, coil_radius , coil_radius, starting_pitch);
    	VSET(pnt6, -coil_radius , coil_radius, pitch/2+starting_pitch);
    	VSET(pnt8, -coil_radius , -coil_radius, pitch+starting_pitch);
    	mk_add_pipe_pt(head, pnt1, wire_diameter, 0.0, pipe_bend);
	mk_add_pipe_pt(head, pnt2, wire_diameter, 0.0, pipe_bend);
	mk_add_pipe_pt(head, pnt4, wire_diameter, 0.0, pipe_bend);
	mk_add_pipe_pt(head, pnt6, wire_diameter, 0.0, pipe_bend);
	mk_add_pipe_pt(head, pnt8, wire_diameter, 0.0, pipe_bend);
	return pitch + starting_pitch;
    } else {
	VSET(pnt2, coil_radius, -coil_radius, starting_pitch);
	VSET(pnt4, coil_radius , coil_radius, pitch/2 + starting_pitch);
    	VSET(pnt6, -coil_radius , coil_radius, pitch + starting_pitch);
    	VSET(pnt8, -coil_radius , -coil_radius, pitch + starting_pitch);
    	VSET(pnt1, 0 , -coil_radius, pitch + starting_pitch);
	mk_add_pipe_pt(head, pnt2, wire_diameter, 0.0, pipe_bend);
	mk_add_pipe_pt(head, pnt4, wire_diameter, 0.0, pipe_bend);
	mk_add_pipe_pt(head, pnt6, wire_diameter, 0.0, pipe_bend);
	mk_add_pipe_pt(head, pnt8, wire_diameter, 0.0, pipe_bend);
	mk_add_pipe_pt(head, pnt1, wire_diameter, 0.0, pipe_bend);
	return pitch + starting_pitch;
    }

    return 0;  
/*
    VSET(origin, 0, 0, 0);
    VSET(height, 0, 0, -wire_diameter-.1*wire_diameter);
    bu_vls_trunc(&str, 0);
    bu_vls_printf(&str, "%s-sub1.s", prefix);
    mk_rcc(file, bu_vls_addr(&str), origin, height,  mean_outer_diameter/2+wire_diameter/2+.1*wire_diameter);

    (void)mk_addmember(bu_vls_addr(&str), &spring.l, NULL, WMOP_SUBTRACT);
    
    VSET(origin, mean_outer_diameter/2, mean_outer_diameter/2, nt*pitch-wire_diameter/2);
    VSET(height, 0, 0, wire_diameter+.1*wire_diameter);
    bu_vls_trunc(&str, 0);
    bu_vls_printf(&str, "%s-sub2.s", prefix);
    mk_rcc(file, bu_vls_addr(&str), origin, height,  mean_outer_diameter/2+wire_diameter/2+.1*wire_diameter);

    (void)mk_addmember(bu_vls_addr(&str), &spring.l, NULL, WMOP_SUBTRACT);

    bu_vls_trunc(&str, 0);
    bu_vls_printf(&str, "%s.c", prefix);
    mk_lcomb(file, bu_vls_addr(&str), &spring, 0, NULL, NULL, NULL, 0); */
}



fastf_t helical_compression_coil_plain(struct bu_list *head, struct wmember *spring, fastf_t mean_outer_diameter, fastf_t wire_diameter, fastf_t helix_angle, fastf_t pitch, fastf_t starting_pitch, int nt, int do_1st_pt, int do_last_pt)
{
    int i;
    fastf_t coil_radius, pipe_bend;
    point_t pnt1, pnt2, pnt4, pnt6, pnt8, endpt;

    coil_radius = mean_outer_diameter/2 - wire_diameter/2;
    pipe_bend = coil_radius;

    /* If requested, take care of first point */
    if (do_1st_pt != 0) {
       VSET(pnt1, 0, -coil_radius, i*pitch+starting_pitch);
       mk_add_pipe_pt(head, pnt1, wire_diameter, 0.0, pipe_bend);
    }

    /* Now, do the coils needed for the section */ 
    for (i = 0; i < nt; i++) {
    	VSET(pnt2, coil_radius , -coil_radius, i*pitch + pitch/8 + starting_pitch);
    	VSET(pnt4, coil_radius , coil_radius, i*pitch + pitch*3/8 + starting_pitch);
    	VSET(pnt6, -coil_radius , coil_radius, i*pitch + pitch*5/8 + starting_pitch);
    	VSET(pnt8, -coil_radius , -coil_radius, i*pitch + pitch*7/8 + starting_pitch);
   	mk_add_pipe_pt(head, pnt2, wire_diameter, 0.0, pipe_bend);
    	mk_add_pipe_pt(head, pnt4, wire_diameter, 0.0, pipe_bend);
    	mk_add_pipe_pt(head, pnt6, wire_diameter, 0.0, pipe_bend);
    	mk_add_pipe_pt(head, pnt8, wire_diameter, 0.0, pipe_bend);
    }

    /* If requested, take care of last point */
    if (do_last_pt != 0) {
	VSET(pnt1, 0 , -coil_radius, nt*pitch+starting_pitch);
	mk_add_pipe_pt(head, pnt1, wire_diameter, 0.0, pipe_bend);
	return nt*pitch+starting_pitch;
    } else {
	return (nt-1)*pitch + pitch*7/8 + starting_pitch;
    }

}

void make_coil(struct rt_wdb (*file), char *prefix, fastf_t mean_outer_diameter, fastf_t wire_diameter, fastf_t helix_angle, fastf_t pitch, int nt, int end_type)
{
    struct bu_list head;
    mk_pipe_init(&head);
    fastf_t last_pitch_pt;

    struct wmember spring_components;
    BU_LIST_INIT(&spring_components.l);
  
    last_pitch_pt = 0; 
    last_pitch_pt = cap_squared(&head, &spring_components, mean_outer_diameter, wire_diameter, helix_angle, pitch, 0, 1); 
    last_pitch_pt = helical_compression_coil_plain(&head, &spring_components, mean_outer_diameter, wire_diameter, helix_angle, pitch, last_pitch_pt, nt, 0, 0);
    last_pitch_pt = cap_squared(&head, &spring_components, mean_outer_diameter, wire_diameter, helix_angle, pitch, last_pitch_pt, 0);  
    
    mk_pipe(file, prefix, &head);

    (void)mk_addmember(prefix, &spring_components.l, NULL, WMOP_UNION);

    
    
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

    mean_outer_diameter = 1000;
    wire_diameter = 100;
    spring_index = mean_outer_diameter/wire_diameter; 
    helix_angle = 10;
    pitch = wire_diameter;
    nt = 2;
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
	if (!bu_file_exists(DEFAULT_COIL_FILENAME)) {
	    db_fp = wdb_fopen(DEFAULT_COIL_FILENAME);
	} else {
	    bu_exit(-1,"Error - no filename supplied and spring.g exists.");
	}
    }
 
    bu_log("Making spring...\n");
    make_coil(db_fp, bu_vls_addr(&name), mean_outer_diameter, wire_diameter, helix_angle, pitch, nt, end_type);

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
