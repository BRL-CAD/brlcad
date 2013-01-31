/**
 * @memo	Simple example criteria for vertex tree visibility culling.
 * @name 	Standard callbacks: testing node visibility
 * 		Routines for classifying nodes against the view frustum.
 *		The user can supply custom routines for testing visibility
 *		(for example, if cell-portal culling was appropriate), but
 *		these standard routines will often suffice.<p>
 *
 * <b>Note</b>:	It may be possible to overlap the visibility and fold tests.
 *		The overlapped tests could then use the node->data field to
 *		store common intermediate results.  Since this depends on the
 *		particular tests used and on the overall system architecture
 *		(e.g., whether simplification and rendering are multithreaded),
 *		such overlapped tests are left for the user to construct.<p>
 *
 * @see		stdvis.c
 */
/*@{*/

#include <math.h>

#include "vds.h"
#include "stdvds.h"
#include "vector.h"

/** Test a node against the view frustum.
 * 		Using a very crude approximation of the viewing frustum as a
 *		simple cone, checks whether the node->bound sphere intersects
 *		the cone.<p>
 *
 * <b>Note</b>:	I have <i>got</i> to write a better view-frustum test.
 *
 * @return	0 if the node lies completely outside the viewing cone<br>
 *		1 if the node partially intersects the viewing cone<br>
 *		2 if the node is completely contained by the viewing cone
 */
int vdsSimpleVisibility(const vdsNode *node)
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

    if (distance < node->bound.radius)
    {
	return 1;			/* eyept is within node's sphere */
    }
    phi = atan(node->bound.radius * invDistance);
    DdotV = - VEC3_DOT(D, vdsLookVec);
    theta = acos(DdotV * invDistance);
    
    if (theta - phi > vdsFOV/2.0)	
    {
	return 0;			/* node completely outside view cone */
    }
    if (theta + phi < vdsFOV/2.0)
    {
	return 2;			/* node completely inside view cone */
    }
    return 1;				/* node partly intersects view cone */
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
