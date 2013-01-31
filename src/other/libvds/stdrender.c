/**
 * @memo	Simple example routines for rendering the vertex tree.
 * @name	Standard callbacks: rendering nodes
 * 
 * 		Standard routines for rendering the active triangle list
 *		associated with a vdsNode.  The user may provide a custom
 *		rendering function to be called	on each node, or may choose
 *		to use one of the standard ones	provided here.<p>
 *
 * <b>Note</b>:	The rendering callback is responsible for updating the
 *		node->proxies field when a node representing a triangle
 *		corner is folded or unfolded.  When writing a custom rendering
 *		callback, be sure to call \Ref{vdsUpdateTriProxies} on each 
 *		triangle before rendering it.<p>
 * 
 * @see		stdrender.c
 */
/*@{*/

#include <GL/gl.h>

#include "vds.h"
#include "stdvds.h"
#include "vector.h"

unsigned int vdsTrisDrawn = 0;

/** Returns the number of triangles rendered since last call.
 * 		Returns the number of tris drawn since the last call to
 *		vdsCountTrisDrawn().  If called once per frame, provides a 
 *		counter of the number of triangles drawn each frame.<p>
 *
 * <b>Note</b>: Counters are updated in the rendering callbacks below.  User-
 *		written rendering callbacks must update the same counter if
 *		they wish to use this function.
 */
unsigned int vdsCountTrisDrawn()
{
    int tmp = vdsTrisDrawn;
    vdsTrisDrawn = 0;
    return tmp;
}

/** Draw a node's triangles in wireframe.
 * 		Draw the triangles in node->vistris in GL_LINE polygon mode.
 */
void vdsRenderWireframe(const vdsNode *node)
{
    vdsTri *t = node->vistris;

    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    glDisable(GL_LIGHTING);
    glDisable(GL_COLOR_MATERIAL);
    glColor3ub(0, 0, 0);
    glBegin(GL_TRIANGLES);
    while (t != NULL)
    {
	vdsUpdateTriProxies(t);			/* Required */
	vdsTrisDrawn ++;
	GL_VERTEX3V(t->proxies[0]->coord);
	GL_VERTEX3V(t->proxies[1]->coord);
	GL_VERTEX3V(t->proxies[2]->coord);
	t = t->next;
    }
    glEnd();
}

/** Draw a node's triangles shaded but not lit.
 * 		Draw the triangles in node->vistris colored according to
 *		the tri->color[] field.  Lighting is turned off.
 */
void vdsRenderShaded(const vdsNode *node)
{
    vdsTri *t = node->vistris;

    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glDisable(GL_LIGHTING);
    glDisable(GL_COLOR_MATERIAL);
    glBegin(GL_TRIANGLES);
    while (t != NULL)
    {
	vdsUpdateTriProxies(t);			/* Required */
	vdsTrisDrawn ++;
	glColor3ubv(t->color[0]);
	GL_VERTEX3V(t->proxies[0]->coord);
	glColor3ubv(t->color[1]);
	GL_VERTEX3V(t->proxies[1]->coord);
	glColor3ubv(t->color[2]);
	GL_VERTEX3V(t->proxies[2]->coord);
	t = t->next;
    }
    glEnd();
}

/** Draw a node's triangles shaded and lit.
 * 		Draw the triangles in node->vistris colored according to
 *		the tri->color[] field.  Lighting is enabled and the diffuse
 *		component of the currently bound material is set to track the
 *		tri->color[] field.<p>
 *
 * <b>Note</b>:	The user is still responsible for placing lights, creating
 *		and binding a material, and adjusting the lighting model.
 */
void vdsRenderShadedLit(const vdsNode *node)
{
    vdsTri *t = node->vistris;

    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glEnable(GL_LIGHTING);
    glEnable(GL_COLOR_MATERIAL);
    glBegin(GL_TRIANGLES);
    while (t != NULL)
    {
	vdsUpdateTriProxies(t);			/* Required */
	vdsTrisDrawn ++;
	GL_NORMAL3V(t->normal[0]);
	glColor3ubv(t->color[0]);
	GL_VERTEX3V(t->proxies[0]->coord);
	GL_NORMAL3V(t->normal[1]);
	glColor3ubv(t->color[1]);
	GL_VERTEX3V(t->proxies[1]->coord);
	GL_NORMAL3V(t->normal[2]);
	glColor3ubv(t->color[2]);
	GL_VERTEX3V(t->proxies[2]->coord);
	t = t->next;
    }
    glEnd();
}

/** Draw a node's triangles lit but not shaded.
 * 		Draw the triangles in node->vistris without specifying color
 *		information.  Lighting is enabled; whatever material the
 *		user has bound is used.<p>
 *
 * <b>Note</b>:	The user is still responsible for placing lights, creating
 *		and binding a material, and adjusting the lighting model.
 */
void vdsRenderLit(const vdsNode *node)
{
    vdsTri *t = node->vistris;

    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glEnable(GL_LIGHTING);
    glDisable(GL_COLOR_MATERIAL);
    glBegin(GL_TRIANGLES);
    while (t != NULL)
    {
	vdsUpdateTriProxies(t);			/* Required */
	vdsTrisDrawn ++;
	GL_NORMAL3V(t->normal[0]);
	GL_VERTEX3V(t->proxies[0]->coord);
	GL_NORMAL3V(t->normal[1]);
	GL_VERTEX3V(t->proxies[1]->coord);
	GL_NORMAL3V(t->normal[2]);
	GL_VERTEX3V(t->proxies[2]->coord);
	t = t->next;
    }
    glEnd();
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
