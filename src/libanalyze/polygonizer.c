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
#include <string.h>
#include <sys/types.h>
#include "bu/env.h"
#include "bu/log.h"
#include "bu/malloc.h"
#include "bu/time.h"
#include "raytrace.h"
#include "analyze.h"

/**
 * Callback function signature for the function used to decide if the query
 * point q is inside or outside the surface.  The value in d is intended to
 * hold any information needed when evaluating the function.
 */
typedef int (*polygonize_func_t)(point_t *q, void *d);


/* Containers for polygonizer output */

struct polygonizer_vertex {            /* surface vertex */
    point_t position;
    point_t normal;                    /* surface normal */
};

/* list of vertices in polygonization */
struct polygonizer_vertices {
    int count;
    int max;
    struct polygonizer_vertex *ptr;
};

struct polygonizer_triangle {
    int i1;
    int i2;
    int i3;
};

struct polygonizer_triangles {
    int count;
    int max;
    struct polygonizer_triangle *ptr;
};

struct polygonizer_mesh {
    struct polygonizer_vertices vertices;
    struct polygonizer_triangles triangles;
};

void polygonizer_mesh_free(struct polygonizer_mesh *m);

/**
 * Callback function signature for the function called when a triangle is
 * generated - this allows an application (for example) to incrementally
 * display progress as the algorithm is running.
 *
 * i1, i2, i3 (indices into the vertex array defining the triangle)
 * vertices (the vertex array, indexed from 0)
 *
 * vertices are ccw when viewed from the out (positive) side in a left-handed
 * coordinate system
 *
 * vertex normals point outwards
 *
 * Function should return 1 to continue the polygonalization, 0 to abort
 */
typedef int (*polygonize_triproc_t)(int i1, int i2, int i3, struct polygonizer_vertices *vertices, void *d);




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
    void *td;                      /* triangle output function data */
    double size, delta;		   /* cube size, normal delta */
    int bounds;			   /* cube range within lattice */
    point_t start;		   /* start point on surface */
    CUBES *cubes;		   /* active cubes */
    CENTERLIST **centers;	   /* cube center hash table */
    CORNERLIST **corners;	   /* corner value hash table */
    EDGELIST **edges;		   /* edge and vertex id hash table */
    int raytrace;                  /* if non-zero, use raytracer for the converge step */
    struct application *ap;        /* librt application structure to use if we are raytracing */
    struct polygonizer_mesh *m;    /* vertices and triangles generated */
    struct bu_ptbl *f;             /* bu_malloc/bu_calloc'ed items to free */
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
    bu_ptbl_ins(p->f, (long *)c);
    c->i = i; c->p[X] = p->start[X]+((double)i-.5)*p->size;
    c->j = j; c->p[Y] = p->start[Y]+((double)j-.5)*p->size;
    c->k = k; c->p[Z] = p->start[Z]+((double)k-.5)*p->size;
    for (; l != NULL; l = l->next)
	if (l->i == i && l->j == j && l->k == k) {
	    c->value = l->value;
	    return c;
	}
    l = (CORNERLIST *) bu_calloc(1, sizeof(CORNERLIST), "cornerlist");
    bu_ptbl_ins(p->f, (long *)l);
    l->i = i; l->j = j; l->k = k;
    l->value = c->value = p->function(&(c->p), p->d);
    l->next = p->corners[index];
    p->corners[index] = l;

    return c;
}

/* setcenter: set (i,j,k) entry of table[]
 * return 1 if already set; otherwise, set and return 0 */

int
setcenter(PROCESS *p, CENTERLIST *table[], int i, int j, int k)
{
    int index = HASH(i, j, k);
    CENTERLIST *snew, *l, *q = table[index];
    for (l = q; l != NULL; l = l->next)
	if (l->i == i && l->j == j && l->k == k) return 1;
    snew = (CENTERLIST *) bu_calloc(1, sizeof(CENTERLIST), "centerlist");
    bu_ptbl_ins(p->f, (long *)snew);
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
    if (setcenter(p, p->centers, i, j, k)) return;

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
	test.value = pr->function(&test.p, pr->d);
	if (sign == (test.value > 0.0)) return test;
	range = range*1.0005; /* slowly expand search outwards */
    }
    test.ok = 0;
    return test;
}


