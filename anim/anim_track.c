/*			A N I M _ T R A C K . C
 *
 *  Animate the links and wheels of a tracked vehicle. Handles tracks that
 *  change in shape during the animation.
 *
 *  Author -
 *	Carl J. Nuzman
 *  
 *  Source -
 *      The U. S. Army Research Laboratory
 *      Aberdeen Proving Ground, Maryland  21005-5068  USA
 *  
 *  Distribution Notice -
 *      Re-distribution of this software is restricted, as described in
 *      your "Statement of Terms and Conditions for the Release of
 *      The BRL-CAD Pacakge" agreement.
 *
 *  Copyright Notice -
 *      This software is Copyright (C) 1993 by the United States Army
 *      in all countries except the USA.  All rights reserved.
 */

#include "conf.h"
#include <math.h>
#include <stdio.h>
#include "machine.h"
#include "externs.h"
#include "vmath.h"
#include "anim.h"

#ifndef M_PI
#define M_PI	3.14159265358979323846
#endif

#ifndef M_PI_2
#define M_PI_2	1.57079632679489661923
#endif

#define GIVEN		0
#define CALCULATED	1

#define NW	num_wheels
#define NEXT(i)	(i+1)%NW
#define PREV(i)	(i+NW-1)%NW
typedef double *pdouble;

struct wheel {
vect_t		pos;	/* displacement of wheel from vehicle origin */
fastf_t		rad;	/* radius of wheel */
fastf_t		ang0;	/* angle where track meets wheel 0<a<2pi*/
fastf_t		ang1;	/* angle where track leaves wheel 0<a<p2i */
fastf_t		arc;	/* radian length of contact between wheel and track 0<a<pi*/
};

struct track {
vect_t		pos0;	/* beginning point of track section */
vect_t		pos1;	/* end of track section */
vect_t		dir;	/* unit vector:direction of track section ending here*/
fastf_t		len;	/* length of track section ending here*/
};

struct slope {
vect_t		dir;	/* vector from previous to current axle*/
fastf_t		len;	/* length of vector above*/
};

struct all {
struct wheel 	w;	/* parameters describing the track around a wheel */
struct track	t;	/* track between this wheel and the previous wheel */
struct slope	s;	/* vector between this axle and the previous axle */
};

/* external variables */
extern int optind;
extern char *optarg;

/* variables describing track geometry - used by main, trackprep, get_link*/
struct all *x;
fastf_t curve_a, curve_b, curve_c, s_start;
int num_links, num_wheels;
fastf_t track_y, tracklen, first_tracklen;

/* variables set by get_args */
char wheel_name[40];	/* base name of wheels */
int print_wheel;	/* flag: do wheel animation */
int  axes, cent;	/* flags: alternate axes, centroid specified */
int  stretch;		/* flag: increase track length if needed */
int  elastic;		/* flag: increase track, but snap back to original size when possible */
int dist_mode;		/* 0 - distance is given; 1 - calculate distance */
int  first_frame;	/* integer to begin numbering frames */
fastf_t init_dist;	/* initial distance of first link along track */
fastf_t radius;		/* common radius of all wheels */
vect_t centroid, rcentroid;	/* alternate centroid and its reverse */
mat_t m_axes, m_rev_axes;	/* matrices to and from alternate axes */

