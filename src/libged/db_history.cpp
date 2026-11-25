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
#include <climits>
#include <cstring>
#include <initializer_list>
#include <iterator>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include "bu/datetime.h"
#include "bu/file.h"
#include "bu/malloc.h"
#include "bu/str.h"
#include "bu/vls.h"
#include "ged.h"
#include "ged/history.h"
#include "ged_private.h"
#include "npush/npush_options.h"
#include "../libbu/json.hpp"
#include "../librt/db_history_db5.h"
#include "../librt/db_history_workspace.h"
#include "../librt/librt_private.h"

namespace {

constexpr size_t session_budget = 64 * 1024 * 1024;
constexpr size_t label_limit = 128;
constexpr uint64_t no_revision = UINT64_MAX;
constexpr int history_json_version = 1;

using Edit = std::unique_ptr<struct db_edit, decltype(&db_edit_abort)>;
using Snapshot = std::unique_ptr<struct db_i, decltype(&db_close)>;
using Json = nlohmann::json;

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

    struct SessionConflict {
	Snapshot snapshot;
	Revision revision;
    };

    using Conflict = std::variant<db_history::Db5HistoryDraft, SessionConflict>;

    struct db_i *live = nullptr;
    struct db_edit *stage = nullptr;
    bool failed = false;
    int original_read_only = 0;
    db_history::EmbeddedStatus embedded_status = db_history::EmbeddedStatus::Missing;
    db_history::EmbeddedPack embedded;
    std::unique_ptr<db_history::Workspace> workspace;
    db_history::WorkspaceHead workspace_head;
    bool workspace_writable = false;
    db_history::WorkspaceExport local_export;
    std::unique_ptr<Conflict> conflict;
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
using db_history::Workspace;
using db_history::WorkspaceHead;
using db_history::WorkspaceWriteStatus;
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
    if (command == "bo")
	return argc > 1 && std::string_view(argv[1]) == "-i";
    return one_of(command, {"adjust", "arced", "c", "comb", "comb_std", "concat", "copyeval", "cp", "dbconcat", "g", "group",
	"hide", "in", "kill", "killall", "killrefs", "killtree", "make", "move", "move_all", "mv", "mvall",
	"npush", "put", "r", "regdef", "region", "rm", "unhide"});
}

bool
permitted_query(std::string_view command, int argc, const char **argv)
{
    if (command == "npush") {
	if (argc <= 1)
	    return true;
	// Options are rearranged by the parser. Leave the dispatched argv intact.
	std::vector<const char *> args(argv + 1, argv + argc);
	npush_options options;
	return bu_opt_parse(nullptr, args.size(), args.data(), options.descriptors) >= 0 &&
	    (options.help || options.dry_run);
    }
    if (command == "attr")
	return argc <= 1 || one_of(argv[1], {"get", "list", "show", "sort"});
    if (one_of(command, {"arced", "bo", "c", "comb_std", "copyeval", "move", "mv", "r", "regdef", "region", "title", "units"}))
	return argc == 1;
    if (command == "killall" || command == "killrefs")
	return argc == 1 || std::string_view(argv[1]) == "-n";
    if (one_of(command, {"killtree", "move_all", "mvall"})) {
	// Like bu_getopt, inspect only short options before the first operand.
	for (int i = 1; i < argc; ++i) {
	    const std::string_view option(argv[i]);
	    if (option.size() < 2 || option.front() != '-' || option[1] == '-')
		break;
	    if (option.find('n', 1) != std::string_view::npos)
		return true;
	}
	return argc == 1;
    }
    return one_of(command, {"db", "db_glob", "dbfind", "dbversion", "expand", "find", "form",
	"get", "get_type", "glob", "help", "l", "ls", "lt", "make_name", "match",
	"pathlist", "paths", "search", "summary", "tops", "version", "?"});
}

bool
permitted_standalone(std::string_view command)
{
    return one_of(command, {"draw", "e", "edit", "erase", "ev", "keep", "redraw", "setview",
	"view", "view2", "view_func", "who", "zap"});
}

bool
session_lifecycle(std::string_view command)
{
    return one_of(command, {"close", "closedb", "exit", "open", "opendb", "q", "quit", "reopen"});
}

