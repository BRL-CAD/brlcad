/*                          C O I L . C
 * BRL-CAD
 *
 * Copyright (c) 2009-2011 United States Government as represented by
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

/** @file shapes/coil.c
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
    struct bu_list l;
    int nt;     /*Number of Turns*/
    fastf_t od; /*Outer Diameter*/
    fastf_t wd; /*Wire Diameter*/
    fastf_t ha; /*Helix Angle*/
    fastf_t p;  /*Pitch*/
    int lhf; /*Winding Direction - 1 is Right Handed, -1 is Left Handed*/
};


fastf_t
cap_squared(struct bu_list *head, fastf_t mean_outer_diameter, fastf_t wire_diameter, fastf_t helix_angle, fastf_t pitch, fastf_t starting_pitch, int is_start, int *need_subtraction, int lhf)
{
    fastf_t pipe_bend, coil_radius;
    point_t pnt1, pnt2, pnt4, pnt6, pnt8;
     
    coil_radius = mean_outer_diameter/2 - wire_diameter/2;
    pipe_bend = coil_radius; 
    
    *need_subtraction += 0;
    
    if (is_start == 1) {
	VSET(pnt1, 0, -coil_radius, starting_pitch - sin(D2R(helix_angle))*coil_radius);
	VSET(pnt2, lhf*coil_radius , -coil_radius, starting_pitch - sin(D2R(helix_angle))*coil_radius);
    	VSET(pnt4, lhf*coil_radius , coil_radius, starting_pitch - sin(D2R(helix_angle))*coil_radius);
    	VSET(pnt6, lhf*-coil_radius , coil_radius, pitch/2+starting_pitch - sin(D2R(helix_angle))*coil_radius);
    	VSET(pnt8, lhf*-coil_radius , -coil_radius, pitch+starting_pitch - sin(D2R(helix_angle))*coil_radius);
    	mk_add_pipe_pt(head, pnt1, wire_diameter, 0.0, pipe_bend);
	mk_add_pipe_pt(head, pnt2, wire_diameter, 0.0, pipe_bend);
	mk_add_pipe_pt(head, pnt4, wire_diameter, 0.0, pipe_bend);
	mk_add_pipe_pt(head, pnt6, wire_diameter, 0.0, pipe_bend);
	mk_add_pipe_pt(head, pnt8, wire_diameter, 0.0, pipe_bend);
	return pitch + starting_pitch;
    } else {
    	VSET(pnt2, lhf*coil_radius , -coil_radius, starting_pitch + pitch/8);
	VSET(pnt4, lhf*coil_radius , coil_radius, starting_pitch + pitch*3/8 + sin(D2R(helix_angle))*coil_radius);
	VSET(pnt6, lhf*-coil_radius , coil_radius, starting_pitch + pitch + sin(D2R(helix_angle))*coil_radius);
    	VSET(pnt8, lhf*-coil_radius , -coil_radius, starting_pitch + pitch + sin(D2R(helix_angle))*coil_radius);
    	VSET(pnt1, 0 , -coil_radius, starting_pitch + pitch + sin(D2R(helix_angle))*coil_radius);
	mk_add_pipe_pt(head, pnt2, wire_diameter, 0.0, pipe_bend);
	mk_add_pipe_pt(head, pnt4, wire_diameter, 0.0, pipe_bend);
	mk_add_pipe_pt(head, pnt6, wire_diameter, 0.0, pipe_bend);
	mk_add_pipe_pt(head, pnt8, wire_diameter, 0.0, pipe_bend);
	mk_add_pipe_pt(head, pnt1, wire_diameter, 0.0, pipe_bend);
	return starting_pitch + pitch + sin(D2R(helix_angle))*coil_radius;
    }

    return 0;  
}


