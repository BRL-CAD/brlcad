/*                        C U R V E T R E E . H
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

	/**
	 * Return just the leaves of the surface tree
	 */
	void getLeaves(std::list<const BRNode *> &out_leaves) const;
	void getLeavesAbove(std::list<const BRNode *> &out_leaves,
		const ON_Interval &u, const ON_Interval &v,
		fastf_t tol = BREP_UV_DIST_FUZZ) const;
	void getLeavesAbove(std::list<const BRNode *> &out_leaves, const ON_2dPoint &pt, fastf_t tol) const;
	void getLeavesRight(std::list<const BRNode *> &out_leaves, const ON_2dPoint &pt, fastf_t tol) const;
	std::size_t preparedLeafImageCount() const;

	/** Classify a face parameter against all prepared trim leaves without
	 * allocating a filtered list. */
	bool isTrimmed(const ON_2dPoint &uv, const BRNode **closest,
		double &closesttrim, double within_distance_tol,
		std::size_t *candidate_count = NULL,
		bool heal_join_gaps = true) const;

	/** Certify a UV cell whose material state cannot change without
	 * crossing a trim or a structural trim endpoint.  Returns false when
	 * the cell must retain pointwise trim checks. */
	bool classifyCell(const ON_Interval &u, const ON_Interval &v,
		double within_distance_tol, bool &trimmed) const;

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
	void prepareLoopLeaves();
	BRNode *subdivideCurve(const ON_Curve *curve, int trim_index, int adj_face_index, double min, double max, bool innerTrim, int depth) const;
	BRNode *curveBBox(const ON_Curve *curve, int trim_index, int adj_face_index, const ON_Interval &t, bool isLeaf, bool innerTrim, const ON_BoundingBox &bb) const;
	static ON_BoundingBox initialLoopBBox(const ON_BrepFace &face);

	const ON_BrepFace * const m_face;
	BRNode *m_root;


	struct Stl : public PooledObject<Stl> {
	    struct Loop {
		struct Leaf {
		    Leaf() : node(NULL), offset(0.0, 0.0),
			extend_minimum{false, false} {}
		    Leaf(const BRNode *leaf, const ON_2dVector &shift,
			bool extend_u_minimum, bool extend_v_minimum) :
			node(leaf), offset(shift),
			extend_minimum{extend_u_minimum,
			    extend_v_minimum} {}

		    const BRNode *node;
		    ON_2dVector offset;
		    bool extend_minimum[2];
		};

		Loop() : index(-1), type(ON_BrepLoop::unknown), minimum(0.0, 0.0),
		    maximum(0.0, 0.0), have_bbox(false), sortedX(), sortedY() {}

		int index;
		ON_BrepLoop::TYPE type;
		ON_2dPoint minimum;
		ON_2dPoint maximum;
		bool have_bbox;
		std::vector<Leaf> sortedX;
		std::vector<Leaf> sortedY;
	    };

	Stl() : m_sortedX(), m_loops() {}

	    std::vector<const BRNode *> m_sortedX;
	    std::vector<Loop> m_loops;
	} * const m_stl;
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
