/*                         Q U A L I T Y . H
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
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
/** @file brep/quality.h */
#ifndef BREP_QUALITY_H
#define BREP_QUALITY_H

#include "common.h"

#include "bu/vls.h"
#include "brep/defines.h"

#ifdef __cplusplus
extern "C++" {

class ON_Brep;

struct brep_quality_options {
    double tolerance;
    double duplicate_vertex_tolerance;
    double min_edge_length;
    double min_trim_length;
    double max_aspect_ratio;
    int self_intersection_samples;
    bool check_self_intersection_risk;
};

BREP_EXPORT extern void
ON_Brep_Quality_Defaults(struct brep_quality_options *opts);

/**
 * Run geometry quality checks that are stricter than ON_Brep::IsValid().
 *
 * Returns the number of hard errors found.  Warnings, including conservative
 * self-intersection-risk notices, are written to msgs when supplied but do not
 * contribute to the return value.
 */
BREP_EXPORT extern int
ON_Brep_Quality_Check(const ON_Brep *brep, struct bu_vls *msgs, const struct brep_quality_options *opts);

}
#endif

#endif  /* BREP_QUALITY_H */
/*
 * Local Variables:
 * mode: C++
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
