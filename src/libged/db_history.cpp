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

#include "bu/str.h"
#include "bu/vls.h"
#include "ged.h"
#include "ged_private.h"
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
	std::string label;
	std::vector<Change> changes;
    };


    struct db_i *live = nullptr;
    struct db_edit *stage = nullptr;
    bool failed = false;
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
	bu_vls_printf(gedp->ged_result_str, "This editor needs database replacement support before enabling session history");
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
	history.revisions.erase(root);
    }
}

bool
save_snapshot(struct ged *gedp, ged_db_history &history, Snapshot &snapshot, struct db_i **replacement)
{
    if (!snapshot) {
	bu_vls_printf(gedp->ged_result_str, "\nDatabase edit could not be committed");
	return false;
    }
    if (!history.save) {
	bu_vls_printf(gedp->ged_result_str, "\nSession history needs the database to be reopened");
	return false;
    }

    struct bu_vls message = BU_VLS_INIT_ZERO;
    const auto status = db5_save_replace(history.save.get(), snapshot.get(), replacement, &message);
    if (status != DB5_SAVE_OK)
	bu_vls_printf(gedp->ged_result_str, "\n%s", bu_vls_addr(&message));
    bu_vls_free(&message);
    return status == DB5_SAVE_OK || status == DB5_SAVE_SYNC_FAILED;
}

void
install_snapshot(struct ged *gedp, ged_db_history &history, struct db_i *replacement)
{
    auto *retired = history.live;
    history.save.reset(db5_save_begin(replacement, nullptr));
    replacement->dbi_read_only = 1;
    replacement->i->dbi_edit_guard = 1;
    history.live = replacement;
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
    if (history.live != gedp->dbip) {
	bu_vls_printf(gedp->ged_result_str, "The open database has changed since session history was enabled");
	return BRLCAD_ERROR;
    }
    if (target == history.head)
	return BRLCAD_OK;
    if (!can_replace_database(gedp))
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
    if (!save_snapshot(gedp, history, snapshot, &replacement))
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
    if (gedp->i->db_history)
	return BRLCAD_OK;
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
    history->live = gedp->dbip;
    // Keep direct database callers on the live read-only handle. Only
    // commands participating in the edit scope receive a writable view.
    history->live->dbi_read_only = 1;
    history->live->i->dbi_edit_guard = 1;
    gedp->i->db_history = history.release();
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

void
print_status(struct ged *gedp, const ged_db_history *history)
{
    if (!history) {
	bu_vls_printf(gedp->ged_result_str, "mode disabled");
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
    for (const auto &change : revision.changes) {
	const char *action = "update";
	if (change.before.empty())
	    action = "create";
	else if (change.after.empty())
	    action = "delete";
	bu_vls_printf(out, "\n%s ", action);
	append_printable(out, change.name);
    }
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
	history->live->dbi_read_only = 0;
	history->live->i->dbi_edit_guard = 0;
    }
    delete history;
    gedp->i->db_history = nullptr;
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
	bu_vls_printf(gedp->ged_result_str, "%s is unavailable while session history is enabled", command);
	return BRLCAD_ERROR;
    }
    if (history->stage)
	return BRLCAD_OK;
    if (history->live != gedp->dbip) {
	bu_vls_printf(gedp->ged_result_str, "The open database has changed since session history was enabled");
	return BRLCAD_ERROR;
    }
    if (!can_replace_database(gedp))
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

    Revision revision;
    revision.parent = history->head;
    revision.label = db_edit_label(stage);
    Collect collect{revision, history->limit};
    if (db_edit_visit(stage, collect_change, &collect) != 0) {
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
    if (!save_snapshot(gedp, *history, snapshot, &replacement))
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
	if (argc == 2 && BU_STR_EQUAL(argv[1], "log"))
	    return print_log(gedp, history, nullptr);
	if (argc == 4 && BU_STR_EQUAL(argv[1], "log") && BU_STR_EQUAL(argv[2], "--object"))
	    return print_log(gedp, history, argv[3]);
	if (argc == 3 && one_of(argv[1], {"show", "checkout"})) {
	    uint64_t target;
	    const auto *revision = find_revision(gedp, history, argv[2], target);
	    if (!revision)
		return BRLCAD_ERROR;
	    if (BU_STR_EQUAL(argv[1], "show")) {
		print_changes(gedp, *history, target, *revision);
		return BRLCAD_OK;
	    }
	    return travel(gedp, *history, target);
	}
	bu_vls_printf(gedp->ged_result_str, "Usage: db history status | enable session [bytes] | log [--object name] | show <revision> | checkout <revision>");
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
