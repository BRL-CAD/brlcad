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
/** Location of all joints on the body located here */
struct jointInfo
{
        point_t headJoint;
        point_t neckJoint;
        point_t leftShoulderJoint;
        point_t rightShoulderJoint;
        point_t elbowJoint;
        point_t wristJoint;
        point_t abdomenJoint;
        point_t pelvisJoint;
        point_t leftThighJoint;
        point_t rightThighJoint;
        point_t kneeJoint;
        point_t ankleJoint;
};

/** Information for building the head */
struct headInfo
{
        fastf_t headSize;
        fastf_t neckSize, neckWidth;

        vect_t neckVector;
};

/** All information needed to build the torso lies here */
struct torsoInfo
{
        fastf_t torsoLength;
        fastf_t topTorsoLength, lowTorsoLength;
        fastf_t shoulderWidth;
        fastf_t abWidth;
        fastf_t pelvisWidth;

        vect_t topTorsoVector;
        vect_t lowTorsoVector;
};

/** All information needed to build the arms lie here */
struct armInfo
{
        fastf_t armLength;
        fastf_t upperArmWidth;
        fastf_t upperArmLength;
        fastf_t lowerArmLength, elbowWidth;
        fastf_t wristWidth;
        fastf_t handLength, handWidth;
        vect_t armVector;

        vect_t lArmDirection;   /* Arm direction vectors */
        vect_t rArmDirection;
        vect_t lElbowDirection;
        vect_t rElbowDirection;
        vect_t lWristDirection;
        vect_t rWristDirection;
};

/** All information needed to build the legs lie here */
struct legInfo
{
        fastf_t legLength;
        fastf_t thighLength, thighWidth;
        fastf_t calfLength, kneeWidth;
        fastf_t footLength, ankleWidth;
        fastf_t toeWidth;

        vect_t thighVector;
        vect_t calfVector;
        vect_t footVector;
        vect_t lLegDirection;   /* Leg direction vectors */
        vect_t rLegDirection;
        vect_t lKneeDirection;
        vect_t rKneeDirection;
        vect_t lFootDirection;
        vect_t rFootDirection;
};

enum genders { male, female };
struct human_data_t
{
        fastf_t height;         /* Height of person standing */
        int age;                /* Age of person, (still relevant?) */
        enum genders gender;    /* Gender of person */
        
	/* Various part lengths */
	struct headInfo head;
	struct torsoInfo torso;
	struct armInfo arms;
	struct legInfo legs;
	struct jointInfo joints;
};

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
 *	int i=0;
 *	for(i=1; i<=16; i++){
 *		bu_log("%3.4f\t", rotMatrix[(i-1)]);
 *		if(i%4==0)
 *			bu_log("\n");
 *	}
 */
	VMOVE(resultVect, outVect);
	return MAGNITUDE(outVect);
}

void vectorTest(struct rt_wdb *file)
{
    /*
     * This code here takes a direction vector, and then redirects it based on the angles given
     * so it is as follows : startingVector, resultVector, xdegrees, ydegrees, zdegrees.
     * and this will be used to position the arms and legs so they are joined yet flexable.
     * Just a test with an rcc.
     */

    /*Vector shape modifying test */
    vect_t test1, test2;
    point_t testpoint;
    VSET(testpoint, 0.0, 0.0, 0.0);
    VSET(test1, 0, 0, 200);
    setDirection(test1, test2, 0, 90, 0);
    bu_log("%f, %f, %f\n", test1[X], test1[Y], test1[Z]);
    bu_log("%f, %f, %f\n", test2[X], test2[Y], test2[Z]);
    mk_rcc(file, "NormalTest.s", testpoint, test1, (5*IN2MM));
    mk_rcc(file, "ChangeTest.s", testpoint, test2, (5*IN2MM));
    /* See, now wasn't that easy? */
}

/******************************************/
/***** Body Parts, from the head down *****/
/******************************************/

fastf_t makeHead(struct rt_wdb (*file), char *name, struct human_data_t *dude, fastf_t *direction, fastf_t showBoxes)
{
	bu_log("HeadSize:%f\n", dude->head.headSize);
	fastf_t head = dude->head.headSize / 2;
	mk_sph(file, name, dude->joints.headJoint, head);
	
	if(showBoxes){
		/* TODO, add bounding boxes */
	}
	return 0;
}

fastf_t makeNeck(struct rt_wdb *file, char *name, struct human_data_t *dude, fastf_t *direction, fastf_t showBoxes)
{
	vect_t startVector;
	dude->head.neckWidth = dude->head.headSize / 4;
	VSET(startVector, 0, 0, dude->head.neckSize);
	setDirection(startVector, dude->head.neckVector, direction[X], direction[Y], direction[Z]);
	VADD2(dude->joints.neckJoint, dude->joints.headJoint, dude->head.neckVector);
	mk_rcc(file, name, dude->joints.headJoint, dude->head.neckVector, dude->head.neckWidth);
	
	if(showBoxes){
	/*TODO: add bounding boxes to neck, and other stuff, and make them fit the roational scheme*/
	}
	return dude->head.neckWidth;
}

