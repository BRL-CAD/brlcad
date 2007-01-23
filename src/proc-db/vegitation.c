/*                    V E G I T A T I O N . C
 * BRL-CAD
 *
 * Copyright (c) 1998-2007 United States Government as represented by
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
 */
/** @file vegitation.c
 *
 *      This is for a program that generages geometry that resembles or
 *      approximates a plant.  More specifically, the generator is geared
 *      towards generating trees and shrubbery.  The plants are generated
 *      based on specification of growth parameters such as growth and
 *      branching rates.
 *
 *      The plant is composed of a number of "particle" primitives (which
 *      is effectively branch growth segments and curved ball joints). The
 *      plant is generated recursively with random probabilities (there is
 *      a seed that may be set for repeatability) and growth parameters.
 *
 *  Author -
 *      Christopher Sean Morrison
 *
 *  Source -
 *      The U. S. Army Research Laboratory
 *      Aberdeen Proving Ground, Maryland  21005-5068  USA
 *
 ***********************************************************************/

#include "./vegitation.h"


static void ageStructure(structure_t *structure) {
  int i;

  /*
    printf("Aging structure\n");
  */

  for (i=0; i < structure->subStructureCount; i++) {
    ageStructure(structure->subStructure[i]);
  }
  structure->age++;
  for (i=0; i < structure->segmentCount; i++) {
    structure->segment[i]->age++;
  }

  return;
}


static int getSegmentCount(structure_t *structure, unsigned int minAge, unsigned int maxAge) {
  int i;
  int total;

  for (total=i=0; i < structure->subStructureCount; i++) {
    total += getSegmentCount(structure->subStructure[i], minAge, maxAge);
  }
  if ((structure->age >= minAge) && (structure->age <= maxAge)) {
    for (i = 0; i < structure->segmentCount; i++) {
      if ((structure->segment[i]->age >= minAge) && (structure->segment[i]->age <= maxAge)) {
	total++;
      }
    }
  }

  return total;
}


/* used
 * http://geometryalgorithms.com/Archive/algorithm_0106/algorithm_0106.htm#dist3D_Segment_to_Segment()
 * as reference */
static float segmentToSegmentDistance( const point_t S1P0, const point_t S1P1, const point_t S2P0, const point_t S2P1) {
  vect_t u;
  vect_t v;
  vect_t w;
  float a;
  float b;
  float c;
  float d;
  float e;
  float D;
  float sc, sN, sD;
  float tc, tN, tD;
  vect_t dP;

  VSUB2(u, S1P1, S1P0);
  VSUB2(v, S2P1, S2P0);
  VSUB2(w, S1P0, S2P0);
  a = VDOT(u,u);  /* always >= 0 */
  b = VDOT(u,v);
  c = VDOT(v,v);  /* always >= 0 */
  d = VDOT(u,w);
  e = VDOT(v,w);
  D = a*c - b*b;  /* always >= 0 */
  sc = sN = sD = D;  /* sc = sN / sD, default sD = D >= 0 */
  tc = tN = tD = D;  /* tc = tN / tD, default tD = D >= 0 */


  /* compute the line parameters of the two closest points */
  if (D < ZERO_TOLERANCE) { /* the lines are almost parallel */
    sN = 0.0;
    tN = e;
    tD = c;
  }
  else {  /* get the closest points on the infinite lines */
    sN = (b*e - c*d);
    tN = (a*e - b*d);
    if (sN < 0) {  /* sc < 0 => the s=0 edge is visible */
      sN = 0.0;
      tN = e;
      tD = c;
    }
    else if (sN > sD) { /* sc > 1  => the s=1 edge is visible */
      sN = sD;
      tN = e + b;
      tD = c;
    }
  }

  if (tN < 0) {  /* tc < 0 => the t=0 edge is visible */
    tN = 0.0;
    /* recompute sc for this edge */
    if (-d < 0)
      sN = 0.0;
    else if (-d > a)
      sN = sD;
    else {
      sN = -d;
      sD = a;
    }
  }
  else if (tN > tD) { /* tc > 1  => the t=1 edge is visible */
    tN = tD;
    /* recompute sc for this edge */
    if ((-d + b) < 0)
      sN = 0;
    else if ((-d + b) > a)
      sN = sD;
    else {
      sN = (-d +  b);
      sD = a;
    }
  }
  /* finally do the division to get sc and tc */
  sc = sN / sD;
  tc = tN / tD;

  /* get the difference of the two closest points */
  VCOMB2(dP, 1.0, w, sc, u);
  VSCALE(v, v, tc);
  VSUB2(dP, dP, v);

  return MAGNITUDE(dP); /* return the closest distance */
}