/* getedge: return vertex id for edge; return -1 if not set */

int
getedge(EDGELIST *table[], int gei1, int gej1, int gek1, int gei2, int gej2, int gek2)
{
    EDGELIST *q;
    if (gei1>gei2 || (gei1==gei2 && (gej1>gej2 || (gej1==gej2 && gek1>gek2)))) {
	int t=gei1; gei1=gei2; gei2=t; t=gej1; gej1=gej2; gej2=t; t=gek1; gek1=gek2; gek2=t;
    };
    q = table[HASH(gei1, gej1, gek1)+HASH(gei2, gej2, gek2)];
    for (; q != NULL; q = q->next)
	if (q->i1 == gei1 && q->j1 == gej1 && q->k1 == gek1 &&
		q->i2 == gei2 && q->j2 == gej2 && q->k2 == gek2)
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
	if ((function(p, d)) > 0.0) {
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
    double f = p->function(point, p->d);
    VMOVE(temp, *point);
    temp[X] = temp[X] + p->delta;
    (*v)[X] = p->function(&temp, p->d)-f;
    VMOVE(temp, *point);
    temp[Y] = temp[Y] + p->delta;
    (*v)[Y] = p->function(&temp, p->d)-f;
    VMOVE(temp, *point);
    temp[Z] = temp[Z] + p->delta;
    (*v)[Z] = p->function(&temp, p->d)-f;
    f = MAGNITUDE(*v);
    if (!NEAR_ZERO(f, VUNITIZE_TOL)) {
	VSCALE(*v, *v, 1.0/f);
    }
}


HIDDEN void
_polygonizer_tgc_hack_fix(struct partition *part, struct soltab *stp) {
    /* hack fix for bad tgc surfaces - avoids a logging crash, which is probably something else altogether... */
    if (bu_strncmp("rec", stp->st_meth->ft_label, 3) == 0 || bu_strncmp("tgc", stp->st_meth->ft_label, 3) == 0) {

	/* correct invalid surface number */
	if (part->pt_inhit->hit_surfno < 1 || part->pt_inhit->hit_surfno > 3) {
	    part->pt_inhit->hit_surfno = 2;
	}
	if (part->pt_outhit->hit_surfno < 1 || part->pt_outhit->hit_surfno > 3) {
	    part->pt_outhit->hit_surfno = 2;
	}
    }
}

HIDDEN int
first_hit(struct application *ap, struct partition *PartHeadp, struct seg *UNUSED(segs))
{
    struct partition *part = PartHeadp->pt_forw;
    struct soltab *stp = part->pt_inseg->seg_stp;
    struct pnt_normal *pt = (struct pnt_normal *)(ap->a_uptr);

    RT_CK_APPLICATION(ap);

    _polygonizer_tgc_hack_fix(part, stp);

    VJOIN1(pt->v, ap->a_ray.r_pt, part->pt_inhit->hit_dist, ap->a_ray.r_dir);
    RT_HIT_NORMAL(pt->n, part->pt_inhit, stp, &(app->a_ray), part->pt_inflip);

    return 0;
}

int
crossing_miss(struct application *UNUSED(ap))
{
    return 0;
}

/* Given the two points, find the hit point and normal using the raytracer */
int
crossing(point_t *n, point_t *p, point_t *p1, point_t *p2, double vorient, struct application *ap)
{
    struct pnt_normal *pt = (struct pnt_normal *)(ap->a_uptr);
    point_t pos, neg;
    vect_t rdir;
    if (vorient < 0) {
	VMOVE(pos, *p2);
	VMOVE(neg, *p1);
    }
    else {
	VMOVE(pos, *p1);
	VMOVE(neg, *p2);
    }

    VSUB2(rdir, neg, pos);
    VUNITIZE(rdir);

    VMOVE(ap->a_ray.r_pt, pos);
    VMOVE(ap->a_ray.r_dir, rdir);

    VSETALL(pt->v, DBL_MAX);
    VSETALL(pt->n, DBL_MAX);

    (void)rt_shootray(ap);

    VMOVE(*p, pt->v);
    VMOVE(*n, pt->n);

    if (!(pt->v[X] < DBL_MAX) || !(pt->n[X] < DBL_MAX)) {
	bu_log("Continuous Meshing: Fatal error, could not find crossing!\n");
	return -1;
    }

    return 0;
    /* MAYBE - if we have a field definition as well, fall back on the field
     * evaluation if the raytracer doesn't give us something sensible.  If the
     * field doesn't converge to a solution locally, ask the raytracer to
     * augment the initial point cloud in this region and locally
     * rebuild/refine the octree. */
}

/* add_vertex: add v to sequence of vertices */
void
add_vertex(VERTICES *vertices, VERTEX *v)
{
    struct polygonizer_vertex *vnew;
    if (vertices->count == vertices->max) {
	vertices->max = vertices->count == 0 ? 10 : 2*vertices->count;
	vertices->ptr = (struct polygonizer_vertex *)bu_realloc(vertices->ptr, vertices->max * sizeof(struct polygonizer_vertex), "vertices");
    }
    vnew = &(vertices->ptr[vertices->count++]);
    VMOVE(vnew->position, v->position);
    VMOVE(vnew->normal, v->normal);
}

/* setedge: set vertex id for edge */
void
setedge(PROCESS *p, EDGELIST *table[], int sei1, int sej1, int sek1, int sei2, int sej2, int sek2, int vid)
{
    unsigned int index;
    EDGELIST *enew;
    if (sei1>sei2 || (sei1==sei2 && (sej1>sej2 || (sej1==sej2 && sek1>sek2)))) {
	int t=sei1; sei1=sei2; sei2=t; t=sej1; sej1=sej2; sej2=t; t=sek1; sek1=sek2; sek2=t;
    }
    index = HASH(sei1, sej1, sek1) + HASH(sei2, sej2, sek2);
    enew = (EDGELIST *) bu_calloc(1, sizeof(EDGELIST), "edgelist");
    bu_ptbl_ins(p->f, (long *)enew);
    enew->i1 = sei1; enew->j1 = sej1; enew->k1 = sek1;
    enew->i2 = sei2; enew->j2 = sej2; enew->k2 = sek2;
    enew->vid = vid;
    enew->next = table[index];
    table[index] = enew;
}

/* vertid: return index for vertex on edge:
 * c1->value and c2->value are presumed of different sign
 * return saved index if any; else compute vertex and save */
int
vertid(CORNER *c1, CORNER *c2, PROCESS *p)
{
    VERTEX v;
    point_t a, b;
    int vid = getedge(p->edges, c1->i, c1->j, c1->k, c2->i, c2->j, c2->k);
    if (vid != -1) return vid;			     /* previously computed */
    VMOVE(a, c1->p);
    VMOVE(b, c2->p);

    if (!p->raytrace) {
	converge(&a, &b, c1->value, p->function, &v.position, p->d); /* position */
	vnormal(&v.position, p, &v.normal);			   /* normal */
    } else {
	if (crossing(&v.normal, &v.position, &a, &b, c1->value, p->ap) < 0) {
	    return -1;
	}
    }

    add_vertex(&p->m->vertices, &v);			   /* save vertex */
    vid = p->m->vertices.count-1;
    setedge(p, p->edges, c1->i, c1->j, c1->k, c2->i, c2->j, c2->k, vid);
    return vid;
}


/**** Tetrahedral Polygonization ****/

int
add_triangle(int i1, int i2, int i3, PROCESS *p)
{
    struct polygonizer_triangle *t;
    struct polygonizer_triangles *triangles = &p->m->triangles;
    if (triangles->count == triangles->max) {
	triangles->max = triangles->count == 0 ? 10 : 2*triangles->count;
	triangles->ptr = (struct polygonizer_triangle *)bu_realloc(triangles->ptr, triangles->max * sizeof(struct polygonizer_triangle), "triangles");
    }
    t = &(triangles->ptr[triangles->count++]);
    t->i1 = i1;
    t->i2 = i2;
    t->i3 = i3;

    if (!p->triproc) return 1;
    return p->triproc(i1, i2, i3, &p->m->vertices, p->td);
}

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
    int index = 0;
    int e1 = 0;
    int e2 = 0;
    int e3 = 0;
    int e4 = 0;
    int e5 = 0;
    int e6 = 0;
    int apos, bpos, cpos, dpos;
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
    /* If one of the vertid calculations failed, we're done */
    if (e1 < 0 || e2 < 0 || e3 < 0 || e4 < 0 || e5 < 0 || e6 < 0) return 0;
    /* 14 productive tetrahedral cases (0000 and 1111 do not yield polygons */
    switch (index) {
	case 1:	 return add_triangle(e5, e6, e3, p);
	case 2:	 return add_triangle(e2, e6, e4, p);
	case 3:	 return add_triangle(e3, e5, e4, p) && add_triangle(e3, e4, e2, p);
	case 4:	 return add_triangle(e1, e4, e5, p);
	case 5:	 return add_triangle(e3, e1, e4, p) && add_triangle(e3, e4, e6, p);
	case 6:	 return add_triangle(e1, e2, e6, p) && add_triangle(e1, e6, e5, p);
	case 7:	 return add_triangle(e1, e2, e3, p);
	case 8:	 return add_triangle(e1, e3, e2, p);
	case 9:	 return add_triangle(e1, e5, e6, p) && add_triangle(e1, e6, e2, p);
	case 10: return add_triangle(e1, e3, e6, p) && add_triangle(e1, e6, e4, p);
	case 11: return add_triangle(e1, e5, e4, p);
	case 12: return add_triangle(e3, e2, e4, p) && add_triangle(e3, e4, e5, p);
	case 13: return add_triangle(e6, e2, e4, p);
	case 14: return add_triangle(e5, e3, e6, p);
    }
    return 1;
}

