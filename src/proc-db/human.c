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
 * Generator for human models based on height, and other stuff eventually
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

/* Pi */
#define PI 3.1415926536

/**
 *Default height is 5 feet, 8 inches, arbitrarily
 */
#define DEFAULT_HEIGHT_INCHES 68.0 
#define DEFAULT_HEAD_RADIUS 6
#define DEFAULT_FILENAME "human.g"

#define MAXLENGTH 64
#define IN2MM	25.4
#define CM2MM	10.0

char *progname = "Human Model";
char filename[MAXLENGTH]=DEFAULT_FILENAME;

/******************************************/
/***** Body Parts, from the head down *****/
/******************************************/

fastf_t makeHead(struct rt_wdb (*file), fastf_t standing_height, fastf_t showBoxes)
{
	/** TODO: Make it where there's a center point, instead of 0,0,0 for the center */
	fastf_t headRadius, headHeight;
	point_t headCenter;
	/* for square hitbox outline */
	point_t p1, p2;

	headRadius = (standing_height / 8) / 2; /* Head is 1/8 of height */
	headHeight = standing_height - headRadius;
	
	headRadius *= IN2MM;
	headHeight *= IN2MM;
	
	if(showBoxes){
		/*make points around head*/
		VSET(p1, (-headRadius), (-headRadius), (headHeight-headRadius));
		VSET(p2, (headRadius), (headRadius), (headHeight+headRadius));
		mk_rpp(file, "HeadBox.s", p1, p2); 
	}

	VSET(headCenter, 0.0, 0.0, headHeight);
	mk_sph(file, "Head.s", headCenter, headRadius);

/*	makeNeck(file, standing_height, headRadius, showBoxes); */
/*	fastf_t num = makeNeck(fastf_t standing_height); */
	return headRadius;
}

fastf_t makeNeck(struct rt_wdb *file, fastf_t standing_height, fastf_t headRadius, fastf_t showBoxes)
{
	fastf_t neckRadius, neckHeight, neckSpot, neckEnd;
	vect_t	neckLength;
	point_t neckPoint;

	neckRadius = headRadius / 2;
	neckHeight = standing_height / 22;
	neckHeight *= IN2MM;
	VSET(neckLength, 0.0, 0.0, (neckHeight*-1));
/*	bu_log("%f\n%f\n", standing_height*IN2MM, headRadius);*/
	neckSpot = (standing_height*IN2MM) - (2 * headRadius);
/*	bu_log("%f\n", neckSpot);*/
	VSET(neckPoint, 0.0, 0.0, neckSpot);
	neckEnd = neckSpot - neckHeight;
	mk_rcc(file, "Neck.s", neckPoint, neckLength, neckRadius);

	if(showBoxes){
		point_t p1, p2;
		VSET(p1, -neckRadius, -neckRadius, neckEnd);
		VSET(p2, neckRadius, neckRadius, neckSpot);
		mk_rpp(file, "NeckBox.s", p1, p2);
	}

	return neckEnd;
}

fastf_t makeUpperTorso(struct rt_wdb *file, fastf_t standing_height, fastf_t neckEnd, fastf_t showBoxes)
{
	fastf_t torsoTop = neckEnd;
	fastf_t topTorsoLength, topTorsoMid, topTorsoWidthTop, topTorsoWidthMid;
	point_t topJoint, middleJoint;

	topTorsoLength = (standing_height / 4.5) * IN2MM;
	
	topTorsoWidthTop = (standing_height / 7) * IN2MM;
	topTorsoWidthMid = (standing_height / 10) * IN2MM;
	topTorsoMid = torsoTop - topTorsoLength;
	
	/* Torso will be an ellipsoidal tgc eventually, not a cone */
	VSET(topJoint, 0.0, 0.0, torsoTop);
	VSET(middleJoint, 0.0, 0.0, topTorsoMid);
	mk_trc_top(file, "UpperTorso.s", topJoint, middleJoint, topTorsoWidthTop, topTorsoWidthMid);

	if(showBoxes){
		point_t p1, p2;
		VSET(p1, -topTorsoWidthTop, -topTorsoWidthTop, torsoTop);
		VSET(p2, topTorsoWidthTop, topTorsoWidthTop, topTorsoMid);
		mk_rpp(file, "UpperTorsoBox.s", p1, p2);
	}

	return topTorsoMid;
}