fastf_t makeUpperTorso(struct rt_wdb *file, char *name, struct human_data_t *dude, fastf_t *direction, fastf_t showBoxes)
{
	vect_t startVector;
	vect_t a, b, c, d;
	vect_t leftVector, rightVector;

	/* Set length of top torso portion */
	VSET(startVector, 0, 0, dude->torso.topTorsoLength);
	setDirection(startVector, dude->torso.topTorsoVector, direction[X], direction[Y], direction[Z]);
	VADD2(dude->joints.abdomenJoint, dude->joints.neckJoint, dude->torso.topTorsoVector);
	
	/* change shoulder joints to match up to torso */
	VSET(leftVector, 0, (dude->torso.shoulderWidth+(dude->torso.shoulderWidth/3)), 0);
	VSET(rightVector, 0, (dude->torso.shoulderWidth+(dude->torso.shoulderWidth/3))*-1, 0);

	VADD2(dude->joints.leftShoulderJoint, dude->joints.neckJoint, leftVector);
	VADD2(dude->joints.rightShoulderJoint, dude->joints.neckJoint, rightVector);

	/*Take shoulder width, and abWidth and convert values to vectors for tgc */
	VSET(a, (dude->torso.shoulderWidth/2), 0, 0);
	VSET(b, 0, dude->torso.shoulderWidth, 0);
	VSET(c, (dude->torso.abWidth/2), 0, 0);
	VSET(d, 0, (dude->torso.abWidth), 0);

	/* Torso will be an ellipsoidal tgc eventually, not a cone */
	mk_tgc(file, name, dude->joints.neckJoint, dude->torso.topTorsoVector, a, b, c, d);

	if(showBoxes){
	/*
	 *	point_t p1, p2;
	 *	VSET(p1, -topTorsoWidthTop, -topTorsoWidthTop, torsoTop);
	 *	VSET(p2, topTorsoWidthTop, topTorsoWidthTop, topTorsoMid);
	 *	mk_rpp(file, "UpperTorsoBox.s", p1, p2);
	 */
	}
	return dude->torso.abWidth;
}

fastf_t makeLowerTorso(struct rt_wdb *file, char *name, struct human_data_t *dude, fastf_t *direction, fastf_t showBoxes)
{
	vect_t startVector, leftVector, rightVector;
	vect_t a, b, c, d;

	VSET(startVector, 0, 0, dude->torso.lowTorsoLength);
	setDirection(startVector, dude->torso.lowTorsoVector, direction[X], direction[Y], direction[Z]);
	VADD2(dude->joints.pelvisJoint, dude->joints.abdomenJoint, dude->torso.lowTorsoVector);

	/* Set locations of legs */
	VSET(leftVector, 0, (dude->torso.pelvisWidth/2), 0);
	VSET(rightVector, 0, ((dude->torso.pelvisWidth/2)*-1), 0);
	VADD2(dude->joints.leftThighJoint, dude->joints.pelvisJoint, leftVector);
	VADD2(dude->joints.rightThighJoint, dude->joints.pelvisJoint, rightVector);

	VSET(a, (dude->torso.abWidth/2), 0, 0);
	VSET(b, 0, dude->torso.abWidth, 0);
	VSET(c, (dude->torso.pelvisWidth/2), 0, 0);
	VSET(d, 0, dude->torso.pelvisWidth, 0);

	mk_tgc(file, name, dude->joints.abdomenJoint, dude->torso.lowTorsoVector, a, b, c, d);

/*
*	mk_trc_h(file, name, abdomenJoint, lowTorsoVector, abWidth, pelvisWidth);
*/
	if(showBoxes){
	/*
	 *	point_t p1, p2;
	 *	VSET(p1, -lowerTorsoWidth, -lowerTorsoWidth, lowerTorso);
	 *	VSET(p2,  lowerTorsoWidth, lowerTorsoWidth, topTorsoMid);
	 *	mk_rpp(file, "LowerTorsoBox.s", p1, p2);
	 */
	}
	return dude->torso.pelvisWidth;
}

fastf_t makeShoulderJoint(struct rt_wdb *file, fastf_t isLeft, char *name, struct human_data_t *dude)
{
	if(isLeft)
		mk_sph(file, name, dude->joints.leftShoulderJoint, (dude->arms.upperArmWidth));
	else
		mk_sph(file, name, dude->joints.rightShoulderJoint, (dude->arms.upperArmWidth));

	return dude->torso.shoulderWidth;
}
fastf_t makeShoulder(struct rt_wdb *file, fastf_t isLeft, char *partName, struct human_data_t *dude, fastf_t showBoxes)
{
	return 1;
}