void
polygonizer_mesh_free(struct polygonizer_mesh *m)
{
    if (!m) return;
    if (m->vertices.ptr) bu_free(m->vertices.ptr, "vertices");
    if (m->triangles.ptr) bu_free(m->triangles.ptr, "triangles");
    bu_free(m, "mesh");
}


HIDDEN int
in_out_hit(struct application *ap, struct partition *partH, struct seg *UNUSED(segs))
{
    struct partition *part = partH->pt_forw;
    struct soltab *stp = part->pt_inseg->seg_stp;

    int *ret = (int *)(ap->a_uptr);

    RT_CK_APPLICATION(ap);

    _polygonizer_tgc_hack_fix(part, stp);

    if (part->pt_inhit->hit_dist < 0) {
	(*ret) = -1;
    }

    return 0;
}

int
in_out_miss(struct application *UNUSED(ap))
{
    return 0;
}

int
pnt_in_out(point_t *p, void *d)
{
    int i;
    int fret = 1;
    int dir_results[6] = {0, 0, 0, 0, 0, 0};
    struct application *ap = (struct application *)d;
    int (*a_hit)(struct application *, struct partition *, struct seg *);
    int (*a_miss)(struct application *);
    void *uptr_stash;

    vect_t mx, my, mz, px, py, pz;
    VSET(mx, -1,  0,  0);
    VSET(my,  0, -1,  0);
    VSET(mz,  0,  0, -1);
    VSET(px,  1,  0,  0);
    VSET(py,  0,  1,  0);
    VSET(pz,  0,  0,  1);

    /* reuse existing application, just cache pre-existing hit routines and
     * substitute our own */
    a_hit = ap->a_hit;
    a_miss = ap->a_miss;
    uptr_stash = ap->a_uptr;

    ap->a_hit = in_out_hit;
    ap->a_miss = in_out_miss;

    VMOVE(ap->a_ray.r_pt, *p);

    /* Check the six directions to see if any of them indicate we are inside */

    /* -x */
    ap->a_uptr = &(dir_results[0]);
    VMOVE(ap->a_ray.r_dir, mx);
    (void)rt_shootray(ap);
    /* -y */
    ap->a_uptr = &(dir_results[1]);
    VMOVE(ap->a_ray.r_dir, my);
    (void)rt_shootray(ap);
    /* -z */
    ap->a_uptr = &(dir_results[2]);
    VMOVE(ap->a_ray.r_dir, mz);
    (void)rt_shootray(ap);
    /* x */
    ap->a_uptr = &(dir_results[3]);
    VMOVE(ap->a_ray.r_dir, px);
    (void)rt_shootray(ap);
    /* y */
    ap->a_uptr = &(dir_results[4]);
    VMOVE(ap->a_ray.r_dir, py);
    (void)rt_shootray(ap);
    /* z */
    ap->a_uptr = &(dir_results[5]);
    VMOVE(ap->a_ray.r_dir, pz);
    (void)rt_shootray(ap);

    for (i = 0; i < 6; i++) {
	if (dir_results[i] < 0) fret = -1;
    }

    /* restore application */
    ap->a_hit = a_hit;
    ap->a_miss = a_miss;
    ap->a_uptr = uptr_stash;

    return fret;
}