fastf_t makeLowerTorso(struct rt_wdb *file, fastf_t standing_height, fastf_t topTorsoMid, fastf_t showBoxes)
{
	fastf_t midTorsoWidth, lowerTorsoWidth, lowerTorsoLength, lowerTorso;	
	point_t midJoint, lowerJoint;

	lowerTorsoLength = (standing_height / 6) * IN2MM;
	lowerTorso = topTorsoMid - lowerTorsoLength;
	midTorsoWidth = (standing_height / 10) * IN2MM;
	lowerTorsoWidth = (standing_height / 8) * IN2MM;
	
	VSET(midJoint, 0.0, 0.0, topTorsoMid);
	VSET(lowerJoint, 0.0, 0.0, lowerTorso);
	mk_trc_top(file, "LowerTorso.s", midJoint, lowerJoint, midTorsoWidth, lowerTorsoWidth);

	if(showBoxes){
		point_t p1, p2;
		VSET(p1, -lowerTorsoWidth, -lowerTorsoWidth, lowerTorso);
		VSET(p2,  lowerTorsoWidth, lowerTorsoWidth, topTorsoMid);
		mk_rpp(file, "LowerTorsoBox.s", p1, p2);
	}

	return lowerTorso;
}

fastf_t makeLeftUpperArm(struct rt_wdb *file, fastf_t standing_height, fastf_t neckEnd, fastf_t showBoxes)
{
	fastf_t leftElbow, upperArmLength, upperArmWidth, x, y, armLength;
	point_t shoulderJoint, elbowJoint;

	upperArmLength = (standing_height / 4.5) * IN2MM;
	upperArmWidth = upperArmLength / 6;
	leftElbow = upperArmLength / 7;

	armLength = neckEnd - upperArmLength;

	x = 0;
	y = ((standing_height / 7) * IN2MM)+upperArmWidth;

	VSET(shoulderJoint, x, y, neckEnd);
	VSET(elbowJoint, x, y, armLength);

	if(showBoxes){
		point_t p1, p2;
		VSET(p1, -upperArmWidth, -upperArmWidth+y, armLength);		
		VSET(p2, upperArmWidth, upperArmWidth+y, neckEnd);
		mk_rpp(file, "LeftUpperArmBox.s", p1, p2);
	}

	mk_trc_top(file, "LeftUpperArm.s", shoulderJoint, elbowJoint, upperArmWidth, leftElbow);

	return armLength;
}

fastf_t makeRightUpperArm(struct rt_wdb *file, fastf_t standing_height, fastf_t neckEnd, fastf_t showBoxes)
{
	fastf_t rightElbow, upperArmLength, upperArmWidth, x, y, armLength;
	point_t shoulderJoint, elbowJoint;

	upperArmLength = (standing_height / 4.5) * IN2MM;	
	upperArmWidth = upperArmLength / 6;
	rightElbow = upperArmLength/7;

	armLength = neckEnd - upperArmLength;

	x = 0;
	y = -1 * (((standing_height / 7) * IN2MM)+upperArmWidth);

	VSET(shoulderJoint, x, y, neckEnd);
	VSET(elbowJoint, x, y, armLength);
	
	if(showBoxes){
		point_t p1, p2;
		VSET(p1, -upperArmWidth, -upperArmWidth+y, armLength);
		VSET(p2, upperArmWidth, upperArmWidth+y, neckEnd);
		mk_rpp(file, "RightUpperArmBox.s", p1, p2);
	}	

	mk_trc_top(file, "RightUpperArm.s", shoulderJoint, elbowJoint, upperArmWidth, rightElbow);

	return armLength;
}

