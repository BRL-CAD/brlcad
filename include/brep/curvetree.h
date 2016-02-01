/*                        C U R V E T R E E . H
 * BRL-CAD
 *
 * Copyright (c) 2004-2016 United States Government as represented by
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

/** @addtogroup brep_curvetree
 *
 * @brief
 * Curve Tree.
 *
 */
#ifndef BREP_CURVETREE_H
#define BREP_CURVETREE_H

#include "common.h"
#include "brep/defines.h"
#include "brep/brnode.h"

/** @{ */
/** @file brep/curvetree.h */

#ifdef __cplusplus

__BEGIN_DECLS

extern "C++" {
    namespace brlcad {

	/**
	 * CurveTree declaration
	 */
	class BREP_EXPORT CurveTree {
	    public:
		CurveTree(const ON_BrepFace *face);
		~CurveTree();

		BRNode *getRootNode() const;

		/**
		 * Calculate, using the surface bounding volume hierarchy, a uv
		 * estimate for the closest point on the surface to the point in
		 * 3-space.
		 */
		ON_2dPoint getClosestPointEstimate(const ON_3dPoint &pt);
		ON_2dPoint getClosestPointEstimate(const ON_3dPoint &pt, ON_Interval &u, ON_Interval &v);

		/**
		 * Return just the leaves of the surface tree
		 */
		void getLeaves(std::list<BRNode *> &out_leaves);
		void getLeavesAbove(std::list<BRNode *> &out_leaves, const ON_Interval &u, const ON_Interval &v);
		void getLeavesAbove(std::list<BRNode *> &out_leaves, const ON_2dPoint &pt, fastf_t tol);
		void getLeavesRight(std::list<BRNode *> &out_leaves, const ON_Interval &u, const ON_Interval &v);
		void getLeavesRight(std::list<BRNode *> &out_leaves, const ON_2dPoint &pt, fastf_t tol);
		int depth();

	    private:
		bool getHVTangents(const ON_Curve *curve, ON_Interval &t, std::list<fastf_t> &list);
		bool isLinear(const ON_Curve *curve, double min, double max);
		BRNode *subdivideCurve(const ON_Curve *curve, int adj_face_index, double min, double max, bool innerTrim, int depth);
		BRNode *curveBBox(const ON_Curve *curve, int adj_face_index, ON_Interval &t, bool leaf, bool innerTrim, const ON_BoundingBox &bb);
		BRNode *initialLoopBBox();

		const ON_BrepFace *m_face;
		BRNode *m_root;
		std::list<BRNode *> *m_sortedX;
	};


    } /* namespace brlcad */
} /* extern C++ */

__END_DECLS

#endif

/** @} */

#endif  /* BREP_CURVETREE_H */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
