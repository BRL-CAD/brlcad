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

/*
 * Default height is 5 feet, 8 inches, arbitrarily
 */
#define DEFAULT_HEIGHT_INCHES 68.0 
#define DEFAULT_FILENAME "human.g"

#define MAXLENGTH 64
#define IN2MM	25.4
#define CM2MM	10.0

char *progname = "Human Model";
char filename[MAXLENGTH]=DEFAULT_FILENAME;

/**
 * This function takes in a vector, then applies a new direction to it
 * using x, y, and z degrees, and exports it to resultVect, and returns
 * the distance of the vector.
 */

fastf_t setDirection(fastf_t *inVect, fastf_t *resultVect, fastf_t x, fastf_t y, fastf_t z)
{
	vect_t outVect;
	mat_t rotMatrix;
	MAT_ZERO(rotMatrix);

	/*
	 * x y z placed inside rotation matrix and applied to vector.
         * bn_mat_angles(rotMatrix, x, y, z) matrix rot in degrees
	 * MAT4X3VEC(outVect, rotMatrix, inVect);
	 */
	bn_mat_angles(rotMatrix, x, y, z);
	MAT4X3VEC(outVect, rotMatrix, inVect);

/* Print rotation matrix
	int i=0;
	for(i=1; i<=16; i++){
		bu_log("%3.4f\t", rotMatrix[(i-1)]);
		if(i%4==0)
			bu_log("\n");
	}
*/
	VMOVE(resultVect, outVect);

	return MAGNITUDE(outVect);
}

/******************************************/
/***** Body Parts, from the head down *****/
/******************************************/

fastf_t makeHead(struct rt_wdb (*file), char *name, fastf_t standing_height, fastf_t headSize, fastf_t *headJoint, fastf_t *direction, fastf_t showBoxes)
{
	fastf_t headRadius = headSize / 2;

	mk_sph(file, name, headJoint, headRadius);

	if(showBoxes){
		point_t p1[8];
		VSET(p1[0], (-headRadius), (-headRadius), (headJoint[Z]-headRadius));
		VSET(p1[1], (-headRadius), (-headRadius), (headJoint[Z]+headRadius));
		VSET(p1[2], (-headRadius), (headRadius), (headJoint[Z]+headRadius));
		VSET(p1[3], (-headRadius), (headRadius), (headJoint[Z]-headRadius));
		VSET(p1[4], (headRadius), (-headRadius), (headJoint[Z]-headRadius));
		VSET(p1[5], (headRadius), (-headRadius), (headJoint[Z]+headRadius));
		VSET(p1[6], (headRadius), (headRadius), (headJoint[Z]+headRadius));
		VSET(p1[7], (headRadius), (headRadius), (headJoint[Z]-headRadius));	
		mk_arb8(file, "HeadBox.s", &p1[0][X]); 
	}

	return headSize;
}

fastf_t makeNeck(struct rt_wdb *file, char *name, fastf_t standing_height, fastf_t headSize, fastf_t neckSize, fastf_t *headJoint, fastf_t *neckJoint,
 fastf_t *direction, fastf_t showBoxes)
{
	fastf_t neckWidth;
	vect_t startVector, neckVector;

	neckWidth = headSize / 4;

	VSET(startVector, 0, 0, neckSize);
	setDirection(startVector, neckVector, direction[X], direction[Y], direction[Z]);
	VADD2(neckJoint, headJoint, neckVector);

	mk_rcc(file, name, headJoint, neckVector, neckWidth);

	if(showBoxes){
	/*TODO: add bounding boxes to neck, and other stuff, and make them fit the roational scheme*/
	}
	return neckWidth;
}

