/**
 * @memo	Simple example criteria for simplifying the vertex tree.
 * @name 	Standard callbacks: folding/unfolding nodes
 *
 * 		Routines for implementing standard criteria to decide when
 *		to fold and unfold nodes.  The VDS library allows users to
 *		plug in their own criteria, in the form of vdsFoldCriterion
 *		callbacks to the vdsAdjust* routines, but most users will
 *		probably want to use the standard criteria implemented here.<p>
 *
 * @see stdfold.c
 */
/*@{*/

#include <math.h>

#include "vds.h"
#include "stdvds.h"
#include "vector.h"

/** A simple vdsFoldCriterion callback based on screen-space node extent.
 * 		Takes a node in the vertex tree	and decides whether that
 *		node should be folded.
 *		The decision is based on the visibility and projected screen
 *		extent of the node's bounding volume
 * @return	1 if the node should be folded, 0 otherwise
 */
int vdsThresholdTest(const vdsNode *node)
{
    vdsVec3 D;			/* Vector from viewpt to node->center 	*/
    vdsFloat distance;		/* Length of vector D			*/
    vdsFloat invDistance;	/* 1/distance				*/
    vdsFloat theta;		/* Angle from V to D			*/
    vdsFloat phi;		/* Angle subtended by sphere		*/
    vdsFloat DdotV;		/* Dot product of D and V		*/

    VEC3_SUBTRACT(D, node->bound.center, vdsViewPt);
    distance = VEC3_LENGTH(D);
    invDistance = 1.0 / distance;

    if (distance < node->bound.radius) {
	return 0;			/* eyept is within node; UNFOLD	*/
    }
    phi = atan(node->bound.radius * invDistance);
    DdotV = - VEC3_DOT(D, vdsLookVec);
    theta = acos(DdotV * invDistance);

    if (theta - phi > vdsFOV / 2.0) {
	return 1;			/* node outside FOV cone; FOLD	*/
    }
    /*
     * Node intersects FOV cone; fold if screenspace extent < threshold.
     * For speed, approximate the fraction f of screen extent covered by node
     * as f = node->radius / (distance * tan FOV), and precalculate 1/tan FOV.
     */
    if (node->bound.radius * vdsInvTanFOV * invDistance < vdsThreshold) {
	return 1;			/* node smaller than threshold; FOLD */
    }
    return 0;				/* larger than threshold; UNFOLD */
}

/*@}*/
/***************************************************************************\

  Copyright 1999 The University of Virginia.
  All Rights Reserved.

  Permission to use, copy, modify and distribute this software and its
  documentation without fee, and without a written agreement, is
  hereby granted, provided that the above copyright notice and the
  complete text of this comment appear in all copies, and provided that
  the University of Virginia and the original authors are credited in
  any publications arising from the use of this software.

  IN NO EVENT SHALL THE UNIVERSITY OF VIRGINIA
  OR THE AUTHOR OF THIS SOFTWARE BE LIABLE TO ANY PARTY FOR DIRECT,
  INDIRECT, SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES, INCLUDING
  LOST PROFITS, ARISING OUT OF THE USE OF THIS SOFTWARE AND ITS
  DOCUMENTATION, EVEN IF THE UNIVERSITY OF VIRGINIA AND/OR THE
  AUTHOR OF THIS SOFTWARE HAVE BEEN ADVISED OF THE POSSIBILITY OF
  SUCH DAMAGES.

  The author of the vdslib software library may be contacted at:

  US Mail:             Dr. David Patrick Luebke
		       Department of Computer Science
		       Thornton Hall, University of Virginia
		       Charlottesville, VA 22903

  Phone:               (804)924-1021

  EMail:               luebke@cs.virginia.edu

\*****************************************************************************/
