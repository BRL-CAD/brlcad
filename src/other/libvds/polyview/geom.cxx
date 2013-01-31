/* Utility routines for OpenGL programs. */

#include "geom.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

/* result is v0.v1 */
float kDot3dv (float *v0, float *v1) {
  int i;
  float result = 0.0;
  for(i=0; i<3; i++) {
    result += v0[i]*v1[i];
  }
  return result;
}

/* r = v0 x v1 */
void kCross3dv (float *r, float *v0, float *v1) {
  r[0] = v0[1]*v1[2] - v0[2]*v1[1];
  r[1] = v0[2]*v1[0] - v0[0]*v1[2];
  r[2] = v0[0]*v1[1] - v0[1]*v1[0];
}

/* result is ||v.v|| */
float kLength3dv (float *v) {
  float result;
  result = sqrt(kDot3dv(v,v));
  return result;
}

/* r = v0 - v1 */
void kSubtract3dv (float *r, float *v0, float *v1) {
  int i;
  for(i=0; i<3; i++) {
    r[i] = v0[i] - v1[i];
  }
}

/* r = v0 + v1 */
void kAdd3dv (float *r, float *v0, float *v1) {
  int i;
  for(i=0; i<3; i++) {
    r[i] = v0[i] + v1[i];
  }
}

/* r = k * v */
void kScalarMult3dv (float *r, float *v0, float k) {
  int i;
  for(i=0; i<3; i++) {
    r[i] = v0[i] * k;
  }
}

/* normalize v and place the result in r */
void kNorm3dv (float *r, float *v) {
  int i;
  float len = kLength3dv(v);
  for(i=0; i<3; i++) {
    r[i] = v[i] / len;
  }
}

/* normalize v in place */
void kNormIP3dv (float *v) {
  int i;
  float len = kLength3dv(v);
  for(i=0; i<3; i++) {
    v[i] = v[i] / len;
  }
}

/* copy v1 into v0 */
void kCopy3dv (float *v0, float *v1) {
  int i;
  for(i=0; i<3; i++) {
    v0[i] = v1[i];
  }
}

/* compute normal of a triangle in an object */
void kTriNormal (object *o, triangle *t) {
  float v0[3], v1[3], v2[3];

  kTriVertexCoords(o,t,0,v0);
  kTriVertexCoords(o,t,1,v1);
  kTriVertexCoords(o,t,2,v2);
  kSubtract3dv(v1,v1,v0);
  kSubtract3dv(v2,v2,v0);
  kCross3dv(t->normal,v1,v2);
  kNormIP3dv(t->normal);
}

/* given a triangle in an object and a vertex number, return the vertex coords */
void kTriVertexCoords (object *o, triangle *t, int i, float *coord) {

  if (i >= 0 && i <= 2) {
    coord[0] = o->v[t->verts[i]-1].coord[0];
    coord[1] = o->v[t->verts[i]-1].coord[1];
    coord[2] = o->v[t->verts[i]-1].coord[2];
  } else {
    coord[0] = coord[1]  = coord[2] = 0.0;
    fprintf(stderr,"You did not specify a valid triangle vertex");
  }
}

/* free resources used by an object. */
void kFreeObject(object *o) {
  int i;

  /* Do the hard stuff first, free list of faces. */
  if (o->f != NULL) {
    for(i=0;i<o->nf;i++) {
      if(o->f[i].verts != NULL) 
	free(o->f[i].verts);
    }
    free(o->f);
  }

  /* Now, kill the triangle list and vertex list. */
  if (o->v != NULL)  
    free(o->v);
  if (o->t != NULL)
    free(o->t);

  /* Now, clean up static fields in case this object is reused. */
  o->nf = o->nv = o->nt = 0;
  o->t = NULL;
  o->v = NULL;
  o->f = NULL;

  for(i=0;i<4;i++) {
    o->mat_ambient[i] = 0.0;
    o->mat_diffuse[i] = 0.0;
    o->mat_specular[i] = 0.0;
    o->mat_emission[i] = 0.0;
  }
  o->mat_shininess = 0.0;
  o->cx = o->cy = o->cz = 0.0;
  o->scale = 1.0;
}

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
