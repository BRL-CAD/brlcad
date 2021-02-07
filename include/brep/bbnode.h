/*                        B B N O D E . H
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
#include "brep/curvetree.h"
#include "brep/ray.h"
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
    class BREP_EXPORT BBNode : public PooledObject<BBNode> {
    public:
	explicit BBNode(const ON_BoundingBox &node, const CurveTree *ct = NULL);
	BBNode(const CurveTree *ct,
	       const ON_BoundingBox &node,
	       const ON_Interval &u,
	       const ON_Interval &v,
	       bool checkTrim,
	       bool trimmed);
	~BBNode();

	BBNode(Deserializer &deserieralizer, const CurveTree &ctree);
	void serialize(Serializer &serializer) const;

	const ON_BrepFace &get_face() const;

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

	const std::vector<BBNode *> &get_children() const;

	/** Bounding Box */
	ON_BoundingBox m_node;

	/** Surface Information */
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

	/** Curve Tree associated with the parent Surface Tree */
	const CurveTree * const m_ctree;


    private:
	BBNode(const BBNode &source);
	BBNode &operator=(const BBNode &source);

	void removeChild(BBNode *child);
	bool intersectedBy(const ON_Ray &ray, double *tnear = NULL, double *tfar = NULL) const;

	/** Report if a given uv point is within the uv boundaries defined
	 * by a node.
	 */
	bool containsUV(const ON_2dPoint &uv) const;

	void getTrimsAbove(const ON_2dPoint &uv, std::list<const BRNode *> &out_leaves) const;
	const BBNode *closer(const ON_3dPoint &pt, const BBNode *left, const BBNode *right) const;

	struct Stl : public PooledObject<Stl> {
	Stl() : m_children(), m_trims_above() {}

	    std::vector<BBNode *> m_children;
	    std::list<const BRNode *> m_trims_above;
	} * const m_stl;
    };

    inline void
    BBNode::addChild(BBNode *child)
    {
	if (LIKELY(child != NULL)) {
	    m_stl->m_children.push_back(child);
	}
    }

    inline void
    BBNode::removeChild(BBNode *child)
    {
	std::vector<BBNode *>::iterator i;
	for (i = m_stl->m_children.begin(); i != m_stl->m_children.end();) {
	    if (*i == child) {
		delete *i;
		i = m_stl->m_children.erase(i);
	    } else {
		++i;
	    }
	}
    }


    inline const ON_BrepFace &
    BBNode::get_face() const
    {
	return *m_ctree->m_face;
    }

    inline const std::vector<BBNode *> &
    BBNode::get_children() const
    {
	return m_stl->m_children;
    }

    inline bool
    BBNode::isLeaf() const
    {
	return m_stl->m_children.empty();
    }

    inline void
    BBNode::GetBBox(float *min, float *max) const
    {
	min[0] = (float)m_node.m_min.x;
	min[1] = (float)m_node.m_min.y;
	min[2] = (float)m_node.m_min.z;
	max[0] = (float)m_node.m_max.x;
	max[1] = (float)m_node.m_max.y;
	max[2] = (float)m_node.m_max.z;
    }

    inline void
    BBNode::GetBBox(double *min, double *max) const
    {
	min[0] = m_node.m_min.x;
	min[1] = m_node.m_min.y;
	min[2] = m_node.m_min.z;
	max[0] = m_node.m_max.x;
	max[1] = m_node.m_max.y;
	max[2] = m_node.m_max.z;
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
