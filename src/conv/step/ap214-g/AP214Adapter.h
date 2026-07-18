/*                   A P 2 1 4 A D A P T E R . H
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 */

#ifndef CONV_STEP_AP214_G_AP214ADAPTER_H
#define CONV_STEP_AP214_G_AP214ADAPTER_H

#include "common.h"

#include <string>
#include <vector>

namespace brlcad {
namespace step {

struct AP214SchemaInfo {
    bool accepted = false;
    bool legacy_cc2 = false;
    std::vector<std::string> identifiers;
    std::string error;
    std::string suggested_converter;
};

/** Lightweight AP214 Part 21 header adapter.
 *
 * Geometry extraction remains in the schema-neutral STEPWrapper.  This class
 * is deliberately independent of generated bindings so schema rejection can
 * happen before the expensive AUTOMOTIVE_DESIGN registry is materialized.
 */
class AP214Adapter {
public:
    static AP214SchemaInfo inspect_file(const std::string &path);
    static AP214SchemaInfo inspect_header(const std::string &header);
};

} // namespace step
} // namespace brlcad

#endif /* CONV_STEP_AP214_G_AP214ADAPTER_H */

