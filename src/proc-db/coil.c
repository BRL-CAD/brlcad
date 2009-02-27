/*                          C O I L . C
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

/** @file coil.c
 *
 * Coil Generator
 *
 * Program to create coils using the pipe primitive.
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
#define DEFAULT_COIL_FILENAME "coil.g"

struct coil_data_t {
    int nt;     /*Number of Turns*/
    fastf_t od; /*Outer Diameter*/
    fastf_t wd; /*Wire Diameter*/
    fastf_t ha; /*Helix Angle*/
    fastf_t p;  /*Pitch*/
};

fastf_t cap_squared(struct rt_wdb *file, struct bu_list *head, char *prefix, struct wmember *coil_subtractions, fastf_t mean_outer_diameter, fastf_t wire_diameter, fastf_t helix_angle, fastf_t pitch, fastf_t starting_pitch, int is_start, int *need_subtraction)
{
    fastf_t pipe_bend, coil_radius;
    point_t pnt1, pnt2, pnt4, pnt6, pnt8;
     
    coil_radius = mean_outer_diameter/2 - wire_diameter/2;
    pipe_bend = coil_radius; 
    
    *need_subtraction += 0;
    
    if (is_start == 1) {
	VSET(pnt1, 0, -coil_radius, starting_pitch - sin(D2R(helix_angle))*coil_radius);
	VSET(pnt2, coil_radius , -coil_radius, starting_pitch - sin(D2R(helix_angle))*coil_radius);
    	VSET(pnt4, coil_radius , coil_radius, starting_pitch - sin(D2R(helix_angle))*coil_radius);
    	VSET(pnt6, -coil_radius , coil_radius, pitch/2+starting_pitch - sin(D2R(helix_angle))*coil_radius);
    	VSET(pnt8, -coil_radius , -coil_radius, pitch+starting_pitch - sin(D2R(helix_angle))*coil_radius);
    	mk_add_pipe_pt(head, pnt1, wire_diameter, 0.0, pipe_bend);
	mk_add_pipe_pt(head, pnt2, wire_diameter, 0.0, pipe_bend);
	mk_add_pipe_pt(head, pnt4, wire_diameter, 0.0, pipe_bend);
	mk_add_pipe_pt(head, pnt6, wire_diameter, 0.0, pipe_bend);
	mk_add_pipe_pt(head, pnt8, wire_diameter, 0.0, pipe_bend);
	return pitch + starting_pitch;
    } else {
	VSET(pnt2, coil_radius, -coil_radius, starting_pitch + sin(D2R(helix_angle))*coil_radius);
	VSET(pnt4, coil_radius , coil_radius, pitch/2 + starting_pitch + sin(D2R(helix_angle))*coil_radius);
    	VSET(pnt6, -coil_radius , coil_radius, pitch + starting_pitch + sin(D2R(helix_angle))*coil_radius);
    	VSET(pnt8, -coil_radius , -coil_radius, pitch + starting_pitch + sin(D2R(helix_angle))*coil_radius);
    	VSET(pnt1, 0 , -coil_radius, pitch + starting_pitch + sin(D2R(helix_angle))*coil_radius);
	mk_add_pipe_pt(head, pnt2, wire_diameter, 0.0, pipe_bend);
	mk_add_pipe_pt(head, pnt4, wire_diameter, 0.0, pipe_bend);
	mk_add_pipe_pt(head, pnt6, wire_diameter, 0.0, pipe_bend);
	mk_add_pipe_pt(head, pnt8, wire_diameter, 0.0, pipe_bend);
	mk_add_pipe_pt(head, pnt1, wire_diameter, 0.0, pipe_bend);
	return pitch + starting_pitch;
    }

    return 0;  
}

