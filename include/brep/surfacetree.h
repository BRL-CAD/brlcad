/*                       S U R F A C E T R E E . H
 * BRL-CAD
 *
 * Copyright (c) 2004-2021 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */

/** @addtogroup brep_surfacetree
 *
 * @brief
 * Curve Tree.
 *
 */
#ifndef BREP_SURFACETREE_H
#define BREP_SURFACETREE_H

#include "common.h"
#ifdef __cplusplus
extern "C++" {
/* @cond */
#include <list>
#include <queue>
/* @endcond */
}
#endif
#include "brep/defines.h"
#include "brep/curvetree.h"
#include "brep/bbnode.h"

/** @{ */
/** @file brep/surfacetree.h */

#ifdef __cplusplus

__BEGIN_DECLS

extern "C++" {
namespace brlcad {

    /**
     * SurfaceTree declaration
     */
    class BREP_EXPORT SurfaceTree {
    public:
	explicit SurfaceTree(const ON_BrepFace *face, bool removeTrimmed = true, int depthLimit = BREP_MAX_FT_DEPTH, double within_distance_tol = BREP_EDGE_MISS_TOLERANCE);
	~SurfaceTree();

	/* Report whether the tree is in a usable state */
	bool Valid() const {
	    if (!m_root)
		return false;
	    return true;
	};

	BBNode *getRootNode() const;

	/**
	 * Calculate, using the surface bounding volume hierarchy, a uv
	 * estimate for the closest point on the surface to the point in
	 * 3-space.
	 */
	ON_2dPoint getClosestPointEstimate(const ON_3dPoint &pt) const;
	ON_2dPoint getClosestPointEstimate(const ON_3dPoint &pt, ON_Interval &u, ON_Interval &v) const;

	/**
	 * Return surface
	 */
	const ON_Surface *getSurface() const;
	int getSurfacePoint(const ON_3dPoint &pt, ON_2dPoint &uv, const ON_3dPoint &from, double tolerance = BREP_SAME_POINT_TOLERANCE) const;

	/**
	 * Return just the leaves of the surface tree
	 */
	void getLeaves(std::list<const BBNode *> &out_leaves) const;

	const CurveTree *m_ctree;

    private:
	SurfaceTree(const SurfaceTree &source);
	SurfaceTree &operator=(const SurfaceTree &source);

	int depth() const;
	bool isFlat(const ON_Plane frames[9]) const;
	bool isStraight(const ON_Plane frames[9]) const;
	bool isFlatU(const ON_Plane frames[9]) const;
	bool isFlatV(const ON_Plane frames[9]) const;
	BBNode *subdivideSurface(const ON_Surface *localsurf, const ON_Interval &u, const ON_Interval &v, ON_Plane frames[9], int depth, int depthLimit, int prev_knot, double within_distance_tol) const;
	BBNode *surfaceBBox(const ON_Surface *localsurf, bool leaf, const ON_Plane frames[9], const ON_Interval &u, const ON_Interval &v, double within_distance_tol) const;

	const bool m_removeTrimmed;
	const ON_BrepFace * const m_face;
	BBNode *m_root;
	std::queue<ON_Plane *> * const m_f_queue;
    };

} /* namespace brlcad */
} /* extern C++ */

__END_DECLS

#endif

/** @} */

#endif  /* BREP_SURFACETREE_H */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
