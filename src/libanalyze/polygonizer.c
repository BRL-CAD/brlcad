/*
 * C code from the article
 * "An Implicit Surface Polygonizer"
 * http::www.unchainedgeometry.com/jbloom/papers/polygonizer.pdf
 * by Jules Bloomenthal, jules@bloomenthal.com
 * in "Graphics Gems IV", Academic Press, 1994

 * Authored by Jules Bloomenthal, Xerox PARC.
 * Copyright (c) Xerox Corporation, 1991.  All rights reserved.
 * Permission is granted to reproduce, use and distribute this code for
 * any and all purposes, provided that this notice appears in all copies.  */

/* A Brief Explanation
 *
 * The main data structures in the polygonizer represent a hexahedral lattice,
 * ie, a collection of semi-adjacent cubes, represented as cube centers, corners,
 * and edges. The centers and corners are three-dimensional indices rerpesented
 * by integer i,j,k. The edges are two three-dimensional indices, represented
 * by integer i1,j1,k1,i2,j2,k2. These indices and associated data are stored
 * in hash tables.
 *
 * The polygonize() routine first allocates memory for the hash tables for the
 * cube centers, corners, and edges that define the polygonizing lattice. It
 * then finds a start point, ie, the center of the first lattice cell. It
 * pushes this cell onto an initially empty stack of cells awaiting processing.
 * It creates the first cell by computing its eight corners and assigning them
 * an implicit value.
 *
 * polygonize() then enters a loop in which a cell is popped from the stack,
 * becoming the 'active' cell c. c is (optionally) decomposed (ie, subdivided)
 * into six tetrahedra; within each transverse tetrahedron (ie, those that
 * intersect the surface), one or two triangles are produced.
 *
 * The six faces of c are tested for intersection with the implicit surface; for
 * a transverse face, a new cube is generated and placed on the stack.
 *
 * Some of the more important routines include:
 *
 * testface (called by polygonize): test given face for surface intersection;
 *    if transverse, create new cube by creating four new corners.
 * setcorner (called by polygonize, testface): create new cell corner at given
 *    (i,j,k), compute its implicit value, and add to corners hash table.
 * find (called by polygonize): search for point with given polarity
 * dotet (called by polygonize) set edge vertices, output triangle by
 *    invoking callback
 *
 * The section Cubical Polygonization contains routines to polygonize directly
 * from the lattice cell rather than first decompose it into tetrahedra;
 * dotet, however, is recommended over docube.
 *
 * The section Storage provides routines to handle the linked lists
 * in the hash tables.
 *
 * The section Vertices contains the following routines.
 * vertid (called by dotet): given two corner indices defining a cell edge,
 *    test whether the edge has been stored in the hash table; if so, return its
 *    associated vertex index. If not, compute intersection of edge and implicit
 *    surface, compute associated surface normal, add vertex to mesh array, and
 *    update hash tables
 * converge (called by polygonize, vertid): find surface crossing on edge */

#include "common.h"

#include <stdlib.h>
#include <math.h>
#include <stdio.h>
#include <sys/types.h>
#include "bu/log.h"
#include "bu/malloc.h"
#include "./polygonizer.h"

#define RES	10  /* # converge iterations    */

#define L	0   /* left direction:	-x, -i  */
#define R	1   /* right direction:	+x, +i  */
#define B	2   /* bottom direction: -y, -j */
#define T	3   /* top direction:	+y, +j  */
#define N	4   /* near direction:	-z, -k  */
#define F	5   /* far direction:	+z, +k  */
#define LBN	0   /* left bottom near corner  */
#define LBF	1   /* left bottom far corner   */
#define LTN	2   /* left top near corner     */
#define LTF	3   /* left top far corner      */
#define RBN	4   /* right bottom near corner */
#define RBF	5   /* right bottom far corner  */
#define RTN	6   /* right top near corner    */
#define RTF	7   /* right top far corner     */

