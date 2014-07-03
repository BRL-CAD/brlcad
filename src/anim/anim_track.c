/*                    A N I M _ T R A C K . C
 * BRL-CAD
 *
 * Copyright (c) 1993-2014 United States Government as represented by
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
/** @file anim_track.c
 *
 * Animate the links and wheels of a tracked vehicle. Handles tracks that
 * change in shape during the animation.
 *
 */

#include "common.h"

#include <math.h>
#include <string.h>
#include "bio.h"

#include "bu.h"
#include "bn.h"
#include "anim.h"
#include "vmath.h"

#include "./cattrack.h"


#define OPT_STR "sycuvb:d:f:i:r:p:w:g:m:l:ah?"

#define GIVEN 0
#define CALCULATED 1
#define STEERED 2

#define PRINT_ANIM 0
#define PRINT_ARCED 1

#define TRACK_MIN 0
#define TRACK_FIXED 1
#define TRACK_STRETCH 2
#define TRACK_ELASTIC 3

#define NW num_wheels
#define NEXT(i)	(i+1)%NW
#define PREV(i)	(i+NW-1)%NW


typedef double *pdouble;

struct wheel {
    vect_t pos;		/* displacement of wheel from vehicle origin */
    fastf_t rad;	/* radius of wheel */
    fastf_t ang0;	/* angle where track meets wheel 0<a<2pi*/
    fastf_t ang1;	/* angle where track leaves wheel 0<a<p2i */
    fastf_t arc;	/* radian length of contact between wheel and track 0<a<pi*/
};


struct track {
    vect_t pos0;	/* beginning point of track section */
    vect_t pos1;	/* end of track section */
    vect_t dir;		/* unit vector:direction of track section ending here*/
    fastf_t len;	/* length of track section ending here*/
};


struct slope {
    vect_t dir;		/* vector from previous to current axle*/
    fastf_t len;	/* length of vector above*/
};


struct all {
    struct wheel w;	/* parameters describing the track around a wheel */
    struct track t;	/* track between this wheel and the previous wheel */
    struct slope s;	/* vector between this axle and the previous axle */
};


/* variables describing track geometry - used by main, trackprep, get_link*/
struct all *x;

fastf_t curve_a, curve_b, curve_c, s_start;
int num_links, num_wheels;
fastf_t track_y, tracklen;

/* variables set by get_args */
int wheel_nindex;	/* argv[wheel_nindex] = wheelname*/
int link_nindex;	/* argv[link_nindex] = linkname*/
int print_wheel;	/* flag: do wheel animation */
int print_link;		/* flag: do link animation */
int print_mode;		/* anim for rt or arced for mged */
int arced_frame;	/* which frame to arced */
int axes, cent;		/* flags: alternate axes, centroid specified */
int dist_mode;		/* given, steered, or calculated */
int first_frame;	/* integer to begin numbering frames */
fastf_t init_dist;	/* initial distance of first link along track */
int one_radius;		/* flag: common radius specified */
fastf_t radius;		/* common radius of all wheels */
char link_cmd[10];	/* default is "rarc" */
char wheel_cmd[10];	/* default is "lmul" */
int get_circumf;	/* flag: just return circumference of track */
int read_wheels;	/* flag: read new wheel positions each frame */
int len_mode;		/* mode for track_len */
int anti_strobe;	/* flag: take measures against strobing effect */
vect_t centroid, rcentroid;	/* alternate centroid and its reverse */
mat_t m_axes, m_rev_axes;	/* matrices to and from alternate axes */

/* intentionally double for scan */
double first_tracklen;

