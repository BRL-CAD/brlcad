/*                A N I M _ H A R D T R A C K . C
 * BRL-CAD
 *
 * Copyright (c) 1993-2007 United States Government as represented by
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
 *
 */
/** @file anim_hardtrack.c
 *  Animate the links and wheels of a tracked vehicle. It is assumed
 *  that the wheels do not translate with respect to the vehicle.
 *
 *  Author -
 *	Carl J. Nuzman
 *
 *  Source -
 *      The U. S. Army Research Laboratory
 *      Aberdeen Proving Ground, Maryland  21005-5068  USA
 */

#include "common.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <math.h>
#ifdef HAVE_STRING_H
#  include <string.h>
#else
#  include <strings.h>
#endif

#include "machine.h"
#include "vmath.h"
#include "bu.h"
#include "bn.h"
#include "anim.h"


#define NW	num_wheels
#define NEXT(i)	(i+1)%NW
#define PREV(i)	(i+NW-1)%NW

#define TRACK_ANIM	0
#define TRACK_ARCED	1

typedef double *pdouble;

struct wheel {
    vect_t		pos;	/* displacement of wheel from vehicle origin */
    fastf_t		rad;	/* radius of wheel */
    fastf_t		ang0;	/* angle where track meets wheel; 0 < a < 2pi */
    fastf_t		ang1;	/* angle where track leaves wheel; 0 < a < 2pi */
    fastf_t		arc;	/* arclength of contact between wheel and track */
};

struct track {
    vect_t		pos0;	/* beginning point of track section */
    vect_t		pos1;	/* end point of track section */
    vect_t		dir;	/* unit vector: direction of track section */
    fastf_t		len;	/* length of track section */
};

struct slope {
    vect_t		dir;	/* vector from previous to current axle */
    fastf_t		len;	/* length of vector described above */
};

struct all {
    struct wheel 	w;	/* parameters describing the track around a wheel */
    struct track	t;	/* track between this wheel and the previous wheel */
    struct slope	s;	/* vector between this axle and the previous axle */
};

struct rlink {
    vect_t		pos;	/* reverse of initial position */
    fastf_t		ang;	/* initial angle */
};

/* external variables */
extern int bu_optind;
extern char *bu_optarg;

/* variables describing track geometry - used by main, trackprep, get_link */
struct all *x;
struct rlink *r;		/* reverse of initial locations of links */
int num_links, num_wheels;
fastf_t track_y, tracklen;

/* variables set by get_args */
int wheel_nindex;	/* argv[wheel_nindex] = wheelname*/
int link_nindex;	/* argv[link_nindex] = linkname*/
int print_wheel;	/* flag: do wheel animation */
int print_link;		/* flag: do link animation */
int print_mode;		/*  anim for rt or arced for mged */
int arced_frame;	/* which frame to arced */
int links_placed;	/* flag: links are initially on the track */
int axes, cent;		/* flags: alternate axes, centroid specified */
int steer;		/* flag: vehicle automatically steered */
int first_frame;	/* integer with which to begin numbering frames */
fastf_t radius;		/* common radius of all wheels */
fastf_t init_dist; 	/* initial distance of first link along track */
vect_t centroid, rcentroid;	/* alternate centroid and its reverse */
mat_t m_axes, m_rev_axes;	/* matrices to and from alternate axes */
char link_cmd[10];		/* default is "rarc" */
char wheel_cmd[10];		/* default is "lmul" */
int get_circumf;	/* flag: just return circumference of track */

int get_link(fastf_t *pos, fastf_t *angle_p, fastf_t dist);

