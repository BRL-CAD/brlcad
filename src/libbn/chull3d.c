/*
 * Ken Clarkson wrote this.  Copyright (c) 1995 by AT&T..
 * Permission to use, copy, modify, and distribute this software for any
 * purpose without fee is hereby granted, provided that this entire notice
 * is included in all copies of any software which is or includes a copy
 * or modification of this software and in all copies of the supporting
 * documentation for such software.
 * THIS SOFTWARE IS BEING PROVIDED "AS IS", WITHOUT ANY EXPRESS OR IMPLIED
 * WARRANTY.  IN PARTICULAR, NEITHER THE AUTHORS NOR AT&T MAKE ANY
 * REPRESENTATION OR WARRANTY OF ANY KIND CONCERNING THE MERCHANTABILITY
 * OF THIS SOFTWARE OR ITS FITNESS FOR ANY PARTICULAR PURPOSE.
 */

#include "common.h"

#include <assert.h>
#include <float.h>
#include <locale.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>

/*#include "chull3d.h"*/
#include "bu/file.h"
#include "bu/log.h"
#include "bu/getopt.h"
#include "bu/malloc.h"
#include "bn.h"

#define MAXBLOCKS 10000
#define max_blocks 10000
#define Nobj 10000
#define MAXPOINTS 10000
#define BLOCKSIZE 100000
#define MAXDIM 8

typedef double Coord;
typedef Coord* point;
typedef point site;
typedef Coord* normalp;
typedef Coord site_struct;

typedef struct basis_s {
    struct basis_s *next; /* free list */
    int ref_count;        /* storage management */
    int lscale;           /* the log base 2 of total scaling of vector */
    Coord sqa, sqb;       /* sums of squared norms of a part and b part */
    Coord vecs[1];        /* the actual vectors, extended by malloc'ing bigger */
} basis_s;

typedef struct neighbor {
    site vert;            /* vertex of simplex */
    struct simplex *simp; /* neighbor sharing all vertices but vert */
    basis_s *basis;       /* derived vectors */
} neighbor;

typedef struct simplex {
    struct simplex *next; /* free list */
    long visit;           /* number of last site visiting this simplex */
    short mark;
    basis_s* normal;      /* normal vector pointing inward */
    neighbor peak;        /* if null, remaining vertices give facet */
    neighbor neigh[1];    /* neighbors of simplex */
} simplex;

typedef struct fg_node fg;
typedef struct tree_node Tree;
struct tree_node {
    Tree *left, *right;
    site key;
    int size;             /* maintained to be the number of nodes rooted here */
    fg *fgs;
    Tree *next;           /* freelist */
};

typedef struct fg_node {
    Tree *facets;
    double dist, vol;     /* of Voronoi face dual to this */
    fg *next;             /* freelist */
    short mark;
    int ref_count;
} fg_node;


typedef site gsitef(void *);
typedef long site_n(void *, site);

/* We use this instead of globals */
struct chull3d_data {
    size_t simplex_size;
    simplex *simplex_list;
    size_t basis_s_size;
    basis_s *basis_s_list;
    size_t Tree_size;
    Tree *Tree_list;
    int pdim;   /* point dimension */
    point *site_blocks;
    int num_blocks;
    simplex *ch_root;
    double Huge;
    int basis_vec_size;
    int exact_bits;
    float b_err_min;
    float b_err_min_sq;
    short vd;
    basis_s *tt_basis;
    basis_s *infinity_basis;
    Coord *hull_infinity;
    int *B;
    gsitef *get_site;
    site_n *site_num;
    fg *faces_gr_t;
    double mult_up;
    Coord *mins;
    Coord *maxs;
    site p;         /* the current site */
    long pnum;
    int rdim;       /* region dimension: (max) number of sites specifying region */
    int cdim;       /* number of sites currently specifying region */
    int site_size;  /* size of malloc needed for a site */
    int point_size; /* size of malloc needed for a point */
    short *mi;
    short *mo;
    char *tmpfilenam;
    int *face_cnt;
    int *face_array;
    int *vert_cnt;
    point_t *vert_array;
    int next_vert;

    /* these were static variables in functions */

    simplex **simplex_block_table;
    int num_simplex_blocks;
    basis_s **basis_s_block_table;
    int num_basis_s_blocks;
    Tree **Tree_block_table;
    int num_Tree_blocks;
    int lscale;
    double max_scale;
    double ldetbound;
    double Sb;
    neighbor p_neigh;
    basis_s *b;
    int fg_vn;
    long vnum;
    simplex *ns;
    simplex **st;
    long ss;
    long s_num;
    void *out_func_here;
};


#define NEARZERO(d)     ((d) < FLT_EPSILON && (d) > -FLT_EPSILON)
#define SWAP(X,a,b) {X t; t = a; a = b; b = t;}
#define VA(x) ((x)->vecs+cdata->rdim)
#define VB(x) ((x)->vecs)
#define trans(z,p,q) {int i; for (i=0;i<cdata->pdim;i++) z[i+cdata->rdim] = z[i] = p[i] - q[i];}
#define DELIFT 0
#define lift(cdata,z,s) {if (cdata->vd) z[2*(cdata->rdim)-1] =z[(cdata->rdim)-1]= ldexp(Vec_dot_pdim(cdata,z,z), -DELIFT);}
#define swap_points(a,b) {point t; t=a; a=b; b=t;}

/* This is the comparison.                                       */
/* Returns <0 if i<j, =0 if i=j, and >0 if i>j                   */
#define compare(cdata, i,j) (cdata->site_num((void *)cdata, i)-cdata->site_num((void *)cdata, j))

/* This macro returns the size of a node.  Unlike "x->size",     */
/* it works even if x=NULL.  The test could be avoided by using  */
/* a special version of NULL which was a real node with size 0.  */
#define node_size(x) ((x) ? ((x)->size) : 0 )

#define snkey(cdata, x) cdata->site_num((void *)cdata, (x)->vert)
#define push(x) *(cdata->st+tms++) = x;
#define pop(x)  x = *(cdata->st + --tms);

#define lookup(cdata, a,b,what,whatt) \
{                                     \
        int i;                        \
        neighbor *x;                  \
        for (i=0, x = a->neigh; (x->what != b) && (i<(cdata->cdim)) ; i++, x++); \
        if (i<(cdata->cdim))          \
          return x;                   \
        else {                        \
	  return 0;                   \
	}                             \
}

#define INCP(X,p,k) ((X*) ( (char*)p + (k) * X##_size)) /* portability? */

#define NEWL(cdata, list, X, p)                                 \
{                                                               \
        p = list ? list : new_block_##X(cdata, 1);              \
        assert(p);                                              \
        list = p->next;                                         \
}

#define NEWLRC(cdata, list, X, p)                               \
{                                                               \
        p = list ? list : new_block_##X(cdata, 1);              \
        assert(p);                                              \
        list = p->next;                                         \
        p->ref_count = 1;                                       \
}

