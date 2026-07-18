/*                 S T E P S T R I N G . H
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 */

#ifndef CONV_STEP_STEPSTRING_H
#define CONV_STEP_STEPSTRING_H

#include "common.h"

#include <cstdint>
#include <set>
#include <string>

namespace brlcad {
namespace step {

/** Decode an ISO 10303-21 string token to UTF-8.
 *
 * Both quoted tokens and their unquoted payloads are accepted.  The decoder
 * handles doubled apostrophes, page/directive escapes, 8-bit hexadecimal
 * escapes, and the \X2\ / \X4\ Unicode forms.
 */
std::string decode_string(const std::string &input);

/** Return a stable BRL-CAD-safe spelling of a STEP string. */
std::string sanitize_name(const std::string &input);

/** Escape UTF-8 text for use as a JSON string payload (without quotes). */
std::string json_escape(const std::string &input);

/** Parse a comma-separated list of positive Part 21 instance identifiers.
 *
 * An optional leading '#' is accepted on each identifier.  Parsing is
 * transactional: values are appended to @p entity_ids only if the complete
 * input is valid.  Duplicate identifiers are harmless.
 */
bool parse_entity_id_list(const std::string &input,
    std::set<int64_t> &entity_ids, std::string *error = NULL);

} // namespace step
} // namespace brlcad

#endif /* CONV_STEP_STEPSTRING_H */