fastf_t
cap_squared_ground(struct rt_wdb *file, struct bu_list *head, char *prefix, struct wmember *coil_subtractions, fastf_t mean_outer_diameter, fastf_t wire_diameter, fastf_t helix_angle, fastf_t pitch, fastf_t starting_pitch, int is_start, int *need_subtraction, int lhf)
{
    fastf_t pipe_bend, coil_radius;
    point_t origin, height, pnt1, pnt2, pnt4, pnt6, pnt8;
    struct bu_vls str1;
  
    coil_radius = mean_outer_diameter/2 - wire_diameter/2;
    pipe_bend = coil_radius; 

    bu_vls_init(&str1); 

    *need_subtraction += 1; 
       
    if (is_start == 1) {
	VSET(pnt1, 0, -coil_radius, starting_pitch);
	VSET(pnt2, lhf*coil_radius , -coil_radius, starting_pitch);
	VSET(pnt4, lhf*coil_radius , coil_radius, starting_pitch);
	VSET(pnt6, lhf*-coil_radius , coil_radius, pitch/2+starting_pitch);
	VSET(pnt8, lhf*-coil_radius , -coil_radius, pitch+starting_pitch);
        mk_add_pipe_pt(head, pnt1, wire_diameter, 0.0, pipe_bend);
        mk_add_pipe_pt(head, pnt2, wire_diameter, 0.0, pipe_bend);
        mk_add_pipe_pt(head, pnt4, wire_diameter, 0.0, pipe_bend);
        mk_add_pipe_pt(head, pnt6, wire_diameter, 0.0, pipe_bend);
        mk_add_pipe_pt(head, pnt8, wire_diameter, 0.0, pipe_bend);
	VSET(origin, 0, 0, starting_pitch);
        VSET(height, 0, 0, -wire_diameter);
    	bu_vls_sprintf(&str1, "%s-startcap.s", prefix);
    	mk_rcc(file, bu_vls_addr(&str1), origin, height, coil_radius+wire_diameter+.1*wire_diameter);
        (void)mk_addmember(bu_vls_addr(&str1), &(*coil_subtractions).l, NULL, WMOP_UNION);
	bu_vls_free(&str1);
	return pitch + starting_pitch;
    } else {
   	VSET(pnt2, lhf*coil_radius , -coil_radius, starting_pitch + pitch/8);
	VSET(pnt4, lhf*coil_radius , coil_radius, starting_pitch + pitch*3/8 + sin(D2R(helix_angle))*coil_radius);
	VSET(pnt6, lhf*-coil_radius , coil_radius, starting_pitch + pitch + sin(D2R(helix_angle))*coil_radius);
    	VSET(pnt8, lhf*-coil_radius , -coil_radius, starting_pitch + pitch + sin(D2R(helix_angle))*coil_radius);
    	VSET(pnt1, 0 , -coil_radius, starting_pitch + pitch + sin(D2R(helix_angle))*coil_radius);
	mk_add_pipe_pt(head, pnt2, wire_diameter, 0.0, pipe_bend);
	mk_add_pipe_pt(head, pnt4, wire_diameter, 0.0, pipe_bend);
	mk_add_pipe_pt(head, pnt6, wire_diameter, 0.0, pipe_bend);
	mk_add_pipe_pt(head, pnt8, wire_diameter, 0.0, pipe_bend);
	mk_add_pipe_pt(head, pnt1, wire_diameter, 0.0, pipe_bend);
	VSET(origin, 0, 0, starting_pitch + pitch + sin(D2R(helix_angle))*coil_radius);
        VSET(height, 0, 0, wire_diameter);
    	bu_vls_sprintf(&str1, "%s-endcap.s", prefix);
    	mk_rcc(file, bu_vls_addr(&str1), origin, height, coil_radius+wire_diameter+.1*wire_diameter);
        (void)mk_addmember(bu_vls_addr(&str1), &(*coil_subtractions).l, NULL, WMOP_UNION);
	bu_vls_free(&str1);
	return starting_pitch + pitch + sin(D2R(helix_angle))*coil_radius;
    }
    bu_vls_free(&str1);
    return 0;  
}


