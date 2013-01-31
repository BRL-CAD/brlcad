#ifndef GEOM_H
#define GEOM_H

#include "vds.h"

typedef struct Face {
    int *verts;                /* Array of indices into vertex array. */
    int n;                     /* No. of vertices for this face. */
} face;

typedef struct Vertex {
    float coord[3];         /* Vertex coordinates. */
    int hasNormal;             /* Flag indicating if vertex has a normal. */
    float normal[3];        /* Vertex normal. */
} vertex;

typedef struct Triangle {
    int verts[3];              /* Vertices of the triangle. */
    float normal[3];        /* Normal for this triangle. */
} triangle;

typedef struct Object {
    vertex *v;                 /* Array of vertices. */
    int nv;                    /* Number of vertices in array. */
    face *f;                   /* Array of faces. */
    int nf;                    /* Number of faces in array. */
    triangle *t;               /* Array of triangles. */
    int nt;                    /* Number of triangles in array. */
    float mat_ambient[4];    /* Material ambient property. */
    float mat_diffuse[4];    /* Material diffuse property. */
    float mat_specular[4];   /* Material specular property. */
    float mat_emission[4];   /* Material emission property. */
    float mat_shininess;     /* Material shininess property. */
    float xdim;             /* Object x dimension. */
    float ydim;             /* Object y dimension. */
    float zdim;             /* Object z dimension. */
    float cx, cy, cz;       /* Translation vals. to put object at origin. */
    float scale;            /* Scaling factor. */
    /* vdslib stuff */
    vdsNode *vertexTree;	/* the VDS vertex tree */
} object;

/* Some general purpose routines for manipulating geometry. */
void kTriNormal (object *o, triangle *t);

/* result is v0.v1 */
float kDot3dv (float *v0, float *v1);

/* r = v0 x v1 */
void kCross3dv (float *r, float *v0, float *v1);

/* result is ||v.v|| */
float kLength3dv (float *v);

/* r = v0 - v1 */
void kSubtract3dv (float *r, float *v0, float *v1);

/* r = v0 + v1 */
void kAdd3dv (float *r, float *v0, float *v1);

/* r = k * v */
void kScalarMult3dv (float *r, float *v0, float k);

/* normalize v and place the result in r. */
void kNorm3dv (float *r, float *v);

/* normalize v in place. */
void kNormIP3dv (float *v);

/* copy v1 into v0. */
void kCopy3dv (float *v0, float *v1);

/* given a triangle in an object and a vertex #, return the vertex coords */
void kTriVertexCoords (object *o, triangle *t, int i, float *coord);

/* free resources used by an object. */
void kFreeObject(object *o);

#endif

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
