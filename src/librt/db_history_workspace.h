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

#include "rt/db_io.h"
#include "db_history_codec.h"

struct db_i;

namespace db_history {

struct Db5HistoryDraft;

struct WorkspaceHead {
    Id revision = absent;
    Id model = absent;
    uint64_t generation = 0;

    bool operator==(const WorkspaceHead &other) const
    {
	return generation == other.generation && revision == other.revision && model == other.model;
    }
};

struct WorkspaceExport {
    Id revision = absent;
    int64_t timestamp = 0;
};

struct RevisionLookup {
    enum class Failure { None, Unknown, Ambiguous };
    std::string_view selector;
    Id id = absent;
    Revision revision{};
    bool boundary = false;
    Failure failure = Failure::None;
};

struct WorkspaceRetention {
    size_t keep = 0;
    std::set<Id> drop;
};

struct WorkspacePrunePreview {
    Id token = absent;
    std::set<Id> expired_revisions;
    std::set<Id> boundaries;
    size_t retained_revisions = 0;
    size_t expired_artifacts = 0;
    uint64_t expired_bytes = 0;
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
    // On success, published receives the head as committed at the destination.
    static RT_EXPORT bool create(const char *path, const Pack &pack, struct bu_vls *message,
	WorkspaceHead *published = nullptr);
    static RT_EXPORT std::unique_ptr<Workspace> open(const char *path, struct bu_vls *message, bool writable = false);
    // Open an ordinary librt database view pinned to one workspace head.
    // The view owns its workspace reader until db_close.
    static RT_EXPORT struct db_i *open_snapshot(const char *path, WorkspaceHead &head, struct bu_vls *message);
    // Open a retained revision without selecting it in the workspace.
    static RT_EXPORT struct db_i *open_revision(const char *path, const Id &revision,
	WorkspaceHead &head, struct bu_vls *message);
    // The manifest belongs to the pinned database view.
    static RT_EXPORT const Manifest *snapshot_model(const struct db_i *db, WorkspaceHead *head = nullptr);
    RT_EXPORT ~Workspace();
    Workspace(const Workspace &) = delete;
    Workspace &operator=(const Workspace &) = delete;

    // Pinned readers can still use their open store after its path changes.
    RT_EXPORT bool location_current(struct bu_vls *message) const;
    // Read the head and selected directory from one committed version.
    RT_EXPORT bool read_directory(WorkspaceHead &head, Manifest &model, struct bu_vls *message) const;
    // Absence is returned as a zero object ID. Records and artifacts are
    // copied out before closing the read transaction.
    RT_EXPORT bool lookup(std::string_view name, Id &id, struct bu_vls *message) const;
    RT_EXPORT bool fetch_artifact(const ArtifactKey &key, Bytes &payload, struct bu_vls *message) const;
    RT_EXPORT bool fetch_record(const Id &id, Bytes &record, struct bu_vls *message) const;
    bool read_record(const Id &id, void *record, size_t size) const;
    // Read graph metadata and owned revision payloads for inspection.
    // The result omits model snapshots, descriptors and bodies.
    RT_EXPORT bool read_graph(const WorkspaceHead &view, WorkspaceHead &current,
	Db5HistoryDraft &result, struct bu_vls *message) const;
    // Read the selected revision and its Undo and Redo metadata together
    // with the live head. Indexed workspaces read only the selected and
    // child revision payloads, plus their boundary and preference data.
    RT_EXPORT bool read_navigation(const WorkspaceHead &view, WorkspaceHead &current,
	Db5HistoryDraft &result, struct bu_vls *message, WorkspaceExport *last_export = nullptr) const;
    // Resolve checkpoints and ID prefixes together in one read transaction.
    // An unknown or ambiguous selector is marked on its request.
    RT_EXPORT bool read_revisions(RevisionLookup *requests, size_t count, struct bu_vls *message) const;
    RT_EXPORT bool read_model_at(const Id &revision, Manifest &model, struct bu_vls *message) const;
    RT_EXPORT bool copy_to(const WorkspaceHead &view, const char *destination, struct bu_vls *message,
	WorkspaceHead *published = nullptr) const;
    RT_EXPORT enum db5_save_result export_snapshot(const struct db_i *view, const char *destination,
	bool with_history, struct bu_vls *message) const;
    RT_EXPORT enum db5_save_result export_staged(const WorkspaceHead &base, const Pack &staged,
	const char *destination, struct bu_vls *message) const;
    RT_EXPORT bool verify(struct bu_vls *message) const;
    RT_EXPORT bool record_export(const WorkspaceExport &exported, struct bu_vls *message);
    RT_EXPORT bool preview_prune(const WorkspaceHead &expected, const WorkspaceRetention &policy,
	WorkspacePrunePreview &preview, struct bu_vls *message) const;
    RT_EXPORT WorkspaceWriteStatus prune(const WorkspaceHead &expected, const WorkspaceRetention &policy,
	std::string_view token, WorkspacePrunePreview &preview, WorkspaceHead &actual, struct bu_vls *message);

    // The caller keeps its staged edit on failure. On success or a stale
    // base, actual receives the authoritative head from the transaction.
    // Only new object descriptors and bodies need to be provided.
    RT_EXPORT WorkspaceWriteStatus commit(const WorkspaceHead &expected, const Revision &revision,
	const ArtifactIndex &artifacts, WorkspaceHead &actual, struct bu_vls *message);
    RT_EXPORT WorkspaceWriteStatus checkout(const WorkspaceHead &expected, const Id &revision,
	WorkspaceHead &actual, struct bu_vls *message);
    RT_EXPORT WorkspaceWriteStatus mark(const WorkspaceHead &expected, std::string_view name, bool move,
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