fastf_t cap_squared_ground(struct rt_wdb *file, struct bu_list *head, char *prefix, struct wmember *coil_subtractions, fastf_t mean_outer_diameter, fastf_t wire_diameter, fastf_t helix_angle, fastf_t pitch, fastf_t starting_pitch, int is_start, int *need_subtraction)
{
    fastf_t pipe_bend, coil_radius;
    point_t origin, height, pnt1, pnt2, pnt4, pnt6, pnt8, pnt10;
    struct bu_vls str;
  
    bu_vls_init(&str); 
    
    coil_radius = mean_outer_diameter/2 - wire_diameter/2;
    pipe_bend = coil_radius;

    *need_subtraction += 1; 
       
    if (is_start == 1) {
	VSET(pnt1, -coil_radius, 0, starting_pitch - sin(D2R(helix_angle))*coil_radius - pitch/4);
	VSET(pnt10, -coil_radius, -coil_radius, starting_pitch - sin(D2R(helix_angle))*coil_radius);
	VSET(pnt2, coil_radius , -coil_radius, starting_pitch - sin(D2R(helix_angle))*coil_radius);
    	VSET(pnt4, coil_radius , coil_radius, starting_pitch - sin(D2R(helix_angle))*coil_radius);
    	VSET(pnt6, -coil_radius , coil_radius, pitch/2+starting_pitch - sin(D2R(helix_angle))*coil_radius);
    	VSET(pnt8, -coil_radius , -coil_radius, pitch+starting_pitch - sin(D2R(helix_angle))*coil_radius);
    	mk_add_pipe_pt(head, pnt1, wire_diameter, 0.0, pipe_bend);
    	mk_add_pipe_pt(head, pnt10, wire_diameter, 0.0, pipe_bend);
	mk_add_pipe_pt(head, pnt2, wire_diameter, 0.0, pipe_bend);
	mk_add_pipe_pt(head, pnt4, wire_diameter, 0.0, pipe_bend);
	mk_add_pipe_pt(head, pnt6, wire_diameter, 0.0, pipe_bend);
	mk_add_pipe_pt(head, pnt8, wire_diameter, 0.0, pipe_bend);
        VSET(origin, 0, 0, starting_pitch - sin(D2R(helix_angle))*coil_radius);
        VSET(height, 0, 0, -wire_diameter);
    	bu_vls_trunc(&str, 0);
    	bu_vls_printf(&str, "%s-startcap.s", prefix);
    	mk_rcc(file, bu_vls_addr(&str), origin, height, coil_radius+wire_diameter+.1*wire_diameter);
        (void)mk_addmember(bu_vls_addr(&str), &(*coil_subtractions).l, NULL, WMOP_UNION);
	bu_vls_free(&str);
	return pitch + starting_pitch;
    } else {
	VSET(pnt2, coil_radius, -coil_radius, starting_pitch + sin(D2R(helix_angle))*coil_radius);
	VSET(pnt4, coil_radius , coil_radius, pitch/2 + starting_pitch + sin(D2R(helix_angle))*coil_radius);
    	VSET(pnt6, -coil_radius , coil_radius, pitch + starting_pitch + sin(D2R(helix_angle))*coil_radius);
    	VSET(pnt8, -coil_radius , -coil_radius, pitch + starting_pitch + sin(D2R(helix_angle))*coil_radius);
	VSET(pnt10, coil_radius , -coil_radius, pitch + starting_pitch + sin(D2R(helix_angle))*coil_radius);
    	VSET(pnt1, coil_radius , 0, pitch + starting_pitch + sin(D2R(helix_angle))*coil_radius + pitch/4);
	mk_add_pipe_pt(head, pnt2, wire_diameter, 0.0, pipe_bend);
	mk_add_pipe_pt(head, pnt4, wire_diameter, 0.0, pipe_bend);
	mk_add_pipe_pt(head, pnt6, wire_diameter, 0.0, pipe_bend);
	mk_add_pipe_pt(head, pnt8, wire_diameter, 0.0, pipe_bend);
	mk_add_pipe_pt(head, pnt10, wire_diameter, 0.0, pipe_bend);
	mk_add_pipe_pt(head, pnt1, wire_diameter, 0.0, pipe_bend);
	VSET(origin, 0, 0, starting_pitch + pitch + sin(D2R(helix_angle))*coil_radius);
        VSET(height, 0, 0, wire_diameter);
    	bu_vls_trunc(&str, 0);
    	bu_vls_printf(&str, "%s-endcap.s", prefix);
    	mk_rcc(file, bu_vls_addr(&str), origin, height, coil_radius+wire_diameter+.1*wire_diameter);
        (void)mk_addmember(bu_vls_addr(&str), &(*coil_subtractions).l, NULL, WMOP_UNION);
	bu_vls_free(&str);
	return pitch + starting_pitch;
    }
    bu_vls_free(&str);
    return 0;  
}