static void
usage(const char *argv0)
{
    bu_log("Usage: %s [options] wheelfile < in.table > out.script\n", argv0);
    bu_log("\n\tOptions:\n");
    bu_log("\t  [-w parent/basename] to specify wheels to animate\n");
    bu_log("\t  [-a] to add random jitter\n");
    bu_log("\t  [-p num_pads parent/basename] to animate track pads\n");
    bu_log("\t  [-b # # #] to specify yaw, pitch, and roll\n");
    bu_log("\t  [-d # # #] to specify centroid (default is origin)\n");
    bu_log("\t  [{-u|-y|-s}] to specify track distance manually, via orientation, or via steering\n");
    bu_log("\t  [-a] to enable anti-strobing (track appears to go backwards\n");
    bu_log("\t  [-v] to specify new wheel positions every frame\n");
    bu_log("\t  [-c] to calculate track circumference\n");
    bu_log("\t  [-lm] to minimize track length\n");
    bu_log("\t  [{-lf|-ls|-le} #] to specify fixed track, stretchable, or elastic track length\n");
    bu_log("\t  [-i #] to specify initial track offset\n");
    bu_log("\t  [-f #] to specify initial script frame number\n");
    bu_log("\t  [-r #] to specify common radius for all wheels\n");
    bu_log("\t  [-g #] to output an mged script instead of an animation script\n");
    bu_log("\t  [-mp command] to specify a pad animation matrix\n");
    bu_log("\t  [-mw command] to specify a wheel animation matrix\n");
}


int
get_args(int argc, char **argv)
{
    int c, i;
    fastf_t yaw, pch, rll;
    const char *argv0 = argv[0];

    /* defaults*/
    wheel_nindex = link_nindex = 0;
    axes = cent = print_wheel = print_link = 0;
    arced_frame = first_frame = 0;
    init_dist = radius = 0.0;
    one_radius = get_circumf = read_wheels = 0;
    bu_strlcpy(link_cmd, "rarc", sizeof(link_cmd));
    bu_strlcpy(wheel_cmd, "lmul", sizeof(wheel_cmd));
    print_mode = PRINT_ANIM;
    dist_mode = GIVEN;
    len_mode = TRACK_MIN;
    anti_strobe = 0;

    while ((c=bu_getopt(argc, argv, OPT_STR)) != -1) {
	double scan[3];

	i=0;
	switch (c) {
	    case 's':
		dist_mode = STEERED;
		break;
	    case 'y':
		dist_mode = CALCULATED;
		break;
	    case 'c':
		get_circumf = 1;
		break;
	    case 'u':
		dist_mode = GIVEN;
		break;
	    case 'v':
		read_wheels = 1;
		break;
	    case 'b':
		bu_optind -= 1;

		sscanf(argv[bu_optind+(i++)], "%lf", &scan[0]);
		yaw = scan[0];
		sscanf(argv[bu_optind+(i++)], "%lf", &scan[0]);
		pch = scan[0];
		sscanf(argv[bu_optind+(i++)], "%lf", &scan[0]);
		rll = scan[0];

		bu_optind += 3;
		anim_dx_y_z2mat(m_axes, rll, -pch, yaw);
		anim_dz_y_x2mat(m_rev_axes, -rll, pch, -yaw);
		axes = 1;
		break;
	    case 'd':
		bu_optind -= 1;

		sscanf(argv[bu_optind+(i++)], "%lf", &scan[0]);
		sscanf(argv[bu_optind+(i++)], "%lf", &scan[1]);
		sscanf(argv[bu_optind+(i++)], "%lf", &scan[2]);
		VMOVE(centroid, scan); /* double to fastf_t */

		bu_optind += 3;
		VREVERSE(rcentroid, centroid);
		cent = 1;
		break;
	    case 'f':
		sscanf(bu_optarg, "%d", &first_frame);
		break;
	    case 'i':
		sscanf(bu_optarg, "%lf", &scan[0]);
		init_dist = scan[0]; /* double to fastf_t */
		break;
	    case 'r':
		sscanf(bu_optarg, "%lf", &scan[0]);
		radius = scan[0]; /* double to fastf_t */
		one_radius = 1;
		break;
	    case 'p':
		sscanf(bu_optarg, "%d", &num_links);
		link_nindex = bu_optind;
		bu_optind += 1;
		print_link = 1;
		break;
	    case 'w':
		wheel_nindex = bu_optind - 1;
		/*sscanf(bu_optarg, "%s", wheel_name);*/
		print_wheel = 1;
		break;
	    case 'g':
		sscanf(bu_optarg, "%d", &arced_frame);
		print_mode = PRINT_ARCED;
		break;
	    case 'm':
		switch (*bu_optarg) {
		    case 'p':
			bu_strlcpy(link_cmd, argv[bu_optind], sizeof(link_cmd));
			break;
		    case 'w':
			bu_strlcpy(wheel_cmd, argv[bu_optind], sizeof(wheel_cmd));
			break;
		    default:
			bu_log("%s: Unknown option: -m%c\n", argv0, *bu_optarg);
			return 0;
		}
		bu_optind += 1;
		break;
	    case 'l':
		switch (*bu_optarg) {
		    case 'm':
			len_mode = TRACK_MIN;
			break;
		    case 'f':
			len_mode = TRACK_FIXED;
			sscanf(argv[bu_optind], "%lf", &first_tracklen);
			tracklen = first_tracklen;
			bu_optind++;
			break;
		    case 's':
			len_mode = TRACK_STRETCH;
			sscanf(argv[bu_optind], "%lf", &first_tracklen);
			tracklen = first_tracklen;
			bu_optind++;
			break;
		    case 'e':
			len_mode = TRACK_ELASTIC;
			sscanf(argv[bu_optind], "%lf", &first_tracklen);
			tracklen = first_tracklen;
			bu_optind++;
			break;
		    default:
			bu_log("%s: Unknown option: -l%c\n", argv0, *bu_optarg);
			return 0;
		}
		break;
	    case 'a':
		anti_strobe = 1;
		break;
	    default:
		return 0;
	}
    }
    return 1;
}