/* the LBN corner of cube (i, j, k), corresponds with location
 * (start.x+(i-.5)*size, start.y+(j-.5)*size, start.z+(k-.5)*size) */

#define RAND()	    ((rand()&32767)/32767.)    /* random number between 0 and 1 */
#define HASHBIT	    (5)
#define HASHSIZE    (size_t)(1<<(3*HASHBIT))   /* hash table size (32768) */
#define MASK	    ((1<<HASHBIT)-1)
#define HASH(i,j,k) ((((((i)&MASK)<<HASHBIT)|((j)&MASK))<<HASHBIT)|((k)&MASK))
#define BIT(i, bit) (((i)>>(bit))&1)
#define FLIP(i,bit) ((i)^1<<(bit)) /* flip the given bit of i */

typedef struct test {		   /* test the function for a signed value */
    point_t p;			   /* location of test */
    double value;		   /* function value at p */
    int ok;			   /* if value is of correct sign */
} TEST;

typedef struct polygonizer_vertex VERTEX;	   /* surface vertex */

typedef struct polygonizer_vertices VERTICES;

typedef struct triangle {
    int i1, i2, i3;
} TRIANGLE;

typedef struct triangles {
    int count, max;
    TRIANGLE *ptr;
} TRIANGLES;

typedef struct corner {		   /* corner of a cube */
    int i, j, k;		   /* (i, j, k) is index within lattice */
    point_t p;                     /* location */
    double value;	   	   /* function value */
} CORNER;

typedef struct cube {		   /* partitioning cell (cube) */
    int i, j, k;		   /* lattice location of cube */
    CORNER *corners[8];		   /* eight corners */
} CUBE;

typedef struct cubes {		   /* linked list of cubes acting as stack */
    CUBE cube;			   /* a single cube */
    struct cubes *next;		   /* remaining elements */
} CUBES;

typedef struct centerlist {	   /* list of cube locations */
    int i, j, k;		   /* cube location */
    struct centerlist *next;	   /* remaining elements */
} CENTERLIST;

typedef struct cornerlist {	   /* list of corners */
    int i, j, k;		   /* corner id */
    double value;		   /* corner value */
    struct cornerlist *next;	   /* remaining elements */
} CORNERLIST;

typedef struct edgelist {	   /* list of edges */
    int i1, j1, k1, i2, j2, k2;	   /* edge corner ids */
    int vid;			   /* vertex id */
    struct edgelist *next;	   /* remaining elements */
} EDGELIST;

typedef struct intlist {	   /* list of integers */
    int i;			   /* an integer */
    struct intlist *next;	   /* remaining elements */
} INTLIST;

typedef struct intlists {	   /* list of list of integers */
    INTLIST *list;		   /* a list of integers */
    struct intlists *next;	   /* remaining elements */
} INTLISTS;

typedef struct process {	   /* parameters, function, storage */
    polygonize_func_t function;	   /* implicit surface function */
    void *d;                       /* implicit surface function data */
    polygonize_triproc_t triproc;  /* triangle output function */
    double size, delta;		   /* cube size, normal delta */
    int bounds;			   /* cube range within lattice */
    point_t start;		   /* start point on surface */
    CUBES *cubes;		   /* active cubes */
    VERTICES vertices;		   /* surface vertices */
    CENTERLIST **centers;	   /* cube center hash table */
    CORNERLIST **corners;	   /* corner value hash table */
    EDGELIST **edges;		   /* edge and vertex id hash table */
} PROCESS;


/* setcorner: return corner with the given lattice location
   set (and cache) its function value */
