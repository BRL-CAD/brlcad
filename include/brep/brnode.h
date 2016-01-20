/*                        B R N O D E . H
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

/** @addtogroup brep_brnode
 *
 * @brief
 * Bounding Rectangle Hierarchy Node.
 *
 */
#ifndef BREP_BRNODE_H
#define BREP_BRNODE_H

#include "common.h"
#ifdef __cplusplus
extern "C++" {
/* @cond */
#include <vector>
#include <list>
/* @endcond */
}
#endif
#include "brep/defines.h"

/** @{ */
/** @file brep/brnode.h */

#ifdef __cplusplus

__BEGIN_DECLS

extern "C++" {
    namespace brlcad {

	/**
	 * Bounding Rectangle Hierarchy
	 */
	class BREP_EXPORT BRNode {
	    public:
		BRNode();
		BRNode(const ON_BoundingBox &node);
		BRNode(const ON_Curve *curve,
			int m_adj_face_index,
			const ON_BoundingBox &node,
			const ON_BrepFace *face,
			const ON_Interval &t,
			bool innerTrim = false,
			bool checkTrim = true,
			bool trimmed = false);
		~BRNode();

		/** List of all children of a given node */
		std::vector<BRNode *> *m_children;

		/** Bounding Box */
		ON_BoundingBox m_node;

		/** Node management functions */
		void addChild(const ON_BoundingBox &child);
		void addChild(BRNode *child);
		void removeChild(BRNode *child);

		/** Test if this node is a leaf node (i.e. m_children is empty) */
		bool isLeaf();

		/** Return a list of all nodes below this node that are leaf nodes */
		void getLeaves(std::list<BRNode *> &out_leaves);

		/** Report the depth of this node in the hierarchy */
		int depth();

		/**
		 * Get 2 points defining bounding box:
		 *
		 * @verbatim
		 *       *----------------max
		 *       |                 |
		 *  v    |                 |
		 *       |                 |
		 *      min----------------*
		 *                 u
		 * @endverbatim
		 */
		void GetBBox(fastf_t *min, fastf_t *max) const;

		/** Surface Information */
		const ON_BrepFace *m_face;
		ON_Interval m_u;
		ON_Interval m_v;

		/** Trim Curve Information */
		const ON_Curve *m_trim;
		ON_Interval m_t;
		int m_adj_face_index;

		/** Trimming Flags */
		bool m_checkTrim;
		bool m_trimmed;
		bool m_XIncreasing;
		bool m_Horizontal;
		bool m_Vertical;
		bool m_innerTrim;

		ON_3dPoint m_estimate;
		ON_2dPoint getClosestPointEstimate(const ON_3dPoint &pt);
		ON_2dPoint getClosestPointEstimate(const ON_3dPoint &pt, ON_Interval &u, ON_Interval &v);
		fastf_t getLinearEstimateOfV(fastf_t u);
		fastf_t getCurveEstimateOfV(fastf_t u, fastf_t tol) const;
		fastf_t getCurveEstimateOfU(fastf_t v, fastf_t tol) const;

		bool isTrimmed(const ON_2dPoint &uv, double &trimdist) const;
		bool doTrimming() const;

	    private:
		BRNode *closer(const ON_3dPoint &pt, BRNode *left, BRNode *right);
		fastf_t m_slope;
		fastf_t m_vdot;
		fastf_t m_bb_diag;
		ON_3dPoint m_start;
		ON_3dPoint m_end;
	};

	inline
	    BRNode::BRNode()
	    {
		m_start = ON_3dPoint::UnsetPoint;
		m_end = ON_3dPoint::UnsetPoint;
		m_children = new std::vector<BRNode *>();
	    }

	inline
	    _BU_ATTR_ALWAYS_INLINE
	    BRNode::BRNode(
		    const ON_Curve *curve,
		    int adj_face_index,
		    const ON_BoundingBox &node,
		    const ON_BrepFace *face,
		    const ON_Interval &t,
		    bool innerTrim /* = false */,
		    bool checkTrim /* = true */,
		    bool trimmed /* = false */)
	    : m_node(node), m_face(face), m_trim(curve), m_t(t),
	    m_adj_face_index(adj_face_index), m_checkTrim(checkTrim),
	    m_trimmed(trimmed), m_innerTrim(innerTrim), m_slope(0.0), m_vdot(0.0)
	{
	    m_start = curve->PointAt(m_t[0]);
	    m_end = curve->PointAt(m_t[1]);
	    /* check for vertical segments they can be removed from trims
	     * above (can't tell direction and don't need
	     */
	    m_Horizontal = false;
	    m_Vertical = false;

	    /*
	     * should be okay since we split on Horz/Vert tangents
	     */
	    if (m_end[X] < m_start[X]) {
		m_u[0] = m_end[X];
		m_u[1] = m_start[X];
	    } else {
		m_u[0] = m_start[X];
		m_u[1] = m_end[X];
	    }
	    if (m_end[Y] < m_start[Y]) {
		m_v[0] = m_end[Y];
		m_v[1] = m_start[Y];
	    } else {
		m_v[0] = m_start[Y];
		m_v[1] = m_end[Y];
	    }

	    if (NEAR_EQUAL(m_end[X], m_start[X], 0.000001)) {
		m_Vertical = true;
		if (m_innerTrim) {
		    m_XIncreasing = false;
		} else {
		    m_XIncreasing = true;
		}
	    } else if (NEAR_EQUAL(m_end[Y], m_start[Y], 0.000001)) {
		m_Horizontal = true;
		if ((m_end[X] - m_start[X]) > 0.0) {
		    m_XIncreasing = true;
		} else {
		    m_XIncreasing = false;
		}
		m_slope = 0.0;
	    } else {
		if ((m_end[X] - m_start[X]) > 0.0) {
		    m_XIncreasing = true;
		} else {
		    m_XIncreasing = false;
		}
		m_slope = (m_end[Y] - m_start[Y]) / (m_end[X] - m_start[X]);
	    }
	    m_bb_diag = DIST_PT_PT(m_start, m_end);
	    m_children = new std::vector<BRNode *>();
	}

	inline
	    _BU_ATTR_ALWAYS_INLINE
	    BRNode::BRNode(const ON_BoundingBox &node)
	    : m_node(node)
	    {
		m_adj_face_index = -99;
		m_checkTrim = true;
		m_trimmed = false;
		m_Horizontal = false;
		m_Vertical = false;
		m_XIncreasing = false;
		m_innerTrim = false;
		m_bb_diag = 0.0;
		m_slope = 0.0;
		m_vdot = 0.0;
		m_face = NULL;
		m_trim = NULL;
		m_start = ON_3dPoint::UnsetPoint;
		m_end = ON_3dPoint::UnsetPoint;
		for (int i = 0; i < 3; i++) {
		    double d = m_node.m_max[i] - m_node.m_min[i];
		    if (NEAR_ZERO(d, ON_ZERO_TOLERANCE)) {
			m_node.m_min[i] -= 0.001;
			m_node.m_max[i] += 0.001;
		    }
		}
		m_start = m_node.m_min;
		m_end = m_node.m_max;
		m_children = new std::vector<BRNode *>();
	    }

	inline void
	    BRNode::addChild(const ON_BoundingBox &child)
	    {
		m_children->push_back(new BRNode(child));
	    }

	inline void
	    BRNode::addChild(BRNode *child)
	    {
		if (LIKELY(child != NULL)) {
		    m_children->push_back(child);
		}
	    }

	inline void
	    BRNode::removeChild(BRNode *child)
	    {
		std::vector<BRNode *>::iterator i;
		for (i = m_children->begin(); i < m_children->end(); ++i) {
		    if (*i == child) {
			delete *i;
			m_children->erase(i);
		    }
		}
	    }

	inline bool
	    BRNode::isLeaf()
	    {
		if (m_children->size() == 0) {
		    return true;
		}
		return false;
	    }

	inline void
	    _BU_ATTR_ALWAYS_INLINE
	    BRNode::GetBBox(fastf_t *min, fastf_t *max) const
	    {
		VSETALL(min, INFINITY);
		VSETALL(max, -INFINITY);
		if (m_start != ON_3dPoint::UnsetPoint) {
		    VMINMAX(min, max, m_start);
		}
		if (m_end != ON_3dPoint::UnsetPoint) {
		    VMINMAX(min, max, m_end);
		}
	    }

	inline bool
	    BRNode::doTrimming() const
	    {
		return m_checkTrim;
	    }

	extern bool sortX(BRNode *first, BRNode *second);
	extern bool sortY(BRNode *first, BRNode *second);


    } /* namespace brlcad */
} /* extern C++ */

__END_DECLS

#endif

/** @} */

#endif  /* BREP_BRNODE_H */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