fastf_t makeUpperTorso(struct rt_wdb *file, char *name, fastf_t standing_height, fastf_t topTorsoLength, fastf_t shoulderWidth, fastf_t abWidth,
 fastf_t *neckJoint, fastf_t *abdomenJoint, fastf_t *leftShoulderJoint, fastf_t *rightShoulderJoint, fastf_t *direction, fastf_t showBoxes)
{
	vect_t startVector, topTorsoVector;
	vect_t leftVector, rightVector;

	/* Set length of top torso portion */
	VSET(startVector, 0, 0, topTorsoLength);
	setDirection(startVector, topTorsoVector, direction[X], direction[Y], direction[Z]);
	VADD2(abdomenJoint, neckJoint, topTorsoVector);
	
	/* change shoulder joints to match up to torso */
	VSET(leftVector, 0, (shoulderWidth+(shoulderWidth/3)), 0);
	VSET(rightVector, 0, (shoulderWidth+(shoulderWidth/3))*-1, 0);

	VADD2(leftShoulderJoint, neckJoint, leftVector);
	VADD2(rightShoulderJoint, neckJoint, rightVector);

	/* Torso will be an ellipsoidal tgc eventually, not a cone */
	mk_trc_h(file, name, neckJoint, topTorsoVector, shoulderWidth, abWidth);
	
	if(showBoxes){
	/*
	 *	point_t p1, p2;
	 *	VSET(p1, -topTorsoWidthTop, -topTorsoWidthTop, torsoTop);
	 *	VSET(p2, topTorsoWidthTop, topTorsoWidthTop, topTorsoMid);
	 *	mk_rpp(file, "UpperTorsoBox.s", p1, p2);
	 */
	}
	return abWidth;
}

fastf_t makeLowerTorso(struct rt_wdb *file, char *name, fastf_t standing_height, fastf_t lowTorsoLength, fastf_t abWidth, fastf_t pelvisWidth,
 fastf_t *abdomenJoint, fastf_t *pelvisJoint, fastf_t *leftThighJoint, fastf_t *rightThighJoint, fastf_t *direction, fastf_t showBoxes)
{
	vect_t startVector, lowTorsoVector, leftVector, rightVector;

	VSET(startVector, 0, 0, lowTorsoLength);
	setDirection(startVector, lowTorsoVector, direction[X], direction[Y], direction[Z]);
	VADD2(pelvisJoint, abdomenJoint, lowTorsoVector);

	/* Set locations of legs */
	VSET(leftVector, 0, (pelvisWidth/2), 0);
	VSET(rightVector, 0, ((pelvisWidth/2)*-1), 0);
	VADD2(leftThighJoint, pelvisJoint, leftVector);
	VADD2(rightThighJoint, pelvisJoint, rightVector);

	mk_trc_h(file, name, abdomenJoint, lowTorsoVector, abWidth, pelvisWidth);

	if(showBoxes){
	/*
	 *	point_t p1, p2;
	 *	VSET(p1, -lowerTorsoWidth, -lowerTorsoWidth, lowerTorso);
	 *	VSET(p2,  lowerTorsoWidth, lowerTorsoWidth, topTorsoMid);
	 *	mk_rpp(file, "LowerTorsoBox.s", p1, p2);
	 */
	}
	return pelvisWidth;
}

fastf_t makeShoulder(struct rt_wdb *file, fastf_t isLeft, char *partName, fastf_t standing_height, fastf_t showBoxes)
{
	return 1;
}

fastf_t makeUpperArm(struct rt_wdb *file, char *partName, fastf_t standing_height, fastf_t upperArmWidth, fastf_t elbowWidth, fastf_t *ShoulderJoint,
 fastf_t *ElbowJoint, fastf_t *direction, fastf_t showBoxes)
{
	fastf_t upperArmLength;
	vect_t startVector, armVector;

	upperArmLength = (standing_height / 4.5) * IN2MM;

	VSET(startVector, 0, 0, upperArmLength);

	setDirection(startVector, armVector, direction[X], direction[Y], direction[Z]); /* set y to 180 to point down */
	VADD2(ElbowJoint, ShoulderJoint, armVector);
	mk_trc_h(file, partName, ShoulderJoint, armVector, upperArmWidth, elbowWidth);

	if(showBoxes){
	/*
	 *	point_t p1, p2;
	 *	VSET(p1, -upperArmWidth, -upperArmWidth+y, armLength);		
	 *	VSET(p2, upperArmWidth, upperArmWidth+y, neckEnd);
	 *	mk_rpp(file, partName + bu_strlcat(Box), p1, p2);
	 */
	}
	return elbowWidth;
}

fastf_t makeElbow(struct rt_wdb *file, char *name, fastf_t *elbowJoint, fastf_t radius)
{
	mk_sph(file, name, elbowJoint, radius);
	return radius;
}