CORNER *
setcorner (PROCESS *p, int i, int j, int k)
{
    /* for speed, do corner value caching here */
    CORNER *c = (CORNER *) bu_calloc(1, sizeof(CORNER), "corner");
    int index = HASH(i, j, k);
    CORNERLIST *l = p->corners[index];
    c->i = i; c->p[X] = p->start[X]+((double)i-.5)*p->size;
    c->j = j; c->p[Y] = p->start[Y]+((double)j-.5)*p->size;
    c->k = k; c->p[Z] = p->start[Z]+((double)k-.5)*p->size;
    for (; l != NULL; l = l->next)
	if (l->i == i && l->j == j && l->k == k) {
	    c->value = l->value;
	    return c;
	}
    l = (CORNERLIST *) bu_calloc(1, sizeof(CORNERLIST), "cornerlist");
    l->i = i; l->j = j; l->k = k;
    l->value = c->value = p->function(c->p, p->d);
    l->next = p->corners[index];
    p->corners[index] = l;
    return c;
}

/* setcenter: set (i,j,k) entry of table[]
 * return 1 if already set; otherwise, set and return 0 */

int
setcenter(CENTERLIST *table[], int i, int j, int k)
{
    int index = HASH(i, j, k);
    CENTERLIST *snew, *l, *q = table[index];
    for (l = q; l != NULL; l = l->next)
	if (l->i == i && l->j == j && l->k == k) return 1;
    snew = (CENTERLIST *) bu_calloc(1, sizeof(CENTERLIST), "centerlist");
    snew->i = i; snew->j = j; snew->k = k; snew->next = q;
    table[index] = snew;
    return 0;
}


/* testface: given cube at lattice (i, j, k), and four corners of face,
 * if surface crosses face, compute other four corners of adjacent cube
 * and add new cube to cube stack */
void
testface(int i, int j, int k, CUBE *old, int face, int c1, int c2, int c3, int c4, PROCESS *p)
{
    CUBE cnew;
    CUBES *oldcubes = p->cubes;
    static int facebit[6] = {2, 2, 1, 1, 0, 0};
    int n, pos = old->corners[c1]->value > 0.0 ? 1 : 0, bit = facebit[face];

    /* test if no surface crossing, cube out of bounds, or already visited: */
    if ((old->corners[c2]->value > 0) == pos &&
	    (old->corners[c3]->value > 0) == pos &&
	    (old->corners[c4]->value > 0) == pos) return;
    if (abs(i) > p->bounds || abs(j) > p->bounds || abs(k) > p->bounds) return;
    if (setcenter(p->centers, i, j, k)) return;

    /* create new cube: */
    cnew.i = i;
    cnew.j = j;
    cnew.k = k;
    for (n = 0; n < 8; n++) cnew.corners[n] = NULL;
    cnew.corners[FLIP(c1, bit)] = old->corners[c1];
    cnew.corners[FLIP(c2, bit)] = old->corners[c2];
    cnew.corners[FLIP(c3, bit)] = old->corners[c3];
    cnew.corners[FLIP(c4, bit)] = old->corners[c4];
    for (n = 0; n < 8; n++)
	if (cnew.corners[n] == NULL)
	    cnew.corners[n] = setcorner(p, i+BIT(n,2), j+BIT(n,1), k+BIT(n,0));

    /*add cube to top of stack: */
    p->cubes = (CUBES *) bu_calloc(1, sizeof(CUBES), "cubes");
    p->cubes->cube = cnew;
    p->cubes->next = oldcubes;
}

/* find: search for point with value of given sign (0: neg, 1: pos) */

TEST
find(int sign, PROCESS *pr, point_t p)
{
    int i;
    TEST test;
    double range = pr->size;
    test.ok = 1;
    for (i = 0; i < 10000; i++) {
	test.p[X] = p[X]+range*(RAND()-0.5);
	test.p[Y] = p[Y]+range*(RAND()-0.5);
	test.p[Z] = p[Z]+range*(RAND()-0.5);
	test.value = pr->function(test.p, pr->d);
	if (sign == (test.value > 0.0)) return test;
	range = range*1.0005; /* slowly expand search outwards */
    }
    test.ok = 0;
    return test;
}


/* getedge: return vertex id for edge; return -1 if not set */