/* TRACK_PREP - Calculate the geometry of the track. Wheel positions and
 * radii should already exist in the x[i] structs. Track_prep fills in the
 * rest of the x[i] structs and also calculates values for curve_a, curve_b,
 * curve_c, and s_start, which describe the catenary segment
 * return values: 0 = GOOD
 * -1 = BAD. Track too short to fit around wheels
 */
int
track_prep(void)
{
    int i;
    fastf_t phi, costheta, arc_angle;
    fastf_t linearlen, hyperlen;
    vect_t difference;

    /* first loop - get inter axle slopes and start/end angles */
    for (i=0;i<NW;i++) {
	/*calculate current slope vector*/
	VSUB2(x[i].s.dir, x[i].w.pos, x[PREV(i)].w.pos);
	x[i].s.len = MAGNITUDE(x[i].s.dir);
	/*calculate end angle of previous wheel assuming all convex*/
	phi = atan2(x[i].s.dir[2], x[i].s.dir[0]);/*absolute angle of slope*/
	costheta = (x[PREV(i)].w.rad - x[i].w.rad)/x[i].s.len;/*cosine of special angle*/
	x[PREV(i)].w.ang1 = phi + acos(costheta);
	while (x[PREV(i)].w.ang1 < 0.0)
	    x[PREV(i)].w.ang1 += M_2PI;
	x[i].w.ang0 = x[PREV(i)].w.ang1;
    }
    /* second loop - handle concavities */
    for (i=0;i<NW;i++) {
	arc_angle = x[i].w.ang0 - x[i].w.ang1;
	while (arc_angle < 0.0)
	    arc_angle += M_2PI;
	if (arc_angle > M_PI) {
	    /* concave */
	    x[i].w.ang0 = 0.5*(x[i].w.ang0 + x[i].w.ang1);
	    x[i].w.ang1 = x[i].w.ang0;
	    x[i].w.arc = 0.0;
	} else {
	    /* convex - angles are already correct */
	    x[i].w.arc = arc_angle;
	}
    }

    /* third loop - calculate geometry of straight track segments */
    for (i=0;i<NW;i++) {
	/*calculate endpoints of track segment*/
	x[i].t.pos1[0] = x[i].w.pos[0] + x[i].w.rad*cos(x[i].w.ang0);
	x[i].t.pos1[1] = x[i].w.pos[1];
	x[i].t.pos1[2] = x[i].w.pos[2] + x[i].w.rad*sin(x[i].w.ang0);
	x[i].t.pos0[0] = x[PREV(i)].w.pos[0] + x[PREV(i)].w.rad*cos(x[PREV(i)].w.ang1);
	x[i].t.pos0[1] = x[PREV(i)].w.pos[1];
	x[i].t.pos0[2] = x[PREV(i)].w.pos[2] + x[PREV(i)].w.rad*sin(x[PREV(i)].w.ang1);
	/*calculate length and direction of track segment*/
	VSUB2(difference, x[i].t.pos1, x[i].t.pos0);
	x[i].t.len = MAGNITUDE(difference);
	VSCALE((x[i].t.dir), difference, (1.0/x[i].t.len));
    }

    /* calculate total track length used so far*/
    linearlen = x[0].w.arc*x[0].w.rad;
    for (i=1;i<NW;i++) {
	linearlen += x[i].t.len;
	linearlen += x[i].w.arc*x[i].w.rad;
    }

    if (len_mode==TRACK_MIN) {
	tracklen = linearlen + x[0].t.len;
	curve_a = 0.0;
	return 0; /* early return */
    }

    if (len_mode==TRACK_ELASTIC) {
	tracklen = first_tracklen;
    }

    /* calculate geometry of hyperbolic segment */
    hyperlen = tracklen - linearlen;
    if (hyperlen < x[0].t.len) {
	/* desired length of hyperbola less than straight line*/
	if ((len_mode==TRACK_ELASTIC)||(len_mode==TRACK_STRETCH)) {
	    tracklen += (x[0].t.len-hyperlen);
	    hyperlen = tracklen - linearlen;
	} else {
	    return -1;/*bad, track is too short*/
	}
    }

    getcurve(&curve_a, &curve_b, &curve_c, &(x[NW-1].w.ang1), &(x[0].w.ang0), hyperlen, x[NW-1].w.pos, x[0].w.pos, x[NW-1].w.rad, x[0].w.rad);

    /* re-evaluate zeroth track section in light of curve information */
    x[0].t.pos0[X] = x[NW-1].w.pos[X] + x[NW-1].w.rad * cos(x[NW-1].w.ang1);
    x[0].t.pos0[Z] = x[NW-1].w.pos[Z] + x[NW-1].w.rad * sin(x[NW-1].w.ang1);
    x[0].t.pos1[X] = x[0].w.pos[X] + x[0].w.rad * cos(x[0].w.ang0);
    x[0].t.pos1[Z] = x[0].w.pos[Z] + x[0].w.rad * sin(x[0].w.ang0);
    VSUB2(difference, x[0].t.pos1, x[0].t.pos0);
    x[0].t.len = MAGNITUDE(difference);
    VSCALE(x[0].t.dir, difference, (1.0/x[0].t.len));

    if (curve_a > VDIVIDE_TOL)
	s_start = hyper_get_s(curve_a, 0.0, x[0].t.pos0[X] - curve_c);

    x[0].w.arc = x[0].w.ang0 - x[0].w.ang1;
    if (x[0].w.arc<0.0)
	x[0].w.arc += M_2PI;
    x[NW-1].w.arc = x[NW-1].w.ang0 - x[NW-1].w.ang1;
    if (x[NW-1].w.arc<0.0)
	x[NW-1].w.arc += M_2PI;

    return 0; /*good*/
}