main(argc,argv)
int argc;
char **argv;
{
	void y_p_r2mat(), add_trans(), an_mat_print();
	int get_args(), get_link(), track_prep(), val, frame, i, count;
	fastf_t y_rot, distance, yaw, pch, roll;
	vect_t cent_pos, wheel_now, wheel_prev;
	vect_t zero, position, vdelta, temp, to_track, to_front;
	mat_t wmat, mat_x;
	FILE *stream;
	struct wheel *wh;

	VSETALL(zero,0.0);
	VSETALL(to_track,0.0);
	VSETALL(centroid,0.0);
	VSETALL(rcentroid,0.0);
	radius = y_rot = init_dist = 0.0;
	first_frame = num_wheels = axes = cent = 0;
	MAT_IDN(mat_x);
	MAT_IDN(wmat);
	MAT_IDN(m_axes);
	MAT_IDN(m_rev_axes);

	if (!get_args(argc,argv))
		fprintf(stderr,"Anim_track: Argument error.\n");

	if (axes || cent ){ /* vehicle has own reference frame */
		add_trans(m_axes,centroid,zero);
		add_trans(m_rev_axes,zero,rcentroid);
	}

	/* get track information from specified file */
	
	if (!(stream = fopen(*(argv+optind+1),"r"))){
		fprintf(stderr,"Track: Could not open file %s.\n",*(argv+optind+1));
		return(0);
	}
	fscanf(stream,"%d %d %lf %lf", &num_wheels, &num_links, &tracklen, &track_y);
	first_tracklen = tracklen;

		/*allocate memory for track information*/
	/* x: contains track geometry for the current frame */
	x = (struct all *) calloc(num_wheels,sizeof(struct all));
	/* wh: contains geometry of wheels in mged database */
	wh = (struct wheel *) calloc(num_wheels,sizeof(struct wheel));

	/*read original wheel positions*/
	for (i=0;i<NW;i++){
		fscanf(stream,"%lf %lf",wh[i].pos,wh[i].pos+2);
		if (radius)
			wh[i].rad = radius;
		else
			fscanf(stream,"%lf",&wh[i].rad);
		wh[i].pos[1] = track_y;

		x[i].w.pos[0] = wh[i].pos[0];
		x[i].w.pos[1] = wh[i].pos[1];
		x[i].w.pos[2] = wh[i].pos[2];
		x[i].w.rad = wh[i].rad;
	}
	(void) fclose(stream);

	/* initialize to_track */
	VSET(to_track, 0.0, track_y, 0.0);
	VSET(to_front,1.0,0.0,0.0);

	/* main loop */
	distance = init_dist;
	frame = first_frame;
	for (val = 2; val > 1; frame++) {

		scanf("%*f");/*time*/
		if (dist_mode==GIVEN){
			scanf("%lf",&distance);
		}
		else if (dist_mode==CALCULATED){
			scanf("%lf %lf %lf", cent_pos, cent_pos+1, cent_pos + 2);/* tank position*/
			scanf("%lf %lf %lf",&yaw,&pch,&roll);/* tank orientation*/
		}

		/* read in all wheel positions */
		for(i=0;i<NW;i++){
			val=scanf("%lf %lf",x[i].w.pos, x[i].w.pos + 2);
			if (val < 2) break;
		}
		if (val < 2) break;


		/* call track_prep to calculate geometry of track */
		if ((track_prep())==-1){
			fprintf(stderr,"Track: error in frame %d: track too short.\n",frame);
			break;
		}


		if (dist_mode==CALCULATED){ /*determine distance traveled*/
			dy_p_r2mat(mat_x,yaw,pch,roll);
			add_trans(mat_x,cent_pos,rcentroid);			
			VMOVE(wheel_prev,wheel_now);
			MAT4X3PNT(wheel_now,mat_x,to_track);
			if (frame > first_frame){ /* increment distance by distance moved*/
				VSUB2(vdelta,wheel_now,wheel_prev);
				MAT3X3VEC(temp,mat_x,to_front);/*new front of vehicle*/
				distance += VDOT(temp,vdelta);/*portion of vdelta in line with track*/
			}
		}

		printf("start %d;\nclean;\n", frame);
	        for (count=0;count<num_links;count++){
        	        (void) get_link(position,&y_rot,distance+tracklen*count/num_links);
			y_p_r2mat(wmat,0.0,y_rot,0.0);
	        	add_trans(wmat,position,zero);
	        	if (axes || cent){ /* link moved to vehicle coords */
	        		MAT_MOVE(mat_x,wmat);
	        		mat_mul(wmat,m_axes,mat_x);
	        	}
			printf("anim %s.%d matrix lmul\n", *(argv+optind),count);
	        	an_mat_print(wmat,1);
		}
		if (print_wheel){
			for (count = 0;count<num_wheels;count++){
				y_p_r2mat(wmat,0.0,-distance/wh[count].rad,0.0);
				VREVERSE(temp,wh[count].pos);
				add_trans(wmat,x[count].w.pos,temp);
		        	if (axes || cent){
			        	mat_mul(mat_x,wmat,m_rev_axes);
		        		mat_mul(wmat,m_axes,mat_x);
		        	}
				printf("anim %s.%d matrix lmul\n",wheel_name,count);
				an_mat_print(wmat,1);
			}
		}
		printf("end;\n");
	}
	free(x);
	free(wh);
}

#define OPT_STR "b:d:cf:i:r:w:se"

