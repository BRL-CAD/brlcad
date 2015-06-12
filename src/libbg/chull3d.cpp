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

#include <map>

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
#include "bu/log.h"
#include "bu/ptbl.h"
#include "bu/malloc.h"
#include "bn/randmt.h"
#include "bg/chull.h"

#define BLOCKSIZE 100000
#define MAXDIM 3

/* It is faster, when dealing with large point sets, to
 * repeatedly calculate the convex hull for subsets of
 * the total set and then fine the hull of that set of
 * hulls.  These parameters control that process - see
 * chull3d_intermediate_set and bg_3d_chull.  In the
 * worst case this approach will slow the calculation,
 * but so long as some significant percentage of the
 * bot's vertices are not part of the hull it should
 * help */
#define CHULL3D_COUNT_THRESHOLD 1000
#define CHULL3D_DELTA_THRESHOLD 200
#define CHULL3D_SUBSET_SIZE 275

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
    long *shufmat;

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
    std::map<long *, int> *point_lookup;
    int free_point_lookup;

    /* libbn style inputs and outputs */
    const point_t *input_vert_array;
    int input_vert_cnt;
    struct bu_ptbl *output_pnts;
    int free_output_pnts;
    int next_vert;
    point_t center_pnt;

    /* Output containers */
    point_t *vert_array;
    int *vert_cnt;
    int *faces;
    int *face_cnt;
};


#define CHULL3D_VA(x) ((x)->vecs+cdata->rdim)
#define CHULL3D_VB(x) ((x)->vecs)
#define CHULL3D_trans(z,p,q) {int ti; for (ti=0;ti<cdata->pdim;ti++) z[ti+cdata->rdim] = z[ti] = p[ti] - q[ti];}
#define CHULL3D_DELIFT 0

#define CHULL3D_push(x) *(cdata->st+tms++) = x;
#define CHULL3D_pop(x)  x = *(cdata->st + --tms);

#define CHULL3D_NEWL(cdata, list, X, p)                                 \
{                                                               \
    p = list ? list : chull3d_new_block_##X(cdata, 1);              \
    /*assert(p);*/                                              \
    list = p->next;                                         \
}

#define CHULL3D_NEWLRC(cdata, list, X, p)                               \
{                                                               \
    p = list ? list : chull3d_new_block_##X(cdata, 1);              \
    /*assert(p);*/                                             \
    list = p->next;                                         \
    p->ref_count = 1;                                       \
}

#define CHULL3D_NULLIFY(cdata, X,v)    { \
    if ((v) && --(v)->ref_count == 0) { \
	memset((v),0,cdata->X##_size);                          \
	(v)->next = cdata->X##_list;                            \
	cdata->X##_list = v;                                    \
    } \
    v = NULL;\
}

#define CHULL3D_copy_simp(cdata, cnew, s) \
{ \
    int mi;                                                   \
    neighbor *mrsn;                                          \
    CHULL3D_NEWL(cdata, cdata->simplex_list, simplex, cnew);             \
    memcpy(cnew,s,cdata->simplex_size);                          \
    for (mi=-1,mrsn=s->neigh-1;mi<(cdata->cdim);mi++,mrsn++)    \
    if (mrsn->basis) mrsn->basis->ref_count++; \
}

HIDDEN simplex *
chull3d_new_block_simplex(struct chull3d_data *cdata, int make_blocks)
{
    int i;
    simplex *xlm, *xbt;
    if (make_blocks && cdata) {
	xbt = cdata->simplex_block_table[(cdata->num_simplex_blocks)++] = (simplex*)malloc(cdata->input_vert_cnt * cdata->simplex_size);
	memset(xbt,0,cdata->input_vert_cnt * cdata->simplex_size);
	xlm = ((simplex*) ( (char*)xbt + (cdata->input_vert_cnt) * cdata->simplex_size));
	for (i=0; i<cdata->input_vert_cnt; i++) {
	    xlm = ((simplex*) ( (char*)xlm + ((-1)) * cdata->simplex_size));
	    xlm->next = cdata->simplex_list;
	    cdata->simplex_list = xlm;
	}
	return cdata->simplex_list;
    }
    for (i=0; i<cdata->num_simplex_blocks; i++) free(cdata->simplex_block_table[i]);
    cdata->num_simplex_blocks = 0;
    if (cdata) cdata->simplex_list = 0;
    return 0;
}

HIDDEN void
chull3d_free_simplex_storage(struct chull3d_data *cdata)
{
    chull3d_new_block_simplex(cdata, 0);
}

HIDDEN basis_s *
chull3d_new_block_basis_s(struct chull3d_data *cdata, int make_blocks)
{
    int i;
    basis_s *xlm, *xbt;
    if (make_blocks && cdata) {
	xbt = cdata->basis_s_block_table[(cdata->num_basis_s_blocks)++] = (basis_s*)malloc(cdata->input_vert_cnt * cdata->basis_s_size);
	memset(xbt,0,cdata->input_vert_cnt * cdata->basis_s_size);
	xlm = ((basis_s*) ( (char*)xbt + (cdata->input_vert_cnt) * cdata->basis_s_size));
	for (i=0; i<cdata->input_vert_cnt; i++) {
	    xlm = ((basis_s*) ( (char*)xlm + ((-1)) * cdata->basis_s_size));
	    xlm->next = cdata->basis_s_list;
	    cdata->basis_s_list = xlm;
	}
	return cdata->basis_s_list;
    }
    for (i=0; i<cdata->num_basis_s_blocks; i++) free(cdata->basis_s_block_table[i]);
    cdata->num_basis_s_blocks = 0;
    if (cdata) cdata->basis_s_list = 0;
    return 0;
}

HIDDEN void
chull3d_free_basis_s_storage(struct chull3d_data *cdata)
{
    chull3d_new_block_basis_s(cdata, 0);
}