int
main(int argc, char **argv)
{
    void anim_dir2mat(fastf_t *, const fastf_t *, const fastf_t *), anim_y_p_r2mat(fastf_t *, double, double, double), anim_add_trans(fastf_t *, const fastf_t *, const fastf_t *),anim_mat_print(FILE *, const fastf_t *, int);
    int get_args(int argc, char **argv), track_prep(void), val, frame, go, i, count;
    fastf_t y_rot, distance, yaw, pitch, roll;
    vect_t p1, p2, p3, dir, dir2, wheel_now, wheel_prev;
    vect_t zero, position, vdelta, temp, to_track, to_front;
    mat_t mat_v, wmat, mat_x;
    FILE *stream;
    int last_frame;

    VSETALL(zero,0.0);
    VSETALL(to_track,0.0);
    VSETALL(centroid,0.0);
    VSETALL(rcentroid,0.0);
    init_dist = y_rot = radius= 0.0;
    first_frame = num_wheels = steer = axes = cent = links_placed=0;
    num_wheels = num_links = last_frame = 0;
    MAT_IDN(mat_v);
    MAT_IDN(mat_x);
    MAT_IDN(wmat);
    MAT_IDN(m_axes);
    MAT_IDN(m_rev_axes);

    if (!get_args(argc,argv)){
	fprintf(stderr,"anim_hardtrack: argument error.");
	return(-1);
    }

    if (axes || cent ){ /* vehicle has own reference frame */
	anim_add_trans(m_axes,centroid,zero);
	anim_add_trans(m_rev_axes,zero,rcentroid);
    }

    /* get track information from specified file */

    if (!(stream = fopen(*(argv+bu_optind),"r"))){
	fprintf(stderr,"Anim_hardtrack: Could not open file %s.\n",*(argv+bu_optind));
	return(0);
    }
    num_wheels = -1;
    if (radius) {
	while (!feof(stream)) {
	    fscanf(stream,"%*f %*f %*f");
	    num_wheels++;
	}
    } else {
	while (!feof(stream)) {
	    fscanf(stream,"%*f %*f %*f %*f");
	    num_wheels++;
	}
    }
    rewind(stream);

    /*allocate memory for track information*/
    x = (struct all *) bu_calloc(num_wheels,sizeof(struct all), "struct all");
    /*read rest of track info */
    for (i=0;i<NW;i++){
	fscanf(stream,"%lf %lf %lf", temp, temp+1, temp+2);
	if (radius)
	    x[i].w.rad = radius;
	else
	    fscanf(stream,"%lf",& x[i].w.rad);
	MAT4X3PNT(x[i].w.pos,m_rev_axes,temp);
	if (i==0)
	    track_y = x[0].w.pos[1];
	else
	    x[i].w.pos[1] = track_y;
    }
    (void) fclose(stream);

    (void) track_prep();

    if (get_circumf) {
	printf("%.10g\n",tracklen);
	return(0);
    }

    /* initialize to_track */
    VSET(to_track, 0.0, track_y, 0.0);
    VSET(to_front,1.0,0.0,0.0);

    if ((!print_link)&&(!print_wheel)) {
	fprintf(stderr,"anim_hardtrack: no ouput requested. Use -l or -w.\n");
	exit(0);
    }
    /* main loop */
    distance = 0.0;
    if(!steer)
	frame = first_frame;
    else
	frame = first_frame-1;
    for (val = 3; val > 2; frame++) {
	go = 1;
	/*p2 is current position. p3 is next;p1 is previous*/
	VMOVE(p1,p2);
	VMOVE(p2,p3);
	scanf("%*f");/*time stamp*/
	val = scanf("%lf %lf %lf", p3, p3+1, p3 + 2);
	if(!steer){
	    scanf("%lf %lf %lf",&yaw,&pitch,&roll);
	    anim_dy_p_r2mat(mat_v,yaw,pitch,roll);
	    anim_add_trans(mat_v,p3,rcentroid);
	}
	else { /* analyze positions for steering */
	    /*get useful direction unit vectors*/
	    if (frame == first_frame){ /* first frame*/
		VSUBUNIT(dir,p3,p2);
		VMOVE(dir2,dir);
	    }
	    else if (val < 3){ /*last frame*/
		VSUBUNIT(dir,p2,p1);
		VMOVE(dir2,dir);
	    }
	    else if (frame > first_frame){ /*normal*/
		VSUBUNIT(dir,p3,p1);
		VSUBUNIT(dir2,p2,p1);/*needed for vertical case*/
	    }
	    else go = 0;/*first time through loop;no p2*/

			/*create matrix which would move vehicle*/
	    anim_dir2mat(mat_v,dir,dir2);
	    anim_add_trans(mat_v,p2,rcentroid);
	}

	/*determine distance traveled*/
	VMOVE(wheel_prev,wheel_now);
	MAT4X3PNT(wheel_now,mat_v,to_track);
	if (frame > first_frame){  /* increment distance by distance moved */
	    VSUB2(vdelta,wheel_now,wheel_prev);
	    MAT3X3VEC(temp,mat_v,to_front);/*new front of vehicle*/
	    distance += VDOT(temp,vdelta);/*portion of vdelta in line with track*/
	}

	if (go){
	    if (print_mode==TRACK_ANIM) {
		printf("start %d;\nclean;\n", frame);
	    } else if (print_mode==TRACK_ARCED) {
		if (frame != arced_frame) continue;
		last_frame = 1;
	    }
	    if (print_link) {
		for (count=0;count<num_links;count++){
		    (void) get_link(position,&y_rot,distance+tracklen*count/num_links+init_dist);
		    anim_y_p_r2mat(wmat,0.0,y_rot+r[count].ang,0.0);
		    anim_add_trans(wmat,position,r[count].pos);
		    if ((axes || cent) && links_placed){ /* link moved from vehicle coords */
			bn_mat_mul(mat_x,wmat,m_rev_axes);
			bn_mat_mul(wmat,m_axes,mat_x);
		    }
		    else if (axes || cent){ /* link moved to vehicle coords */
			MAT_MOVE(mat_x,wmat);
			bn_mat_mul(wmat,m_axes,mat_x);
		    }
		    if (print_mode==TRACK_ANIM) {
			printf("anim %s.%d matrix %s\n", *(argv+link_nindex),count,link_cmd);
			anim_mat_printf(stdout,wmat,"%.10g ","\n",";\n");
		    } else if (print_mode==TRACK_ARCED) {
			printf("arced %s.%d matrix %s ", *(argv+link_nindex),count,link_cmd);
			anim_mat_printf(stdout,wmat,"%.10g ","","\n");
		    }
		}
	    }
	    if (print_wheel){
		for (count = 0;count<num_wheels;count++){
		    anim_y_p_r2mat(wmat,0.0,-distance/x[count].w.rad,0.0);
		    VREVERSE(temp,x[count].w.pos);
		    anim_add_trans(wmat,x[count].w.pos,temp);
		    if (axes || cent){
			bn_mat_mul(mat_x,wmat,m_rev_axes);
			bn_mat_mul(wmat,m_axes,mat_x);
		    }
		    if (print_mode==TRACK_ANIM) {
			printf("anim %s.%d matrix %s\n",*(argv+wheel_nindex),count,wheel_cmd);
			anim_mat_printf(stdout,wmat,"%.10g ","\n",";\n");
		    } else if (print_mode==TRACK_ARCED) {
			printf("arced %s.%d matrix %s ",*(argv+wheel_nindex),count,wheel_cmd);
			anim_mat_printf(stdout,wmat,"%.10g ","","\n");
		    }
		}
	    }
	    if (print_mode==TRACK_ANIM)
		printf("end;\n");
	}
	if (last_frame) break;
    }

    if (x) {
	bu_free(x, "struct all");
    }
    if (r) {
	bu_free(r, "struct rlink");
    }
    return( 0 );
}


