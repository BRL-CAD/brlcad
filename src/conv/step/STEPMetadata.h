/*                     S T E P M E T A D A T A . H
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 */

#ifndef CONV_STEP_STEPMETADATA_H
#define CONV_STEP_STEPMETADATA_H

#include "common.h"

#include <map>
#include <string>
#include <vector>

struct db_i;

namespace brlcad {
namespace step {

extern const char *const STEP_METADATA_OBJECT;
extern const char *const STEP_METADATA_FORMAT;
extern const char *const STEP_METADATA_OBJECT_ATTRIBUTE;
extern const char *const STEP_METADATA_FORMAT_ATTRIBUTE;
extern const char *const STEP_METADATA_RECORDS_ATTRIBUTE;

/** Encode a flattened retained STEP graph in the portable, versioned binary
 * representation used by STEP_METADATA_OBJECT. */
bool EncodeSTEPMetadata(const std::map<std::string, std::string> &attributes,
	std::vector<unsigned char> &bytes, std::string &error);

/** Decode and validate a retained STEP graph.  The destination is changed
 * only after the whole payload has passed validation. */
bool DecodeSTEPMetadata(const unsigned char *bytes, size_t byte_count,
	std::map<std::string, std::string> &attributes, std::string &error);

/** Read STEP_METADATA_OBJECT from a BRL-CAD database.  A missing object is
 * not an error and is reported through found. */
bool ReadSTEPMetadata(struct db_i *database,
	std::map<std::string, std::string> &attributes, bool &found,
	std::string &error);

/** True for the reserved locator attributes written on _GLOBAL. */
bool IsSTEPMetadataLocatorAttribute(const std::string &name);

} // namespace step
} // namespace brlcad

#endif /* CONV_STEP_STEPMETADATA_H */