HIDDEN Tree *
chull3d_new_block_Tree(struct chull3d_data *cdata, int make_blocks) {
    int i;
    Tree *xlm, *xbt;
    if (make_blocks && cdata) {
	xbt = cdata->Tree_block_table[cdata->num_Tree_blocks++] = (Tree*)malloc(cdata->input_vert_cnt * cdata->Tree_size);
	memset(xbt,0,cdata->input_vert_cnt * cdata->Tree_size);
	xlm = ((Tree*) ( (char*)xbt + (cdata->input_vert_cnt) * cdata->Tree_size));
	for (i=0; i<cdata->input_vert_cnt; i++) {
	    xlm = ((Tree*) ( (char*)xlm + ((-1)) * cdata->Tree_size));
	    xlm->next = cdata->Tree_list;
	    cdata->Tree_list = xlm;
	}
	return cdata->Tree_list;
    }
    for (i=0; i<cdata->num_Tree_blocks; i++) free(cdata->Tree_block_table[i]);
    cdata->num_Tree_blocks = 0;
    if (cdata) cdata->Tree_list = 0;
    return 0;
}

HIDDEN void
chull3d_free_Tree_storage(struct chull3d_data *cdata)
{
    chull3d_new_block_Tree(cdata, 0);
}

/* ch.c : numerical functions for hull computation */

HIDDEN Coord
chull3d_Vec_dot(struct chull3d_data *cdata, point x, point y)
{
    int i;
    Coord sum = 0;
    for (i=0;i<cdata->rdim;i++) sum += x[i] * y[i];
    return sum;
}

    HIDDEN Coord
chull3d_Vec_dot_pdim(struct chull3d_data *cdata, point x, point y)
{
    int i;
    Coord sum = 0;
    for (i=0;i<cdata->pdim;i++) sum += x[i] * y[i];
    return sum;
}

    HIDDEN Coord
chull3d_Norm2(struct chull3d_data *cdata, point x)
{
    int i;
    Coord sum = 0;
    for (i=0;i<cdata->rdim;i++) sum += x[i] * x[i];
    return sum;
}

    HIDDEN void
chull3d_Ax_plus_y(struct chull3d_data *cdata, Coord a, point x, point y)
{
    int i;
    for (i=0;i<cdata->rdim;i++) {
	*y++ += a * *x++;
    }
}

    HIDDEN void
chull3d_Vec_scale(struct chull3d_data *UNUSED(cdata), int n, Coord a, Coord *x)
{
    register Coord *xx = x;
    register Coord *xend = xx + n;

    while (xx!=xend) {
	*xx *= a;
	xx++;
    }
}


#ifndef HAVE_LOGB
double logb(double x) {
    if (x<=0) return -1e305;
    return log(x)/log(2.);
}
#endif


/* amount by which to scale up vector, for reduce_inner */
    HIDDEN double