fastf_t makeLeftLowerArm(struct rt_wdb *file, fastf_t standing_height, fastf_t armLength, fastf_t showBoxes)
{
	fastf_t leftWrist, lowerArmLength, lowerArmWidth, x, y, wristWidth;
	point_t elbowJoint, wristJoint;

	lowerArmLength = (standing_height / 4.5) * IN2MM;
	lowerArmWidth = lowerArmLength / 6;
	wristWidth = lowerArmLength / 7;

	leftWrist = armLength - lowerArmLength;
	
	x=0;
	y=((standing_height / 7)*IN2MM) + lowerArmWidth;

	VSET(elbowJoint, x, y, armLength);
	VSET(wristJoint, x, y, leftWrist);

	if(showBoxes){
		point_t p1, p2;
		VSET(p1, -lowerArmWidth, -lowerArmWidth+y, leftWrist);
		VSET(p2, lowerArmWidth, lowerArmWidth+y, armLength);
		mk_rpp(file, "LeftLowerArmBox.s", p1, p2);
	}
	mk_trc_top(file, "LeftLowerArm.s", elbowJoint, wristJoint, lowerArmWidth, wristWidth);
	return leftWrist;
}

fastf_t makeRightLowerArm(struct rt_wdb *file, fastf_t standing_height, fastf_t armLength, fastf_t showBoxes)
{
	fastf_t rightWrist, lowerArmLength, lowerArmWidth, x, y, wristWidth;
	point_t elbowJoint, wristJoint;

	lowerArmLength = (standing_height / 4.5) * IN2MM;
	lowerArmWidth = lowerArmLength / 6;
	wristWidth = lowerArmLength / 7;

	rightWrist = armLength - lowerArmLength;
	x=0;
	y=(((standing_height / 7) * IN2MM) + lowerArmWidth) * -1;

	VSET(elbowJoint, x, y, armLength);
	VSET(wristJoint, x, y, rightWrist);

	mk_trc_top(file, "RightLowerArm.s", elbowJoint, wristJoint, lowerArmWidth, wristWidth);

        if(showBoxes){
                point_t p1, p2;
                VSET(p1, -lowerArmWidth, -lowerArmWidth+y, rightWrist);
                VSET(p2, lowerArmWidth, lowerArmWidth+y, armLength);
                mk_rpp(file, "RightLowerArmBox.s", p1, p2);
        }

	return rightWrist;
}

void makeLeftHand(struct rt_wdb *file, fastf_t standing_height, fastf_t leftWrist, fastf_t showBoxes)
{
	fastf_t handLength, handWidth, x, y;
	point_t handPoint;
	handLength = (standing_height / 16) * IN2MM;
	handWidth = (standing_height / 32) * IN2MM;
	x = 0;
	y = (standing_height / 6) * IN2MM;

	VSET(handPoint, x, y, leftWrist);
	mk_sph(file, "LeftHand.s", handPoint, handWidth);
	if(showBoxes){
		point_t p1, p2;
		VSET(p1, -handWidth, -handWidth+y, leftWrist-handWidth);  
		VSET(p2, handWidth, handWidth+y, leftWrist+handWidth);
		mk_rpp(file, "LeftHandBox.s", p1, p2);
	}
}

void makeRightHand(struct rt_wdb *file, fastf_t standing_height, fastf_t RightWrist, fastf_t showBoxes)
{
	fastf_t handLength, handWidth, x, y;
	point_t handPoint;
	handLength = (standing_height / 16) * IN2MM;
	handWidth = (standing_height / 32) * IN2MM;
	x = 0;
	y = -1 * (standing_height / 6) * IN2MM;
	VSET(handPoint, x, y, RightWrist);
	mk_sph(file, "RightHand.s", handPoint, handWidth);
        if(showBoxes){
                point_t p1, p2;
                VSET(p1, -handWidth, -handWidth+y, RightWrist-handWidth);
                VSET(p2, handWidth, handWidth+y, RightWrist+handWidth);
                mk_rpp(file, "RightHandBox.s", p1, p2);
        }
}

