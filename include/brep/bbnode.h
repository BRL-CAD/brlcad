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
		BBNode();
		BBNode(const ON_BoundingBox &node);
		BBNode(CurveTree *ct);
		BBNode(CurveTree *ct, const ON_BoundingBox &node);
		BBNode(CurveTree *ct,
			const ON_BoundingBox &node,
			const ON_BrepFace *face,
			const ON_Interval &u,
			const ON_Interval &v,
			bool checkTrim = false,
			bool trimmed = false);
		~BBNode();

		/** List of all children of a given node */
		std::vector<BBNode *> *m_children;

		/** Curve Tree associated with the parent Surface Tree */
		CurveTree *m_ctree;

		/** Bounding Box */
		ON_BoundingBox m_node;

		/** Test if this node is a leaf node in the hierarchy */
		bool isLeaf();

		/** Return all leaves below this node that are leaf nodes */
		void getLeaves(std::list<BBNode *> &out_leaves);

		/** Functions to add and remove child nodes from this node. */
		void addChild(const ON_BoundingBox &child);
		void addChild(BBNode *child);
		void removeChild(BBNode *child);

		/** Report the depth of this node in the hierarchy */
		int depth();

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
		void GetBBox(float *min, float *max);
		void GetBBox(double *min, double *max);

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

		/** Test whether a ray intersects the 3D bounding volume of the
		 * node - if so, and node is not a leaf node, query children.  If
		 * leaf node, and intersects, add to list.
		 */
		bool intersectedBy(ON_Ray &ray, double *tnear = NULL, double *tfar = NULL);
		bool intersectsHierarchy(ON_Ray &ray, std::list<BBNode *> &results);

		/** Report if a given uv point is within the uv boundaries defined
		 * by a node.
		 */
		bool containsUV(const ON_2dPoint &uv);

		ON_2dPoint getClosestPointEstimate(const ON_3dPoint &pt);
		ON_2dPoint getClosestPointEstimate(const ON_3dPoint &pt, ON_Interval &u, ON_Interval &v);
		int getLeavesBoundingPoint(const ON_3dPoint &pt, std::list<BBNode *> &out);
		bool isTrimmed(const ON_2dPoint &uv, BRNode **closest, double &closesttrim, double within_distance_tol) const;
		bool doTrimming() const;

		void getTrimsAbove(const ON_2dPoint &uv, std::list<BRNode *> &out_leaves) const;
		void BuildBBox();
		bool prepTrims();

	    private:
		BBNode *closer(const ON_3dPoint &pt, BBNode *left, BBNode *right);
		std::list<BRNode *> *m_trims_above;
	};

	inline
	    BBNode::BBNode()
	    : m_ctree(NULL), m_face(NULL), m_checkTrim(true), m_trimmed(false)
	    {
		m_children = new std::vector<BBNode *>();
		m_trims_above = new std::list<BRNode *>();
	    }

	inline
	    BBNode::BBNode(const ON_BoundingBox &node)
	    : m_ctree(NULL), m_node(node), m_face(NULL), m_checkTrim(true), m_trimmed(false)
	    {
		for (int i = 0; i < 3; i++) {
		    double d = m_node.m_max[i] - m_node.m_min[i];
		    if (ON_NearZero(d, ON_ZERO_TOLERANCE)) {
			m_node.m_min[i] -= 0.001;
			m_node.m_max[i] += 0.001;
		    }
		}
		m_children = new std::vector<BBNode *>();
		m_trims_above = new std::list<BRNode *>();
	    }

	inline
	    BBNode::BBNode(CurveTree *ct)
	    : m_ctree(ct), m_face(NULL), m_checkTrim(true), m_trimmed(false)
	    {
		m_children = new std::vector<BBNode *>();
		m_trims_above = new std::list<BRNode *>();
	    }

	inline
	    BBNode::BBNode(CurveTree *ct, const ON_BoundingBox &node)
	    : m_ctree(ct), m_node(node), m_face(NULL), m_checkTrim(true), m_trimmed(false)
	    {
		for (int i = 0; i < 3; i++) {
		    double d = m_node.m_max[i] - m_node.m_min[i];
		    if (ON_NearZero(d, ON_ZERO_TOLERANCE)) {
			m_node.m_min[i] -= 0.001;
			m_node.m_max[i] += 0.001;
		    }
		}
		m_children = new std::vector<BBNode *>();
		m_trims_above = new std::list<BRNode *>();
	    }

	inline
	    _BU_ATTR_ALWAYS_INLINE
	    BBNode::BBNode(
		    CurveTree *ct,
		    const ON_BoundingBox &node,
		    const ON_BrepFace *face,
		    const ON_Interval &u,
		    const ON_Interval &v,
		    bool checkTrim /* = false */,
		    bool trimmed /* = false */)
	    : m_ctree(ct), m_node(node), m_face(face), m_u(u), m_v(v),
	    m_checkTrim(checkTrim), m_trimmed(trimmed)
	{
	    for (int i = 0; i < 3; i++) {
		double d = m_node.m_max[i] - m_node.m_min[i];
		if (ON_NearZero(d, ON_ZERO_TOLERANCE)) {
		    m_node.m_min[i] -= 0.001;
		    m_node.m_max[i] += 0.001;
		}
	    }
	    m_children = new std::vector<BBNode *>();
	    m_trims_above = new std::list<BRNode *>();
	}

	inline void
	    BBNode::addChild(const ON_BoundingBox &child)
	    {
		m_children->push_back(new BBNode(child));
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
		for (i = m_children->begin(); i < m_children->end(); ++i) {
		    if (*i == child) {
			delete *i;
			m_children->erase(i);
		    }
		}
	    }

	inline bool
	    BBNode::isLeaf()
	    {
		if (m_children->size() == 0) {
		    return true;
		}
		return false;
	    }

	inline void
	    BBNode::GetBBox(float *min, float *max)
	    {
		min[0] = m_node.m_min[0];
		min[1] = m_node.m_min[1];
		min[2] = m_node.m_min[2];
		max[0] = m_node.m_max[0];
		max[1] = m_node.m_max[1];
		max[2] = m_node.m_max[2];
	    }

	inline void
	    BBNode::GetBBox(double *min, double *max)
	    {
		min[0] = m_node.m_min[0];
		min[1] = m_node.m_min[1];
		min[2] = m_node.m_min[2];
		max[0] = m_node.m_max[0];
		max[1] = m_node.m_max[1];
		max[2] = m_node.m_max[2];
	    }

	inline bool
	    BBNode::intersectedBy(ON_Ray &ray, double *tnear_opt /* = NULL */, double *tfar_opt /* = NULL */)
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