fastf_t makeLowerArm(struct rt_wdb *file, char *name, fastf_t standing_height, fastf_t elbowWidth, fastf_t wristWidth, fastf_t *elbowJoint,
 fastf_t *wristJoint, fastf_t *direction, fastf_t showBoxes)
{
	fastf_t lowerArmLength;
	vect_t startVector, armVector;

	lowerArmLength = (standing_height / 4.5) * IN2MM;
	
	VSET(startVector, 0, 0, lowerArmLength);

	setDirection(startVector, armVector, direction[X], direction[Y], direction[Z]);

	VADD2(wristJoint, elbowJoint, armVector);
	mk_trc_h(file, name, elbowJoint, armVector, elbowWidth, wristWidth);

        if(showBoxes){
	/*
         *       point_t p1, p2;
         *       VSET(p1, -lowerArmWidth, -lowerArmWidth+y, leftWrist);
         *       VSET(p2, lowerArmWidth, lowerArmWidth+y, armLength);
         *       mk_rpp(file, "LeftLowerArmBox.s", p1, p2);
	 */
	}
	return wristWidth;
}

fastf_t makeWrist(struct rt_wdb *file, char *name, fastf_t *wristJoint, fastf_t wristWidth)
{
	mk_sph(file, name, wristJoint, wristWidth);
	return wristWidth;
}

void makeHand(struct rt_wdb *file, char *name, fastf_t standing_height, fastf_t wristWidth, fastf_t *wristJoint, fastf_t showBoxes)
{
	fastf_t handLength, handWidth, x, y;
	handLength = (standing_height / 16) * IN2MM;
	handWidth = (standing_height / 32) * IN2MM;
	x = 0;
	y = (standing_height / 6) * IN2MM;

	/* VSET(handPoint, x, y, wrist); */
	mk_sph(file, name, wristJoint, wristWidth);

	if(showBoxes){
	/*
	 *	point_t p1, p2;
	 *	VSET(p1, -handWidth, -handWidth+y, leftWrist-handWidth);  
	 *	VSET(p2, handWidth, handWidth+y, leftWrist+handWidth);
	 *	mk_rpp(file, "LeftHandBox.s", p1, p2);
	 */
	}
}

fastf_t makeThighJoint(struct rt_wdb *file, char *name, fastf_t *thighJoint, fastf_t thighWidth)
{
	mk_sph(file, name, thighJoint, thighWidth);
	return thighWidth;
}

fastf_t makeThigh(struct rt_wdb *file, char *name, fastf_t standing_height, fastf_t thighLength, fastf_t thighWidth, fastf_t kneeWidth,
 fastf_t *thighJoint, fastf_t *kneeJoint, fastf_t *direction, fastf_t showBoxes)
{
	vect_t startVector, thighVector;

	VSET(startVector, 0, 0, thighLength);
	
	setDirection(startVector, thighVector, direction[X], direction[Y], direction[Z]);
	VADD2(kneeJoint, thighJoint, thighVector);
	mk_trc_h(file, name, thighJoint, thighVector, thighWidth, kneeWidth);

	if(showBoxes)
	{
	/*	point_t p1, p2;
	 *	VSET(p1, -thighWidth, -thighWidth+y, leftKnee);
	 *	VSET(p2, thighWidth, thighWidth+y, lowerTorso);
	 *	mk_rpp(file, "LeftThighBox.s", p1, p2);
	 */
	}

	return kneeWidth;
}

fastf_t makeKnee(struct rt_wdb *file, char *name, fastf_t *kneeJoint, fastf_t kneeWidth)
{
	mk_sph(file, name, kneeJoint, kneeWidth);	
	return kneeWidth;
}

fastf_t makeCalf(struct rt_wdb *file, char *name, fastf_t standing_height, fastf_t calfLength, fastf_t kneeWidth, fastf_t ankleWidth,
 fastf_t *kneeJoint, fastf_t *ankleJoint, fastf_t *calfDirection, fastf_t showBoxes)
{
	vect_t startVector, calfVector;
	
	VSET(startVector, 0, 0, calfLength);

	if(ankleJoint[Y] <= 0){
                ankleJoint[Y] = ankleWidth;
        }

	setDirection(startVector, calfVector, calfDirection[X], calfDirection[Y], calfDirection[Z]);
        VADD2(ankleJoint, kneeJoint, calfVector);

	mk_trc_h(file, name, kneeJoint, calfVector, kneeWidth, ankleWidth);

	if(showBoxes){
	/*
	 *	point_t p1, p2;
	 *	VSET(p1, -kneeWidth, -kneeWidth+y, leftKnee);
	 *	VSET(p2, kneeWidth, kneeWidth+y, leftAnkle);
	 *	mk_rpp(file, "LeftCalfBox.s", p1, p2);
	 */
	}
	return ankleWidth;
}