chull3d_sc(struct chull3d_data *cdata, basis_s *v,simplex *s, int k, int j)
{
    double labound;

    if (j<10) {
	labound = logb(v->sqa)/2;
	cdata->max_scale = cdata->exact_bits - labound - 0.66*(k-2)-1  -CHULL3D_DELIFT;
	if (cdata->max_scale<1) {
	    cdata->max_scale = 1;
	}

	if (j==0) {
	    int	i;
	    neighbor *sni;
	    basis_s *snib;

	    cdata->ldetbound = CHULL3D_DELIFT;

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
	double dlscale = floor(logb(2*cdata->Sb/(v->sqb + v->sqa*cdata->b_err_min)))/2;
	cdata->lscale = (int)dlscale;
	if (cdata->lscale > cdata->max_scale) {
	    dlscale = floor(cdata->max_scale);
	    cdata->lscale = (int)dlscale;
	} else if (cdata->lscale<0) cdata->lscale = 0;
	v->lscale += cdata->lscale;
	return ( ((cdata->lscale)<20) ? 1<<(cdata->lscale) : ldexp(1.,(cdata->lscale)) );
    }
}


    HIDDEN int
chull3d_reduce_inner(struct chull3d_data *cdata, basis_s *v, simplex *s, int k)
{
    point	va = CHULL3D_VA(v);
    point	vb = CHULL3D_VB(v);
    int	i,j;
    double	dd;
    double	scale;
    basis_s	*snibv;
    neighbor *sni;

    v->sqa = v->sqb = chull3d_Norm2(cdata, vb);
    if (k<=1) {
	memcpy(vb,va,cdata->basis_vec_size);
	return 1;
    }
    for (j=0;j<250;j++) {

	memcpy(vb,va,cdata->basis_vec_size);
	for (i=k-1,sni=s->neigh+k-1;i>0;i--,sni--) {
	    snibv = sni->basis;
	    dd = -chull3d_Vec_dot(cdata, CHULL3D_VB(snibv),vb)/ snibv->sqb;
	    chull3d_Ax_plus_y(cdata, dd, CHULL3D_VA(snibv), vb);
	}
	v->sqb = chull3d_Norm2(cdata, vb);
	v->sqa = chull3d_Norm2(cdata, va);

	if (2*v->sqb >= v->sqa) {cdata->B[j]++; return 1;}

	chull3d_Vec_scale(cdata, cdata->rdim,scale = chull3d_sc(cdata,v,s,k,j),va);

	for (i=k-1,sni=s->neigh+k-1;i>0;i--,sni--) {
	    snibv = sni->basis;
	    dd = -chull3d_Vec_dot(cdata, CHULL3D_VB(snibv),va)/snibv->sqb;
	    dd = floor(dd+0.5);
	    chull3d_Ax_plus_y(cdata, dd, CHULL3D_VA(snibv), va);
	}
    }
    return 0;
}

HIDDEN int
chull3d_reduce(struct chull3d_data *cdata, basis_s **v, point p, simplex *s, int k)
{
    point	z;
    point	tt = s->neigh[0].vert;

    if (!*v) CHULL3D_NEWLRC(cdata, cdata->basis_s_list,basis_s,(*v))
    else (*v)->lscale = 0;
    z = CHULL3D_VB(*v);
    CHULL3D_trans(z,p,tt);
    return chull3d_reduce_inner(cdata,*v,s,k);
}

    HIDDEN void
chull3d_get_basis_sede(struct chull3d_data *cdata, simplex *s)
{
    int	k=1;
    neighbor *sn = s->neigh+1,
	     *sn0 = s->neigh;
    if (!sn0->basis) {
	sn0->basis = cdata->tt_basis;
	cdata->tt_basis->ref_count++;
    } else while (k < (cdata->cdim) && sn->basis) {k++;sn++;}
    while (k<(cdata->cdim)) {
	CHULL3D_NULLIFY(cdata, basis_s,sn->basis);
	chull3d_reduce(cdata, &sn->basis,sn->vert,s,k);
	k++; sn++;
    }
}

    HIDDEN int
chull3d_out_of_flat(struct chull3d_data *cdata, simplex *root, point p)
{
    if (!cdata->p_neigh.basis) cdata->p_neigh.basis = (basis_s*) malloc(cdata->basis_s_size);
    cdata->p_neigh.vert = p;
    (cdata->cdim)++;
    root->neigh[(cdata->cdim)-1].vert = root->peak.vert;
    CHULL3D_NULLIFY(cdata, basis_s,root->neigh[(cdata->cdim)-1].basis);
    chull3d_get_basis_sede(cdata, root);
    chull3d_reduce(cdata, &cdata->p_neigh.basis,p,root,(cdata->cdim));
    /*if (cdata->p_neigh.basis->sqa != 0) return 1;*/
    if (!ZERO(cdata->p_neigh.basis->sqa)) return 1;
    (cdata->cdim)--;
    return 0;
}

HIDDEN int chull3d_sees(struct chull3d_data *, site, simplex *);

HIDDEN void
chull3d_get_normal_sede(struct chull3d_data *cdata, simplex *s)
{
    neighbor *rn;
    int i,j;

    chull3d_get_basis_sede(cdata, s);
    if (cdata->rdim==3 && (cdata->cdim)==3) {
	point	c;
	point	a = CHULL3D_VB(s->neigh[1].basis);
	point   b = CHULL3D_VB(s->neigh[2].basis);
	CHULL3D_NEWLRC(cdata, cdata->basis_s_list, basis_s,s->normal);
	c = CHULL3D_VB(s->normal);
	c[0] = a[1]*b[2] - a[2]*b[1];
	c[1] = a[2]*b[0] - a[0]*b[2];
	c[2] = a[0]*b[1] - a[1]*b[0];
	s->normal->sqb = chull3d_Norm2(cdata, c);
	for (i=(cdata->cdim)+1,rn = cdata->ch_root->neigh+(cdata->cdim)-1; i; i--, rn--) {
	    for (j = 0; j<(cdata->cdim) && rn->vert != s->neigh[j].vert;j++);
	    if (j<(cdata->cdim)) continue;
	    if (rn->vert==cdata->hull_infinity) {
		if (c[2] > -cdata->b_err_min) continue;
	    } else  if (!chull3d_sees(cdata, rn->vert,s)) continue;
	    c[0] = -c[0]; c[1] = -c[1]; c[2] = -c[2];
	    break;
	}
	return;
    }

    for (i=(cdata->cdim)+1,rn = cdata->ch_root->neigh+(cdata->cdim)-1; i; i--, rn--) {
	for (j = 0; j<(cdata->cdim) && rn->vert != s->neigh[j].vert;j++);
	if (j<(cdata->cdim)) continue;
	chull3d_reduce(cdata, &s->normal,rn->vert,s,(cdata->cdim));
	if (!ZERO(s->normal->sqb)) break;
    }
}


HIDDEN void chull3d_get_normal(struct chull3d_data *cdata, simplex *s) {chull3d_get_normal_sede(cdata,s); return;}

HIDDEN int
chull3d_sees(struct chull3d_data *cdata, site p, simplex *s) {

    point	tt,zz;
    double	dd,dds;
    int i;

    if (!cdata->b) cdata->b = (basis_s*)malloc(cdata->basis_s_size);
    else cdata->b->lscale = 0;
    zz = CHULL3D_VB(cdata->b);
    if ((cdata->cdim)==0) return 0;
    if (!s->normal) {
	chull3d_get_normal_sede(cdata, s);
	for (i=0;i<(cdata->cdim);i++) CHULL3D_NULLIFY(cdata, basis_s,s->neigh[i].basis);
    }
    tt = s->neigh[0].vert;
    CHULL3D_trans(zz,p,tt);
    for (i=0;i<3;i++) {
	dd = chull3d_Vec_dot(cdata, zz,s->normal->vecs);
	if (ZERO(dd)) {
	    return 0;
	}
	dds = dd*dd/s->normal->sqb/chull3d_Norm2(cdata, zz);
	if (dds > cdata->b_err_min_sq) return (dd<0);
	chull3d_get_basis_sede(cdata, s);
	chull3d_reduce_inner(cdata,cdata->b,s,(cdata->cdim));
    }
    return 0;
}

HIDDEN void
chull3d_free_hull_storage(struct chull3d_data *cdata)
{
    chull3d_free_basis_s_storage(cdata);
    chull3d_free_simplex_storage(cdata);
    chull3d_free_Tree_storage(cdata);
}

HIDDEN void
chull3d_visit_fg_i(struct chull3d_data *cdata, void (*v_fg)(struct chull3d_data *,Tree *, int, int),
	Tree *t, int depth, int vn, int boundary)
{
    int	boundaryc = boundary;

    if (!t) return;

    //assert(t->fgs);
    if (t->fgs->mark!=vn) {
	t->fgs->mark = vn;
	if (t->key!=cdata->hull_infinity && !cdata->mo[cdata->site_num((void *)cdata, t->key)]) boundaryc = 0;
	v_fg(cdata, t,depth, boundaryc);
	chull3d_visit_fg_i(cdata, v_fg, t->fgs->facets,depth+1, vn, boundaryc);
    }
    chull3d_visit_fg_i(cdata, v_fg, t->left,depth,vn, boundary);
    chull3d_visit_fg_i(cdata, v_fg, t->right,depth,vn,boundary);
}

HIDDEN void
chull3d_visit_fg(struct chull3d_data *cdata, fg *faces_gr, void (*v_fg)(struct chull3d_data *, Tree *, int, int))
{
    chull3d_visit_fg_i(cdata, v_fg, faces_gr->facets, 0, ++(cdata->fg_vn), 1);
}

HIDDEN int
chull3d_visit_fg_i_far(struct chull3d_data *cdata, void (*v_fg)(struct chull3d_data *, Tree *, int),
	Tree *t, int depth, int vn)
{
    int nb = 0;

    if (!t) return 0;

    //assert(t->fgs);
    if (t->fgs->mark!=vn) {
	t->fgs->mark = vn;
	nb = (t->key==cdata->hull_infinity) || cdata->mo[cdata->site_num((void *)cdata, t->key)];
	if (!nb && !chull3d_visit_fg_i_far(cdata, v_fg, t->fgs->facets,depth+1,vn))
	    v_fg(cdata, t,depth);
    }
    nb = chull3d_visit_fg_i_far(cdata, v_fg, t->left,depth,vn) || nb;
    nb = chull3d_visit_fg_i_far(cdata, v_fg, t->right,depth,vn) || nb;
    return nb;
}

HIDDEN void
chull3d_visit_fg_far(struct chull3d_data *cdata, fg *faces_gr, void (*v_fg)(struct chull3d_data *, Tree *, int))
{
    chull3d_visit_fg_i_far(cdata, v_fg,faces_gr->facets, 0, --(cdata->fg_vn));
}

/* hull.c : "combinatorial" functions for hull computation */

typedef int chull3d_test_func(struct chull3d_data *, simplex *, int, void *);
typedef void* chull3d_visit_func(struct chull3d_data *, simplex *, void *);

/* starting at s, visit simplices t such that test(s,i,0) is true,
 * and t is the i'th neighbor of s;
 * apply visit function to all visited simplices;
 * when visit returns nonNULL, exit and return its value */
HIDDEN void *
chull3d_visit_triang_gen(struct chull3d_data *cdata, simplex *s, chull3d_visit_func *visit, chull3d_test_func *test)
{
    neighbor *sn;
    void *v;
    simplex *t;
    int i;
    long tms = 0;

    (cdata->vnum)--;
    if (s) CHULL3D_push(s);
    while (tms) {
	if (tms>cdata->ss) {
	    cdata->st=(simplex**)realloc(cdata->st,((cdata->ss+=cdata->ss)+MAXDIM+1)*sizeof(simplex*));
	}
	CHULL3D_pop(t);
	if (!t || t->visit == cdata->vnum) continue;
	t->visit = cdata->vnum;
	if ((v=(*visit)(cdata,t,0))) {return v;}
	for (i=-1,sn = t->neigh-1;i<(cdata->cdim);i++,sn++)
	    if ((sn->simp->visit != cdata->vnum) && sn->simp && test(cdata,t,i,0))
		CHULL3D_push(sn->simp);
    }
    return NULL;
}

/* visit the whole triangulation */
HIDDEN int chull3d_truet(struct chull3d_data *UNUSED(cdata), simplex *UNUSED(s), int UNUSED(i), void *UNUSED(dum)) {return 1;}
HIDDEN void *
chull3d_visit_triang(struct chull3d_data *cdata, simplex *root, chull3d_visit_func *visit)
{
    return chull3d_visit_triang_gen(cdata, root, visit, chull3d_truet);
}


HIDDEN int chull3d_hullt(struct chull3d_data *UNUSED(cdata), simplex *UNUSED(s), int i, void *UNUSED(dummy)) {return i>-1;}
HIDDEN void *chull3d_facet_test(struct chull3d_data *UNUSED(cdata), simplex *s, void *UNUSED(dummy)) {return (!s->peak.vert) ? s : NULL;}
HIDDEN void *
chull3d_visit_hull(struct chull3d_data *cdata, simplex *root, chull3d_visit_func *visit)
{
    return chull3d_visit_triang_gen(cdata, (struct simplex *)chull3d_visit_triang(cdata, root, &chull3d_facet_test), visit, chull3d_hullt);
}

HIDDEN void *
chull3d_check_simplex(struct chull3d_data *cdata, simplex *s, void *UNUSED(dum))
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

HIDDEN void *
chull3d_collect_hull_pnts(struct chull3d_data *cdata, simplex *s, void *UNUSED(p)) {

    point v[MAXDIM];
    int j;
    long int ip[3];
    /*point_t p1, p2, p3;*/
    const point_t *pp[3];
    int f;

    if (!s) return NULL;

    for (j=0;j<(cdata->cdim);j++) v[j] = s->neigh[j].vert;
    for (j=0;j<(cdata->cdim);j++) ip[j] = (cdata->site_num)((void *)cdata, v[j]);
    for (j=0;j<(cdata->cdim);j++) pp[j] = &(cdata->input_vert_array[ip[j]]);

    /* Don't add a point if it's already added */
    for (j=0;j<(cdata->cdim);j++){
	std::map<long *, int>::iterator pli;
	pli = cdata->point_lookup->find((long *)pp[j]);
	if (pli == cdata->point_lookup->end()) {
	    f = bu_ptbl_ins(cdata->output_pnts, (long *)pp[j]);
	    cdata->point_lookup->insert(std::make_pair((long *)pp[j], f));
	    VMOVE(cdata->vert_array[f],*pp[j]);
	    (*cdata->vert_cnt)++;
	}
    }
    return NULL;
}


HIDDEN void *
chull3d_collect_faces(struct chull3d_data *cdata, simplex *s, void *UNUSED(p)) {

    point v[MAXDIM];
    int j;
    long int ip[3];
    /*point_t p1, p2, p3;*/
    const point_t *pp[3];
    int f[3];
    vect_t a, b, normal, center_to_edge;

    if (!s) return NULL;

    /* If we're not 3d, it doesn't make sense to do this */
    if (cdata->cdim != 3) return NULL;

    for (j=0;j<(cdata->cdim);j++) v[j] = s->neigh[j].vert;
    for (j=0;j<(cdata->cdim);j++) ip[j] = (cdata->site_num)((void *)cdata, v[j]);
    for (j=0;j<(cdata->cdim);j++) pp[j] = &(cdata->input_vert_array[ip[j]]);

    for (j=0;j<(cdata->cdim);j++){
	std::map<long *, int>::iterator pli;
	pli = cdata->point_lookup->find((long *)pp[j]);
	f[j] = pli->second;
    }

    /* Use cdata->center_pnt and the center pnt of the triangle to construct
     * a vector, and find the normal of the proposed face.  If the face normal
     * does not point in the same direction (dot product is positive) then
     * the face needs to be reversed. Do this so the eventual bot primitive
     * won't need a bot sync.  Since by definition the chull is convex, the
     * center point should be "inside" relative to all faces and this test
     * should be reliable. */
    VSUB2(a, cdata->vert_array[f[1]], cdata->vert_array[f[0]]);
    VSUB2(b, cdata->vert_array[f[2]], cdata->vert_array[f[0]]);
    VCROSS(normal, a, b);
    VUNITIZE(normal);

    VSUB2(center_to_edge, cdata->vert_array[f[0]], cdata->center_pnt);
    VUNITIZE(center_to_edge);

    cdata->faces[(*cdata->face_cnt)*3] = f[0];
    if(VDOT(normal, center_to_edge) < 0) {
	cdata->faces[(*cdata->face_cnt)*3+1] = f[2];
	cdata->faces[(*cdata->face_cnt)*3+2] = f[1];
    } else {
	cdata->faces[(*cdata->face_cnt)*3+1] = f[1];
	cdata->faces[(*cdata->face_cnt)*3+2] = f[2];
    }
    (*cdata->face_cnt)++;
    return NULL;
}


HIDDEN Coord
chull3d_maxdist(int dim, point p1, point p2)
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
chull3d_op_simp(struct chull3d_data *cdata, simplex *a, simplex *b)
{
    int i;
    neighbor *x;
    for (i=0, x = a->neigh; (x->simp != b) && (i<(cdata->cdim)) ; i++, x++);
    if (i<(cdata->cdim))
	return x;
    else {
	return 0;
    }
}

HIDDEN neighbor *
chull3d_op_vert(struct chull3d_data *cdata, simplex *a, site b)
{
    int i;
    neighbor *x;
    for (i=0, x = a->neigh; (x->vert != b) && (i<(cdata->cdim)) ; i++, x++);
    if (i<(cdata->cdim))
	return x;
    else {
	return 0;
    }
}


/* make neighbor connections between newly created simplices incident to p */
HIDDEN void
chull3d_connect(struct chull3d_data *cdata, simplex *s)
{
    site xf,xb,xfi;
    simplex *sb, *sf, *seen;
    int i;
    neighbor *sn;

    if (!s) return;
    //assert(!s->peak.vert && s->peak.simp->peak.vert==cdata->p && !chull3d_op_vert(cdata,s,cdata->p)->simp->peak.vert);
    if (s->visit==cdata->pnum) return;
    s->visit = cdata->pnum;
    seen = s->peak.simp;
    xfi = chull3d_op_simp(cdata,seen,s)->vert;
    for (i=0, sn = s->neigh; i<(cdata->cdim); i++,sn++) {
	xb = sn->vert;
	if (cdata->p == xb) continue;
	sb = seen;
	sf = sn->simp;
	xf = xfi;
	if (!sf->peak.vert) {	/* are we done already? */
	    sf = chull3d_op_vert(cdata,seen,xb)->simp;
	    if (sf->peak.vert) continue;
	} else do {
	    xb = xf;
	    xf = chull3d_op_simp(cdata,sf,sb)->vert;
	    sb = sf;
	    sf = chull3d_op_vert(cdata,sb,xb)->simp;
	} while (sf->peak.vert);

	sn->simp = sf;
	chull3d_op_vert(cdata,sf,xf)->simp = s;

	chull3d_connect(cdata, sf);
    }
}


/* visit simplices s with sees(p,s), and make a facet for every neighbor
 * of s not seen by p */
HIDDEN simplex *
chull3d_make_facets(struct chull3d_data *cdata, simplex *seen)
{
    simplex *n;
    neighbor *bn;
    int i;


    if (!seen) return NULL;
    seen->peak.vert = cdata->p;

    for (i=0,bn = seen->neigh; i<(cdata->cdim); i++,bn++) {
	neighbor *ns;
	n = bn->simp;
	if (cdata->pnum != n->visit) {
	    n->visit = cdata->pnum;
	    if (chull3d_sees(cdata, cdata->p,n)) chull3d_make_facets(cdata, n);
	}
	if (n->peak.vert) continue;
	CHULL3D_copy_simp(cdata, cdata->ns, seen);
	cdata->ns->visit = 0;
	cdata->ns->peak.vert = 0;
	cdata->ns->normal = 0;
	cdata->ns->peak.simp = seen;
	CHULL3D_NULLIFY(cdata, basis_s,cdata->ns->neigh[i].basis);
	cdata->ns->neigh[i].vert = cdata->p;
	bn->simp = cdata->ns;
	ns = chull3d_op_simp(cdata,n,seen);
	ns->simp = cdata->ns;
    }
    return cdata->ns;
}


/* p lies outside flat containing previous sites;
 * make p a vertex of every current simplex, and create some new simplices */
HIDDEN simplex *
chull3d_extend_simplices(struct chull3d_data *cdata, simplex *s)
{
    int	i;
    int ocdim=(cdata->cdim)-1;
    simplex *ns = NULL;
    neighbor *nsn = NULL;


    if (s->visit == cdata->pnum) return s->peak.vert ? s->neigh[ocdim].simp : s;
    s->visit = cdata->pnum;
    s->neigh[ocdim].vert = cdata->p;
    CHULL3D_NULLIFY(cdata, basis_s,s->normal);
    CHULL3D_NULLIFY(cdata, basis_s,s->neigh[0].basis);
    if (!s->peak.vert) {
	s->neigh[ocdim].simp = chull3d_extend_simplices(cdata, s->peak.simp);
	return s;
    } else {
	CHULL3D_copy_simp(cdata, ns, s);
	s->neigh[ocdim].simp = ns;
	ns->peak.vert = NULL;
	ns->peak.simp = s;
	ns->neigh[ocdim] = s->peak;
	if (s->peak.basis) s->peak.basis->ref_count++;
	for (i=0,nsn=ns->neigh;i<(cdata->cdim);i++,nsn++)
	    nsn->simp = chull3d_extend_simplices(cdata, nsn->simp);
    }
    return ns;
}


/* return a simplex s that corresponds to a facet of the
 * current hull, and sees(p, s) */
HIDDEN simplex *
chull3d_search(struct chull3d_data *cdata, simplex *root)
{
    simplex *s;
    neighbor *sn;
    int i;
    long tms = 0;


    CHULL3D_push(root->peak.simp);
    root->visit = cdata->pnum;
    if (!chull3d_sees(cdata, cdata->p,root))
	for (i=0,sn=root->neigh;i<(cdata->cdim);i++,sn++) CHULL3D_push(sn->simp);
    while (tms) {
	if (tms>cdata->ss)
	    cdata->st=(simplex**)realloc(cdata->st, ((cdata->ss+=cdata->ss)+MAXDIM+1)*sizeof(simplex*));
	CHULL3D_pop(s);
	if (s->visit == cdata->pnum) continue;
	s->visit = cdata->pnum;
	if (!chull3d_sees(cdata, cdata->p,s)) continue;
	if (!s->peak.vert) return s;
	for (i=0, sn=s->neigh; i<(cdata->cdim); i++,sn++) CHULL3D_push(sn->simp);
    }
    return NULL;
}


HIDDEN point
chull3d_get_another_site(struct chull3d_data *cdata)
{
    point pnext;
    pnext = (*(cdata->get_site))((void *)cdata);
    if (!pnext) return NULL;
    cdata->pnum = cdata->site_num((void *)cdata, pnext)+2;
    return pnext;
}



HIDDEN void
chull3d_buildhull(struct chull3d_data *cdata, simplex *root)
{
    while ((cdata->cdim) < cdata->rdim) {
	cdata->p = chull3d_get_another_site(cdata);
	if (!cdata->p) return;
	if (chull3d_out_of_flat(cdata, root,cdata->p))
	    chull3d_extend_simplices(cdata, root);
	else
	    chull3d_connect(cdata, chull3d_make_facets(cdata, chull3d_search(cdata, root)));
    }
    while ((cdata->p = chull3d_get_another_site(cdata)))
	chull3d_connect(cdata, chull3d_make_facets(cdata, chull3d_search(cdata, root)));
}


/*
   get_s             returns next site each call;
   hull construction stops when NULL returned;
   site_numm         returns number of site when given site;
   dim               dimension of point set;
   */
HIDDEN simplex *
chull3d_build_convex_hull(struct chull3d_data *cdata, gsitef *get_s, site_n *site_numm, short dim)
{
    double febits;
    simplex *s = NULL;
    simplex *root = NULL;

    if (ZERO(cdata->Huge)) cdata->Huge = DBL_MAX*DBL_MAX;

    (cdata->cdim) = 0;
    cdata->get_site = get_s;
    cdata->site_num = site_numm;
    cdata->pdim = dim;

    febits = floor(DBL_MANT_DIG*log(double(FLT_RADIX))/log(2.));
    cdata->exact_bits = (int)febits;
    cdata->b_err_min = (float)(DBL_EPSILON*MAXDIM*(1<<MAXDIM)*MAXDIM*3.01);
    cdata->b_err_min_sq = cdata->b_err_min * cdata->b_err_min;

    //assert(cdata->get_site!=NULL); assert(cdata->site_num!=NULL);

    cdata->rdim = cdata->pdim;

    cdata->point_size = cdata->site_size = sizeof(Coord)*cdata->pdim;
    cdata->basis_vec_size = sizeof(Coord)*cdata->rdim;
    cdata->basis_s_size = sizeof(basis_s)+ (2*cdata->rdim-1)*sizeof(Coord);
    cdata->simplex_size = sizeof(simplex) + (cdata->rdim-1)*sizeof(neighbor);
    cdata->Tree_size = sizeof(Tree);

    root = NULL;
    if (!(cdata->p = (*(cdata->get_site))((void *)cdata))) return 0;

    CHULL3D_NEWL(cdata, cdata->simplex_list, simplex,root);

    cdata->ch_root = root;

    CHULL3D_copy_simp(cdata, s, root);
    root->peak.vert = cdata->p;
    root->peak.simp = s;
    s->peak.simp = root;

    chull3d_buildhull(cdata, root);
    return root;
}


/* hullmain.c */

HIDDEN long
chull3d_site_numm(void *data, site p)
{
    long i,j;
    struct chull3d_data *cdata = (struct chull3d_data *)data;

    if (!p) return -2;
    for (i=0; i<cdata->num_blocks; i++)
	if ((j=p-cdata->site_blocks[i])>=0 && j < BLOCKSIZE*cdata->pdim)
	    return j/cdata->pdim+BLOCKSIZE*i;
    return -3;
}

HIDDEN site
chull3d_new_site(struct chull3d_data *cdata, site p, long j)
{
    //assert(cdata->num_blocks+1<cdata->input_vert_cnt);
    if (0==(j%BLOCKSIZE)) {
	//assert(cdata->num_blocks < cdata->input_vert_cnt);
	return(cdata->site_blocks[cdata->num_blocks++]=(site)malloc(BLOCKSIZE*cdata->site_size));
    } else
	return p+cdata->pdim;
}

HIDDEN site
chull3d_read_next_site(struct chull3d_data *cdata, long j)
{
    int i=0;

    cdata->p = chull3d_new_site(cdata, cdata->p,j);

    if (cdata->next_vert == cdata->input_vert_cnt) return 0;

    cdata->p[0] = cdata->input_vert_array[cdata->next_vert][0];
    cdata->p[1] = cdata->input_vert_array[cdata->next_vert][1];
    cdata->p[2] = cdata->input_vert_array[cdata->next_vert][2];
    (cdata->next_vert)++;
    for(i = 0; i < 3; i++) {
	(cdata->p)[i] = floor(cdata->mult_up*(cdata->p)[i]+0.5);
	cdata->mins[i] = (cdata->mins[i]<(cdata->p)[i]) ? cdata->mins[i] : (cdata->p)[i];
	cdata->maxs[i] = (cdata->maxs[i]>(cdata->p)[i]) ? cdata->maxs[i] : (cdata->p)[i];
    }

    return cdata->p;
}

HIDDEN void
chull3d_make_shuffle(struct chull3d_data *cdata)
{
    long i, j, t;
    for (i=0;i<cdata->input_vert_cnt;i++) cdata->shufmat[i] = i;
    for (i=0;i<cdata->input_vert_cnt;i++) {
	double random_num = bn_randmt();
	t = cdata->shufmat[i];
	j = i + (cdata->input_vert_cnt - i) * (long)random_num;
	cdata->shufmat[i] = cdata->shufmat[j];
	cdata->shufmat[j] = t;
    }
}

HIDDEN site
chull3d_get_next_site(void *data) {
    struct chull3d_data *cdata = (struct chull3d_data *)data;
    return chull3d_read_next_site(cdata, cdata->shufmat[(cdata->s_num)++]);
}

HIDDEN void
chull3d_data_init(struct chull3d_data *data, int vert_cnt)
{
    int i = 0;
    data->simplex_list = 0;
    data->basis_s_list = 0;
    data->Tree_list = 0;
    data->site_blocks = (point *)bu_calloc(vert_cnt * 3, sizeof(point), "site_blocks");
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
    data->B = (int *)bu_calloc(vert_cnt, sizeof(int), "A");
    data->mins = (Coord *)bu_calloc(MAXDIM, sizeof(Coord), "mins");
    for(i = 0; i < MAXDIM; i++) data->mins[i] = DBL_MAX;
    data->maxs = (Coord *)bu_calloc(MAXDIM, sizeof(Coord), "maxs");
    for(i = 0; i < MAXDIM; i++) data->maxs[i] = -DBL_MAX;
    data->mult_up = 1000.0; /* we'll need to multiply based on a tolerance parameter at some point... */
    data->mi = (short *)bu_calloc(vert_cnt, sizeof(short), "mi");
    data->mo = (short *)bu_calloc(vert_cnt, sizeof(short), "mo");
    data->pdim = 3;
    data->shufmat = (long*)bu_calloc(vert_cnt+1, sizeof(long), "shufmat");

    /* These were static variables in functions */
    data->simplex_block_table = (simplex **)bu_calloc(vert_cnt, sizeof(simplex *), "simplex_block_table");
    data->num_simplex_blocks = 0;
    data->basis_s_block_table = (basis_s **)bu_calloc(vert_cnt, sizeof(basis_s *), "basis_s_block_table");
    data->num_basis_s_blocks = 0;
    data->Tree_block_table = (Tree **)bu_calloc(vert_cnt, sizeof(Tree *), "Tree_block_table");
    data->num_Tree_blocks = 0;
    data->p_neigh.vert = 0;
    data->p_neigh.simp = NULL;
    data->p_neigh.basis = NULL;
    data->b = NULL;
    data->vnum = -1;
    data->ss = vert_cnt + MAXDIM;
    data->s_num = 0;
    data->fg_vn = 0;
    data->st=(simplex**)malloc((data->ss+MAXDIM+1)*sizeof(simplex*));
    data->ns = NULL;
    data->free_point_lookup = 0;
    data->free_output_pnts = 0;
    if (!data->point_lookup) {
	data->point_lookup = new std::map<long *, int>;
	data->free_point_lookup = 1;
    }
    if (!data->output_pnts) {
	BU_GET(data->output_pnts, struct bu_ptbl);
	bu_ptbl_init(data->output_pnts, vert_cnt, "output pnts container");
	data->free_output_pnts = 1;
    }
    data->input_vert_cnt = vert_cnt;

    VSET(data->center_pnt,0,0,0);

    /* Output containers */
    if (!data->vert_array) data->vert_array = (point_t *)bu_calloc(vert_cnt, sizeof(point_t), "vertex array");
    data->next_vert = 0;
    if (!data->faces) data->faces = (int *)bu_calloc(3*vert_cnt, sizeof(int), "face array");
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
    if (data->free_output_pnts) {
	bu_ptbl_free(data->output_pnts);
	BU_PUT(data->output_pnts, struct bu_ptbl);
    }
    if (data->free_point_lookup) delete data->point_lookup;
}

HIDDEN
int
chull3d_intermediate_set(point_t **vertices, int *num_vertices, const point_t *input_points_3d, int num_input_pnts)
{
    int nv = 0;
    int nf = 0;
    int curr_stp = 0;
    int pnt_stp = 0;
    int last_stp = 0;
    int pnts_per_stp = CHULL3D_SUBSET_SIZE;
    const point_t *curr_inputs;
    struct bu_ptbl *opnts;
    std::map<long *, int> *pl = new std::map<long *, int>;
    point_t *intermediate_vert_array = (point_t *)bu_calloc(num_input_pnts, sizeof(point_t), "vertex array");
    simplex *root = NULL;
    BU_GET(opnts, struct bu_ptbl);
    bu_ptbl_init(opnts, num_input_pnts, "output pnts container");
    pnt_stp = (num_input_pnts < pnts_per_stp) ? num_input_pnts : pnts_per_stp;
    last_stp = num_input_pnts / pnts_per_stp;
    //bu_log("input point cnt: %d\n", num_input_pnts);
    while (curr_stp <= last_stp) {
	struct chull3d_data *cdata;
	//bu_log("step %d of %d\n", curr_stp, last_stp);
	curr_inputs = input_points_3d + curr_stp * pnt_stp;
	if (curr_stp == last_stp) pnt_stp = num_input_pnts - curr_stp * pnt_stp;
	BU_GET(cdata, struct chull3d_data);
	cdata->output_pnts = opnts;
	cdata->point_lookup = pl;
	cdata->vert_array = intermediate_vert_array;
	chull3d_data_init(cdata, pnt_stp);
	cdata->input_vert_array = curr_inputs;
	cdata->vert_cnt = &nv;
	cdata->face_cnt = &nf;
	chull3d_make_shuffle(cdata);
	root = chull3d_build_convex_hull(cdata, chull3d_get_next_site, chull3d_site_numm, cdata->pdim);
	if (!root) return -1;
	chull3d_visit_hull(cdata, root, chull3d_collect_hull_pnts);
	chull3d_free_hull_storage(cdata);
	chull3d_data_free(cdata);
	BU_PUT(cdata, struct chull3d_data);
	curr_stp++;
	//bu_log("points accumulated: %d\n", nv);
    }
    //bu_log("output pnt cnt: %d\n", nv);
    bu_ptbl_free(opnts);
    BU_PUT(opnts, struct bu_ptbl);
    delete pl;

    (*vertices) = intermediate_vert_array;
    (*num_vertices) = nv;
    return 0;
}

int
bg_3d_chull(int **faces, int *num_faces, point_t **vertices, int *num_vertices,
	const point_t *input_points_3d, int num_input_pnts)
{
    int i;
    struct chull3d_data *cdata;
    simplex *root = NULL;
    int output_dim = 0;
    point_t* hull_2d;

    const point_t *iv_1;
    point_t *iv_2, *iva;
    int nv_1, nv_2, nva;
    iv_1 = input_points_3d;
    nv_1 = num_input_pnts;

    (void)chull3d_intermediate_set(&iv_2, &nv_2, iv_1, nv_1);
    iva = iv_2;
    nva = nv_2;
    while (nva > CHULL3D_COUNT_THRESHOLD && abs(nv_1 - nv_2) > CHULL3D_DELTA_THRESHOLD) {
	nv_1 = nv_2;
	iv_1 = (const point_t *)iv_2;
	(void)chull3d_intermediate_set(&iv_2, &nv_2, iv_1, nv_1);
	bu_free((point_t *)iv_1, "free old set");
	iva = iv_2;
	nva = nv_2;
    }

    BU_GET(cdata, struct chull3d_data);
    chull3d_data_init(cdata, nva);
    cdata->input_vert_array = (const point_t *)iva;
    cdata->vert_cnt = num_vertices;
    cdata->face_cnt = num_faces;
    (*cdata->vert_cnt) = 0;
    (*cdata->face_cnt) = 0;
    chull3d_make_shuffle(cdata);
    root = chull3d_build_convex_hull(cdata, chull3d_get_next_site, chull3d_site_numm, cdata->pdim);
    if (!root) return -1;
    chull3d_visit_hull(cdata, root, chull3d_collect_hull_pnts);
    switch(cdata->cdim) {
	case 0:
	    bu_log("point?\n");
	    break;
	case 1:
	    bu_log("line?\n");
	    break;
	case 2:
	    /* We already have the hull points, but we need to assemble them into a CCW hull */
	    bg_3d_coplanar_chull(&hull_2d, (const point_t *)cdata->vert_array, (*cdata->vert_cnt));
	    (*vertices) = hull_2d;
	    bu_free(cdata->vert_array, "using 2d vertex list from coplanar_chull");
	    (*faces) = NULL; /* Should we tessellate the hull and make faces? */
	    bu_free(cdata->faces, "free face list - not used in 2d");
	    break;
	case 3:
	    /* 3d hull - calculate the center point so we can correctly add triangle faces
	     * with normals outward from the convex hull */
	    for(i = 0; i < (int)BU_PTBL_LEN(cdata->output_pnts); i++) {
		point_t p1;
		VMOVE(p1,cdata->vert_array[i]);
		cdata->center_pnt[0] += p1[0];
		cdata->center_pnt[1] += p1[1];
		cdata->center_pnt[2] += p1[2];
	    }
	    cdata->center_pnt[0] = cdata->center_pnt[0]/(*cdata->vert_cnt);
	    cdata->center_pnt[1] = cdata->center_pnt[1]/(*cdata->vert_cnt);
	    cdata->center_pnt[2] = cdata->center_pnt[2]/(*cdata->vert_cnt);
	    /* Build the faces list and assign the results */
	    chull3d_visit_hull(cdata, root, chull3d_collect_faces);
	    (*vertices) = cdata->vert_array;
	    (*faces) = cdata->faces;
	    break;
	default:
	    break;
    }
    output_dim = cdata->cdim;
    chull3d_free_hull_storage(cdata);
    chull3d_data_free(cdata);
    bu_free(iva, "intermediate_vert_array");
    BU_PUT(cdata, struct chull3d_data);

    return output_dim;
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