fastf_t makeUpperArm(struct rt_wdb *file, fastf_t isLeft, char *partName, struct human_data_t *dude, fastf_t showBoxes)
{
	vect_t startVector;

	dude->arms.upperArmLength = (dude->height / 4.5) * IN2MM;
	VSET(startVector, 0, 0, dude->arms.upperArmLength);
	if(isLeft){
		setDirection(startVector, dude->arms.armVector, dude->arms.lArmDirection[X], dude->arms.lArmDirection[Y], dude->arms.lArmDirection[Z]); /* set y to 180 to point down */
		VADD2(dude->joints.elbowJoint, dude->joints.leftShoulderJoint, dude->arms.armVector);
		mk_trc_h(file, partName, dude->joints.leftShoulderJoint, dude->arms.armVector, dude->arms.upperArmWidth, dude->arms.elbowWidth);

	}
	else{
		setDirection(startVector, dude->arms.armVector, dude->arms.rArmDirection[X], dude->arms.rArmDirection[Y], dude->arms.rArmDirection[Z]); /* set y to 180 to point down */
		VADD2(dude->joints.elbowJoint, dude->joints.rightShoulderJoint, dude->arms.armVector);
		mk_trc_h(file, partName, dude->joints.rightShoulderJoint, dude->arms.armVector, dude->arms.upperArmWidth, dude->arms.elbowWidth);

	}	
/* Vectors and equations for making TGC arms 
*	vect_t a, b, c, d;
*	VSET(a, upperArmWidth, 0 , 0);
*	VSET(b, 0, upperArmWidth, 0);
*	VSET(c, ((elbowWidth*3)/4), 0, 0)
*	VSET(d, 0, elbowWidth, 0);
*	
*	mk_tgc(file, partName, ShoulderJoint, armVector, a, b, c, d);
*/

	if(showBoxes){
	/*
	 *	point_t p1, p2;
	 *	VSET(p1, -upperArmWidth, -upperArmWidth+y, armLength);		
	 *	VSET(p2, upperArmWidth, upperArmWidth+y, neckEnd);
	 *	mk_rpp(file, partName + bu_strlcat(Box), p1, p2);
	 */
	}
	return dude->arms.elbowWidth;
}

fastf_t makeElbow(struct rt_wdb *file, fastf_t isLeft, char *name, struct human_data_t *dude)
{
	vect_t a, b, c;
	
	VSET(a, (dude->arms.elbowWidth), 0, 0);
	VSET(b, 0, (dude->arms.elbowWidth), 0);
	VSET(c, 0, 0, dude->arms.elbowWidth);
	
	mk_ell(file, name, dude->joints.elbowJoint, a, b, c);

/*
*	mk_sph(file, name, elbowJoint, radius);
*/
	return dude->arms.elbowWidth;
}

fastf_t makeLowerArm(struct rt_wdb *file, fastf_t isLeft, char *name, struct human_data_t *dude, fastf_t showBoxes)
{
	vect_t startVector;

	dude->arms.lowerArmLength = (dude->height / 4.5) * IN2MM;
	VSET(startVector, 0, 0, dude->arms.lowerArmLength);
	if(isLeft){
		setDirection(startVector, dude->arms.armVector, dude->arms.lElbowDirection[X], dude->arms.lElbowDirection[Y], dude->arms.lElbowDirection[Z]);
		VADD2(dude->joints.wristJoint, dude->joints.elbowJoint, dude->arms.armVector);
	}
	else{
		setDirection(startVector, dude->arms.armVector, dude->arms.rElbowDirection[X], dude->arms.rElbowDirection[Y], dude->arms.rElbowDirection[Z]);
		VADD2(dude->joints.wristJoint, dude->joints.elbowJoint, dude->arms.armVector);
	}
	/* vectors for building a tgc arm (which looks weird when rotated) */
/*	vect_t a, b, c, d;
*
*	VSET(a, ((elbowWidth*3)/4), 0, 0);
*	VSET(b, 0, (elbowWidth), 0);
*	VSET(c, wristWidth, 0, 0);
*	VSET(d, 0, wristWidth, 0);
*
*	mk_tgc(file, name, elbowJoint, armVector, a, b, c, d);
*/
	mk_trc_h(file, name, dude->joints.elbowJoint, dude->arms.armVector, dude->arms.elbowWidth, dude->arms.wristWidth);

        if(showBoxes){
	/*
         *       point_t p1, p2;
         *       VSET(p1, -lowerArmWidth, -lowerArmWidth+y, leftWrist);
         *       VSET(p2, lowerArmWidth, lowerArmWidth+y, armLength);
         *       mk_rpp(file, "LeftLowerArmBox.s", p1, p2);
	 */
	}
	return dude->arms.wristWidth;
}