fastf_t makeAnkle(struct rt_wdb *file, char *name, fastf_t *ankleJoint, fastf_t ankleRadius)
{
	mk_sph(file, name, ankleJoint, ankleRadius);
	return ankleRadius;
}

fastf_t makeFoot(struct rt_wdb *file, char *name, fastf_t standing_height, fastf_t ankleRadius, fastf_t *ankleJoint, fastf_t *direction, fastf_t showBoxes)
{
	fastf_t footLength, toeRadius;
	vect_t startVector, footVector;
	
	footLength = ankleRadius * 3;
	toeRadius = ankleRadius * 1.2;

	VSET(startVector, 0, 0, footLength);

        setDirection(startVector, footVector, direction[X], direction[Y], direction[Z]);
	
	/*
	 * VADD2(ankleJoint, kneeJoint, footVector);
	 */
	mk_particle(file, name, ankleJoint, footVector, ankleRadius, toeRadius);

	if(showBoxes){
	/*
	 *	point_t p1, p2;
	 *	VSET(p1, -ankleRadius, -toeRadius+y, 0);
	 *	VSET(p2, footLength+toeRadius, toeRadius+y, toeRadius*2);
	 *	mk_rpp(file, "LeftFootBox.s", p1, p2);
	 */
	}
	return 0;
}

/**
 * Make profile makes the head and neck of the body
 */
void makeProfile(struct rt_wdb (*file), char *suffix, fastf_t standing_height, fastf_t ProfileSize, fastf_t *headJoint, fastf_t *neckJoint, fastf_t *direction, fastf_t showBoxes)
{
	fastf_t headSize, neckSize;
	char headName[MAXLENGTH]="Head.s";
	char neckName[MAXLENGTH]="Neck.s";

	bu_strlcat(headName, suffix, MAXLENGTH);
	bu_strlcat(neckName, suffix, MAXLENGTH);

	headSize = ProfileSize;
	neckSize = ProfileSize / 2;

	headSize = makeHead(file, headName, standing_height, headSize, headJoint, direction, showBoxes);
	makeNeck(file, neckName, standing_height, headSize, neckSize, headJoint, neckJoint, direction, showBoxes);
}

/*
 * Create all the torso parts, and set joint locations for each arm, and each leg.
 */
void makeTorso(struct rt_wdb (*file), char *suffix, fastf_t standing_height, fastf_t torsoLength, fastf_t *neckJoint, fastf_t *leftShoulderJoint, fastf_t *rightShoulderJoint,
fastf_t *leftThighJoint, fastf_t *rightThighJoint, fastf_t *direction, fastf_t showBoxes)
{
	fastf_t shoulderWidth, abWidth, pelvisWidth;
	fastf_t topTorsoLength, lowTorsoLength;
	point_t abdomenJoint, pelvisJoint;
	char upperTorsoName[MAXLENGTH]="UpperTorso.s";
	char lowerTorsoName[MAXLENGTH]="LowerTorso.s";
	char leftShoulderName[MAXLENGTH]="LeftShoulder.s";
	char rightShoulderName[MAXLENGTH]="RightShoulder.s";

	bu_strlcat(upperTorsoName, suffix, MAXLENGTH);
	bu_strlcat(lowerTorsoName, suffix, MAXLENGTH);
	bu_strlcat(leftShoulderName, suffix, MAXLENGTH);
	bu_strlcat(rightShoulderName, suffix, MAXLENGTH);

	topTorsoLength = (torsoLength *5) / 8;
	lowTorsoLength = (torsoLength *3) / 8;

	shoulderWidth = (standing_height / 8) *IN2MM;
	abWidth=(standing_height / 9) * IN2MM;
	pelvisWidth=(standing_height / 8) * IN2MM;

        abWidth = makeUpperTorso(file, upperTorsoName, standing_height, topTorsoLength, shoulderWidth, abWidth, neckJoint, abdomenJoint,
 leftShoulderJoint, rightShoulderJoint, direction, showBoxes);

	makeShoulder(file, 1, leftShoulderName, standing_height, showBoxes);
	makeShoulder(file, 0, rightShoulderName, standing_height, showBoxes);

        pelvisWidth = makeLowerTorso(file, lowerTorsoName, standing_height, lowTorsoLength, abWidth, pelvisWidth, abdomenJoint, pelvisJoint,
 leftThighJoint, rightThighJoint, direction, showBoxes);	
}

