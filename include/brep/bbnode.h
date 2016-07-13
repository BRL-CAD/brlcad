/*                        B B N O D E . H
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

/** @addtogroup brep_bbnode
 *
 * @brief
 * Bounding Box Hierarchy Node.
 *
 */
#ifndef BREP_BBNODE_H
#define BREP_BBNODE_H

#include "common.h"
#include "brep/defines.h"
#include "brep/ray.h"
#include "brep/brnode.h"
#include "brep/curvetree.h"
#include "brep/util.h"

/** @{ */
/** @file brep/bbnode.h */

#ifdef __cplusplus
extern "C++" {
    namespace brlcad {
	class BBNode;
    }
}
#endif

#ifndef __cplusplus
typedef struct _bounding_volume_placeholder {
    int dummy;
} BrepBoundingVolume;
#else
__BEGIN_DECLS
typedef brlcad::BBNode BrepBoundingVolume;
__END_DECLS
#endif

#ifdef __cplusplus

__BEGIN_DECLS

extern "C++" {
    namespace brlcad {

	/**
	 * Bounding Box Hierarchy Node
	 */
	class BREP_EXPORT BBNode {
	    public:
		explicit BBNode(const ON_BoundingBox &node, const CurveTree *ct = NULL);
		BBNode(const CurveTree *ct,
			const ON_BoundingBox &node,
			const ON_BrepFace *face,
			const ON_Interval &u,
			const ON_Interval &v,
			bool checkTrim,
			bool trimmed);
		~BBNode();

		/** Test if this node is a leaf node in the hierarchy */
		bool isLeaf() const;

		/** Return all leaves below this node that are leaf nodes */
		void getLeaves(std::list<const BBNode *> &out_leaves) const;

		/** Functions to add and remove child nodes from this node. */
		void addChild(BBNode *child);

		/** Report the depth of this node in the hierarchy */
		int depth() const;

		/** Get 2 points defining a bounding box
		 *
		 * @verbatim
		 *                _  max  _
		 *        _   -       +      -  _
		 *     *  _           +         _  *
		 *     |      -   _   + _   -      |
		 *     |             *+            |
		 *     |             |+            |
		 *     |          _  |+   _        |
		 *     |  _   -      |       -  _  |
		 *     *  _          |          _  *
		 *            -   _  |  _   -
		 *                  min
		 * @endverbatim
		 */
		void GetBBox(float *min, float *max) const;
		void GetBBox(double *min, double *max) const;

		/** Test whether a ray intersects the 3D bounding volume of the
		 * node - if so, and node is not a leaf node, query children.  If
		 * leaf node, and intersects, add to list.
		 */
		bool intersectsHierarchy(const ON_Ray &ray, std::list<const BBNode *> &results) const;

		ON_2dPoint getClosestPointEstimate(const ON_3dPoint &pt) const;
		ON_2dPoint getClosestPointEstimate(const ON_3dPoint &pt, ON_Interval &u, ON_Interval &v) const;
		int getLeavesBoundingPoint(const ON_3dPoint &pt, std::list<const BBNode *> &out) const;
		bool isTrimmed(const ON_2dPoint &uv, const BRNode **closest, double &closesttrim, double within_distance_tol) const;

		void BuildBBox();
		bool prepTrims();

		/** List of all children of a given node */
		std::vector<BBNode *> * const m_children;

		/** Bounding Box */
		ON_BoundingBox m_node;

		/** Surface Information */
		const ON_BrepFace *m_face;
		ON_Interval m_u;
		ON_Interval m_v;

		/** Trimming Flags */
		bool m_checkTrim;
		bool m_trimmed;

		/** Point used for closeness testing - usually based on evaluation
		 * of the curve/surface at the center of the parametric domain
		 */
		ON_3dPoint m_estimate;

		/* Normal at the m_estimate point */
		ON_3dVector m_normal;

	    private:
		BBNode(const BBNode &source);
		BBNode &operator=(const BBNode &source);

		void removeChild(BBNode *child);

		bool intersectedBy(const ON_Ray &ray, double *tnear = NULL, double *tfar = NULL) const;

		/** Report if a given uv point is within the uv boundaries defined
		 * by a node.
		 */
		bool containsUV(const ON_2dPoint &uv) const;

		bool doTrimming() const;
		void getTrimsAbove(const ON_2dPoint &uv, std::list<const BRNode *> &out_leaves) const;

		const BBNode *closer(const ON_3dPoint &pt, const BBNode *left, const BBNode *right) const;

		/** Curve Tree associated with the parent Surface Tree */
		const CurveTree * const m_ctree;

		std::list<const BRNode *> * const m_trims_above;
	};

	inline
	    BBNode::BBNode(const ON_BoundingBox &node, const CurveTree *ct) :
		m_children(new std::vector<BBNode *>),
		m_node(node),
		m_face(NULL),
		m_u(),
		m_v(),
		m_checkTrim(true),
		m_trimmed(false),
		m_estimate(),
		m_normal(),
		m_ctree(ct),
		m_trims_above(new std::list<const BRNode *>)
	{
	    for (int i = 0; i < 3; i++) {
		double d = m_node.m_max[i] - m_node.m_min[i];
		if (ON_NearZero(d, ON_ZERO_TOLERANCE)) {
		    m_node.m_min[i] -= 0.001;
		    m_node.m_max[i] += 0.001;
		}
	    }
	}

	inline
	    _BU_ATTR_ALWAYS_INLINE
	    BBNode::BBNode(
		    const CurveTree *ct,
		    const ON_BoundingBox &node,
		    const ON_BrepFace *face,
		    const ON_Interval &u,
		    const ON_Interval &v,
		    bool checkTrim,
		    bool trimmed):
		m_children(new std::vector<BBNode *>),
		m_node(node),
		m_face(face),
		m_u(u),
		m_v(v),
		m_checkTrim(checkTrim),
		m_trimmed(trimmed),
		m_estimate(),
		m_normal(),
		m_ctree(ct),
		m_trims_above(new std::list<const BRNode *>)
	{
	    for (int i = 0; i < 3; i++) {
		double d = m_node.m_max[i] - m_node.m_min[i];
		if (ON_NearZero(d, ON_ZERO_TOLERANCE)) {
		    m_node.m_min[i] -= 0.001;
		    m_node.m_max[i] += 0.001;
		}
	    }
	}

	inline void
	    BBNode::addChild(BBNode *child)
	    {
		if (LIKELY(child != NULL)) {
		    m_children->push_back(child);
		}
	    }

	inline void
	    BBNode::removeChild(BBNode *child)
	    {
		std::vector<BBNode *>::iterator i;
		for (i = m_children->begin(); i != m_children->end();) {
		    if (*i == child) {
			delete *i;
			i = m_children->erase(i);
		    } else {
			++i;
		    }
		}
	    }

	inline bool
	    BBNode::isLeaf() const
	    {
		if (m_children->empty()) {
		    return true;
		}
		return false;
	    }

	inline void
	    BBNode::GetBBox(float *min, float *max) const
	    {
		min[0] = m_node.m_min[0];
		min[1] = m_node.m_min[1];
		min[2] = m_node.m_min[2];
		max[0] = m_node.m_max[0];
		max[1] = m_node.m_max[1];
		max[2] = m_node.m_max[2];
	    }

	inline void
	    BBNode::GetBBox(double *min, double *max) const
	    {
		min[0] = m_node.m_min[0];
		min[1] = m_node.m_min[1];
		min[2] = m_node.m_min[2];
		max[0] = m_node.m_max[0];
		max[1] = m_node.m_max[1];
		max[2] = m_node.m_max[2];
	    }

	inline bool
	    BBNode::intersectedBy(const ON_Ray &ray, double *tnear_opt /* = NULL */, double *tfar_opt /* = NULL */) const
	    {
		double tnear = -DBL_MAX;
		double tfar = DBL_MAX;
		bool untrimmedresult = true;
		for (int i = 0; i < 3; i++) {
		    if (UNLIKELY(ON_NearZero(ray.m_dir[i]))) {
			if (ray.m_origin[i] < m_node.m_min[i] || ray.m_origin[i] > m_node.m_max[i]) {
			    untrimmedresult = false;
			}
		    } else {
			double t1 = (m_node.m_min[i] - ray.m_origin[i]) / ray.m_dir[i];
			double t2 = (m_node.m_max[i] - ray.m_origin[i]) / ray.m_dir[i];
			if (t1 > t2) {
			    double tmp = t1;    /* swap */
			    t1 = t2;
			    t2 = tmp;
			}

			V_MAX(tnear, t1);
			V_MIN(tfar, t2);

			if (tnear > tfar) { /* box is missed */
			    untrimmedresult = false;
			}
			/* go ahead and solve hits behind us
			   if (tfar < 0) untrimmedresult = false;
			   */
		    }
		}
		if (LIKELY(tnear_opt != NULL && tfar_opt != NULL)) {
		    *tnear_opt = tnear;
		    *tfar_opt = tfar;
		}
		if (isLeaf()) {
		    return !m_trimmed && untrimmedresult;
		} else {
		    return untrimmedresult;
		}
	    }

	inline bool
	    BBNode::doTrimming() const
	    {
		return m_checkTrim;
	    }


    } /* namespace brlcad */
} /* extern C++ */

__END_DECLS

#endif

/** @} */

#endif  /* BREP_BBNODE_H */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