fastf_t makeWrist(struct rt_wdb *file, fastf_t isLeft, char *name, struct human_data_t *dude)
{
	mk_sph(file, name, dude->joints.wristJoint, dude->arms.wristWidth);
	return dude->arms.wristWidth;
}

void makeHand(struct rt_wdb *file, fastf_t isLeft, char *name, struct human_data_t *dude, fastf_t showBoxes)
{
	dude->arms.handLength = (dude->height / 16) * IN2MM;
	dude->arms.handWidth = (dude->height / 32) * IN2MM;
	mk_sph(file, name, dude->joints.wristJoint, dude->arms.wristWidth);

	if(showBoxes){
	/*
	 *	point_t p1, p2;
	 *	VSET(p1, -handWidth, -handWidth+y, leftWrist-handWidth);  
	 *	VSET(p2, handWidth, handWidth+y, leftWrist+handWidth);
	 *	mk_rpp(file, "LeftHandBox.s", p1, p2);
	 */
	}
}

fastf_t makeThighJoint(struct rt_wdb *file, fastf_t isLeft, char *name, struct human_data_t *dude)
{
	if(isLeft)
		mk_sph(file, name, dude->joints.leftThighJoint, dude->legs.thighWidth);
	else
		mk_sph(file, name, dude->joints.rightThighJoint, dude->legs.thighWidth);

	return dude->legs.thighWidth;
}

fastf_t makeThigh(struct rt_wdb *file, fastf_t isLeft, char *name, struct human_data_t *dude, fastf_t showBoxes)
{
	vect_t startVector;
	VSET(startVector, 0, 0, dude->legs.thighLength);

	if(isLeft){
		setDirection(startVector, dude->legs.thighVector, dude->legs.lLegDirection[X], dude->legs.lLegDirection[Y], dude->legs.lLegDirection[Z]);
		VADD2(dude->joints.kneeJoint, dude->joints.leftThighJoint, dude->legs.thighVector);
		mk_trc_h(file, name, dude->joints.leftThighJoint, dude->legs.thighVector, dude->legs.thighWidth, dude->legs.kneeWidth);
	}
	else{
		setDirection(startVector, dude->legs.thighVector, dude->legs.rLegDirection[X], dude->legs.rLegDirection[Y], dude->legs.rLegDirection[Z]);
		VADD2(dude->joints.kneeJoint, dude->joints.rightThighJoint, dude->legs.thighVector);
		mk_trc_h(file, name, dude->joints.rightThighJoint, dude->legs.thighVector, dude->legs.thighWidth, dude->legs.kneeWidth);
	}

	if(showBoxes)
	{
	/*	point_t p1, p2;
	 *	VSET(p1, -thighWidth, -thighWidth+y, leftKnee);
	 *	VSET(p2, thighWidth, thighWidth+y, lowerTorso);
	 *	mk_rpp(file, "LeftThighBox.s", p1, p2);
	 */
	}

	return dude->legs.kneeWidth;
}

fastf_t makeKnee(struct rt_wdb *file, fastf_t isLeft, char *name, struct human_data_t *dude)
{
	mk_sph(file, name, dude->joints.kneeJoint, dude->legs.kneeWidth);	
	return dude->legs.kneeWidth;
}

fastf_t makeCalf(struct rt_wdb *file, fastf_t isLeft, char *name, struct human_data_t *dude, fastf_t showBoxes)
{
	vect_t startVector;
	VSET(startVector, 0, 0, dude->legs.calfLength);

/*
* Find a better way of error checking to see if the legs go thru the ground.
*	if(ankleJoint[Y] <= 0){
*                ankleJoint[Y] = ankleWidth;
*        }
*/
	if(isLeft)
		setDirection(startVector, dude->legs.calfVector, dude->legs.lKneeDirection[X], dude->legs.lKneeDirection[Y], dude->legs.lKneeDirection[Z]);
	else
		setDirection(startVector, dude->legs.calfVector, dude->legs.rKneeDirection[X], dude->legs.rKneeDirection[Y], dude->legs.rKneeDirection[Z]);

        VADD2(dude->joints.ankleJoint, dude->joints.kneeJoint, dude->legs.calfVector);
	mk_trc_h(file, name, dude->joints.kneeJoint, dude->legs.calfVector, dude->legs.kneeWidth, dude->legs.ankleWidth);

	if(showBoxes){
	/*
	 *	point_t p1, p2;
	 *	VSET(p1, -kneeWidth, -kneeWidth+y, leftKnee);
	 *	VSET(p2, kneeWidth, kneeWidth+y, leftAnkle);
	 *	mk_rpp(file, "LeftCalfBox.s", p1, p2);
	 */
	}
	return dude->legs.ankleWidth;
}

