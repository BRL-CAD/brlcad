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

struct bu_mime_traits;
struct bu_mime_capability;

namespace brlcad {
namespace step {

struct HeaderSchemaInfo;
std::vector<std::string> schema_plugin_candidates(const HeaderSchemaInfo &header,
    const std::string &requested, std::string &error);

std::string select_schema_output(const char *format, const bu_mime_traits *requested,
    const std::string &explicit_schema, const std::string &default_schema, std::string &error);

/** Copy a configured binding's MIME clause into an initially zeroed owner.
 * Reader clauses describe product-model families. Generic Part 21 remains
 * an explicit source assertion, rather than a promise to read any EXPRESS
 * schema. A decoder binding's edition is not necessarily a source constraint. */
bool schema_mime_capability(const std::string &schema, const char *type,
    unsigned operation, bu_mime_capability &capability);
bool list_schema_mime_profiles(unsigned operation);

bool schema_plugin_available(const std::string &schema);
std::vector<std::string> available_schema_plugins();
std::string schema_plugin_command(const std::string &schema, bool import_operation);
bool load_schema_plugin(const std::string &schema, std::string &error);

} // namespace step
} // namespace brlcad

#endif /* CONV_STEP_STEPPLUGINHOST_H */