int get_args(argc,argv)
int argc;
char **argv;
{
	fastf_t yaw, pch, rll;
	void dx_y_z2mat(), dz_y_x2mat();
	int c, i;
	axes = cent = print_wheel = 0;
	stretch = elastic= 0;
	dist_mode = GIVEN;
        while ( (c=getopt(argc,argv,OPT_STR)) != EOF) {
                i=0;
                switch(c){
                case 'b':
                	optind -= 1;
                        sscanf(argv[optind+(i++)],"%lf", &yaw );
                        sscanf(argv[optind+(i++)],"%lf", &pch );
                        sscanf(argv[optind+(i++)],"%lf", &rll );
			optind += 3;
			dx_y_z2mat(m_axes, rll, -pch, yaw);
			dz_y_x2mat(m_rev_axes, -rll, pch, -yaw);
			axes = 1;
                        break;
                case 'd':
                        optind -= 1;
                        sscanf(argv[optind+(i++)],"%lf",centroid);
                        sscanf(argv[optind+(i++)],"%lf",centroid+1);
                        sscanf(argv[optind+(i++)],"%lf",centroid+2);
                        optind += 3;
                        VREVERSE(rcentroid,centroid);
                	cent = 1;
                        break;
                case 'c':
                	dist_mode = CALCULATED;
                	break;
                case 'f':
                	sscanf(optarg,"%d",&first_frame);
                        break;
                case 'i':
                	sscanf(optarg,"%lf",&init_dist);
                	break;
                case 'r':
                	sscanf(optarg,"%lf",&radius);
                	break;
                case 'w':
                	sscanf(optarg,"%s",wheel_name);
                	print_wheel = 1;
                        break;
                case 's':
                	stretch = 1;
                	break;
                case 'e':
                	elastic = 1;
                	break;
                default:
                        fprintf(stderr,"Unknown option: -%c\n",c);
                        return(0);
                }
        }
        return(1);
}

/* TRACK_PREP - Calculate the geometry of the track. Wheel positions and
 * radii should already exist in the x[i] structs. Track_prep fills in the
 * rest of the x[i] structs and also calculates values for curve_a, curve_b,
 * curve_c, and s_start, which describe the caternary segment
 * return values: 0 = GOOD
 * 		 -1 = BAD. Track too short to fit around wheels
 */
