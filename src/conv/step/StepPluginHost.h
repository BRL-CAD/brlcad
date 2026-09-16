/*                     S T E P P L U G I N H O S T . H
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 */

#ifndef CONV_STEP_STEPPLUGINHOST_H
#define CONV_STEP_STEPPLUGINHOST_H

#include "common.h"

#include <string>
#include <vector>

namespace brlcad {
namespace step {

bool schema_plugin_available(const std::string &schema);
std::vector<std::string> available_schema_plugins();
std::string schema_plugin_command(const std::string &schema, bool import_operation);
bool load_schema_plugin(const std::string &schema, std::string &error);

} // namespace step
} // namespace brlcad

#endif /* CONV_STEP_STEPPLUGINHOST_H */