/**
 * Make the 3 components of the arm:the upper arm, the lower arm, and the hand.
 */
void makeArm(struct rt_wdb (*file), char *suffix, int isLeft, fastf_t standing_height, fastf_t armLength, fastf_t *shoulderJoint, fastf_t *direction, fastf_t showBoxes)
{
	fastf_t upperArmWidth, elbowWidth, wristWidth;
	point_t elbowJoint, wristJoint;

	upperArmWidth = armLength / 10;
	elbowWidth = armLength / 13;
	wristWidth = armLength / 15;

	char upperArmName[MAXLENGTH];
	char elbowName[MAXLENGTH];
	char lowerArmName[MAXLENGTH];
	char wristName[MAXLENGTH];
	char handName[MAXLENGTH];

        if(isLeft){
                bu_strlcpy(upperArmName, "LeftUpperArm.s", MAXLENGTH);
                bu_strlcpy(elbowName, "LeftElbow.s", MAXLENGTH);
                bu_strlcpy(lowerArmName, "LeftLowerArm.s", MAXLENGTH);
                bu_strlcpy(wristName, "LeftWrist.s", MAXLENGTH);
                bu_strlcpy(handName, "LeftHand.s", MAXLENGTH);
        }
        else{
                bu_strlcpy(upperArmName, "RightUpperArm.s", MAXLENGTH);
                bu_strlcpy(elbowName, "RightElbow.s", MAXLENGTH);
                bu_strlcpy(lowerArmName, "RightLowerArm.s", MAXLENGTH);
                bu_strlcpy(wristName, "RightWrist.s", MAXLENGTH);
                bu_strlcpy(handName, "RightHand.s", MAXLENGTH);
        }

	bu_strlcat(upperArmName, suffix, MAXLENGTH);
	bu_strlcat(elbowName, suffix, MAXLENGTH);
	bu_strlcat(lowerArmName, suffix, MAXLENGTH);
	bu_strlcat(wristName, suffix, MAXLENGTH);
	bu_strlcat(handName, suffix, MAXLENGTH);

	vect_t armDirection;
	setDirection(direction, armDirection, 0, 0, 0);

	/* direction is the direction that the arm will be pointing at the shoulder. */
	/* armDirection is the derivative of that, and can be adjusted to fit a pose */
        makeUpperArm(file, upperArmName, standing_height, upperArmWidth, elbowWidth, shoulderJoint, elbowJoint, armDirection, showBoxes);
        makeElbow(file, elbowName, elbowJoint, elbowWidth);

        makeLowerArm(file, lowerArmName, standing_height, elbowWidth, wristWidth, elbowJoint, wristJoint, armDirection, showBoxes);
        makeWrist(file, wristName, wristJoint, wristWidth);
        makeHand(file, handName, standing_height, wristWidth, wristJoint, showBoxes);
}

/**
 * Create the leg to be length 'legLength' by making a thigh, calf, and foot to meet requirements.
 */