fastf_t cap_ground(struct rt_wdb *file, struct bu_list *head, char *prefix, struct wmember *coil_subtractions, fastf_t mean_outer_diameter, fastf_t wire_diameter, fastf_t helix_angle, fastf_t pitch, fastf_t starting_pitch, int is_start, int *need_subtraction)
{
    fastf_t coil_radius, pipe_bend;
    point_t origin, height, pnt1, pnt2, pnt4, pnt6, pnt8;

    coil_radius = mean_outer_diameter/2 - wire_diameter/2;
    pipe_bend = coil_radius;

    struct bu_vls str;
    bu_vls_init(&str);
   
    *need_subtraction += 1;
    
    if (is_start == 1) {
	VSET(pnt1, 0, -coil_radius, starting_pitch);
	VSET(pnt2, coil_radius , -coil_radius, pitch/8 + starting_pitch + sin(D2R(helix_angle))*coil_radius);
	VSET(pnt4, coil_radius , coil_radius, pitch*3/8 + starting_pitch + sin(D2R(helix_angle))*coil_radius);
	VSET(pnt6, -coil_radius , coil_radius, pitch*5/8 + starting_pitch - sin(D2R(helix_angle))*coil_radius);
	VSET(pnt8, -coil_radius , -coil_radius, pitch*7/8 + starting_pitch - sin(D2R(helix_angle))*coil_radius);
        mk_add_pipe_pt(head, pnt1, wire_diameter, 0.0, pipe_bend);
   	mk_add_pipe_pt(head, pnt2, wire_diameter, 0.0, pipe_bend);
    	mk_add_pipe_pt(head, pnt4, wire_diameter, 0.0, pipe_bend);
    	mk_add_pipe_pt(head, pnt6, wire_diameter, 0.0, pipe_bend);
    	mk_add_pipe_pt(head, pnt8, wire_diameter, 0.0, pipe_bend);
        VSET(origin, 0, 0, starting_pitch);
        VSET(height, 0, 0, -wire_diameter - sin(D2R(helix_angle))*coil_radius);
    	bu_vls_trunc(&str, 0);
    	bu_vls_printf(&str, "%s-startcap.s", prefix);
    	mk_rcc(file, bu_vls_addr(&str), origin, height, coil_radius+wire_diameter+.1*wire_diameter);
        (void)mk_addmember(bu_vls_addr(&str), &(*coil_subtractions).l, NULL, WMOP_UNION);
	bu_vls_free(&str);
        return pitch + starting_pitch;	
    } else { 
    	VSET(pnt2, coil_radius , -coil_radius, pitch/8 + starting_pitch + sin(D2R(helix_angle))*coil_radius);
    	VSET(pnt4, coil_radius , coil_radius,  pitch*3/8 + starting_pitch + sin(D2R(helix_angle))*coil_radius);
    	VSET(pnt6, -coil_radius , coil_radius, pitch*5/8 + starting_pitch - sin(D2R(helix_angle))*coil_radius);
    	VSET(pnt8, -coil_radius , -coil_radius, pitch*7/8 + starting_pitch - sin(D2R(helix_angle))*coil_radius);
	VSET(pnt1, 0 , -coil_radius, pitch+starting_pitch);
	mk_add_pipe_pt(head, pnt2, wire_diameter, 0.0, pipe_bend);
    	mk_add_pipe_pt(head, pnt4, wire_diameter, 0.0, pipe_bend);
    	mk_add_pipe_pt(head, pnt6, wire_diameter, 0.0, pipe_bend);
    	mk_add_pipe_pt(head, pnt8, wire_diameter, 0.0, pipe_bend);
	mk_add_pipe_pt(head, pnt1, wire_diameter, 0.0, pipe_bend);
       	VSET(origin, 0, 0, starting_pitch + pitch);
        VSET(height, 0, 0, wire_diameter + sin(D2R(helix_angle))*coil_radius);
    	bu_vls_trunc(&str, 0);
    	bu_vls_printf(&str, "%s-endcap.s", prefix);
    	mk_rcc(file, bu_vls_addr(&str), origin, height, coil_radius+wire_diameter+.1*wire_diameter);
        (void)mk_addmember(bu_vls_addr(&str), &(*coil_subtractions).l, NULL, WMOP_UNION);
	bu_vls_free(&str);
	return pitch+starting_pitch;
    }
    bu_vls_free(&str);
    return 0; 
}



