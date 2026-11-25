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

struct Db5HistoryDraft {
    Db5HistoryDraft() = default;
    Db5HistoryDraft(Db5HistoryDraft &&) = default;
    Db5HistoryDraft &operator=(Db5HistoryDraft &&) = default;

    RT_EXPORT void store(const ArtifactKey &key, Bytes &&payload);
    // Store the revision and its full selected manifest and update head.
    RT_EXPORT bool record(const Manifest &manifest, Revision &revision);

    std::shared_ptr<const uint8_t> backing;
    std::map<ArtifactKey, Bytes> owned;
    Pack pack;
};

// Seed one revision from a plain DB5 model. Use a retained mapped reader
// to borrow the large bodies. Other readers copy each unique body once.
RT_EXPORT bool seed_db5_history(const struct db_i *db, std::string_view label,
			      int64_t time, Db5HistoryDraft &result);

enum class AppendStatus {
    Error,
    Unchanged,
    Ready
};

// Record net DB5 changes against the selected manifest. The returned
// archive owns any new payloads and retains the prior archive's backing.
RT_EXPORT AppendStatus append_db5_history(const EmbeddedPack &archive, struct db_edit *edit,
					int64_t time, Db5HistoryDraft &result);

// Compare the live DB5 model to the selected archived revision. Optionally
// prepare the corresponding archive commit for explicit adoption.
RT_EXPORT AppendStatus stage_db5_adoption(const struct db_i *db, const EmbeddedPack &archive,
				       int64_t time, Changes &changes, Db5HistoryDraft *result = nullptr);

// Stage a complete model checkout, and prepare the archive to select
// it. The caller commits the edit and publishes the two together.
RT_EXPORT bool stage_db5_checkout(const EmbeddedPack &archive, const Id &target,
				struct db_edit *edit, Db5HistoryDraft &result);

// Replay into a private view of the selected model. Supply the live
// base when it has external edits so those can be reversed in the view.
RT_EXPORT bool stage_db5_preview(const EmbeddedPack &archive, const Id &target,
	struct db_edit *edit, const struct db_i *external_base);

// Return an owned archive for Current and ExternalEdits. Opening never
// modifies the database. The remaining statuses leave result untouched.
RT_EXPORT EmbeddedStatus read_db5_history(const struct db_i *db, EmbeddedPack &result);

// Reload the archive on the freshly published handle returned by
// save_db5_history. That publisher has already compared the live model.
RT_EXPORT bool read_published_db5_history(const struct db_i *db, const Id &expected,
					 EmbeddedPack &result);

// These helpers are used by the DB5 publisher and archive opener to
// compare exactly the same model representations.
bool selected_manifest(const Pack &pack, Manifest &manifest);
bool add_model_record(Manifest &manifest, std::string_view name, ByteView record);
RT_EXPORT EmbeddedStatus inspect_db5_history(const struct db_i *db);

// Atomically stream a complete DB5 snapshot and replace or append its
// recognized history archive. Both the pack and its borrowed payloads
// must remain valid through publication.
RT_EXPORT enum db5_save_result save_db5_history(struct db5_save *save,
	const struct db_i *snapshot, const Pack &pack, struct db_i **replacement, struct bu_vls *message);
RT_EXPORT bool db5_history_save_is_current(const struct db5_save *save);

// Publish a new DB5 path, preserving any existing destination. Without
// a supplied pack, omit recognized archives and retain user objects.
RT_EXPORT enum db5_save_result export_db5_snapshot(const struct db_i *snapshot,
	const char *destination, const Pack *pack, struct bu_vls *message);

// A workspace supplies records from the same read transaction as its pack.
// The reader returns an owned external record, as db_get_external does.
using SnapshotRecordReader = std::function<bool(const struct directory *, struct bu_external *)>;
enum db5_save_result export_db5_snapshot(const struct db_i *snapshot, const char *destination,
	const Pack *pack, struct bu_vls *message, const SnapshotRecordReader &reader);

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