fastf_t
cap_ground(struct rt_wdb *file, struct bu_list *head, char *prefix, struct wmember *coil_subtractions, fastf_t mean_outer_diameter, fastf_t wire_diameter, fastf_t helix_angle, fastf_t pitch, fastf_t starting_pitch, int is_start, int *need_subtraction, int lhf)
{
    fastf_t coil_radius, pipe_bend, center_height;
    point_t origin, height, pnt1, pnt2, pnt4, pnt6, pnt8;
    struct bu_vls str1;

    coil_radius = mean_outer_diameter/2 - wire_diameter/2;
    pipe_bend = coil_radius;

    bu_vls_init(&str1);
   
    *need_subtraction += 1;

    center_height = sqrt(((coil_radius/cos(D2R(helix_angle)))*(coil_radius/cos(D2R(helix_angle))))-coil_radius*coil_radius) + 0.25*pitch*cos(D2R(helix_angle));
    bu_log("Center_height:  %f\n", center_height);
    
    if (is_start == 1) {
	VSET(pnt1, 0, -coil_radius, starting_pitch);
	VSET(pnt2, lhf*coil_radius , -coil_radius, pitch/8 + starting_pitch + sin(D2R(helix_angle))*coil_radius);
	VSET(pnt4, lhf*coil_radius , coil_radius, pitch*3/8 + starting_pitch + sin(D2R(helix_angle))*coil_radius);
	VSET(pnt6, lhf*-coil_radius , coil_radius, pitch*5/8 + starting_pitch - sin(D2R(helix_angle))*coil_radius);
	VSET(pnt8, lhf*-coil_radius , -coil_radius, pitch*7/8 + starting_pitch - sin(D2R(helix_angle))*coil_radius);
        mk_add_pipe_pt(head, pnt1, wire_diameter, 0.0, pipe_bend);
   	mk_add_pipe_pt(head, pnt2, wire_diameter, 0.0, pipe_bend);
    	mk_add_pipe_pt(head, pnt4, wire_diameter, 0.0, pipe_bend);
    	mk_add_pipe_pt(head, pnt6, wire_diameter, 0.0, pipe_bend);
    	mk_add_pipe_pt(head, pnt8, wire_diameter, 0.0, pipe_bend);
        VSET(origin, 0, 0, 0);
        VSET(height, sin(D2R(helix_angle))*(wire_diameter+0.25*pitch), 0, -wire_diameter - 0.25*pitch);
    	bu_vls_trunc(&str1, 0);
    	bu_vls_printf(&str1, "%s-startcap.s", prefix);
    	mk_rcc(file, bu_vls_addr(&str1), origin, height, coil_radius*(1+sin(D2R(helix_angle)))+wire_diameter+.1*wire_diameter);
        (void)mk_addmember(bu_vls_addr(&str1), &(*coil_subtractions).l, NULL, WMOP_UNION);
	bu_vls_free(&str1);
        return pitch + starting_pitch;	
    } else { 
    	VSET(pnt2, lhf*coil_radius , -coil_radius, pitch/8 + starting_pitch + sin(D2R(helix_angle))*coil_radius);
    	VSET(pnt4, lhf*coil_radius , coil_radius,  pitch*3/8 + starting_pitch + sin(D2R(helix_angle))*coil_radius);
    	VSET(pnt6, lhf*-coil_radius , coil_radius, pitch*5/8 + starting_pitch - sin(D2R(helix_angle))*coil_radius);
    	VSET(pnt8, lhf*-coil_radius , -coil_radius, pitch*7/8 + starting_pitch - sin(D2R(helix_angle))*coil_radius);
	VSET(pnt1, 0 , -coil_radius, pitch+starting_pitch);
	mk_add_pipe_pt(head, pnt2, wire_diameter, 0.0, pipe_bend);
    	mk_add_pipe_pt(head, pnt4, wire_diameter, 0.0, pipe_bend);
    	mk_add_pipe_pt(head, pnt6, wire_diameter, 0.0, pipe_bend);
    	mk_add_pipe_pt(head, pnt8, wire_diameter, 0.0, pipe_bend);
	mk_add_pipe_pt(head, pnt1, wire_diameter, 0.0, pipe_bend);
        VSET(origin, 0, 0, starting_pitch + 15/8*pitch - center_height + sin(D2R(helix_angle))*coil_radius);
    	VSET(height, -sin(D2R(helix_angle))*(wire_diameter+0.25*pitch), 0, wire_diameter + 0.25*pitch);
	bu_vls_trunc(&str1, 0);
    	bu_vls_printf(&str1, "%s-endcap.s", prefix);
    	mk_rcc(file, bu_vls_addr(&str1), origin, height, coil_radius*(1+sin(D2R(helix_angle)))+wire_diameter+.1*wire_diameter);
        (void)mk_addmember(bu_vls_addr(&str1), &(*coil_subtractions).l, NULL, WMOP_UNION);
	bu_vls_free(&str1);
	return pitch+starting_pitch;
    }
    bu_vls_free(&str1);
    return 0; 
}


