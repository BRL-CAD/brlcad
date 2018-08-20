#include "common.h"
#include "nanoflann.hpp"

#include "vmath.h"
#include "bn/mat.h"
#include "rt/geom.h"

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
    fastf_t *voronoi_areas;
    struct pnt_normal *pn, *pl;
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
    voronoi_areas = (fastf_t *)bu_calloc(pnts->count, sizeof(fastf_t), "a[i] values");
    pl = (struct pnt_normal *)pnts->point;
    pind = 0;
    for (BU_LIST_FOR(pn, pnt_normal, &(pl->l))) {

	bu_log("pnt %d: center %g %g %g\n", pind, V3ARGS(pn->v));

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

	/* 4.  Project 3D points into plane, get 2D pnt set. */
	point2d_t *knn2dpnts = (point2d_t *)bu_calloc(num_results, sizeof(point2d_t), "knn 2d points");
	for (size_t i = 0; i < num_results; i++) {
	    vect_t v3d;
	    vect_t v3d_ortho, v3d_parallel;
	    vect_t v2d_ortho, v2d_parallel;
	    point_t *p = cloud.pts.at(ret_index[i]);
	    bu_log("pnt %d(%d): center %g %g %g\n", pind, i, V3ARGS(*p));
	    VSUB2(v3d, *p, pn->v);
	    VPROJECT(v3d, pn->n, v3d_parallel, v3d_ortho);
	    VPROJECT(v3d_ortho, ux, v2d_parallel, v2d_ortho);
	    V2SET(knn2dpnts[i], MAGNITUDE(v2d_parallel), MAGNITUDE(v2d_ortho));
	}

	/* 5.  Calculate Voronoi diagram in 2D.  Thoughts:
	 *
	 * Incorporate https://github.com/JCash/voronoi with (maybe) some
	 * additional mapping logic to make sure we can extract a polygon for a
	 * given point from the generated voronoi diagram.
	 */
	voronoi_areas[pind] = 0.0;

	/* 6.  Calculate the area of the Voronoi polygon around the current
	 * point - paper uses TRIANGLE but couldn't we just use
	 * http://alienryderflex.com/polygon_area/ for the area? */

	/*ai[pind] = polygon_area(...);*/

	pind++;
    }

    /* 7. Figure out how to set up the actual WN calculation to make the input
     * used to drive polygonalization, based on:
     *
     * https://github.com/sideeffects/WindingNumber
     * https://www.sidefx.com/docs/hdk/_s_o_p___winding_number_8_c_source.html
     *
     */


    /* 8. The paper mentions using a "continuation method [Wyvill et al.  1986]
     * for voxelization and isosurface extraction."  That appears to be this
     * approach:
     *
     * http://www.unchainedgeometry.com/jbloom/papers.html
     * https://github.com/Tonsty/polygonizer
     *
     * although again we have to figure out how to feed its inputs using the WN
     */
}

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