static segmentList_t *findIntersectors(const growthSegment_t * const segment, const structure_t * const structure, const segmentList_t * const exemptList) {
  int i, j;
  segmentList_t *bigList = NULL;
  segmentList_t *segList = NULL;
  double maxFromRadius = 0.0;
  double maxOntoRadius = 0.0;
  int skip;

  if (segment == NULL) {
    fprintf(stderr, "Cannot find intersectors for null segment\n");
    return NULL;
  }
  if (structure == NULL) {
    fprintf(stderr, "Cannot find intersectors within null structure\n");
    return NULL;
  }
  if (bigList == NULL) {
    bigList = (segmentList_t *)bu_calloc(1, sizeof(segmentList_t), "bigList");
    INIT_GROWTHSEGMENTLIST_T(bigList);

    /* allocate 10 initial segment slots */
    bigList->segment = (growthSegment_t **)bu_calloc(10, sizeof(growthSegment_t *), "bigList->segment");
    bigList->capacity = 10;
  }

  for (i=0; i < structure->subStructureCount; i++) {
    segList = findIntersectors(segment, structure->subStructure[i], exemptList);

    /* ensure we have enough room */
    if (bigList->count + segList->count + 1 >= bigList->capacity) {
      bigList->segment = (growthSegment_t **)bu_realloc(bigList->segment, (bigList->count + segList->count + 10) * sizeof(growthSegment_t *), "bigList->segment");
      bigList->capacity = bigList->count + segList->count + 10; /* a few extra so not to do this so often */
    }

    /* add the individual items */
    for (j=0; j < segList->count; j++) {
      bigList->segment[bigList->count] = segList->segment[j];
      bigList->count++;
    }

    /* release the returned resource */
    if (segList != NULL) {
      if (segList->capacity > 0) {
	free(segList->segment);
	segList->segment = NULL;
      }
      free(segList);
      segList = NULL;
    }
  }

  /* only worry about the "general" area of the segment in question */
  maxFromRadius = segment->startRadius > segment->endRadius ? segment->startRadius : segment->endRadius;

  /* iterate over the segments seeing if we intersect anything in the structure segment list */
  for (i=0; i < structure->segmentCount; i++) {
    double distance;
    point_t endPoint;
    point_t endOntoPoint;

    /* skip segments that are in exemption list */
    if (exemptList != NULL) {

      for (skip = j = 0; (j < exemptList->count) && (skip == 0) ; j++) {
	if (structure->segment[i]->id == exemptList->segment[j]->id) {
	  /*	  printf("Found exempt segment with id %ld\n", exemptList->segment[j]->id); */
	  skip = 1;
	  continue;
	}
      }
      /* end iteration over exemption list */

      /* see if we got lucky */
      if (skip != 0) {
	continue;
      }
    }
    /* end check for exemption list */

    /* only worry about the "general" area of the segment in question */
    maxOntoRadius = structure->segment[i]->startRadius > structure->segment[i]->endRadius ? structure->segment[i]->startRadius : structure->segment[i]->endRadius;

    /* find the shortest distance between the two segments */
    VCOMB2(endPoint, 1.0, segment->position, segment->length, segment->direction);
    VCOMB2(endOntoPoint, 1.0, structure->segment[i]->position, structure->segment[i]->length, structure->segment[i]->direction);
    distance = segmentToSegmentDistance(segment->position, endPoint, structure->segment[i]->position, endOntoPoint);

    /*
    printf("distance is %f\n", distance);
    */
    if (distance < (maxFromRadius + maxOntoRadius)) {
      if (bigList->count >= bigList->capacity) {
	  bigList->segment = (growthSegment_t **)bu_realloc(bigList->segment, (bigList->count + 10) * sizeof(growthSegment_t *), "bigList->segment");
	  bigList->capacity = bigList->count + 10;
      }
      bigList->segment[bigList->count] = structure->segment[i];
      bigList->count++;
    }
    /* end check for within distance */
  }
  /* end iteration over segments */

  return bigList;
}