fastf_t
helical_coil_plain(struct bu_list *head, fastf_t mean_outer_diameter, fastf_t wire_diameter, fastf_t helix_angle, fastf_t pitch, fastf_t starting_pitch, int nt, int lhf)
{
    int i;
    fastf_t coil_radius, pipe_bend;
    point_t pnt2, pnt4, pnt6, pnt8;

    coil_radius = mean_outer_diameter/2 - wire_diameter/2;
    pipe_bend = coil_radius;

    for (i = 0; i < nt; i++) {
    	VSET(pnt2, lhf*coil_radius , -coil_radius, i*pitch + pitch/8 + starting_pitch + sin(D2R(helix_angle))*coil_radius);
    	VSET(pnt4, lhf*coil_radius , coil_radius, i*pitch + pitch*3/8 + starting_pitch + sin(D2R(helix_angle))*coil_radius);
    	VSET(pnt6, lhf*-coil_radius , coil_radius, i*pitch + pitch*5/8 + starting_pitch - sin(D2R(helix_angle))*coil_radius);
    	VSET(pnt8, lhf*-coil_radius , -coil_radius, i*pitch + pitch*7/8 + starting_pitch - sin(D2R(helix_angle))*coil_radius);
   	mk_add_pipe_pt(head, pnt2, wire_diameter, 0.0, pipe_bend);
    	mk_add_pipe_pt(head, pnt4, wire_diameter, 0.0, pipe_bend);
    	mk_add_pipe_pt(head, pnt6, wire_diameter, 0.0, pipe_bend);
    	mk_add_pipe_pt(head, pnt8, wire_diameter, 0.0, pipe_bend);
    }

    return (nt-1)*pitch + pitch*7/8 + starting_pitch;
}


void
make_coil(struct rt_wdb (*file), char *prefix, struct bu_list *sections, int start_cap_type, int end_cap_type)
{
    struct bu_list head;
    fastf_t last_pitch_pt;
    point_t pnt1;
    int need_subtractions = 0;
    struct wmember coil;
    struct wmember coil_subtractions;
	struct coil_data_t *s_data;
    struct coil_data_t *e_data;
    struct coil_data_t *cd;
    struct bu_vls str;
	
    BU_LIST_INIT(&coil.l);
    BU_LIST_INIT(&coil_subtractions.l);
    mk_pipe_init(&head);
   
    s_data = BU_LIST_FIRST(coil_data_t, &(*sections));
    e_data = BU_LIST_LAST(coil_data_t, &(*sections));
   
    bu_vls_init(&str);
   
    last_pitch_pt = 0; 
    
    switch (start_cap_type) {
	case 0:
	    VSET(pnt1, 0, -1*(s_data->od/2-s_data->wd/2), last_pitch_pt);
       	    mk_add_pipe_pt(&head, pnt1, s_data->wd, 0.0, (s_data->od/2-s_data->wd/2));
	    break;
	case 1:
	    last_pitch_pt = cap_squared(&head, s_data->od, s_data->wd, s_data->ha, s_data->p, 0, 1, &need_subtractions, s_data->lhf);
	    break;
	case 2:
	    last_pitch_pt = cap_ground(file, &head, prefix, &coil_subtractions, s_data->od, s_data->wd, s_data->ha, s_data->p, 0, 1, &need_subtractions, s_data->lhf);
	    break;
	case 3:
	    last_pitch_pt = cap_squared_ground(file, &head, prefix, &coil_subtractions, s_data->od, s_data->wd, s_data->ha, s_data->p, 0, 1, &need_subtractions, s_data->lhf);
	    break;
	default:
	    break;
    }
   
    for (BU_LIST_FOR(cd, coil_data_t, &(*sections))) {
        last_pitch_pt = helical_coil_plain(&head, cd->od, cd->wd, cd->ha, cd->p, last_pitch_pt, cd->nt, cd->lhf);
    }

    switch (end_cap_type) {
	case 0:
	    VSET(pnt1, 0 , -1*(e_data->od/2-e_data->wd/2), 0.125*(e_data->p)+last_pitch_pt);
	    mk_add_pipe_pt(&head, pnt1, e_data->wd, 0.0, (e_data->od/2-e_data->wd/2));
	    break;
	case 1:
	    last_pitch_pt = cap_squared(&head, e_data->od, e_data->wd, e_data->ha, e_data->p, last_pitch_pt, 0, &need_subtractions, s_data->lhf);
	    break;
	case 2:
	    last_pitch_pt = cap_ground(file, &head, prefix, &coil_subtractions, e_data->od, e_data->wd, e_data->ha, e_data->p, last_pitch_pt, 0, &need_subtractions, s_data->lhf);
	    break;
	case 3:
	    last_pitch_pt = cap_squared_ground(file, &head, prefix, &coil_subtractions, e_data->od, e_data->wd, e_data->ha, e_data->p, last_pitch_pt, 0, &need_subtractions, s_data->lhf);
	    break;
	default:
	    break;
    }
   
    bu_vls_trunc(&str, 0);
    bu_vls_printf(&str, "%s_core.s", prefix); 
    mk_pipe(file, bu_vls_addr(&str), &head);

    (void)mk_addmember(bu_vls_addr(&str), &coil.l, NULL, WMOP_UNION);
  
    if (need_subtractions > 0) { 
	bu_vls_trunc(&str, 0);
	bu_vls_printf(&str, "%s_subtractions.c", prefix);
	mk_lcomb(file, bu_vls_addr(&str), &coil_subtractions, 0, NULL, NULL, NULL, 0);
	(void)mk_addmember(bu_vls_addr(&str), &coil.l, NULL, WMOP_SUBTRACT);
    }

    mk_lcomb(file, prefix, &coil, 0, NULL, NULL, NULL, 0); 
   
    bu_vls_free(&str);
    mk_pipe_free(&head);
}