/* GET_LINK - Find the position and angle of a link which is a given
 * distance around the track, measured from the point where the catenary
 * section meets wheel.0.
 */
int
get_link(fastf_t *pos, fastf_t *angle_p, fastf_t dist)
{
    int i;
    vect_t temp;

    while (dist >= tracklen) /*periodicize*/
	dist -= tracklen;
    while (dist < 0.0)
	dist += tracklen;

    /* we want it to ignore the distance between wheel(n-1) and wheel(0)*/
    dist += x[0].t.len;
    for (i=0;i<NW;i++) {
	if ((dist -= x[i].t.len) < 0) {
	    VSCALE(temp, (x[i].t.dir), dist);
	    VADD2(pos, x[i].t.pos1, temp);
	    *angle_p = atan2(x[i].t.dir[2], x[i].t.dir[0]);
	    return 2*i;
	}
	if ((dist -= x[i].w.rad*x[i].w.arc) < 0) {
	    *angle_p = dist/x[i].w.rad;
	    *angle_p = x[i].w.ang1 - *angle_p;/*from x-axis to link*/
	    pos[X] = x[i].w.pos[X] + x[i].w.rad*cos(*angle_p);
	    pos[Y] = x[i].w.pos[Y];
	    pos[Z] = x[i].w.pos[Z] + x[i].w.rad*sin(*angle_p);
	    *angle_p -= M_PI_2; /*angle of clockwise tangent to circle*/
	    return 2*i+1;
	}
    }

    /* catenary section */
    if (curve_a > VDIVIDE_TOL) {
	pos[X] = hyper_get_x(curve_a, 0.0, s_start+dist);
	pos[Y] = x[0].w.pos[Y];
	pos[Z] = hyper_get_z(curve_a, curve_b, 0.0, pos[X]);
	pos[X] += curve_c;
	*angle_p = hyper_get_ang(curve_a, curve_c, pos[X]);
    } else {
	/* practically linear */
	VSCALE(temp, (x[0].t.dir), dist);
	VADD2(pos, x[0].t.pos0, temp);
	*angle_p = atan2(x[0].t.dir[Z], x[0].t.dir[X]);
    }


    return -1;
}


int
main(int argc, char *argv[])
{
    int val, frame, i, count;
    int go;
    fastf_t y_rot, distance, yaw, pch, roll;
    vect_t cent_pos, wheel_now, wheel_prev;
    vect_t zero, position, vdelta, to_track, to_front;
    mat_t wmat, mat_x;
    FILE *stream;
    struct wheel *wh;
    int last_steer, last_frame;
    int rndtabi=0;
    fastf_t halfpadlen, delta, prev_dist;
    const char *argv0 = argv[0];

    /* intentionally double for scan */
    double temp[3];

    VSETALL(zero, 0.0);
    VSETALL(to_track, 0.0);
    VSETALL(centroid, 0.0);
    VSETALL(rcentroid, 0.0);
    VSETALL(wheel_now, 0.0);
    VSETALL(wheel_prev, 0.0);
    y_rot = 0.0;
    num_wheels = 0;
    last_steer = last_frame = 0;
    MAT_IDN(mat_x);
    MAT_IDN(wmat);
    MAT_IDN(m_axes);
    MAT_IDN(m_rev_axes);

    if (argc == 1 && isatty(fileno(stdin)) && isatty(fileno(stdout))) {
	usage(argv0);
	return 0;
    }

    if (!get_args(argc, argv)) {
	usage(argv0);
	return 0;
    }

    if (axes || cent) {
	/* vehicle has own reference frame */
	anim_add_trans(m_axes, centroid, zero);
	anim_add_trans(m_rev_axes, zero, rcentroid);
    }

    /* get track information from specified file */

    if (!(stream = fopen(*(argv+bu_optind), "rb"))) {
	bu_log("Track: Could not open file %s.\n", *(argv+bu_optind));
	return 0;
    }

    if (one_radius) {
	while (!feof(stream)) {
	    count = fscanf(stream, "%*f %*f %*f");
	    if (count != 3)
		break;
	    num_wheels++;
	}
    } else {
	while (!feof(stream)) {
	    count = fscanf(stream, "%*f %*f %*f %*f");
	    if (count != 3)
		break;
	    num_wheels++;
	}
    }
    rewind(stream);


    /*allocate memory for track information*/
    /* x: contains track geometry for the current frame */
    x = (struct all *) bu_calloc(num_wheels, sizeof(struct all), "x all");
    /* wh: contains geometry of wheels in mged database */
    wh = (struct wheel *) bu_calloc(num_wheels, sizeof(struct wheel), "wh wheel");

    /*read original wheel positions*/
    for (i=0;i<NW;i++) {
	double scan;

	count = fscanf(stream, "%lf %lf %lf", temp, temp+1, temp+2);
	if (count != 3)
	    break;

	if (one_radius) {
	    x[i].w.rad = radius;
	} else {
	    count = fscanf(stream, "%lf", &scan);
	    x[i].w.rad = scan;
	}
	MAT4X3PNT(x[i].w.pos, m_rev_axes, temp);
	if (i==0)
	    track_y = x[0].w.pos[1];
	else
	    x[i].w.pos[1] = track_y;


	wh[i].pos[0] = x[i].w.pos[0];
	wh[i].pos[1] = x[i].w.pos[1];
	wh[i].pos[2] = x[i].w.pos[2];
	wh[i].rad = x[i].w.rad;
    }
    (void) fclose(stream);

    /* initialize to_track */
    VSET(to_track, 0.0, track_y, 0.0);
    VSET(to_front, 1.0, 0.0, 0.0);

    if (get_circumf&& (!read_wheels)) {
	track_prep();
	printf("%.10g\n", tracklen);
	return 0;
    }


    if (dist_mode==STEERED) {
	/* prime the pumps */
	val = scanf("%*f");/*time*/
	val = scanf("%lf %lf %lf", temp, temp+1, temp+2);
	VMOVE(cent_pos, temp);
	if (val < 3)
	    return 0;
	go = anim_steer_mat(mat_x, cent_pos, 0);
	last_steer = 0;
    } else {
	go = 1;
    }

    if (anti_strobe) {
	BN_RANDSEED(rndtabi, 0);
    }

    /* main loop */
    prev_dist = distance = 0.0;
    frame = first_frame;
    for (; ; frame++) {
	if (dist_mode==GIVEN) {
	    val = scanf("%*f");/*time*/
	    val = scanf("%lf", &temp[0]);
	    distance = temp[0];
	    if (val < 1) {
		break;
	    }
	} else if (dist_mode==CALCULATED) {
	    val = scanf("%*f");/*time*/
	    val = scanf("%lf %lf %lf", &temp[0], &temp[1], &temp[2]);
	    /* converting double to fastf_t */
	    VMOVE(cent_pos, temp);
	    val = scanf("%lf %lf %lf", &temp[0], &temp[1], &temp[2]);
	    /* converting double to fastf_t */
	    yaw = temp[0];
	    pch = temp[1];
	    roll = temp[2];
	    if (val < 3)
		break;
	    anim_dy_p_r2mat(mat_x, yaw, pch, roll);
	    anim_dy_p_r2mat(mat_x, yaw, pch, roll);
	    anim_add_trans(mat_x, cent_pos, rcentroid);
	}

	if (read_wheels) {
	    /* read in all wheel positions */
	    for (i=0;i<NW;i++) {
		val = scanf("%lf %lf", &temp[0], &temp[1]);
		x[i].w.pos[0] = temp[0];
		x[i].w.pos[2] = temp[1]; /* ??? is [2] right? */
		if (val < 2) {
		    break;
		}
	    }
	}

	if (dist_mode==STEERED) {
	    val = scanf("%*f");/*time*/
	    val = scanf("%lf %lf %lf", &temp[0], &temp[1], &temp[2]);
	    if (val < 3) {
		if (last_steer)
		    break;
		else
		    last_steer = 1;
	    }
	    /* converting double to fastf_t */
	    VMOVE(cent_pos, temp);

	    go = anim_steer_mat(mat_x, cent_pos, last_steer);
	    anim_add_trans(mat_x, cent_pos, rcentroid);
	}

	/* call track_prep to calculate geometry of track */
	if ((frame==first_frame)||read_wheels) {
	    if ((track_prep())==-1) {
		bu_log("Track: error in frame %d: track too short.\n", frame);
		break;
	    }
	    if (get_circumf) {
		printf("%d\t%.10g\n", frame, tracklen);
	    }
	}


	if ((dist_mode==CALCULATED)||(dist_mode==STEERED)) {
	    /*determine distance traveled*/
	    VMOVE(wheel_prev, wheel_now);
	    MAT4X3PNT(wheel_now, mat_x, to_track);
	    if (frame > first_frame) {
		/* increment distance by distance moved*/
		VSUB2(vdelta, wheel_now, wheel_prev);
		MAT3X3VEC(temp, mat_x, to_front);/*new front of vehicle*/
		distance += VDOT(temp, vdelta);/*portion of vdelta in line with track*/
	    }
	}

	if (anti_strobe) {
	    halfpadlen = 0.5 * tracklen/(fastf_t) num_links;
	    delta = distance - prev_dist;
	    prev_dist = distance;
	    if ((delta >= halfpadlen)||(delta <= -halfpadlen)) {
		distance += BN_RANDOM(rndtabi) * 2.0* halfpadlen;
	    }
	}

	if (go && (!get_circumf)) {
	    if (print_mode==PRINT_ANIM) {
		printf("start %d;\nclean;\n", frame);
	    } else if (print_mode==PRINT_ARCED) {
		if (frame != arced_frame) continue;
		last_frame = 1;
	    }
	    if (print_link) {
		for (count=0;count<num_links;count++) {
		    (void) get_link(position, &y_rot, distance+tracklen*count/num_links+init_dist);
		    anim_y_p_r2mat(wmat, 0.0, y_rot, 0.0);
		    anim_add_trans(wmat, position, zero);
		    if (axes || cent) {
			/* link moved to vehicle coords */
			MAT_MOVE(mat_x, wmat);
			bn_mat_mul(wmat, m_axes, mat_x);
		    }
		    if (print_mode==PRINT_ANIM) {
			printf("anim %s%d matrix %s\n", *(argv+link_nindex), count, link_cmd);
			anim_mat_printf(stdout, wmat, "%.10g ", "\n", ";\n");
		    } else if (print_mode==PRINT_ARCED) {
			printf("arced %s%d matrix %s ", *(argv+link_nindex), count, link_cmd);
			anim_mat_printf(stdout, wmat, "%.10g ", "", "\n");
		    }
		}
	    }
	    if (print_wheel) {
		for (count = 0;count<num_wheels;count++) {
		    vect_t xpos;

		    anim_y_p_r2mat(wmat, 0.0, -distance/wh[count].rad, 0.0);
		    VREVERSE(xpos, wh[count].pos);
		    anim_add_trans(wmat, x[count].w.pos, xpos);
		    if (axes || cent) {
			bn_mat_mul(mat_x, wmat, m_rev_axes);
			bn_mat_mul(wmat, m_axes, mat_x);
		    }
		    if (print_mode==PRINT_ANIM) {
			printf("anim %s%d matrix %s\n", *(argv+wheel_nindex), count, wheel_cmd);
			anim_mat_printf(stdout, wmat, "%.10g ", "\n", ";\n");
		    } else if (print_mode==PRINT_ARCED) {
			printf("arced %s%d matrix %s ", *(argv+wheel_nindex), count, wheel_cmd);
			anim_mat_printf(stdout, wmat, "%.10g ", "", "\n");
		    }
		}
	    }
	    if (print_mode==PRINT_ANIM)
		printf("end;\n");
	}
	if (last_frame) break;
    }
    bu_free(x, "x all");
    bu_free(wh, "wh wheel");

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