fastf_t makeLeftThigh(struct rt_wdb *file, fastf_t standing_height, fastf_t lowerTorso, fastf_t showBoxes)
{
	fastf_t leftKnee, thighWidth, kneeWidth, thighLength, x, y;
	point_t thighJoint, kneeJoint;

	thighLength = (standing_height / 4.2) * IN2MM;
	leftKnee = lowerTorso - thighLength;

	thighWidth = thighLength / 4.5;
	kneeWidth = thighWidth / 1.5;

	x=0;
	y=thighWidth;
	
	VSET(thighJoint, x, y, lowerTorso);
	VSET(kneeJoint, x, y, leftKnee);

	mk_trc_top(file, "LeftThigh.s", thighJoint, kneeJoint, thighWidth, kneeWidth);
	if(showBoxes)
	{
		point_t p1, p2;
		VSET(p1, -thighWidth, -thighWidth+y, leftKnee);
		VSET(p2, thighWidth, thighWidth+y, lowerTorso);
		mk_rpp(file, "LeftThighBox.s", p1, p2);
	}

	return leftKnee;
}

fastf_t makeRightThigh(struct rt_wdb *file, fastf_t standing_height, fastf_t lowerTorso, fastf_t showBoxes)
{
	fastf_t rightKnee, thighWidth, kneeWidth, thighLength, x ,y;
	point_t thighJoint, kneeJoint;
	
	thighLength = (standing_height / 4.2) * IN2MM;
	rightKnee = lowerTorso - thighLength;

	thighWidth = thighLength / 4.5;
        kneeWidth = thighWidth / 1.5;

	x=0;
	y=thighWidth*-1;

	VSET(thighJoint, x, y, lowerTorso);
	VSET(kneeJoint, x, y, rightKnee);

	mk_trc_top(file, "RightThigh.s", thighJoint, kneeJoint, thighWidth, kneeWidth);
        
	if(showBoxes)
        {
                point_t p1, p2;
                VSET(p1, -thighWidth, -thighWidth+y, rightKnee);
                VSET(p2, thighWidth, thighWidth+y, lowerTorso);
                mk_rpp(file, "RightThighBox.s", p1, p2);
        }
	return rightKnee;
}

fastf_t makeLeftCalf(struct rt_wdb *file, fastf_t standing_height, fastf_t leftKnee, fastf_t showBoxes)
{
	fastf_t leftAnkle, ankleRadius, kneeWidth, calfLength, x, y;
	point_t kneeJoint, ankleJoint;
	
	calfLength = (standing_height / 4.2) * IN2MM;
	leftAnkle = leftKnee - calfLength;
	kneeWidth = calfLength / 6;
	ankleRadius = calfLength / 6.5;

	x=0;
	y=kneeWidth;

	if(leftAnkle <= 0){
                leftAnkle = ankleRadius;
        }
	
	VSET(kneeJoint, x, y, leftKnee);
	VSET(ankleJoint, x, y, leftAnkle);

	mk_trc_top(file, "LeftCalf.s", kneeJoint, ankleJoint, kneeWidth, ankleRadius);
	if(showBoxes){
		point_t p1, p2;
		VSET(p1, -kneeWidth, -kneeWidth+y, leftKnee);
		VSET(p2, kneeWidth, kneeWidth+y, leftAnkle);
		mk_rpp(file, "LeftCalfBox.s", p1, p2);
	}
	return leftAnkle;
}

fastf_t makeRightCalf(struct rt_wdb *file, fastf_t standing_height, fastf_t rightKnee, fastf_t showBoxes)
{
	fastf_t rightAnkle, ankleRadius, kneeWidth, calfLength, x, y;
	point_t kneeJoint, ankleJoint;

        calfLength = (standing_height / 4.2) * IN2MM;
        rightAnkle = rightKnee - calfLength;
        kneeWidth = calfLength / 6;
        ankleRadius = calfLength / 6.5;

	x=0;
	y=kneeWidth*-1;

        if(rightAnkle <= 0){
                rightAnkle = ankleRadius;
        }

        VSET(kneeJoint, x, y, rightKnee);
        VSET(ankleJoint, x, y, rightAnkle);

        mk_trc_top(file, "RightCalf.s", kneeJoint, ankleJoint, kneeWidth, ankleRadius);
        if(showBoxes){
                point_t p1, p2;
                VSET(p1, -kneeWidth, -kneeWidth+y, rightKnee);
                VSET(p2, kneeWidth, kneeWidth+y, rightAnkle);
                mk_rpp(file, "RightCalfBox.s", p1, p2);
        }
	return rightAnkle;
}