int
getedge(EDGELIST *table[], int i1, int j1, int k1, int i2, int j2, int k2)
{
    EDGELIST *q;
    if (i1>i2 || (i1==i2 && (j1>j2 || (j1==j2 && k1>k2)))) {
	int t=i1; i1=i2; i2=t; t=j1; j1=j2; j2=t; t=k1; k1=k2; k2=t;
    };
    q = table[HASH(i1, j1, k1)+HASH(i2, j2, k2)];
    for (; q != NULL; q = q->next)
	if (q->i1 == i1 && q->j1 == j1 && q->k1 == k1 &&
		q->i2 == i2 && q->j2 == j2 && q->k2 == k2)
	    return q->vid;
    return -1;
}


/* converge: from two points of differing sign, converge to zero crossing */

void
converge(point_t *p1, point_t *p2, double v, polygonize_func_t function, point_t *p, void *d)
{
    int i = 0;
    point_t pos, neg;
    if (v < 0) {
	VMOVE(pos, *p2);
	VMOVE(neg, *p1);
    }
    else {
	VMOVE(pos, *p1);
	VMOVE(neg, *p2);
    }
    while (1) {
	(*p)[X] = 0.5*(pos[X]+ neg[X]);
	(*p)[Y] = 0.5*(pos[Y]+ neg[Y]);
	(*p)[Z] = 0.5*(pos[Z]+ neg[Z]);
	if (i++ == RES) return;
	if ((function(*p, d)) > 0.0) {
	    VMOVE(pos, *p);
	} else {
	    VMOVE(neg, *p);
	}
    }
}


/* vnormal: compute unit length surface normal at point */

void
vnormal(point_t *point, PROCESS *p, point_t *v)
{
    point_t temp;
    double f = p->function(*point, p->d);
    VMOVE(temp, *point);
    temp[X] = temp[X] + p->delta;
    (*v)[X] = p->function(temp, p->d)-f;
    VMOVE(temp, *point);
    temp[Y] = temp[Y] + p->delta;
    (*v)[Y] = p->function(temp, p->d)-f;
    VMOVE(temp, *point);
    temp[Z] = temp[Z] + p->delta;
    (*v)[Z] = p->function(temp, p->d)-f;
    f = MAGNITUDE(*v);
    if (!NEAR_ZERO(f, VUNITIZE_TOL)) {
	VSCALE(*v, *v, 1.0/f);
    }
}


/* vertid: return index for vertex on edge:
 * c1->value and c2->value are presumed of different sign
 * return saved index if any; else compute vertex and save */


/* addtovertices: add v to sequence of vertices */

void
addtovertices(VERTICES *vertices, VERTEX v)
{
    if (vertices->count == vertices->max) {
	int i;
	VERTEX *vnew;
	vertices->max = vertices->count == 0 ? 10 : 2*vertices->count;
	vnew = (VERTEX *) bu_calloc(vertices->max, sizeof(VERTEX), "vertex");
	for (i = 0; i < vertices->count; i++) vnew[i] = vertices->ptr[i];
	if (vertices->ptr != NULL) free((char *) vertices->ptr);
	vertices->ptr = vnew;
    }
    vertices->ptr[vertices->count++] = v;
}


/* setedge: set vertex id for edge */

void
setedge(EDGELIST *table[], int i1, int j1, int k1, int i2, int j2, int k2, int vid)
{
    unsigned int index;
    EDGELIST *enew;
    if (i1>i2 || (i1==i2 && (j1>j2 || (j1==j2 && k1>k2)))) {
	int t=i1; i1=i2; i2=t; t=j1; j1=j2; j2=t; t=k1; k1=k2; k2=t;
    }
    index = HASH(i1, j1, k1) + HASH(i2, j2, k2);
    enew = (EDGELIST *) bu_calloc(1, sizeof(EDGELIST), "edgelist");
    enew->i1 = i1; enew->j1 = j1; enew->k1 = k1;
    enew->i2 = i2; enew->j2 = j2; enew->k2 = k2;
    enew->vid = vid;
    enew->next = table[index];
    table[index] = enew;
}