void makeLeg(struct rt_wdb (*file), char *suffix, int isLeft, fastf_t standing_height, fastf_t legLength, fastf_t *thighJoint, fastf_t *thighDirection,
 fastf_t *kneeDirection, fastf_t *footDirection, fastf_t showBoxes)
{
	fastf_t thighWidth, kneeWidth, ankleWidth;
	fastf_t thighLength, calfLength;
	point_t kneeJoint, ankleJoint;

	char thighJointName[MAXLENGTH];
	char thighName[MAXLENGTH];
	char kneeName[MAXLENGTH];
	char calfName[MAXLENGTH];
	char ankleName[MAXLENGTH];
	char footName[MAXLENGTH];

	if(isLeft){
		bu_strlcpy(thighJointName, "LeftThighJoint.s", MAXLENGTH);
                bu_strlcpy(thighName, "LeftThigh.s", MAXLENGTH);
                bu_strlcpy(kneeName, "LeftKnee.s", MAXLENGTH);
                bu_strlcpy(calfName, "LeftCalf.s", MAXLENGTH);
                bu_strlcpy(ankleName, "LeftAnkle.s", MAXLENGTH);
                bu_strlcpy(footName, "LeftFoot.s", MAXLENGTH);
	}
        else{
		bu_strlcpy(thighJointName, "RightThighJoint.s", MAXLENGTH);
                bu_strlcpy(thighName, "RightThigh.s", MAXLENGTH);
                bu_strlcpy(kneeName, "RightKnee.s", MAXLENGTH);
                bu_strlcpy(calfName, "RightCalf.s", MAXLENGTH);
                bu_strlcpy(ankleName, "RightAnkle.s", MAXLENGTH);
                bu_strlcpy(footName, "RightFoot.s", MAXLENGTH);
	}
	bu_strlcat(thighJointName, suffix, MAXLENGTH);
	bu_strlcat(thighName, suffix, MAXLENGTH);
	bu_strlcat(kneeName, suffix, MAXLENGTH);
	bu_strlcat(calfName, suffix, MAXLENGTH);
	bu_strlcat(ankleName, suffix, MAXLENGTH);
	bu_strlcat(footName, suffix, MAXLENGTH);

	/* divvy up the length of the leg to the leg parts */
	thighLength = legLength / 2;
	calfLength = legLength / 2;

	thighWidth = thighLength / 4;
	kneeWidth = thighLength / 6;
	ankleWidth = calfLength / 8;

	makeThighJoint(file, thighJointName, thighJoint, thighWidth);
	makeThigh(file, thighName, standing_height, thighLength, thighWidth, kneeWidth, thighJoint, kneeJoint, thighDirection, showBoxes);
	makeKnee(file, kneeName, kneeJoint, kneeWidth);
	ankleWidth = makeCalf(file, calfName, standing_height, calfLength, kneeWidth, ankleWidth, kneeJoint, ankleJoint, kneeDirection, showBoxes);
	makeAnkle(file, ankleName, ankleJoint, ankleWidth);
	makeFoot(file, footName, standing_height, ankleWidth, ankleJoint, footDirection, showBoxes);
}

/**
 * Make the head, shoulders knees and toes, so to speak.
 * Head, neck, torso, arms, legs.
 */
void makeBody(struct rt_wdb (*file), char *suffix, fastf_t standing_height, fastf_t *location, fastf_t showBoxes)
{
	fastf_t headSize = (standing_height / 8) * IN2MM;
	fastf_t armLength = (standing_height / 2) * IN2MM;
	fastf_t legLength = ((standing_height*4) / 8) * IN2MM;
	fastf_t torsoLength = ((standing_height*3) / 8) * IN2MM;
	point_t leftShoulderJoint, rightShoulderJoint;
	point_t leftThighJoint, rightThighJoint;
	point_t neckJoint, headJoint;
	vect_t direction, lArmDirection, rArmDirection, lLegDirection, rLegDirection;
	vect_t lKneeDirection, rKneeDirection, lFootDirection, rFootDirection;
	/* pass off important variables to their respective functions */

	/* 
	 * Make sure that vectors, points, and widths are sent to each function 
         * for direction, location, and correct sizing, respectivly.
	 */
	VSET(headJoint, location[X], location[Y], ((standing_height*IN2MM)-(headSize/2)));
	VSET(direction, 180, 0, 0); /*Make the body build down, from head to toe. Or else it's upsidedown */

	/**
	 * Head Parts
	 */
	/*makeProfile makes the head and the neck */
	makeProfile(file, suffix, standing_height, headSize, headJoint, neckJoint, direction, showBoxes);

	/**
	 * Torso Parts
	 */
	makeTorso(file, suffix, standing_height, torsoLength, neckJoint, leftShoulderJoint, rightShoulderJoint, leftThighJoint, rightThighJoint, direction, showBoxes);

	/**
	 * Arm Parts
	 */
	/*The second argument is whether or not it is the left side, 1 = yes, 0 = no) */

	/* Arm sway, buy use of relative direction */
	setDirection(direction, lArmDirection, 0, 0, 0);
	setDirection(direction, rArmDirection, 0, 0, 0);

/* Arm Sway, by use of absolute direction
 *	VSET(lArmDirection, 0, 150, 0);
 *	VSET(rArmDirection, 0, 210, 0);
 */
	makeArm(file, suffix, 1, standing_height, armLength, leftShoulderJoint, lArmDirection, showBoxes);
	makeArm(file, suffix, 0, standing_height, armLength, rightShoulderJoint, rArmDirection, showBoxes);
	
	/**
	 * Leg Parts
	 */

	/*left and right legs */
	setDirection(direction, lLegDirection, 0, 0, 0);
	setDirection(direction, rLegDirection, 0, 0, 0);
	
	/* Leg Sway */
/*
	setDirection(lLegDirection, lLegDirection, 0, 0, 45);
	setDirection(rLegDirection, rLegDirection, 0, 0, 45);
*/

	VSET(lLegDirection, 0, 90, 0);
	VSET(lKneeDirection, 0, 180, 0);
	VSET(lFootDirection, 0, 90, 0);
	VSET(rLegDirection, 0, 90, 0);
	VSET(rKneeDirection, 0, 180, 0);
	VSET(rFootDirection, 0, 90, 0);

	makeLeg(file, suffix, 1, standing_height, legLength, leftThighJoint, lLegDirection, lKneeDirection, lFootDirection, showBoxes);
	makeLeg(file, suffix, 0, standing_height, legLength, rightThighJoint, rLegDirection, rKneeDirection, rFootDirection, showBoxes);
}	

