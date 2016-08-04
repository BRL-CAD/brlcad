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
	class BREP_EXPORT BRNode : public Arena<BRNode>::Object {
	    public:
		explicit BRNode(const ON_BoundingBox &node);
		BRNode(const ON_Curve *curve,
			int trim_curve_index,
			int adj_face_index,
			const ON_BoundingBox &node,
			const ON_BrepFace *face,
			const ON_Interval &t,
			bool innerTrim,
			bool checkTrim,
			bool trimmed);
		~BRNode();

		BRNode(ON_BinaryArchive &archive, const ON_Brep &brep, Arena<BRNode> &arena);
		void serialize(ON_BinaryArchive &archive) const;

		/** Node management functions */
		void addChild(BRNode *child);

		/** Return a list of all nodes below this node that are leaf nodes */
		void getLeaves(std::list<const BRNode *> &out_leaves) const;

		/** Report the depth of this node in the hierarchy */
		int depth() const;

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

		bool isTrimmed(const ON_2dPoint &uv, double &trimdist) const;

		ON_2dPoint getClosestPointEstimate(const ON_3dPoint &pt) const;
		ON_2dPoint getClosestPointEstimate(const ON_3dPoint &pt, ON_Interval &u, ON_Interval &v) const;
		fastf_t getCurveEstimateOfV(fastf_t u, fastf_t tol) const;
		fastf_t getCurveEstimateOfU(fastf_t v, fastf_t tol) const;

		/** Bounding Box */
		ON_BoundingBox m_node;

		/** Surface Information */
		ON_Interval m_v;

		/** Trim Curve Information */
		int m_adj_face_index;

		/** Trimming Flags */
		bool m_XIncreasing;
		bool m_Horizontal;
		bool m_Vertical;
		bool m_innerTrim;

	    private:
		BRNode(const BRNode &source);
		BRNode &operator=(const BRNode &source);

		void removeChild(BRNode *child);

		/** Test if this node is a leaf node (i.e. m_children is empty) */
		bool isLeaf() const;

		bool doTrimming() const;

		fastf_t getLinearEstimateOfV(fastf_t u) const;

		const BRNode *closer(const ON_3dPoint &pt, const BRNode *left, const BRNode *right) const;

		std::vector<const BRNode *> m_children;

		const ON_BrepFace *m_face;
		ON_Interval m_u;

		const ON_Curve *m_trim;
		int m_trim_index;
		ON_Interval m_t;

		bool m_checkTrim;
		bool m_trimmed;

		ON_3dPoint m_estimate;

		fastf_t m_slope;
		fastf_t m_bb_diag;
		ON_3dPoint m_start;
		ON_3dPoint m_end;
	};

	inline
	    _BU_ATTR_ALWAYS_INLINE
	    BRNode::BRNode(
		    const ON_Curve *curve,
		    int trim_index,
		    int adj_face_index,
		    const ON_BoundingBox &node,
		    const ON_BrepFace *face,
		    const ON_Interval &t,
		    bool innerTrim,
		    bool checkTrim,
		    bool trimmed) :
		m_node(node),
		m_v(),
		m_adj_face_index(adj_face_index),
		m_XIncreasing(false),
		m_Horizontal(false),
		m_Vertical(false),
		m_innerTrim(innerTrim),
		m_children(),
		m_face(face),
		m_u(),
		m_trim(curve),
		m_trim_index(trim_index),
		m_t(t),
		m_checkTrim(checkTrim),
		m_trimmed(trimmed),
		m_estimate(),
		m_slope(0.0),
		m_bb_diag(0.0),
		m_start(curve->PointAt(m_t[0])),
		m_end(curve->PointAt(m_t[1]))
	{
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
	}

	inline
	    _BU_ATTR_ALWAYS_INLINE
	    BRNode::BRNode(const ON_BoundingBox &node) :
		m_node(node),
		m_v(),
		m_adj_face_index(-1),
		m_XIncreasing(false),
		m_Horizontal(false),
		m_Vertical(false),
		m_innerTrim(false),
		m_children(),
		m_face(NULL),
		m_u(),
		m_trim(NULL),
		m_trim_index(-1),
		m_t(),
		m_checkTrim(true),
		m_trimmed(false),
		m_estimate(),
		m_slope(0.0),
		m_bb_diag(0.0),
		m_start(ON_3dPoint::UnsetPoint),
		m_end(ON_3dPoint::UnsetPoint)
	{
		for (int i = 0; i < 3; i++) {
		    double d = m_node.m_max[i] - m_node.m_min[i];
		    if (NEAR_ZERO(d, ON_ZERO_TOLERANCE)) {
			m_node.m_min[i] -= 0.001;
			m_node.m_max[i] += 0.001;
		    }
		}
		m_start = m_node.m_min;
		m_end = m_node.m_max;
	    }

	inline void
	    BRNode::addChild(BRNode *child)
	    {
		if (LIKELY(child != NULL)) {
		    m_children.push_back(child);
		}
	    }

	inline void
	    BRNode::removeChild(BRNode *child)
	    {
		std::vector<const BRNode *>::iterator i;
		for (i = m_children.begin(); i != m_children.end();) {
		    if (*i == child) {
			delete *i;
			i = m_children.erase(i);
		    } else {
			++i;
		    }
		}
	    }

	inline bool
	    BRNode::isLeaf() const
	    {
		if (m_children.empty()) {
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

	extern bool sortX(const BRNode *first, const BRNode *second);
	extern bool sortY(const BRNode *first, const BRNode *second);


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