int track_prep(void)/*run once at the beginning to establish important track info*/
{
    int i;
    fastf_t phi, costheta, link_angle, arc_angle;
    vect_t difference, link_cent;

    /* first loop - get inter axle slopes and start/end angles */
    for (i=0;i<NW;i++){
	/*calculate current slope vector*/
	VSUB2(x[i].s.dir,x[i].w.pos,x[PREV(i)].w.pos);
	x[i].s.len = MAGNITUDE(x[i].s.dir);
	/*calculate end angle of previous wheel - atan2(y,x)*/
	phi = atan2(x[i].s.dir[2],x[i].s.dir[0]);/*absolute angle of slope*/
	costheta = (x[PREV(i)].w.rad - x[i].w.rad)/x[i].s.len;/*cosine of special angle*/
	x[PREV(i)].w.ang1 = phi +  acos(costheta);
	while (x[PREV(i)].w.ang1 < 0.0)
	    x[PREV(i)].w.ang1 += 2.0*M_PI;
	x[i].w.ang0 = x[PREV(i)].w.ang1;
    }

    /* second loop - handle concavities */
    for (i=0;i<NW;i++){
	arc_angle = x[i].w.ang0 - x[i].w.ang1;
	while (arc_angle < 0.0)
	    arc_angle += 2.0*M_PI;
	if (arc_angle > M_PI) { /* concave */
	    x[i].w.ang0 = 0.5*(x[i].w.ang0 + x[i].w.ang1);
	    x[i].w.ang1 = x[i].w.ang0;
	    x[i].w.arc = 0.0;
	}
	else { /* convex - angles are already correct */
	    x[i].w.arc = arc_angle;
	}
    }

    /* third loop - calculate geometry of track segments */
    tracklen = 0.0;
    for (i=0;i<NW;i++){
	/*calculate endpoints of track segment*/
	x[i].t.pos1[0] = x[i].w.pos[0] + x[i].w.rad*cos(x[i].w.ang0);
	x[i].t.pos1[1] = x[i].w.pos[1];
	x[i].t.pos1[2] = x[i].w.pos[2] + x[i].w.rad*sin(x[i].w.ang0);
	x[i].t.pos0[0] = x[PREV(i)].w.pos[0] + x[PREV(i)].w.rad*cos(x[PREV(i)].w.ang1);
	x[i].t.pos0[1] = x[PREV(i)].w.pos[1];
	x[i].t.pos0[2] = x[PREV(i)].w.pos[2] + x[PREV(i)].w.rad*sin(x[PREV(i)].w.ang1);
	/*calculate length and direction*/
	VSUB2(difference,x[i].t.pos1,x[i].t.pos0);
	x[i].t.len = MAGNITUDE(difference);
	VSCALE((x[i].t.dir),difference,(1.0/x[i].t.len));

	/*calculate arclength and total track length*/
	tracklen += x[i].t.len;
	tracklen += x[i].w.arc*x[i].w.rad;
    }

    /* for a track with links already placed, get initial positions*/
    r = (struct rlink *) bu_calloc(num_links, sizeof(struct rlink), "struct rlink");
    if (links_placed)
	for (i=0;i<num_links;i++){
	    get_link(link_cent,&link_angle,init_dist + tracklen*i/num_links);
	    VREVERSE(r[i].pos,link_cent);
	    r[i].ang = -link_angle;
	}
    return(0);
}


