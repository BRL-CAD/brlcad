/* BRL-CAD
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#ifndef CONV_IGES_NATIVE_H
#define CONV_IGES_NATIVE_H

#include "common.h"
#include "iges_document.h"
#include "brep.h"
#include <functional>
#include <memory>
#include <string>
#include <vector>

struct rt_wdb;

namespace brlcad {
namespace iges {

using ProfileCurves = std::vector<std::unique_ptr<ON_NurbsCurve>>;
using ProfileReader = std::function<bool(EntityId, ProfileCurves &)>;
using NativeBrepWriter = std::function<bool(const std::string &, ON_Brep &)>;

bool read_model_curves(const Document &document, EntityId id,
    ProfileCurves &curves, std::string &error);

bool write_wire_curves(const Document &document, const DirectoryEntry &entry,
    struct rt_wdb *database, const std::string &name, bool project, std::string &error);

const char *native_solid_name(int type);

/** Coordinates returned by read_profile are in millimeters, with the curve's
 * placement applied.  The caller applies the solid's directory placement. */
bool write_native_solid(const Document &document, const DirectoryEntry &entry,
    struct rt_wdb *database, const std::string &name, double unit_to_mm,
    double tolerance, const ProfileReader &read_profile,
    const NativeBrepWriter &write_brep, std::string &error);

} // namespace iges
} // namespace brlcad
#endif

/*
 * Local Variables:
 * mode: C++
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