/**
 * MakeArmy makes a square of persons n by n large, where n is the number of persons entered using -N
 * if N is large (>= 20) Parts start disapearing, oddly enough.
 */
void makeArmy(struct rt_wdb (*file), fastf_t standing_height, int number, fastf_t showBoxes)
{
	point_t locations;
	VSET(locations, 0, 0, 0); /* Starting location */
	int x = 0;
	int y = 0;
	int num;
	char testname[10]={'0'};
	
	num = 0.0;
	char suffix[MAXLENGTH];

	for(x = 0; x<number; x++){
		for(y=0; y<number; y++){
			sprintf(testname, "%d", num);
			bu_strlcpy(suffix, testname, MAXLENGTH);
			makeBody(file, suffix, standing_height, locations, showBoxes);
			VSET(locations, (locations[X]-(48*IN2MM)), locations[Y], 0);
			num++;
		}
		VSET(locations, 0, (locations[Y]-(36*IN2MM)), 0);
	}
}

/* human_data (will eventually) hold most/all data needed to make a person */

enum Genders { male, female } gender;

struct human_data_t
{
	fastf_t height;		/* Height of person standing */
	int age;		/* Age of person */
	enum gender;		/* Gender of person */
	fastf_t legLength;
	fastf_t torsoLength;
	fastf_t armLength;
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
	   "\t-A\t\tAutoMake defaults\n"
	   "\t-H\t\tSet Height in inches\n"
	   "\t-o\t\tSet output file name\n"
	   "\t-b\t\tShow bounding Boxes\n"
	   "\t-N\t\tNumber to make (square)\n"
	);

    bu_vls_free(&str);
    return;
}

