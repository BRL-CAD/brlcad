/*                        B R N O D E . H
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
/** @{ */
/** @file brep/brnode.h */
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
#  include <vector>
#  include <list>
/* @endcond */
}
#endif

#include "brep/defines.h"
#include "brep/util.h"

__BEGIN_DECLS

#ifdef __cplusplus
extern "C++" {
namespace brlcad {

    /**
     * Bounding Rectangle Hierarchy
     */
    class BREP_EXPORT BRNode : public PooledObject<BRNode> {
    public:
	explicit BRNode(const ON_BoundingBox &node);
	BRNode(const ON_Curve *curve,
	       int trim_index,
	       int adj_face_index,
	       const ON_BoundingBox &node,
	       const ON_BrepFace *face,
	       const ON_Interval &t,
	       bool innerTrim,
	       bool checkTrim,
	       bool trimmed);
	~BRNode();

	BRNode(Deserializer &deserializer, const ON_Brep &brep);
	void serialize(Serializer &serializer) const;

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

	fastf_t getLinearEstimateOfV(fastf_t u) const;
	const BRNode *closer(const ON_3dPoint &pt, const BRNode *left, const BRNode *right) const;

	struct Stl : public PooledObject<Stl> {
	Stl() : m_children() {}

	    std::vector<const BRNode *> m_children;
	} * const m_stl;

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

    inline void
    BRNode::addChild(BRNode *child)
    {
	if (LIKELY(child != NULL)) {
	    m_stl->m_children.push_back(child);
	}
    }

    inline void
    BRNode::removeChild(BRNode *child)
    {
	std::vector<const BRNode *>::iterator i;
	for (i = m_stl->m_children.begin(); i != m_stl->m_children.end();) {
	    if (*i == child) {
		delete *i;
		i = m_stl->m_children.erase(i);
	    } else {
		++i;
	    }
	}
    }

    inline bool
    BRNode::isLeaf() const
    {
	if (m_stl->m_children.empty()) {
	    return true;
	}
	return false;
    }

    inline void
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