int
vertid(CORNER *c1, CORNER *c2, PROCESS *p)
{
    VERTEX v;
    point_t a, b;
    int vid = getedge(p->edges, c1->i, c1->j, c1->k, c2->i, c2->j, c2->k);
    if (vid != -1) return vid;			     /* previously computed */
    VMOVE(a, c1->p);
    VMOVE(b, c2->p);
    converge(&a, &b, c1->value, p->function, &v.position, p->d); /* position */
    vnormal(&v.position, p, &v.normal);			   /* normal */
    addtovertices(&p->vertices, v);			   /* save vertex */
    vid = p->vertices.count-1;
    setedge(p->edges, c1->i, c1->j, c1->k, c2->i, c2->j, c2->k, vid);
    return vid;
}


/**** Tetrahedral Polygonization ****/


/* dotet: triangulate the tetrahedron
 * b, c, d should appear clockwise when viewed from a
 * return 0 if client aborts, 1 otherwise */

int
dotet(CUBE *cube, int c1, int c2, int c3, int c4, PROCESS *p)
{
    CORNER *a = cube->corners[c1];
    CORNER *b = cube->corners[c2];
    CORNER *c = cube->corners[c3];
    CORNER *d = cube->corners[c4];
    int index = 0, apos, bpos, cpos, dpos, e1, e2, e3, e4, e5, e6;
    apos = (a->value > 0.0);
    bpos = (b->value > 0.0);
    cpos = (c->value > 0.0);
    dpos = (d->value > 0.0);
    if (apos) index += 8;
    if (bpos) index += 4;
    if (cpos) index += 2;
    if (dpos) index += 1;
    /* index is now 4-bit number representing one of the 16 possible cases */
    if (apos != bpos) e1 = vertid(a, b, p);
    if (apos != cpos) e2 = vertid(a, c, p);
    if (apos != dpos) e3 = vertid(a, d, p);
    if (bpos != cpos) e4 = vertid(b, c, p);
    if (bpos != dpos) e5 = vertid(b, d, p);
    if (cpos != dpos) e6 = vertid(c, d, p);
    /* 14 productive tetrahedral cases (0000 and 1111 do not yield polygons */
    switch (index) {
	case 1:	 return p->triproc(e5, e6, e3, &p->vertices);
	case 2:	 return p->triproc(e2, e6, e4, &p->vertices);
	case 3:	 return p->triproc(e3, e5, e4, &p->vertices) &&
		 p->triproc(e3, e4, e2, &p->vertices);
	case 4:	 return p->triproc(e1, e4, e5, &p->vertices);
	case 5:	 return p->triproc(e3, e1, e4, &p->vertices) &&
		 p->triproc(e3, e4, e6, &p->vertices);
	case 6:	 return p->triproc(e1, e2, e6, &p->vertices) &&
		 p->triproc(e1, e6, e5, &p->vertices);
	case 7:	 return p->triproc(e1, e2, e3, &p->vertices);
	case 8:	 return p->triproc(e1, e3, e2, &p->vertices);
	case 9:	 return p->triproc(e1, e5, e6, &p->vertices) &&
		 p->triproc(e1, e6, e2, &p->vertices);
	case 10: return p->triproc(e1, e3, e6, &p->vertices) &&
		 p->triproc(e1, e6, e4, &p->vertices);
	case 11: return p->triproc(e1, e5, e4, &p->vertices);
	case 12: return p->triproc(e3, e2, e4, &p->vertices) &&
		 p->triproc(e3, e4, e5, &p->vertices);
	case 13: return p->triproc(e6, e2, e4, &p->vertices);
	case 14: return p->triproc(e5, e3, e6, &p->vertices);
    }
    return 1;
}


/**** Cubical Polygonization (optional) ****/