fastf_t makeLeftFoot(struct rt_wdb *file, fastf_t standing_height, fastf_t leftAnkle, fastf_t showBoxes)
{
	fastf_t footLength, toeRadius, ankleRadius, x, y;
	vect_t footVector;
	point_t footPoint;
	
	footLength = (standing_height / 8) * IN2MM;
	ankleRadius = leftAnkle;
	toeRadius = ankleRadius * 1.2;

	x=0;
	y=ankleRadius;
	
	VSET(footPoint, x, y, ankleRadius);
	VSET(footVector, footLength, 0.0, 0.0)

	mk_particle(file, "LeftFoot.s", footPoint, footVector, ankleRadius, toeRadius);	

	if(showBoxes){
		point_t p1, p2;
		VSET(p1, -ankleRadius, -toeRadius+y, 0);
		VSET(p2, footLength+toeRadius, toeRadius+y, toeRadius*2);
		mk_rpp(file, "LeftFootBox.s", p1, p2);
	}
	return 0;
}

fastf_t makeRightFoot(struct rt_wdb *file, fastf_t standing_height, fastf_t rightAnkle, fastf_t showBoxes)
{
	fastf_t footLength, toeRadius, ankleRadius, x, y;
	vect_t footVector;
	point_t footPoint;

	footLength = (standing_height / 8) * IN2MM;
	ankleRadius = rightAnkle;
	toeRadius = ankleRadius * 1.2;
	
	x=0;
	y=ankleRadius*-1;

	VSET(footPoint, x, y, ankleRadius);
	VSET(footVector, footLength, 0.0, 0.0);

	mk_particle(file, "RightFoot.s", footPoint, footVector, ankleRadius, toeRadius);	

        if(showBoxes){
                point_t p1, p2;
                VSET(p1, -ankleRadius, -toeRadius+y, 0);
                VSET(p2, footLength+toeRadius, toeRadius+y, toeRadius*2);
                mk_rpp(file, "RightFootBox.s", p1, p2);
        }
	return 0;
}

void makeBody(struct rt_wdb (*file), fastf_t standing_height, fastf_t showBoxes)
{
	fastf_t headRadius, neckEnd, midTorso, lowTorso, leftElbow, rightElbow, leftWrist, rightWrist;
	fastf_t leftKnee, rightKnee, leftAnkle, rightAnkle; 

	/* pass off important variables to their respective functions */
	headRadius = makeHead(file, standing_height, showBoxes);
	neckEnd = makeNeck(file, standing_height, headRadius, showBoxes);
	midTorso = makeUpperTorso(file, standing_height, neckEnd, showBoxes);
	lowTorso = makeLowerTorso(file, standing_height, midTorso, showBoxes);
	leftElbow = makeLeftUpperArm(file, standing_height, neckEnd, showBoxes);
	rightElbow = makeRightUpperArm(file, standing_height, neckEnd, showBoxes);
	leftWrist = makeLeftLowerArm(file, standing_height, leftElbow, showBoxes);
	rightWrist = makeRightLowerArm(file, standing_height, rightElbow, showBoxes);
	makeLeftHand(file, standing_height, leftWrist, showBoxes);
	makeRightHand(file, standing_height, rightWrist, showBoxes);
	leftKnee = makeLeftThigh(file, standing_height, lowTorso, showBoxes);
	rightKnee = makeRightThigh(file, standing_height, lowTorso, showBoxes);
	leftAnkle = makeLeftCalf(file, standing_height, leftKnee, showBoxes);
	rightAnkle = makeRightCalf(file, standing_height, rightKnee, showBoxes);
	makeLeftFoot(file, standing_height, leftAnkle, showBoxes);
	makeRightFoot(file, standing_height, rightAnkle, showBoxes);
}	

