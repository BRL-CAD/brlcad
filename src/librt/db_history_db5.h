/*                   D B _ H I S T O R Y _ D B 5 . H
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
#ifndef LIBRT_DB_HISTORY_DB5_H
#define LIBRT_DB_HISTORY_DB5_H

#include <memory>

#include "rt/db_io.h"
#include "db_history_codec.h"

namespace db_history {

enum class EmbeddedStatus {
    Missing,
    Collision,
    Malformed,
    Unsupported,
    ExternalEdits,
    Current
};

// Keep the source record alive while its pack payloads are borrowed.
struct EmbeddedPack {
    std::shared_ptr<const uint8_t> backing;
    Pack pack;
    Manifest model;
};

struct Db5HistorySeed {
    Db5HistorySeed() = default;
    Db5HistorySeed(Db5HistorySeed &&) = default;
    Db5HistorySeed &operator=(Db5HistorySeed &&) = default;

    std::shared_ptr<struct db_i> backing;
    std::map<ArtifactKey, Bytes> owned;
    Pack pack;
};

// Seed one revision from a plain DB5 model. Use a retained mapped reader
// to borrow the large bodies. Other readers copy each unique body once.
RT_EXPORT bool seed_db5_history(const struct db_i *db, std::string_view label,
			      int64_t time, Db5HistorySeed &result);

// Return an owned archive for Current and ExternalEdits. Opening never
// modifies the database. The remaining statuses leave result untouched.
RT_EXPORT EmbeddedStatus read_db5_history(const struct db_i *db, EmbeddedPack &result);

// These helpers are used by the DB5 publisher and archive opener to
// compare exactly the same model representations.
bool selected_manifest(const Pack &pack, Manifest &manifest);
bool add_model_record(Manifest &manifest, std::string_view name, ByteView record);
EmbeddedStatus inspect_db5_history(const struct db_i *db);

// Atomically stream a complete DB5 snapshot and replace or append its
// recognized history archive. Both the pack and its borrowed payloads
// must remain valid through publication.
RT_EXPORT enum db5_save_result save_db5_history(struct db5_save *save,
	const struct db_i *snapshot, const Pack &pack, struct db_i **replacement, struct bu_vls *message);

} // namespace db_history

#endif /* LIBRT_DB_HISTORY_DB5_H */

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