/* Process command line arguments */
int read_args(int argc, char **argv, fastf_t *standing_height, fastf_t *stance, fastf_t *troops, fastf_t *showBoxes)
{
    int c = 0;
    char *options="H:N:h:O:o:a:b:s:A";
    float height=0;
    int soldiers=0;
    int pose=0;

    struct human_data_t *human_data;

    /* don't report errors */
    bu_opterr = 0;

    fflush(stdout);
    while ((c=bu_getopt(argc, argv, options)) != EOF) {
	switch (c) {
/*	    case 'a':
 *
 *		sscanf(bu_optarg, "%d", age);
 *		int i = age;
 *		break;
 */
	    case 'A':
		bu_log("AutoMode, making average man\n");
		*standing_height = DEFAULT_HEIGHT_INCHES;
		break;
	
            case 'N':
                sscanf(bu_optarg, "%d", &soldiers);
                fflush(stdin);
                if(soldiers <= 1){
                        bu_log("Only 1 person. Making 16\n");
                        soldiers = 4;
                }
                bu_log("Auto %d (squared) troop formation\n", soldiers);
                *troops = (float)soldiers;
                break;

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

	    case 's':
		sscanf(bu_optarg, "%d", &pose);
		fflush(stdin);
		switch(pose)
		{
			case 0:
				bu_log("Standing\n");
				break;
			case 1:
				bu_log("Sitting\n");
				break;
			case 2:
				bu_log("Driving\n");
				break;
			case 3:
				bu_log("Arms out\n");
				break;
			case 4:
				bu_log("The Letterman\n");
				break;
			default:
				bu_log("Bad Pose, default to stand\n");
				pose=0;
				*stance=0;
				break;
		}
		*stance=pose;
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
    fastf_t troops = 0;
    fastf_t stance = 0;
    char suffix[MAXLENGTH]= "";

    point_t location;
    VSET(location, 0, 0, 0); /* Default standing location */

    bu_vls_init(&name);
    bu_vls_trunc(&name, 0);

    bu_vls_init(&str);
    bu_vls_trunc(&str, 0);

    /* Process command line arguments */
    read_args(ac, av, &standing_height, &stance, &troops, &showBoxes);
    db_fp = wdb_fopen(filename);

    /*
     * This code here takes a direction vector, and then redirects it based on the angles given
     * so it is as follows : startingVector, resultVector, xdegrees, ydegrees, zdegrees.
     * and this will be used to position the arms and legs so they are joined yet flexable.
     * Just a test with an rcc.
     */  

 /*    Vector shape modifying test */
/*
    vect_t test1, test2;
    point_t testpoint;
    VSET(testpoint, 0.0, 0.0, 0.0);
	
    VSET(test1, 0, 0, 200);
    setDirection(test1, test2, 0, 90, 0);
    
    bu_log("%f, %f, %f\n", test1[X], test1[Y], test1[Z]);
    bu_log("%f, %f, %f\n", test2[X], test2[Y], test2[Z]);
   
    mk_rcc(db_fp, "NormalTest.s", testpoint, test1, (5*IN2MM));
    mk_rcc(db_fp, "ChangeTest.s", testpoint, test2, (5*IN2MM));
*/
/* See, now wasn't that easy? */
/*
    char testsuffix[MAXLENGTH]="test";
    point_t testJoint;
    VSET(testJoint, 0.0, 0.0, 0.0);
    vect_t testLeg, testKnee, testFoot;

    VSET(testLeg, 0.0, 0.0, 90.0);
    bu_log("Knee\n");

    setDirection(testLeg, testKnee, 0, -90, 0);

    bu_log("%f\t%f\t%f\n", testLeg[X], testLeg[Y], testLeg[Z]);
    bu_log("%f\t%f\t%f\n", testKnee[X], testKnee[Y], testKnee[Z]);

    bu_log("Foot\n");
    setDirection(testKnee, testFoot, 0.0, 90.0, 0.0);

    makeLeg(db_fp, testsuffix, 0, 5*IN2MM, 10*IN2MM, testJoint, testLeg, testKnee, testFoot, 0); 
*/
    if(!troops){
    	makeBody(db_fp, suffix, standing_height, location, showBoxes);
    }
    if(troops){
	makeArmy(db_fp, standing_height, troops, showBoxes);
    }

/** Make the Regions (.r's) of the body */

/* Make the .r for the real body */
    int is_region = 0;
    unsigned char rgb[3], rgb2[3], rgb3[3];
    
    BU_LIST_INIT(&human.l);
    (void)mk_addmember("Head.s", &human.l, NULL, WMOP_UNION);
    (void)mk_addmember("Neck.s", &human.l, NULL, WMOP_UNION);
    (void)mk_addmember("UpperTorso.s", &human.l, NULL, WMOP_UNION);
    (void)mk_addmember("LowerTorso.s", &human.l, NULL, WMOP_UNION);
    (void)mk_addmember("LeftUpperArm.s", &human.l, NULL, WMOP_UNION);
    (void)mk_addmember("RightUpperArm.s", &human.l, NULL, WMOP_UNION);
    (void)mk_addmember("LeftLowerArm.s",&human.l, NULL, WMOP_UNION);
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