int track_prep()
{
	int i;
	fastf_t phi, costheta, link_angle, hyper_start_x, arc_angle;
	fastf_t linearlen, hyperlen;
	vect_t start, difference, link_cent;
	int getcurve();
	fastf_t hyper_get_s();

	/* first loop - get inter axle slopes and start/end angles */
	for (i=0;i<NW;i++){
		/*calculate current slope vector*/
		VSUB2(x[i].s.dir,x[i].w.pos,x[PREV(i)].w.pos);
		x[i].s.len = MAGNITUDE(x[i].s.dir);
		/*calculate end angle of previous wheel assuming all convex*/
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
	
	/* third loop - calculate geometry of straight track segments */
	for (i=0;i<NW;i++){
		/*calculate endpoints of track segment*/
		x[i].t.pos1[0] = x[i].w.pos[0] + x[i].w.rad*cos(x[i].w.ang0);
		x[i].t.pos1[1] = x[i].w.pos[1];
		x[i].t.pos1[2] = x[i].w.pos[2] + x[i].w.rad*sin(x[i].w.ang0);
		x[i].t.pos0[0] = x[PREV(i)].w.pos[0] + x[PREV(i)].w.rad*cos(x[PREV(i)].w.ang1);
		x[i].t.pos0[1] = x[PREV(i)].w.pos[1];
		x[i].t.pos0[2] = x[PREV(i)].w.pos[2] + x[PREV(i)].w.rad*sin(x[PREV(i)].w.ang1);
		/*calculate length and direction of track segment*/
		VSUB2(difference,x[i].t.pos1,x[i].t.pos0);
		x[i].t.len = MAGNITUDE(difference);
		VSCALE((x[i].t.dir),difference,(1.0/x[i].t.len));
	}

	/* calculate total track length used so far*/
	linearlen = x[0].w.arc*x[0].w.rad;
	for (i=1;i<NW;i++){
		linearlen += x[i].t.len;
		linearlen += x[i].w.arc*x[i].w.rad;
	}

	/* calculate geometry of hyperbolic segment */
	if(elastic){ /* restore track to original size */
		tracklen = first_tracklen;
	}
	hyperlen = tracklen - linearlen;
	if(hyperlen < x[0].t.len){ /* desired length of hyperbola less than straight line*/
		if(stretch||elastic){
			tracklen += (x[0].t.len-hyperlen);
			hyperlen = tracklen - linearlen;
		}
		else{
			return(-1);/*bad, track is too short*/
		}
	}

	getcurve(&curve_a,&curve_b,&curve_c,&(x[NW-1].w.ang1),&(x[0].w.ang0),hyperlen,x[NW-1].w.pos,x[0].w.pos,x[NW-1].w.rad,x[0].w.rad);

	/* re-evaluate zeroth track section in light of curve information */
	x[0].t.pos0[X] = x[NW-1].w.pos[X] + x[NW-1].w.rad * cos(x[NW-1].w.ang1);
	x[0].t.pos0[Z] = x[NW-1].w.pos[Z] + x[NW-1].w.rad * sin(x[NW-1].w.ang1);
	x[0].t.pos1[X] = x[0].w.pos[X] + x[0].w.rad * cos(x[0].w.ang0);
	x[0].t.pos1[Z] = x[0].w.pos[Z] + x[0].w.rad * sin(x[0].w.ang0);
	VSUB2(difference,x[0].t.pos1,x[0].t.pos0);
	x[0].t.len = MAGNITUDE(difference);
	VSCALE(x[0].t.dir,difference,(1.0/x[0].t.len));
		
	if (curve_a > VDIVIDE_TOL)
		s_start = hyper_get_s(curve_a, 0.0, x[0].t.pos0[X] - curve_c);
	
	x[0].w.arc = x[0].w.ang0 - x[0].w.ang1;
	if (x[0].w.arc<0.0)
		x[0].w.arc += 2.0*M_PI;
	x[NW-1].w.arc = x[NW-1].w.ang0 - x[NW-1].w.ang1;
	if (x[NW-1].w.arc<0.0)
		x[NW-1].w.arc += 2.0*M_PI;

	return(0); /*good*/
}

/* GET_LINK - Find the position and angle of a link which is a given
 * distance around the track, measured from the point where the caternary
 * section meets wheel.0.
 */
int get_link(pos,angle_p,dist)
fastf_t *angle_p, dist;
vect_t pos;
{
	int i;
	vect_t temp;
	fastf_t hyper_get_x(), hyper_get_z(), hyper_get_ang();

	while (dist >= tracklen) /*periodicize*/
		dist -= tracklen;
	while (dist < 0.0)
		dist += tracklen;

	/* we want it to ignore the distance between wheel(n-1) and wheel(0)*/
	dist += x[0].t.len; 
	for (i=0;i<NW;i++){
		if ( (dist  -= x[i].t.len) < 0 ){
			VSCALE(temp,(x[i].t.dir),dist);
			VADD2(pos,x[i].t.pos1,temp);
			*angle_p = atan2(x[i].t.dir[2],x[i].t.dir[0]);
			return(2*i);
		}
		if ((dist -= x[i].w.rad*x[i].w.arc) < 0){
			*angle_p = dist/x[i].w.rad;
			*angle_p = x[i].w.ang1 - *angle_p;/*from x-axis to link*/
			pos[X] = x[i].w.pos[X] + x[i].w.rad*cos(*angle_p);
			pos[Y] = x[i].w.pos[Y];
			pos[Z] = x[i].w.pos[Z] + x[i].w.rad*sin(*angle_p);
			*angle_p -= M_PI_2; /*angle of clockwise tangent to circle*/
			return(2*i+1);
		}
	}

	/* caternary section */
	if ( curve_a > VDIVIDE_TOL){
		pos[X] = hyper_get_x(curve_a, 0.0, s_start+dist);
		pos[Y] = x[0].w.pos[Y];
		pos[Z] = hyper_get_z(curve_a,curve_b,0.0, pos[X]);
		pos[X] += curve_c;
		*angle_p = hyper_get_ang(curve_a,curve_c, pos[X]);
	}
	else { /* practically linear */
		VSCALE(temp,(x[0].t.dir),dist);
		VADD2(pos,x[0].t.pos0,temp);
		*angle_p = atan2(x[0].t.dir[Z],x[0].t.dir[X]);
	}


	
	return -1;
}

void show_info(which)/* for debugging - -1:track 0:both 1:link*/
int which;
{
	int i;
	if (which <=0){
		fprintf(stderr,"track length: %f\n",tracklen);
		fprintf(stderr,"link length: %f\n",tracklen/num_links);
		for (i=0;i<NW;i++){
			fprintf(stderr,"wheel %d: \n",i);
			fprintf(stderr," pos\t%f\t%f\t%f\t\n",x[i].w.pos[X],x[i].w.pos[Y],x[i].w.pos[Z]);
			fprintf(stderr," rad\t%f\tang0\t%f\tang1\t%f\tarc\t%f\n",x[i].w.rad,x[i].w.ang0,x[i].w.ang1,x[i].w.arc);

			fprintf(stderr,"track %d: \n",i);
			fprintf(stderr," pos0\t%f\t%f\tpos1\t%f\t%f\n",x[i].t.pos0[X],x[i].t.pos0[Z],x[i].t.pos1[X],x[i].t.pos1[Z]);
			fprintf(stderr," dir\t%f\t%f\t%f\tlen\t%f\n",x[i].t.dir[X],x[i].t.dir[Y],x[i].t.dir[Z],x[i].t.len);
			fprintf(stderr,"slope %d: %f %f %f %f\n",i,x[i].s.dir[0],x[i].s.dir[1],x[i].s.dir[2],x[i].s.len);
		}
	}
/*	if (which >= 0){
		fprintf(stderr,"%d %f %f %f %f\n",count,position[0],position[1],position[2],y_rot);
	}*/
}

