#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <assert.h>

#include "poly.h"
#include "polytok.h"

#include "vds.h"
#include "vector.h"

extern char *yytext;
extern FILE *yyin;
extern int pLineNo;

static char *curFileName;

/* Prototypes. */
void read_poly_head(int *nv, int *nf);
void read_poly_body(object *o);
int read_poly_vertex(vertex *v);
int read_poly_face(face *f);

int yylex(void);

/* Routine prints an error message in the appropriate format. */
void poly_error(char *msg) {
    fprintf(stderr,"[%s]:%d  %s.\n",curFileName,pLineNo,msg);
    exit(1);
}

/* Read the .poly file given by fname and return a single object. */
void read_poly(char *fname, object *o) {
    int i;
    
    /* Open the file... */
    if (fname == NULL) {
	fprintf(stderr,"Filename param was not initialized in read_poly!\n");
	exit(1);
    } else {
	curFileName = fname;
	pLineNo = 1;
    }
    if ( (yyin = fopen(fname,"r")) == NULL ) {
	poly_error("Could not open file for reading");
    }

    /* Read the header. */
    read_poly_head(&o->nv,&o->nf);
 
    if ( (o->nv > 0) && (o->nf > 0) ) {
	/* Allocate some memory. */
	o->v = (vertex *)malloc(sizeof(vertex) * o->nv);
	o->f = (face *)malloc(sizeof(face) * o->nf);
	if ( (o->v == NULL) || (o->f == NULL) ) {
	    poly_error("Couldn't allocate enough memory for verts and faces");
	}
	/* Read the rest of the file. */
	read_poly_body(o);
    } else {
	poly_error("Invalid vertex or face count");
    }
}	

void read_poly_head(int *nv, int *nf) {
    if (yylex() == NUMVERT) {
	if (yylex() == INT) {
	    *nv = atoi(yytext);
	} else {
	    poly_error("Expected a positive integer after vertices:");
	}
	if (yylex() == NUMFACE) {
	    if (yylex() == INT) {
		*nf = atoi(yytext);
	    } else {
		poly_error("Expected a positive integer after faces:");
	    }
	} else {
	    poly_error("Expected faces:\n");
	}
    } else {
	poly_error("Expected vertices:\n");
    }
}

void read_poly_body(object *o) {
    int ttype, prevtok;
    int iv, ifc, numtris, i, j, k;
    float x, y, z;

    iv = ifc = numtris = 0;
    prevtok = -1;

    /* Read lines until we get the EOF. */
    while(1) {
	if (prevtok == -1) {
	    ttype = yylex();
	} else {
	    ttype = prevtok;
	}
	if (ttype == VERT) {
	    if (iv == o->nv) {
		poly_error("There are more vertices than indicated by the vertices: directive");
	    }
	    prevtok = read_poly_vertex(&(o->v[iv]));
	    iv++;
	} else if (ttype == FACE) {
	    if (ifc == o->nf) {
		poly_error("There are more faces than indicated by the faces: directive");
	    }
	    prevtok = read_poly_face(&(o->f[ifc]));
	    numtris += o->f[ifc].n - 2;
	    ifc++;
	} else if (ttype == PEOF) {
	    break;
	} else {
	    poly_error("Invalid .poly file syntax");
	}
    }

    if (iv != o->nv || ifc != o->nf) {
	poly_error("Number of vertices or faces is less than specified");
    }

    /* Convert faces to triangles here. */
    o->nt = numtris;
    if ((o->t = (triangle *)malloc(sizeof(triangle)*numtris)) == NULL) {
	poly_error("Could not allocate triangle array");
    }
    /* Iterate through faces, building triangles from CONVEX faces. */
    k = 0;
    for(i=0; i < o->nf; i++) {
	for(j=0; j < (o->f[i].n - 2); j++) {
	    o->t[k].verts[0] = o->f[i].verts[0];
	    o->t[k].verts[1] = o->f[i].verts[j+1];
	    o->t[k].verts[2] = o->f[i].verts[j+2];
	    /* Compute normal for this triangle. */
	    kTriNormal(o,&o->t[k]);
	    k++;
	}
    }
    /* Generate the vertex tree */
    generateVertexTree(o);
}

/* Read a vertex. Returns type of last token read. */
int read_poly_vertex(vertex *v) {
    int ttype;
    int i;

    v->hasNormal = 0;

    for(i=0;i<3;i++) {
	ttype = yylex();
	if (ttype == INT || ttype == FLOAT) {
	    v->coord[i] = atof(yytext);
	} else if (ttype == PEOF) {
	    poly_error("Premature end of file");
	} else {
	    poly_error("Invalid vector specification");
	}
    }

    ttype = yylex();
    if (ttype == VERT || ttype == FACE) {
	return ttype;
    } else if (!(ttype == INT || ttype == FLOAT)) {
	poly_error("I don't know what just happened, but your file is foobar");
    }

    v->hasNormal = 1;

    for(i=0;i<3;i++) {
	if (i > 0) ttype = yylex();
	if (ttype == INT || ttype == FLOAT) {
	    v->normal[i] = atof(yytext);
	} else if (ttype == PEOF) {
	    poly_error("Premature end of file");
	} else {
	    poly_error("Invalid normal specification");
	}
    }   
    return yylex();
}

/* Read a face. Returns type of last token read. */
int read_poly_face(face *f) {
    int ttype;
    int queue[20];
    int i;

    i = 0;
    while(1) {
	ttype = yylex();
	if (ttype == INT) {
	    if (i == 20) {
		poly_error("Cannot handle faces with more than 20 sides");
	    } else {
		queue[i++] = atoi(yytext);
	    }
	} else if (ttype == PEOF || ttype == VERT || ttype == FACE) {
	    break;
	} else {
	    poly_error("Invalid face syntax");
	}
    }

    /* allocate memory for face vertex indices. */
    if (i < 3) {
	poly_error("Faces must have 3 or more vertices");
    }
    f->n = i;
    if ((f->verts = (int *)malloc(sizeof(int)*f->n)) == NULL) {
	poly_error("Could not allocate memory for vertices in face object");
    }
    for(i=0;i<f->n;i++) {
	f->verts[i] = queue[i];
    }

    return ttype;
    
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
