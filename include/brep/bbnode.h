/*                        B B N O D E . H
 * BRL-CAD
 *
 * Copyright (c) 2004-2026 United States Government as represented by
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

#ifdef __cplusplus
extern "C++" {
#  include <cstdint>
#  include <list>
}
#endif

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
    class BBNodeArena;
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
	       const ON_Interval &u,
	       const ON_Interval &v,
	       bool checkTrim,
	       bool trimmed);
	~BBNode();

	BBNode(Deserializer &deserieralizer, const CurveTree &ctree);
	static BBNode *deserializeTree(Deserializer &deserializer,
		const CurveTree &ctree);
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
	 *
	 * The fixed-array overload appends while count is below capacity,
	 * increments count for every intersected leaf, and sets overflow if
	 * any leaf cannot be stored.  The caller initializes count and
	 * overflow and may use the total count to select a fallback.
	 */
	bool intersectsHierarchy(const ON_Ray &ray, std::list<const BBNode *> &results) const;
	bool intersectsHierarchy(const ON_Ray &ray, const BBNode **results,
		std::size_t capacity, std::size_t &count,
		bool &overflow) const;

	ON_2dPoint getClosestPointEstimate(const ON_3dPoint &pt) const;
	ON_2dPoint getClosestPointEstimate(const ON_3dPoint &pt, ON_Interval &u, ON_Interval &v) const;
	int getLeavesBoundingPoint(const ON_3dPoint &pt, std::list<const BBNode *> &out) const;
	/** Classify against the prepared trim leaves without allocating.
	 * isTrimmedAllocating retains the former filtered-list path for
	 * transition-time equivalence diagnostics only. */
	bool isTrimmed(const ON_2dPoint &uv, const BRNode **closest,
		double &closesttrim, double within_distance_tol,
		std::size_t *candidate_count = NULL) const;
	bool isTrimmedAllocating(const ON_2dPoint &uv, const BRNode **closest,
		double &closesttrim, double within_distance_tol,
		std::size_t *candidate_count = NULL) const;

	void BuildBBox();
	bool prepTrims(double within_distance_tol =
		BREP_EDGE_MISS_TOLERANCE);

	std::size_t childCount() const;
	BBNode *child(std::size_t index) const;

	/** Bounding Box */
	ON_BoundingBox m_node;

	/** Surface Information */
	ON_Interval m_u;
	ON_Interval m_v;

	/** Trimming Flags */
	bool m_checkTrim;
	bool m_trimmed;
	/* Arena construction nodes are destroyed shallowly by their arena. */
	bool m_arena_node;

	/** Point used for closeness testing - usually based on evaluation
	 * of the curve/surface at the center of the parametric domain
	 */
	ON_3dPoint m_estimate;

	/* Normal at the m_estimate point */
	ON_3dVector m_normal;

	/** Curve Tree associated with the parent Surface Tree */
	const CurveTree * const m_ctree;


    private:
	friend class BBNodeArena;

	BBNode(const BBNode &source);
	BBNode &operator=(const BBNode &source);
	BBNode(Deserializer &deserializer, const CurveTree &ctree,
		BBNodeArena *arena);
	void adopt(BBNode &source);
	void setArenaOwner(BBNodeArena *owner);

	bool intersectedBy(const ON_Ray &ray, double *tnear = NULL, double *tfar = NULL) const;

	/** Report if a given uv point is within the uv boundaries defined
	 * by a node.
	 */
	bool containsUV(const ON_2dPoint &uv) const;

	const BBNode *closer(const ON_3dPoint &pt, const BBNode *left, const BBNode *right) const;

	BBNode **m_children;
	uint32_t m_child_count;
	uint32_t m_child_capacity;
    };

    /**
     * Per-SurfaceTree construction storage.  The first node uses ordinary
     * transferable storage and subsequent nodes are bulk allocated.  The
     * final logical root is adopted into that first node, whose lifetime owns
     * the arena.
     */
    class BBNodeArena {
    public:
	BBNodeArena();
	~BBNodeArena();

	BBNode *make(const ON_BoundingBox &node,
		const CurveTree *ct = NULL);
	BBNode *make(const CurveTree *ct, const ON_BoundingBox &node,
		const ON_Interval &u, const ON_Interval &v,
		bool checkTrim, bool trimmed);
	BBNode *finish(BBNode *root);

    private:
	friend class BBNode;
	struct Block;
	void *allocate();
	BBNode *make(Deserializer &deserializer, const CurveTree &ctree);

	Block *m_blocks;
	BBNode *m_anchor;
	bool m_started;
    };

    inline const ON_BrepFace &
    BBNode::get_face() const
    {
	return *m_ctree->m_face;
    }

    inline std::size_t
    BBNode::childCount() const
    {
	return m_child_count;
    }

    inline BBNode *
    BBNode::child(std::size_t index) const
    {
	return index < m_child_count ? m_children[index] : NULL;
    }

    inline bool
    BBNode::isLeaf() const
    {
	return m_child_count == 0;
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