fastf_t makeAnkle(struct rt_wdb *file, fastf_t isLeft, char *name, struct human_data_t *dude)
{
	mk_sph(file, name, dude->joints.ankleJoint, dude->legs.ankleWidth);
	return dude->legs.ankleWidth;
}

fastf_t makeFoot(struct rt_wdb *file, fastf_t isLeft, char *name, struct human_data_t *dude, fastf_t showBoxes)
{
	vect_t startVector;
	dude->legs.footLength = dude->legs.ankleWidth * 3;
	dude->legs.toeWidth = dude->legs.ankleWidth * 1.2;
	VSET(startVector, 0, 0, dude->legs.footLength);

	if(isLeft)
        	setDirection(startVector, dude->legs.footVector, dude->legs.lFootDirection[X], dude->legs.lFootDirection[Y], dude->legs.lFootDirection[Z]);
	else
        	setDirection(startVector, dude->legs.footVector, dude->legs.rFootDirection[X], dude->legs.rFootDirection[Y], dude->legs.rFootDirection[Z]);

	mk_particle(file, name, dude->joints.ankleJoint, dude->legs.footVector, dude->legs.ankleWidth, dude->legs.toeWidth);

	if(showBoxes){
	/*
	 *	point_t p1, p2;
	 *	VSET(p1, -ankleWidth, -toeWidth+y, 0);
	 *	VSET(p2, footLength+toeWidth, toeWidth+y, toeWidth*2);
	 *	mk_rpp(file, "LeftFootBox.s", p1, p2);
	 */
	}
	return 0;
}

/**
 * Make profile makes the head and neck of the body
 */
void makeProfile(struct rt_wdb (*file), char *suffix, struct human_data_t *dude, fastf_t *direction, fastf_t showBoxes)
{
	char headName[MAXLENGTH]="Head.s";
	char neckName[MAXLENGTH]="Neck.s";
	bu_strlcat(headName, suffix, MAXLENGTH);
	bu_strlcat(neckName, suffix, MAXLENGTH);
	dude->head.neckSize = dude->head.headSize / 2;
	makeHead(file, headName, dude, direction, showBoxes);
	makeNeck(file, neckName, dude, direction, showBoxes);
}

/*
 * Create all the torso parts, and set joint locations for each arm, and each leg.
 */
void makeTorso(struct rt_wdb (*file), char *suffix, struct human_data_t *dude, fastf_t *direction, fastf_t showBoxes)
{
	char upperTorsoName[MAXLENGTH]="UpperTorso.s";
	char lowerTorsoName[MAXLENGTH]="LowerTorso.s";
	char leftShoulderName[MAXLENGTH]="LeftShoulder.s";
	char rightShoulderName[MAXLENGTH]="RightShoulder.s";

	bu_strlcat(upperTorsoName, suffix, MAXLENGTH);
	bu_strlcat(lowerTorsoName, suffix, MAXLENGTH);
	bu_strlcat(leftShoulderName, suffix, MAXLENGTH);
	bu_strlcat(rightShoulderName, suffix, MAXLENGTH);

	dude->torso.topTorsoLength = (dude->torso.torsoLength *5) / 8;
	dude->torso.lowTorsoLength = (dude->torso.torsoLength *3) / 8;

	dude->torso.shoulderWidth = (dude->height / 8) *IN2MM;
	dude->torso.abWidth=(dude->height / 9) * IN2MM;
	dude->torso.pelvisWidth=(dude->height / 8) * IN2MM;

        makeUpperTorso(file, upperTorsoName, dude, direction, showBoxes);

	makeShoulder(file, 1, leftShoulderName, dude, showBoxes);
	makeShoulder(file, 0, rightShoulderName, dude, showBoxes);

        makeLowerTorso(file, lowerTorsoName, dude, direction, showBoxes);	
}

/**
 * Make the 3 components of the arm:the upper arm, the lower arm, and the hand.
 */