static int branchWithProbability(plant_t *plant, structure_t* structure, unsigned int minAge, unsigned int maxAge, double probability) {
  int i;
  int total;

  /* make sure there is something to do.. */
  if (probability <= 0.0) {
    return 0;
  }

  for (total=i=0; i < structure->subStructureCount; i++) {
    total += branchWithProbability(plant, structure->subStructure[i], minAge, maxAge, probability);
  }
  /* do NOT check for the max age as there usually are young segments that can spawn branches */
  if (structure->age >= minAge) {
    for (i = 0; i < structure->segmentCount; i++) {
      if ((structure->segment[i]->age >= minAge) && (structure->segment[i]->age <= maxAge)) {

	/* see if we branch */
	if (drand48() <= probability) {
	  double branchPoint, branchPointRadius;
	  vect_t direction;
	  growthPoint_t *newGrowthPoint;

	  /* decide whether or not to use an endpoint */
	  if (drand48() <= plant->characteristic->branchAtEndpointRate) {
	    /* randomly pick between the two points evenly */
	    if (drand48() < 5.0) {
	      branchPoint = 0.0;
	      branchPointRadius = structure->segment[i]->startRadius;
	    } else {
	      branchPoint = structure->segment[i]->length;
	      branchPointRadius = structure->segment[i]->endRadius;
	    }
	    /*
	      printf("branching on endpoint: %f  with radius: %f\n", branchPoint, branchPointRadius);
	    */
	  } else {

	    /* pick a point on the segment to branch from */
	    branchPoint = drand48() * (double)structure->segment[i]->length;
	    /*
	      printf("branching between %f and %f at %f\n", 0.0, structure->segment[i]->length,  branchPoint);
	    */

	    /* detect increasing branches
	     *  if (structure->segment[i]->startRadius > structure->segment[i]->endRadius) {
	     *  minRadius = structure->segment[i]->endRadius;
	     *  maxRadius = structure->segment[i]->startRadius;
	     *  } else {
	     *  minRadius = structure->segment[i]->startRadius;
	     *  maxRadius = structure->segment[i]->endRadius;
	     *  }
	    */
	    /*	    branchPointRadius = ((branchPoint / structure->segment[i]->length) * (maxRadius - minRadius)) + minRadius; */
	    branchPointRadius = ((branchPoint / structure->segment[i]->length) * (structure->segment[i]->endRadius - structure->segment[i]->startRadius)) + structure->segment[i]->startRadius;
	    /*
	      printf("branch point radius: %f (between %f and %f)\n", branchPointRadius, structure->segment[i]->startRadius, structure->segment[i]->endRadius);
	    */
	  }
	  /* end endpoint decision */

	  /* figure out a direction to grow */
	  VMOVE(direction, structure->segment[i]->direction);
	  direction[X] += (drand48() * (plant->characteristic->branchMaxVariation[X] - plant->characteristic->branchMinVariation[X])) + plant->characteristic->branchMinVariation[X];
	  direction[Y] += (drand48() * (plant->characteristic->branchMaxVariation[Y] - plant->characteristic->branchMinVariation[Y])) + plant->characteristic->branchMinVariation[Y];
	  direction[Z] += (drand48() * (plant->characteristic->branchMaxVariation[Z] - plant->characteristic->branchMinVariation[Z])) + plant->characteristic->branchMinVariation[Z];
	  VUNITIZE(direction);
	  /*
	    VPRINT("New Growth Direction: ", direction);
	  */

	  /* create and fill in the the growth point */
	  newGrowthPoint = (growthPoint_t *)bu_calloc(1, sizeof(growthPoint_t), "newGrowthPoint");
	  INIT_GROWTHPOINT_T(newGrowthPoint);
	  newGrowthPoint->growthEnergy = plant->characteristic->growthEnergy;

	  /* length and radius is based off of the segment we grew off of -- random initial length */
	  /* !!! just  use the prior length  until working !!! */
	  newGrowthPoint->length = structure->segment[i]->length - (structure->segment[i]->length * plant->characteristic->lengthDecayRate); /* * drand48(); */

	  /* starting radius is exactly in line with where on the segment we start from */
	  newGrowthPoint->radius = branchPointRadius;

	  newGrowthPoint->lengthDecay = plant->characteristic->lengthDecayRate;
	  newGrowthPoint->radiusDecay = plant->characteristic->radiusDecayRate;

	  VCOMB2(newGrowthPoint->position, 1.0, structure->segment[i]->position, branchPoint, structure->segment[i]->direction);
	  VMOVE(newGrowthPoint->direction, direction);
	  newGrowthPoint->age = 0;

	  /* associate the point with a new structure */
	  newGrowthPoint->structure = (structure_t *)bu_calloc(1, sizeof(structure_t), "newGrowthPoint->structure");
	  INIT_STRUCTURE_T(newGrowthPoint->structure);

	  /* make sure the growth point is linked back to his parent branch */
	  if (structure->subStructureCount >= structure->subStructureCapacity) {
	      structure->subStructure = (structure_t **)bu_realloc(structure->subStructure, (structure->subStructureCount + 10) * sizeof(structure_t *), "structure->subStructure");
	      structure->subStructureCapacity += 10;
	  }
	  structure->subStructure[structure->subStructureCount] = newGrowthPoint->structure;
	  structure->subStructureCount++;

	  /* see if we need to add more room for growth points */
	  if (plant->growth->count >= plant->growth->capacity) {
	      plant->growth->point = (growthPoint_t **)bu_realloc(plant->growth->point, (plant->growth->capacity + 10) * sizeof(growthPoint_t *), "plant->growth->point");
	      plant->growth->capacity += 10;
	  }

	  plant->growth->point[plant->growth->count] = newGrowthPoint;
	  plant->growth->count++;

	  total++;

	} else {
	  /* no branch */
	}
	/* end want to branch block */
      }
      /* end check for proper age */
    }
    /* end iteration over segments */
  }
  /* end check for proper segment age */

  return total;

}

