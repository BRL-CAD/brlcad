/*
 * File:	vdsprivate.h
 * Description:	"Private" header file of the view-dependent simplification
 *		package.  This includes boilerplate macros and preprocessing
 *		directives used by the library, as well as routines which
 *		need not be exposed to the user, but which could not be
 *		labeled static since functions in other files call them.
 * 
 * Copyright 1999 The University of Virginia.  All Rights Reserved.  Disclaimer
 * and copyright notice are given in full below and apply to this entire file.
 */
#ifndef VDS_PRIVATE_H
#define VDS_PRIVATE_H

#ifdef INSTRUMENT
#define STAT(x) x
#else
#define STAT(x) 
#endif

#ifdef VDS_DEBUGPRINT
#define VDS_DEBUG(x) printf x
#else
#define VDS_DEBUG(x)
#define NDEBUG 				/* disable asserts */
#endif

#include <assert.h>

/* Routines for maintaining the vertex tree (dynamic.c) */
extern void vdsUnfoldNode(vdsNode *);
extern void vdsFoldNode(vdsNode *);
extern void vdsFoldSubtree(vdsNode *);
/* Routines for rendering the vertex tree (render.c) */
extern void vdsUpdateTriProxies(vdsTri *t);
extern void vdsUpdateTriNormal(vdsTri *t);
/* Routines for building the vertex tree (build.c) */
extern vdsNode *vdsFindNode(vdsNodeId id, vdsNode *root);
extern vdsNodeId vdsFindCommonId(vdsNodeId id1, vdsNodeId id2, int maxdepth);
extern void vdsComputeTriNodes(vdsNode *node, vdsNode *root);

#endif 		/* VDS_PRIVATE_H */

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