void makeArm(struct rt_wdb (*file), char *suffix, int isLeft, struct human_data_t *dude, fastf_t showBoxes)
{
	dude->arms.upperArmWidth = dude->arms.armLength / 10;
	dude->arms.elbowWidth = dude->arms.armLength / 13;
	dude->arms.wristWidth = dude->arms.armLength / 15;

	char shoulderJointName[MAXLENGTH];
	char upperArmName[MAXLENGTH];
	char elbowName[MAXLENGTH];
	char lowerArmName[MAXLENGTH];
	char wristName[MAXLENGTH];
	char handName[MAXLENGTH];

        if(isLeft){
		bu_strlcpy(shoulderJointName, "LeftShoulderJoint.s", MAXLENGTH);
                bu_strlcpy(upperArmName, "LeftUpperArm.s", MAXLENGTH);
                bu_strlcpy(elbowName, "LeftElbow.s", MAXLENGTH);
                bu_strlcpy(lowerArmName, "LeftLowerArm.s", MAXLENGTH);
                bu_strlcpy(wristName, "LeftWrist.s", MAXLENGTH);
                bu_strlcpy(handName, "LeftHand.s", MAXLENGTH);
        }
        else{
		bu_strlcpy(shoulderJointName, "RightShoulderJoint.s", MAXLENGTH);
                bu_strlcpy(upperArmName, "RightUpperArm.s", MAXLENGTH);
                bu_strlcpy(elbowName, "RightElbow.s", MAXLENGTH);
                bu_strlcpy(lowerArmName, "RightLowerArm.s", MAXLENGTH);
                bu_strlcpy(wristName, "RightWrist.s", MAXLENGTH);
                bu_strlcpy(handName, "RightHand.s", MAXLENGTH);
        }

	bu_strlcat(shoulderJointName, suffix, MAXLENGTH);
	bu_strlcat(upperArmName, suffix, MAXLENGTH);
	bu_strlcat(elbowName, suffix, MAXLENGTH);
	bu_strlcat(lowerArmName, suffix, MAXLENGTH);
	bu_strlcat(wristName, suffix, MAXLENGTH);
	bu_strlcat(handName, suffix, MAXLENGTH);

	/* direction is the direction that the arm will be pointing at the shoulder. */
	/* armDirection is the derivative of that, and can be adjusted to fit a pose */
        makeShoulderJoint(file, isLeft, shoulderJointName, dude);
	makeUpperArm(file, isLeft, upperArmName, dude, showBoxes);
        makeElbow(file, isLeft, elbowName, dude);

        makeLowerArm(file, isLeft, lowerArmName, dude, showBoxes);
        makeWrist(file, isLeft, wristName, dude);
        makeHand(file, isLeft, handName, dude, showBoxes);
}

/**
 * Create the leg to be length 'legLength' by making a thigh, calf, and foot to meet requirements.
 */
void makeLeg(struct rt_wdb (*file), char *suffix, int isLeft, struct human_data_t *dude, fastf_t showBoxes)
{
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
	dude->legs.thighLength = dude->legs.legLength / 2;
	dude->legs.calfLength = dude->legs.legLength / 2;
	dude->legs.thighWidth = dude->legs.thighLength / 5;
	dude->legs.kneeWidth = dude->legs.thighLength / 6;
	dude->legs.ankleWidth = dude->legs.calfLength / 8;

	makeThighJoint(file, isLeft, thighJointName, dude);
	makeThigh(file, isLeft, thighName, dude, showBoxes);
	makeKnee(file, isLeft, kneeName, dude);
	makeCalf(file, isLeft, calfName, dude, showBoxes);
	makeAnkle(file, isLeft, ankleName, dude);
	makeFoot(file, isLeft, footName, dude, showBoxes);
}

/**
 * Make the head, shoulders knees and toes, so to speak.
 * Head, neck, torso, arms, legs.
 */
void makeBody(struct rt_wdb (*file), char *suffix, struct human_data_t *dude, fastf_t *location, fastf_t showBoxes)
{
	bu_log("Making Body\n");
	vect_t direction;
	dude->torso.torsoLength = 0;
	bu_log("Height:%f\n", dude->height);
	dude->head.headSize = (dude->height / 8) * IN2MM;
	bu_log("Head:%f\n", dude->head.headSize);
	dude->arms.armLength = (dude->height / 2) * IN2MM;
	bu_log("Arms:%f\n", dude->arms.armLength);
	dude->legs.legLength = ((dude->height * 4) / 8) * IN2MM;
	bu_log("Legs:%f\n", dude->legs.legLength);
	
	dude->torso.torsoLength = ((dude->height * 3) / 8) * IN2MM;
	bu_log("Torso:%f\n", dude->torso.torsoLength);

	/* 
	 * Make sure that vectors, points, and widths are sent to each function 
         * for direction, location, and correct sizing, respectivly.
	 */
	bu_log("Setting Direction\n");
	VSET(dude->joints.headJoint, location[X], location[Y], ((dude->height*IN2MM)-(dude->head.headSize/2)));
	VSET(direction, 180, 0, 0); /*Make the body build down, from head to toe. Or else it's upsidedown */

	/**
	 * Head Parts
	 */
	/*makeProfile makes the head and the neck */
	bu_log("Making Head\n");
	makeProfile(file, suffix, dude, direction, showBoxes);

	/**
	 * Torso Parts
	 */
	bu_log("Making Torso\n");
	makeTorso(file, suffix, dude, direction, showBoxes);

	/**
	 * Arm Parts
	 */
	/*The second argument is whether or not it is the left side, 1 = yes, 0 = no) */
	bu_log("Making Arms\n");
	makeArm(file, suffix, 1, dude, showBoxes);
	makeArm(file, suffix, 0, dude, showBoxes);
	
	/**
	 * Leg Parts
	 */
	bu_log("Making Legs\n");
	makeLeg(file, suffix, 1, dude, showBoxes);
	makeLeg(file, suffix, 0, dude, showBoxes);
}	