fastf_t helical_coil_plain(struct bu_list *head, struct wmember *coil, fastf_t mean_outer_diameter, fastf_t wire_diameter, fastf_t helix_angle, fastf_t pitch, fastf_t starting_pitch, int nt, int do_1st_pt, int do_last_pt)
{
    int i;
    fastf_t coil_radius, pipe_bend;
    point_t pnt1, pnt2, pnt4, pnt6, pnt8;

    coil_radius = mean_outer_diameter/2 - wire_diameter/2;
    pipe_bend = coil_radius;

    /* If requested, take care of first point */
    if (do_1st_pt == 0) {
       VSET(pnt1, 0, -coil_radius, starting_pitch);
       mk_add_pipe_pt(head, pnt1, wire_diameter, 0.0, pipe_bend);
    }

    /* Now, do the coils needed for the section */ 
    for (i = 0; i < nt; i++) {
    	VSET(pnt2, coil_radius , -coil_radius, i*pitch + pitch/8 + starting_pitch + sin(D2R(helix_angle))*coil_radius);
    	VSET(pnt4, coil_radius , coil_radius, i*pitch + pitch*3/8 + starting_pitch + sin(D2R(helix_angle))*coil_radius);
    	VSET(pnt6, -coil_radius , coil_radius, i*pitch + pitch*5/8 + starting_pitch - sin(D2R(helix_angle))*coil_radius);
    	VSET(pnt8, -coil_radius , -coil_radius, i*pitch + pitch*7/8 + starting_pitch - sin(D2R(helix_angle))*coil_radius);
   	mk_add_pipe_pt(head, pnt2, wire_diameter, 0.0, pipe_bend);
    	mk_add_pipe_pt(head, pnt4, wire_diameter, 0.0, pipe_bend);
    	mk_add_pipe_pt(head, pnt6, wire_diameter, 0.0, pipe_bend);
    	mk_add_pipe_pt(head, pnt8, wire_diameter, 0.0, pipe_bend);
    }

    /* If requested, take care of last point */
    if (do_last_pt == 0) {
	VSET(pnt1, 0 , -coil_radius, nt*pitch+starting_pitch);
	mk_add_pipe_pt(head, pnt1, wire_diameter, 0.0, pipe_bend);
	return nt*pitch+starting_pitch;
    } else {
	return (nt-1)*pitch + pitch*7/8 + starting_pitch;
    }

}

