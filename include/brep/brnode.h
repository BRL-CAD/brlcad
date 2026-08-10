/*                        B R N O D E . H
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
#  include <algorithm>
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
     * Compact prepared trim-curve leaf.
     */
    class BREP_EXPORT BRNode {
    public:
	BRNode(const ON_Curve *curve,
	       int trim_index,
	       int adj_face_index,
	       const ON_BoundingBox &node,
	       const ON_Interval &t,
	       bool innerTrim);
	BRNode(Deserializer &deserializer, const ON_Brep &brep);
	void serialize(Serializer &serializer) const;

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

	/** Test a vertical +V parameter-space ray against this monotone
	 * trim leaf.  Endpoint fuzz bridges small imported loop gaps while
	 * the exact test uses a half-open interval to avoid double counting
	 * shared vertices. */
	bool crossesAbove(const ON_2dPoint &uv, double &distance,
		double endpoint_tolerance = 0.0) const;
	/** Test a horizontal +U parameter-space ray using the same half-open
	 * endpoint ownership as crossesAbove(). */
	bool crossesRight(const ON_2dPoint &uv, double &distance,
		double endpoint_tolerance = 0.0) const;
	/** Return a fixed-workspace estimate of the shortest UV distance from
	 * a point to this trim leaf. */
	double curveDistance(const ON_2dPoint &uv) const;
	/** Lower bound from the leaf's sampled UV bounding box. */
	double curveBBoxDistance(const ON_2dPoint &uv) const;
	int trimIndex() const { return m_trim_index; }
	ON_Interval parameterInterval() const { return m_t; }
	ON_Interval curveDomain() const {
	    return m_trim ? m_trim->Domain() : ON_Interval::EmptyInterval;
	}
	ON_3dPoint startPoint() const {
	    return ON_3dPoint(m_start[0], m_start[1], 0.0);
	}
	ON_3dPoint endPoint() const {
	    return ON_3dPoint(m_end[0], m_end[1], 0.0);
	}
	ON_2dPoint bboxMinimum() const {
	    return ON_2dPoint(m_bbox_min[0], m_bbox_min[1]);
	}
	ON_2dPoint bboxMaximum() const {
	    return ON_2dPoint(m_bbox_max[0], m_bbox_max[1]);
	}

	fastf_t getCurveEstimateOfV(fastf_t u, fastf_t tol) const;
	fastf_t getCurveEstimateOfU(fastf_t v, fastf_t tol) const;

	/** Trim Curve Information */
	int m_adj_face_index;

	/** Trimming Flags */
	bool m_XIncreasing;
	bool m_Horizontal;
	bool m_Vertical;
	bool m_innerTrim;

    private:
	const ON_Curve *m_trim;
	int m_trim_index;
	ON_Interval m_t;
	double m_bbox_min[2];
	double m_bbox_max[2];
	double m_start[2];
	double m_end[2];
    };

    inline void
    BRNode::GetBBox(fastf_t *min, fastf_t *max) const
    {
	VSET(min, std::min(m_start[X], m_end[X]),
	    std::min(m_start[Y], m_end[Y]), 0.0);
	VSET(max, std::max(m_start[X], m_end[X]),
	    std::max(m_start[Y], m_end[Y]), 0.0);
    }
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
