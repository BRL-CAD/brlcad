/*                       A P 2 1 4 C S G . H
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 */

#ifndef CONV_STEP_AP214_G_AP214CSG_H
#define CONV_STEP_AP214_G_AP214CSG_H

#include <cstdint>
#include <vector>

class BRLCADWrapper;
class STEPWrapper;

/** Convert AP214 CSG shape representations to native BRL-CAD trees. */
void ConvertAP214CSG(STEPWrapper &wrapper, BRLCADWrapper &database,
    const std::vector<uint64_t> &excluded_sdrs = std::vector<uint64_t>());

#endif /* CONV_STEP_AP214_G_AP214CSG_H */