#define LB	0  /* left bottom edge	*/
#define LT	1  /* left top edge	*/
#define LN	2  /* left near edge	*/
#define LF	3  /* left far edge	*/
#define RB	4  /* right bottom edge */
#define RT	5  /* right top edge	*/
#define RN	6  /* right near edge	*/
#define RF	7  /* right far edge	*/
#define BN	8  /* bottom near edge	*/
#define BF	9  /* bottom far edge	*/
#define TN	10 /* top near edge	*/
#define TF	11 /* top far edge	*/

static INTLISTS *cubetable[256];

/*			edge: LB, LT, LN, LF, RB, RT, RN, RF, BN, BF, TN, TF */
static int corner1[12]	   = {LBN,LTN,LBN,LBF,RBN,RTN,RBN,RBF,LBN,LBF,LTN,LTF};
static int corner2[12]	   = {LBF,LTF,LTN,LTF,RBF,RTF,RTN,RTF,RBN,RBF,RTN,RTF};
static int leftface[12]	   = {B,  L,  L,  F,  R,  T,  N,  R,  N,  B,  T,  F};
/* face on left when going corner1 to corner2 */
static int rightface[12]   = {L,  T,  N,  L,  B,  R,  R,  F,  B,  F,  N,  T};
/* face on right when going corner1 to corner2 */


/* docube: triangulate the cube directly, without decomposition */

int docube (cube, p)
    CUBE *cube;
    PROCESS *p;
{
    INTLISTS *polys;
    int i, index = 0;
    for (i = 0; i < 8; i++) if (cube->corners[i]->value > 0.0) index += (1<<i);
    for (polys = cubetable[index]; polys; polys = polys->next) {
	INTLIST *edges;
	int a = -1, b = -1, count = 0;
	for (edges = polys->list; edges; edges = edges->next) {
	    CORNER *c1 = cube->corners[corner1[edges->i]];
	    CORNER *c2 = cube->corners[corner2[edges->i]];
	    int c = vertid(c1, c2, p);
	    if (++count > 2 && ! p->triproc(a, b, c, &p->vertices)) return 0;
	    if (count < 3) a = b;
	    b = c;
	}
    }
    return 1;
}


/* nextcwedge: return next clockwise edge from given edge around given face */

int
nextcwedge (int edge, int face)
{
    switch (edge) {
	case LB: return (face == L)? LF : BN;
	case LT: return (face == L)? LN : TF;
	case LN: return (face == L)? LB : TN;
	case LF: return (face == L)? LT : BF;
	case RB: return (face == R)? RN : BF;
	case RT: return (face == R)? RF : TN;
	case RN: return (face == R)? RT : BN;
	case RF: return (face == R)? RB : TF;
	case BN: return (face == B)? RB : LN;
	case BF: return (face == B)? LB : RF;
	case TN: return (face == T)? LT : RN;
	case TF: return (face == T)? RT : LF;
    }
    return -1;
}


/* otherface: return face adjoining edge that is not the given face */

int
otherface (int edge, int face)
{
    int other = leftface[edge];
    return face == other? rightface[edge] : other;
}


/* makecubetable: create the 256 entry table for cubical polygonization */

void
makecubetable()
{
    int i, e, c, done[12], pos[8];
    for (i = 0; i < 256; i++) {
	for (e = 0; e < 12; e++) done[e] = 0;
	for (c = 0; c < 8; c++) pos[c] = BIT(i, c);
	for (e = 0; e < 12; e++)
	    if (!done[e] && (pos[corner1[e]] != pos[corner2[e]])) {
		INTLIST *ints = 0;
		INTLISTS *lists = (INTLISTS *) bu_calloc(1, sizeof(INTLISTS), "intlists");
		int start = e, edge = e;
		/* get face that is to right of edge from pos to neg corner: */
		int face = pos[corner1[e]]? rightface[e] : leftface[e];
		while (1) {
		    edge = nextcwedge(edge, face);
		    done[edge] = 1;
		    if (pos[corner1[edge]] != pos[corner2[edge]]) {
			INTLIST *tmp = ints;
			ints = (INTLIST *) bu_calloc(1, sizeof(INTLIST), "intlist");
			ints->i = edge;
			ints->next = tmp; /* add edge to head of list */
			if (edge == start) break;
			face = otherface(edge, face);
		    }
		}
		lists->list = ints; /* add ints to head of table entry */
		lists->next = cubetable[i];
		cubetable[i] = lists;
	    }
    }
}