static void branchGrowthPoints(plant_t *plant) {
  int totalSegments;
  double segmentProbability;

  if (!plant->structure) {
    printf("No structure defined yet -- cannot branch\n");
    return;
  }

  /* is there a probability of actually branching */
  if (plant->characteristic->branchingRate > 0.0) {

    /* is the plant even capable of branching yet?
     * we do not check the max age just because we do not know the youngest segment
     */
    if (plant->characteristic->minBranchingAge <= plant->structure->age) {

      /* take the overall rate divided by the number of segments to determine a segment rate */
      totalSegments = getSegmentCount(plant->structure, plant->characteristic->minBranchingAge, plant->characteristic->maxBranchingAge);
      if (totalSegments <= 0) {
	segmentProbability = 0;
      } else {
	segmentProbability=plant->characteristic->branchingRate; /* !!! / totalSegments; */
      }
      printf("age: %d\nsegmentCount: %d\nsegmentProbabilty: %f\n", plant->structure->age, totalSegments, segmentProbability);

      totalSegments = branchWithProbability(plant, plant->structure, plant->characteristic->minBranchingAge, plant->characteristic->maxBranchingAge, segmentProbability);
      /*
	printf("branched %d times\n", totalSegments);
      */
    }
  }

}


static void growPlant(plant_t *plant) {
  int i;
  int growthSteps;
  int retryCount;
  growthSegment_t *segment;
  growthPoint_t *point;

  printf ("Growing plant\n");

  /* recursive call -- increase the age of every segment */
  ageStructure(plant->structure);

  branchGrowthPoints(plant);

  /* begin iteration over growth points */
  for (i = 0; i < plant->growth->count; i++) {
    point = plant->growth->point[i];

    /* make sure the point is not "dead" */
    if (!point->alive) {
      continue;
    }

    /* make sure this growth point is not too small */
    if (NEAR_ZERO(point->radius, ZERO_TOLERANCE)) {
      point->alive=FALSE;
      continue;
    }

    /* begin iteration over single age growth steps */
    for (growthSteps = 0; growthSteps < point->growthEnergy; growthSteps++) {
      vect_t newGrowthDirection;
      segmentList_t *excluded;
      segmentList_t *included;
      growthSegment_t startingPoint;

      /* keep checking if the point is still alive since it dies when we intersect */
      if (!point->alive) {
	continue;
      }

      segment = (growthSegment_t *)bu_calloc(1, sizeof(growthSegment_t), "segment");

      INIT_GROWTHSEGMENT_T(segment);
      VMOVE(segment->position, point->position);
      VMOVE(segment->direction, point->direction);
      segment->age = 0;
      segment->length = point->length;
      segment->startRadius = point->radius; /* must start with same radius to match up properly with previous segment */

      /* move and decay growth point */
      VCOMB2(point->position, 1.0, point->position, segment->length, point->direction);

      point->length -= point->length * point->lengthDecay;
      /* jitter next segment length */
      if (!NEAR_ZERO(plant->characteristic->lengthMaxVariation - plant->characteristic->lengthMinVariation, ZERO_TOLERANCE)) {
	point->length += (drand48() * (plant->characteristic->lengthMaxVariation - plant->characteristic->lengthMinVariation)) + plant->characteristic->lengthMinVariation;
      }
      /* clamp the length to positive values */
      if (point->length < ZERO_TOLERANCE) {
	point->length = 0.0;
      }
      /* clamp the length to the minimum value */
      if (point->length < plant->characteristic->minLength) {
	point->length = plant->characteristic->minLength;
      }

      point->radius -= point->radius * point->radiusDecay;
      /* jitter next radius */
      if (!NEAR_ZERO(plant->characteristic->radiusMaxVariation - plant->characteristic->radiusMinVariation, ZERO_TOLERANCE)) {
	point->radius += (drand48() * (plant->characteristic->radiusMaxVariation - plant->characteristic->radiusMinVariation)) + plant->characteristic->radiusMinVariation;
      }
      /* clamp the radius to positive values */
      if (point->radius < ZERO_TOLERANCE) {
	point->radius = 0.0;
      }
      /* clamp the radius to the minimum width */
      if (point->radius < plant->characteristic->minRadius) {
	point->radius = plant->characteristic->minRadius;
      }

      /* jitter the growth direction */
      VSUB2(newGrowthDirection, plant->characteristic->dirMaxVariation, plant->characteristic->dirMinVariation);
      if (!VNEAR_ZERO(newGrowthDirection, ZERO_TOLERANCE)) {
	point->direction[X] += (drand48() * newGrowthDirection[X]) + plant->characteristic->dirMinVariation[X];
	point->direction[Y] += (drand48() * newGrowthDirection[Y]) + plant->characteristic->dirMinVariation[Y];
	point->direction[Z] += (drand48() * newGrowthDirection[Z]) + plant->characteristic->dirMinVariation[Z];
	VUNITIZE(point->direction);
	/*	VPRINT("Point direction: ", point->direction); */
      }

      segment->endRadius = point->radius;

      /* done creating the new segment -- check it for intersection validity */
      INIT_GROWTHSEGMENT_T(&startingPoint);
      /* fill in a dummy segment to find out what we are initially intersecting with (they are ok) */
      VMOVE(startingPoint.position, segment->position);
      VMOVE(startingPoint.direction, segment->direction);
      startingPoint.length = ZERO_TOLERANCE; /* important to be very short */
      startingPoint.startRadius = segment->startRadius;
      startingPoint.endRadius = segment->startRadius;  /* the other important one => make same as start */
      excluded = findIntersectors(&startingPoint, plant->structure, NULL);
      included = findIntersectors(segment, plant->structure, excluded);

      if (included == NULL || included->count > 0) {
	/* test for failure and try to retry  */
	for (retryCount = 0; retryCount < plant->characteristic->regrowthAttempts; retryCount++) {

	  VMOVE(segment->direction, point->direction);

	  INIT_GROWTHSEGMENT_T(&startingPoint);
	  VMOVE(startingPoint.position, segment->position);
	  VMOVE(startingPoint.direction, segment->direction);
	  startingPoint.length = ZERO_TOLERANCE;
	  startingPoint.startRadius = segment->startRadius;
	  startingPoint.endRadius = segment->startRadius;

	  included = findIntersectors(segment, plant->structure, excluded);

	  /* jitter the growth direction */
	  VSUB2(newGrowthDirection, plant->characteristic->dirMaxVariation, plant->characteristic->dirMinVariation);
	  if (!VNEAR_ZERO(newGrowthDirection, ZERO_TOLERANCE)) {
	    point->direction[X] += (drand48() * newGrowthDirection[X]) + plant->characteristic->dirMinVariation[X];
	    point->direction[Y] += (drand48() * newGrowthDirection[Y]) + plant->characteristic->dirMinVariation[Y];
	    point->direction[Z] += (drand48() * newGrowthDirection[Z]) + plant->characteristic->dirMinVariation[Z];
	    VUNITIZE(point->direction);
	    /*	VPRINT("Point direction: ", point->direction); */
	  }

	  if (included != NULL) {
	    if (included->capacity > 0) {
	      free(included->segment);
	      included->segment = (growthSegment_t **)NULL;
	      included->capacity = 0;
	    }
	    if (included->count > 0) {
	      free(included);
	      included=(segmentList_t *)NULL;
	    } else {
	      /*
		printf("successfull regrowth attempt\n");
	      */
	      printf(".");
	      break;
	    }
	  }
	}
	/* end iteration of regrowth attempts */
      }

      if (excluded != NULL) {
	/*
	  if (excluded->count > 0) {
	  printf("found %d segments at start point\n", excluded->count);
	  }
	*/
	if (excluded->capacity > 0) {
	  free(excluded->segment);
	  excluded->segment = (growthSegment_t **)NULL;
	  excluded->capacity = 0;
	}
	free(excluded);
	excluded=(segmentList_t *)NULL;
      }
      /*
      if (included != NULL) {
	if (included->capacity > 0) {
	  free(included->segment);
	  included->capacity = NULL;
	}
	if (included->count > 0) {
	  free(included);
	  included=NULL;

	  point->alive = FALSE;
	  continue;
	} else {
	  free(included);
	  included=NULL;
	}
      }

      */


      /* what if there is no structure yet? -- make one */
      if (point->structure == NULL) {
	  plant->structure = (structure_t *)bu_calloc(1, sizeof(structure_t), "plant->structure");
	  INIT_STRUCTURE_T(plant->structure);
      }

      /* add segment to list of segments for this growth point structure*/
      if ( point->structure->segmentCount >= point->structure->segmentCapacity ) {
	  point->structure->segment = (growthSegment_t **)bu_realloc(point->structure->segment, (point->structure->segmentCapacity + 10) * sizeof(growthSegment_t *), "point->structure->segment");
	  point->structure->segmentCapacity+=10;
      }

      segment->id = plant->segmentCount++;
      point->structure->segment[point->structure->segmentCount] = segment;
      point->structure->segmentCount++;


    } /* end growthStep iteration */
    /* still going in same direction */
    point->age++;

  } /* end growth point iteration */

  return;
}


static plant_t *createPlant(unsigned int age, vect_t position, double radius, vect_t direction, characteristic_t *characteristic ) {
  plant_t *plant;

  /* List of growth points */
  growthPointList_t *gpoints;

  /*
    printf ("Creating a plant at %f %f %f of age %d\n", position[X], position[Y], position[Z], age);
  */

  if (age == 0) {
    fprintf(stderr, "A plant with no age does not exist\n");
    return (plant_t *)NULL;
  }

  /* not our responsibility to release this sucker in here -- it is what we return */
  plant = (plant_t *)bu_calloc(1, sizeof(plant_t), "plant");
  INIT_PLANT_T(plant);
  VMOVE(plant->position, position);
  plant->radius = radius;
  VMOVE(plant->direction, direction);
  plant->characteristic = characteristic;

  plant->structure = (structure_t *)bu_calloc(1, sizeof(structure_t), "plant->structure");
  INIT_STRUCTURE_T(plant->structure);
  plant->structure->segment = (growthSegment_t **)bu_calloc(1, sizeof(growthSegment_t *), "plant->structure->segment");
  plant->structure->segmentCapacity=1;

  plant->structure->subStructure = (structure_t **)bu_calloc(1, sizeof(structure_t *), "plant->structure->subStructure");
  plant->structure->subStructureCapacity=1;

  gpoints = (growthPointList_t *)bu_calloc(1, sizeof(growthPointList_t), "gpoints");
  INIT_GROWTHPOINTLIST_T(gpoints);

  gpoints->point = (growthPoint_t **)bu_calloc(10, sizeof(growthPoint_t *), "gpoints->point");
  gpoints->capacity=10;
  gpoints->point[0] = (growthPoint_t *)bu_calloc(1, sizeof(growthPoint_t), "gpoints->point[0]");
  gpoints->count=1;
  INIT_GROWTHPOINT_T(gpoints->point[0]);

  gpoints->point[0]->growthEnergy = 1;
  gpoints->point[0]->growthEnergyDelta = 0;
  gpoints->point[0]->length = characteristic->totalHeight / age; /* XXX ideal max */
  gpoints->point[0]->radius = radius;
  gpoints->point[0]->lengthDecay = characteristic->lengthDecayRate;
  gpoints->point[0]->radiusDecay = characteristic->radiusDecayRate;
  VMOVE(gpoints->point[0]->position, position);
  VMOVE(gpoints->point[0]->direction, direction);
  gpoints->point[0]->age = 0;
  gpoints->point[0]->structure = plant->structure;

  plant->growth = gpoints;

  /* grow plant */
  for (plant->age = 0; plant->age < age; plant->age++) {
    printf("plant age is %d\n", plant->age);
    /* recursive call to build the tree per time step */
    growPlant(plant);
    /*  trimPlant(plant);  */ /* kill off dead limbs */
  }

  return plant;
}


static int writeStructureToDisk(struct rt_wdb *fp, structure_t *structure, outputCounter_t *oc) {
  int i;
  vect_t height;

  for (i=0; i < structure->segmentCount; i++) {
    snprintf(oc->name, MAX_STRING_LENGTH, "seg%d_%d.s", oc->combinations, oc->primitives);
    /*    if (mk_cone(fp, oc->name, structure->segment[i]->position, structure->segment[i]->direction, structure->segment[i]->length, structure->segment[i]->startRadius, structure->segment[i]->endRadius) != 0) { */
    VSCALE(height, structure->segment[i]->direction, structure->segment[i]->length);

    /* error check for bad primitive creation */
    if ( (structure->segment[i]->startRadius < 0.0) || (structure->segment[i]->endRadius < 0.0) || VNEAR_ZERO(height, ZERO_TOLERANCE) ) {
      fprintf(stderr, "Negative radius or height\n");
    }

    if (mk_particle(fp, oc->name, structure->segment[i]->position, height, structure->segment[i]->startRadius, structure->segment[i]->endRadius) != 0) {
      fprintf(stderr, "Unable to write segment to database\n");
      exit(2);
    }
    oc->primitives++;
    if (mk_addmember(oc->name, &(oc->combination.l), NULL, WMOP_UNION) == NULL) {
      fprintf(stderr, "Unable to add object to combination list\n");
      exit(3);
    }
  }
  snprintf(oc->name, MAX_STRING_LENGTH, "branch%d.c", oc->combinations);
  if (mk_lcomb(fp, oc->name, &(oc->combination), 0, NULL, NULL, NULL, 0) != 0) {
    fprintf(stderr, "Unable to write region to database\n");
    exit(2);
  }
  oc->combinations++;

  /* add combination to master region list */
  if (mk_addmember(oc->name, &(oc->region.l), NULL, WMOP_UNION) == NULL) {
    fprintf(stderr, "Unable to add combination to region list\n");
    exit(3);
  }

  for (i=0; i < structure->subStructureCount; i++) {
    writeStructureToDisk(fp, structure->subStructure[i], oc);
  }

  return 0;
}