void make_coil(struct rt_wdb (*file), char *prefix, fastf_t mean_outer_diameter, fastf_t wire_diameter, fastf_t helix_angle, fastf_t pitch, int nt, int start_cap_type, int end_cap_type)
{
    struct bu_list head;
    mk_pipe_init(&head);
    fastf_t last_pitch_pt;
    int need_subtractions = 0;
    struct wmember coil;
    BU_LIST_INIT(&coil.l);
    struct wmember coil_subtractions;
    BU_LIST_INIT(&coil_subtractions.l);
    
    struct bu_vls str;
    bu_vls_init(&str);
   
    last_pitch_pt = 0; 
    
    switch (start_cap_type) {
	case 1:
	    last_pitch_pt = cap_squared(file, &head, prefix, &coil_subtractions, mean_outer_diameter, wire_diameter, helix_angle, pitch, 0, 1, &need_subtractions);
	    break;
	case 2:
	    last_pitch_pt = cap_ground(file, &head, prefix, &coil_subtractions, mean_outer_diameter, wire_diameter, helix_angle, pitch, 0, 1, &need_subtractions);
	    break;
	case 3:
	    last_pitch_pt = cap_squared_ground(file, &head, prefix, &coil_subtractions, mean_outer_diameter, wire_diameter, helix_angle, pitch, 0, 1, &need_subtractions);
	    break;
	default:
	    break;
    }

    last_pitch_pt = helical_coil_plain(&head, &coil_subtractions, mean_outer_diameter, wire_diameter, helix_angle, pitch, last_pitch_pt, nt, start_cap_type, end_cap_type);

    switch (end_cap_type) {
	case 1:
	    last_pitch_pt = cap_squared(file, &head, prefix, &coil_subtractions, mean_outer_diameter, wire_diameter, helix_angle, pitch, last_pitch_pt, 0, &need_subtractions);
	    break;
	case 2:
	    last_pitch_pt = cap_ground(file, &head, prefix, &coil_subtractions, mean_outer_diameter, wire_diameter, helix_angle, pitch, last_pitch_pt, 0, &need_subtractions);
	    break;
	case 3:
	    last_pitch_pt = cap_squared_ground(file, &head, prefix, &coil_subtractions, mean_outer_diameter, wire_diameter, helix_angle, pitch, last_pitch_pt, 0, &need_subtractions);
	    break;
	default:
	    break;
    }
   
    bu_vls_trunc(&str,0);
    bu_vls_printf(&str, "%s_core.s", prefix); 
    mk_pipe(file, bu_vls_addr(&str), &head);

    (void)mk_addmember(bu_vls_addr(&str), &coil.l, NULL, WMOP_UNION);
  
    if (need_subtractions > 0) { 
       bu_vls_trunc(&str,0);
       bu_vls_printf(&str, "%s_subtractions.c", prefix);
       mk_lcomb(file, bu_vls_addr(&str), &coil_subtractions, 0, NULL, NULL, NULL, 0);
       (void)mk_addmember(bu_vls_addr(&str), &coil.l, NULL, WMOP_SUBTRACT);
    }

    mk_lcomb(file, prefix, &coil, 0, NULL, NULL, NULL, 0); 
   
    bu_vls_free(&str);
    mk_pipe_free(&head);
} 



/* Process command line arguments */ 
int ReadArgs(int argc, char **argv, fastf_t *mean_outer_diameter, fastf_t *wire_diameter, fastf_t *helix_angle, fastf_t *pitch, int *nt, int *start_cap_type, int *end_cap_type)
{
    int c = 0;
    char *options="d:w:h:p:n:s:e:";
    int numturns, stype, etype;
    float mean_od, wired, h_angle, ptch;

    bu_opterr = 0;

    while ((c=bu_getopt(argc, argv, options)) != -1) {
	switch (c) {
	    case 'd' :
		sscanf(bu_optarg, "%f", &mean_od);
		*mean_outer_diameter = mean_od;
		break;
	    case 'w':
		sscanf(bu_optarg, "%f", &wired);
		*wire_diameter = wired;
		break;
	    case 'h':
		sscanf(bu_optarg, "%f", &h_angle);
		*helix_angle = h_angle;
		break;
	    case 'p':
		sscanf(bu_optarg, "%f", &ptch);
		*pitch = ptch;
		break;
	    case 'n':
		sscanf(bu_optarg, "%d", &numturns);
		*nt = numturns;
		break;
	    case 's':
		sscanf(bu_optarg, "%d", &stype);
		*start_cap_type = stype;
		break;
	    case 'e':
		sscanf(bu_optarg, "%d", &etype);
		*end_cap_type = etype;
		break;
	    default:
		bu_log("%s: illegal option -- %c\n", bu_getprogname(), c);
		bu_exit(EXIT_SUCCESS, NULL);
	}
    }
    return(bu_optind);
}