int get_link(fastf_t *pos, fastf_t *angle_p, fastf_t dist)
{
    int i;
    vect_t temp;
    while (dist >= tracklen) /*periodicize*/
	dist -= tracklen;
    while (dist < 0.0)
	dist += tracklen;
    for (i=0;i<NW;i++){
	if ( (dist  -= x[i].t.len) < 0 ){
	    VSCALE(temp,(x[i].t.dir),dist);
	    VADD2(pos,x[i].t.pos1,temp);
	    *angle_p = atan2(x[i].t.dir[2],x[i].t.dir[0]);
	    return(2*i);
	}
	if ((dist -= x[i].w.rad*x[i].w.arc) < 0){
	    *angle_p = dist/x[i].w.rad;
	    *angle_p = x[i].w.ang1 - *angle_p; /*from x-axis to link*/
	    pos[0] = x[i].w.pos[0] + x[i].w.rad*cos(*angle_p);
	    pos[1] = x[i].w.pos[1];
	    pos[2] = x[i].w.pos[2] + x[i].w.rad*sin(*angle_p);
	    *angle_p -= M_PI_2; /*angle of clockwise tangent to circle*/
	    return(2*i+1);
	}
    }
    return -1;
}

#define OPT_STR "b:d:f:i:l:pr:w:sg:m:c"
int get_args(int argc, char **argv)
{
    fastf_t yaw, pch, rll;
    void anim_dx_y_z2mat(fastf_t *, double, double, double), anim_dz_y_x2mat(fastf_t *, double, double, double);
    int c, i;
    axes = cent = links_placed = print_wheel = print_link = 0;
    get_circumf = 0;
    print_mode = TRACK_ANIM;
    strcpy(link_cmd, "rarc");
    strcpy(wheel_cmd, "lmul");
    while ( (c=bu_getopt(argc,argv,OPT_STR)) != EOF) {
	i=0;
	switch(c){
	case 'b':
	    bu_optind -= 1;
	    sscanf(argv[bu_optind+(i++)],"%lf", &yaw );
	    sscanf(argv[bu_optind+(i++)],"%lf", &pch );
	    sscanf(argv[bu_optind+(i++)],"%lf", &rll );
	    bu_optind += 3;
	    anim_dx_y_z2mat(m_axes, rll, -pch, yaw);
	    anim_dz_y_x2mat(m_rev_axes, -rll, pch, -yaw);
	    axes = 1;
	    break;
	case 'd':
	    bu_optind -= 1;
	    sscanf(argv[bu_optind+(i++)],"%lf",centroid);
	    sscanf(argv[bu_optind+(i++)],"%lf",centroid+1);
	    sscanf(argv[bu_optind+(i++)],"%lf",centroid+2);
	    bu_optind += 3;
	    VREVERSE(rcentroid,centroid);
	    cent = 1;
	    break;
	case 'f':
	    sscanf(bu_optarg,"%d",&first_frame);
	    break;
	case 'i':
	    sscanf(bu_optarg,"%lf",&init_dist);
	    break;
	case 'p':
	    links_placed = 1;
	    break;
	case 'r':
	    sscanf(bu_optarg,"%lf",&radius);
	    break;
	case 'w':
	    wheel_nindex = bu_optind - 1;
	    /*sscanf(bu_optarg,"%s",wheel_name);*/
	    print_wheel = 1;
	    break;
	case 'l':
	    sscanf(bu_optarg,"%d", &num_links);
	    link_nindex = bu_optind;
	    bu_optind += 1;
	    print_link = 1;
	    break;
	case 's':
	    steer = 1;
	    break;
	case 'g':
	    sscanf(bu_optarg,"%d",&arced_frame);
	    print_mode = TRACK_ARCED;
	    break;
	case 'm':
	    switch (*bu_optarg) {
	    case 'l':
		strncpy(link_cmd,argv[bu_optind], 10);
		break;
	    case 'w':
		strncpy(wheel_cmd,argv[bu_optind], 10);
		break;
	    default:
		fprintf(stderr,"Unknown option: -m%c\n",*bu_optarg);
		return(0);
	    }
	    bu_optind += 1;
	    break;
	case 'c':
	    get_circumf = 1;
	    break;
	default:
	    fprintf(stderr,"Unknown option: -%c\n",c);
	    return(0);
	}
    }
    return(1);
}

void show_info(int which)/* for debugging - -1:track 0:both 1:link*/

{
    int i;
    if (which <=0){
	fprintf(stderr,"track length: %f\n",tracklen);
	fprintf(stderr,"link length: %f\n",tracklen/num_links);
	for (i=0;i<NW;i++){
	    fprintf(stderr,"wheel %d: %f %f %f %f %f %f\n",i,x[i].w.pos[0],x[i].w.pos[1],x[i].w.pos[2],x[i].w.rad,x[i].w.ang1,x[i].w.arc);
	    fprintf(stderr,"track %d: %f %f %f %f %f %f %f\n",i,x[i].t.pos1[0],x[i].t.pos1[1],x[i].t.pos1[2],x[i].t.dir[0],x[i].t.dir[1],x[i].t.dir[2],x[i].t.len);
	    fprintf(stderr,"slope %d: %f %f %f %f\n",i,x[i].s.dir[0],x[i].s.dir[1],x[i].s.dir[2],x[i].s.len);
	}
    }
    /*	if (which >= 0){
	fprintf(stderr,"%d %f %f %f %f\n",count,position[0],position[1],position[2],y_rot);
	}*/
}


/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
