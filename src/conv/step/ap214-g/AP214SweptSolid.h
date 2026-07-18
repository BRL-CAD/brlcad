/*                 A P 2 1 4 S W E P T S O L I D . H
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 */

#ifndef CONV_STEP_AP214_G_AP214SWEPTSOLID_H
#define CONV_STEP_AP214_G_AP214SWEPTSOLID_H

#include <cstdint>
#include <vector>

class STEPWrapper;
class BRLCADWrapper;

/** Convert exact AP214 swept-area solids retained by the schema adapter. */
void ConvertAP214SweptSolids(STEPWrapper &wrapper, BRLCADWrapper &database,
    const std::vector<uint64_t> &excluded_sdrs = std::vector<uint64_t>());

#endif /* CONV_STEP_AP214_G_AP214SWEPTSOLID_H */