/* Process command line arguments */ 
int
ReadArgs(int argc, char **argv, struct bu_list *sections, fastf_t *mean_outer_diameter, fastf_t *wire_diameter, fastf_t *helix_angle, fastf_t *pitch, int *nt, int *start_cap_type, int *end_cap_type, fastf_t *overall_length, int *lhf)
{
    int c = 0;
    char *options="d:w:h:p:n:s:e:S:l:L";
    int numturns, stype, etype, lhflag;
    float mean_od, wired, h_angle, ptch, lngth;
    int d1, d6;
    float d2, d3, d4, d5;
    char s1, s2, s3, s4, s5;
   
    struct coil_data_t *coil_data;
    
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
	    case 'L':
		lhflag = -1;
		*lhf = lhflag;
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
	    case 'l':
		sscanf(bu_optarg, "%f", &lngth);
		*overall_length = lngth;
		break;
	    case 'S':
	        coil_data = (struct coil_data_t *)
		    bu_malloc(sizeof(struct coil_data_t), "coil data structure");
		sscanf(bu_optarg, "%d%c%f%c%f%c%f%c%f%c%d", &d1, &s1, &d2, &s2, &d3, &s3, &d4, &s4, &d5, &s5, &d6);
	        coil_data->nt = d1;
		coil_data->od = d2;
		coil_data->wd = d3;
		coil_data->ha = d4;
		coil_data->p = d5;
		if (d6 == 1) {
    		    coil_data->lhf = 1;
		} else {
		    coil_data->lhf = -1;
		}
		BU_LIST_INSERT(&(*sections), &((*coil_data).l));	
		break;
	    default:
		bu_log("%s: illegal option -- %c\n", bu_getprogname(), c);
		bu_exit(EXIT_SUCCESS, NULL);
	}
    }
    return bu_optind;
}