/**
 * MakeArmy makes a square of persons n by n large, where n is the number of persons entered using -N
 * if N is large (>= 20) Parts start disappearing, oddly enough.
 */
void makeArmy(struct rt_wdb (*file), struct human_data_t dude, int number, fastf_t showBoxes)
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
			makeBody(file, suffix, &dude, locations, showBoxes); 
			VSET(locations, (locations[X]-(48*IN2MM)), locations[Y], 0);
			num++;
		}
		VSET(locations, 0, (locations[Y]-(36*IN2MM)), 0);
	}
}

/* position limbs to fit stance input in command line (or just default to standing) */
void setStance(fastf_t stance, struct human_data_t *dude)
{
	vect_t downVect, forwardVect, rightVect, leftVect; /* generic vectors for holding temp values */

	/**
	 * The stances are the way the body is positioned via
	 * adjusting limbs and whatnot. THEY ARE:
	 * 0: Standing
	 * 1: Sitting
	 * 2: Driving
	 * 3: Arms Out
	 * 4: The Letterman
	 * #: and more as needed
	 * 999: Custom (done interactivly, in time)
	 */

	VSET(downVect, 0, 180, 0); /*straight down*/
	VSET(forwardVect, 0, 90, 0); /*forwards, down X axis */
	VSET(rightVect, 90, 0, 0); /*Right, down Y axis */
	VSET(leftVect, -90, 0, 0); /*Left, up Y axis */

	switch((int)stance)
	{
	case 0:
		bu_log("Making it stand\n");
		VMOVE(dude->arms.lArmDirection, downVect);
		VMOVE(dude->arms.rArmDirection, downVect);
		VMOVE(dude->arms.lElbowDirection, downVect);
		VMOVE(dude->arms.rElbowDirection, downVect);
		VMOVE(dude->arms.lWristDirection, downVect);
		VMOVE(dude->arms.rWristDirection, downVect);
		VMOVE(dude->legs.lLegDirection, downVect);
		VMOVE(dude->legs.rLegDirection, downVect);
		VMOVE(dude->legs.lKneeDirection, downVect);
		VMOVE(dude->legs.rKneeDirection, downVect);		
		VMOVE(dude->legs.lFootDirection, forwardVect);
		VMOVE(dude->legs.rFootDirection, forwardVect);
		bu_log("Standing\n");
		break;
	case 1:
		bu_log("Making it sit\n");
		VMOVE(dude->arms.lArmDirection, downVect);
		VMOVE(dude->arms.rArmDirection, downVect);
                VMOVE(dude->arms.lElbowDirection, downVect);
                VMOVE(dude->arms.rElbowDirection, downVect);
                VMOVE(dude->arms.lWristDirection, downVect);
                VMOVE(dude->arms.rWristDirection, downVect);
		VMOVE(dude->legs.lLegDirection, forwardVect);
		VMOVE(dude->legs.rLegDirection, forwardVect);
		VMOVE(dude->legs.lKneeDirection, downVect);
		VMOVE(dude->legs.rKneeDirection, downVect);
		VMOVE(dude->legs.lFootDirection, forwardVect);
		VMOVE(dude->legs.rFootDirection, forwardVect);
		break;
	case 2:
		bu_log("Making it Drive\n"); /* it's like sitting, but with the arms extended */
		VMOVE(dude->arms.lArmDirection, downVect);
		VMOVE(dude->arms.rArmDirection, downVect);
                VMOVE(dude->arms.lElbowDirection, forwardVect);
                VMOVE(dude->arms.rElbowDirection, forwardVect);
                VMOVE(dude->arms.lWristDirection, forwardVect);
                VMOVE(dude->arms.rWristDirection, forwardVect);
                VMOVE(dude->legs.lLegDirection, forwardVect);
                VMOVE(dude->legs.rLegDirection, forwardVect);
                VMOVE(dude->legs.lKneeDirection, downVect);
                VMOVE(dude->legs.rKneeDirection, downVect);
                VMOVE(dude->legs.lFootDirection, forwardVect);
                VMOVE(dude->legs.rFootDirection, forwardVect);
		break;
	case 3:
		bu_log("Making arms out (standing)\n");
                VMOVE(dude->arms.lArmDirection, leftVect);
                VMOVE(dude->arms.rArmDirection, rightVect);
                VMOVE(dude->arms.lElbowDirection, leftVect);
                VMOVE(dude->arms.rElbowDirection, rightVect);
                VMOVE(dude->arms.lWristDirection, leftVect);
                VMOVE(dude->arms.rWristDirection, rightVect);
                VMOVE(dude->legs.lLegDirection, downVect);
                VMOVE(dude->legs.rLegDirection, downVect);
                VMOVE(dude->legs.lKneeDirection, downVect);
                VMOVE(dude->legs.rKneeDirection, downVect);
                VMOVE(dude->legs.lFootDirection, forwardVect);
                VMOVE(dude->legs.rFootDirection, forwardVect);		
		break;
	case 4:
		bu_log("Making the Letterman\n");
		vect_t larm, rarm;
		VSET(larm, -32, 135, 0);
		VSET(rarm, 32, 135, 0);
                VMOVE(dude->arms.lArmDirection, larm);
                VMOVE(dude->arms.rArmDirection, rarm);
                VMOVE(dude->arms.lElbowDirection, larm);
                VMOVE(dude->arms.rElbowDirection, rarm);
                VMOVE(dude->arms.lWristDirection, larm);
                VMOVE(dude->arms.rWristDirection, rarm);
		vect_t lleg;
		VSET(lleg, 0, 75, 0);
                VMOVE(dude->legs.lLegDirection, lleg);
                VMOVE(dude->legs.rLegDirection, forwardVect);
		vect_t knee;
		VSET(knee, 90, 5, 0);
                VMOVE(dude->legs.lKneeDirection, knee);
                VMOVE(dude->legs.rKneeDirection, downVect);
                VMOVE(dude->legs.lFootDirection, forwardVect);
                VMOVE(dude->legs.rFootDirection, forwardVect);
		break;
	case 999:
		/* interactive position-setter-thingermajiger */
		/* manual() */
		break;
	default:
		break;
	}
	bu_log("Exiting stance maker\n");
}

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
	   "\t-s\t\tStance to take 0-Stand 1-Sit 2-Drive 3-Arms out 4-Letterman 999-Custom\n"
	);

    bu_vls_free(&str);
    return;
}

