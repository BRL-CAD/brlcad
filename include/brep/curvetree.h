/*                        C U R V E T R E E . H
 * BRL-CAD
 *
 * Copyright (c) 2004-2022 United States Government as represented by
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
/** @file brep/curvetree.h */
/** @addtogroup brep_curvetree
 *
 * @brief
 * Curve Tree.
 *
 */

#ifndef BREP_CURVETREE_H
#define BREP_CURVETREE_H

#include "common.h"

#ifdef __cplusplus
// @cond SKIP_C++_INCLUDE
extern "C++" {
#  include <map>
}
// @endcond
#endif

#include "brep/defines.h"
#include "brep/brnode.h"
#include "brep/util.h"


__BEGIN_DECLS

#ifdef __cplusplus
extern "C++" {

namespace brlcad {

    /**
     * CurveTree declaration
     */
    class BREP_EXPORT CurveTree : public PooledObject<CurveTree> {
    public:
	explicit CurveTree(const ON_BrepFace *face);
	~CurveTree();

	CurveTree(Deserializer &deserializer, const ON_BrepFace &face);
	void serialize(Serializer &serializer) const;
	std::vector<std::size_t> serialize_get_leaves_keys(const std::list<const BRNode *> &leaves) const;
	std::list<const BRNode *> serialize_get_leaves(const std::size_t *keys, std::size_t num_keys) const;
	void serialize_cleanup() const;

	/**
	 * Return just the leaves of the surface tree
	 */
	void getLeaves(std::list<const BRNode *> &out_leaves) const;
	void getLeavesAbove(std::list<const BRNode *> &out_leaves, const ON_Interval &u, const ON_Interval &v) const;
	void getLeavesAbove(std::list<const BRNode *> &out_leaves, const ON_2dPoint &pt, fastf_t tol) const;
	void getLeavesRight(std::list<const BRNode *> &out_leaves, const ON_2dPoint &pt, fastf_t tol) const;

    private:
	friend class BBNode;

	CurveTree(const CurveTree &source);
	CurveTree &operator=(const CurveTree &source);

	const BRNode *getRootNode() const;

	/**
	 * Calculate, using the surface bounding volume hierarchy, a uv
	 * estimate for the closest point on the surface to the point in
	 * 3-space.
	 */
	ON_2dPoint getClosestPointEstimate(const ON_3dPoint &pt) const;
	ON_2dPoint getClosestPointEstimate(const ON_3dPoint &pt, ON_Interval &u, ON_Interval &v) const;

	void getLeavesRight(std::list<const BRNode *> &out_leaves, const ON_Interval &u, const ON_Interval &v) const;

	int depth() const;

	bool getHVTangents(const ON_Curve *curve, const ON_Interval &t, std::list<fastf_t> &list) const;
	bool isLinear(const ON_Curve *curve, double min, double max) const;
	BRNode *subdivideCurve(const ON_Curve *curve, int trim_index, int adj_face_index, double min, double max, bool innerTrim, int depth) const;
	BRNode *curveBBox(const ON_Curve *curve, int trim_index, int adj_face_index, const ON_Interval &t, bool isLeaf, bool innerTrim, const ON_BoundingBox &bb) const;
	static ON_BoundingBox initialLoopBBox(const ON_BrepFace &face);

	const ON_BrepFace * const m_face;
	BRNode *m_root;


	struct Stl : public PooledObject<Stl> {
	Stl() : m_sortedX() {}

	    std::vector<const BRNode *> m_sortedX;
	} * const m_stl;

	mutable std::map<const BRNode *, std::size_t> *m_sortedX_indices;
    };

} /* namespace brlcad */

} /* extern C++ */

__END_DECLS

#endif /* __cplusplus */

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
