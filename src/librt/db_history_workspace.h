/*             D B _ H I S T O R Y _ W O R K S P A C E . H
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
#ifndef LIBRT_DB_HISTORY_WORKSPACE_H
#define LIBRT_DB_HISTORY_WORKSPACE_H

#include <memory>

#include "db_history_codec.h"

struct db_i;

namespace db_history {

struct WorkspaceHead {
    Id revision = absent;
    Id model = absent;
    uint64_t generation = 0;
};

enum class WorkspaceWriteStatus {
    Ok,
    Stale,
    Error
};

class Workspace {
public:
    // The destination must be a new directory. Publish the bootstrap,
    // history and selected directory together in a durable transaction.
    static RT_EXPORT bool create(const char *path, const Pack &pack, struct bu_vls *message);
    static RT_EXPORT std::unique_ptr<Workspace> open(const char *path, struct bu_vls *message, bool writable = false);
    // Open an ordinary librt database view pinned to one workspace head.
    // The view owns its workspace reader until db_close.
    static RT_EXPORT struct db_i *open_snapshot(const char *path, WorkspaceHead &head, struct bu_vls *message);
    RT_EXPORT ~Workspace();
    Workspace(const Workspace &) = delete;
    Workspace &operator=(const Workspace &) = delete;

    // Read the head and selected directory from one committed version.
    RT_EXPORT bool read_directory(WorkspaceHead &head, Manifest &model, struct bu_vls *message) const;
    // Absence is returned as a zero object ID. Records and artifacts are
    // copied out before closing the read transaction.
    RT_EXPORT bool lookup(std::string_view name, Id &id, struct bu_vls *message) const;
    RT_EXPORT bool fetch_artifact(const ArtifactKey &key, Bytes &payload, struct bu_vls *message) const;
    RT_EXPORT bool fetch_record(const Id &id, Bytes &record, struct bu_vls *message) const;
    bool read_record(const Id &id, void *record, size_t size) const;

    // The caller keeps its staged edit on failure. On success or a stale
    // base, actual receives the authoritative head from the transaction.
    // Only new object descriptors and bodies need to be provided.
    RT_EXPORT WorkspaceWriteStatus commit(const WorkspaceHead &expected, const Revision &revision,
	const ArtifactIndex &artifacts, WorkspaceHead &actual, struct bu_vls *message);
    RT_EXPORT WorkspaceWriteStatus checkout(const WorkspaceHead &expected, const Id &revision,
	WorkspaceHead &actual, struct bu_vls *message);

private:
    struct Store;
    struct Registry;
    static Registry &registry();
    Workspace(Store *store, bool writable);
    Store *store;
    bool writable;
};

} // namespace db_history

#endif /* LIBRT_DB_HISTORY_WORKSPACE_H */

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
