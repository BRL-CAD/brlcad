/*
 *                 V E G I T A T I O N . H
 *
 *      This is the header file to the program that generages geometry
 *      that resembles or approximates a plant.  More specifically, 
 *      the generator is geared towards generating trees and shrubbery.
 *      The plants are generated based on specification of growth
 *      parameters such as growth and branching rates.
 *
 *  Author -
 *      Christopher Sean Morrison
 *  
 *  Source -
 *      The U. S. Army Research Laboratory
 *      Aberdeen Proving Ground, Maryland  21005-5068  USA
 *  
 *  Distribution Notice -
 *      Re-distribution of this software is restricted, as described in
 *      your "Statement of Terms and Conditions for the Release of
 *      The BRL-CAD Package" agreement.
 *
 *  Copyright Notice -
 *      This software is Copyright (C) 1998 & 1999 by the United States
 *      Army in all countries except the USA.  All rights reserved.
 */
#ifndef __VEGITATION_H__
#define __VEGITATION_H__

#include <stdio.h>
#include <limits.h>
#include <math.h>
#include <time.h>

#include "conf.h"
#include "machine.h"
#include "bu.h"
#include "vmath.h"
#include "bn.h"
#include "raytrace.h"
#include "wdb.h"

#define ZERO_TOLERANCE 0.000005

typedef struct growthSegment {
  unsigned long id;
  point_t position;
  vect_t direction;
  double length;
  double startRadius;
  double endRadius;
  unsigned int age;
} growthSegment_t;
#define INIT_GROWTHSEGMENT_T(_i) { (_i)->id=0; VSETALL((_i)->position, 0.0); VSETALL((_i)->direction, 0.0); (_i)->length = 0.0; (_i)->startRadius = 0.0; (_i)->endRadius = 0.0; (_i)->age = 0; }  


/* not really a list (it is a container), but close enough */
typedef struct segmentList {
  unsigned int capacity;
  unsigned int count;
  growthSegment_t **segment;
} segmentList_t;
#define INIT_GROWTHSEGMENTLIST_T(_i) { (_i)->capacity = 0; (_i)->count = 0; (_i)->segment=NULL; }


typedef struct structure {
  int subStructureCapacity;
  int subStructureCount;
  struct structure **subStructure;
  unsigned int age;
  int segmentCapacity;
  int segmentCount;
  growthSegment_t **segment;
} structure_t;
#define INIT_STRUCTURE_T(_i) { (_i)->subStructureCapacity = 0; (_i)->subStructureCount = 0; (_i)->subStructure=NULL; (_i)->age=0; (_i)->segmentCapacity=0; (_i)->segmentCount=0; (_i)->segment=NULL; }


#define TRUE 1
#define FALSE 0

typedef unsigned char bool;

typedef struct growthPoint {
  bool alive;
  unsigned int growthEnergy;
  int growthEnergyDelta; /* amount growth energy changes per age */
  double length;
  double radius;
  double lengthDecay;
  double radiusDecay;
  point_t position;
  vect_t direction;
  unsigned int age;
  structure_t *structure; /* which structure are we growing on */
} growthPoint_t;
#define INIT_GROWTHPOINT_T(_i) { (_i)->alive=TRUE; (_i)->growthEnergy=0; (_i)->growthEnergyDelta=0; (_i)->length=0.0; (_i)->radius=0.0; (_i)->lengthDecay=0.0; (_i)->radiusDecay=0.0; VSETALL((_i)->position, 0.0); VSETALL((_i)->direction, 0.0); (_i)->age=0; (_i)->structure = NULL; }

/* XXX not really a list -- it is a container object for a list */
typedef struct growthPointList {
  unsigned int capacity;
  unsigned int count;
  growthPoint_t **point;
} growthPointList_t;
#define INIT_GROWTHPOINTLIST_T(_i) { (_i)->capacity = 0; (_i)->count = 0; (_i)->point=NULL; }


typedef struct characteristic {
  double totalHeight; /* ideal height */
  double totalRadius; /* ideal bounding radius */

  double minLength;
  double minRadius;
  double lengthMinVariation; 
  double lengthMaxVariation; 
  double radiusMinVariation; 
  double radiusMaxVariation; 

  vect_t dirMinVariation; /* new segments are at least this delta from the growth direction */
  vect_t dirMaxVariation; /* new segments are no greater than this delta from the growth direction */
  
  double branchingRate;  /* probability that a segment will branch per timestep */
  double branchAtEndpointRate; /* probability to branch on a segment end point */
  unsigned int minBranchingAge; /* segments younger than this cannot branch */
  unsigned int maxBranchingAge; /* segments older than this cannot branch */

  unsigned int regrowthAttempts; /* how many times to keep trying to grow when blocked */
  unsigned int growthEnergy; /* how much it groes per timestep (and potentially branches) */
  
  vect_t branchMinVariation;
  vect_t branchMaxVariation;

  double dyingRate;  /* rate at which a segment will fall off */
  unsigned int dyingAge; /* segments older than this may fall off (they are dead) */

  double lengthDecayRate; /* rate at which the length decays */
  double radiusDecayRate; /* rate at which the radius decays */
  

} characteristic_t;
#define INIT_CHARACTERISTIC_T(_i) { (_i)->totalHeight = (_i)->totalRadius = (_i)->minLength = (_i)->minRadius = (_i)->lengthMinVariation = (_i)->lengthMaxVariation = (_i)->radiusMinVariation = (_i)->radiusMaxVariation = 0.0; VSETALL((_i)->dirMinVariation, 0.0); VSETALL((_i)->dirMaxVariation, 0.0); (_i)->branchingRate = (_i)->branchAtEndpointRate = 0.0, (_i)->minBranchingAge = (_i)->maxBranchingAge = (_i)->regrowthAttempts = (_i)->growthEnergy = 0; VSETALL((_i)->branchMinVariation, 0.0); VSETALL((_i)->dirMaxVariation, 0.0); (_i)->dyingRate = 0.0, (_i)->dyingAge = 0; (_i)->lengthDecayRate = (_i)->radiusDecayRate = 0.0; }


typedef struct plant {
  /* static plant properties */
  point_t position;
  double radius; /* inital base trunk radius */
  vect_t direction; /* general initial growth direction */

  /* variable plant properties */
  characteristic_t *characteristic; 
  unsigned int age;

  /* dynamic plant structure */
  structure_t *structure;
  growthPointList_t *growth;

  unsigned long segmentCount;
} plant_t;
#define INIT_PLANT_T(_i) { VSETALL((_i)->position, 0.0); (_i)->radius=0.0; VSET((_i)->direction, 0.0, 0.0, 1.0); (_i)->characteristic = NULL; (_i)->age=0; (_i)->structure = NULL; (_i)->growth = NULL; (_i)->segmentCount = 0; }


#define MAX_STRING_LENGTH 128

typedef struct outputCounter {
  unsigned int primitives; 
  unsigned int combinations;
  struct wmember combination;
  char name[MAX_STRING_LENGTH];
  struct wmember region;
  char plantName[MAX_STRING_LENGTH];
} outputCounter_t;
#define INIT_OUTPUTCOUNTER_T(_i) { (_i)->primitives=0; (_i)->combinations=0; BU_LIST_INIT(&((_i)->combination).l); sprintf((_i)->name, "XXX"); BU_LIST_INIT(&((_i)->region).l); snprintf((_i)->plantName, MAX_STRING_LENGTH, "plant.r"); }


#endif /* __VEGITATION_H__ */