#define FREEL(cdata, X,p)                                       \
{                                                               \
        memset((p),0,cdata->X##_size);                          \
        (p)->next = cdata->X##_list;                            \
        cdata->X##_list = p;                                    \
}

#define dec_ref(cdata, X,v)    {if ((v) && --(v)->ref_count == 0) FREEL(cdata, X,(v));}
#define inc_ref(X,v)    {if (v) v->ref_count++;}
#define NULLIFY(cdata, X,v)    {dec_ref(cdata, X,v); v = NULL;}

#define mod_refs(op,s)                                           \
{                                                                \
        int i;                                                   \
        neighbor *mrsn;                                          \
        for (i=-1,mrsn=s->neigh-1;i<(cdata->cdim);i++,mrsn++)    \
        op##_ref(basis_s, mrsn->basis);                          \
}

#define free_simp(cdata, s)                            \
{       mod_refs(dec,s);                               \
        FREEL(cdata, basis_s,s->normal);               \
        FREEL(cdata, simplex, s);                      \
}

#define copy_simp(cdata, new, s, simplex_list, simplex_size) \
{       NEWL(cdata, simplex_list, simplex, new);             \
        memcpy(new,s,simplex_size);                          \
        mod_refs(inc,s);                                     \
}

HIDDEN simplex *
new_block_simplex(struct chull3d_data *cdata, int make_blocks)
{
    int i;
    simplex *xlm, *xbt;
    if (make_blocks && cdata) {
	((cdata->num_simplex_blocks<max_blocks) ? (void) (0) : __assert_fail ("cdata->num_simplex_blocks<max_blocks", "chull3d.c", 31, __PRETTY_FUNCTION__));
	xbt = cdata->simplex_block_table[(cdata->num_simplex_blocks)++] = (simplex*)malloc(max_blocks * cdata->simplex_size);
	memset(xbt,0,max_blocks * cdata->simplex_size);
	((xbt) ? (void) (0) : __assert_fail ("xbt", "chull3d.c", 31, __PRETTY_FUNCTION__));
	xlm = ((simplex*) ( (char*)xbt + (max_blocks) * cdata->simplex_size));
	for (i=0; i<max_blocks; i++) {
	    xlm = ((simplex*) ( (char*)xlm + ((-1)) * cdata->simplex_size));
	    xlm->next = cdata->simplex_list;
	    cdata->simplex_list = xlm;
	}
	return cdata->simplex_list;
    };
    for (i=0; i<cdata->num_simplex_blocks; i++) free(cdata->simplex_block_table[i]);
    cdata->num_simplex_blocks = 0;
    if (cdata) cdata->simplex_list = 0;
    return 0;
}

HIDDEN void
free_simplex_storage(struct chull3d_data *cdata)
{
    new_block_simplex(cdata, 0);
}

HIDDEN basis_s *
new_block_basis_s(struct chull3d_data *cdata, int make_blocks)
{
    int i;
    basis_s *xlm, *xbt;
    if (make_blocks && cdata) {
	((cdata->num_basis_s_blocks<max_blocks) ? (void) (0) : __assert_fail ("num_basis_s_blocks<max_blocks", "chull3d.c", 32, __PRETTY_FUNCTION__));
	xbt = cdata->basis_s_block_table[(cdata->num_basis_s_blocks)++] = (basis_s*)malloc(max_blocks * cdata->basis_s_size);
	memset(xbt,0,max_blocks * cdata->basis_s_size);
	((xbt) ? (void) (0) : __assert_fail ("xbt", "chull3d.c", 32, __PRETTY_FUNCTION__));
	xlm = ((basis_s*) ( (char*)xbt + (max_blocks) * cdata->basis_s_size));
	for (i=0; i<max_blocks; i++) {
	    xlm = ((basis_s*) ( (char*)xlm + ((-1)) * cdata->basis_s_size));
	    xlm->next = cdata->basis_s_list;
	    cdata->basis_s_list = xlm;
	}
	return cdata->basis_s_list;
    };
    for (i=0; i<cdata->num_basis_s_blocks; i++) free(cdata->basis_s_block_table[i]);
    cdata->num_basis_s_blocks = 0;
    if (cdata) cdata->basis_s_list = 0;
    return 0;
}
HIDDEN void
free_basis_s_storage(struct chull3d_data *cdata)
{
    new_block_basis_s(cdata, 0);
}


HIDDEN Tree *
new_block_Tree(struct chull3d_data *cdata, int make_blocks)
{
    int i;
    Tree *xlm, *xbt;
    if (make_blocks && cdata) {
	((cdata->num_Tree_blocks<max_blocks) ? (void) (0) : __assert_fail ("num_Tree_blocks<max_blocks", "chull3d.c", 33, __PRETTY_FUNCTION__));
	xbt = cdata->Tree_block_table[cdata->num_Tree_blocks++] = (Tree*)malloc(max_blocks * cdata->Tree_size);
	memset(xbt,0,max_blocks * cdata->Tree_size);
	((xbt) ? (void) (0) : __assert_fail ("xbt", "chull3d.c", 33, __PRETTY_FUNCTION__));
	xlm = ((Tree*) ( (char*)xbt + (max_blocks) * cdata->Tree_size));
	for (i=0; i<max_blocks; i++) {
	    xlm = ((Tree*) ( (char*)xlm + ((-1)) * cdata->Tree_size));
	    xlm->next = cdata->Tree_list;
	    cdata->Tree_list = xlm;
	}
	return cdata->Tree_list;
    };
    for (i=0; i<cdata->num_Tree_blocks; i++) free(cdata->Tree_block_table[i]);
    cdata->num_Tree_blocks = 0;
    if (cdata) cdata->Tree_list = 0;
    return 0;
}
HIDDEN void
free_Tree_storage(struct chull3d_data *cdata)
{
    new_block_Tree(cdata, 0);
}

/* ch.c : numerical functions for hull computation */

HIDDEN Coord
Vec_dot(struct chull3d_data *cdata, point x, point y)
{
    int i;
    Coord sum = 0;
    for (i=0;i<cdata->rdim;i++) sum += x[i] * y[i];
    return sum;
}

HIDDEN Coord
Vec_dot_pdim(struct chull3d_data *cdata, point x, point y)
{
    int i;
    Coord sum = 0;
    for (i=0;i<cdata->pdim;i++) sum += x[i] * y[i];
    return sum;
}

HIDDEN Coord
Norm2(struct chull3d_data *cdata, point x)
{
    int i;
    Coord sum = 0;
    for (i=0;i<cdata->rdim;i++) sum += x[i] * x[i];
    return sum;
}

HIDDEN void
Ax_plus_y(struct chull3d_data *cdata, Coord a, point x, point y)
{
    int i;
    for (i=0;i<cdata->rdim;i++) {
	*y++ += a * *x++;
    }
}

HIDDEN void
Vec_scale(struct chull3d_data *cdata, int n, Coord a, Coord *x)
{
    register Coord *xx = x;
    register Coord *xend = xx + n;

    while (xx!=xend) {
	*xx *= a;
	xx++;
    }
}


/* amount by which to scale up vector, for reduce_inner */
HIDDEN double
sc(struct chull3d_data *cdata, basis_s *v,simplex *s, int k, int j)
{
    double labound;

    if (j<10) {
	labound = logb(v->sqa)/2;
	cdata->max_scale = cdata->exact_bits - labound - 0.66*(k-2)-1  -DELIFT;
	if (cdata->max_scale<1) {
	    cdata->max_scale = 1;
	}

	if (j==0) {
	    int	i;
	    neighbor *sni;
	    basis_s *snib;

	    cdata->ldetbound = DELIFT;

	    cdata->Sb = 0;
	    for (i=k-1,sni=s->neigh+k-1;i>0;i--,sni--) {
		snib = sni->basis;
		cdata->Sb += snib->sqb;
		cdata->ldetbound += logb(snib->sqb)/2 + 1;
		cdata->ldetbound -= snib->lscale;
	    }
	}
    }
    if (cdata->ldetbound - v->lscale + logb(v->sqb)/2 + 1 < 0) {
	return 0;
    } else {
	cdata->lscale = (int)floor(logb(2*cdata->Sb/(v->sqb + v->sqa*cdata->b_err_min)))/2;
	if (cdata->lscale > cdata->max_scale) {
	    cdata->lscale = (int)floor(cdata->max_scale);
	} else if (cdata->lscale<0) cdata->lscale = 0;
	v->lscale += cdata->lscale;
	return ( ((cdata->lscale)<20) ? 1<<(cdata->lscale) : ldexp(1,(cdata->lscale)) );
    }
}


HIDDEN int
reduce_inner(struct chull3d_data *cdata, basis_s *v, simplex *s, int k)
{
    point	va = VA(v);
    point	vb = VB(v);
    int	i,j;
    double	dd;
    double	scale;
    basis_s	*snibv;
    neighbor *sni;

    v->sqa = v->sqb = Norm2(cdata, vb);
    if (k<=1) {
	memcpy(vb,va,cdata->basis_vec_size);
	return 1;
    }
    for (j=0;j<250;j++) {

	memcpy(vb,va,cdata->basis_vec_size);
	for (i=k-1,sni=s->neigh+k-1;i>0;i--,sni--) {
	    snibv = sni->basis;
	    dd = -Vec_dot(cdata, VB(snibv),vb)/ snibv->sqb;
	    Ax_plus_y(cdata, dd, VA(snibv), vb);
	}
	v->sqb = Norm2(cdata, vb);
	v->sqa = Norm2(cdata, va);

	if (2*v->sqb >= v->sqa) {cdata->B[j]++; return 1;}

	Vec_scale(cdata, cdata->rdim,scale = sc(cdata,v,s,k,j),va);

	for (i=k-1,sni=s->neigh+k-1;i>0;i--,sni--) {
	    snibv = sni->basis;
	    dd = -Vec_dot(cdata, VB(snibv),va)/snibv->sqb;
	    dd = floor(dd+0.5);
	    Ax_plus_y(cdata, dd, VA(snibv), va);
	}
    }
    return 0;
}

HIDDEN int
reduce(struct chull3d_data *cdata, basis_s **v, point p, simplex *s, int k)
{
    point	z;
    point	tt = s->neigh[0].vert;

    if (!*v) NEWLRC(cdata, cdata->basis_s_list,basis_s,(*v))
    else (*v)->lscale = 0;
    z = VB(*v);
    if (cdata->vd) {
	if (p==cdata->hull_infinity) memcpy(*v,cdata->infinity_basis,cdata->basis_s_size);
	else {trans(z,p,tt); lift(cdata,z,s);}
    } else trans(z,p,tt);
    return reduce_inner(cdata,*v,s,k);
}

HIDDEN void
get_basis_sede(struct chull3d_data *cdata, simplex *s)
{
    int	k=1;
    neighbor *sn = s->neigh+1,
	     *sn0 = s->neigh;

    if (cdata->vd && sn0->vert == cdata->hull_infinity && (cdata->cdim) >1) {
	SWAP(neighbor, *sn0, *sn );
	NULLIFY(cdata, basis_s,sn0->basis);
	sn0->basis = cdata->tt_basis;
	cdata->tt_basis->ref_count++;
    } else {
	if (!sn0->basis) {
	    sn0->basis = cdata->tt_basis;
	    cdata->tt_basis->ref_count++;
	} else while (k < (cdata->cdim) && sn->basis) {k++;sn++;}
    }
    while (k<(cdata->cdim)) {
	NULLIFY(cdata, basis_s,sn->basis);
	reduce(cdata, &sn->basis,sn->vert,s,k);
	k++; sn++;
    }
}

HIDDEN int
out_of_flat(struct chull3d_data *cdata, simplex *root, point p)
{
    if (!cdata->p_neigh.basis) cdata->p_neigh.basis = (basis_s*) malloc(cdata->basis_s_size);
    cdata->p_neigh.vert = p;
    (cdata->cdim)++;
    root->neigh[(cdata->cdim)-1].vert = root->peak.vert;
    NULLIFY(cdata, basis_s,root->neigh[(cdata->cdim)-1].basis);
    get_basis_sede(cdata, root);
    if (cdata->vd && root->neigh[0].vert == cdata->hull_infinity) return 1;
    reduce(cdata, &cdata->p_neigh.basis,p,root,(cdata->cdim));
    if (cdata->p_neigh.basis->sqa != 0) return 1;
    (cdata->cdim)--;
    return 0;
}

HIDDEN int sees(struct chull3d_data *, site, simplex *);

HIDDEN void
get_normal_sede(struct chull3d_data *cdata, simplex *s)
{
    neighbor *rn;
    int i,j;

    get_basis_sede(cdata, s);
    if (cdata->rdim==3 && (cdata->cdim)==3) {
	point	c;
	point	a = VB(s->neigh[1].basis);
	point   b = VB(s->neigh[2].basis);
	NEWLRC(cdata, cdata->basis_s_list, basis_s,s->normal);
	c = VB(s->normal);
	c[0] = a[1]*b[2] - a[2]*b[1];
	c[1] = a[2]*b[0] - a[0]*b[2];
	c[2] = a[0]*b[1] - a[1]*b[0];
	s->normal->sqb = Norm2(cdata, c);
	for (i=(cdata->cdim)+1,rn = cdata->ch_root->neigh+(cdata->cdim)-1; i; i--, rn--) {
	    for (j = 0; j<(cdata->cdim) && rn->vert != s->neigh[j].vert;j++);
	    if (j<(cdata->cdim)) continue;
	    if (rn->vert==cdata->hull_infinity) {
		if (c[2] > -cdata->b_err_min) continue;
	    } else  if (!sees(cdata, rn->vert,s)) continue;
	    c[0] = -c[0]; c[1] = -c[1]; c[2] = -c[2];
	    break;
	}
	return;
    }

    for (i=(cdata->cdim)+1,rn = cdata->ch_root->neigh+(cdata->cdim)-1; i; i--, rn--) {
	for (j = 0; j<(cdata->cdim) && rn->vert != s->neigh[j].vert;j++);
	if (j<(cdata->cdim)) continue;
	reduce(cdata, &s->normal,rn->vert,s,(cdata->cdim));
	if (s->normal->sqb != 0) break;
    }
}


HIDDEN void get_normal(struct chull3d_data *cdata, simplex *s) {get_normal_sede(cdata,s); return;}

HIDDEN int
sees(struct chull3d_data *cdata, site p, simplex *s) {

    point	tt,zz;
    double	dd,dds;
    int i;

    if (!cdata->b) cdata->b = (basis_s*)malloc(cdata->basis_s_size);
    else cdata->b->lscale = 0;
    zz = VB(cdata->b);
    if ((cdata->cdim)==0) return 0;
    if (!s->normal) {
	get_normal_sede(cdata, s);
	for (i=0;i<(cdata->cdim);i++) NULLIFY(cdata, basis_s,s->neigh[i].basis);
    }
    tt = s->neigh[0].vert;
    if (cdata->vd) {
	if (p==cdata->hull_infinity) memcpy(cdata->b,cdata->infinity_basis,cdata->basis_s_size);
	else {trans(zz,p,tt); lift(cdata,zz,s);}
    } else trans(zz,p,tt);
    for (i=0;i<3;i++) {
	dd = Vec_dot(cdata, zz,s->normal->vecs);
	if (dd == 0.0) {
	    return 0;
	}
	dds = dd*dd/s->normal->sqb/Norm2(cdata, zz);
	if (dds > cdata->b_err_min_sq) return (dd<0);
	get_basis_sede(cdata, s);
	reduce_inner(cdata,cdata->b,s,(cdata->cdim));
    }
    return 0;
}

HIDDEN void
free_hull_storage(struct chull3d_data *cdata)
{
    free_basis_s_storage(cdata);
    free_simplex_storage(cdata);
    free_Tree_storage(cdata);
}

/*
   An implementation of top-down splaying with sizes
   D. Sleator <sleator@cs.cmu.edu>, January 1994.

   This extends top-down-splay.c to maintain a size field in each node.
   This is the number of nodes in the subtree rooted there.  This makes
   it possible to efficiently compute the rank of a key.  (The rank is
   the number of nodes to the left of the given key.)  It it also
   possible to quickly find the node of a given rank.  Both of these
   operations are illustrated in the code below.  The remainder of this
   introduction is taken from top-down-splay.c.

   "Splay trees", or "self-adjusting search trees" are a simple and
   efficient data structure for storing an ordered set.  The data
   structure consists of a binary tree, with no additional fields.  It
   allows searching, insertion, deletion, deletemin, deletemax,
   splitting, joining, and many other operations, all with amortized
   logarithmic performance.  Since the trees adapt to the sequence of
   requests, their performance on real access patterns is typically even
   better.  Splay trees are described in a number of texts and papers
   [1,2,3,4].

   The code here is adapted from simple top-down splay, at the bottom of
   page 669 of [2].  It can be obtained via anonymous ftp from
   spade.pc.cs.cmu.edu in directory /usr/sleator/public.

   The chief modification here is that the splay operation works even if the
   item being splayed is not in the tree, and even if the tree root of the
   tree is NULL.  So the line:

   t = splay(cdata, i, t);

   causes it to search for item with key i in the tree rooted at t.  If it's
   there, it is splayed to the root.  If it isn't there, then the node put
   at the root is the last one before NULL that would have been reached in a
   normal binary search for i.  (It's a neighbor of i in the tree.)  This
   allows many other operations to be easily implemented, as shown below.

   [1] "Data Structures and Their Algorithms", Lewis and Denenberg,
   Harper Collins, 1991, pp 243-251.
   [2] "Self-adjusting Binary Search Trees" Sleator and Tarjan,
   JACM Volume 32, No 3, July 1985, pp 652-686.
   [3] "Data Structure and Algorithm Analysis", Mark Weiss,
   Benjamin Cummins, 1992, pp 119-130.
   [4] "Data Structures, Algorithms, and Performance", Derick Wood,
   Addison-Wesley, 1993, pp 367-375

   Adding the following notice, which appears in the version of this file at
   http://www.cs.cmu.edu/afs/cs.cmu.edu/user/sleator/public/splaying/ but
   did not originally appear in the hull version:

   The following code was written by Daniel Sleator, and is released
   in the public domain.

   Fri Oct 21 21:15:01 EDT 1994
   style changes, removed Sedgewickized...
   Ken Clarkson

   The reworking for hull, insofar as copyright protection applies, would
   thus be under the same license as hull.
*/


/* Splay using the key i (which may or may not be in the tree.)
   The starting root is t, and the tree used is defined by rat
   size fields are maintained */
HIDDEN Tree *
splay(struct chull3d_data *cdata, site i, Tree *t)
{
    Tree N, *l, *r, *y;
    int comp, root_size, l_size, r_size;

    if (!t) return t;
    N.left = N.right = NULL;
    l = r = &N;
    root_size = node_size(t);
    l_size = r_size = 0;

    for (;;) {
	comp = compare(cdata, i, t->key);
	if (comp < 0) {
	    if (!t->left) break;
	    if (compare(cdata, i, t->left->key) < 0) {
		y = t->left;                           /* rotate right */
		t->left = y->right;
		y->right = t;
		t->size = node_size(t->left) + node_size(t->right) + 1;
		t = y;
		if (!t->left) break;
	    }
	    r->left = t;                               /* link right */
	    r = t;
	    t = t->left;
	    r_size += 1+node_size(r->right);
	} else if (comp > 0) {
	    if (!t->right) break;
	    if (compare(cdata, i, t->right->key) > 0) {
		y = t->right;                          /* rotate left */
		t->right = y->left;
		y->left = t;
		t->size = node_size(t->left) + node_size(t->right) + 1;
		t = y;
		if (!t->right) break;
	    }
	    l->right = t;                              /* link left */
	    l = t;
	    t = t->right;
	    l_size += 1+node_size(l->left);
	} else break;
    }
    l_size += node_size(t->left);  /* Now l_size and r_size are the sizes of */
    r_size += node_size(t->right); /* the left and right trees we just built.*/
    t->size = l_size + r_size + 1;

    l->right = r->left = NULL;

    /* The following two loops correct the size fields of the right path  */
    /* from the left child of the root and the right path from the left   */
    /* child of the root.                                                 */
    for (y = N.right; y != NULL; y = y->right) {
	y->size = l_size;
	l_size -= 1+node_size(y->left);
    }
    for (y = N.left; y != NULL; y = y->left) {
	y->size = r_size;
	r_size -= 1+node_size(y->right);
    }

    l->right = t->left;                                /* assemble */
    r->left = t->right;
    t->left = N.right;
    t->right = N.left;

    return t;
}

/* Insert key i into the tree t, if it is not already there. */
/* Return a pointer to the resulting tree.                   */
HIDDEN Tree *
insert(struct chull3d_data *cdata, site i, Tree * t)
{
    Tree * new;

    if (t != NULL) {
	t = splay(cdata, i,t);
	if (compare(cdata, i, t->key)==0) {
	    return t;  /* it's already there */
	}
    }
    NEWL(cdata, cdata->Tree_list, Tree,new)
	if (!t) {
	    new->left = new->right = NULL;
	} else if (compare(cdata, i, t->key) < 0) {
	    new->left = t->left;
	    new->right = t;
	    t->left = NULL;
	    t->size = 1+node_size(t->right);
	} else {
	    new->right = t->right;
	    new->left = t;
	    t->right = NULL;
	    t->size = 1+node_size(t->left);
	}
    new->key = i;
    new->size = 1 + node_size(new->left) + node_size(new->right);
    return new;
}

/* Deletes i from the tree if it's there.               */
/* Return a pointer to the resulting tree.              */
HIDDEN Tree *
delete(struct chull3d_data *cdata, site i, Tree *t)
{
    Tree * x;
    int tsize;

    if (!t) return NULL;
    tsize = t->size;
    t = splay(cdata, i,t);
    if (compare(cdata, i, t->key) == 0) {               /* found it */
	if (!t->left) {
	    x = t->right;
	} else {
	    x = splay(cdata, i, t->left);
	    x->right = t->right;
	}
	FREEL(cdata, Tree,t);
	if (x) x->size = tsize-1;
	return x;
    } else {
	return t;                         /* It wasn't there */
    }
}

/* Returns a pointer to the node in the tree with the given rank.  */
/* Returns NULL if there is no such node.                          */
/* Does not change the tree.  To guarantee logarithmic behavior,   */
/* the node found here should be splayed to the root.              */
HIDDEN Tree *
find_rank(int r, Tree *t)
{
    int lsize;
    if ((r < 0) || (r >= node_size(t))) return NULL;
    for (;;) {
	lsize = node_size(t->left);
	if (r < lsize) {
	    t = t->left;
	} else if (r > lsize) {
	    r = r - lsize -1;
	    t = t->right;
	} else {
	    return t;
	}
    }
}

HIDDEN void
visit_fg_i(struct chull3d_data *cdata, void (*v_fg)(struct chull3d_data *,Tree *, int, int),
	Tree *t, int depth, int vn, int boundary)
{
    int	boundaryc = boundary;

    if (!t) return;

    assert(t->fgs);
    if (t->fgs->mark!=vn) {
	t->fgs->mark = vn;
	if (t->key!=cdata->hull_infinity && !cdata->mo[cdata->site_num((void *)cdata, t->key)]) boundaryc = 0;
	v_fg(cdata, t,depth, boundaryc);
	visit_fg_i(cdata, v_fg, t->fgs->facets,depth+1, vn, boundaryc);
    }
    visit_fg_i(cdata, v_fg, t->left,depth,vn, boundary);
    visit_fg_i(cdata, v_fg, t->right,depth,vn,boundary);
}

HIDDEN void
visit_fg(struct chull3d_data *cdata, fg *faces_gr, void (*v_fg)(struct chull3d_data *, Tree *, int, int))
{
    visit_fg_i(cdata, v_fg, faces_gr->facets, 0, ++(cdata->fg_vn), 1);
}

HIDDEN int
visit_fg_i_far(struct chull3d_data *cdata, void (*v_fg)(struct chull3d_data *, Tree *, int),
	Tree *t, int depth, int vn)
{
    int nb = 0;

    if (!t) return 0;

    assert(t->fgs);
    if (t->fgs->mark!=vn) {
	t->fgs->mark = vn;
	nb = (t->key==cdata->hull_infinity) || cdata->mo[cdata->site_num((void *)cdata, t->key)];
	if (!nb && !visit_fg_i_far(cdata, v_fg, t->fgs->facets,depth+1,vn))
	    v_fg(cdata, t,depth);
    }
    nb = visit_fg_i_far(cdata, v_fg, t->left,depth,vn) || nb;
    nb = visit_fg_i_far(cdata, v_fg, t->right,depth,vn) || nb;
    return nb;
}

HIDDEN void
visit_fg_far(struct chull3d_data *cdata, fg *faces_gr, void (*v_fg)(struct chull3d_data *, Tree *, int))
{
    visit_fg_i_far(cdata, v_fg,faces_gr->facets, 0, --(cdata->fg_vn));
}

/* hull.c : "combinatorial" functions for hull computation */

typedef int test_func(struct chull3d_data *, simplex *, int, void *);
typedef void* visit_func(struct chull3d_data *, simplex *, void *);

/* starting at s, visit simplices t such that test(s,i,0) is true,
 * and t is the i'th neighbor of s;
 * apply visit function to all visited simplices;
 * when visit returns nonNULL, exit and return its value */
HIDDEN void *
visit_triang_gen(struct chull3d_data *cdata, simplex *s, visit_func *visit, test_func *test)
{
    neighbor *sn;
    void *v;
    simplex *t;
    int i;
    long tms = 0;

    (cdata->vnum)--;
    if (s) push(s);
    while (tms) {
	if (tms>cdata->ss) {
	    cdata->st=(simplex**)realloc(cdata->st,((cdata->ss+=cdata->ss)+MAXDIM+1)*sizeof(simplex*));
	}
	pop(t);
	if (!t || t->visit == cdata->vnum) continue;
	t->visit = cdata->vnum;
	if ((v=(*visit)(cdata,t,0))) {return v;}
	for (i=-1,sn = t->neigh-1;i<(cdata->cdim);i++,sn++)
	    if ((sn->simp->visit != cdata->vnum) && sn->simp && test(cdata,t,i,0))
		push(sn->simp);
    }
    return NULL;
}

/* visit the whole triangulation */
HIDDEN int truet(struct chull3d_data *cdata, simplex *s, int i, void *dum) {return 1;}
HIDDEN void *
visit_triang(struct chull3d_data *cdata, simplex *root, visit_func *visit)
{
    return visit_triang_gen(cdata, root, visit, truet);
}


HIDDEN int hullt(struct chull3d_data *cdata, simplex *s, int i, void *dummy) {return i>-1;}
HIDDEN void *facet_test(struct chull3d_data *cdata, simplex *s, void *dummy) {return (!s->peak.vert) ? s : NULL;}
HIDDEN void *
visit_hull(struct chull3d_data *cdata, simplex *root, visit_func *visit)
{
    return visit_triang_gen(cdata, visit_triang(cdata, root, &facet_test), visit, hullt);
}

HIDDEN void *
check_simplex(struct chull3d_data *cdata, simplex *s, void *dum)
{
    int i,j,k,l;
    neighbor *sn, *snn, *sn2;
    simplex *sns;
    site vn;

    for (i=-1,sn=s->neigh-1;i<(cdata->cdim);i++,sn++) {
	sns = sn->simp;
	if (!sns) {
	    return s;
	}
	if (!s->peak.vert && sns->peak.vert && i!=-1) {
	    exit(1);
	}
	for (j=-1,snn=sns->neigh-1; j<(cdata->cdim) && snn->simp!=s; j++,snn++);
	if (j==(cdata->cdim)) {
	    exit(1);
	}
	for (k=-1,snn=sns->neigh-1; k<(cdata->cdim); k++,snn++){
	    vn = snn->vert;
	    if (k!=j) {
		for (l=-1,sn2 = s->neigh-1;
			l<(cdata->cdim) && sn2->vert != vn;
			l++,sn2++);
		if (l==(cdata->cdim)) {
		    exit(1);
		}
	    }
	}
    }
    return NULL;
}

/* outfuncs: given a list of points, output in a given format */


FILE* efopen(struct chull3d_data *cdata, char *file, char *mode) {
    FILE* fp;
    if ((fp = fopen(file, mode))) return fp;
    exit(1);
    return NULL;
}


#ifndef WIN32
FILE* epopen(char *com, char *mode) {
    FILE* fp;
    if ((fp = popen(com, mode))) return fp;
    fprintf(stderr, "couldn't open stream %s mode %s\n",com,mode);
    exit(1);
    return 0;
}
#endif


void off_out(struct chull3d_data *cdata, point *v, int vdim, FILE *Fin, int amble) {

    static FILE *F, *G;
    static FILE *OFFFILE;
    static char offfilenam[MAXPATHLEN];
    char comst[100], buf[200];
    int j,i;

    if (Fin) {F=Fin;}

    if (amble==0) {
	for (i=0;i<vdim;i++) if (v[i]==cdata->hull_infinity) return;
	fprintf(OFFFILE, "%d ", vdim);
	for (j=0;j<vdim;j++) {
	    fprintf(OFFFILE, "%ld ", (cdata->site_num)((void *)cdata, v[j]));
	}
	fprintf(OFFFILE,"\n");
    } else if (amble==-1) {
	OFFFILE = bu_temp_file((char *)&offfilenam, MAXPATHLEN);
    } else {
	fclose(OFFFILE);

	fprintf(F, "OFF\n");

	sprintf(comst, "wc %s", cdata->tmpfilenam);
	G = epopen(comst, "r");
	fscanf(G, "%d", &i);
	fprintf(F, " %d", i);
	pclose(G);

	sprintf(comst, "wc %s", offfilenam);
	G = epopen(comst, "r");
	fscanf(G, "%d", &i);
	fprintf(F, " %d", i);
	pclose(G);

	fprintf (F, " 0\n");


	G = efopen(cdata, cdata->tmpfilenam, "r");
	while (fgets(buf, sizeof(buf), G)) {
	    fprintf(F, "%s", buf);
	}
	fclose(G);

	G = efopen(cdata, offfilenam, "r");
	while (fgets(buf, sizeof(buf), G)) {
	    fprintf(F, "%s", buf);
	}
	fclose(G);
    }

}


/* vist_funcs for different kinds of output: facets, alpha shapes, etc. */
typedef void out_func(struct chull3d_data *, point *, int, FILE*, int);

void *facets_print(struct chull3d_data *cdata, simplex *s, void *p) {

    out_func *out_func_here = (out_func *)cdata->out_func_here;
    point v[MAXDIM];
    int j;

    if (p) {out_func_here = (out_func*)p; if (!s) return NULL;}

    for (j=0;j<(cdata->cdim);j++) v[j] = s->neigh[j].vert;

    out_func_here(cdata, v,(cdata->cdim),0,0);

    return NULL;
}

#if 0
/* swap this with facets_print when printing out triangulation */
void *ridges_print(struct chull3d_data *cdata, simplex *s, void *p) {

    out_func *out_func_here = (out_func *)cdata->out_func_here;
    point v[MAXDIM];
    int j,k,vnum;

    if (p) {out_func_here = (out_func*)p; if (!s) return NULL;}

    for (j=0;j<(cdata->cdim);j++) {
	vnum=0;
	for (k=0;k<(cdata->cdim);k++) {
	    if (k==j) continue;
	    v[vnum++] = (s->neigh[k].vert);
	}
	out_func_here(cdata, v,(cdata->cdim)-1,0,0);
    }
    return NULL;
}
#endif

/* TODO -make contingent on test */
#ifdef WIN32
double logb(double x) {
    if (x<=0) return -1e305;
    return log(x)/log(2);
}
#endif

HIDDEN Coord
maxdist(int dim, point p1, point p2)
{
    Coord x = 0;
    Coord y = 0;
    Coord d = 0;
    int i = dim;
    while (i--) {
	x = *p1++;
	y = *p2++;
	d += (x<y) ? y-x : x-y ;
    }
    return d;
}

/* the neighbor entry of a containing b */
HIDDEN neighbor *
op_simp(struct chull3d_data *cdata, simplex *a, simplex *b)
{
    lookup(cdata, a,b,simp,simplex)
}
HIDDEN neighbor *
op_vert(struct chull3d_data *cdata, simplex *a, site b)
{
    lookup(cdata, a,b,vert,site)
}


/* make neighbor connections between newly created simplices incident to p */
HIDDEN void
connect(struct chull3d_data *cdata, simplex *s)
{
    site xf,xb,xfi;
    simplex *sb, *sf, *seen;
    int i;
    neighbor *sn;

    if (!s) return;
    assert(!s->peak.vert
	    && s->peak.simp->peak.vert==cdata->p
	    && !op_vert(cdata,s,cdata->p)->simp->peak.vert);
    if (s->visit==cdata->pnum) return;
    s->visit = cdata->pnum;
    seen = s->peak.simp;
    xfi = op_simp(cdata,seen,s)->vert;
    for (i=0, sn = s->neigh; i<(cdata->cdim); i++,sn++) {
	xb = sn->vert;
	if (cdata->p == xb) continue;
	sb = seen;
	sf = sn->simp;
	xf = xfi;
	if (!sf->peak.vert) {	/* are we done already? */
	    sf = op_vert(cdata,seen,xb)->simp;
	    if (sf->peak.vert) continue;
	} else do {
	    xb = xf;
	    xf = op_simp(cdata,sf,sb)->vert;
	    sb = sf;
	    sf = op_vert(cdata,sb,xb)->simp;
	} while (sf->peak.vert);

	sn->simp = sf;
	op_vert(cdata,sf,xf)->simp = s;

	connect(cdata, sf);
    }
}


/* visit simplices s with sees(p,s), and make a facet for every neighbor
 * of s not seen by p */
HIDDEN simplex *
make_facets(struct chull3d_data *cdata, simplex *seen)
{
    simplex *n;
    neighbor *bn;
    int i;


    if (!seen) return NULL;
    seen->peak.vert = cdata->p;

    for (i=0,bn = seen->neigh; i<(cdata->cdim); i++,bn++) {
	n = bn->simp;
	if (cdata->pnum != n->visit) {
	    n->visit = cdata->pnum;
	    if (sees(cdata, cdata->p,n)) make_facets(cdata, n);
	}
	if (n->peak.vert) continue;
	copy_simp(cdata, cdata->ns,seen,cdata->simplex_list,cdata->simplex_size);
	cdata->ns->visit = 0;
	cdata->ns->peak.vert = 0;
	cdata->ns->normal = 0;
	cdata->ns->peak.simp = seen;
	NULLIFY(cdata, basis_s,cdata->ns->neigh[i].basis);
	cdata->ns->neigh[i].vert = cdata->p;
	bn->simp = op_simp(cdata,n,seen)->simp = cdata->ns;
    }
    return cdata->ns;
}


/* p lies outside flat containing previous sites;
 * make p a vertex of every current simplex, and create some new simplices */
HIDDEN simplex *
extend_simplices(struct chull3d_data *cdata, simplex *s)
{
    int	i;
    int ocdim=(cdata->cdim)-1;
    simplex *ns;
    neighbor *nsn;

    if (s->visit == cdata->pnum) return s->peak.vert ? s->neigh[ocdim].simp : s;
    s->visit = cdata->pnum;
    s->neigh[ocdim].vert = cdata->p;
    NULLIFY(cdata, basis_s,s->normal);
    NULLIFY(cdata, basis_s,s->neigh[0].basis);
    if (!s->peak.vert) {
	s->neigh[ocdim].simp = extend_simplices(cdata, s->peak.simp);
	return s;
    } else {
	copy_simp(cdata, ns,s,cdata->simplex_list,cdata->simplex_size);
	s->neigh[ocdim].simp = ns;
	ns->peak.vert = NULL;
	ns->peak.simp = s;
	ns->neigh[ocdim] = s->peak;
	inc_ref(basis_s,s->peak.basis);
	for (i=0,nsn=ns->neigh;i<(cdata->cdim);i++,nsn++)
	    nsn->simp = extend_simplices(cdata, nsn->simp);
    }
    return ns;
}


/* return a simplex s that corresponds to a facet of the
 * current hull, and sees(p, s) */
HIDDEN simplex *
search(struct chull3d_data *cdata, simplex *root)
{
    simplex *s;
    neighbor *sn;
    int i;
    long tms = 0;

    push(root->peak.simp);
    root->visit = cdata->pnum;
    if (!sees(cdata, cdata->p,root))
	for (i=0,sn=root->neigh;i<(cdata->cdim);i++,sn++) push(sn->simp);
    while (tms) {
	if (tms>cdata->ss)
	    cdata->st=(simplex**)realloc(cdata->st, ((cdata->ss+=cdata->ss)+MAXDIM+1)*sizeof(simplex*));
	pop(s);
	if (s->visit == cdata->pnum) continue;
	s->visit = cdata->pnum;
	if (!sees(cdata, cdata->p,s)) continue;
	if (!s->peak.vert) return s;
	for (i=0, sn=s->neigh; i<(cdata->cdim); i++,sn++) push(sn->simp);
    }
    return NULL;
}


HIDDEN point
get_another_site(struct chull3d_data *cdata)
{
    point pnext;
    pnext = (*(cdata->get_site))((void *)cdata);
    if (!pnext) return NULL;
    cdata->pnum = cdata->site_num((void *)cdata, pnext)+2;
    return pnext;
}



HIDDEN void
buildhull(struct chull3d_data *cdata, simplex *root)
{
    while ((cdata->cdim) < cdata->rdim) {
	cdata->p = get_another_site(cdata);
	if (!cdata->p) return;
	if (out_of_flat(cdata, root,cdata->p))
	    extend_simplices(cdata, root);
	else
	    connect(cdata, make_facets(cdata, search(cdata, root)));
    }
    while ((cdata->p = get_another_site(cdata)))
	connect(cdata, make_facets(cdata, search(cdata, root)));
}


/*
   get_s             returns next site each call;
   hull construction stops when NULL returned;
   site_numm         returns number of site when given site;
   dim               dimension of point set;
   vdd               if (vdd) then return Delaunay triangulation
*/
HIDDEN simplex *
build_convex_hull(struct chull3d_data *cdata, gsitef *get_s, site_n *site_numm, short dim, short vdd)
{
    simplex *s, *root;

    if (!cdata->Huge) cdata->Huge = DBL_MAX*DBL_MAX;

    (cdata->cdim) = 0;
    cdata->get_site = get_s;
    cdata->site_num = site_numm;
    cdata->pdim = dim;

    cdata->exact_bits = (int)floor(DBL_MANT_DIG*log(FLT_RADIX)/log(2));
    cdata->b_err_min = (float)(DBL_EPSILON*MAXDIM*(1<<MAXDIM)*MAXDIM*3.01);
    cdata->b_err_min_sq = cdata->b_err_min * cdata->b_err_min;

    assert(cdata->get_site!=NULL); assert(cdata->site_num!=NULL);

    cdata->rdim = cdata->vd ? cdata->pdim+1 : cdata->pdim;

    cdata->point_size = cdata->site_size = sizeof(Coord)*cdata->pdim;
    cdata->basis_vec_size = sizeof(Coord)*cdata->rdim;
    cdata->basis_s_size = sizeof(basis_s)+ (2*cdata->rdim-1)*sizeof(Coord);
    cdata->simplex_size = sizeof(simplex) + (cdata->rdim-1)*sizeof(neighbor);
    cdata->Tree_size = sizeof(Tree);

    root = NULL;
    if (cdata->vd) {
	cdata->p = cdata->hull_infinity;
	NEWLRC(cdata, cdata->basis_s_list, basis_s, cdata->infinity_basis);
	cdata->infinity_basis->vecs[2*cdata->rdim-1]
	    = cdata->infinity_basis->vecs[cdata->rdim-1]
	    = 1;
	cdata->infinity_basis->sqa
	    = cdata->infinity_basis->sqb
	    = 1;
    } else if (!(cdata->p = (*(cdata->get_site))((void *)cdata))) return 0;

    NEWL(cdata, cdata->simplex_list, simplex,root);

    cdata->ch_root = root;

    copy_simp(cdata, s,root,cdata->simplex_list,cdata->simplex_size);
    root->peak.vert = cdata->p;
    root->peak.simp = s;
    s->peak.simp = root;

    buildhull(cdata, root);
    return root;
}


/* hullmain.c */

FILE *OUTFILE, *TFILE;

HIDDEN long
site_numm(void *data, site p)
{
    long i,j;
    struct chull3d_data *cdata = (struct chull3d_data *)data;

    if (cdata->vd && p==cdata->hull_infinity) return -1;
    if (!p) return -2;
    for (i=0; i<cdata->num_blocks; i++)
	if ((j=p-cdata->site_blocks[i])>=0 && j < BLOCKSIZE*cdata->pdim)
	    return j/cdata->pdim+BLOCKSIZE*i;
    return -3;
}


HIDDEN site
new_site(struct chull3d_data *cdata, site p, long j)
{
    assert(cdata->num_blocks+1<MAXBLOCKS);
    if (0==(j%BLOCKSIZE)) {
	assert(cdata->num_blocks < MAXBLOCKS);
	return(cdata->site_blocks[cdata->num_blocks++]=(site)malloc(BLOCKSIZE*cdata->site_size));
    } else
	return p+cdata->pdim;
}

HIDDEN site
read_next_site(struct chull3d_data *cdata, long j)
{
    int i=0, k=0;

    cdata->p = new_site(cdata, cdata->p,j);

    if (cdata->next_vert == 9) return 0;

    cdata->p[0] = cdata->vert_array[cdata->next_vert][0];
    cdata->p[1] = cdata->vert_array[cdata->next_vert][1];
    cdata->p[2] = cdata->vert_array[cdata->next_vert][2];
    fprintf(TFILE, "%f %f %f\n", cdata->p[0], cdata->p[1], cdata->p[2]);
    (cdata->next_vert)++;
    for(i = 0; i < 3; i++) {
	(cdata->p)[i] = floor(cdata->mult_up*(cdata->p)[i]+0.5);
	cdata->mins[i] = (cdata->mins[i]<(cdata->p)[i]) ? cdata->mins[i] : (cdata->p)[i];
	cdata->maxs[i] = (cdata->maxs[i]>(cdata->p)[i]) ? cdata->maxs[i] : (cdata->p)[i];
    }

    return cdata->p;
}

HIDDEN site
get_next_site(void *data) {
    return read_next_site((struct chull3d_data *)data, (((struct chull3d_data *)data)->s_num)++);
}

HIDDEN void
chull3d_data_init(struct chull3d_data *data)
{
    int i = 0;
    data->simplex_list = 0;
    data->basis_s_list = 0;
    data->Tree_list = 0;
    data->site_blocks = (point *)bu_calloc(MAXBLOCKS, sizeof(point), "site_blocks");
    BU_GET(data->tt_basis, basis_s);
    data->tt_basis->next = 0;
    data->tt_basis->ref_count = 1;
    data->tt_basis->lscale = -1;
    data->tt_basis->sqa = 0;
    data->tt_basis->sqb = 0;
    data->tt_basis->vecs[0] = 0;
    /* point at infinity for Delaunay triang; value not used */
    data->hull_infinity = (Coord *)bu_calloc(10, sizeof(Coord), "hull_infinity");
    data->hull_infinity[0] = 57.2;
    data->B = (int *)bu_calloc(100, sizeof(int), "A");
    data->mins = (Coord *)bu_calloc(MAXDIM, sizeof(Coord), "mins");
    for(i = 0; i < MAXDIM; i++) data->mins[i] = DBL_MAX;
    data->maxs = (Coord *)bu_calloc(MAXDIM, sizeof(Coord), "maxs");
    for(i = 0; i < MAXDIM; i++) data->maxs[i] = -DBL_MAX;
    data->mult_up = 1000.0; /* we'll need to multiply based on a tolerance parameter at some point... */
    data->mi = (short *)bu_calloc(MAXPOINTS, sizeof(short), "mi");
    data->mo = (short *)bu_calloc(MAXPOINTS, sizeof(short), "mo");
    data->tmpfilenam = (char *)bu_calloc(MAXPATHLEN, sizeof(char), "tmpfilenam");
    data->pdim = 3;
    data->vd = 0;  /* we're not using the triangulation by default */

    /* These were static variables in functions */

    data->simplex_block_table = (simplex **)bu_calloc(100, sizeof(simplex *), "simplex_block_table");
    data->num_simplex_blocks = 0;
    data->basis_s_block_table = (basis_s **)bu_calloc(100, sizeof(basis_s *), "basis_s_block_table");
    data->num_basis_s_blocks = 0;
    data->Tree_block_table = (Tree **)bu_calloc(100, sizeof(Tree *), "Tree_block_table");
    data->num_Tree_blocks = 0;
    data->p_neigh.vert = 0;
    data->p_neigh.simp = NULL;
    data->p_neigh.basis = NULL;
    data->b = NULL;
    data->vnum = -1;
    data->ss = 2000 + MAXDIM;
    data->s_num = 0;
    data->out_func_here = (void *)off_out;
    data->fg_vn = 0;
    data->st=(simplex**)malloc((data->ss+MAXDIM+1)*sizeof(simplex*));
    data->ns = NULL;

    data->vert_array = (point_t *)bu_calloc(9, sizeof(point_t), "vertex array");
    data->next_vert = 0;
}

HIDDEN void
chull3d_data_free(struct chull3d_data *data)
{
    bu_free(data->site_blocks, "site_blocks");
    bu_free(data->hull_infinity, "hull_infinity");
    bu_free(data->B, "B");
    bu_free(data->mins, "mins");
    bu_free(data->maxs, "maxs");
    bu_free(data->mi, "mi");
    bu_free(data->mo, "mo");
    bu_free(data->simplex_block_table, "simplex_block_table");
    bu_free(data->basis_s_block_table, "basis_s_block_table");
    bu_free(data->Tree_block_table, "Tree_block_table");
    BU_PUT(data->tt_basis, basis_s);
    bu_free(data->tmpfilenam, "tmpfilenam");
}

int
bn_3d_chull(int *faces, int *num_faces, point_t **vertices, int *num_vertices,
            const point_t *input_points_3d, int num_input_pnts)
{
    struct chull3d_data *cdata;
    simplex *root = NULL;

    BU_GET(cdata, struct chull3d_data);
    chull3d_data_init(cdata);
    root = build_convex_hull(cdata, get_next_site, site_numm, cdata->pdim, cdata->vd);

    if (!root) return -1;

    off_out(cdata, 0,0,stdout,-1);
    facets_print(cdata, 0, off_out);
    visit_hull(cdata, root, facets_print);
    off_out(cdata, 0,0,stdout,1);

    free_hull_storage(cdata);
    chull3d_data_free(cdata);
    BU_PUT(cdata, struct chull3d_data);

    return 0;
}


extern char *optarg;
extern int optind;
extern int opterr;

/* TODO - replace this with a top level function using libbn types */
int main(int argc, char **argv) {

    short ifn = 0;
    int	option;
    char ifile[50] = "";
    simplex *root;


    struct chull3d_data *cdata;
    BU_GET(cdata, struct chull3d_data);
    chull3d_data_init(cdata);

    while ((option = getopt(argc, argv, "i:")) != EOF) {
	switch (option) {
	    case 'i':
		strcpy(ifile, optarg);
		break;
	    default:
		exit(1);
	}
    }

    VSET(cdata->vert_array[0], 0.0, 0.0, 0.0);
    VSET(cdata->vert_array[1], 2.0, 0.0, 0.0);
    VSET(cdata->vert_array[2], 2.0, 2.0, 0.0);
    VSET(cdata->vert_array[3], 0.0, 2.0, 0.0);
    VSET(cdata->vert_array[4], 0.0, 0.0, 2.0);
    VSET(cdata->vert_array[5], 2.0, 0.0, 2.0);
    VSET(cdata->vert_array[6], 2.0, 2.0, 2.0);
    VSET(cdata->vert_array[7], 0.0, 2.0, 2.0);
    VSET(cdata->vert_array[8], 1.0, 1.0, 1.0);

    OUTFILE = stdout;

    TFILE = bu_temp_file(cdata->tmpfilenam, MAXPATHLEN);

    /*read_next_site(cdata, -1);*/

    cdata->point_size = cdata->site_size = sizeof(Coord)*cdata->pdim;

    root = build_convex_hull(cdata, get_next_site, site_numm, cdata->pdim, cdata->vd);

    fclose(TFILE);

    off_out(cdata, 0,0,stdout,-1);
    facets_print(cdata, 0, off_out);
    visit_hull(cdata, root, facets_print);
    off_out(cdata, 0,0,stdout,1);

    free_hull_storage(cdata);
    chull3d_data_free(cdata);
    BU_PUT(cdata, struct chull3d_data);

    exit(0);
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

