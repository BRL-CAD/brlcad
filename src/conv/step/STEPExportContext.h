/*                    S T E P E X P O R T C O N T E X T . H
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 */

#ifndef CONV_STEP_STEPEXPORTCONTEXT_H
#define CONV_STEP_STEPEXPORTCONTEXT_H

#include <cstddef>
#include <map>
#include <set>
#include <string>
#include <utility>

#include <sdai.h>

/** Schema-bound entities and schema-neutral lookup state shared by the four
 * mechanical exporters.  Include this only after the selected schema's
 * generated declarations have been loaded. */
struct AP203_Contents {
    Registry *registry = NULL;
    InstMgr *instance_list = NULL;
    STEPentity *default_context = NULL;
    STEPentity *application_context = NULL;
#if defined(AP203) || defined(AP203e2)
    STEPentity *design_context = NULL;
#endif
    std::map<struct directory *, STEPentity *> *solid_to_step = NULL;
    std::map<struct directory *, STEPentity *> *solid_to_step_shape = NULL;
    std::map<struct directory *, STEPentity *> *solid_to_step_manifold = NULL;
    std::map<struct directory *, STEPentity *> *comb_to_step = NULL;
    std::map<struct directory *, STEPentity *> *comb_to_step_shape = NULL;
    std::map<struct directory *, STEPentity *> *comb_to_step_manifold = NULL;
    std::map<std::pair<struct directory *, size_t>, STEPentity *> *occurrence_to_step = NULL;
    std::set<std::pair<struct directory *, size_t> > *representation_memberships = NULL;
    int flip_transforms = 0;
    /** Physical millimetres represented by one output length unit and the
     * reciprocal scale applied to BRL-CAD's millimetre geometry. */
    double length_unit_mm = 1.0;
    double mm_to_length_unit = 1.0;
    std::string length_unit = "mm";
    /** Global uncertainty is stored in output length units. */
    double uncertainty = 0.05;
    std::string plane_angle_unit = "degree";
    double radians_to_plane_angle = 57.2957795130823208768;
    struct db_i *dbip = NULL;
    struct rt_wdb *wdbp = NULL;
};

#endif /* CONV_STEP_STEPEXPORTCONTEXT_H */