/**** An Implicit Surface Polygonizer ****/
int
analyze_polygonize(
	int **faces,
       	int *num_faces,
       	point_t **vertices,
       	int *num_vertices,
	fastf_t size,
	point_t p_s,
       	const char *obj,
       	struct db_i *dbip,
	struct analyze_polygonize_params *params
	)
{
    int ret = 0;
    size_t ncpus = bu_avail_cpus();
    struct pnt_normal *rtpnt;
    PROCESS p;
    int i, n;
    int noabort = 0;
    struct polygonizer_mesh *m;
    struct application *ap;
    struct resource *resp;
    struct rt_i *rtip;
    fastf_t timestamp;
    fastf_t timestamp2;
    fastf_t mt = (params && params->max_time > 0) ? (fastf_t)params->max_time : 0.0;

    if (!faces || !num_faces || !vertices || !num_vertices || !obj || !dbip) return -1;

    timestamp = bu_gettime();
    timestamp2 = bu_gettime();

    p.function = pnt_in_out;
    p.triproc = NULL;
    p.bounds = INT_MAX;
    p.delta = 0;
    p.td = NULL;
    p.raytrace = 1;

    BU_GET(p.f , struct bu_ptbl);
    bu_ptbl_init(p.f , 64, "free list");

    m = (struct polygonizer_mesh *)bu_calloc(1, sizeof(struct polygonizer_mesh), "results");

    p.size = size;
    p.m = m;
    p.cubes = NULL;

    /* allocate hash tables and build cube polygon table: */
    p.centers = (CENTERLIST **) bu_calloc(HASHSIZE, sizeof(CENTERLIST *), "hashsize centerlist");
    p.corners = (CORNERLIST **) bu_calloc(HASHSIZE,sizeof(CORNERLIST *), "hashsize, cornerlist");
    p.edges =	(EDGELIST   **) bu_calloc(2*HASHSIZE,sizeof(EDGELIST *), "2*hashsize, edgelist");

    /* p_s must be on the surface for this method */
    VMOVE(p.start, p_s);

    /* Set up raytracing */
    BU_GET(ap, struct application);
    RT_APPLICATION_INIT(ap);
    BU_GET(resp, struct resource);
    rtip = rt_new_rti(dbip);
    rt_init_resource(resp, 0, rtip);
    ap->a_rt_i = rtip;
    ap->a_resource = resp;
    ap->a_onehit = 1;
    ap->a_hit = first_hit;
    ap->a_miss = crossing_miss;
    ap->a_overlap = NULL;
    ap->a_logoverlap = rt_silent_logoverlap;
    BU_GET(rtpnt, struct pnt_normal);
    ap->a_uptr = (void *)rtpnt;
    if ((rt_gettree(rtip, obj) < 0)) {
	ret = -1;
	goto analyze_polygonizer_memfree;
    }
    rt_prep_parallel(rtip, (int)ncpus);
    p.ap = ap;
    p.d = ap;

    /* push initial cube on stack: */
    p.cubes = (CUBES *) bu_calloc(1, sizeof(CUBES), "cubes"); /* list of 1 */
    p.cubes->cube.i = p.cubes->cube.j = p.cubes->cube.k = 0;
    p.cubes->next = NULL;

    /* set corners of initial cube: */
    for (n = 0; n < 8; n++)
	p.cubes->cube.corners[n] = setcorner(&p, BIT(n,2), BIT(n,1), BIT(n,0));

    p.m->vertices.count = p.m->vertices.max = 0; /* no vertices yet */
    p.m->vertices.ptr = NULL;

    p.m->triangles.count = p.m->triangles.max = 0; /* no triangles yet */
    p.m->triangles.ptr = NULL;

    setcenter(&p, p.centers, 0, 0, 0);

    while (p.cubes != NULL) { /* process active cubes till none left */
	unsigned long long avail_mem = 0;
	CUBE c;
	CUBES *temp = p.cubes;
	c = p.cubes->cube;

	/* decompose into tetrahedra and polygonize: */
	noabort =
	    dotet(&c, LBN, LTN, RBN, LBF, &p) &&
	    dotet(&c, RTN, LTN, LBF, RBN, &p) &&
	    dotet(&c, RTN, LTN, LTF, LBF, &p) &&
	    dotet(&c, RTN, RBN, LBF, RBF, &p) &&
	    dotet(&c, RTN, LBF, LTF, RBF, &p) &&
	    dotet(&c, RTN, LTF, RTF, RBF, &p);

	if (!noabort) {
	    ret = -1;
	    goto analyze_polygonizer_memfree;
	}

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

	if (((bu_gettime() - timestamp)/1e6) > mt) {
	    /* Taking too long, bail */
	    ret = 2;
	    goto analyze_polygonizer_memfree;
	}

	if (params && params->minimum_free_mem > 0) {
	    int ec;
	    avail_mem = bu_mem(BU_MEM_AVAIL, &ec);
	    if (!ec && avail_mem < params->minimum_free_mem) {
		/* memory too tight, bail */
		ret = 3;
		goto analyze_polygonizer_memfree;
	    }
	}

	if (((bu_gettime() - timestamp2)/1e6) > 5) {
	    if (params && params->verbosity) {
		int delta = (int)(bu_gettime() - timestamp)/1e6;
		bu_log("Triangle count after %d seconds: %d\n", delta, m->triangles.count);
	    }
	    timestamp2 = bu_gettime();
	}
    }

    /* Translate m into vert and face arrays */
    if (m->triangles.count && m->vertices.count) {
	int *nfaces = (int *)bu_calloc(m->triangles.count * 3, sizeof(int), "faces array");
	point_t *nverts = (point_t *)bu_calloc(m->vertices.count, sizeof(point_t), "verts array");
	for (i = 0; i < m->vertices.count; i++) {
	    VMOVE(nverts[i], m->vertices.ptr[i].position);
	}
	for (i = 0; i < m->triangles.count; i++) {
	    nfaces[i*3 + 0] = m->triangles.ptr[i].i1;
	    nfaces[i*3 + 1] = m->triangles.ptr[i].i3;
	    nfaces[i*3 + 2] = m->triangles.ptr[i].i2;
	}
	(*num_faces) = m->triangles.count;
	(*num_vertices) = m->vertices.count;
	(*faces) = nfaces;
	(*vertices) = nverts;
	ret = 0;
    } else {
	ret = -1;
    }

analyze_polygonizer_memfree:


    while (p.cubes != NULL) { /* free any remaining cubes */
	CUBES *temp = p.cubes;
	p.cubes = p.cubes->next;
	free((char *) temp);
    }

    /* polygonizer memory */
    polygonizer_mesh_free(m);
    if (p.f) {
	for (i = 0; i < (int)BU_PTBL_LEN(p.f); i++) {
	    bu_free(BU_PTBL_GET(p.f, i), "free list item");
	}
	bu_ptbl_free(p.f);
	BU_PUT(p.f, struct bu_ptbl);
    }
    bu_free(p.centers, "centerlist");
    bu_free(p.corners, "cornerlist");
    bu_free(p.edges, "edgelist");

    /* LIBRT memory */
    rt_clean_resource(rtip, resp);
    rt_free_rti(rtip);
    BU_PUT(resp, struct resource);
    BU_PUT(ap, struct appliation);
    BU_PUT(rtpnt, struct pnt_normal);

    return ret;
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