/* human_data (will) hold most/all data needed to make a person */

enum Genders { male, female } gender;

struct human_data_t
{
	fastf_t height;		/* Height of person standing */
	int age;		/* Age of person */
	enum gender;		/* Gender of person */
	/*etc*/	
};

/**
 * Display input parameters when debugging
 */
void print_human_info(char *name)
{
    bu_log("%s\n", name);
}

/**
 * Help message printed when -h/-? option is supplied
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
	   "\t-h\t\tShow help\n"
	   "\t-?\t\tShow help\n"
	   "\t-H\t\tSet Height in inches\n"
	   "\t-o\t\tSet output file name\n"
	   "\t-b\t\tShow bounding Boxes\n"
	);

    bu_vls_free(&str);
    return;
}

/* Process command line arguments */
int read_args(int argc, char **argv, fastf_t *standing_height, fastf_t *showBoxes)
{
    int c = 0;
    char *options="H:f:F:k:K:p:P:e:E:s:S:n:h:O:o:a:b";
    float height=0;

    struct human_data_t *human_data;

    /* don't report errors */
    bu_opterr = 0;

    fflush(stdout);
    while ((c=bu_getopt(argc, argv, options)) != EOF) {
	switch (c) {
/*	    case 'a':

		sscanf(bu_optarg, "%d", age);
		int i = age;
		break;
*/
	    case 'b':
		*showBoxes = 1;
		bu_log("Drawing bounding boxes\n");
		break;
	    case 'H':
		sscanf(bu_optarg, "%f", &height);
		fflush(stdin);
		if(height <= 0.0)
		{
			bu_log("Impossible height, setting default height!\n");
			height = DEFAULT_HEIGHT_INCHES;
			*standing_height = DEFAULT_HEIGHT_INCHES;
			bu_log("%.2f = height in inches\n", height);
		}
		else
		{
			*standing_height = height;
			bu_log("%.2f = height in inches\n", height);
		}
		break;
	    case 'h':
	    case '?':
		show_help(*argv, options);
		bu_exit(EXIT_SUCCESS, NULL);
		break;
	    case 'o':
	    case 'O':
		memset(filename, 0, MAXLENGTH);
		bu_strlcpy(filename, bu_optarg, MAXLENGTH);
		break;
	    default:
		show_help(*argv, options);
		bu_log("%s: illegal option -- %c\n", bu_getprogname(), c);
	    	bu_exit(EXIT_SUCCESS, NULL);
		break;
	}
    }
    fflush(stdout);
    
    return(bu_optind);
}