/**** An Implicit Surface Polygonizer ****/

int
polygonalize(
	polygonize_func_t pf,
	void *pf_d,
	fastf_t size,
	int bounds,
	point_t p_s,
	polygonize_triproc_t triproc,
	int mode)
{
    PROCESS p;
    int n, noabort;
    TEST in, out;

    p.function = pf;
    p.triproc = triproc;
    p.size = size;
    p.bounds = bounds;
    p.delta = size/(double)(RES*RES);
    p.d = pf_d;

    /* allocate hash tables and build cube polygon table: */
    p.centers = (CENTERLIST **) bu_calloc(HASHSIZE, sizeof(CENTERLIST *), "hashsize centerlist");
    p.corners = (CORNERLIST **) bu_calloc(HASHSIZE,sizeof(CORNERLIST *), "hashsize, cornerlist");
    p.edges =	(EDGELIST   **) bu_calloc(2*HASHSIZE,sizeof(EDGELIST *), "2*hashsize, edgelist");
    makecubetable();

    /* find point on surface, beginning search at point p_s: */
    srand(1);
    in = find(1, &p, p_s);
    out = find(0, &p, p_s);
    if (!in.ok || !out.ok) {
	bu_log ("polygonizer: Error, can't find starting point");
	return -1;
    }
    converge(&in.p, &out.p, in.value, p.function, &p.start, p.d);

    /* push initial cube on stack: */
    p.cubes = (CUBES *) bu_calloc(1, sizeof(CUBES), "cubes"); /* list of 1 */
    p.cubes->cube.i = p.cubes->cube.j = p.cubes->cube.k = 0;
    p.cubes->next = NULL;

    /* set corners of initial cube: */
    for (n = 0; n < 8; n++)
	p.cubes->cube.corners[n] = setcorner(&p, BIT(n,2), BIT(n,1), BIT(n,0));

    p.vertices.count = p.vertices.max = 0; /* no vertices yet */
    p.vertices.ptr = NULL;

    setcenter(p.centers, 0, 0, 0);

    while (p.cubes != NULL) { /* process active cubes till none left */
	CUBE c;
	CUBES *temp = p.cubes;
	c = p.cubes->cube;

	noabort = mode == POLYGONIZE_TET?
	    /* either decompose into tetrahedra and polygonize: */
	    dotet(&c, LBN, LTN, RBN, LBF, &p) &&
	    dotet(&c, RTN, LTN, LBF, RBN, &p) &&
	    dotet(&c, RTN, LTN, LTF, LBF, &p) &&
	    dotet(&c, RTN, RBN, LBF, RBF, &p) &&
	    dotet(&c, RTN, LBF, LTF, RBF, &p) &&
	    dotet(&c, RTN, LTF, RTF, RBF, &p)
	    :
	    /* or polygonize the cube directly: */
	    docube(&c, &p);
	if (! noabort) return -1;

	/* pop current cube from stack */
	p.cubes = p.cubes->next;
	free((char *) temp);
	/* test six face directions, maybe add to stack: */
	testface(c.i-1, c.j, c.k, &c, L, LBN, LBF, LTN, LTF, &p);
	testface(c.i+1, c.j, c.k, &c, R, RBN, RBF, RTN, RTF, &p);
	testface(c.i, c.j-1, c.k, &c, B, LBN, LBF, RBN, RBF, &p);
	testface(c.i, c.j+1, c.k, &c, T, LTN, LTF, RTN, RTF, &p);
	testface(c.i, c.j, c.k-1, &c, N, LBN, LTN, RBN, RTN, &p);
	testface(c.i, c.j, c.k+1, &c, F, LBF, LTF, RBF, RTF, &p);
    }
    return 0;
}

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */

