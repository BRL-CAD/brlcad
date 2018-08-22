#include "common.h"

#include "nanoflann.hpp"

#if defined(__GNUC__) && (__GNUC__ == 4 && __GNUC_MINOR__ < 6) && !defined(__clang__)
#  pragma message "Disabling GCC float equality comparison warnings via pragma due to jc_voronoi ..."
#endif
#if defined(__GNUC__) && (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 6)) && !defined(__clang__)
#  pragma GCC diagnostic push
#endif
#if defined(__clang__)
#  pragma clang diagnostic push
#endif
#if defined(__GNUC__) && (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 3)) && !defined(__clang__)
#  pragma GCC diagnostic ignored "-Wfloat-equal"
#endif
#if defined(__clang__)
#  pragma clang diagnostic ignored "-Wfloat-equal"
#endif

/* https://github.com/JCash/voronoi */

#define JC_VORONOI_IMPLEMENTATION
#define JCV_REAL_TYPE double
#define JCV_FABS fabs
#define JCV_SQRT sqrt
#define JCV_ATAN2 atan2
#include "jc_voronoi.h"

#if defined(__GNUC__) && (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 6)) && !defined(__clang__)
#  pragma GCC diagnostic pop
#endif
#if defined(__clang__)
#  pragma clang diagnostic pop
#endif

extern "C" {
#include "vmath.h"
#include "bn/mat.h"
#include "rt/geom.h"
}

/* Guided by http://tensor-compiler.org/codegen.html with
 * expression A(i,j) = B(i) * C(j) */
void
tensor2(fastf_t **o, vect_t *v1, vect_t *v2)
{
    int i, j;
    for (i = 0; i < 3; i++) {
	for (j = 0; j < 3; j++) {
	    (*o)[(i * 3) + j] = (*v1)[i] * (*v2)[j];
	}
    }
}

/* Guided by http://tensor-compiler.org/codegen.html with
 * expression A(i,j,k) = B(i) * C(j) * D(k) */
void
tensor3(fastf_t **o, vect_t *v1, vect_t *v2, vect_t *v3)
{
    int i, j, k;
    for (i = 0; i < 3; i++) {
	for (j = 0; j < 3; j++) {
	    for (k = 0; k < 3; k++) {
		(*o)[(((i * 3) + j) * 3) + k] = (*v1)[i] * (*v2)[j] * (*v3)[k];
	    }
	}
    }
}

/* Based on https://github.com/brandonpelfrey/SimpleOctree */
struct octree {
    /* Node's physical position/size. This implicitly defines the bounding
     * box of this node */
    vect_t origin; /* The physical center of this node */
    vect_t hdim;   /* Half the width/height/depth of this node */

    /* Children follow a predictable pattern to make accesses simple.
     * Here, - means less than 'origin' in that dimension, + means greater than.
     *
     * child:   0 1 2 3 4 5 6 7
     * x:      - - - - + + + +
     * y:      - - + + - - + +
     * z:      - + - + - + - +
     *
     * The tree has up to eight children */
    struct octree **children; /* Pointers to child octants */

    /* Points associated with node */
    std::vector<struct pnt_normal *> *npnts;

    /* Winding number calculation coefficients for this node */
    vect_t w0;
    fastf_t w1[9];
    fastf_t w2[27];
    point_t p_wsum;
};

struct octree *
octree_node_create(vect_t *origin, vect_t *hdim)
{
    struct octree *nnode;
    BU_GET(nnode, struct octree);
    nnode->children = (struct octree **)bu_calloc(8, sizeof(struct octree *), "children");
    nnode->npnts = new std::vector<struct pnt_normal *>;
    if (origin) VMOVE(nnode->origin, *origin);
    if (hdim) VMOVE(nnode->hdim, *hdim);
    return nnode;
}

void
octree_destroy(struct octree *t)
{
    int i;
    if (!t) return;
    for (i = 0; i < 8; i++) {
	octree_destroy(t->children[i]);
    }
    delete t->npnts;
    bu_free(t->children, "children array");
    BU_PUT(t, struct octree);
}

int
octree_node_octant(struct octree *t, struct pnt_normal *p)
{
    int oct = 0;
    if(p->v[X] >= t->origin[X]) oct |= 4;
    if(p->v[Y] >= t->origin[Y]) oct |= 2;
    if(p->v[Z] >= t->origin[Z]) oct |= 1;
    return oct;
}

int
octree_node_is_leaf(struct octree *t)
{
    int i = 0;
    if (!t) return -1;

    for(i = 0; i < 8; i++) {
	if(t->children[i] != NULL) return 0;
    }

    return 1;
}

void
octree_insert(struct octree *t, struct pnt_normal *p)
{
    int i;
    int is_leaf = octree_node_is_leaf(t);

    /* normal is optional, t and p aren't */
    if (!t || !p) return;

    /* All nodes keep track of which points are within them */
    t->npnts->push_back(p);

    if (is_leaf) {

	/* NOTE: if we don't want one point per leaf (which is what the paper
	 * ended up using) this size check needs to compare to a threshold */
	if (t->npnts->size() > 1) {
	    /* Split this node so that it has 8 child octants and then
	     * re-insert the node_pnts that were here in the appropriate
	     * children. */

	    /* Split the current node and create new empty trees for each
	     * child octant. */
	    for(i = 0; i < 8; i++) {
		/* Compute new bounding box for this child */
		vect_t neworigin, newhdim;
		VMOVE(neworigin, t->origin);
		neworigin[X] += t->hdim[X] * (i&4 ? .5f : -.5f);
		neworigin[Y] += t->hdim[Y] * (i&2 ? .5f : -.5f);
		neworigin[Z] += t->hdim[Z] * (i&1 ? .5f : -.5f);
		VMOVE(newhdim, t->hdim);
		VSCALE(newhdim, newhdim, .5f);
		t->children[i] = octree_node_create(&neworigin, &newhdim);
	    }

	    /* Distribute the points in the node to the children */
	    std::vector<struct pnt_normal *>::iterator npnts_it;
	    for (npnts_it = t->npnts->begin(); npnts_it != t->npnts->end(); npnts_it++) {
		octree_insert(t->children[octree_node_octant(t, *npnts_it)], *npnts_it);
	    }
	}
    } else {

	/* We are at a non-leaf node. Insert recursively into the appropriate
	 * child octant */
	octree_insert(t->children[octree_node_octant(t, p)], p);
    }
}

void
wn_calc_coeff(struct octree *t, std::map<struct pnt_normal *, fastf_t> *va)
{
    point_t pavg = VINIT_ZERO;
    vect_t nsum = VINIT_ZERO;
    fastf_t va_sum = 0.0;
    if (!t || !t->npnts->size()) return;

    for (int i = 0; i < 9; i++) t->w1[i] = 0.0;
    for (int i = 0; i < 27; i++) t->w2[i] = 0.0;

    std::vector<struct pnt_normal *>::iterator npnts_it;
    for (npnts_it = t->npnts->begin(); npnts_it != t->npnts->end(); npnts_it++) {
	struct pnt_normal *cp = *npnts_it;
	point_t pcurr;
	vect_t ncurr;
	VMOVE(pcurr, cp->v);
	VMOVE(ncurr, cp->n);
	VSCALE(pcurr, pcurr, (*va)[cp]);
	VSCALE(ncurr, ncurr, (*va)[cp]);
	VADD2(pavg, pavg, pcurr);
	VADD2(nsum, nsum, ncurr);
	va_sum = va_sum + (*va)[cp];
    }
    VSCALE(t->p_wsum, pavg, 1.0/va_sum);
    VMOVE(t->w0, nsum);

    for (npnts_it = t->npnts->begin(); npnts_it != t->npnts->end(); npnts_it++) {
	vect_t pp = VINIT_ZERO;
	vect_t ppw = VINIT_ZERO;
	fastf_t *w1 = (fastf_t *)bu_calloc(9, sizeof(fastf_t), "w1");
	fastf_t *w2 = (fastf_t *)bu_calloc(27, sizeof(fastf_t), "w1");
	struct pnt_normal *cp = *npnts_it;

	VSUB2(pp, cp->v, t->p_wsum);
	VSCALE(ppw, pp, (*va)[cp]);

	tensor2(&w1, &ppw, &(cp->n));
	tensor3(&w2, &ppw, &pp, &(cp->n));

	for (int i = 0; i < 9; i++) t->w1[i] += w1[i];
	for (int i = 0; i < 27; i++) t->w2[i] += w2[i];
	bu_free(w1, "w1");
	bu_free(w2, "w2");
    }
    for (int i = 0; i < 27; i++) t->w2[i] = t->w2[i] * 0.5;
}

void
wn_calc_coeffs(struct octree *t, int depth, std::map<struct pnt_normal *, fastf_t> *va)
{
    struct bu_vls w1 = BU_VLS_INIT_ZERO;
    struct bu_vls w2 = BU_VLS_INIT_ZERO;
    int d;

    if (!t || !t->npnts->size()) return;

    bu_log("%*sDepth %d: calculating coefficients from %d points...\n", depth, "", depth, t->npnts->size());
    wn_calc_coeff(t, va);

    /* Print results */
    bu_log("%*sw0: %g %g %g\n", depth, "", V3ARGS(t->w0));
    for (int i = 0; i < 9; i++) bu_vls_printf(&w1, "%g ", t->w1[i]);
    for (int i = 0; i < 27; i++) bu_vls_printf(&w2, "%g ", t->w2[i]);
    bu_log("%*sw1: %s\n", depth, "", bu_vls_addr(&w1));
    bu_log("%*sw2: %s\n", depth, "", bu_vls_addr(&w2));
    bu_vls_free(&w1);
    bu_vls_free(&w2);

    /* Handle children */
    d = depth + 1;
    for(int i = 0; i < 8; i++) {
	if (t->children[i]) wn_calc_coeffs(t->children[i], d, va);
    }
}

/* Eqn. 22 */
void
g1(vect_t *o, point_t *q, point_t *p_wavg)
{
    vect_t r;
    VSUB2(r, *p_wavg, *q);
    fastf_t rmag = MAGNITUDE(r);
    fastf_t denom = 4.0 * M_PI * rmag * rmag * rmag;
    VSCALE(*o, r, 1.0/denom);
}

/* Eqn. 23 */
void
g2(fastf_t **o, point_t *q, point_t *p_wavg)
{
    vect_t r;
    VSUB2(r, *p_wavg, *q);
    fastf_t id[9];
    fastf_t *rr = (fastf_t *)bu_calloc(9, sizeof(fastf_t), "w1");
    for (int i = 0; i < 9; i++) id[i] = 0.0;
    fastf_t rmag = MAGNITUDE(r);
    fastf_t denom1 = 4.0 * M_PI * rmag * rmag * rmag;
    fastf_t denom2 = (4.0 * M_PI * rmag * rmag * rmag * rmag * rmag);
    tensor2(&rr, &r, &r);
    id[0] = 1.0/denom1;
    id[4] = 1.0/denom1;
    id[8] = 1.0/denom1;
    for (int i = 0; i < 9; i++) rr[i] = rr[i] * 3.0/denom2;
    for (int i = 0; i < 9; i++) (*o)[i] = id[i] - rr[i];
    bu_free(rr, "rr");
}

/* Eqn. 24 */
void
g3(fastf_t **o, point_t *q, point_t *p_wavg)
{
    vect_t r;
    fastf_t t1[27];
    fastf_t *t2 = (fastf_t *)bu_calloc(27, sizeof(fastf_t), "w1");
    for (int i = 0; i < 27; i++) t1[i] = 0.0;
    vect_t idents[3];
    VSET(idents[0], 1, 0, 0);
    VSET(idents[1], 0, 1, 0);
    VSET(idents[2], 0, 0, 1);
    VSUB2(r, *p_wavg, *q);
    fastf_t rmag = MAGNITUDE(r);
    fastf_t m1 = -1.0 / (4.0 * M_PI * rmag * rmag * rmag * rmag * rmag);
    fastf_t m2 = 15.0 /(4.0 * M_PI * rmag * rmag * rmag * rmag * rmag * rmag * rmag);
    for (int i = 0; i < 3; i++) {
	fastf_t *temp1 = (fastf_t *)bu_calloc(27, sizeof(fastf_t), "w1");
	fastf_t *temp2 = (fastf_t *)bu_calloc(27, sizeof(fastf_t), "w1");
	fastf_t *temp3 = (fastf_t *)bu_calloc(27, sizeof(fastf_t), "w1");
	tensor3(&temp1, &r, &idents[i], &idents[i]);
	tensor3(&temp2, &idents[i], &r, &idents[i]);
	tensor3(&temp3, &idents[i], &idents[i], &r);
	for (int j = 0; j < 27; j++) t1[j] += temp1[j];
	for (int j = 0; j < 27; j++) t1[j] += temp2[j];
	for (int j = 0; j < 27; j++) t1[j] += temp3[j];
	bu_free(temp1, "temp1");
	bu_free(temp2, "temp2");
	bu_free(temp3, "temp3");
    }
    tensor3((fastf_t **)&t2, &r, &r, &r);
    for (int j = 0; j < 27; j++) t1[j] = t1[j] * m1;
    for (int j = 0; j < 27; j++) t1[j] = t2[j] * m2;
    for (int j = 0; j < 27; j++) (*o)[j] = t1[j] + t2[j];
    bu_free(t2, "t2");
}

double
wn_calc(struct octree *t, struct pnt_normal *q, std::map<struct pnt_normal *, fastf_t> *va)
{
    double wn;

    if (!t) return DBL_MAX;

    // If q not in t bbox, calculate and return wn approxmation

    // If not a leaf node, sum the wn approximations of the children
    // and return the sum.
    for(int i = 0; i < 8; i++) {
	wn += wn_calc(t->children[octree_node_octant(t, q)], q, va);
    }

    // If leaf node, calculate area-weighted dipole contributed by
    // associated point(s) and return the sum

    return wn;
}


/* nanoflann adaptor for BRL-CAD points */

struct NF_PC {
    std::vector<point_t *> pts;
};

struct NF_Adaptor {
    const struct NF_PC obj;
    NF_Adaptor(const struct NF_PC &obj_) : obj(obj_) { }
    inline const struct NF_PC& derived() const {return obj;}
    inline size_t kdtree_get_point_count() const { return derived().pts.size(); }
    inline fastf_t kdtree_get_pt(const size_t idx, int dim) const
    {
	if (dim == 0) return (*(derived().pts[idx]))[X];
	if (dim == 1) return (*(derived().pts[idx]))[Y];
	return (*(derived().pts[idx]))[Z];
    }
    template <class BBOX> bool kdtree_get_bbox(BBOX&) const {return false;}
};

/* Winding Number Meshing (as described in the paper at
 * http://www.dgp.toronto.edu/projects/fast-winding-numbers/):
 */

extern "C" void
wn_mesh(struct rt_pnts_internal *pnts)
{
    int pind = 0;
    std::map<struct pnt_normal *, fastf_t> voronoi_areas;
    struct pnt_normal *pn, *pl;
    point_t pcenter = VINIT_ZERO;
    point_t pmin, pmax;
    NF_PC cloud;
    typedef nanoflann::KDTreeSingleIndexAdaptor<nanoflann::L2_Simple_Adaptor<fastf_t, NF_Adaptor >, NF_Adaptor, 3 > nf_kdtree_t;

    /* 1.  Build kdtree for nanoflann to get nearest neighbor lookup */
    pl = (struct pnt_normal *)pnts->point;
    for (BU_LIST_FOR(pn, pnt_normal, &(pl->l))) {
	cloud.pts.push_back(&(pn->v));
    }
    const NF_Adaptor pc2kd(cloud);
    nf_kdtree_t index(3, pc2kd, nanoflann::KDTreeSingleIndexAdaptorParams(10));
    index.buildIndex();

    /* Calculate a[i] parameters for points */
    pl = (struct pnt_normal *)pnts->point;
    pind = 0;
    for (BU_LIST_FOR(pn, pnt_normal, &(pl->l))) {

	bu_log("pnt %d: center %g %g %g\n", pind, V3ARGS(pn->v));
	/* Running calculation of center pnt calculation input and bbox */
	VADD2(pcenter, pcenter, pn->v);
	VMINMAX(pmin, pmax, pn->v);

	/* 2.  Find k-nearest-neighbors set */
	size_t num_results = 3;
	std::vector<size_t> ret_index(num_results);
	std::vector<fastf_t> out_dist_sqr(num_results);
	index.knnSearch(pn->v, num_results, &ret_index[0], &out_dist_sqr[0]);
	// In case of less points in the tree than requested:
	ret_index.resize(num_results);
	out_dist_sqr.resize(num_results);
	std::cout << "knnSearch(" << pind << "): num_results=" << num_results << "\n";
	for (size_t i = 0; i < num_results; i++) {
	    std::cout << "idx["<< i << "]=" << ret_index[i] << " dist["<< i << "]=" << out_dist_sqr[i] << std::endl;
	}
	std::cout << "\n";

	/* MAYBE: Discard point if it doesn't have more than 2 other points
	 * within .01*radius of the bounding sphere of the object - in that situation
	 * it's too sparse to be a useful indicator and may even be some stray
	 * grazing ray reporting weirdness... */


	/* 3.  Find best fit plane for knn set.
	 *
	 * Thought:  port https://github.com/lucasmaystre/svdlibc to libbn data
	 * types and use the SVD plane fitter from test_bot2nurbs.cpp 
	 *
	 * As a first cut, try to use the tangent plane to the query point for
	 * this since we're (at the moment, anyway) guaranteed to have normals
	 * associated with the points anyway?  Eventually we probably want the
	 * above anyway to sanely handle weird things like thick plate mode
	 * inputs where normal behavior in the same point area is going to be
	 * strange, but for now see if we can go super simple...
	 */
	vect_t ux;
	bn_vec_ortho(ux, pn->n);
	VUNITIZE(ux);

	/* 4.  Project 3D points into plane, get 2D pnt set.
	 *
	 * Note: because the KNN search will return the query point as the
	 * first point in all cases (because in this situation the query point
	 * is itself part of the search set) we don't need to specially add it.
	 * Should we ever want to do this with arbitrary points, we won't be
	 * able to assume the query point is in ret_index */
	jcv_point *knn2dpnts = (jcv_point *)bu_calloc(num_results, sizeof(jcv_point), "knn 2d points");
	for (size_t i = 0; i < num_results; i++) {
	    vect_t v3d;
	    vect_t v3d_ortho, v3d_parallel;
	    vect_t v2d_ortho, v2d_parallel;
	    point_t *p = cloud.pts.at(ret_index[i]);
	    bu_log("pnt %d(%d): center %g %g %g\n", pind, i, V3ARGS(*p));
	    VSUB2(v3d, *p, pn->v);
	    VPROJECT(v3d, pn->n, v3d_parallel, v3d_ortho);
	    VPROJECT(v3d_ortho, ux, v2d_parallel, v2d_ortho);
	    knn2dpnts[i].x = MAGNITUDE(v2d_parallel);
	    knn2dpnts[i].y = MAGNITUDE(v2d_ortho);
	}

	/* 5.  Calculate Voronoi diagram in 2D.  Thoughts:
	 *
	 * Incorporate https://github.com/JCash/voronoi with (maybe) some
	 * additional mapping logic to make sure we can extract a polygon for a
	 * given point from the generated voronoi diagram.
	 */
	jcv_diagram diagram;
	memset(&diagram, 0, sizeof(jcv_diagram));
	jcv_diagram_generate(num_results, knn2dpnts, NULL, &diagram );
	const jcv_site* sites = jcv_diagram_get_sites(&diagram);
	const jcv_site* site = &sites[0]; // CHECK:  We want the cell around the query point, presumably this is it?

	/* 6.  Calculate the area of the Voronoi polygon around the current
	 * point.
	 *
	 * Hmm... paper uses TRIANGLE, but couldn't we just use Green's Theorem
	 * a.l.a http://alienryderflex.com/polygon_area/ for the area? */

	const jcv_graphedge* e = site->edges;
	double parea2 = 0.0;
	while (e) {
	    /* Note - we're assuming the edges are in polygon defining order
	     * here - if not, we will need to fix that... */
	    parea2 += (e->pos[1].x + e->pos[0].x) * (e->pos[1].y - e->pos[0].y);
	    e = e->next;
	}
	voronoi_areas[pn] = fabs(parea2) * 0.5;

	bu_log("voronoi_area[%d]: %g\n", pind, voronoi_areas[pn]);

	pind++;
    }

    /* 7.  Construct the octree for the input points */
    vect_t halfDim = VINIT_ZERO;
    VSCALE(pcenter, pcenter, 1.0/pind);
    VSET(halfDim, fabs(pmax[X] - pmin[X])/2, fabs(pmax[Y] - pmin[Y])/2, fabs(pmax[Z] - pmin[Z])/2);
    struct octree *t = octree_node_create(&pcenter, &halfDim);
    for (BU_LIST_FOR(pn, pnt_normal, &(pl->l))) {
	octree_insert(t, pn);
    }


    /* 7. Figure out how to set up the actual WN calculation to make the input
     * used to drive polygonalization, based on:
     *
     * https://github.com/sideeffects/WindingNumber
     * https://www.sidefx.com/docs/hdk/_s_o_p___winding_number_8_c_source.html
     */
    wn_calc_coeffs(t, 0, &voronoi_areas);


    /* 8. The paper mentions using a "continuation method [Wyvill et al.  1986]
     * for voxelization and isosurface extraction."  That appears to be this
     * approach:
     *
     * http://www.unchainedgeometry.com/jbloom/papers.html
     * https://github.com/Tonsty/polygonizer
     *
     * although again we have to figure out how to feed its inputs using the WN
     */
    struct pnt_normal query_pnt;
    wn_calc(t, &query_pnt, &voronoi_areas);
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8