void
refresh_view_units(struct ged *gedp)
{
    struct bu_ptbl *views = bv_set_views(&gedp->ged_views);
    for (size_t i = 0; i < BU_PTBL_LEN(views); ++i) {
	auto *view = reinterpret_cast<struct bview *>(BU_PTBL_GET(views, i));
	view->gv_base2local = gedp->dbip->dbi_base2local;
	view->gv_local2base = gedp->dbip->dbi_local2base;
    }
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
    if (!gedp->i->history_replaced &&
	ged_clbk_get(&callback, &data, gedp, "opendb", BU_CLBK_POST) == BRLCAD_OK && callback) {
	bu_vls_printf(gedp->ged_result_str, "This editor needs database replacement support to write history");
	return false;
    }
    const auto *display = ged_dl(gedp);
    if (display && BU_LIST_NON_EMPTY(&display->l) && !gedp->i->history_replaced) {
	bu_vls_strcat(gedp->ged_result_str, "Clear legacy displayed geometry before replacing the database");
	return false;
    }
    if (!gedp->i->i->edit_buf.empty() || BU_PTBL_LEN(&gedp->ged_subp)) {
	bu_vls_strcat(gedp->ged_result_str, "Finish active edits and subprocesses before replacing the database");
	return false;
    }
    return !gedp->i->history_prepare ||
	gedp->i->history_prepare(gedp, gedp->i->history_handler_data) == BRLCAD_OK;
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
require_db5_history(struct ged *gedp)
{
    const int version = db_version(gedp->dbip);
    if (version == 5)
	return true;

    bu_vls_strcat(gedp->ged_result_str, "Database history requires a DB5 model");
    if (version == 4)
	bu_vls_strcat(gedp->ged_result_str, "\nRun dbupgrade <input.g> <new.g> and open the new file");
    return false;
}

bool
has_pending_conflict(const ged_db_history &history, struct bu_vls *out)
{
    if (!history.conflict)
	return false;
    bu_vls_strcat(out, "Use db history conflict to inspect or export the staged edit, then reopen the database");
    return true;
}

bool
can_write_history(struct ged *gedp, const ged_db_history &history, bool adopt_external = false)
{
    if (history.live != gedp->dbip) {
	bu_vls_printf(gedp->ged_result_str, "The open database has changed since history was loaded");
	return false;
    }
    if (has_pending_conflict(history, gedp->ged_result_str))
	return false;
    if (history.workspace) {
	if (!history.workspace_writable) {
	    bu_vls_strcat(gedp->ged_result_str, "The workspace is open read-only");
	    return false;
	}
	return can_replace_database(gedp);
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

enum db5_save_result
save_snapshot(struct ged *gedp, ged_db_history &history, const struct db_i *snapshot,
	      struct db_i **replacement, const db_history::Pack *pack = nullptr)
{
    if (!snapshot) {
	bu_vls_printf(gedp->ged_result_str, "\nDatabase edit could not be committed");
	return DB5_SAVE_ERROR;
    }

    struct bu_vls message = BU_VLS_INIT_ZERO;
    const auto status = pack ? db_history::save_db5_history(history.save.get(), snapshot, *pack, replacement, &message) :
	db5_save_replace(history.save.get(), snapshot, replacement, &message);
    if (status != DB5_SAVE_OK)
	bu_vls_printf(gedp->ged_result_str, "\n%s", bu_vls_addr(&message));
    bu_vls_free(&message);
    return status;
}

bool
saved_database(enum db5_save_result result)
{
    return result == DB5_SAVE_OK || result == DB5_SAVE_SYNC_FAILED;
}

void
retain_conflict(struct ged *gedp, ged_db_history &history, ged_db_history::Conflict &&conflict)
{
    history.conflict = std::make_unique<ged_db_history::Conflict>(std::move(conflict));
    bu_vls_strcat(gedp->ged_result_str,
	"\nInspect with db history conflict or save to a new file with db history conflict export <path>");
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
copy_region_defaults(struct db_i *destination, struct db_i *source)
{
    auto *target = wdb_dbopen(destination, RT_WDB_TYPE_DB_DEFAULT);
    const auto *defaults = wdb_dbopen(source, RT_WDB_TYPE_DB_DEFAULT);
    target->wdb_item_default = defaults->wdb_item_default;
    target->wdb_air_default = defaults->wdb_air_default;
    target->wdb_los_default = defaults->wdb_los_default;
    target->wdb_mat_default = defaults->wdb_mat_default;
}

struct db_edit *
begin_editor_edit(struct db_i *base, const char *label)
{
    auto *edit = db_edit_begin(base, label);
    if (edit)
	copy_region_defaults(db_edit_view(edit), base);
    return edit;
}

void
switch_database(struct ged *gedp, struct db_i *replacement)
{
    auto *retired = gedp->dbip;
    // Region defaults are editor settings. Carry them across history travel.
    copy_region_defaults(replacement, retired);
    ged_edit_sessions_database_close_private(gedp);
    gedp->dbip = replacement;
    ged_event_database_set_private(gedp, replacement);
    refresh_view_units(gedp);

    struct ged_event event;
    ged_event_init(&event);
    event.kind = GED_EVENT_BATCH_REBUILD;
    event.redraw = 1;
    if (ged_event_publish(gedp, &event, 1, nullptr) != GED_EVENT_OK)
	bu_vls_printf(gedp->ged_result_str, "\nDatabase saved. Refresh the view to see the change.");
    ged_dbi_state_rebind_private(gedp);
    if (gedp->i->history_replaced)
	gedp->i->history_replaced(gedp, retired, gedp->i->history_handler_data);
    db_close(retired);
}

void
install_snapshot(struct ged *gedp, ged_db_history &history, struct db_i *replacement,
		 const db_history::Id *embedded_head = nullptr)
{
    if (!history.workspace)
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
    switch_database(gedp, replacement);
}

std::unique_ptr<ged_db_history>
workspace_history(struct db_i *db, bool read_only, struct bu_vls *message)
{
    WorkspaceHead selected;
    if (!Workspace::snapshot_model(db, &selected))
	return nullptr;

    auto history = std::make_unique<ged_db_history>();
    if (!read_only) {
	history->workspace = Workspace::open(db->dbi_filename, nullptr, true);
	history->workspace_writable = static_cast<bool>(history->workspace);
    }
    if (!history->workspace)
	history->workspace = Workspace::open(db->dbi_filename, message);
    if (!history->workspace)
	return nullptr;
    history->workspace_head = selected;
    return history;
}

void
reload_workspace(struct ged *gedp, ged_db_history &history, const WorkspaceHead &committed)
{
    WorkspaceHead selected;
    struct bu_vls message = BU_VLS_INIT_ZERO;
    auto *replacement = Workspace::open_snapshot(history.live->dbi_filename, selected, &message);
    if (replacement) {
	history.workspace_head = selected;
	--history.live->i->dbi_edit_guard;
	install_snapshot(gedp, history, replacement);
	if (selected.generation != committed.generation)
	    bu_vls_strcat(gedp->ged_result_str, "\nAnother writer advanced the workspace after publication");
    } else {
	bu_vls_printf(gedp->ged_result_str, "\nWorkspace saved. Reopen it to refresh the view (%s).", bu_vls_addr(&message));
    }
    bu_vls_free(&message);
}

struct ReplayStep {
    uint64_t revision;
    bool reverse;
};

std::vector<ReplayStep>
session_replay(const ged_db_history &history, uint64_t from, uint64_t to)
{
    // Parents have smaller IDs. Walk back from each head to their common
    // ancestor, then apply the target branch in parent-to-child order.
    std::vector<ReplayStep> steps;
    std::vector<uint64_t> forward;
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
    return steps;
}

Snapshot
session_snapshot(const ged_db_history &history, uint64_t target, std::vector<ReplayStep> &steps,
		  struct bu_vls *out)
{
    steps = session_replay(history, history.head, target);
    Edit edit(db_edit_begin(history.live, history.revisions.at(target).label.c_str()), db_edit_abort);
    if (!edit) {
	if (out)
	    bu_vls_strcat(out, "Could not stage database history");
	return Snapshot(nullptr, db_close);
    }
    for (const auto &step : steps) {
	for (const auto &change : history.revisions.at(step.revision).changes) {
	    auto before = record_view(step.reverse ? change.after : change.before);
	    auto after = record_view(step.reverse ? change.before : change.after);
	    if (db_edit_apply(edit.get(), change.name.c_str(),
		    before.ext_nbytes ? &before : nullptr, after.ext_nbytes ? &after : nullptr) != 0) {
		if (out)
		    bu_vls_printf(out, "Current record %s differs from session history", change.name.c_str());
		return Snapshot(nullptr, db_close);
	    }
	}
    }

    Snapshot snapshot(db_edit_commit(edit.get()), db_close);
    if (snapshot)
	edit.release();
    else if (out)
	bu_vls_strcat(out, "Could not finalize the session revision");
    return snapshot;
}

int
travel(struct ged *gedp, ged_db_history &history, uint64_t target)
{
    if (target == history.head)
	return BRLCAD_OK;
    if (!can_write_history(gedp, history))
	return BRLCAD_ERROR;

    std::vector<ReplayStep> steps;
    auto snapshot = session_snapshot(history, target, steps, gedp->ged_result_str);
    if (!snapshot)
	return BRLCAD_ERROR;
    struct db_i *replacement = nullptr;
    // Keep the requested view if another writer won publication.
    const auto result = save_snapshot(gedp, history, snapshot.get(), &replacement);
    if (!saved_database(result)) {
	if (result == DB5_SAVE_STALE)
	    retain_conflict(gedp, history, ged_db_history::SessionConflict{std::move(snapshot), history.revisions.at(target)});
	return BRLCAD_ERROR;
    }
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
	if (gedp->i->db_history->workspace) {
	    bu_vls_strcat(gedp->ged_result_str, "This database already carries workspace history");
	    return BRLCAD_ERROR;
	}
	if (!is_embedded(*gedp->i->db_history))
	    return BRLCAD_OK;
	bu_vls_printf(gedp->ged_result_str, "This database already carries embedded history");
	return BRLCAD_ERROR;
    }
    if (!require_db5_history(gedp))
	return BRLCAD_ERROR;
    if (gedp->dbip->dbi_read_only) {
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
valid_session_journal(const ged_db_history &history)
{
    const auto &revisions = history.revisions;
    if (revisions.empty() || !revisions.count(history.head) || revisions.rbegin()->first >= history.next)
	return false;

    std::map<uint64_t, size_t> children;
    size_t bytes = 0;
    for (const auto &entry : revisions) {
	const auto id = entry.first;
	const auto &revision = entry.second;
	if (id == revisions.begin()->first) {
	    if (revision.parent != no_revision || revision.bytes || !revision.changes.empty() || !revision.renames.empty())
		return false;
	} else {
	    if (revision.parent >= id || !revisions.count(revision.parent) || revision.changes.empty())
		return false;
	    ++children[revision.parent];
	}
	if (revision.preferred != no_revision) {
	    const auto preferred = revisions.find(revision.preferred);
	    if (preferred == revisions.end() || preferred->second.parent != id)
		return false;
	}
	if (revision.bytes > history.limit - bytes)
	    return false;
	bytes += revision.bytes;
    }

    if (bytes != history.used)
	return false;
    for (const auto &entry : revisions) {
	const auto count = children.find(entry.first);
	if (entry.second.children != (count == children.end() ? 0 : count->second))
	    return false;
    }
    return true;
}

bool
archive_session(const ged_db_history &history, db_history::Db5HistoryDraft &draft)
{
    if (!valid_session_journal(history))
	return false;

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

    struct PromotedRevision {
	db_history::Id id;
	size_t distance;
    };
    std::map<uint64_t, PromotedRevision> ids;
    uint64_t previous = no_revision;
    db_history::Manifest previous_model;
    for (const auto &entry : history.revisions) {
	auto &revision = revisions.at(entry.first);
	db_history::Manifest model;
	size_t distance = 0;
	if (entry.second.parent == no_revision) {
	    model = std::move(oldest);
	} else {
	    const auto &parent = ids.at(entry.second.parent);
	    revision.parent = parent.id;
	    distance = parent.distance + 1;
	    if (entry.second.parent == previous)
		model = std::move(previous_model);
	    else if (!db_history::manifest_at_revision(draft.pack, revision.parent, model))
		return false;
	    if (!apply_session_revision(model, revision, false))
		return false;
	}
	const bool checkpoint = entry.first == history.head || !distance || distance >= db_history::max_manifest_distance;
	if (!draft.record(model, revision, checkpoint))
	    return false;
	if (checkpoint)
	    distance = 0;
	ids.emplace(entry.first, PromotedRevision{draft.pack.selected_revision, distance});
	previous = entry.first;
	previous_model = std::move(model);
    }

    for (const auto &entry : history.revisions)
	if (entry.second.preferred != no_revision)
	    draft.pack.preferences.emplace(ids.at(entry.first).id, ids.at(entry.second.preferred).id);
    draft.pack.selected_revision = ids.at(history.head).id;
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
	bu_vls_printf(gedp->ged_result_str, "Could not build the initial DB5 history");
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
    if (history && history->workspace) {
	bu_vls_strcat(gedp->ged_result_str, "This database already carries workspace history");
	return BRLCAD_ERROR;
    }
    if (history && is_embedded(*history)) {
	if (history->embedded_status == EmbeddedStatus::Current)
	    return BRLCAD_OK;
	bu_vls_printf(gedp->ged_result_str, "This database already carries %s", embedded_status_text(history->embedded_status));
	return BRLCAD_ERROR;
    }
    if (!require_db5_history(gedp))
	return BRLCAD_ERROR;
    if (!history && gedp->dbip->dbi_read_only) {
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
    if (!saved_database(save_snapshot(gedp, *history, history->live, &replacement, &draft.pack)))
	return BRLCAD_ERROR;
    if (initial)
	gedp->i->db_history = initial.release();
    history->revisions.clear();
    install_snapshot(gedp, *history, replacement, &draft.pack.selected_revision);
    return BRLCAD_OK;
}

int
create_workspace(struct ged *gedp, const char *path)
{
    auto *out = gedp->ged_result_str;
    if (!require_db5_history(gedp))
	return BRLCAD_ERROR;
    auto *history = gedp->i->db_history;
    if (history && history->workspace && bu_file_same(path, gedp->dbip->dbi_filename))
	return BRLCAD_OK;
    if (history && has_pending_conflict(*history, out))
	return BRLCAD_ERROR;
    if (!can_replace_database(gedp))
	return BRLCAD_ERROR;

    if (history && history->workspace) {
	if (!history->workspace->copy_to(history->workspace_head, path, out))
	    return BRLCAD_ERROR;
    } else {
	db_history::Db5HistoryDraft draft;
	const db_history::Pack *pack = &draft.pack;
	if (history && is_embedded(*history)) {
	    if (history->embedded_status != EmbeddedStatus::Current ||
		(history->save && !db_history::db5_history_save_is_current(history->save.get()))) {
		bu_vls_strcat(out, "This DB5 archive must match the open model before workspace creation");
		return BRLCAD_ERROR;
	    }
	    pack = &history->embedded.pack;
	} else if (!prepare_archive(gedp, history, draft)) {
	    return BRLCAD_ERROR;
	}
	if (!Workspace::create(path, *pack, out))
	    return BRLCAD_ERROR;
    }

    WorkspaceHead selected;
    Snapshot replacement(Workspace::open_snapshot(path, selected, out), db_close);
    auto next = workspace_history(replacement.get(), false, out);
    if (!next) {
	bu_vls_printf(out, "\nWorkspace created at %s. Reopen it to switch databases.", path);
	return BRLCAD_ERROR;
    }
    ged_db_history_reset(gedp);
    attach_history(*next, replacement.get());
    gedp->i->db_history = next.release();
    switch_database(gedp, replacement.release());
    return BRLCAD_OK;
}

int
publish_workspace_edit(struct ged *gedp, ged_db_history &history, Edit &edit)
{
    auto *out = gedp->ged_result_str;
    const auto *model = Workspace::snapshot_model(history.live);
    if (!model) {
	bu_vls_strcat(out, "\nThe workspace database view is unavailable");
	return BRLCAD_ERROR;
    }
    db_history::EmbeddedPack base;
    base.model = *model;
    base.pack.selected_revision = history.workspace_head.revision;
    base.pack.selected_model = history.workspace_head.model;
    db_history::Db5HistoryDraft draft;
    const auto status = db_history::append_db5_history(base, edit.get(), revision_time(), draft);
    if (status == db_history::AppendStatus::Unchanged) {
	copy_region_defaults(history.live, db_edit_view(edit.get()));
	return BRLCAD_OK;
    }
    db_history::Revision revision;
    if (status != db_history::AppendStatus::Ready ||
	!db_history::read_revision(draft.pack, draft.pack.selected_revision, revision)) {
	bu_vls_strcat(out, "\nCould not record the staged workspace edit");
	return BRLCAD_ERROR;
    }

    WorkspaceHead committed;
    struct bu_vls message = BU_VLS_INIT_ZERO;
    const auto write = history.workspace->commit(history.workspace_head, revision, draft.pack.artifacts, committed, &message);
    if (write != WorkspaceWriteStatus::Ok)
	bu_vls_printf(out, "\n%s", bu_vls_addr(&message));
    bu_vls_free(&message);
    if (write == WorkspaceWriteStatus::Stale)
	retain_conflict(gedp, history, std::move(draft));
    if (write != WorkspaceWriteStatus::Ok)
	return BRLCAD_ERROR;
    copy_region_defaults(history.live, db_edit_view(edit.get()));
    edit.reset();
    reload_workspace(gedp, history, committed);
    return BRLCAD_OK;
}

int
publish_embedded_edit(struct ged *gedp, ged_db_history &history, Edit &edit, db_history::Db5HistoryDraft &draft)
{
    Snapshot snapshot(db_edit_commit(edit.get()), db_close);
    if (snapshot)
	edit.release();
    struct db_i *replacement = nullptr;
    const auto &pack = draft.pack;
    const auto result = save_snapshot(gedp, history, snapshot.get(), &replacement, &pack);
    if (!saved_database(result)) {
	if (result == DB5_SAVE_STALE)
	    retain_conflict(gedp, history, std::move(draft));
	return BRLCAD_ERROR;
    }
    copy_region_defaults(history.live, snapshot.get());
    snapshot.reset();
    install_snapshot(gedp, history, replacement, &pack.selected_revision);
    return BRLCAD_OK;
}

int
checkout_embedded(struct ged *gedp, ged_db_history &history, const db_history::Id &target)
{
    if (target == history.embedded.pack.selected_revision)
	return BRLCAD_OK;
    Edit edit(begin_editor_edit(history.live, "Checkout revision"), db_edit_abort);
    db_history::Db5HistoryDraft draft;
    if (!edit || !db_history::stage_db5_checkout(history.embedded, target, edit.get(), draft)) {
	bu_vls_printf(gedp->ged_result_str, "Could not replay the selected revision against the current model");
	return BRLCAD_ERROR;
    }
    return publish_embedded_edit(gedp, history, edit, draft);
}

int
checkout_history(struct ged *gedp, ged_db_history &history, const db_history::Id &target)
{
    if (!can_write_history(gedp, history))
	return BRLCAD_ERROR;
    if (!history.workspace)
	return checkout_embedded(gedp, history, target);
    WorkspaceHead selected;
    if (history.workspace->checkout(history.workspace_head, target, selected,
	gedp->ged_result_str) != WorkspaceWriteStatus::Ok)
	return BRLCAD_ERROR;
    if (selected.generation != history.workspace_head.generation)
	reload_workspace(gedp, history, selected);
    return BRLCAD_OK;
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

enum ged_history_change_type
change_type(bool before, bool after)
{
    return !before ? GED_HISTORY_CREATE : !after ? GED_HISTORY_DELETE : GED_HISTORY_UPDATE;
}

const char *
change_action(enum ged_history_change_type type)
{
    switch (type) {
	case GED_HISTORY_CREATE: return "create";
	case GED_HISTORY_DELETE: return "delete";
	case GED_HISTORY_UPDATE: return "update";
	case GED_HISTORY_RENAME: return "rename";
    }
    return "";
}

void
print_change(struct bu_vls *out, bool before, bool after, std::string_view name)
{
    bu_vls_printf(out, "\n%s ", change_action(change_type(before, after)));
    append_printable(out, name);
}

void
print_archive_changes(struct bu_vls *out, const db_history::Changes &changes, const char *object = nullptr)
{
    for (const auto &change : changes)
	if (!object || change.first == object)
	    print_change(out, change.second.before != db_history::absent, change.second.after != db_history::absent, change.first);
}

int
print_revision(const struct ged_history_revision *revision, void *data)
{
    auto *out = static_cast<struct bu_vls *>(data);
    if (bu_vls_strlen(out))
	bu_vls_putc(out, '\n');
    bu_vls_printf(out, "revision %s", revision->id);
    if (revision->selected)
	bu_vls_strcat(out, " (selected)");
    if (revision->retention_boundary)
	bu_vls_strcat(out, " (retention boundary)");
    else if (revision->parent)
	bu_vls_printf(out, " parent %s", revision->parent);
    if (revision->label[0]) {
	bu_vls_putc(out, ' ');
	append_printable(out, revision->label);
    }
    return BRLCAD_OK;
}

int
print_checkpoint(const char *name, const char *id, void *data)
{
    auto *out = static_cast<struct bu_vls *>(data);
    bu_vls_printf(out, "\nmark %s ", id);
    append_printable(out, name);
    return BRLCAD_OK;
}

int
print_revision_change(const struct ged_history_change *change, void *data)
{
    auto *out = static_cast<struct bu_vls *>(data);
    bu_vls_printf(out, "\n%s ", change_action(change->type));
    append_printable(out, change->name);
    if (change->new_name) {
	bu_vls_strcat(out, " -> ");
	append_printable(out, change->new_name);
    }
    return BRLCAD_OK;
}

Json
history_json_string(const char *value)
{
    return value ? Json(value) : Json(nullptr);
}

void
append_history_json(struct bu_vls *out, const Json &result)
{
    bu_vls_strcat(out, result.dump(-1, ' ', false, Json::error_handler_t::replace).c_str());
}

Json
revision_json(const struct ged_history_revision &revision)
{
    return {{"id", revision.id}, {"parent", history_json_string(revision.parent)},
	{"label", revision.label}, {"authored", revision.authored}, {"selected", revision.selected != 0},
	{"retention_boundary", revision.retention_boundary != 0}};
}

int
history_log(struct ged *gedp, int argc, const char **argv)
{
    auto *out = gedp->ged_result_str;
    const char *object = nullptr;
    bool json = false;
    for (int i = 0; i < argc; ++i) {
	if (BU_STR_EQUAL(argv[i], "--json") && !json) {
	    json = true;
	} else if (BU_STR_EQUAL(argv[i], "--object") && !object && i + 1 < argc) {
	    object = argv[++i];
	} else {
	    bu_vls_strcat(out, "Usage: db history log [--object name] [--json]");
	    return BRLCAD_ERROR;
	}
    }

    if (!json)
	return ged_history_visit(gedp, object, print_revision, print_checkpoint, out, out);

    Json output = {{"schema", history_json_version}, {"revisions", Json::array()}, {"checkpoints", Json::array()}};
    const int status = ged_history_visit(gedp, object,
	[](const struct ged_history_revision *revision, void *data) {
	    (*static_cast<Json *>(data))["revisions"].push_back(revision_json(*revision));
	    return BRLCAD_OK;
	}, [](const char *name, const char *revision, void *data) {
	    (*static_cast<Json *>(data))["checkpoints"].push_back({{"name", name}, {"revision", revision}});
	    return BRLCAD_OK;
	}, &output, out);
    if (status == BRLCAD_OK)
	append_history_json(out, output);
    return status;
}

int
history_show(struct ged *gedp, const char *id, bool json)
{
    auto *out = gedp->ged_result_str;
    if (!json)
	return ged_history_visit_revision(gedp, id, print_revision, print_revision_change, out, out);

    Json output = {{"schema", history_json_version}, {"revision", nullptr}, {"changes", Json::array()}};
    const int status = ged_history_visit_revision(gedp, id,
	[](const struct ged_history_revision *revision, void *data) {
	    (*static_cast<Json *>(data))["revision"] = revision_json(*revision);
	    return BRLCAD_OK;
	}, [](const struct ged_history_change *change, void *data) {
	    (*static_cast<Json *>(data))["changes"].push_back({{"type", change_action(change->type)},
		{"name", change->name}, {"new_name", history_json_string(change->new_name)}});
	    return BRLCAD_OK;
	}, &output, out);
    if (status == BRLCAD_OK)
	append_history_json(out, output);
    return status;
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
		if (out)
		    bu_vls_printf(out, "Ambiguous revision prefix %s", argument.c_str());
		return false;
	    }
	    target = entry.first.second;
	    matched = true;
	}
    }
    if (!matched && out) {
	bu_vls_strcat(out, "Unknown revision ");
	append_printable(out, argument);
    }
    return matched;
}

const db_history::Pack *
history_graph(struct bu_vls *out, const ged_db_history &history, db_history::Db5HistoryDraft &draft)
{
    if (history.workspace) {
	WorkspaceHead observed;
	if (!history.workspace->read_graph(history.workspace_head, observed, draft, out))
	    return nullptr;
	return &draft.pack;
    }
    if (history.embedded.pack.selected_revision != db_history::absent)
	return &history.embedded.pack;
    if (out)
	bu_vls_printf(out, "Embedded history is unavailable (%s)", embedded_status_text(history.embedded_status));
    return nullptr;
}

int
prune_workspace(struct ged *gedp, ged_db_history *history, int argc, const char **argv)
{
    auto *out = gedp->ged_result_str;
    if (!history || !history->workspace) {
	bu_vls_strcat(out, "Pruning requires a workspace");
	return BRLCAD_ERROR;
    }
    db_history::WorkspaceRetention policy;
    const char *approval = nullptr;
    std::vector<const char *> drop;
    for (int i = 2; i < argc; i += 2) {
	if (i + 1 == argc)
	    return GED_HELP;
	const std::string_view flag(argv[i]);
	if (flag == "--keep" && !policy.keep) {
	    uint64_t keep = 0;
	    if (!parse_number(argv[i + 1], keep) || !keep || keep > SIZE_MAX) {
		bu_vls_strcat(out, "Expected a positive number of revisions to keep");
		return BRLCAD_ERROR;
	    }
	    policy.keep = static_cast<size_t>(keep);
	} else if (flag == "--drop") {
	    drop.push_back(argv[i + 1]);
	} else if (flag == "--apply" && !approval) {
	    approval = argv[i + 1];
	} else {
	    return GED_HELP;
	}
    }
    if (!policy.keep)
	return GED_HELP;
    if (approval && std::string_view(approval).size() != id_string(db_history::absent).size()) {
	bu_vls_strcat(out, "Expected the full preview token from db history prune");
	return BRLCAD_ERROR;
    }
    if (!drop.empty()) {
	db_history::Db5HistoryDraft graph;
	const auto *pack = history_graph(out, *history, graph);
	if (!pack)
	    return BRLCAD_ERROR;
	for (const auto *argument : drop) {
	    db_history::Id id;
	    if (!resolve_archive_revision(out, *pack, argument, id))
		return BRLCAD_ERROR;
	    policy.drop.insert(id);
	}
    }

    db_history::WorkspacePrunePreview preview;
    if (approval) {
	WorkspaceHead committed;
	if (history->workspace->prune(history->workspace_head, policy, approval, preview, committed, out) !=
	    WorkspaceWriteStatus::Ok)
	    return BRLCAD_ERROR;
	history->workspace_head = committed;
	bu_vls_strcat(out, "Prune applied\n");
    } else {
	if (!history->workspace->preview_prune(history->workspace_head, policy, preview, out))
	    return BRLCAD_ERROR;
	bu_vls_printf(out, "preview %s\n", id_string(preview.token).c_str());
    }
    bu_vls_printf(out, "retained revisions %zu\nexpired artifacts %zu\nexpired payload bytes %ju",
	preview.retained_revisions, preview.expired_artifacts, (uintmax_t)preview.expired_bytes);
    for (const auto &expired : preview.expired_revisions)
	bu_vls_printf(out, "\nexpire revision %s", id_string(expired).c_str());
    for (const auto &boundary : preview.boundaries)
	bu_vls_printf(out, "\nretention boundary %s", id_string(boundary).c_str());
    return BRLCAD_OK;
}

struct StatusChoice {
    std::string revision;
    std::string label;
    bool preferred = false;
};

void
copy_choices(struct ged_history_status &status, const StatusChoice &undo, const std::vector<StatusChoice> &redo)
{
    if (!undo.revision.empty())
	status.undo = {bu_strdup(undo.revision.c_str()), bu_strdup(undo.label.c_str()), 0};
    if (redo.empty())
	return;
    auto *choices = static_cast<struct ged_history_choice *>(bu_calloc(redo.size(), sizeof(struct ged_history_choice),
	"history redo choices"));
    for (size_t i = 0; i < redo.size(); ++i)
	choices[i] = {bu_strdup(redo[i].revision.c_str()), bu_strdup(redo[i].label.c_str()), redo[i].preferred};
    status.redo = choices;
    status.redo_count = redo.size();
}

enum ged_history_state
history_state(EmbeddedStatus embedded)
{
    switch (embedded) {
	case EmbeddedStatus::Current: return GED_HISTORY_CURRENT;
	case EmbeddedStatus::ExternalEdits: return GED_HISTORY_EXTERNAL_EDITS;
	case EmbeddedStatus::Malformed: return GED_HISTORY_MALFORMED;
	case EmbeddedStatus::Unsupported: return GED_HISTORY_UNSUPPORTED;
	case EmbeddedStatus::Missing:
	case EmbeddedStatus::Collision: return GED_HISTORY_UNAVAILABLE;
    }
    return GED_HISTORY_UNAVAILABLE;
}

bool
load_history_status(const ged_db_history *history, struct ged_history_status &status, struct bu_vls *message)
{
    if (!history)
	return true;

    status.edit_active = history->stage != nullptr;
    status.staged_conflict = static_cast<bool>(history->conflict);
    StatusChoice undo;
    std::vector<StatusChoice> redo;
    if (!history->workspace && !is_embedded(*history)) {
	status.mode = GED_HISTORY_SESSION;
	status.state = history->conflict ? GED_HISTORY_STALE : GED_HISTORY_CURRENT;
	status.writable = history->save && !history->conflict;
	status.session_bytes = history->used;
	status.session_limit = history->limit;
	const auto oldest = history->revisions.begin()->first;
	status.oldest_revision = bu_strdup(std::to_string(oldest).c_str());
	status.selected_revision = bu_strdup(std::to_string(history->head).c_str());
	const auto &head = history->revisions.at(history->head);
	status.retention_boundary = oldest != 0 && history->head == oldest;
	if (head.parent != no_revision)
	    undo = {std::to_string(head.parent), head.label};
	for (const auto &entry : history->revisions)
	    if (entry.second.parent == history->head)
		redo.push_back({std::to_string(entry.first), entry.second.label, head.preferred == entry.first});
    } else {
	db_history::Db5HistoryDraft graph;
	const auto *pack = &history->embedded.pack;
	if (history->workspace) {
	    WorkspaceHead current;
	    db_history::WorkspaceExport last_export;
	    if (!history->workspace->read_navigation(history->workspace_head, current, graph, message, &last_export))
		return false;
	    pack = &graph.pack;
	    status.mode = GED_HISTORY_WORKSPACE;
	    const bool current_view = current.generation == history->workspace_head.generation &&
		current.revision == history->workspace_head.revision && current.model == history->workspace_head.model;
	    status.state = current_view ? GED_HISTORY_CURRENT : GED_HISTORY_STALE;
	    status.writable = history->workspace_writable;
	    status.generation = history->workspace_head.generation;
	    status.workspace_generation = current.generation;
	    status.workspace_head = bu_strdup(id_string(current.revision).c_str());
	    if (history->local_export.revision != db_history::absent &&
		history->local_export.timestamp >= last_export.timestamp)
		last_export = history->local_export;
	    if (last_export.revision != db_history::absent) {
		status.exported_revision = bu_strdup(id_string(last_export.revision).c_str());
		status.exported_at = last_export.timestamp;
	    }
	} else {
	    status.mode = GED_HISTORY_EMBEDDED;
	    status.state = history->conflict ? GED_HISTORY_STALE : history_state(history->embedded_status);
	    status.writable = history->save && history->embedded_status == EmbeddedStatus::Current && !history->conflict;
	}

	if (pack->selected_revision != db_history::absent) {
	    status.selected_revision = bu_strdup(id_string(pack->selected_revision).c_str());
	    status.retention_boundary = pack->boundaries.count(pack->selected_revision) != 0;
	    db_history::Revision selected;
	    if (!db_history::read_revision(*pack, pack->selected_revision, selected)) {
		status.state = GED_HISTORY_MALFORMED;
		status.writable = 0;
		return true;
	    }
	    if (selected.parent != db_history::absent && !status.retention_boundary)
		undo = {id_string(selected.parent), selected.label};

	    const auto preferred = pack->preferences.find(pack->selected_revision);
	    if (!visit_archive_revisions(*pack, [&](const db_history::Id &id, const db_history::Revision &revision) {
		if (revision.parent == pack->selected_revision && !pack->boundaries.count(id))
		    redo.push_back({id_string(id), revision.label,
			preferred != pack->preferences.end() && preferred->second == id});
	    })) {
		if (message)
		    bu_vls_strcpy(message, "An archived revision could not be read");
		return false;
	    }
	}
    }
    copy_choices(status, undo, redo);
    return true;
}

const char *
mode_name(enum ged_history_mode mode)
{
    switch (mode) {
	case GED_HISTORY_SESSION: return "session";
	case GED_HISTORY_EMBEDDED: return "embedded";
	case GED_HISTORY_WORKSPACE: return "workspace";
	case GED_HISTORY_DISABLED: return "disabled";
    }
    return "disabled";
}

const char *
state_name(enum ged_history_state state, bool json = false)
{
    switch (state) {
	case GED_HISTORY_INACTIVE: return "inactive";
	case GED_HISTORY_CURRENT: return "current";
	case GED_HISTORY_STALE: return "stale";
	case GED_HISTORY_EXTERNAL_EDITS: return json ? "external_edits" : "external edits";
	case GED_HISTORY_MALFORMED: return json ? "malformed" : "malformed archive";
	case GED_HISTORY_UNSUPPORTED: return json ? "unsupported" : "unsupported archive";
	case GED_HISTORY_UNAVAILABLE: return "unavailable";
    }
    return "unavailable";
}

void
print_status_text(struct bu_vls *out, const struct ged_history_status &status)
{
    bu_vls_printf(out, "mode %s", mode_name(status.mode));
    if (status.mode == GED_HISTORY_DISABLED)
	return;
    const bool session = status.mode == GED_HISTORY_SESSION;
    const bool workspace = status.mode == GED_HISTORY_WORKSPACE;
    if (!session || status.staged_conflict)
	bu_vls_printf(out, "\nstatus %s\naccess %s", state_name(status.state), status.writable ? "read-write" : "read-only");
    if (workspace)
	bu_vls_printf(out, "\ngeneration %ju", (uintmax_t)status.generation);
    if (status.selected_revision)
	bu_vls_printf(out, "\nrevision %s", status.selected_revision);
    if (session)
	bu_vls_printf(out, "\noldest %s\nbytes %zu\nlimit %zu", status.oldest_revision, status.session_bytes, status.session_limit);
    if (!session && status.retention_boundary)
	bu_vls_strcat(out, "\nretention boundary");
    if (status.undo.revision) {
	bu_vls_strcat(out, "\nundo ");
	if (!session)
	    bu_vls_printf(out, "%s ", status.undo.revision);
	append_printable(out, status.undo.label);
    }
    for (size_t i = 0; i < status.redo_count; ++i) {
	const auto &redo = status.redo[i];
	bu_vls_printf(out, "\nredo %s ", redo.revision);
	append_printable(out, redo.label);
	if (!session && redo.preferred)
	    bu_vls_strcat(out, " (preferred)");
    }
    if (status.staged_conflict)
	bu_vls_strcat(out, "\nstaged conflict");
    if (workspace) {
	if (status.state == GED_HISTORY_STALE)
	    bu_vls_printf(out, "\nworkspace head %s generation %ju", status.workspace_head, (uintmax_t)status.workspace_generation);
	bu_vls_printf(out, "\nlast export %s", status.exported_revision ? status.exported_revision : "none");
	if (status.exported_at) {
	    struct bu_vls utc = BU_VLS_INIT_ZERO;
	    bu_utctime(&utc, status.exported_at);
	    bu_vls_printf(out, "\nlast export time %s", bu_vls_cstr(&utc));
	    bu_vls_free(&utc);
	}
    }
}

void
print_status_json(struct bu_vls *out, const struct ged_history_status &status)
{
    const auto choice = [](const struct ged_history_choice &action) {
	return Json{{"revision", action.revision}, {"label", action.label}, {"preferred", action.preferred != 0}};
    };
    Json redo = Json::array();
    for (size_t i = 0; i < status.redo_count; ++i)
	redo.push_back(choice(status.redo[i]));
    const bool workspace = status.mode == GED_HISTORY_WORKSPACE;
    const bool session = status.mode == GED_HISTORY_SESSION;
    const Json result = {
	{"schema", history_json_version}, {"mode", mode_name(status.mode)}, {"state", state_name(status.state, true)},
	{"writable", status.writable != 0}, {"edit_active", status.edit_active != 0},
	{"staged_conflict", status.staged_conflict != 0}, {"retention_boundary", status.retention_boundary != 0},
	{"revision", history_json_string(status.selected_revision)}, {"oldest_revision", history_json_string(status.oldest_revision)},
	{"workspace_head", history_json_string(status.workspace_head)}, {"last_export", history_json_string(status.exported_revision)},
	{"last_export_time", status.exported_at ? Json(status.exported_at) : Json(nullptr)},
	{"generation", workspace ? Json(status.generation) : Json(nullptr)},
	{"workspace_generation", workspace ? Json(status.workspace_generation) : Json(nullptr)},
	{"session_bytes", session ? Json(status.session_bytes) : Json(nullptr)},
	{"session_limit", session ? Json(status.session_limit) : Json(nullptr)},
	{"undo", status.undo.revision ? choice(status.undo) : Json(nullptr)}, {"redo", std::move(redo)}
    };
    append_history_json(out, result);
}

int
print_status(struct ged *gedp, bool json = false)
{
    auto *out = gedp->ged_result_str;
    const std::unique_ptr<const struct ged_history_status, decltype(&ged_history_status_destroy)>
	status(ged_history_status_create(gedp, out), ged_history_status_destroy);
    if (!status)
	return BRLCAD_ERROR;
    if (json)
	print_status_json(out, *status);
    else
	print_status_text(out, *status);
    return BRLCAD_OK;
}

struct HistoryCommandArgs {
    HistoryCommandArgs() = default;
    HistoryCommandArgs(const HistoryCommandArgs &) = delete;
    HistoryCommandArgs &operator=(const HistoryCommandArgs &) = delete;

    ~HistoryCommandArgs()
    {
	if (argv)
	    bu_free(argv, "history command list");
    }

    bool parse(const char *list)
    {
	return std::strlen(list) < INT_MAX && bu_argv_from_tcl_list(list, &argc, &argv) == 0 && argc > 0;
    }

    int argc = 0;
    const char **argv = nullptr;
};

int
execute_history_batch(struct ged *gedp, void *data)
{
    const auto &commands = *static_cast<const std::vector<HistoryCommandArgs> *>(data);
    auto *out = gedp->ged_result_str;
    for (size_t i = 0; i < commands.size(); ++i) {
	const auto &command = commands[i];
	bu_vls_trunc(out, 0);
	const int result = ged_exec(gedp, command.argc, command.argv);
	if (result == BRLCAD_OK)
	    continue;
	if (bu_vls_strlen(out))
	    bu_vls_putc(out, '\n');
	bu_vls_printf(out, "Batch command %zu %s", i + 1,
	    (result & GED_MORE) ? "requires more input" : "failed");
	return BRLCAD_ERROR;
    }
    return BRLCAD_OK;
}

int
history_batch(struct ged *gedp, const ged_db_history *history, const char *label, const char *command_list)
{
    auto *out = gedp->ged_result_str;
    if (!history) {
	bu_vls_strcat(out, "Enable database history before running an atomic batch");
	return BRLCAD_ERROR;
    }
    HistoryCommandArgs list;
    if (!list.parse(command_list)) {
	bu_vls_strcat(out, "Expected a nonempty list of GED commands");
	return BRLCAD_ERROR;
    }
    std::vector<HistoryCommandArgs> commands(static_cast<size_t>(list.argc));
    for (size_t i = 0; i < commands.size(); ++i) {
	if (!commands[i].parse(list.argv[i]) || !commands[i].argv[0][0]) {
	    bu_vls_printf(out, "Expected a GED argument list for batch command %zu", i + 1);
	    return BRLCAD_ERROR;
	}
    }
    return ged_history_apply_edit(gedp, label, execute_history_batch, &commands);
}

const Revision *
find_revision(struct bu_vls *out, const ged_db_history *history, const char *argument, uint64_t &id)
{
    if (history && parse_number(argument, id)) {
	const auto entry = history->revisions.find(id);
	if (entry != history->revisions.end())
	    return &entry->second;
    }
    if (out)
	bu_vls_strcat(out, "Unknown or expired session revision");
    return nullptr;
}

int
visit_session_revision(const ged_db_history &history, uint64_t id, const Revision &revision,
		       ged_history_revision_func_t visit, void *data)
{
    const auto key = std::to_string(id);
    const auto parent = revision.parent == no_revision ? std::string() : std::to_string(revision.parent);
    const struct ged_history_revision row{key.c_str(), parent.empty() ? nullptr : parent.c_str(),
	revision.label.c_str(), revision.authored, id == history.head, id != 0 && parent.empty()};
    return visit(&row, data);
}

int
visit_persistent_revision(const db_history::Pack &pack, const db_history::Id &id, const db_history::Revision &revision,
			  ged_history_revision_func_t visit, void *data)
{
    const auto key = id_string(id);
    const bool boundary = pack.boundaries.count(id) != 0;
    const auto parent = boundary || revision.parent == db_history::absent ? std::string() : id_string(revision.parent);
    const struct ged_history_revision row{key.c_str(), parent.empty() ? nullptr : parent.c_str(),
	revision.label.c_str(), revision.time, id == pack.selected_revision, boundary};
    return visit(&row, data);
}

int
visit_session_log(const ged_db_history &history, const char *object,
		  ged_history_revision_func_t visit, void *data)
{
    for (auto entry = history.revisions.rbegin(); entry != history.revisions.rend(); ++entry) {
	if (object) {
	    const auto &changes = entry->second.changes;
	    if (std::none_of(changes.begin(), changes.end(),
		    [object](const Change &change) { return change.name == object; }))
		continue;
	}
	if (visit_session_revision(history, entry->first, entry->second, visit, data) != BRLCAD_OK)
	    return BRLCAD_ERROR;
    }
    return BRLCAD_OK;
}

int
visit_persistent_log(const db_history::Pack &pack, const char *object,
		     ged_history_revision_func_t visit, ged_history_checkpoint_func_t checkpoint,
		     void *data, struct bu_vls *out)
{
    std::vector<std::pair<db_history::Id, db_history::Revision>> entries;
    if (!visit_archive_revisions(pack, [&](const db_history::Id &id, db_history::Revision &revision) {
	if (!object || revision.changes.count(object))
	    entries.emplace_back(id, std::move(revision));
    })) {
	if (out)
	    bu_vls_strcat(out, "An archived revision could not be read");
	return BRLCAD_ERROR;
    }
    std::sort(entries.begin(), entries.end(), [](const auto &a, const auto &b) {
	return a.second.time != b.second.time ? a.second.time > b.second.time : a.first < b.first;
    });
    for (const auto &entry : entries) {
	if (visit_persistent_revision(pack, entry.first, entry.second, visit, data) != BRLCAD_OK)
	    return BRLCAD_ERROR;
    }
    if (!object && checkpoint) {
	for (const auto &ref : pack.references) {
	    if (checkpoint(ref.first.c_str(), id_string(ref.second).c_str(), data) != BRLCAD_OK)
		return BRLCAD_ERROR;
	}
    }
    return BRLCAD_OK;
}

int
visit_rename_changes(const db_history::Renames &renames, ged_history_change_func_t visit, void *data)
{
    for (const auto &rename : renames) {
	const struct ged_history_change row{GED_HISTORY_RENAME, rename.first.c_str(), rename.second.c_str()};
	if (visit(&row, data) != BRLCAD_OK)
	    return BRLCAD_ERROR;
    }
    return BRLCAD_OK;
}

int
diff_session_revisions(struct bu_vls *out, const ged_db_history &history,
		       const char *a, const char *b, const char *object)
{
    uint64_t first, second;
    if (!find_revision(out, &history, a, first) || !find_revision(out, &history, b, second))
	return BRLCAD_ERROR;

    struct RecordChange {
	const std::vector<uint8_t> *before;
	const std::vector<uint8_t> *after;
    };
    // Compare the first and last values along the path, including on a
    // branch crossing. Every touched record is already in the journal.
    std::map<std::string_view, RecordChange> changed;
    for (const auto &step : session_replay(history, first, second)) {
	for (const auto &change : history.revisions.at(step.revision).changes) {
	    if (object && change.name != object)
		continue;
	    const auto &before = step.reverse ? change.after : change.before;
	    const auto &after = step.reverse ? change.before : change.after;
	    auto entry = changed.try_emplace(change.name, RecordChange{&before, &after}).first;
	    entry->second.after = &after;
	}
    }

    bu_vls_printf(out, "changes %ju %ju", static_cast<uintmax_t>(first), static_cast<uintmax_t>(second));
    for (const auto &entry : changed) {
	const auto &change = entry.second;
	if (*change.before != *change.after)
	    print_change(out, !change.before->empty(), !change.after->empty(), entry.first);
    }
    return BRLCAD_OK;
}

int
diff_archive_revisions(struct ged *gedp, const db_history::Pack &pack, const Workspace *workspace,
		       const char *a, const char *b, const char *object)
{
    auto *out = gedp->ged_result_str;
    db_history::Id first, second;
    if (!resolve_archive_revision(out, pack, a, first) || !resolve_archive_revision(out, pack, b, second))
	return BRLCAD_ERROR;
    db_history::Manifest before, after;
    const auto load = [&](const db_history::Id &id, db_history::Manifest &model) {
	return workspace ? workspace->read_model_at(id, model, nullptr) : db_history::manifest_at_revision(pack, id, model);
    };
    if (!load(first, before) || !load(second, after)) {
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
verify_session_history(struct ged *gedp, const ged_db_history &history)
{
    if (!unchanged_history_file(gedp, history))
	return BRLCAD_ERROR;

    db_history::Db5HistoryDraft checked;
    if (!prepare_archive(gedp, &history, checked) || !db_history::verify_pack(checked.pack, gedp->ged_result_str) ||
	!unchanged_history_file(gedp, history))
	return BRLCAD_ERROR;

    bu_vls_strcat(gedp->ged_result_str, "Session journal and selected model verified");
    return BRLCAD_OK;
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
    db_history::Db5HistoryDraft graph;
    if (!unchanged_history_file(gedp, history) || !history_graph(gedp->ged_result_str, history, graph))
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
    if (!saved_database(save_snapshot(gedp, history, history.live, &replacement, &draft.pack)))
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
    if (history.workspace) {
	WorkspaceHead selected;
	if (history.workspace->mark(history.workspace_head, name, move, selected,
	    gedp->ged_result_str) != WorkspaceWriteStatus::Ok)
	    return BRLCAD_ERROR;
	history.workspace_head = selected;
	return BRLCAD_OK;
    }
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
    if (!saved_database(save_snapshot(gedp, history, history.live, &replacement, &marked)))
	return BRLCAD_ERROR;
    install_snapshot(gedp, history, replacement, &marked.selected_revision);
    return BRLCAD_OK;
}

int
export_history(struct ged *gedp, ged_db_history *history, const char *path, bool include_archive)
{
    if (!require_db5_history(gedp))
	return BRLCAD_ERROR;
    auto *out = gedp->ged_result_str;
    const db_history::Pack *pack = nullptr;
    db_history::Db5HistoryDraft draft;
    const auto *workspace = history ? history->workspace.get() : nullptr;
    if (include_archive && !workspace) {
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
    const auto status = workspace ? workspace->export_snapshot(gedp->dbip, path, include_archive, out) :
	db_history::export_db5_snapshot(gedp->dbip, path, pack, out);
    if (status == DB5_SAVE_OK) {
	bu_vls_strcat(out, "Exported ");
	append_printable(out, path);
	if (history && history->workspace) {
	    history->local_export = {history->workspace_head.revision, revision_time()};
	    if (history->workspace_writable) {
		struct bu_vls message = BU_VLS_INIT_ZERO;
		if (history->workspace->record_export(history->local_export, &message))
		    history->local_export = {};
		else
		    bu_vls_printf(out, "\nCould not record the export in the workspace (%s)", bu_vls_addr(&message));
		bu_vls_free(&message);
	    }
	}
    }
    return saved_database(status) ? BRLCAD_OK : BRLCAD_ERROR;
}

int
handle_conflict(struct bu_vls *out, const ged_db_history *history, const char *destination)
{
    if (!history || !history->conflict) {
	bu_vls_strcat(out, "There is no staged conflict");
	return BRLCAD_ERROR;
    }

    const auto *session = std::get_if<ged_db_history::SessionConflict>(history->conflict.get());
    const auto *archive = std::get_if<db_history::Db5HistoryDraft>(history->conflict.get());
    if (destination) {
	enum db5_save_result saved;
	if (session)
	    saved = db_history::export_db5_conflict(history->save.get(), session->snapshot.get(), destination, out);
	else if (history->workspace)
	    saved = history->workspace->export_staged(history->workspace_head, archive->pack, destination, out);
	else
	    saved = db_history::export_db5_archive(archive->pack, destination, out);
	if (saved == DB5_SAVE_OK) {
	    bu_vls_strcat(out, "Exported staged revision to ");
	    append_printable(out, destination);
	}
	return saved_database(saved) ? BRLCAD_OK : BRLCAD_ERROR;
    }

    if (session) {
	const auto &revision = session->revision;
	bu_vls_strcat(out, "staged session revision ");
	append_printable(out, revision.label);
	visit_rename_changes(revision.renames, print_revision_change, out);
	for (const auto &change : revision.changes)
	    print_change(out, !change.before.empty(), !change.after.empty(), change.name);
	return BRLCAD_OK;
    }

    db_history::Revision revision;
    if (!db_history::read_revision(archive->pack, archive->pack.selected_revision, revision)) {
	bu_vls_strcat(out, "The staged revision could not be read");
	return BRLCAD_ERROR;
    }
    bu_vls_printf(out, "staged edit from %s ", id_string(revision.parent).c_str());
    append_printable(out, revision.label);
    visit_rename_changes(revision.renames, print_revision_change, out);
    print_archive_changes(out, revision.changes);
    return BRLCAD_OK;
}

int
step_archive_revision(struct ged *gedp, ged_db_history &history, const db_history::Pack &pack,
		      bool undo, const char *argument)
{
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
	    print_status(gedp);
	    bu_vls_strcat(out, "\nSelect a revision with db redo <revision>");
	    return BRLCAD_ERROR;
	}
    }
    if (target == db_history::absent) {
	bu_vls_printf(out, "No retained revision to %s", undo ? "undo" : "redo");
	if (undo && pack.boundaries.count(pack.selected_revision))
	    bu_vls_strcat(out, " (retention boundary)");
	return BRLCAD_ERROR;
    }
    return checkout_history(gedp, history, target);
}

} // namespace


const struct ged_history_status *
ged_history_status_create(struct ged *gedp, struct bu_vls *message)
{
    if (!gedp || !gedp->i) {
	if (message)
	    bu_vls_strcpy(message, "A GED context is required to query database history");
	return nullptr;
    }
    struct ged_history_status *allocated;
    BU_ALLOC(allocated, struct ged_history_status);
    std::unique_ptr<struct ged_history_status, decltype(&ged_history_status_destroy)>
	status(allocated, ged_history_status_destroy);
    if (!load_history_status(gedp->i->db_history, *status, message))
	return nullptr;
    if (gedp->i->i && !gedp->i->i->edit_buf.empty())
	status->edit_active = 1;
    return status.release();
}

void
ged_history_status_destroy(const struct ged_history_status *status)
{
    if (!status)
	return;
    const char *fields[] = {status->selected_revision, status->oldest_revision, status->workspace_head,
	status->exported_revision, status->undo.revision, status->undo.label};
    for (const auto *field : fields)
	if (field)
	    bu_free(const_cast<char *>(field), "history status text");
    for (size_t i = 0; i < status->redo_count; ++i) {
	bu_free(const_cast<char *>(status->redo[i].revision), "history redo revision");
	bu_free(const_cast<char *>(status->redo[i].label), "history redo label");
    }
    if (status->redo)
	bu_free(const_cast<struct ged_history_choice *>(status->redo), "history redo choices");
    bu_free(const_cast<struct ged_history_status *>(status), "history status");
}

int
ged_history_visit(struct ged *gedp, const char *object, ged_history_revision_func_t revision,
    ged_history_checkpoint_func_t checkpoint, void *data, struct bu_vls *message)
{
    const auto *history = gedp && gedp->i ? gedp->i->db_history : nullptr;
    if (!history || !revision) {
	if (message)
	    bu_vls_strcat(message, !history ? "Session history is disabled" : "A revision visitor is required");
	return BRLCAD_ERROR;
    }
    if (!history->workspace && !is_embedded(*history))
	return visit_session_log(*history, object, revision, data);

    db_history::Db5HistoryDraft graph;
    const auto *pack = history_graph(message, *history, graph);
    return pack ? visit_persistent_log(*pack, object, revision, checkpoint, data, message) : BRLCAD_ERROR;
}

int
ged_history_visit_revision(struct ged *gedp, const char *id, ged_history_revision_func_t revision,
    ged_history_change_func_t change, void *data, struct bu_vls *message)
{
    const auto *history = gedp && gedp->i ? gedp->i->db_history : nullptr;
    if (!history || !id) {
	if (message)
	    bu_vls_strcat(message, !history ? "Session history is disabled" : "A revision ID is required");
	return BRLCAD_ERROR;
    }
    if (!history->workspace && !is_embedded(*history)) {
	uint64_t target;
	const auto *saved = find_revision(message, history, id, target);
	if (!saved || (revision && visit_session_revision(*history, target, *saved, revision, data) != BRLCAD_OK))
	    return BRLCAD_ERROR;
	if (!change)
	    return BRLCAD_OK;
	if (visit_rename_changes(saved->renames, change, data) != BRLCAD_OK)
	    return BRLCAD_ERROR;
	for (const auto &entry : saved->changes) {
	    const struct ged_history_change row{change_type(!entry.before.empty(), !entry.after.empty()),
		entry.name.c_str(), nullptr};
	    if (change(&row, data) != BRLCAD_OK)
		return BRLCAD_ERROR;
	}
	return BRLCAD_OK;
    }

    db_history::Db5HistoryDraft graph;
    const auto *pack = history_graph(message, *history, graph);
    db_history::Id target;
    if (!pack || !resolve_archive_revision(message, *pack, id, target))
	return BRLCAD_ERROR;
    db_history::Revision saved;
    if (!db_history::read_revision(*pack, target, saved)) {
	if (message)
	    bu_vls_strcat(message, "The archived revision could not be read");
	return BRLCAD_ERROR;
    }
    if (revision && visit_persistent_revision(*pack, target, saved, revision, data) != BRLCAD_OK)
	return BRLCAD_ERROR;
    if (!change)
	return BRLCAD_OK;
    if (visit_rename_changes(saved.renames, change, data) != BRLCAD_OK)
	return BRLCAD_ERROR;
    for (const auto &entry : saved.changes) {
	const struct ged_history_change row{change_type(entry.second.before != db_history::absent,
	    entry.second.after != db_history::absent), entry.first.c_str(), nullptr};
	if (change(&row, data) != BRLCAD_OK)
	    return BRLCAD_ERROR;
    }
    return BRLCAD_OK;
}

struct db_i *
ged_history_open_revision(struct ged *gedp, const char *id, struct bu_vls *message)
{
    const auto *history = gedp && gedp->i ? gedp->i->db_history : nullptr;
    if (!history || !id) {
	if (message)
	    bu_vls_strcat(message, !history ? "Session history is disabled" : "A revision ID is required");
	return nullptr;
    }
    if (!history->workspace && !is_embedded(*history)) {
	uint64_t target;
	if (!find_revision(message, history, id, target))
	    return nullptr;
	std::vector<ReplayStep> steps;
	return session_snapshot(*history, target, steps, message).release();
    }

    db_history::Db5HistoryDraft graph;
    const auto *pack = history_graph(message, *history, graph);
    db_history::Id target;
    if (!pack || !resolve_archive_revision(message, *pack, id, target))
	return nullptr;
    if (history->workspace) {
	WorkspaceHead selected;
	return Workspace::open_revision(history->live->dbi_filename, target, selected, message);
    }

    const auto *external_base = history->embedded_status == EmbeddedStatus::ExternalEdits ? history->live : nullptr;
    Edit edit(db_edit_begin(history->live, "Preview revision"), db_edit_abort);
    if (edit && db_history::stage_db5_preview(history->embedded, target, edit.get(), external_base)) {
	if (auto *snapshot = db_edit_commit(edit.get())) {
	    edit.release();
	    return snapshot;
	}
    }
    if (message)
	bu_vls_strcat(message, "Could not construct the selected revision for preview");
    return nullptr;
}

void
ged_history_set_replacement_handlers(struct ged *gedp,
    ged_history_prepare_func_t prepare, ged_history_replaced_func_t replaced, void *data)
{
    if (!gedp || !gedp->i)
	return;
    gedp->i->history_prepare = prepare;
    gedp->i->history_replaced = replaced;
    gedp->i->history_handler_data = data;
}

void
ged_history_rebuild_legacy_display(struct ged *gedp, struct db_i *retired)
{
    struct display_list *display = ged_dl(gedp);
    if (!retired || !display || gedp->new_cmd_forms || BU_LIST_IS_EMPTY(&display->l))
	return;

    struct DrawRequest {
	std::string path;
	struct bv_obj_settings settings;
    };
    const auto same_style = [](const struct bv_obj_settings &a, const struct bv_obj_settings &b) {
	return a.s_dmode == b.s_dmode && EQUAL(a.transparency, b.transparency) &&
	    a.draw_solid_lines_only == b.draw_solid_lines_only &&
	    a.draw_non_subtract_only == b.draw_non_subtract_only &&
	    a.color_override == b.color_override &&
	    (!a.color_override || !std::memcmp(a.color, b.color, sizeof(a.color)));
    };

    std::vector<DrawRequest> requests;
    struct display_list *item;
    while (BU_LIST_WHILE(item, display_list, &display->l)) {
	const auto *root = static_cast<const struct directory *>(item->dl_dp);
	if (root && root->d_addr != RT_DIR_PHONY_ADDR &&
	    BU_LIST_NON_EMPTY(&item->dl_head_scene_obj)) {
	    const auto *first = BU_LIST_FIRST(bv_scene_obj, &item->dl_head_scene_obj);
	    bool uniform_style = true;
	    struct bv_scene_obj *solid;
	    for (BU_LIST_FOR(solid, bv_scene_obj, &item->dl_head_scene_obj)) {
		if (!same_style(*first->s_os, *solid->s_os)) {
		    uniform_style = false;
		    break;
		}
	    }
	    if (uniform_style) {
		requests.push_back({bu_vls_cstr(&item->dl_path), *first->s_os});
	    } else {
		for (BU_LIST_FOR(solid, bv_scene_obj, &item->dl_head_scene_obj)) {
		    const auto *data = static_cast<const struct ged_bv_data *>(solid->s_u_data);
		    if (!data || !data->s_fullpath.fp_len)
			continue;
		    char *path = db_path_to_string(&data->s_fullpath);
		    requests.push_back({path, *solid->s_os});
		    bu_free(path, "legacy draw path");
		}
	    }
	}
	_ged_dl_free_display_item(gedp, item, retired);
    }

    const int saved_result = ged_results_ret(gedp->ged_results);
    struct bu_vls *out = gedp->ged_result_str;
    const std::string saved_output(bu_vls_cstr(out));
    std::vector<std::string> failed;
    gedp->ged_skip_clbks++;
    for (const auto &request : requests) {
	if (!db_lookup(gedp->dbip, request.path.c_str(), LOOKUP_QUIET))
	    continue;

	const auto &style = request.settings;
	const bool evaluated = style.s_dmode == _GED_BOOL_EVAL;
	const int mode = style.s_dmode == _GED_SHADED_MODE_EVAL ? 3 :
	    style.s_dmode == _GED_WIREFRAME_EVAL ? 5 : style.s_dmode;
	const auto mode_arg = std::to_string(mode);
	const auto opacity = std::to_string(style.transparency);
	const std::string color = std::to_string(style.color[0]) + "/" +
	    std::to_string(style.color[1]) + "/" + std::to_string(style.color[2]);
	std::vector<const char *> argv{evaluated ? "ev" : "draw", "-R", "-x", opacity.c_str()};
	if (!evaluated) {
	    argv.push_back("-m");
	    argv.push_back(mode_arg.c_str());
	}
	if (style.color_override) {
	    argv.push_back("-C");
	    argv.push_back(color.c_str());
	}
	if (style.draw_solid_lines_only)
	    argv.push_back("-s");
	if (style.draw_non_subtract_only)
	    argv.push_back("-S");
	argv.push_back(request.path.c_str());
	if (ged_exec(gedp, static_cast<int>(argv.size()), argv.data()) != BRLCAD_OK)
	    failed.push_back(request.path);
    }
    gedp->ged_skip_clbks--;
    gedp->ged_results->ret = saved_result;
    bu_vls_strcpy(out, saved_output.c_str());
    for (const auto &path : failed)
	bu_vls_printf(out, "\nDatabase saved. Could not redraw %s.", path.c_str());
}

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
ged_db_history_open(struct ged *gedp, int read_only)
{
    if (!gedp->dbip || gedp->i->db_history || db_version(gedp->dbip) != 5)
	return;
    if (auto workspace = workspace_history(gedp->dbip, read_only, gedp->ged_result_str)) {
	attach_history(*workspace, gedp->dbip);
	gedp->i->db_history = workspace.release();
	return;
    }
    if (!db_lookup(gedp->dbip, db_history::archive_name, LOOKUP_QUIET))
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

const struct directory *
ged_db_history_archive(const struct db_i *dbip)
{
    if (!dbip || db_version(dbip) != 5)
	return nullptr;
    const auto *archive = db_lookup(dbip, db_history::archive_name, LOOKUP_QUIET);
    if (!archive)
	return nullptr;
    const auto status = db_history::inspect_db5_history(dbip);
    return status == EmbeddedStatus::Missing || status == EmbeddedStatus::Collision ? nullptr : archive;
}

int
ged_db_history_staged(struct ged *gedp)
{
    const auto *history = gedp && gedp->i ? gedp->i->db_history : nullptr;
    return history && history->stage;
}

static int
begin_history_edit(struct ged *gedp, ged_db_history &history, const char *label, struct db_edit **edit)
{
    if (history.stage)
	return BRLCAD_OK;
    if (!can_write_history(gedp, history))
	return BRLCAD_ERROR;

    auto *stage = begin_editor_edit(history.live, label);
    if (!stage) {
	bu_vls_printf(gedp->ged_result_str, "Could not stage database edit");
	return BRLCAD_ERROR;
    }
    *edit = stage;
    history.stage = stage;
    history.failed = false;
    ged_event_stage_private(gedp, 1);
    gedp->dbip = db_edit_view(stage);
    return BRLCAD_OK;
}

int
ged_history_apply_edit(struct ged *gedp, const char *label, ged_history_edit_func_t apply, void *data)
{
    if (!gedp || !gedp->i || !gedp->dbip || !apply)
	return BRLCAD_ERROR;
    auto *history = gedp->i->db_history;
    if (!history) {
	if (gedp->dbip->dbi_read_only) {
	    bu_vls_strcat(gedp->ged_result_str, "The database is open read-only");
	    return BRLCAD_ERROR;
	}
	return apply(gedp, data);
    }

    struct db_edit *edit = nullptr;
    const std::string name(std::string_view(label ? label : "Edit model").substr(0, label_limit));
    const int begun = begin_history_edit(gedp, *history, name.c_str(), &edit);
    return begun == BRLCAD_OK ?
	ged_db_history_leave(gedp, edit, apply(gedp, data)) : begun;
}

int
ged_db_history_enter(struct ged *gedp, const char *command, int argc, const char **argv,
		     struct db_edit **edit)
{
    auto *history = gedp->i->db_history;
    if (!history || permitted_query(command, argc, argv) ||
	(!history->stage && (session_lifecycle(command) || permitted_standalone(command))))
	return BRLCAD_OK;
    if (!permitted_edit(command, argc, argv)) {
	bu_vls_printf(gedp->ged_result_str, "%s is unavailable while database history is enabled", command);
	return BRLCAD_ERROR;
    }
    const auto label = operation_label(command, argc, argv);
    return begin_history_edit(gedp, *history, label.c_str(), edit);
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

    if (history->workspace)
	return publish_workspace_edit(gedp, *history, edit);

    if (is_embedded(*history)) {
	db_history::Db5HistoryDraft draft;
	switch (db_history::append_db5_history(history->embedded, stage, revision_time(), draft)) {
	    case db_history::AppendStatus::Unchanged:
		copy_region_defaults(history->live, db_edit_view(stage));
		return BRLCAD_OK;
	    case db_history::AppendStatus::Ready:
		return publish_embedded_edit(gedp, *history, edit, draft);
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
    if (revision.changes.empty()) {
	copy_region_defaults(history->live, db_edit_view(stage));
	return BRLCAD_OK;
    }

    // Allocate the journal entry before installing the file. After the
    // replacement succeeds, advancing the head requires no allocation.
    std::map<uint64_t, Revision> pending;
    pending.emplace(history->next, std::move(revision));
    Snapshot snapshot(db_edit_commit(stage), db_close);
    if (snapshot)
	edit.release();
    struct db_i *replacement = nullptr;
    const auto saved = save_snapshot(gedp, *history, snapshot.get(), &replacement);
    if (!saved_database(saved)) {
	if (saved == DB5_SAVE_STALE)
	    retain_conflict(gedp, *history, ged_db_history::SessionConflict{std::move(snapshot), std::move(pending.begin()->second)});
	return BRLCAD_ERROR;
    }
    copy_region_defaults(history->live, snapshot.get());
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
	if (argc == 4 && BU_STR_EQUAL(argv[1], "batch"))
	    return history_batch(gedp, history, argv[2], argv[3]);
	if ((argc == 2 || (argc == 3 && BU_STR_EQUAL(argv[2], "--json"))) && BU_STR_EQUAL(argv[1], "status"))
	    return print_status(gedp, argc == 3);
	if (argc >= 2 && BU_STR_EQUAL(argv[1], "prune")) {
	    const int pruned = prune_workspace(gedp, history, argc, argv);
	    if (pruned == GED_HELP)
		bu_vls_strcat(gedp->ged_result_str,
		    "Usage: db history prune --keep <revisions> [--drop <tip>]... [--apply <preview>]");
	    return pruned;
	}
	if (argc == 3 && BU_STR_EQUAL(argv[1], "workspace")) {
	    const int status = create_workspace(gedp, argv[2]);
	    if (status != BRLCAD_OK)
		return status;
	    if (bu_vls_strlen(gedp->ged_result_str))
		bu_vls_putc(gedp->ged_result_str, '\n');
	    return print_status(gedp);
	}
	const bool save_conflict = argc == 4 && BU_STR_EQUAL(argv[2], "export");
	if ((argc == 2 || save_conflict) && BU_STR_EQUAL(argv[1], "conflict"))
	    return handle_conflict(gedp->ged_result_str, history, save_conflict ? argv[3] : nullptr);
	const bool verify = argc == 2 && BU_STR_EQUAL(argv[1], "verify");
	const bool adopt = (argc == 2 || (argc == 3 && BU_STR_EQUAL(argv[2], "--apply"))) && BU_STR_EQUAL(argv[1], "adopt");
	if (verify) {
	    if (!history) {
		bu_vls_strcat(gedp->ged_result_str, "Enable database history before using this command");
		return BRLCAD_ERROR;
	    }
	    if (history->workspace) {
		if (!history->workspace->verify(gedp->ged_result_str))
		    return BRLCAD_ERROR;
		bu_vls_strcat(gedp->ged_result_str, "Workspace archive and selected model verified");
		return BRLCAD_OK;
	    }
	    return is_embedded(*history) ? verify_embedded_history(gedp, *history) : verify_session_history(gedp, *history);
	}
	if (adopt) {
	    if (!history || !is_embedded(*history)) {
		bu_vls_strcat(gedp->ged_result_str, "This command requires embedded history");
		return BRLCAD_ERROR;
	    }
	    return adopt_external_history(gedp, *history, argc == 3);
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
		print_status(gedp);
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
		print_status(gedp);
	    return enabled;
	}
	if (argc >= 2 && BU_STR_EQUAL(argv[1], "log"))
	    return history_log(gedp, argc - 2, argv + 2);
	if ((argc == 3 || (argc == 4 && BU_STR_EQUAL(argv[3], "--json"))) && BU_STR_EQUAL(argv[1], "show"))
	    return history_show(gedp, argv[2], argc == 4);
	if (argc == 3 && BU_STR_EQUAL(argv[1], "checkout")) {
	    if (history && (history->workspace || is_embedded(*history))) {
		db_history::Db5HistoryDraft graph;
		const auto *pack = history_graph(gedp->ged_result_str, *history, graph);
		db_history::Id id;
		if (!pack || !resolve_archive_revision(gedp->ged_result_str, *pack, argv[2], id))
		    return BRLCAD_ERROR;
		return checkout_history(gedp, *history, id);
	    }
	    uint64_t target;
	    if (!find_revision(gedp->ged_result_str, history, argv[2], target))
		return BRLCAD_ERROR;
	    return travel(gedp, *history, target);
	}
	const bool diff = (argc == 4 || (argc == 6 && BU_STR_EQUAL(argv[4], "--object"))) && BU_STR_EQUAL(argv[1], "diff");
	const bool mark = (argc == 3 || (argc == 4 && BU_STR_EQUAL(argv[3], "--move"))) && BU_STR_EQUAL(argv[1], "mark");
	if (diff || mark) {
	    if (!history) {
		bu_vls_strcat(gedp->ged_result_str, "Enable database history before using this command");
		return BRLCAD_ERROR;
	    }
	    const bool persistent = history->workspace || is_embedded(*history);
	    if (mark) {
		if (!persistent) {
		    bu_vls_strcat(gedp->ged_result_str, "Checkpoints require embedded or workspace history");
		    return BRLCAD_ERROR;
		}
		return mark_archive_revision(gedp, *history, argv[2], argc == 4);
	    }
	    const char *object = argc == 6 ? argv[5] : nullptr;
	    if (!persistent)
		return diff_session_revisions(gedp->ged_result_str, *history, argv[2], argv[3], object);
	    db_history::Db5HistoryDraft graph;
	    const auto *pack = history_graph(gedp->ged_result_str, *history, graph);
	    return pack ? diff_archive_revisions(gedp, *pack, history->workspace.get(), argv[2], argv[3], object) : BRLCAD_ERROR;
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
    if (history->workspace || is_embedded(*history)) {
	db_history::Db5HistoryDraft graph;
	if (history->workspace && argc == 1) {
	    WorkspaceHead observed;
	    return history->workspace->read_navigation(history->workspace_head, observed, graph, gedp->ged_result_str) ?
		step_archive_revision(gedp, *history, graph.pack, undo, nullptr) : BRLCAD_ERROR;
	}
	const auto *pack = history_graph(gedp->ged_result_str, *history, graph);
	return pack ? step_archive_revision(gedp, *history, *pack, undo, argc == 2 ? argv[1] : nullptr) : BRLCAD_ERROR;
    }

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
	    print_status(gedp);
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