static int writePlantToDisk(struct rt_wdb *fp, plant_t *plant) {
  outputCounter_t oc;

  printf ("Writing a plant at %f %f %f of age %d to disk\n", plant->position[X], plant->position[Y], plant->position[Z], plant->age);
  if (plant->structure == NULL) {
    printf("The plant has no structure\n");
    return 1;
  }

  /* begins recursive output */
  INIT_OUTPUTCOUNTER_T(&oc);
  writeStructureToDisk(fp, plant->structure, &oc);

  if (mk_lcomb(fp, oc.plantName, &(oc.region), 1, NULL, NULL, NULL, 0) != 0) {
    fprintf(stderr, "Unable to write region to database\n");
    exit(2);
  }

  return 0;
}


static void destroyPlant(plant_t *plant) {
  int i;

  /* get rid of the plant structure properly */
  if (plant != NULL) {
    if (plant->structure != NULL) {
      if (plant->structure->subStructureCount > 0) {
	for (i=0; i < plant->structure->subStructureCount; i++) {
	  free(plant->structure->subStructure[i]);
	}
	free(plant->structure->subStructure);
      }
      plant->structure->subStructure = NULL;

      if (plant->structure->segmentCount > 0) {
	for (i=0; i < plant->structure->segmentCount; i++) {
	  free(plant->structure->segment[i]);
	}
	free(plant->structure->segment);
      }
      plant->structure->segment = NULL;

      free(plant->structure);
      plant->structure = NULL;
    }
    if (plant->growth != NULL) {
      if (plant->growth->count > 0) {
	for (i=0; i < plant->growth->count; i++) {
	  plant->growth->point[i]->structure = NULL;
	  free(plant->growth->point[i]);
	}
	free(plant->growth->point);
      }
      plant->growth->point = NULL;
      free(plant->growth);
      plant->growth = NULL;
    }
    free(plant);
    plant = NULL;
  }

}

static int invalidCharacteristics(const characteristic_t * const c) {
  if (c->totalHeight <= 0.0) {
    fprintf(stderr, "Need positive plant height\n");
    return 1;
  }
  if (c->totalRadius <= 0.0) {
    fprintf(stderr, "Need positive plant radius\n");
    return 2;
  }
  if (c->minLength < 0.0) {
    fprintf(stderr, "Cannot have negative growth length\n");
    return 3;
  }
  if (c->minRadius < 0.0) {
    fprintf(stderr, "Cannot have negative growth radius\n");
    return 4;
  }
  if (c->branchingRate < 0.0) {
    fprintf(stderr, "Negative branching rate is meaninglss (same as 0.0 rate)\n");
  }
  if (c->branchAtEndpointRate < 0.0) {
    fprintf(stderr, "Negative branch at endpoint rate is meaningless (same as 0.0)\n");
  }
  return 0;
}


