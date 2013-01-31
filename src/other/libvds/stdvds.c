/**
 * @memo	Common routines and globals used by the standard callbacks.
 * @name 	Standard callbacks: common routines and variables
 * 		
 * 		Common functions and global variables used by the standard 
 *		routines provided by the view-dependent simplification 
 *		package.<p>
 *
 * @see		stdvds.c
 */
/*@{*/
#include <math.h>
#include <assert.h>

#include "vds.h"
#include "stdvds.h"
#include "vector.h"

/*
 * Globals:
 */
/** Position of the viewer		*/
vdsVec3 vdsViewPt;
/** Direction of the viewer's gaze	*/
vdsVec3 vdsLookVec;
/** Field of view, in radians		*/
vdsFloat vdsFOV;
/** We precompute 1/tan(FOV) for speed	*/
vdsFloat vdsInvTanFOV;
/** Fraction of FOV which a node must cover to avoid being folded 	*/
vdsFloat vdsThreshold;

/** Tell VDSlib the current field of view, in radians.
 *		This is needed by the standard visibility and simplification
 *		callbacks.
 */
void vdsSetFOV(vdsFloat FOV)
{
    assert(FOV > 0);
    assert(FOV < M_PI);
    vdsFOV = FOV;
    vdsInvTanFOV = 1.0 / tan(vdsFOV);
}

/** Tell VDSlib the current viewpoint.
 * 		Used by the standard simplification and visibility callbacks.
 */
void vdsSetViewpoint(vdsVec3 viewpt)
{
    VEC3_COPY(vdsViewPt, viewpt);
}

/** Tell VDSlib the current "look vector".
 * 		This is the view direction expressed as a normalized vector.  
 *		Used by the standard simplification and visibility callbacks.
 */
void vdsSetLookVec(vdsVec3 look)
{
    VEC3_COPY(vdsLookVec, look);
}

/** Set the simplification threshold for the standard simplification callback.
 *		This threshold corresponds to a fraction of the angular field 
 *		of view (as specified by \Ref{vdsSetFOV}). The standard 
 *		simplification callback directs nodes whose on-screen extent 
 * 		exceeds this fraction to be unfolded, while nodes smaller 
 *		on screen than the threshold are folded.
 * @param	thresh	the fraction of the angular field-of-view below
 *			which nodes should be folded.
 */
void vdsSetThreshold(vdsFloat thresh)
{
    vdsThreshold = thresh;
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