int main(int ac, char *av[])
{
    struct rt_wdb *db_fp;
    struct bu_vls coil_type;
    struct bu_vls name;
    struct bu_vls str;
    fastf_t mean_outer_diameter, wire_diameter;
    fastf_t helix_angle, pitch;

    int nt; /* Number of turns */
    int start_cap_type, end_cap_type;
    
    struct coil_data_t *coil_data = (struct coil_data_t *)
    	bu_malloc( sizeof(struct coil_data_t), "coil data structure");
	
    bu_vls_init(&str);
    bu_vls_init(&coil_type);
    bu_vls_init(&name);
    bu_vls_trunc(&coil_type, 0);
    bu_vls_trunc(&name, 0);

    mean_outer_diameter = 0;
    wire_diameter = 0;
    helix_angle = 0;
    pitch = 0;
    nt = 0;
    start_cap_type = 0;
    end_cap_type = 0;

   
    /* Process arguments */  
    ReadArgs(ac, av, &mean_outer_diameter, &wire_diameter, &helix_angle, &pitch, &nt, &start_cap_type, &end_cap_type);

    /* Handle various potential errors in args and set defaults if nothing supplied */
  
    if (mean_outer_diameter < 0 || wire_diameter < 0 || helix_angle < 0 || pitch < 0 || nt < 0 || start_cap_type < 0 || end_cap_type < 0) 
	bu_exit(-1," Error - negative value in one or more arguments supplied to coil");
    
    if (wire_diameter == 0 && mean_outer_diameter == 0) {
       mean_outer_diameter = 1000;
       wire_diameter = 100;       
    }
   
    if (wire_diameter == 0 && mean_outer_diameter > 0) {
	wire_diameter = mean_outer_diameter/10;
    }

    if (mean_outer_diameter == 0 && wire_diameter > 0) {
	mean_outer_diameter = wire_diameter * 10;
    }

    if (pitch == 0) {
	pitch = wire_diameter;
    }

    if (pitch < wire_diameter) {
	bu_log("Warning - pitch less than wire diameter.  Setting pitch to wire diameter: %f mm\n", wire_diameter);
	pitch = wire_diameter;
    }
    
    if (nt == 0) nt = 30;

    bu_log("Outer Diameter: %f\n",mean_outer_diameter);
    bu_log("Wire Diameter: %f\n",wire_diameter);
    
    /* Generate Name */
    bu_vls_printf(&coil_type, "hc");
    bu_vls_printf(&name, "coil_%s_%.1f_%.1f_%.1f_%.1f_%d", bu_vls_addr(&coil_type), mean_outer_diameter, wire_diameter, helix_angle, pitch, nt);
 
    
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
	    bu_exit(-1,"Error - no filename supplied and coil.g exists.");
	}
    }
 
    bu_log("Making coil...\n");
    make_coil(db_fp, bu_vls_addr(&name), mean_outer_diameter, wire_diameter, helix_angle, pitch, nt, start_cap_type, end_cap_type);

    bu_free(coil_data, "coil_data");
    bu_vls_free(&str);
    bu_vls_free(&name);
    
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
