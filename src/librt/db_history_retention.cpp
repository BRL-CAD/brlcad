/*          D B _ H I S T O R Y _ R E T E N T I O N . C P P
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
#include "common.h"

#include <limits>
#include <map>
#include <set>

#include "bu/vls.h"
#include "db_history_retention.h"

namespace db_history {
namespace {

bool
retention_error(struct bu_vls *message, const char *reason)
{
    if (message)
	bu_vls_strcpy(message, reason);
    return false;
}

using Revisions = std::map<Id, Revision>;

bool
select_revisions(const Pack &source, const WorkspaceRetention &policy, const std::set<Id> &pins,
		 Revisions &revisions, std::map<Id, size_t> &retain, struct bu_vls *message)
{
    std::set<Id> tips;
    for (const auto &entry : source.artifacts) {
	if (entry.first.first != Kind::Revision)
	    continue;
	const auto &id = entry.first.second;
	if (!read_revision(source, id, revisions[id]))
	    return retention_error(message, "A workspace revision could not be read");
	tips.insert(id);
    }
    for (const auto &entry : revisions) {
	const auto &parent = entry.second.parent;
	if (parent != absent && !revisions.count(parent) && !source.boundaries.count(entry.first))
	    return retention_error(message, "A workspace revision has a missing parent");
	tips.erase(parent);
    }

    std::set<Id> anchors = pins;
    anchors.insert(source.selected_revision);
    for (const auto &reference : source.references)
	anchors.insert(reference.second);
    for (const auto &tip : policy.drop) {
	if (!tips.count(tip))
	    return retention_error(message, "Only a branch tip can be dropped");
	if (anchors.count(tip))
	    return retention_error(message, "The selected, marked or reader-pinned revision cannot be dropped");
	tips.erase(tip);
    }
    anchors.insert(tips.begin(), tips.end());

    // Each anchor keeps a bounded parent path. Reuse paths already
    // walked with an equal or greater number of remaining revisions.
    for (const auto &anchor : anchors) {
	Id id = anchor;
	for (size_t remaining = policy.keep; remaining && id != absent; --remaining) {
	    const auto revision = revisions.find(id);
	    if (revision == revisions.end())
		return retention_error(message, "A retained workspace revision is missing");
	    auto included = retain.try_emplace(id, 0).first;
	    if (included->second >= remaining)
		break;
	    included->second = remaining;
	    if (source.boundaries.count(id))
		break;
	    id = revision->second.parent;
	}
    }
    return true;
}

bool
keep_artifact(const Pack &source, Pack &retained, const ArtifactKey &key)
{
    const auto item = source.artifacts.find(key);
    if (item == source.artifacts.end())
	return false;
    retained.artifacts.emplace(*item);
    return true;
}

bool
keep_object(const Pack &source, Pack &retained, const Id &id)
{
    if (id == absent || retained.artifacts.count({Kind::Object, id}))
	return true;
    Object object;
    ByteView descriptor;
    return read_artifact(source, {Kind::Object, id}, descriptor) && decode_object(descriptor, object) &&
	keep_artifact(source, retained, {Kind::Object, id}) &&
	(object.body_id == absent || keep_artifact(source, retained, {Kind::Body, object.body_id}));
}

bool
keep_model(const Pack &source, Db5HistoryDraft &draft, const Id &revision, const Id &model, bool boundary)
{
    auto &retained = draft.pack;
    const ArtifactKey key{Kind::Manifest, model};
    if (retained.artifacts.count(key))
	return true;

    Manifest snapshot;
    if (source.artifacts.count(key)) {
	if (!read_manifest(source, model, snapshot) || !keep_artifact(source, retained, key))
	    return false;
    } else if (boundary) {
	Bytes encoded;
	if (!manifest_at_revision(source, revision, snapshot) || !encode_manifest(snapshot, encoded) ||
	    identify(Kind::Manifest, encoded) != model)
	    return false;
	draft.store(key, std::move(encoded));
    } else {
	return true;
    }

    for (const auto &entry : snapshot)
	if (!keep_object(source, retained, entry.second))
	    return false;
    return true;
}

Id
preview_token(const WorkspaceHead &head, const std::vector<ArtifactKey> &expired, const std::set<Id> &boundaries)
{
    constexpr std::string_view domain = "workspace-retention-v1";
    Bytes encoded(domain.begin(), domain.end());
    auto generation = head.generation;
    for (size_t byte = 0; byte < sizeof(generation); ++byte) {
	encoded.push_back(static_cast<uint8_t>(generation));
	generation >>= 8;
    }
    encoded.insert(encoded.end(), head.revision.begin(), head.revision.end());
    for (const auto &key : expired) {
	encoded.push_back(static_cast<uint8_t>(key.first));
	encoded.insert(encoded.end(), key.second.begin(), key.second.end());
    }
    constexpr uint8_t boundary_tag = 0;
    for (const auto &id : boundaries) {
	encoded.push_back(boundary_tag);
	encoded.insert(encoded.end(), id.begin(), id.end());
    }
    return identify(Kind::Revision, encoded);
}

} // namespace

bool
plan_retention(const Pack &source, const WorkspaceHead &head, const WorkspaceRetention &policy,
	       const std::set<Id> &pins, RetainedWorkspace &result, struct bu_vls *message)
{
    if (!policy.keep)
	return retention_error(message, "Keep at least one revision from each retained anchor");

    Revisions revisions;
    std::map<Id, size_t> retain;
    if (!select_revisions(source, policy, pins, revisions, retain, message))
	return false;

    RetainedWorkspace planned;
    auto &archive = planned.archive.pack;
    archive.selected_revision = source.selected_revision;
    archive.selected_model = source.selected_model;
    archive.references = source.references;
    for (const auto &entry : source.preferences)
	if (retain.count(entry.first) && retain.count(entry.second))
	    archive.preferences.insert(entry);

    for (const auto &entry : retain) {
	const auto &id = entry.first;
	const auto &revision = revisions.at(id);
	const bool boundary = revision.parent != absent && !retain.count(revision.parent);
	if (boundary)
	    archive.boundaries.insert(id);
	if (!keep_artifact(source, archive, {Kind::Revision, id}) ||
	    !keep_model(source, planned.archive, id, revision.model, boundary))
	    return retention_error(message, "A retained workspace model could not be reconstructed");
	for (const auto &change : revision.changes)
	    if (!keep_object(source, archive, change.second.before) || !keep_object(source, archive, change.second.after))
		return retention_error(message, "A retained workspace change is missing an object");
    }
    if (!verify_pack(archive, message))
	return false;

    auto &preview = planned.preview;
    preview.boundaries = archive.boundaries;
    preview.retained_revisions = retain.size();
    for (const auto &entry : source.artifacts) {
	if (archive.artifacts.count(entry.first))
	    continue;
	planned.expired.push_back(entry.first);
	if (entry.first.first == Kind::Revision)
	    preview.expired_revisions.insert(entry.first.second);
	if (entry.second.size > std::numeric_limits<uint64_t>::max() - preview.expired_bytes)
	    return retention_error(message, "The expired workspace payload size is too large");
	preview.expired_bytes += entry.second.size;
    }
    preview.expired_artifacts = planned.expired.size();
    preview.token = preview_token(head, planned.expired, archive.boundaries);
    result = std::move(planned);
    return true;
}

} // namespace db_history

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
