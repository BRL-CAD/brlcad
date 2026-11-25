/*                     D B _ H I S T O R Y . C P P
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

#include <algorithm>
#include <charconv>
#include <cstring>
#include <initializer_list>
#include <iterator>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "bu/datetime.h"
#include "bu/str.h"
#include "bu/vls.h"
#include "ged.h"
#include "ged_private.h"
#include "../librt/db_history_db5.h"
#include "../librt/librt_private.h"

namespace {

constexpr size_t session_budget = 64 * 1024 * 1024;
constexpr size_t label_limit = 128;
constexpr uint64_t no_revision = UINT64_MAX;

using Edit = std::unique_ptr<struct db_edit, decltype(&db_edit_abort)>;
using Snapshot = std::unique_ptr<struct db_i, decltype(&db_close)>;

} // namespace


struct ged_db_history {
    struct Change {
	std::string name;
	std::vector<uint8_t> before;
	std::vector<uint8_t> after;
    };

    struct Revision {
	uint64_t parent = no_revision;
	uint64_t preferred = no_revision;
	size_t children = 0;
	size_t bytes = 0;
	int64_t authored = 0;
	std::string label;
	std::vector<Change> changes;
	db_history::Renames renames;
    };


    struct db_i *live = nullptr;
    struct db_edit *stage = nullptr;
    bool failed = false;
    int original_read_only = 0;
    db_history::EmbeddedStatus embedded_status = db_history::EmbeddedStatus::Missing;
    db_history::EmbeddedPack embedded;
    std::unique_ptr<struct db5_save, decltype(&db5_save_free)> save{nullptr, db5_save_free};
    size_t limit = session_budget;
    size_t used = 0;
    uint64_t head = 0;
    uint64_t next = 1;
    std::map<uint64_t, Revision> revisions{{0, {}}};
};


namespace {

using Change = ged_db_history::Change;
using Revision = ged_db_history::Revision;
using db_history::EmbeddedStatus;
using db_history::id_string;

bool
is_embedded(const ged_db_history &history)
{
    return history.embedded_status != EmbeddedStatus::Missing;
}

const char *
embedded_status_text(EmbeddedStatus status)
{
    switch (status) {
	case EmbeddedStatus::Current: return "current";
	case EmbeddedStatus::ExternalEdits: return "external edits";
	case EmbeddedStatus::Malformed: return "malformed archive";
	case EmbeddedStatus::Unsupported: return "unsupported archive";
	case EmbeddedStatus::Collision: return "reserved name collision";
	case EmbeddedStatus::Missing: return "missing archive";
    }
    return "unknown archive state";
}

int64_t
revision_time()
{
    constexpr int64_t microseconds_per_second = 1000000;
    return bu_gettime() / microseconds_per_second;
}

bool
parse_number(const char *text, uint64_t &value)
{
    const std::string_view input(text);
    const auto parsed = std::from_chars(input.data(), input.data() + input.size(), value);
    return parsed.ec == std::errc{} && parsed.ptr == input.data() + input.size();
}

bool
one_of(std::string_view value, std::initializer_list<std::string_view> options)
{
    for (const auto option : options)
	if (value == option)
	    return true;
    return false;
}

bool
permitted_edit(std::string_view command, int argc, const char **argv)
{
    if (command == "title" || command == "units")
	return argc > 1;
    if (command == "attr")
	return argc > 1 && one_of(argv[1], {"append", "copy", "rm", "set"});
    return one_of(command, {"adjust", "cp", "g", "hide", "in", "kill", "make",
	"mv", "mvall", "put", "unhide"});
}

bool
permitted_query(std::string_view command, int argc, const char **argv)
{
    if (command == "attr")
	return argc <= 1 || one_of(argv[1], {"get", "list", "show", "sort"});
    if (command == "title" || command == "units")
	return argc == 1;
    return one_of(command, {"db", "dbfind", "dbversion", "expand", "find", "form",
	"get", "get_type", "help", "l", "ls", "lt", "make_name", "match",
	"pathlist", "paths", "search", "summary", "tops", "version", "?"});
}

bool
session_lifecycle(std::string_view command)
{
    return one_of(command, {"close", "closedb", "exit", "open", "opendb", "q", "quit", "reopen"});
}

void
refresh_view_units(struct ged *gedp)
{
    if (gedp->ged_gvp) {
	gedp->ged_gvp->gv_base2local = gedp->dbip->dbi_base2local;
	gedp->ged_gvp->gv_local2base = gedp->dbip->dbi_local2base;
    }
}

bool
can_replace_database(struct ged *gedp)
{
    bu_clbk_t callback = nullptr;
    void *data = nullptr;
    if (gedp->new_cmd_forms || gedp->dbi_state ||
	(ged_clbk_get(&callback, &data, gedp, "opendb", BU_CLBK_POST) == BRLCAD_OK && callback)) {
	bu_vls_printf(gedp->ged_result_str, "This editor needs database replacement support to write history");
	return false;
    }
    const auto *display = ged_dl(gedp);
    if ((display && BU_LIST_NON_EMPTY(&display->l)) ||
	!gedp->i->i->edit_buf.empty() || BU_PTBL_LEN(&gedp->ged_subp)) {
	bu_vls_printf(gedp->ged_result_str, "Clear displayed geometry and finish active edits and subprocesses before replacing the database");
	return false;
    }
    return true;
}

std::string
operation_label(const char *command, int argc, const char **argv)
{
    std::string label(command);
    for (int i = 1; i < argc && label.size() < label_limit; i++) {
	const std::string_view arg(argv[i]);
	if (arg.size() >= label_limit - label.size())
	    break;
	label += ' ';
	label += arg;
    }
    return label;
}

struct Collect {
    Revision &revision;
    size_t limit;
};

int
collect_change(const char *name, const struct bu_external *before,
	       const struct bu_external *after, void *data)
{
    auto &collect = *static_cast<Collect *>(data);
    auto &revision = collect.revision;
    const size_t edit_overhead = revision.changes.empty() ? sizeof(Revision) + revision.label.size() : 0;
    const size_t overhead = edit_overhead + sizeof(Change) + strlen(name) + 1;
    const size_t previous = before ? before->ext_nbytes : 0;
    const size_t current = after ? after->ext_nbytes : 0;
    const size_t remaining = collect.limit - revision.bytes;
    if (overhead > remaining || previous > remaining - overhead ||
	current > remaining - overhead - previous)
	return -1;

    Change change;
    change.name = name;
    if (before)
	change.before.assign(before->ext_buf, before->ext_buf + previous);
    if (after)
	change.after.assign(after->ext_buf, after->ext_buf + current);
    revision.bytes += overhead + previous + current;
    revision.changes.push_back(std::move(change));
    return 0;
}

int
collect_rename(const char *before, const char *after, void *data)
{
    auto &collect = *static_cast<Collect *>(data);
    auto &revision = collect.revision;
    const size_t overhead = sizeof(db_history::Renames::value_type);
    const size_t previous = strlen(before) + 1;
    const size_t current = strlen(after) + 1;
    const size_t remaining = collect.limit - revision.bytes;
    if (overhead > remaining || previous > remaining - overhead ||
	current > remaining - overhead - previous)
	return -1;

    if (!revision.renames.emplace(before, after).second)
	return -1;
    revision.bytes += overhead + previous + current;
    return 0;
}

struct bu_external
record_view(const std::vector<uint8_t> &bytes)
{
    struct bu_external record = BU_EXTERNAL_INIT_ZERO;
    record.ext_buf = const_cast<uint8_t *>(bytes.data());
    record.ext_nbytes = bytes.size();
    return record;
}

void
prune(ged_db_history &history)
{
    while (history.used > history.limit) {
	auto candidate = history.revisions.end();
	for (auto it = history.revisions.begin(); it != history.revisions.end(); ++it) {
	    if (it->first != history.head && !it->second.children &&
		it->second.parent != no_revision) {
		candidate = it;
		break;
	    }
	}
	if (candidate != history.revisions.end()) {
	    auto &parent = history.revisions.at(candidate->second.parent);
	    parent.children--;
	    if (parent.preferred == candidate->first)
		parent.preferred = no_revision;
	    history.used -= candidate->second.bytes;
	    history.revisions.erase(candidate);
	    continue;
	}
	// Only the selected ancestry remains. The next revision is now
	// the oldest state that Undo can reach.
	auto root = history.revisions.begin();
	auto child = std::next(root);
	history.used -= child->second.bytes;
	child->second.parent = no_revision;
	child->second.bytes = 0;
	std::vector<Change>().swap(child->second.changes);
	child->second.renames.clear();
	history.revisions.erase(root);
    }
}

bool
can_write_history(struct ged *gedp, const ged_db_history &history, bool adopt_external = false)
{
    if (history.live != gedp->dbip) {
	bu_vls_printf(gedp->ged_result_str, "The open database has changed since history was loaded");
	return false;
    }

    if (is_embedded(history) && history.embedded_status != EmbeddedStatus::Current &&
	!(adopt_external && history.embedded_status == EmbeddedStatus::ExternalEdits)) {
	bu_vls_printf(gedp->ged_result_str, "Embedded history is read-only (%s)",
		embedded_status_text(history.embedded_status));
	return false;
    }
    if (!history.save) {
	bu_vls_printf(gedp->ged_result_str, "History requires a writable database with atomic replacement support");
	return false;
    }
    return can_replace_database(gedp);
}

bool
save_snapshot(struct ged *gedp, ged_db_history &history, const struct db_i *snapshot,
	      struct db_i **replacement, const db_history::Pack *pack = nullptr)
{
    if (!snapshot) {
	bu_vls_printf(gedp->ged_result_str, "\nDatabase edit could not be committed");
	return false;
    }

    struct bu_vls message = BU_VLS_INIT_ZERO;
    const auto status = pack ? db_history::save_db5_history(history.save.get(), snapshot, *pack, replacement, &message) :
	db5_save_replace(history.save.get(), snapshot, replacement, &message);
    if (status != DB5_SAVE_OK)
	bu_vls_printf(gedp->ged_result_str, "\n%s", bu_vls_addr(&message));
    bu_vls_free(&message);
    return status == DB5_SAVE_OK || status == DB5_SAVE_SYNC_FAILED;
}

void
attach_history(ged_db_history &history, struct db_i *db)
{
    // Only transaction views may be written. Preserve access flags for
    // shared handles when this editor eventually closes the database.
    history.live = db;
    history.original_read_only = db->dbi_read_only;
    db->dbi_read_only = 1;
    db->i->dbi_edit_guard++;
}

void
install_snapshot(struct ged *gedp, ged_db_history &history, struct db_i *replacement,
		 const db_history::Id *embedded_head = nullptr)
{
    auto *retired = history.live;
    history.save.reset(db5_save_begin(replacement, nullptr));
    attach_history(history, replacement);
    if (embedded_head) {
	db_history::EmbeddedPack published;
	if (db_history::read_published_db5_history(replacement, *embedded_head, published)) {
	    history.embedded_status = EmbeddedStatus::Current;
	    history.embedded = std::move(published);
	} else {
	    history.embedded_status = EmbeddedStatus::Malformed;
	    history.embedded = {};
	    bu_vls_printf(gedp->ged_result_str, "\nDatabase saved. Reopen it to reload embedded history.");
	}
    }
    gedp->dbip = replacement;
    ged_event_database_set_private(gedp, replacement);
    refresh_view_units(gedp);
    db_close(retired);

    struct ged_event event;
    ged_event_init(&event);
    event.kind = GED_EVENT_BATCH_REBUILD;
    event.redraw = 1;
    if (ged_event_publish(gedp, &event, 1, nullptr) != GED_EVENT_OK)
	bu_vls_printf(gedp->ged_result_str, "\nDatabase saved. Refresh the view to see the change.");
}

struct ReplayStep {
    uint64_t revision;
    bool reverse;
};

int
travel(struct ged *gedp, ged_db_history &history, uint64_t target)
{
    if (target == history.head)
	return BRLCAD_OK;
    if (!can_write_history(gedp, history))
	return BRLCAD_ERROR;

    // Parents have smaller IDs. Walk back from each head to their common
    // ancestor, then apply the target branch in parent-to-child order.
    std::vector<ReplayStep> steps;
    std::vector<uint64_t> forward;
    uint64_t from = history.head;
    uint64_t to = target;
    while (from != to) {
	if (from > to) {
	    steps.push_back({from, true});
	    from = history.revisions.at(from).parent;
	} else {
	    forward.push_back(to);
	    to = history.revisions.at(to).parent;
	}
    }
    for (auto it = forward.rbegin(); it != forward.rend(); ++it)
	steps.push_back({*it, false});

    Edit edit(db_edit_begin(history.live, history.revisions.at(target).label.c_str()), db_edit_abort);
    if (!edit) {
	bu_vls_printf(gedp->ged_result_str, "Could not stage database history");
	return BRLCAD_ERROR;
    }
    for (const auto &step : steps) {
	for (const auto &change : history.revisions.at(step.revision).changes) {
	    auto before = record_view(step.reverse ? change.after : change.before);
	    auto after = record_view(step.reverse ? change.before : change.after);
	    if (db_edit_apply(edit.get(), change.name.c_str(),
		    before.ext_nbytes ? &before : nullptr, after.ext_nbytes ? &after : nullptr) != 0) {
		bu_vls_printf(gedp->ged_result_str, "Current record %s differs from session history", change.name.c_str());
		return BRLCAD_ERROR;
	    }
	}
    }

    Snapshot snapshot(db_edit_commit(edit.get()), db_close);
    if (snapshot)
	edit.release();
    struct db_i *replacement = nullptr;
    if (!save_snapshot(gedp, history, snapshot.get(), &replacement))
	return BRLCAD_ERROR;
    snapshot.reset();
    for (const auto &step : steps) {
	const auto &revision = history.revisions.at(step.revision);
	history.revisions.at(revision.parent).preferred = step.revision;
    }
    history.head = target;
    install_snapshot(gedp, history, replacement);
    return BRLCAD_OK;
}

int
enable_session(struct ged *gedp, size_t limit)
{
    if (gedp->i->db_history) {
	if (!is_embedded(*gedp->i->db_history))
	    return BRLCAD_OK;
	bu_vls_printf(gedp->ged_result_str, "This database already carries embedded history");
	return BRLCAD_ERROR;
    }
    if (!gedp->dbip || gedp->dbip->dbi_read_only) {
	bu_vls_printf(gedp->ged_result_str, "Session history requires a writable DB5 file");
	return BRLCAD_ERROR;
    }
    if (!can_replace_database(gedp))
	return BRLCAD_ERROR;

    auto history = std::make_unique<ged_db_history>();
    history->save.reset(db5_save_begin(gedp->dbip, gedp->ged_result_str));
    if (!history->save)
	return BRLCAD_ERROR;
    history->limit = limit;
    history->revisions.at(0).authored = revision_time();
    history->revisions.at(0).label = "Initial model";
    attach_history(*history, gedp->dbip);
    gedp->i->db_history = history.release();
    return BRLCAD_OK;
}

bool
archive_session_record(db_history::Db5HistoryDraft &draft, const Change &change,
		       const std::vector<uint8_t> &record, db_history::Id &id)
{
    if (record.empty()) {
	id = db_history::absent;
	return true;
    }
    db_history::RecordArtifact artifact;
    if (!db_history::factor_record(record, artifact) || artifact.name != change.name ||
	artifact.name == db_history::archive_name)
	return false;
    id = artifact.object_id;
    if (artifact.body_id != db_history::absent)
	draft.pack.artifacts.emplace(db_history::ArtifactKey{db_history::Kind::Body, artifact.body_id}, artifact.body);
    draft.store({db_history::Kind::Object, id}, std::move(artifact.object));
    return true;
}

bool
apply_session_revision(db_history::Manifest &model, const db_history::Revision &revision, bool reverse)
{
    for (const auto &change : revision.changes) {
	const auto existing = model.find(change.first);
	const auto &before = reverse ? change.second.after : change.second.before;
	const auto &after = reverse ? change.second.before : change.second.after;
	if ((existing == model.end() ? db_history::absent : existing->second) != before)
	    return false;
	if (after != db_history::absent)
	    model.insert_or_assign(change.first, after);
	else if (existing != model.end())
	    model.erase(existing);
    }
    return true;
}

bool
archive_session(const ged_db_history &history, db_history::Db5HistoryDraft &draft)
{
    // The seed describes the current live model. Walk the journal
    // backwards to discover the oldest retained model, then assign
    // persistent IDs in parent-first order, including inactive branches.
    const auto live_model = draft.pack.selected_model;
    db_history::Manifest oldest;
    if (!db_history::read_manifest(draft.pack, live_model, oldest))
	return false;
    const db_history::ArtifactKey seed{db_history::Kind::Revision, draft.pack.selected_revision};
    draft.pack.artifacts.erase(seed);
    draft.owned.erase(seed);

    std::map<uint64_t, db_history::Revision> revisions;
    for (const auto &entry : history.revisions) {
	auto &revision = revisions[entry.first];
	revision.label = entry.second.label;
	revision.time = entry.second.authored;
	revision.renames = entry.second.renames;
	for (const auto &record : entry.second.changes) {
	    db_history::Change change;
	    if (!archive_session_record(draft, record, record.before, change.before) ||
		!archive_session_record(draft, record, record.after, change.after))
		return false;
	    revision.changes.emplace(record.name, change);
	}
    }
    for (auto cursor = history.head; cursor != no_revision; cursor = history.revisions.at(cursor).parent)
	if (!apply_session_revision(oldest, revisions.at(cursor), true))
	    return false;

    std::map<uint64_t, db_history::Id> ids;
    for (const auto &entry : history.revisions) {
	auto &revision = revisions.at(entry.first);
	db_history::Manifest model;
	if (entry.second.parent == no_revision) {
	    model = std::move(oldest);
	} else {
	    revision.parent = ids.at(entry.second.parent);
	    if (!db_history::read_manifest(draft.pack, revisions.at(entry.second.parent).model, model) ||
		!apply_session_revision(model, revision, false))
		return false;
	}
	if (!draft.record(model, revision))
	    return false;
	ids.emplace(entry.first, draft.pack.selected_revision);
    }

    for (const auto &entry : history.revisions)
	if (entry.second.preferred != no_revision)
	    draft.pack.preferences.emplace(ids.at(entry.first), ids.at(entry.second.preferred));
    draft.pack.selected_revision = ids.at(history.head);
    draft.pack.selected_model = revisions.at(history.head).model;
    return draft.pack.selected_model == live_model;
}

bool
prepare_archive(struct ged *gedp, const ged_db_history *journal, db_history::Db5HistoryDraft &draft)
{
    // Borrow the original bodies from a mapped reader. The publisher
    // compares the resulting model against the records it writes.
    Snapshot source(db_open(gedp->dbip->dbi_filename, DB_OPEN_READONLY), db_close);
    if (!source || db_dirbuild(source.get()) != 0 ||
	!db_history::seed_db5_history(source.get(), "Initial model", revision_time(), draft)) {
	bu_vls_printf(gedp->ged_result_str, "Could not build the initial embedded history");
	return false;
    }
    if (journal && !archive_session(*journal, draft)) {
	bu_vls_printf(gedp->ged_result_str, "Could not preserve the retained session revisions");
	return false;
    }
    return true;
}

int
enable_embedded(struct ged *gedp)
{
    auto *history = gedp->i->db_history;
    if (history && is_embedded(*history)) {
	if (history->embedded_status == EmbeddedStatus::Current)
	    return BRLCAD_OK;
	bu_vls_printf(gedp->ged_result_str, "This database already carries %s", embedded_status_text(history->embedded_status));
	return BRLCAD_ERROR;
    }
    if (!gedp->dbip || db_version(gedp->dbip) != 5 || (!history && gedp->dbip->dbi_read_only)) {
	bu_vls_printf(gedp->ged_result_str, "Embedded history requires a writable DB5 file");
	return BRLCAD_ERROR;
    }
    if (db_lookup(gedp->dbip, db_history::archive_name, LOOKUP_QUIET)) {
	bu_vls_printf(gedp->ged_result_str, "A user object occupies the embedded history name");
	return BRLCAD_ERROR;
    }

    std::unique_ptr<ged_db_history> initial;
    if (history) {
	if (!can_write_history(gedp, *history))
	    return BRLCAD_ERROR;
    } else {
	if (!can_replace_database(gedp))
	    return BRLCAD_ERROR;
	initial = std::make_unique<ged_db_history>();
	initial->live = gedp->dbip;
	initial->save.reset(db5_save_begin(gedp->dbip, gedp->ged_result_str));
	if (!initial->save)
	    return BRLCAD_ERROR;
	history = initial.get();
    }

    db_history::Db5HistoryDraft draft;
    if (!prepare_archive(gedp, initial ? nullptr : history, draft))
	return BRLCAD_ERROR;

    struct db_i *replacement = nullptr;
    if (!save_snapshot(gedp, *history, history->live, &replacement, &draft.pack))
	return BRLCAD_ERROR;
    if (initial)
	gedp->i->db_history = initial.release();
    history->revisions.clear();
    install_snapshot(gedp, *history, replacement, &draft.pack.selected_revision);
    return BRLCAD_OK;
}

int
publish_embedded_edit(struct ged *gedp, ged_db_history &history, Edit &edit, const db_history::Pack &pack)
{
    Snapshot snapshot(db_edit_commit(edit.get()), db_close);
    if (snapshot)
	edit.release();
    struct db_i *replacement = nullptr;
    if (!save_snapshot(gedp, history, snapshot.get(), &replacement, &pack))
	return BRLCAD_ERROR;
    snapshot.reset();
    install_snapshot(gedp, history, replacement, &pack.selected_revision);
    return BRLCAD_OK;
}

int
checkout_embedded(struct ged *gedp, ged_db_history &history, const db_history::Id &target)
{
    if (target == history.embedded.pack.selected_revision)
	return BRLCAD_OK;
    Edit edit(db_edit_begin(history.live, "Checkout revision"), db_edit_abort);
    db_history::Db5HistoryDraft draft;
    if (!edit || !db_history::stage_db5_checkout(history.embedded, target, edit.get(), draft)) {
	bu_vls_printf(gedp->ged_result_str, "Could not replay the selected revision against the current model");
	return BRLCAD_ERROR;
    }
    return publish_embedded_edit(gedp, history, edit, draft.pack);
}

void
append_printable(struct bu_vls *out, std::string_view text)
{
    constexpr unsigned char first_printable_ascii = 0x20;
    constexpr unsigned char ascii_delete = 0x7f;
    for (const unsigned char byte : text) {
	if (byte < first_printable_ascii || byte == ascii_delete)
	    bu_vls_printf(out, "\\x%02x", static_cast<unsigned>(byte));
	else if (byte == '\\')
	    bu_vls_strcat(out, "\\\\");
	else
	    bu_vls_putc(out, byte);
    }
}

const char *
change_action(bool before, bool after)
{
    return !before ? "create" : !after ? "delete" : "update";
}

void
print_change(struct bu_vls *out, bool before, bool after, std::string_view name)
{
    bu_vls_printf(out, "\n%s ", change_action(before, after));
    append_printable(out, name);
}

void
print_archive_changes(struct bu_vls *out, const db_history::Changes &changes, const char *object = nullptr)
{
    for (const auto &change : changes)
	if (!object || change.first == object)
	    print_change(out, change.second.before != db_history::absent, change.second.after != db_history::absent, change.first);
}

void
print_renames(struct bu_vls *out, const db_history::Renames &renames)
{
    for (const auto &rename : renames) {
	bu_vls_strcat(out, "\nrename ");
	append_printable(out, rename.first);
	bu_vls_strcat(out, " -> ");
	append_printable(out, rename.second);
    }
}

void
print_archive_revision(struct bu_vls *out, const db_history::Pack &pack, const db_history::Id &id,
		       const db_history::Revision &revision)
{
    if (bu_vls_strlen(out))
	bu_vls_putc(out, '\n');
    bu_vls_printf(out, "revision %s", id_string(id).c_str());
    if (id == pack.selected_revision)
	bu_vls_strcat(out, " (selected)");
    if (revision.parent != db_history::absent && !pack.boundaries.count(id))
	bu_vls_printf(out, " parent %s", id_string(revision.parent).c_str());
    if (!revision.label.empty()) {
	bu_vls_putc(out, ' ');
	append_printable(out, revision.label);
    }
}

template <typename Visitor>
bool
visit_archive_revisions(const db_history::Pack &pack, Visitor visit)
{
    for (const auto &entry : pack.artifacts) {
	if (entry.first.first != db_history::Kind::Revision)
	    continue;
	const auto &id = entry.first.second;
	db_history::Revision revision;
	if (!db_history::read_revision(pack, id, revision))
	    return false;
	visit(id, revision);
    }
    return true;
}

bool
resolve_archive_revision(struct bu_vls *out, const db_history::Pack &pack, std::string argument, db_history::Id &target)
{
    const auto ref = pack.references.find(argument);
    if (ref != pack.references.end()) {
	target = ref->second;
	return true;
    }

    for (char &c : argument) {
	if (c >= 'A' && c <= 'F')
	    c = c - 'A' + 'a';
    }
    bool matched = false;
    if (!argument.empty() && argument.size() <= 2 * target.size()) {
	for (const auto &entry : pack.artifacts) {
	    if (entry.first.first != db_history::Kind::Revision ||
		id_string(entry.first.second).compare(0, argument.size(), argument) != 0)
		continue;
	    if (matched) {
		bu_vls_printf(out, "Ambiguous revision prefix %s", argument.c_str());
		return false;
	    }
	    target = entry.first.second;
	    matched = true;
	}
    }
    if (!matched) {
	bu_vls_strcat(out, "Unknown revision ");
	append_printable(out, argument);
    }
    return matched;
}

void
print_embedded_status(struct ged *gedp, const ged_db_history &history)
{
    auto *out = gedp->ged_result_str;
    bu_vls_printf(out, "mode embedded\nstatus %s\naccess %s", embedded_status_text(history.embedded_status),
	history.save && history.embedded_status == EmbeddedStatus::Current ? "read-write" : "read-only");
    const auto &pack = history.embedded.pack;
    if (pack.selected_revision == db_history::absent)
	return;
    bu_vls_printf(out, "\nrevision %s", id_string(pack.selected_revision).c_str());
    db_history::Revision selected;
    if (db_history::read_revision(pack, pack.selected_revision, selected) &&
	selected.parent != db_history::absent && !pack.boundaries.count(pack.selected_revision)) {
	bu_vls_printf(out, "\nundo %s ", id_string(selected.parent).c_str());
	append_printable(out, selected.label);
    }
    const auto preferred = pack.preferences.find(pack.selected_revision);
    if (!visit_archive_revisions(pack, [&](const db_history::Id &id, const db_history::Revision &revision) {
	if (revision.parent != pack.selected_revision || pack.boundaries.count(id))
	    return;
	bu_vls_printf(out, "\nredo %s ", id_string(id).c_str());
	append_printable(out, revision.label);
	if (preferred != pack.preferences.end() && preferred->second == id)
	    bu_vls_strcat(out, " (preferred)");
    }))
	bu_vls_strcat(out, "\nAn archived revision could not be read");
}

void
print_status(struct ged *gedp, const ged_db_history *history)
{
    if (!history) {
	bu_vls_printf(gedp->ged_result_str, "mode disabled");
	return;
    }
    if (is_embedded(*history)) {
	print_embedded_status(gedp, *history);
	return;
    }
    const auto &head = history->revisions.at(history->head);
    bu_vls_printf(gedp->ged_result_str, "mode session\nrevision %ju\noldest %ju\nbytes %zu\nlimit %zu",
	(uintmax_t)history->head, (uintmax_t)history->revisions.begin()->first,
	history->used, history->limit);
    if (head.parent != no_revision) {
	bu_vls_strcat(gedp->ged_result_str, "\nundo ");
	append_printable(gedp->ged_result_str, head.label);
    }
    for (const auto &entry : history->revisions) {
	if (entry.second.parent == history->head) {
	    bu_vls_printf(gedp->ged_result_str, "\nredo %ju ", (uintmax_t)entry.first);
	    append_printable(gedp->ged_result_str, entry.second.label);
	}
    }
}

const Revision *
find_revision(struct ged *gedp, const ged_db_history *history, const char *argument, uint64_t &id)
{
    if (history && parse_number(argument, id)) {
	const auto entry = history->revisions.find(id);
	if (entry != history->revisions.end())
	    return &entry->second;
    }
    bu_vls_printf(gedp->ged_result_str, "Unknown or expired session revision");
    return nullptr;
}

void
print_revision(struct bu_vls *out, const ged_db_history &history, uint64_t id, const Revision &revision)
{
    if (bu_vls_strlen(out))
	bu_vls_putc(out, '\n');
    bu_vls_printf(out, "revision %ju", (uintmax_t)id);
    if (id == history.head)
	bu_vls_strcat(out, " (selected)");
    if (revision.parent != no_revision)
	bu_vls_printf(out, " parent %ju", (uintmax_t)revision.parent);
    if (!revision.label.empty()) {
	bu_vls_putc(out, ' ');
	append_printable(out, revision.label);
    }
}

int
print_log(struct ged *gedp, const ged_db_history *history, const char *object)
{
    if (!history) {
	bu_vls_printf(gedp->ged_result_str, "Session history is disabled");
	return BRLCAD_ERROR;
    }
    for (auto entry = history->revisions.rbegin(); entry != history->revisions.rend(); ++entry) {
	if (object) {
	    const auto &changes = entry->second.changes;
	    if (std::none_of(changes.begin(), changes.end(),
		    [object](const Change &change) { return change.name == object; }))
		continue;
	}
	print_revision(gedp->ged_result_str, *history, entry->first, entry->second);
    }
    return BRLCAD_OK;
}

void
print_changes(struct ged *gedp, const ged_db_history &history, uint64_t id, const Revision &revision)
{
    auto *out = gedp->ged_result_str;
    print_revision(out, history, id, revision);
    print_renames(out, revision.renames);
    for (const auto &change : revision.changes)
	print_change(out, !change.before.empty(), !change.after.empty(), change.name);
}

bool
archive_readable(struct ged *gedp, const ged_db_history &history)
{
    if (history.embedded.pack.selected_revision != db_history::absent)
	return true;
    bu_vls_printf(gedp->ged_result_str, "Embedded history is unavailable (%s)", embedded_status_text(history.embedded_status));
    return false;
}

int
print_archive_log(struct ged *gedp, const ged_db_history &history, const char *object)
{
    if (!archive_readable(gedp, history))
	return BRLCAD_ERROR;

    const auto &pack = history.embedded.pack;
    auto *out = gedp->ged_result_str;
    std::vector<std::pair<db_history::Id, db_history::Revision>> entries;
    if (!visit_archive_revisions(pack, [&](const db_history::Id &id, const db_history::Revision &revision) {
	if (!object || revision.changes.count(object))
	    entries.emplace_back(id, revision);
    })) {
	bu_vls_strcat(out, "An archived revision could not be read");
	return BRLCAD_ERROR;
    }
    std::sort(entries.begin(), entries.end(), [](const auto &a, const auto &b) {
	return a.second.time != b.second.time ? a.second.time > b.second.time : a.first < b.first;
    });
    for (const auto &entry : entries)
	print_archive_revision(out, pack, entry.first, entry.second);
    if (!object) {
	for (const auto &ref : pack.references) {
	    bu_vls_printf(out, "\nmark %s ", id_string(ref.second).c_str());
	    append_printable(out, ref.first);
	}
    }
    return BRLCAD_OK;
}

int
show_archive_revision(struct ged *gedp, const db_history::Pack &pack, const db_history::Id &id)
{
    db_history::Revision revision;
    auto *out = gedp->ged_result_str;
    if (!db_history::read_revision(pack, id, revision)) {
	bu_vls_strcat(out, "The archived revision could not be read");
	return BRLCAD_ERROR;
    }
    print_archive_revision(out, pack, id, revision);
    print_renames(out, revision.renames);
    print_archive_changes(out, revision.changes);
    return BRLCAD_OK;
}

int
diff_archive_revisions(struct ged *gedp, const db_history::Pack &pack, const char *a, const char *b, const char *object)
{
    auto *out = gedp->ged_result_str;
    db_history::Id first, second;
    if (!resolve_archive_revision(out, pack, a, first) || !resolve_archive_revision(out, pack, b, second))
	return BRLCAD_ERROR;
    db_history::Manifest before, after;
    if (!db_history::manifest_at_revision(pack, first, before) || !db_history::manifest_at_revision(pack, second, after)) {
	bu_vls_strcat(out, "An archived model could not be reconstructed");
	return BRLCAD_ERROR;
    }
    bu_vls_printf(out, "changes %s %s", id_string(first).c_str(), id_string(second).c_str());
    print_archive_changes(out, db_history::diff_manifests(before, after), object);
    return BRLCAD_OK;
}

bool
unchanged_history_file(struct ged *gedp, const ged_db_history &history)
{
    if (history.live == gedp->dbip && (!history.save || db_history::db5_history_save_is_current(history.save.get())))
	return true;
    bu_vls_strcat(gedp->ged_result_str, "The database file changed since opening. Reopen it to check history.");
    return false;
}

int
verify_embedded_history(struct ged *gedp, ged_db_history &history)
{
    if (!unchanged_history_file(gedp, history))
	return BRLCAD_ERROR;

    auto *out = gedp->ged_result_str;
    db_history::EmbeddedPack checked;
    const auto state = db_history::read_db5_history(history.live, checked);
    if (state != EmbeddedStatus::Current && state != EmbeddedStatus::ExternalEdits) {
	bu_vls_printf(out, "Could not read embedded history (%s)", embedded_status_text(state));
	return BRLCAD_ERROR;
    }
    if (!db_history::verify_pack(checked.pack, out)) {
	history.embedded_status = EmbeddedStatus::Malformed;
	return BRLCAD_ERROR;
    }
    if (!unchanged_history_file(gedp, history))
	return BRLCAD_ERROR;

    history.embedded_status = state;
    if (state == EmbeddedStatus::ExternalEdits) {
	bu_vls_strcat(out, "Archive verified. The live model has external edits.");
	return BRLCAD_ERROR;
    }
    bu_vls_strcat(out, "Archive verified. The live model matches the selected revision.");
    return BRLCAD_OK;
}

int
adopt_external_history(struct ged *gedp, ged_db_history &history, bool apply)
{
    if (!unchanged_history_file(gedp, history) || !archive_readable(gedp, history))
	return BRLCAD_ERROR;

    auto *out = gedp->ged_result_str;
    if (history.embedded_status == EmbeddedStatus::Current) {
	bu_vls_strcat(out, "There are no external edits to adopt");
	return BRLCAD_OK;
    }
    if (history.embedded_status != EmbeddedStatus::ExternalEdits) {
	bu_vls_printf(out, "External edits cannot be adopted (%s)", embedded_status_text(history.embedded_status));
	return BRLCAD_ERROR;
    }
    if (apply && !can_write_history(gedp, history, true))
	return BRLCAD_ERROR;
    if (!db_history::verify_pack(history.embedded.pack, out)) {
	history.embedded_status = EmbeddedStatus::Malformed;
	return BRLCAD_ERROR;
    }

    db_history::Db5HistoryDraft draft;
    db_history::Changes changes;
    const auto prepared = db_history::stage_db5_adoption(history.live, history.embedded,
	    revision_time(), changes, apply ? &draft : nullptr);
    if (prepared == db_history::AppendStatus::Error) {
	bu_vls_strcat(out, "Could not compare the live DB5 model to the archived revision");
	return BRLCAD_ERROR;
    }
    if (!unchanged_history_file(gedp, history))
	return BRLCAD_ERROR;
    if (prepared == db_history::AppendStatus::Unchanged) {
	bu_vls_strcat(out, "There are no external edits to adopt");
	return BRLCAD_OK;
    }
    if (!apply) {
	bu_vls_printf(out, "External DB5 edits against %s", id_string(history.embedded.pack.selected_revision).c_str());
	print_archive_changes(out, changes);
	return BRLCAD_OK;
    }

    struct db_i *replacement = nullptr;
    if (!save_snapshot(gedp, history, history.live, &replacement, &draft.pack))
	return BRLCAD_ERROR;
    if (bu_vls_strlen(out))
	bu_vls_putc(out, '\n');
    bu_vls_printf(out, "Adopted external DB5 edits in revision %s", id_string(draft.pack.selected_revision).c_str());
    print_archive_changes(out, changes);
    install_snapshot(gedp, history, replacement, &draft.pack.selected_revision);
    return BRLCAD_OK;
}

int
mark_archive_revision(struct ged *gedp, ged_db_history &history, const char *name, bool move)
{
    if (!can_write_history(gedp, history))
	return BRLCAD_ERROR;
    const auto &current = history.embedded.pack;
    const auto found = current.references.find(name);
    if (!*name || (found != current.references.end() && !move)) {
	bu_vls_strcat(gedp->ged_result_str, *name ? "Mark already exists. Use --move to retarget it." : "A mark name is required");
	return BRLCAD_ERROR;
    }
    if (found != current.references.end() && found->second == current.selected_revision)
	return BRLCAD_OK;
    auto marked = current;
    marked.references.insert_or_assign(name, marked.selected_revision);
    struct db_i *replacement = nullptr;
    if (!save_snapshot(gedp, history, history.live, &replacement, &marked))
	return BRLCAD_ERROR;
    install_snapshot(gedp, history, replacement, &marked.selected_revision);
    return BRLCAD_OK;
}

int
export_history(struct ged *gedp, const ged_db_history *history, const char *path, bool include_archive)
{
    auto *out = gedp->ged_result_str;
    const db_history::Pack *pack = nullptr;
    db_history::Db5HistoryDraft draft;
    if (include_archive) {
	if (!history) {
	    bu_vls_strcat(out, "The open database does not have history to export");
	    return BRLCAD_ERROR;
	}
	if (is_embedded(*history)) {
	    if (history->embedded_status != EmbeddedStatus::Current) {
		bu_vls_printf(out, "The archive cannot be included (%s)", embedded_status_text(history->embedded_status));
		return BRLCAD_ERROR;
	    }
	    pack = &history->embedded.pack;
	} else {
	    if (!prepare_archive(gedp, history, draft))
		return BRLCAD_ERROR;
	    pack = &draft.pack;
	}
    }
    const auto status = db_history::export_db5_snapshot(gedp->dbip, path, pack, out);
    if (status == DB5_SAVE_OK) {
	bu_vls_strcat(out, "Exported ");
	append_printable(out, path);
    }
    return status == DB5_SAVE_OK || status == DB5_SAVE_SYNC_FAILED ? BRLCAD_OK : BRLCAD_ERROR;
}

int
step_archive_revision(struct ged *gedp, ged_db_history &history, bool undo, const char *argument)
{
    if (!can_write_history(gedp, history))
	return BRLCAD_ERROR;
    const auto &pack = history.embedded.pack;
    auto *out = gedp->ged_result_str;
    db_history::Id target = db_history::absent;
    if (undo || argument) {
	if (argument && !resolve_archive_revision(out, pack, argument, target))
	    return BRLCAD_ERROR;
	db_history::Revision revision;
	if (!db_history::read_revision(pack, undo ? pack.selected_revision : target, revision)) {
	    bu_vls_strcat(out, "The archived revision could not be read");
	    return BRLCAD_ERROR;
	}
	if (undo && !pack.boundaries.count(pack.selected_revision))
	    target = revision.parent;
	if (!undo && (revision.parent != pack.selected_revision || pack.boundaries.count(target))) {
	    bu_vls_strcat(out, "Redo revision must be a child of the current revision");
	    return BRLCAD_ERROR;
	}
    } else {
	const auto preferred = pack.preferences.find(pack.selected_revision);
	size_t children = 0;
	if (!visit_archive_revisions(pack, [&](const db_history::Id &id, const db_history::Revision &revision) {
	    if (revision.parent != pack.selected_revision || pack.boundaries.count(id))
		return;
	    children++;
	    if (children == 1 || (preferred != pack.preferences.end() && preferred->second == id))
		target = id;
	})) {
	    bu_vls_strcat(out, "An archived revision could not be read");
	    return BRLCAD_ERROR;
	}
	if (children > 1 && (preferred == pack.preferences.end() || preferred->second != target)) {
	    print_embedded_status(gedp, history);
	    bu_vls_strcat(out, "\nSelect a revision with db redo <revision>");
	    return BRLCAD_ERROR;
	}
    }
    if (target == db_history::absent) {
	bu_vls_printf(out, "No retained revision to %s", undo ? "undo" : "redo");
	return BRLCAD_ERROR;
    }
    return checkout_embedded(gedp, history, target);
}

} // namespace


void
ged_db_history_reset(struct ged *gedp)
{
    if (!gedp->i)
	return;
    auto *history = gedp->i->db_history;
    if (!history)
	return;
    if (history->live == gedp->dbip) {
	if (--history->live->i->dbi_edit_guard == 0)
	    history->live->dbi_read_only = history->original_read_only;
    }
    delete history;
    gedp->i->db_history = nullptr;
}

void
ged_db_history_open(struct ged *gedp)
{
    if (!gedp->dbip || gedp->i->db_history || db_version(gedp->dbip) != 5 ||
	!db_lookup(gedp->dbip, db_history::archive_name, LOOKUP_QUIET))
	return;

    auto history = std::make_unique<ged_db_history>();
    if (!gedp->dbip->dbi_read_only)
	history->save.reset(db5_save_begin(gedp->dbip, nullptr));
    history->embedded_status = db_history::read_db5_history(gedp->dbip, history->embedded);
    if (history->embedded_status == EmbeddedStatus::Missing || history->embedded_status == EmbeddedStatus::Collision)
	return;
    attach_history(*history, gedp->dbip);
    gedp->i->db_history = history.release();
}

int
ged_db_history_enter(struct ged *gedp, const char *command, int argc, const char **argv,
		     struct db_edit **edit)
{
    auto *history = gedp->i->db_history;
    if (!history)
	return BRLCAD_OK;

    const bool edits = permitted_edit(command, argc, argv);
    if (permitted_query(command, argc, argv) || (!history->stage && session_lifecycle(command)))
	return BRLCAD_OK;
    if (!edits) {
	bu_vls_printf(gedp->ged_result_str, "%s is unavailable while database history is enabled", command);
	return BRLCAD_ERROR;
    }
    if (history->stage)
	return BRLCAD_OK;
    if (!can_write_history(gedp, *history))
	return BRLCAD_ERROR;

    const auto label = operation_label(command, argc, argv);
    auto *stage = db_edit_begin(history->live, label.c_str());
    if (!stage) {
	bu_vls_printf(gedp->ged_result_str, "Could not stage database edit");
	return BRLCAD_ERROR;
    }
    *edit = stage;
    history->stage = stage;
    history->failed = false;
    ged_event_stage_private(gedp, 1);
    gedp->dbip = db_edit_view(stage);
    return BRLCAD_OK;
}

int
ged_db_history_leave(struct ged *gedp, struct db_edit *stage, int result)
{
    auto *history = gedp->i->db_history;
    if (!stage) {
	if (history && history->stage && result != BRLCAD_OK)
	    history->failed = true;
	return result;
    }

    Edit edit(stage, db_edit_abort);
    history->stage = nullptr;
    gedp->dbip = history->live;
    ged_event_stage_private(gedp, 0);
    refresh_view_units(gedp);
    if (result != BRLCAD_OK)
	return result;
    if (history->failed) {
	bu_vls_printf(gedp->ged_result_str, "\nA nested operation failed");
	return BRLCAD_ERROR;
    }

    if (is_embedded(*history)) {
	db_history::Db5HistoryDraft draft;
	switch (db_history::append_db5_history(history->embedded, stage, revision_time(), draft)) {
	    case db_history::AppendStatus::Unchanged:
		return BRLCAD_OK;
	    case db_history::AppendStatus::Ready:
		return publish_embedded_edit(gedp, *history, edit, draft.pack);
	    case db_history::AppendStatus::Error:
		bu_vls_strcat(gedp->ged_result_str, "\nCould not record the staged changes in embedded history");
		return BRLCAD_ERROR;
	}
    }

    Revision revision;
    revision.parent = history->head;
    revision.authored = revision_time();
    revision.label = db_edit_label(stage);
    Collect collect{revision, history->limit};
    if (db_edit_visit(stage, collect_change, &collect) != 0 ||
	db_edit_visit_renames(stage, collect_rename, &collect) != 0) {
	bu_vls_printf(gedp->ged_result_str, "\nDatabase edit failed or exceeds the session history budget");
	return BRLCAD_ERROR;
    }
    if (revision.changes.empty())
	return BRLCAD_OK;

    // Allocate the journal entry before installing the file. After the
    // replacement succeeds, advancing the head requires no allocation.
    std::map<uint64_t, Revision> pending;
    pending.emplace(history->next, std::move(revision));
    Snapshot snapshot(db_edit_commit(stage), db_close);
    if (snapshot)
	edit.release();
    struct db_i *replacement = nullptr;
    if (!save_snapshot(gedp, *history, snapshot.get(), &replacement))
	return BRLCAD_ERROR;
    snapshot.reset();
    history->revisions.at(history->head).children++;
    history->head = history->next++;
    history->used += pending.begin()->second.bytes;
    history->revisions.insert(pending.extract(pending.begin()));
    prune(*history);
    install_snapshot(gedp, *history, replacement);
    return BRLCAD_OK;
}


int
ged_db_history_command(struct ged *gedp, int argc, const char **argv)
{
    auto *history = gedp->i->db_history;
    const std::string_view command(argv[0]);
    bu_vls_trunc(gedp->ged_result_str, 0);
    if (history && history->stage) {
	bu_vls_printf(gedp->ged_result_str, "History commands require a completed edit");
	return BRLCAD_ERROR;
    }
    if (command == "history") {
	if (argc == 2 && BU_STR_EQUAL(argv[1], "status")) {
	    print_status(gedp, history);
	    return BRLCAD_OK;
	}
	const bool verify = argc == 2 && BU_STR_EQUAL(argv[1], "verify");
	const bool adopt = (argc == 2 || (argc == 3 && BU_STR_EQUAL(argv[2], "--apply"))) && BU_STR_EQUAL(argv[1], "adopt");
	if (verify || adopt) {
	    if (!history || !is_embedded(*history)) {
		bu_vls_strcat(gedp->ged_result_str, "This command requires embedded history");
		return BRLCAD_ERROR;
	    }
	    return verify ? verify_embedded_history(gedp, *history) : adopt_external_history(gedp, *history, argc == 3);
	}
	if ((argc == 3 || argc == 4) && BU_STR_EQUAL(argv[1], "export")) {
	    if (argc == 3 && !BU_STR_EQUAL(argv[2], "--with-history"))
		return export_history(gedp, history, argv[2], false);
	    if (argc == 4 && BU_STR_EQUAL(argv[2], "--with-history") && !BU_STR_EQUAL(argv[3], "--with-history"))
		return export_history(gedp, history, argv[3], true);
	    if (argc == 4 && BU_STR_EQUAL(argv[3], "--with-history") && !BU_STR_EQUAL(argv[2], "--with-history"))
		return export_history(gedp, history, argv[2], true);
	}
	if ((argc == 2 || (argc == 3 && BU_STR_EQUAL(argv[2], "embedded"))) && BU_STR_EQUAL(argv[1], "enable")) {
	    const int enabled = enable_embedded(gedp);
	    if (enabled == BRLCAD_OK) {
		if (bu_vls_strlen(gedp->ged_result_str))
		    bu_vls_putc(gedp->ged_result_str, '\n');
		print_status(gedp, gedp->i->db_history);
	    }
	    return enabled;
	}
	if ((argc == 3 || argc == 4) && BU_STR_EQUAL(argv[1], "enable") && BU_STR_EQUAL(argv[2], "session")) {
	    uint64_t limit = session_budget;
	    if (argc == 4 && (!parse_number(argv[3], limit) || !limit || limit > SIZE_MAX / 2)) {
		bu_vls_printf(gedp->ged_result_str, "Expected a positive session history byte limit");
		return BRLCAD_ERROR;
	    }
	    const int enabled = enable_session(gedp, static_cast<size_t>(limit));
	    if (enabled == BRLCAD_OK)
		print_status(gedp, gedp->i->db_history);
	    return enabled;
	}
	if ((argc == 2 || (argc == 4 && BU_STR_EQUAL(argv[2], "--object"))) && BU_STR_EQUAL(argv[1], "log")) {
	    const auto *object = argc == 4 ? argv[3] : nullptr;
	    return history && is_embedded(*history) ? print_archive_log(gedp, *history, object) :
		print_log(gedp, history, object);
	}
	if (argc == 3 && one_of(argv[1], {"show", "checkout"})) {
	    const bool inspect = BU_STR_EQUAL(argv[1], "show");
	    if (history && is_embedded(*history)) {
		db_history::Id id;
		if (!archive_readable(gedp, *history) ||
		    !resolve_archive_revision(gedp->ged_result_str, history->embedded.pack, argv[2], id))
		    return BRLCAD_ERROR;
		if (inspect)
		    return show_archive_revision(gedp, history->embedded.pack, id);
		return can_write_history(gedp, *history) ? checkout_embedded(gedp, *history, id) : BRLCAD_ERROR;
	    }
	    uint64_t target;
	    const auto *revision = find_revision(gedp, history, argv[2], target);
	    if (!revision)
		return BRLCAD_ERROR;
	    if (inspect) {
		print_changes(gedp, *history, target, *revision);
		return BRLCAD_OK;
	    }
	    return travel(gedp, *history, target);
	}
	const bool diff = (argc == 4 || (argc == 6 && BU_STR_EQUAL(argv[4], "--object"))) && BU_STR_EQUAL(argv[1], "diff");
	const bool mark = (argc == 3 || (argc == 4 && BU_STR_EQUAL(argv[3], "--move"))) && BU_STR_EQUAL(argv[1], "mark");
	if (diff || mark) {
	    if (!history || !is_embedded(*history)) {
		bu_vls_strcat(gedp->ged_result_str, "This command requires embedded history");
		return BRLCAD_ERROR;
	    }
	    if (mark)
		return mark_archive_revision(gedp, *history, argv[2], argc == 4);
	    if (!archive_readable(gedp, *history))
		return BRLCAD_ERROR;
	    return diff_archive_revisions(gedp, history->embedded.pack, argv[2], argv[3], argc == 6 ? argv[5] : nullptr);
	}
	bu_vls_strcat(gedp->ged_result_str, "Usage: db history " GED_DB_HISTORY_USAGE);
	return GED_HELP;
    }
    const bool undo = command == "undo";
    if ((undo && argc != 1) || (!undo && argc > 2)) {
	bu_vls_printf(gedp->ged_result_str, "Usage: db undo | db redo [revision]");
	return GED_HELP;
    }
    if (!history) {
	bu_vls_printf(gedp->ged_result_str, "Session history is disabled");
	return BRLCAD_ERROR;
    }
    if (is_embedded(*history))
	return step_archive_revision(gedp, *history, undo, argc == 2 ? argv[1] : nullptr);

    const auto &revision = history->revisions.at(history->head);
    uint64_t target = undo ? revision.parent : revision.preferred;
    if (!undo && argc == 2) {
	const auto child = parse_number(argv[1], target) ? history->revisions.find(target) : history->revisions.end();
	if (child == history->revisions.end() || child->second.parent != history->head) {
	    bu_vls_printf(gedp->ged_result_str, "Redo revision must be a child of the current revision");
	    return BRLCAD_ERROR;
	}
    } else if (!undo && target == no_revision && revision.children) {
	if (revision.children > 1) {
	    print_status(gedp, history);
	    bu_vls_printf(gedp->ged_result_str, "\nSelect a revision with db redo <revision>");
	    return BRLCAD_ERROR;
	}
	for (const auto &entry : history->revisions)
	    if (entry.second.parent == history->head)
		target = entry.first;
    }
    if (target == no_revision) {
	bu_vls_printf(gedp->ged_result_str, "No retained revision to %s", argv[0]);
	return BRLCAD_ERROR;
    }
    return travel(gedp, *history, target);
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