int main(int ac, char *av[])
{
    struct rt_wdb *db_fp;
    struct wmember human;
    struct wmember boxes;
    struct wmember hollow;

    progname = *av;
    
    struct bu_vls name;
    struct bu_vls str;
    
    fastf_t standing_height = DEFAULT_HEIGHT_INCHES;
    fastf_t showBoxes = 0;

    bu_vls_init(&name);
    bu_vls_trunc(&name, 0);

    bu_vls_init(&str);
    bu_vls_trunc(&str, 0);

    /* Process arguments */
/*    read_args(ac, av, &standing_height, &l_foot_bend_angle, &l_knee_bend_angle, &l_pelvis_bend_angle_x, &l_pelvis_bend_angle_y, &l_elbow_bend_angle, &l_shoulder_bend_angle_x, &l_shoulder_bend_angle_y, &r_foot_bend_angle, &r_knee_bend_angle, &r_pelvis_bend_angle_x, &r_pelvis_bend_angle_y, &r_elbow_bend_angle, &r_shoulder_bend_angle_x, &r_shoulder_bend_angle_y, &neck_bend_angle_x, &neck_bend_angle_y, &age); */
    read_args(ac, av, &standing_height, &showBoxes);
    db_fp = wdb_fopen(filename);

    makeBody(db_fp, standing_height, showBoxes);

/** Make the Regions of the body */
/* Make the .r for the realbody */
    int is_region = 0;
    unsigned char rgb[3], rgb2[3], rgb3[3];
    
    BU_LIST_INIT(&human.l);
    (void)mk_addmember("Head.s", &human.l, NULL, WMOP_UNION);
    (void)mk_addmember("Neck.s", &human.l, NULL, WMOP_UNION);
    (void)mk_addmember("UpperTorso.s", &human.l, NULL, WMOP_UNION);
    (void)mk_addmember("LowerTorso.s", &human.l, NULL, WMOP_UNION);
    (void)mk_addmember("LeftUpperArm.s", &human.l, NULL, WMOP_UNION);
    (void)mk_addmember("RightUpperArm.s", &human.l, NULL, WMOP_UNION);
    (void)mk_addmember("LeftLowerArm.s", &human.l, NULL, WMOP_UNION);
    (void)mk_addmember("RightLowerArm.s", &human.l, NULL, WMOP_UNION);
    (void)mk_addmember("LeftHand.s", &human.l, NULL, WMOP_UNION);
    (void)mk_addmember("RightHand.s", &human.l, NULL, WMOP_UNION);
    (void)mk_addmember("LeftThigh.s", &human.l, NULL, WMOP_UNION);
    (void)mk_addmember("RightThigh.s", &human.l, NULL, WMOP_UNION);
    (void)mk_addmember("LeftCalf.s", &human.l, NULL, WMOP_UNION);
    (void)mk_addmember("RightCalf.s", &human.l, NULL, WMOP_UNION);
    (void)mk_addmember("LeftFoot.s", &human.l, NULL, WMOP_UNION);
    (void)mk_addmember("RightFoot.s", &human.l, NULL, WMOP_UNION);

    is_region = 1;
    VSET(rgb, 128, 255, 128); /* some wonky bright green color */
    mk_lcomb(db_fp,
	     "Body.r",
	     &human,
	     is_region,
	     "plastic",
	     "di=.99 sp=.01",
	     rgb,
	     0);

/* make the .r for the bounding boxes */
    if(showBoxes){

    /*
     * Create opaque bounding boxes for representaions of where the person model
     * may lay up next to another model
     */

    BU_LIST_INIT(&boxes.l)
    (void)mk_addmember("HeadBox.s", &boxes.l, NULL, WMOP_UNION);
    (void)mk_addmember("NeckBox.s", &boxes.l, NULL, WMOP_UNION);
    (void)mk_addmember("UpperTorsoBox.s", &boxes.l, NULL, WMOP_UNION);
    (void)mk_addmember("LowerTorsoBox.s", &boxes.l, NULL, WMOP_UNION);
    (void)mk_addmember("LeftUpperArmBox.s", &boxes.l, NULL, WMOP_UNION);
    (void)mk_addmember("RightUpperArmBox.s", &boxes.l, NULL, WMOP_UNION);
    (void)mk_addmember("LeftLowerArmBox.s", &boxes.l, NULL, WMOP_UNION);
    (void)mk_addmember("RightLowerArmBox.s", &boxes.l, NULL, WMOP_UNION);
    (void)mk_addmember("LeftHandBox.s", &boxes.l, NULL, WMOP_UNION);
    (void)mk_addmember("RightHandBox.s", &boxes.l, NULL, WMOP_UNION);
    (void)mk_addmember("LeftThighBox.s", &boxes.l, NULL, WMOP_UNION);
    (void)mk_addmember("RightThighBox.s", &boxes.l, NULL, WMOP_UNION);
    (void)mk_addmember("LeftCalfBox.s", &boxes.l, NULL, WMOP_UNION);
    (void)mk_addmember("RightCalfBox.s", &boxes.l, NULL, WMOP_UNION);
    (void)mk_addmember("LeftFootBox.s", &boxes.l, NULL, WMOP_UNION);
    (void)mk_addmember("RightFootBox.s", &boxes.l, NULL, WMOP_UNION);
    
    is_region = 1;
    VSET(rgb2, 255, 128, 128); /* redish color */
        mk_lcomb(db_fp,   
             "Boxes.r",
             &boxes,
             is_region,
             "plastic",
             "di=0.5 sp=0.5",
             rgb2,
             0);

    /*
     * Creating a hollow box that would allow for a person to see inside the
     * bounding boxes to the actual body representation inside.
     */

    BU_LIST_INIT(&hollow.l)
    (void)mk_addmember("HeadBox.s", &hollow.l, NULL, WMOP_UNION);
    (void)mk_addmember("Head.s", &hollow.l, NULL, WMOP_SUBTRACT);

    (void)mk_addmember("NeckBox.s", &hollow.l, NULL, WMOP_UNION);
    (void)mk_addmember("Neck.s", &hollow.l, NULL, WMOP_SUBTRACT);   

    (void)mk_addmember("UpperTorsoBox.s", &hollow.l, NULL, WMOP_UNION);
    (void)mk_addmember("UpperTorso.s", &hollow.l, NULL, WMOP_SUBTRACT);

    (void)mk_addmember("LowerTorsoBox.s", &hollow.l, NULL, WMOP_UNION);
    (void)mk_addmember("LowerTorso.s", &hollow.l, NULL, WMOP_SUBTRACT);
    
    (void)mk_addmember("LeftUpperArmBox.s", &hollow.l, NULL, WMOP_UNION);
    (void)mk_addmember("LeftUpperArm.s", &hollow.l, NULL, WMOP_SUBTRACT);
    
    (void)mk_addmember("RightUpperArmBox.s", &hollow.l, NULL, WMOP_UNION);
    (void)mk_addmember("RightUpperArm.s", &hollow.l, NULL, WMOP_SUBTRACT);
    
    (void)mk_addmember("LeftLowerArmBox.s", &hollow.l, NULL, WMOP_UNION);
    (void)mk_addmember("LeftLowerArm.s", &hollow.l, NULL, WMOP_SUBTRACT);
    
    (void)mk_addmember("RightLowerArmBox.s", &hollow.l, NULL, WMOP_UNION);
    (void)mk_addmember("RightLowerArm.s", &hollow.l, NULL, WMOP_SUBTRACT);
    
    (void)mk_addmember("LeftHandBox.s", &hollow.l, NULL, WMOP_UNION);
    (void)mk_addmember("LeftHand.s", &hollow.l, NULL, WMOP_SUBTRACT);
    
    (void)mk_addmember("RightHandBox.s", &hollow.l, NULL, WMOP_UNION);
    (void)mk_addmember("RightHand.s", &hollow.l, NULL, WMOP_SUBTRACT);
    
    (void)mk_addmember("LeftThighBox.s", &hollow.l, NULL, WMOP_UNION);
    (void)mk_addmember("LeftThigh.s", &hollow.l, NULL, WMOP_SUBTRACT);
    
    (void)mk_addmember("RightThighBox.s", &hollow.l, NULL, WMOP_UNION);
    (void)mk_addmember("RightThigh.s", &hollow.l, NULL, WMOP_SUBTRACT);
    
    (void)mk_addmember("LeftCalfBox.s", &hollow.l, NULL, WMOP_UNION);
    (void)mk_addmember("LeftCalf.s", &hollow.l, NULL, WMOP_SUBTRACT);
    
    (void)mk_addmember("RightCalfBox.s", &hollow.l, NULL, WMOP_UNION);
    (void)mk_addmember("RightCalf.s", &hollow.l, NULL, WMOP_SUBTRACT);
    
    (void)mk_addmember("LeftFootBox.s", &hollow.l, NULL, WMOP_UNION);
    (void)mk_addmember("LeftFoot.s", &hollow.l, NULL, WMOP_SUBTRACT);
    
    (void)mk_addmember("RightFootBox.s", &hollow.l, NULL, WMOP_UNION);
    (void)mk_addmember("RightFoot.s", &hollow.l, NULL, WMOP_SUBTRACT);

    is_region = 1;
    VSET(rgb3, 128, 128, 255); /* blueish color */
        mk_lcomb(db_fp,
             "Hollow.r",
             &hollow,
             is_region,
             "glass",
             "di=0.5 sp=0.5 tr=0.95 ri=1",
             rgb3,
             0);
    }



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
