/*               S T E P N A T I V E C S G I M P O R T . H
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 */

#ifndef CONV_STEP_NATIVE_CSG_IMPORT_H
#define CONV_STEP_NATIVE_CSG_IMPORT_H

#include <cstdint>
#include <vector>

class BRLCADWrapper;
class STEPWrapper;

/** Convert AP203e2/AP214/AP242 CSG shape representations to BRL-CAD trees. */
void ImportSTEPNativeCSG(STEPWrapper &wrapper, BRLCADWrapper &database,
    const std::vector<uint64_t> &excluded_sdrs = std::vector<uint64_t>());

#endif /* CONV_STEP_NATIVE_CSG_IMPORT_H */
