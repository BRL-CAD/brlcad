/*                    S T E P H E A D E R S C H E M A . H
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 */

#ifndef CONV_STEP_STEPHEADERSCHEMA_H
#define CONV_STEP_STEPHEADERSCHEMA_H

#include "common.h"

#include <string>
#include <vector>

namespace brlcad {
namespace step {

enum class HeaderSchema {
    Unknown,
    AP203,
    AP203e2,
    AP214,
    AP242,
    IFC
};

struct HeaderSchemaInfo {
    HeaderSchema schema = HeaderSchema::Unknown;
    bool recognized = false;
    bool ambiguous = false;
    bool legacy_identifier = false;
    std::vector<std::string> identifiers;
    std::vector<std::string> unrecognized_identifiers;
    std::string error;
};

class STEPHeaderSchema {
public:
    static HeaderSchemaInfo inspect_file(const std::string &path);
    static HeaderSchemaInfo inspect_header(const std::string &header);
    static const char *key(HeaderSchema schema);
};

} // namespace step
} // namespace brlcad

#endif /* CONV_STEP_STEPHEADERSCHEMA_H */