/* Process command line arguments */
int read_args(int argc, char **argv, struct human_data_t *dude, fastf_t *stance, fastf_t *troops, fastf_t *showBoxes)
{
    int c = 0;
    char *options="H:N:h:O:o:a:b:s:A";
    float height=0;
    int soldiers=0;
    int pose=0;

    /* don't report errors */
    bu_opterr = 0;

    fflush(stdout);
    while ((c=bu_getopt(argc, argv, options)) != EOF) {
	switch (c) {
	    case 'A':
		bu_log("AutoMode, making average man\n");
		dude->height = DEFAULT_HEIGHT_INCHES;
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
			dude->height = DEFAULT_HEIGHT_INCHES;
			bu_log("%.2f = height in inches\n", height);
		}
		else
		{
			dude->height = height;
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
				*stance = pose;
				break;
			case 1:
				bu_log("Sitting\n");
				*stance = pose;
				break;
			case 2:
				bu_log("Driving\n");
				*stance = pose;
				break;
			case 3:
				bu_log("Arms out\n");
				*stance = pose;
				break;
			case 4:
				bu_log("The Letterman\n");
				*stance = pose;
				break;
			/* Custom case */
			case 999:
				bu_log("Custom\n");
				*stance = pose;
				break;
			default:
				bu_log("Bad Pose, default to stand\n");
				pose=0;
				*stance=0;
				break;
		}
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
    struct human_data_t human_data;
    human_data.height = DEFAULT_HEIGHT_INCHES;
    fastf_t showBoxes = 0, troops = 0, stance = 0;
    char suffix[MAXLENGTH]= "";
    point_t location;
    VSET(location, 0, 0, 0); /* Default standing location */
    bu_vls_init(&name);
    bu_vls_trunc(&name, 0);
    bu_vls_init(&str);
    bu_vls_trunc(&str, 0);

    /* Process command line arguments */
    read_args(ac, av, &human_data, &stance, &troops, &showBoxes);
    db_fp = wdb_fopen(filename);

/******MAGIC******/
/*Magically set pose, and apply pose to human geometry*/ 
    setStance(stance, &human_data); 
    if(!troops){
    	makeBody(db_fp, suffix, &human_data, location, showBoxes);
    }
    if(troops){
	makeArmy(db_fp, human_data, troops, showBoxes);
    }
/****End Magic****/

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