int
main(int ac, char *av[])
{
    struct rt_wdb *db_fp = NULL;
    struct bu_vls coil_type;
    struct bu_vls name;
    struct bu_vls str;
    fastf_t mean_outer_diameter, wire_diameter, overall_length, nominal_length;
    fastf_t helix_angle, pitch;

    struct coil_data_t *coil_data = NULL;
    struct bu_list sections;
    
    int nt; /* Number of turns */
    int nt_user_set = 1;
    int start_cap_type, end_cap_type;
    int lhf; /* Winding flag */

    bu_vls_init(&str);
    bu_vls_init(&coil_type);
    bu_vls_init(&name);
    bu_vls_trunc(&coil_type, 0);
    bu_vls_trunc(&name, 0);

    BU_LIST_INIT(&sections);
    
    mean_outer_diameter = 0;
    wire_diameter = 0;
    helix_angle = 0;
    pitch = 0;
    nt = 0;
    start_cap_type = 0;
    end_cap_type = 0;
    overall_length = 0;
    lhf = 1;
   
    /* Process arguments */  
    ReadArgs(ac, av, &sections, &mean_outer_diameter, &wire_diameter, &helix_angle, &pitch, &nt, &start_cap_type, &end_cap_type, &overall_length, &lhf);

    /* Handle various potential errors in args and set defaults if nothing supplied */
  
    if (BU_LIST_IS_EMPTY(&sections)) {
       
	if (mean_outer_diameter < 0 || wire_diameter < 0 || helix_angle < 0 || pitch < 0 || nt < 0 || start_cap_type < 0 || end_cap_type < 0) 
	    bu_exit(-1, " Error - negative value in one or more arguments supplied to coil");
    
        if (ZERO(wire_diameter) && ZERO(mean_outer_diameter)) {
            mean_outer_diameter = 1000;
            wire_diameter = 100;       
        }
   
        if (ZERO(wire_diameter) && mean_outer_diameter > 0) {
	    wire_diameter = mean_outer_diameter/10;
        }

        if (ZERO(mean_outer_diameter) && wire_diameter > 0) {
	    mean_outer_diameter = wire_diameter * 10;
        }

        if (ZERO(pitch)) {
	    pitch = wire_diameter;
        }

        if (pitch < wire_diameter) {
	    bu_log("Warning - pitch less than wire diameter.  Setting pitch to wire diameter: %f mm\n", wire_diameter);
	    pitch = wire_diameter;
        }
    
        if (nt == 0) {
	    nt_user_set = 0;
	    nt = 30;
	}

	coil_data = (struct coil_data_t *) bu_malloc(sizeof(struct coil_data_t), "coil data structure");
	coil_data->nt = nt;
	coil_data->od = mean_outer_diameter;
	coil_data->wd = wire_diameter;
	coil_data->ha = helix_angle;
	coil_data->p = pitch;
	coil_data->lhf = lhf;
	
        BU_LIST_APPEND(&(sections), &((*coil_data).l));	

    	coil_data = BU_LIST_FIRST(coil_data_t, &sections);
    }

    /* If hard clamping the length, have to check some things and maybe clamp some values */

    if (!ZERO(overall_length)) {
	bu_log("Caution:  Length clamping overrides other specified values - if supplied values are\ninconsistent with specified length, they will be overriden in this order:\n\nWhen Shrinking:  pitch, number of turns, wire diameter\nWhen Expanding:  number of turns, pitch\n\nCurrently, this override order is independent of whether the value is supplied by the user or calculated\ninternally - i.e. there is no preference for protecting user specified properties.\n");
	if (start_cap_type != 0 || end_cap_type != 0) {
	    bu_log("Note:  At this time only uncapped coils are allowed when length is constrained.\n");
	    start_cap_type = 0;
	    end_cap_type = 0;
	}
	if (!ZERO(helix_angle)) {
	    bu_log("Note:  At this time variable helix angles are unsupported when length is constrained.\n");
	    helix_angle = 0;
	}
	/* Thanks to earlier checks, we're guaranteed to have valid data for this calculation regardless of
	 * user input */
	nominal_length = coil_data->wd + coil_data->p * coil_data->nt;
	if (nominal_length > overall_length) {
	    /* Something has to give - start with pitch */
	    coil_data->p = (overall_length - coil_data->wd)/coil_data->nt;
	    while (coil_data->p < coil_data->wd) {
		/* That didn't work, start knocking off turns*/
		while ((coil_data->nt > 1) && (coil_data->p < coil_data->wd)) {
		   coil_data->nt--;
	    	   coil_data->p = (overall_length - coil_data->wd)/coil_data->nt;
		}
		if (coil_data->nt == 1) {
		    /* THAT didn't work, change the wire diameter */
		    coil_data->wd = overall_length/2;
		    coil_data->p = coil_data->wd ;
		}
	    }
	} else {
	    if (!EQUAL(nominal_length, overall_length)) {
		/* Add turns first, then adjust pitch */
		while (nominal_length < overall_length) {
		    coil_data->nt++;
		    nominal_length = coil_data->wd + coil_data->p * coil_data->nt;
		}
		coil_data->nt--;
		coil_data->p = (overall_length - coil_data->wd)/coil_data->nt;
	    }
	}
    }	
    
    /* Generate Name - this needs some thought for multiple section coils*/
    bu_vls_printf(&name, "coil");


    /* Create file name if supplied, else use "string.g" */
    if (av[bu_optind]) {
	if (!bu_file_exists(av[bu_optind])) {
	    db_fp = wdb_fopen(av[bu_optind]);
	} else {
	    bu_exit(-1, "Error - refusing to overwrite pre-existing file %s", av[bu_optind]);
	}
    }
    if (!av[bu_optind]) {
	if (!bu_file_exists(DEFAULT_COIL_FILENAME)) {
	    db_fp = wdb_fopen(DEFAULT_COIL_FILENAME);
	} else {
	    bu_exit(-1, "Error - no filename supplied and coil.g exists.");
	}
    }
 
    bu_log("Making coil...\n");
    make_coil(db_fp, bu_vls_addr(&name), &sections, start_cap_type, end_cap_type);

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