int main (int argc, char *argv[]) {

  struct rt_wdb *fp;
  plant_t *plant;
  characteristic_t c;

  /* all distance units are in mm */

  unsigned int age=20;  /* how many iterations */
  point_t position={0.0, 0.0, 0.0};
  double trunkRadius = 300.0;  /* how big is the start base */
  point_t direction={0.0, 0.0, 1.0}; /* positive z is "up" */
  double height = 30000.0;  /* about how tall is it */
  double branchingRate = 0.1;  /* 0->1 probability to branch per iteration */
  long seed;

  printf("Vegitation generator\n");
  printf("====================\n");

  if (argc > 1) {
    age = atoi(argv[1]);
  }
  printf("Growing for %d years\n", age);
  if (argc > 2) {
    height = atof(argv[2]);
  }
  printf("Growing to about %f meters in height\n", height / 1000);
  if (argc > 3) {
    trunkRadius = atof(argv[3]);
  }
  printf("Growing from a base width of %f meters\n", trunkRadius / 1000);
  if (argc > 4) {
    branchingRate = atof(argv[4]);
  }
  if (argc > 5) {
    seed = atol(argv[5]);
  }

  /* save the seed just in case we want to know it */
  seed=time(0);
#ifndef HAVE_SRAND48
  srand(seed);
#else
  srand48(seed);
#endif
  printf("Vegitation seed is %ld\n", seed);

  fp=wdb_fopen("vegitation.g");

  mk_id_units(fp, "Vegitation", "mm");

  INIT_CHARACTERISTIC_T(&c);
  c.totalHeight = height;
  c.totalRadius = height * 1.0;  /* set to ratio of width to height */

  c.minLength = 1.0; /* 1mm minimum branch length */
  c.minRadius = 1.0; /* 1mm minimum branch radius */
  c.lengthMinVariation = (c.totalHeight / age) * -0.1; /* -10% shorter than usual */
  c.lengthMaxVariation = (c.totalHeight / age) * 0.0;  /* 0% longer than usual */

  c.radiusMinVariation = (c.totalRadius / age) * -0.02;  /* maybe a tad smaller if < 0 */
  c.radiusMaxVariation = (c.totalRadius / age) * 0.00; /* will not get bigger if 0 */

  /* set these to the delta potential change of branch (e.g. 0.1 is +- 10% of value) */
  VSET(c.dirMinVariation, -0.2, -0.2, -0.2);
  VSET(c.dirMaxVariation, 0.2, 0.2, 0.2);

  c.branchingRate = branchingRate;
  c.minBranchingAge = 0;  /* minimum age of a segment before it may branch */
  c.maxBranchingAge = 50; /* branches older than this may not branch */
  c.branchAtEndpointRate = 0.1;  /* probability of branching on a joints (evergreens have this rate high)  */

  /* !!! bug in regrowth causing startpoint to shift -- do not set > 0 */
  c.regrowthAttempts=0;
  c.growthEnergy=1;  /* how far (iteration wise) for a branch to grow each life iteration */

  /* how inward or outwards the branches will go */
  VSET(c.branchMinVariation, -0.8, -0.8, 0.0); /* branches just cannot go "inward" */
  VSET(c.branchMaxVariation, 0.8, 0.8, 1.0);  /* branches cannot go "inwards" */

  c.dyingRate = 0.0;  /* unimplemnted */
  c.dyingAge = INT_MAX;  /* unimplemented */
  c.lengthDecayRate = 0.01; /* almost same length every year */
  c.radiusDecayRate = 0.15;  /* radius gets smaller by about 20% every year */

  if (invalidCharacteristics(&c)) {
    fprintf(stderr, "Invalid plant characteristics\n");
    exit(3);
  }

  if ((plant = createPlant(age, position, trunkRadius, direction, &c)) == NULL) {
    fprintf(stderr, "Unable to create plant\n");
    exit(1);
  }

  printf("There are %ld segments\n", plant->segmentCount);

  if (writePlantToDisk(fp, plant) != 0) {
    fprintf(stderr, "Unable to write plant to disk\n");
    exit(1);
  }

  destroyPlant(plant);

  wdb_close(fp);
  return 0;
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
