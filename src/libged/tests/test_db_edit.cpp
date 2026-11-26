/*                     D B _ E D I T . C P P
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

#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "bu/app.h"
#include "bu/file.h"
#include "ged.h"
#include "ged/edit.h"
#include "ged/event.h"
#include "ged/history.h"
#include "raytrace.h"
#include "rt/geom.h"
#include "../../libbu/json.hpp"

#define CHECK(expr) do { \
    if (!(expr)) { \
	bu_log("GED database edit test failed at line %d: %s\n", __LINE__, #expr); \
	return 1; \
    } \
} while (0)

static struct ged *
open_handle(const char *type, struct db_i *dbip)
{
    struct bu_vls address = BU_VLS_INIT_ZERO;
    bu_vls_sprintf(&address, "%p", static_cast<void *>(dbip));
    struct ged *gedp = ged_open(type, bu_vls_cstr(&address), 0);
    bu_vls_free(&address);
    return gedp;
}

static struct ged *
open_inmem(struct db_i *dbip)
{
    return open_handle("inmem", dbip);
}

template <size_t argc>
int
run(struct ged *gedp, const char *(&argv)[argc])
{
    return ged_exec(gedp, static_cast<int>(argc), argv);
}

static int
count_change(const char *UNUSED(name), const struct bu_external *UNUSED(before),
	     const struct bu_external *UNUSED(after), void *data)
{
    ++*static_cast<int *>(data);
    return 0;
}

static int
record_rename(const char *before, const char *after, void *data)
{
    auto &names = *static_cast<std::string *>(data);
    names += before;
    names += " -> ";
    names += after;
    return 0;
}

static void
fail_after_write(struct db_i *dbip, struct directory *UNUSED(dp), int mode, void *data)
{
    if (mode != 0)
	return;
    // The first dependent write succeeds. The rename that follows must
    // fail after the private view has diverged from the base.
    ++*static_cast<int *>(data);
    dbip->dbi_read_only = 1;
}

static bool
contains_result(const struct ged *gedp, const char *text)
{
    return std::string_view(bu_vls_addr(gedp->ged_result_str)).find(text) != std::string_view::npos;
}


static void
count_commit(struct ged *UNUSED(gedp), const struct ged_event *events, size_t count,
	     const struct ged_event_result *UNUSED(result), void *data)
{
    for (size_t i = 0; i < count; ++i) {
	if (events[i].kind == GED_EVENT_BATCH_REBUILD)
	    ++*static_cast<int *>(data);
    }
}


static int
fail_staged_edit(int UNUSED(argc), const char **UNUSED(argv), void *ged_context, void *data)
{
    auto *gedp = static_cast<struct ged *>(ged_context);
    return db_add_changed_clbk(gedp->dbip, fail_after_write, data);
}


static int
test_disk_history()
{
    char path[MAXPATHLEN] = {};
    FILE *file = bu_temp_file(path, sizeof(path));
    CHECK(file && fclose(file) == 0);
    struct db_i *initial = db_create(path, 5);
    CHECK(initial);
    db_close(initial);
    const std::unique_ptr<struct ged, decltype(&ged_close)> owner(ged_open("db", path, 1), ged_close);
    auto *gedp = owner.get();
    CHECK(gedp);

    const char *make[] = {"make", "extrusion.s", "extrude", nullptr};
    const char *reference[] = {"get", "extrusion.s", "S", nullptr};
    CHECK(run(gedp, make) == BRLCAD_OK && run(gedp, reference) == BRLCAD_OK);
    const std::string old_name = bu_vls_addr(gedp->ged_result_str);
    CHECK(!old_name.empty());
    const char *group[] = {"g", "assembly.c", old_name.c_str(), "extrusion.s", nullptr};
    const char *status[] = {"db", "history", "status", nullptr};
    const char *enable[] = {"db", "history", "enable", "session", nullptr};
    const char *rename[] = {"db", "mvall", old_name.c_str(), "outline.s", nullptr};
    const char *undo[] = {"db", "undo", nullptr};
    const char *redo[] = {"db", "redo", nullptr};
    CHECK(run(gedp, group) == BRLCAD_OK);
    CHECK(run(gedp, status) == BRLCAD_OK && contains_result(gedp, "mode disabled"));
    const char *help[] = {"db", "help", nullptr};
    CHECK(run(gedp, help) == GED_HELP && contains_result(gedp, "Available subcommands"));
    CHECK(contains_result(gedp, "Restore the parent revision."));
    CHECK(contains_result(gedp, "Restore a child revision."));
    const int enable_result = run(gedp, enable);
    if (enable_result != BRLCAD_OK && contains_result(gedp, "Atomic DB5 replacement is unavailable"))
	return 0;
    CHECK(enable_result == BRLCAD_OK && contains_result(gedp, "revision 0"));
    const char *verify_session[] = {"db", "history", "verify", nullptr};
    CHECK(run(gedp, verify_session) == BRLCAD_OK && contains_result(gedp, "Session journal and selected model verified"));
    const char *status_json[] = {"db", "history", "status", "--json", nullptr};
    CHECK(run(gedp, status_json) == BRLCAD_OK);
    const auto session_json = nlohmann::json::parse(bu_vls_addr(gedp->ged_result_str), nullptr, false);
    CHECK(!session_json.is_discarded() && session_json["mode"] == "session" && session_json["revision"] == "0" &&
	session_json["oldest_revision"] == "0" && session_json["generation"].is_null() &&
	session_json["undo"].is_null() && session_json["redo"].empty());
    auto *sketch = db_lookup(gedp->dbip, old_name.c_str(), LOOKUP_QUIET);
    CHECK(sketch && db_rename(gedp->dbip, sketch, "bypass.s") < 0);
    CHECK(db_dirdelete(gedp->dbip, sketch) < 0);

    const std::unique_ptr<struct db_i, decltype(&db_close)> old_reader(db_open(path, DB_OPEN_READONLY), db_close);
    CHECK(old_reader && db_dirbuild(old_reader.get()) == 0);
    int commits = 0;
    CHECK(ged_event_observer_add(gedp, GED_EVENT_OBSERVER_POST_RECONCILE, count_commit, &commits));
    int failed_writes = 0;
    CHECK(ged_clbk_set(gedp, "mvall", BU_CLBK_PRE, fail_staged_edit, &failed_writes) == BRLCAD_OK);
    CHECK(run(gedp, rename) != BRLCAD_OK && failed_writes == 1 && commits == 0);
    CHECK(db_lookup(gedp->dbip, old_name.c_str(), LOOKUP_QUIET));
    CHECK(ged_clbk_set(gedp, "mvall", BU_CLBK_PRE, nullptr, nullptr) == GED_OVERRIDE);

    CHECK(run(gedp, rename) == BRLCAD_OK && commits == 1);
    CHECK(!db_lookup(gedp->dbip, old_name.c_str(), LOOKUP_QUIET));
    CHECK(db_lookup(old_reader.get(), old_name.c_str(), LOOKUP_QUIET));
    CHECK(run(gedp, status) == BRLCAD_OK && contains_result(gedp, "revision 1"));
    CHECK(contains_result(gedp, ("undo mvall " + old_name + " outline.s").c_str()));
    CHECK(run(gedp, reference) == BRLCAD_OK && contains_result(gedp, "outline.s"));
    const std::unique_ptr<struct db_i, decltype(&db_close)>
	preview_initial(ged_history_open_revision(gedp, "0", nullptr), db_close);
    CHECK(preview_initial && preview_initial->dbi_read_only &&
	db_lookup(preview_initial.get(), old_name.c_str(), LOOKUP_QUIET) &&
	!db_lookup(preview_initial.get(), "outline.s", LOOKUP_QUIET));
    CHECK(db_lookup(gedp->dbip, "outline.s", LOOKUP_QUIET) && commits == 1);
    CHECK(run(gedp, undo) == BRLCAD_OK && commits == 2);
    CHECK(run(gedp, reference) == BRLCAD_OK && contains_result(gedp, old_name.c_str()));
    CHECK(run(gedp, redo) == BRLCAD_OK && commits == 3);
    CHECK(run(gedp, reference) == BRLCAD_OK && contains_result(gedp, "outline.s"));

    CHECK(run(gedp, undo) == BRLCAD_OK && commits == 4);
    const char *title[] = {"title", "fork", nullptr};
    CHECK(run(gedp, title) == BRLCAD_OK && commits == 5);
    CHECK(run(gedp, title) == BRLCAD_OK && commits == 5);
    CHECK(run(gedp, undo) == BRLCAD_OK && commits == 6);
    CHECK(run(gedp, status) == BRLCAD_OK && contains_result(gedp, "redo 1 mvall"));
    CHECK(contains_result(gedp, "redo 2 title fork"));
    const std::string before_verification = bu_vls_cstr(gedp->ged_result_str);
    CHECK(run(gedp, verify_session) == BRLCAD_OK && contains_result(gedp, "verified"));
    CHECK(run(gedp, status) == BRLCAD_OK && before_verification == bu_vls_cstr(gedp->ged_result_str));
    const char *log[] = {"db", "history", "log", nullptr};
    CHECK(run(gedp, log) == BRLCAD_OK && contains_result(gedp, "revision 2 parent 0 title fork"));
    CHECK(contains_result(gedp, "revision 1 parent 0 mvall"));
    CHECK(contains_result(gedp, "revision 0 (selected)"));
    const char *object_log[] = {"db", "history", "log", "--object", "extrusion.s", nullptr};
    CHECK(run(gedp, object_log) == BRLCAD_OK && contains_result(gedp, "revision 1 parent 0 mvall"));
    CHECK(!contains_result(gedp, "revision 2"));
    const char *show[] = {"db", "history", "show", "1", nullptr};
    CHECK(run(gedp, show) == BRLCAD_OK && contains_result(gedp, "create outline.s"));
    CHECK(contains_result(gedp, ("rename " + old_name + " -> outline.s").c_str()));
    CHECK(contains_result(gedp, ("delete " + old_name).c_str()));
    CHECK(contains_result(gedp, "update assembly.c") && contains_result(gedp, "update extrusion.s"));
    CHECK(commits == 6);
    const char *first_branch[] = {"db", "redo", "1", nullptr};
    CHECK(run(gedp, first_branch) == BRLCAD_OK && commits == 7);
    CHECK(run(gedp, reference) == BRLCAD_OK && contains_result(gedp, "outline.s"));

    const char *fork_checkout[] = {"db", "history", "checkout", "2", nullptr};
    const char *rename_checkout[] = {"db", "history", "checkout", "1", nullptr};
    CHECK(run(gedp, fork_checkout) == BRLCAD_OK && commits == 8);
    CHECK(run(gedp, reference) == BRLCAD_OK && contains_result(gedp, old_name.c_str()));
    CHECK(run(gedp, rename_checkout) == BRLCAD_OK && commits == 9);
    CHECK(run(gedp, reference) == BRLCAD_OK && contains_result(gedp, "outline.s"));

    // A different writer may change the file while a session is idle.
    const std::unique_ptr<struct ged, decltype(&ged_close)> external(ged_open("db", path, 1), ged_close);
    const char *external_shape[] = {"make", "external.s", "sph", nullptr};
    CHECK(external && run(external.get(), title) == BRLCAD_OK && run(external.get(), external_shape) == BRLCAD_OK);
    CHECK(run(gedp, verify_session) == BRLCAD_ERROR && contains_result(gedp, "changed since opening") && commits == 9);
    CHECK(run(gedp, undo) != BRLCAD_OK && contains_result(gedp, "changed since staging began"));
    char recovered_path[MAXPATHLEN] = {};
    file = bu_temp_file(recovered_path, sizeof(recovered_path));
    CHECK(file && fclose(file) == 0 && bu_file_delete(recovered_path));
    const char *save_conflict[] = {"db", "history", "conflict", "export", recovered_path, nullptr};
    CHECK(run(gedp, save_conflict) != BRLCAD_OK && contains_result(gedp, "original file contents changed") &&
	!bu_file_exists(recovered_path, nullptr));
    CHECK(commits == 9);
    const char *quit[] = {"quit", nullptr};
    CHECK(run(gedp, quit) == GED_EXIT);
    const char *close[] = {"db", "close", nullptr};
    CHECK(run(gedp, close) == BRLCAD_OK);
    CHECK(run(gedp, status) == BRLCAD_OK && contains_result(gedp, "mode disabled"));

    // An edit that exceeds the byte limit must leave the file intact.
    const std::unique_ptr<struct ged, decltype(&ged_close)> limited(ged_open("db", path, 1), ged_close);
    const char *small_budget[] = {"db", "history", "enable", "session", "1", nullptr};
    const char *new_title[] = {"title", "too large", nullptr};
    const char *query_title[] = {"title", nullptr};
    CHECK(limited && run(limited.get(), small_budget) == BRLCAD_OK);
    CHECK(run(limited.get(), new_title) != BRLCAD_OK && contains_result(limited.get(), "budget"));
    CHECK(run(limited.get(), query_title) == BRLCAD_OK && contains_result(limited.get(), "fork"));

    CHECK(run(limited.get(), close) == BRLCAD_OK);
    const std::unique_ptr<struct ged, decltype(&ged_close)> bounded(ged_open("db", path, 1), ged_close);
    const char *bounded_budget[] = {"db", "history", "enable", "session", "700", nullptr};
    CHECK(bounded && run(bounded.get(), bounded_budget) == BRLCAD_OK);
    for (const char *name : {"one", "two", "three", "four", "five", "six"}) {
	const char *change_title[] = {"title", name, nullptr};
	CHECK(run(bounded.get(), change_title) == BRLCAD_OK);
    }
    CHECK(run(bounded.get(), status) == BRLCAD_OK && contains_result(bounded.get(), "revision 6"));
    CHECK(!contains_result(bounded.get(), "oldest 0"));
    const std::string before_compaction = bu_vls_cstr(bounded->ged_result_str);
    const char *compact[] = {"garbage_collect", "-c", nullptr};
    CHECK(run(bounded.get(), compact) == BRLCAD_OK && contains_result(bounded.get(), "Compacted DB5"));
    CHECK(run(bounded.get(), status) == BRLCAD_OK && before_compaction == bu_vls_cstr(bounded->ged_result_str));
    CHECK(run(bounded.get(), verify_session) == BRLCAD_OK && contains_result(bounded.get(), "verified"));
    const char *expired[] = {"db", "history", "checkout", "0", nullptr};
    CHECK(run(bounded.get(), expired) != BRLCAD_OK && contains_result(bounded.get(), "expired"));
    CHECK(run(bounded.get(), undo) == BRLCAD_OK);
    CHECK(run(bounded.get(), query_title) == BRLCAD_OK && contains_result(bounded.get(), "five"));

    const std::unique_ptr<struct ged, decltype(&ged_close)> stale(ged_open("db", path, 1), ged_close);
    CHECK(stale && run(stale.get(), enable) == BRLCAD_OK);
    CHECK(run(bounded.get(), title) == BRLCAD_OK);
    CHECK(run(stale.get(), new_title) != BRLCAD_OK && contains_result(stale.get(), "conflict export"));
    CHECK(run(stale.get(), status) == BRLCAD_OK && contains_result(stale.get(), "status stale"));
    CHECK(run(stale.get(), save_conflict) == BRLCAD_OK);
    const std::unique_ptr<struct ged, decltype(&ged_close)> saved(ged_open("db", recovered_path, 1), ged_close);
    CHECK(saved && run(saved.get(), query_title) == BRLCAD_OK && contains_result(saved.get(), "too large"));
    CHECK(run(saved.get(), reference) == BRLCAD_OK && contains_result(saved.get(), "outline.s"));
    CHECK(db_lookup(saved->dbip, "external.s", LOOKUP_QUIET));
    CHECK(run(bounded.get(), query_title) == BRLCAD_OK && contains_result(bounded.get(), "fork"));
    return 0;
}

static std::string
selected_id(struct ged *gedp)
{
    const char *status[] = {"db", "history", "status", nullptr};
    if (run(gedp, status) != BRLCAD_OK)
	return {};
    const std::string_view output(bu_vls_addr(gedp->ged_result_str));
    const std::string_view field("\nrevision ");
    const auto offset = output.find(field);
    if (offset == std::string_view::npos)
	return {};
    const auto id = output.substr(offset + field.size());
    return std::string(id.substr(0, id.find('\n')));
}

static int
test_embedded_history()
{
    char path[MAXPATHLEN] = {};
    FILE *file = bu_temp_file(path, sizeof(path));
    CHECK(file && fclose(file) == 0);
    using Db = std::unique_ptr<struct db_i, decltype(&db_close)>;
    using Ged = std::unique_ptr<struct ged, decltype(&ged_close)>;
    Db initial(db_create(path, 5), db_close);
    CHECK(initial);
    initial.reset();
    Ged owner(ged_open("db", path, 1), ged_close);
    auto *gedp = owner.get();
    CHECK(gedp);

    const char *make[] = {"make", "extrusion.s", "extrude", nullptr};
    const char *reference[] = {"get", "extrusion.s", "S", nullptr};
    CHECK(run(gedp, make) == BRLCAD_OK && run(gedp, reference) == BRLCAD_OK);
    const std::string old_name = bu_vls_addr(gedp->ged_result_str);
    const char *group[] = {"g", "assembly.c", old_name.c_str(), "extrusion.s", nullptr};
    const char *status[] = {"db", "history", "status", nullptr};
    const char *enable[] = {"db", "history", "enable", nullptr};
    const char *rename[] = {"db", "mvall", old_name.c_str(), "outline.s", nullptr};
    const char *undo[] = {"db", "undo", nullptr};
    const char *redo[] = {"db", "redo", nullptr};
    CHECK(run(gedp, group) == BRLCAD_OK);
    const int enabled = run(gedp, enable);
    if (enabled != BRLCAD_OK && contains_result(gedp, "Atomic DB5 replacement is unavailable"))
	return 0;
    CHECK(enabled == BRLCAD_OK && contains_result(gedp, "mode embedded") && contains_result(gedp, "status current"));
    const std::string root = selected_id(gedp);
    CHECK(!root.empty());
    CHECK(run(gedp, enable) == BRLCAD_OK && selected_id(gedp) == root);

    int commits = 0;
    int failed_writes = 0;
    CHECK(ged_event_observer_add(gedp, GED_EVENT_OBSERVER_POST_RECONCILE, count_commit, &commits));
    CHECK(ged_clbk_set(gedp, "mvall", BU_CLBK_PRE, fail_staged_edit, &failed_writes) == BRLCAD_OK);
    CHECK(run(gedp, rename) != BRLCAD_OK && failed_writes == 1 && commits == 0);
    CHECK(selected_id(gedp) == root);
    CHECK(ged_clbk_set(gedp, "mvall", BU_CLBK_PRE, nullptr, nullptr) == GED_OVERRIDE);
    CHECK(run(gedp, rename) == BRLCAD_OK && commits == 1);
    CHECK(run(gedp, reference) == BRLCAD_OK && contains_result(gedp, "outline.s"));
    const std::string head = selected_id(gedp);
    CHECK(!head.empty() && head != root);
    const std::string prefix = head.substr(0, head.size() / 2);
    const char *show[] = {"db", "history", "show", prefix.c_str(), nullptr};
    CHECK(run(gedp, show) == BRLCAD_OK && contains_result(gedp, "create outline.s"));
    CHECK(contains_result(gedp, ("rename " + old_name + " -> outline.s").c_str()));
    CHECK(contains_result(gedp, "update assembly.c") && contains_result(gedp, "update extrusion.s"));
    const char *mark[] = {"db", "history", "mark", "rename-point", nullptr};
    CHECK(run(gedp, mark) == BRLCAD_OK && selected_id(gedp) == head);
    CHECK(run(gedp, mark) != BRLCAD_OK && contains_result(gedp, "already exists"));

    owner.reset(ged_open("db", path, 1));
    gedp = owner.get();
    CHECK(gedp && selected_id(gedp) == head);
    const char *diff[] = {"db", "history", "diff", root.c_str(), "rename-point", "--object", "extrusion.s", nullptr};
    CHECK(run(gedp, diff) == BRLCAD_OK && contains_result(gedp, "update extrusion.s"));
    CHECK(!contains_result(gedp, "outline.s"));
    const char *log[] = {"db", "history", "log", "--object", "assembly.c", nullptr};
    CHECK(run(gedp, log) == BRLCAD_OK && contains_result(gedp, head.c_str()));
    CHECK(run(gedp, undo) == BRLCAD_OK && selected_id(gedp) == root);
    CHECK(run(gedp, reference) == BRLCAD_OK && contains_result(gedp, old_name.c_str()));

    // Reopen through db open and exercise the archived redo choice.
    const char *close[] = {"db", "close", nullptr};
    const char *open[] = {"db", "open", path, nullptr};
    CHECK(run(gedp, close) == BRLCAD_OK && run(gedp, open) == BRLCAD_OK);
    CHECK(run(gedp, status) == BRLCAD_OK && contains_result(gedp, "(preferred)"));
    CHECK(run(gedp, redo) == BRLCAD_OK && selected_id(gedp) == head);
    CHECK(run(gedp, undo) == BRLCAD_OK && selected_id(gedp) == root);
    const char *fork_title[] = {"title", "embedded branch", nullptr};
    CHECK(run(gedp, fork_title) == BRLCAD_OK);
    const auto fork = selected_id(gedp);
    CHECK(!fork.empty() && fork != head && fork != root);
    CHECK(run(gedp, fork_title) == BRLCAD_OK && selected_id(gedp) == fork);
    CHECK(run(gedp, undo) == BRLCAD_OK && selected_id(gedp) == root);
    const char *choose[] = {"db", "redo", "rename-point", nullptr};
    CHECK(run(gedp, choose) == BRLCAD_OK && selected_id(gedp) == head);
    const char *checkout[] = {"db", "history", "checkout", fork.c_str(), nullptr};
    CHECK(run(gedp, checkout) == BRLCAD_OK && selected_id(gedp) == fork);
    CHECK(run(gedp, reference) == BRLCAD_OK && contains_result(gedp, old_name.c_str()));

    Ged inspector(ged_create(), ged_close);
    const char *open_readonly[] = {"db", "open", "-r", path, nullptr};
    CHECK(inspector && run(inspector.get(), open_readonly) == BRLCAD_OK);
    CHECK(run(inspector.get(), status) == BRLCAD_OK && contains_result(inspector.get(), "access read-only"));
    CHECK(run(inspector.get(), show) == BRLCAD_OK);
    Db preview_mark(ged_history_open_revision(inspector.get(), "rename-point", nullptr), db_close);
    CHECK(preview_mark && preview_mark->dbi_read_only &&
	db_lookup(preview_mark.get(), "outline.s", LOOKUP_QUIET) && selected_id(inspector.get()) == fork);
    const char *verify[] = {"db", "history", "verify", nullptr};
    CHECK(run(inspector.get(), verify) == BRLCAD_OK && contains_result(inspector.get(), "Archive verified"));
    CHECK(run(inspector.get(), undo) != BRLCAD_OK && contains_result(inspector.get(), "open read-only"));
    const char *compact[] = {"garbage_collect", "-c", nullptr};
    CHECK(run(inspector.get(), compact) != BRLCAD_OK && contains_result(inspector.get(), "open read-only"));
    const char *missing_path[] = {"db", "history", "export", "--with-history", nullptr};
    CHECK(run(inspector.get(), missing_path) == GED_HELP);
    char archive_copy[MAXPATHLEN] = {};
    file = bu_temp_file(archive_copy, sizeof(archive_copy));
    CHECK(file && fclose(file) == 0);
    const char *export_archive[] = {"db", "history", "export", archive_copy, "--with-history", nullptr};
    CHECK(run(inspector.get(), export_archive) != BRLCAD_OK && contains_result(inspector.get(), "already exists"));
    CHECK(bu_file_delete(archive_copy));
    const char *dump_archive[] = {"dump", archive_copy, nullptr};
    CHECK(run(inspector.get(), dump_archive) == BRLCAD_OK);
    Ged archived(ged_open("db", archive_copy, 1), ged_close);
    CHECK(archived && selected_id(archived.get()) == fork && run(archived.get(), show) == BRLCAD_OK);
    const char *full_log[] = {"db", "history", "log", "--json", nullptr};
    CHECK(run(archived.get(), full_log) == BRLCAD_OK);
    const std::string compacted_log = bu_vls_cstr(archived->ged_result_str);
    archived.reset();

    // Insert physical free space without changing the portable model.
    const auto packed_size = std::filesystem::file_size(archive_copy);
    constexpr size_t free_bytes = 4 * 1024;
    struct bu_external free_record = BU_EXTERNAL_INIT_ZERO;
    db5_make_free_object(&free_record, free_bytes);
    file = fopen(archive_copy, "ab");
    CHECK(file && bu_fwrite_external(file, &free_record) == 0 && fclose(file) == 0);
    bu_free_external(&free_record);
    CHECK(std::filesystem::file_size(archive_copy) == packed_size + free_bytes);
    archived.reset(ged_open("db", archive_copy, 1));
    CHECK(archived && run(archived.get(), compact) == BRLCAD_OK && contains_result(archived.get(), "Compacted DB5"));
    CHECK(std::filesystem::file_size(archive_copy) == packed_size && selected_id(archived.get()) == fork &&
	run(archived.get(), full_log) == BRLCAD_OK && compacted_log == bu_vls_cstr(archived->ged_result_str));
    CHECK(run(archived.get(), verify) == BRLCAD_OK && run(archived.get(), undo) == BRLCAD_OK &&
	selected_id(archived.get()) == root && run(archived.get(), redo) == BRLCAD_OK && selected_id(archived.get()) == fork);
    inspector.reset();

    // The legacy writer changes only the live model. Reopening protects
    // both the archived graph and the live contents from further edits.
    Db legacy(db_open(path, DB_OPEN_READWRITE), db_close);
    CHECK(legacy && db_dirbuild(legacy.get()) == 0 && db5_update_ident(legacy.get(), "legacy title", 1.0) == 0);
    auto *old_sketch = db_lookup(legacy.get(), old_name.c_str(), LOOKUP_QUIET);
    struct bu_external copied = BU_EXTERNAL_INIT_ZERO;
    CHECK(old_sketch && db_get_external(&copied, old_sketch, legacy.get()) == 0);
    CHECK(wdb_export_external(wdb_dbopen(legacy.get(), RT_WDB_TYPE_DB_DEFAULT), &copied,
	"imported.s", old_sketch->d_flags, old_sketch->d_minor_type) == 0);
    bu_free_external(&copied);
    auto *old_combination = db_lookup(legacy.get(), "assembly.c", LOOKUP_QUIET);
    CHECK(old_combination && db_delete(legacy.get(), old_combination) == 0 && db_dirdelete(legacy.get(), old_combination) == 0);
    db_sync(legacy.get());
    legacy.reset();
    CHECK(run(gedp, verify) != BRLCAD_OK && contains_result(gedp, "changed since opening"));
    CHECK(run(gedp, close) == BRLCAD_OK && run(gedp, open) == BRLCAD_OK);
    CHECK(run(gedp, status) == BRLCAD_OK && contains_result(gedp, "status external edits"));
    CHECK(run(gedp, verify) != BRLCAD_OK && contains_result(gedp, "Archive verified") && contains_result(gedp, "external edits"));
    Db preview_archived(ged_history_open_revision(gedp, fork.c_str(), nullptr), db_close);
    CHECK(preview_archived && BU_STR_EQUAL(preview_archived->dbi_title, "embedded branch") &&
	db_lookup(preview_archived.get(), "assembly.c", LOOKUP_QUIET) &&
	!db_lookup(preview_archived.get(), "imported.s", LOOKUP_QUIET));
    const char *preview[] = {"db", "history", "adopt", nullptr};
    CHECK(run(gedp, preview) == BRLCAD_OK && contains_result(gedp, "update _GLOBAL") &&
	contains_result(gedp, "create imported.s") && contains_result(gedp, "delete assembly.c"));
    CHECK(selected_id(gedp) == fork);
    CHECK(selected_id(gedp) == fork && run(gedp, show) == BRLCAD_OK);
    CHECK(run(gedp, rename) != BRLCAD_OK && contains_result(gedp, "read-only (external edits)"));
    const char *title[] = {"title", nullptr};
    CHECK(run(gedp, title) == BRLCAD_OK && contains_result(gedp, "legacy title"));
    char plain_copy[MAXPATHLEN] = {};
    file = bu_temp_file(plain_copy, sizeof(plain_copy));
    CHECK(file && fclose(file) == 0 && bu_file_delete(plain_copy));
    const char *export_plain[] = {"db", "history", "export", plain_copy, nullptr};
    CHECK(run(gedp, export_plain) == BRLCAD_OK);
    Ged recovered(ged_open("db", plain_copy, 1), ged_close);
    CHECK(recovered && run(recovered.get(), status) == BRLCAD_OK && contains_result(recovered.get(), "mode disabled"));
    CHECK(run(recovered.get(), title) == BRLCAD_OK && contains_result(recovered.get(), "legacy title"));
    CHECK(run(gedp, status) == BRLCAD_OK && contains_result(gedp, "status external edits"));
    const char *adopt[] = {"db", "history", "adopt", "--apply", nullptr};
    CHECK(run(gedp, adopt) == BRLCAD_OK && contains_result(gedp, "Adopted external DB5 edits"));
    const auto adopted = selected_id(gedp);
    CHECK(!adopted.empty() && adopted != fork);
    CHECK(run(gedp, verify) == BRLCAD_OK && contains_result(gedp, "matches"));
    CHECK(run(gedp, undo) == BRLCAD_OK && selected_id(gedp) == fork);
    CHECK(run(gedp, title) == BRLCAD_OK && contains_result(gedp, "embedded branch"));
    CHECK(db_lookup(gedp->dbip, "assembly.c", LOOKUP_QUIET) && !db_lookup(gedp->dbip, "imported.s", LOOKUP_QUIET));
    CHECK(run(gedp, close) == BRLCAD_OK && run(gedp, open) == BRLCAD_OK);
    CHECK(run(gedp, redo) == BRLCAD_OK && selected_id(gedp) == adopted);
    CHECK(run(gedp, title) == BRLCAD_OK && contains_result(gedp, "legacy title"));
    CHECK(!db_lookup(gedp->dbip, "assembly.c", LOOKUP_QUIET) && db_lookup(gedp->dbip, "imported.s", LOOKUP_QUIET));

    Ged stale(ged_open("db", path, 1), ged_close);
    CHECK(stale && selected_id(stale.get()) == adopted);
    CHECK(run(gedp, fork_title) == BRLCAD_OK);
    const auto committed = selected_id(gedp);
    const char *staged_title[] = {"title", "unpublished embedded title", nullptr};
    CHECK(run(stale.get(), staged_title) != BRLCAD_OK && contains_result(stale.get(), "conflict export"));
    CHECK(run(stale.get(), status) == BRLCAD_OK && contains_result(stale.get(), "status stale") &&
	contains_result(stale.get(), "staged conflict"));
    const char *conflict[] = {"db", "history", "conflict", nullptr};
    CHECK(run(stale.get(), conflict) == BRLCAD_OK && contains_result(stale.get(), "update _GLOBAL"));
    const char *existing_conflict_export[] = {"db", "history", "conflict", "export", path, nullptr};
    CHECK(run(stale.get(), existing_conflict_export) != BRLCAD_OK && contains_result(stale.get(), "already exists"));
    char recovered_path[MAXPATHLEN] = {};
    file = bu_temp_file(recovered_path, sizeof(recovered_path));
    CHECK(file && fclose(file) == 0 && bu_file_delete(recovered_path));
    const char *save_conflict[] = {"db", "history", "conflict", "export", recovered_path, nullptr};
    CHECK(run(stale.get(), save_conflict) == BRLCAD_OK);
    Ged recovered_conflict(ged_open("db", recovered_path, 1), ged_close);
    CHECK(recovered_conflict && run(recovered_conflict.get(), verify) == BRLCAD_OK &&
	run(recovered_conflict.get(), title) == BRLCAD_OK && contains_result(recovered_conflict.get(), "unpublished embedded title"));
    CHECK(run(recovered_conflict.get(), undo) == BRLCAD_OK && selected_id(recovered_conflict.get()) == adopted);
    Ged original(ged_open("db", path, 1), ged_close);
    CHECK(original && selected_id(original.get()) == committed && selected_id(stale.get()) == adopted);
    return 0;
}

static int
test_session_promotion()
{
    char path[MAXPATHLEN] = {};
    FILE *file = bu_temp_file(path, sizeof(path));
    CHECK(file && fclose(file) == 0);
    auto *initial = db_create(path, 5);
    CHECK(initial);
    db_close(initial);
    std::unique_ptr<struct ged, decltype(&ged_close)> owner(ged_open("db", path, 1), ged_close);
    auto *gedp = owner.get();
    CHECK(gedp);

    const char *make[] = {"make", "branch.s", "sph", nullptr};
    CHECK(run(gedp, make) == BRLCAD_OK);
    const char *session[] = {"db", "history", "enable", "session", nullptr};
    const int enabled = run(gedp, session);
    if (enabled != BRLCAD_OK && contains_result(gedp, "Atomic DB5 replacement is unavailable"))
	return 0;
    CHECK(enabled == BRLCAD_OK);
    const char *first[] = {"title", "first branch", nullptr};
    const char *rename[] = {"mv", "branch.s", "renamed.s", nullptr};
    const char *second[] = {"title", "second branch", nullptr};
    const char *undo[] = {"db", "undo", nullptr};
    const char *redo[] = {"db", "redo", nullptr};
    const char *embedded[] = {"db", "history", "enable", "embedded", nullptr};
    const char *status[] = {"db", "history", "status", nullptr};
    const char *title[] = {"title", nullptr};
    CHECK(run(gedp, first) == BRLCAD_OK && run(gedp, rename) == BRLCAD_OK);
    const auto first_head = selected_id(gedp);
    CHECK(run(gedp, undo) == BRLCAD_OK && run(gedp, undo) == BRLCAD_OK);
    const auto session_root = selected_id(gedp);
    CHECK(run(gedp, second) == BRLCAD_OK);
    const auto second_head = selected_id(gedp);
    CHECK(run(gedp, undo) == BRLCAD_OK && selected_id(gedp) == session_root);
    const char *diff[] = {"db", "history", "diff", first_head.c_str(), second_head.c_str(), nullptr};
    CHECK(run(gedp, diff) == BRLCAD_OK && contains_result(gedp, "create branch.s") &&
	contains_result(gedp, "delete renamed.s") && contains_result(gedp, "update _GLOBAL"));
    const char *filtered[] = {"db", "history", "diff", first_head.c_str(), second_head.c_str(),
	"--object", "branch.s", nullptr};
    CHECK(run(gedp, filtered) == BRLCAD_OK && contains_result(gedp, "create branch.s") &&
	!contains_result(gedp, "delete renamed.s") && !contains_result(gedp, "update _GLOBAL") &&
	selected_id(gedp) == session_root);
    char archive_copy[MAXPATHLEN] = {};
    file = bu_temp_file(archive_copy, sizeof(archive_copy));
    CHECK(file && fclose(file) == 0 && bu_file_delete(archive_copy));
    const char *export_archive[] = {"db", "history", "export", "--with-history", archive_copy, nullptr};
    CHECK(run(gedp, export_archive) == BRLCAD_OK);
    CHECK(run(gedp, status) == BRLCAD_OK && contains_result(gedp, "mode session"));
    const std::unique_ptr<struct ged, decltype(&ged_close)> copied(ged_open("db", archive_copy, 1), ged_close);
    CHECK(copied && run(copied.get(), redo) == BRLCAD_OK);
    CHECK(run(copied.get(), title) == BRLCAD_OK && contains_result(copied.get(), "second branch"));
    CHECK(run(gedp, embedded) == BRLCAD_OK && contains_result(gedp, "mode embedded"));
    const auto root = selected_id(gedp);
    CHECK(!root.empty());

    owner.reset(ged_open("db", path, 1));
    gedp = owner.get();
    CHECK(gedp && selected_id(gedp) == root);
    CHECK(run(gedp, status) == BRLCAD_OK && contains_result(gedp, "title second branch (preferred)"));
    const std::string result = bu_vls_addr(gedp->ged_result_str);
    const auto label = result.find(" title first branch");
    CHECK(label != std::string::npos);
    const auto line = result.rfind("\nredo ", label);
    CHECK(line != std::string::npos);
    const std::string_view tag("\nredo ");
    const auto first_id = result.substr(line + tag.size(), label - line - tag.size());
    CHECK(run(gedp, redo) == BRLCAD_OK && run(gedp, title) == BRLCAD_OK && contains_result(gedp, "second branch"));
    CHECK(run(gedp, undo) == BRLCAD_OK && selected_id(gedp) == root);
    const char *other[] = {"db", "redo", first_id.c_str(), nullptr};
    CHECK(run(gedp, other) == BRLCAD_OK && run(gedp, title) == BRLCAD_OK && contains_result(gedp, "first branch"));
    CHECK(run(gedp, redo) == BRLCAD_OK && db_lookup(gedp->dbip, "renamed.s", LOOKUP_QUIET));
    const std::string renamed = selected_id(gedp);
    const char *show[] = {"db", "history", "show", renamed.c_str(), nullptr};
    CHECK(run(gedp, show) == BRLCAD_OK && contains_result(gedp, "rename branch.s -> renamed.s"));
    return 0;
}

static int
reject_reference_cleanup(int UNUSED(argc), const char **UNUSED(argv), void *ged_context, void *data)
{
    auto *gedp = static_cast<struct ged *>(ged_context);
    *static_cast<bool *>(data) = !db_lookup(gedp->dbip, "trunk.c", LOOKUP_QUIET) &&
	!db_lookup(gedp->dbip, "node.s", LOOKUP_QUIET);
    return BRLCAD_ERROR;
}

static int
test_reference_removal(struct ged *gedp)
{
    const char *make[] = {"make", "node.s", "sph", nullptr};
    const char *trunk[] = {"g", "trunk.c", "node.s", "node.s", nullptr};
    const char *outside[] = {"g", "outside.c", "trunk.c", "node.s", nullptr};
    CHECK(run(gedp, make) == BRLCAD_OK && run(gedp, trunk) == BRLCAD_OK && run(gedp, outside) == BRLCAD_OK);
    const auto root = selected_id(gedp);

    int commits = 0;
    const auto observer = ged_event_observer_add(gedp, GED_EVENT_OBSERVER_POST_RECONCILE, count_commit, &commits);
    CHECK(observer);
    const char *refs_preview[] = {"killrefs", "-n", "node.s", nullptr};
    const char *all_preview[] = {"killall", "-n", "node.s", nullptr};
    const char *tree_preview[] = {"killtree", "-an", "trunk.c", nullptr};
    const std::unique_ptr<struct ged, decltype(&ged_close)> inspector(ged_create(), ged_close);
    const char *open[] = {"db", "open", "-r", gedp->dbip->dbi_filename, nullptr};
    CHECK(inspector && run(inspector.get(), open) == BRLCAD_OK);
    for (auto *reader : {gedp, inspector.get()}) {
	CHECK(run(reader, refs_preview) == BRLCAD_OK && contains_result(reader, "trunk.c") &&
	    contains_result(reader, "outside.c"));
	CHECK(run(reader, all_preview) == BRLCAD_OK && contains_result(reader, "{node.s } {") &&
	    contains_result(reader, "trunk.c") && contains_result(reader, "outside.c"));
	CHECK(BU_STR_EQUAL(all_preview[0], "killall"));
	CHECK(run(reader, tree_preview) == BRLCAD_OK && contains_result(reader, "node.s") &&
	    contains_result(reader, "trunk.c") && contains_result(reader, "outside.c"));
    }
    CHECK(selected_id(gedp) == root && commits == 0);
    const auto has_original_refs = [gedp]() {
	db_update_nref(gedp->dbip);
	auto *node = db_lookup(gedp->dbip, "node.s", LOOKUP_QUIET);
	auto *branch = db_lookup(gedp->dbip, "trunk.c", LOOKUP_QUIET);
	auto *parent = db_lookup(gedp->dbip, "outside.c", LOOKUP_QUIET);
	return node && branch && parent && node->d_nref == 3 && branch->d_nref == 1 &&
	    db_comb_has_instance(nullptr, gedp->dbip, branch, node, 0) &&
	    db_comb_has_instance(nullptr, gedp->dbip, parent, node, 0) &&
	    db_comb_has_instance(nullptr, gedp->dbip, parent, branch, 0);
    };
    CHECK(has_original_refs());

    const char *remove_all[] = {"killall", "node.s", nullptr};
    int staged_writes = 0;
    CHECK(ged_clbk_set(gedp, "killall", BU_CLBK_PRE, fail_staged_edit, &staged_writes) == BRLCAD_OK);
    CHECK(run(gedp, remove_all) == BRLCAD_ERROR && staged_writes == 1);
    CHECK(selected_id(gedp) == root && commits == 0 && has_original_refs());
    CHECK(ged_clbk_set(gedp, "killall", BU_CLBK_PRE, nullptr, nullptr) == GED_OVERRIDE);

    const char *remove_tree[] = {"killtree", "-a", "trunk.c", nullptr};
    bool tree_staged = false;
    CHECK(ged_clbk_set(gedp, "killrefs", BU_CLBK_PRE, reject_reference_cleanup, &tree_staged) == BRLCAD_OK);
    CHECK(run(gedp, remove_tree) == BRLCAD_ERROR && tree_staged);
    CHECK(selected_id(gedp) == root && commits == 0 && has_original_refs());
    CHECK(ged_clbk_set(gedp, "killrefs", BU_CLBK_PRE, nullptr, nullptr) == GED_OVERRIDE);

    const char *empty[] = {"lt", "outside.c", nullptr};
    const char *undo[] = {"db", "undo", nullptr};
    const char *remove_refs[] = {"killrefs", "node.s", nullptr};
    CHECK(run(gedp, remove_refs) == BRLCAD_OK && commits == 1 && selected_id(gedp) != root);
    db_update_nref(gedp->dbip);
    auto *node = db_lookup(gedp->dbip, "node.s", LOOKUP_QUIET);
    CHECK(node && node->d_nref == 0);
    CHECK(run(gedp, remove_refs) == BRLCAD_OK && commits == 1);
    CHECK(run(gedp, undo) == BRLCAD_OK && commits == 2 && selected_id(gedp) == root && has_original_refs());

    CHECK(run(gedp, remove_all) == BRLCAD_OK && commits == 3 && selected_id(gedp) != root);
    CHECK(!db_lookup(gedp->dbip, "node.s", LOOKUP_QUIET) && db_lookup(gedp->dbip, "trunk.c", LOOKUP_QUIET));
    CHECK(run(gedp, empty) == BRLCAD_OK && contains_result(gedp, "trunk.c") && !contains_result(gedp, "node.s"));
    CHECK(run(gedp, undo) == BRLCAD_OK && commits == 4 && selected_id(gedp) == root && has_original_refs());

    CHECK(run(gedp, remove_tree) == BRLCAD_OK && commits == 5 && selected_id(gedp) != root);
    const auto removed = selected_id(gedp);
    CHECK(!db_lookup(gedp->dbip, "node.s", LOOKUP_QUIET) && !db_lookup(gedp->dbip, "trunk.c", LOOKUP_QUIET));
    CHECK(run(gedp, empty) == BRLCAD_OK && !contains_result(gedp, "node.s") && !contains_result(gedp, "trunk.c"));
    CHECK(run(gedp, undo) == BRLCAD_OK && commits == 6 && selected_id(gedp) == root && has_original_refs());
    const char *redo_tree[] = {"db", "redo", removed.c_str(), nullptr};
    CHECK(run(gedp, redo_tree) == BRLCAD_OK && commits == 7 && selected_id(gedp) == removed &&
	!db_lookup(gedp->dbip, "node.s", LOOKUP_QUIET) && !db_lookup(gedp->dbip, "trunk.c", LOOKUP_QUIET));

    const char *option_name[] = {"mv", "renamed.s", "-n", nullptr};
    CHECK(run(gedp, option_name) == BRLCAD_OK);
    const char *reference_option_name[] = {"g", "option.c", "-n", nullptr};
    CHECK(run(gedp, reference_option_name) == BRLCAD_OK);
    const char *remove_option_name[] = {"killall", "--", "-n", nullptr};
    CHECK(run(gedp, remove_option_name) == BRLCAD_OK && !db_lookup(gedp->dbip, "-n", LOOKUP_QUIET));
    const char *show_option[] = {"lt", "option.c", nullptr};
    CHECK(run(gedp, show_option) == BRLCAD_OK && !contains_result(gedp, "-n"));

    const char *cycle_a[] = {"g", "cycle-a.c", "outside.c", nullptr};
    const char *cycle_b[] = {"g", "cycle-b.c", "cycle-a.c", nullptr};
    const char *back_edge[] = {"g", "cycle-a.c", "cycle-b.c", nullptr};
    CHECK(run(gedp, cycle_a) == BRLCAD_OK && run(gedp, cycle_b) == BRLCAD_OK && run(gedp, back_edge) == BRLCAD_OK);
    const auto cycle = selected_id(gedp);
    const char *cycle_preview[] = {"killtree", "-an", "cycle-a.c", nullptr};
    CHECK(run(gedp, cycle_preview) == BRLCAD_OK && contains_result(gedp, "{outside.c cycle-b.c cycle-a.c } {"));
    CHECK(selected_id(gedp) == cycle);
    const char *erase_cycle[] = {"killtree", "-a", "cycle-a.c", nullptr};
    CHECK(run(gedp, erase_cycle) == BRLCAD_OK && selected_id(gedp) != cycle);
    CHECK(!db_lookup(gedp->dbip, "cycle-a.c", LOOKUP_QUIET) && !db_lookup(gedp->dbip, "cycle-b.c", LOOKUP_QUIET));
    CHECK(run(gedp, undo) == BRLCAD_OK && selected_id(gedp) == cycle);
    const char *show_cycle_a[] = {"lt", "cycle-a.c", nullptr};
    const char *show_cycle_b[] = {"lt", "cycle-b.c", nullptr};
    CHECK(run(gedp, show_cycle_a) == BRLCAD_OK && contains_result(gedp, "cycle-b.c"));
    CHECK(run(gedp, show_cycle_b) == BRLCAD_OK && contains_result(gedp, "cycle-a.c"));
    CHECK(ged_event_observer_remove(gedp, observer) == 1);
    return 0;
}

static int
test_versioned_region_defaults(struct ged *gedp)
{
    const auto root = selected_id(gedp);
    const char *show_defaults[] = {"regdef", nullptr};
    const char *set_defaults[] = {"regdef", "4800", "0", "75", "9", nullptr};
    CHECK(run(gedp, set_defaults) == BRLCAD_OK && selected_id(gedp) == root);
    const auto defaults_are = [gedp, &show_defaults](const char *expected) {
	return run(gedp, show_defaults) == BRLCAD_OK &&
	    BU_STR_EQUAL(bu_vls_cstr(gedp->ged_result_str), expected);
    };
    const char *defaults = "ident 4800 air 0 los 75 material 9";
    CHECK(defaults_are(defaults));
    const char *invalid_defaults[] = {"regdef", "19", "0", "75", "invalid", nullptr};
    CHECK(run(gedp, invalid_defaults) == BRLCAD_ERROR && defaults_are(defaults));

    const char *failed_region[] = {"r", "incomplete.r", "u", "renamed.s", "u", "renamed.s", nullptr};
    int staged_writes = 0;
    CHECK(ged_clbk_set(gedp, "r", BU_CLBK_PRE, fail_staged_edit, &staged_writes) == BRLCAD_OK);
    CHECK(run(gedp, failed_region) == BRLCAD_ERROR && staged_writes == 1);
    CHECK(ged_clbk_set(gedp, "r", BU_CLBK_PRE, nullptr, nullptr) == GED_OVERRIDE);
    CHECK(!db_lookup(gedp->dbip, "incomplete.r", LOOKUP_QUIET) && selected_id(gedp) == root &&
	defaults_are(defaults));

    const char *failed_batch[] = {"db", "history", "batch", "Discard construction",
	"{regdef 6350 0 89 27} {r transient.r u renamed.s} {mv transient.r assembly.c}", nullptr};
    CHECK(run(gedp, failed_batch) == BRLCAD_ERROR && contains_result(gedp, "Batch command 3 failed"));
    CHECK(!db_lookup(gedp->dbip, "transient.r", LOOKUP_QUIET) && selected_id(gedp) == root &&
	defaults_are(defaults));

    const char *batch[] = {"db", "history", "batch", "Build two regions",
	"{regdef 6350 0 89 27} {r first.r u renamed.s} {region second.r u renamed.s}", nullptr};
    CHECK(run(gedp, batch) == BRLCAD_OK);
    const auto tip = selected_id(gedp);
    CHECK(tip != root);
    const char *advanced = "ident 6352 air 0 los 89 material 27";
    CHECK(defaults_are(advanced));
    const auto region_matches = [gedp](const char *name, int ident) {
	auto *dp = db_lookup(gedp->dbip, name, LOOKUP_QUIET);
	struct rt_db_internal intern = RT_DB_INTERNAL_INIT_ZERO;
	if (!dp || rt_db_get_internal(&intern, dp, gedp->dbip, nullptr) < 0)
	    return false;
	bool valid = false;
	if (intern.idb_type == ID_COMBINATION) {
	    const auto *comb = static_cast<const struct rt_comb_internal *>(intern.idb_ptr);
	    valid = comb->region_flag && comb->region_id == ident && comb->aircode == 0 &&
		comb->los == 89 && comb->GIFTmater == 27;
	}
	rt_db_free_internal(&intern);
	return valid;
    };
    CHECK(region_matches("first.r", 6350) && region_matches("second.r", 6351));

    const char *undo[] = {"db", "undo", nullptr};
    const char *redo[] = {"db", "redo", nullptr};
    CHECK(run(gedp, undo) == BRLCAD_OK && selected_id(gedp) == root && defaults_are(advanced) &&
	!db_lookup(gedp->dbip, "first.r", LOOKUP_QUIET) && !db_lookup(gedp->dbip, "second.r", LOOKUP_QUIET));
    CHECK(run(gedp, redo) == BRLCAD_OK && selected_id(gedp) == tip && defaults_are(advanced) &&
	region_matches("first.r", 6350) && region_matches("second.r", 6351));

    const char *bad_expression[] = {"db", "history", "batch", "Discard invalid expression",
	"{regdef 9410} {c invalid.c renamed.s ( renamed.s u renamed.s ) u}", nullptr};
    CHECK(run(gedp, bad_expression) == BRLCAD_ERROR && contains_result(gedp, "missing operator"));
    CHECK(selected_id(gedp) == tip && defaults_are(advanced) &&
	!db_lookup(gedp->dbip, "invalid.c", LOOKUP_QUIET));
    const char *missing_combination[] = {"c", "-r", "missing.c", nullptr};
    CHECK(run(gedp, missing_combination) == BRLCAD_ERROR && selected_id(gedp) == tip);

    const char *expressions[] = {"db", "history", "batch", "Build Boolean regions",
	"{c -r third.r ( renamed.s u renamed.s )} {c -c converted.c renamed.s} {comb_std -r converted.c}", nullptr};
    CHECK(run(gedp, expressions) == BRLCAD_OK);
    const auto expression_tip = selected_id(gedp);
    CHECK(expression_tip != tip && region_matches("third.r", 6352) && region_matches("converted.c", 6353));
    const char *after_expr = "ident 6354 air 0 los 89 material 27";
    CHECK(defaults_are(after_expr));
    CHECK(run(gedp, undo) == BRLCAD_OK && selected_id(gedp) == tip && defaults_are(after_expr) &&
	!db_lookup(gedp->dbip, "third.r", LOOKUP_QUIET) && !db_lookup(gedp->dbip, "converted.c", LOOKUP_QUIET));
    CHECK(run(gedp, redo) == BRLCAD_OK && selected_id(gedp) == expression_tip &&
	region_matches("third.r", 6352) && region_matches("converted.c", 6353));

    const std::unique_ptr<struct ged, decltype(&ged_close)> inspector(ged_create(), ged_close);
    const char *open[] = {"db", "open", "-r", gedp->dbip->dbi_filename, nullptr};
    CHECK(inspector && run(inspector.get(), open) == BRLCAD_OK);
    CHECK(run(inspector.get(), show_defaults) == BRLCAD_OK && contains_result(inspector.get(), "ident 1000"));
    CHECK(run(inspector.get(), set_defaults) == BRLCAD_ERROR);
    return 0;
}

static int
test_binary_asset_history(struct ged *gedp, const std::filesystem::path &directory)
{
    const auto original_file = directory / "original grid.bin";
    const auto replacement_file = directory / "replacement grid.bin";
    const auto bitmap_file = directory / "mask samples.bin";
    const auto external_file = directory / "unexpected export.bin";
    // A 2 by 2 height field in network byte order.
    const uint8_t original[] = {1, 2, 0, 4, 0, 6, 0, 8};
    const uint8_t replacement[] = {2, 1, 0, 4, 0, 6, 0, 8};
    const uint8_t bitmap[] = {1, 0, 0, 1};
    const auto write_samples = [](const auto &path, const auto &samples) {
	std::ofstream file(path, std::ios::binary);
	file.write(reinterpret_cast<const char *>(samples), sizeof(samples));
	file.close();
	return file.good();
    };
    CHECK(write_samples(original_file, original) && write_samples(replacement_file, replacement));
    CHECK(write_samples(bitmap_file, bitmap));

    const auto root = selected_id(gedp);
    const auto import = "{bo -i u nS terrain.bin {" + original_file.string() +
	"}} {put terrain.s dsp src o name terrain.bin w 2 n 2} {bo -i u C volume.bin {" +
	original_file.string() + "}} {put volume.s vol src o name volume.bin w 2 n 2 d 2"
	" size {1 1 1} mat {1 0 0 0 0 1 0 0 0 0 1 0 0 0 0 1}} {bo -i u C mask.bin {" +
	bitmap_file.string() + "}} {in mask.s ebm o mask.bin 2 2 1} {in file-mask.s ebm f mask.bin 2 2 1}"
	" {attr set mask.s sample-role mask-fixture}";
    const char *first[] = {"db", "history", "batch", "Import terrain", import.c_str(), nullptr};
    CHECK(run(gedp, first) == BRLCAD_OK);
    const auto before = selected_id(gedp);
    CHECK(before != root);
    db_update_nref(gedp->dbip);
    auto *terrain_data = db_lookup(gedp->dbip, "terrain.bin", LOOKUP_QUIET);
    auto *volume_data = db_lookup(gedp->dbip, "volume.bin", LOOKUP_QUIET);
    auto *bitmap_data = db_lookup(gedp->dbip, "mask.bin", LOOKUP_QUIET);
    CHECK(terrain_data && terrain_data->d_nref == 1 && volume_data && volume_data->d_nref == 1 &&
	bitmap_data && bitmap_data->d_nref == 1);
    const auto grid_matches = [](const struct db_i *dbip, const char *name, int type, const auto &samples) {
	auto *grid = db_lookup(dbip, name, LOOKUP_QUIET);
	struct bu_external raw = BU_EXTERNAL_INIT_ZERO;
	struct db5_raw_internal record;
	const bool matches = grid && db_get_external(&raw, grid, dbip) == 0 &&
	    db5_get_raw_internal_bounded(&record, raw.ext_buf, raw.ext_nbytes) &&
	    record.major_type == DB5_MAJORTYPE_BINARY_UNIF &&
	    record.minor_type == type &&
	    record.body.ext_nbytes == sizeof(samples) &&
	    std::memcmp(record.body.ext_buf, samples, sizeof(samples)) == 0;
	bu_free_external(&raw);
	return matches;
    };
    const auto terrain_matches = [gedp, &grid_matches](const auto &samples, const char *name = "terrain.bin",
						   const char *shape = "terrain.s") {
	if (!grid_matches(gedp->dbip, name, DB5_MINORTYPE_BINU_16BITINT_U, samples))
	    return false;

	auto *surface = db_lookup(gedp->dbip, shape, LOOKUP_QUIET);
	struct rt_db_internal intern = RT_DB_INTERNAL_INIT_ZERO;
	if (!surface || rt_db_get_internal(&intern, surface, gedp->dbip, nullptr) < 0)
	    return false;
	bool loaded = false;
	if (intern.idb_type == ID_DSP) {
	    const auto *dsp = static_cast<const struct rt_dsp_internal *>(intern.idb_ptr);
	    const auto first_sample = (static_cast<unsigned int>(samples[0]) << 8) | samples[1];
	    loaded = dsp->dsp_datasrc == RT_DSP_SRC_OBJ && BU_STR_EQUAL(bu_vls_cstr(&dsp->dsp_name), name) && dsp->dsp_buf &&
		dsp->dsp_buf[0] == first_sample;
	}
	rt_db_free_internal(&intern);
	return loaded;
    };
    CHECK(terrain_matches(original));

    const auto import_new = "{bo -i u nS terrain.bin {" + replacement_file.string() + "}}";
    const auto with_output = "{kill terrain.bin volume.bin mask.bin} " + import_new + " {bo -o {" +
	external_file.string() + "} terrain.bin}";
    const char *unsupported[] = {"db", "history", "batch", "Attempt external write", with_output.c_str(), nullptr};
    const auto inspect_missing_assets = [](int, const char **, void *context, void *data) {
	auto *view = static_cast<struct ged *>(context)->dbip;
	const auto count_missing = [](struct db_i *, struct directory *parent, struct directory *child,
				      const char *name, db_op_t, matp_t, void *counter) {
	    if (parent && !child && (BU_STR_EQUAL(name, "terrain.bin") || BU_STR_EQUAL(name, "volume.bin") ||
		BU_STR_EQUAL(name, "mask.bin")))
		++*static_cast<int *>(counter);
	};
	if (db_add_update_nref_clbk(view, count_missing, data) < 0)
	    return BRLCAD_ERROR;
	db_update_nref(view);
	return db_rm_update_nref_clbk(view, count_missing, data) < 0 ? BRLCAD_ERROR : BRLCAD_OK;
    };
    int missing_assets = 0;
    CHECK(ged_clbk_set(gedp, "bo", BU_CLBK_PRE, inspect_missing_assets, &missing_assets) == BRLCAD_OK);
    CHECK(run(gedp, unsupported) == BRLCAD_ERROR && contains_result(gedp, "Batch command 3 failed"));
    CHECK(ged_clbk_set(gedp, "bo", BU_CLBK_PRE, nullptr, nullptr) == GED_OVERRIDE);
    CHECK(missing_assets == 3 && !std::filesystem::exists(external_file) &&
	selected_id(gedp) == before && terrain_matches(original) && db_lookup(gedp->dbip, "volume.bin", LOOKUP_QUIET));

    const auto replace = "{kill terrain.bin} " + import_new;
    const char *second[] = {"db", "history", "batch", "Replace terrain samples", replace.c_str(), nullptr};
    CHECK(run(gedp, second) == BRLCAD_OK && terrain_matches(replacement));
    const auto after = selected_id(gedp);
    CHECK(after != before);
    const char *show[] = {"db", "history", "show", after.c_str(), nullptr};
    CHECK(run(gedp, show) == BRLCAD_OK && contains_result(gedp, "terrain.bin") &&
	!contains_result(gedp, "terrain.s"));
    const char *undo[] = {"db", "undo", nullptr};
    const char *redo[] = {"db", "redo", nullptr};
    CHECK(run(gedp, undo) == BRLCAD_OK && selected_id(gedp) == before && terrain_matches(original));
    CHECK(run(gedp, redo) == BRLCAD_OK && selected_id(gedp) == after && terrain_matches(replacement));

    const auto volume_matches = [gedp, &grid_matches, &original](const char *name, const char *shape = "volume.s") {
	if (!grid_matches(gedp->dbip, name, DB5_MINORTYPE_BINU_8BITINT_U, original))
	    return false;
	auto *surface = db_lookup(gedp->dbip, shape, LOOKUP_QUIET);
	struct rt_db_internal intern = RT_DB_INTERNAL_INIT_ZERO;
	if (!surface || rt_db_get_internal(&intern, surface, gedp->dbip, nullptr) < 0)
	    return false;
	bool loaded = false;
	if (intern.idb_type == ID_VOL) {
	    const auto *volume = static_cast<const struct rt_vol_internal *>(intern.idb_ptr);
	    loaded = volume->datasrc == RT_VOL_SRC_OBJ && BU_STR_EQUAL(volume->name, name) && volume->map;
	}
	rt_db_free_internal(&intern);
	return loaded;
    };
    CHECK(volume_matches("volume.bin"));
    const auto bitmap_matches = [gedp, &grid_matches, &bitmap](const char *name, const char *shape = "mask.s") {
	if (!grid_matches(gedp->dbip, name, DB5_MINORTYPE_BINU_8BITINT_U, bitmap))
	    return false;
	auto *surface = db_lookup(gedp->dbip, shape, LOOKUP_QUIET);
	struct rt_db_internal intern = RT_DB_INTERNAL_INIT_ZERO;
	if (!surface || rt_db_get_internal(&intern, surface, gedp->dbip, nullptr) < 0)
	    return false;
	bool loaded = false;
	if (intern.idb_type == ID_EBM) {
	    const auto *ebm = static_cast<const struct rt_ebm_internal *>(intern.idb_ptr);
	    loaded = ebm->datasrc == RT_EBM_SRC_OBJ && BU_STR_EQUAL(ebm->name, name) && ebm->buf;
	}
	rt_db_free_internal(&intern);
	return loaded;
    };
    CHECK(bitmap_matches("mask.bin"));

    const std::unique_ptr<struct ged, decltype(&ged_close)> inspector(ged_create(), ged_close);
    const char *open[] = {"db", "open", "-r", gedp->dbip->dbi_filename, nullptr};
    const char *preview[] = {"mvall", "-n", "volume.bin", "voxel samples.bin", nullptr};
    CHECK(inspector && run(inspector.get(), open) == BRLCAD_OK);
    const auto exported_path = (directory / "readonly samples.bin").string();
    const char *export_samples[] = {"bo", "-o", exported_path.c_str(), "volume.bin", nullptr};
    const int exported = run(inspector.get(), export_samples);
    if (exported == BRLCAD_OK) {
	std::ifstream samples(exported_path, std::ios::binary);
	uint8_t restored[sizeof(original)];
	CHECK(samples.read(reinterpret_cast<char *>(restored), sizeof(restored)) &&
	    std::memcmp(restored, original, sizeof(original)) == 0 && samples.peek() == EOF);
	CHECK(run(gedp, export_samples) == BRLCAD_ERROR && contains_result(gedp, "already exists") &&
	    selected_id(gedp) == after);
    } else {
	CHECK(contains_result(inspector.get(), "Atomic binary export is unavailable"));
    }
    CHECK(run(inspector.get(), preview) == BRLCAD_OK && contains_result(inspector.get(), "volume.s"));
    CHECK(db_lookup(inspector->dbip, "volume.bin", LOOKUP_QUIET) &&
	!db_lookup(inspector->dbip, "voxel samples.bin", LOOKUP_QUIET) && selected_id(gedp) == after);
    const char *preview_bitmap[] = {"mvall", "-n", "mask.bin", "bitmap samples.bin", nullptr};
    CHECK(run(inspector.get(), preview_bitmap) == BRLCAD_OK && contains_result(inspector.get(), "mask.s") &&
	!contains_result(inspector.get(), "file-mask.s"));

    const char *rename_dsp[] = {"mvall", "terrain.bin", "graded samples.bin", nullptr};
    int staged_writes = 0;
    CHECK(ged_clbk_set(gedp, "mvall", BU_CLBK_PRE, fail_staged_edit, &staged_writes) == BRLCAD_OK);
    CHECK(run(gedp, rename_dsp) == BRLCAD_ERROR && staged_writes == 1);
    CHECK(ged_clbk_set(gedp, "mvall", BU_CLBK_PRE, nullptr, nullptr) == GED_OVERRIDE);
    CHECK(selected_id(gedp) == after && terrain_matches(replacement));

    const std::string long_name(RT_VOL_NAME_LEN, 'v');
    CHECK(run(gedp, preview) == BRLCAD_OK && contains_result(gedp, "volume.s"));
    const char *invalid_name[] = {"mvall", "volume.bin", long_name.c_str(), nullptr};
    CHECK(run(gedp, invalid_name) == BRLCAD_ERROR && selected_id(gedp) == after &&
	volume_matches("volume.bin") && !db_lookup(gedp->dbip, long_name.c_str(), LOOKUP_QUIET));

    const char *rename_grids[] = {"db", "history", "batch", "Rename raster source objects",
	"{mvall terrain.bin {graded samples.bin}} {move_all volume.bin {voxel samples.bin}}"
	" {mvall mask.bin {bitmap samples.bin}}", nullptr};
    CHECK(run(gedp, rename_grids) == BRLCAD_OK);
    const auto renamed = selected_id(gedp);
    CHECK(renamed != after && terrain_matches(replacement, "graded samples.bin") && volume_matches("voxel samples.bin") &&
	bitmap_matches("bitmap samples.bin") && !db_lookup(gedp->dbip, "terrain.bin", LOOKUP_QUIET) &&
	!db_lookup(gedp->dbip, "volume.bin", LOOKUP_QUIET) && !db_lookup(gedp->dbip, "mask.bin", LOOKUP_QUIET));
    CHECK(run(gedp, undo) == BRLCAD_OK && selected_id(gedp) == after &&
	terrain_matches(replacement) && volume_matches("volume.bin") && bitmap_matches("mask.bin"));
    CHECK(run(gedp, redo) == BRLCAD_OK && selected_id(gedp) == renamed &&
	terrain_matches(replacement, "graded samples.bin") && volume_matches("voxel samples.bin") &&
	bitmap_matches("bitmap samples.bin"));

    const auto subset_path = (directory / "raster shapes.g").string();
    const char *keep[] = {"keep", subset_path.c_str(), "terrain.s", "volume.s", "mask.s", nullptr};
    CHECK(run(gedp, keep) == BRLCAD_OK && selected_id(gedp) == renamed);
    const std::unique_ptr<struct db_i, decltype(&db_close)> subset(db_open(subset_path.c_str(), DB_OPEN_READONLY), db_close);
    CHECK(subset && db_dirbuild(subset.get()) == 0 && !db_lookup(subset.get(), "_BRLCAD_HISTORY", LOOKUP_QUIET));
    CHECK(grid_matches(subset.get(), "graded samples.bin", DB5_MINORTYPE_BINU_16BITINT_U, replacement) &&
	grid_matches(subset.get(), "voxel samples.bin", DB5_MINORTYPE_BINU_8BITINT_U, original) &&
	grid_matches(subset.get(), "bitmap samples.bin", DB5_MINORTYPE_BINU_8BITINT_U, bitmap));
    for (const char *name : {"terrain.s", "volume.s", "mask.s"}) {
	auto *shape = db_lookup(subset.get(), name, LOOKUP_QUIET);
	struct rt_db_internal internal = RT_DB_INTERNAL_INIT_ZERO;
	CHECK(shape && rt_db_get_internal(&internal, shape, subset.get(), nullptr) > 0);
	rt_db_free_internal(&internal);
    }

    const std::string long_prefix(RT_EBM_NAME_LEN, 'm');
    const std::string long_grid = long_prefix + "graded samples.bin";
    const char *bad_concat[] = {"concat", subset_path.c_str(), long_prefix.c_str(), nullptr};
    CHECK(run(gedp, bad_concat) == BRLCAD_ERROR && contains_result(gedp, "remap the raster reference") &&
	selected_id(gedp) == renamed && !db_lookup(gedp->dbip, long_grid.c_str(), LOOKUP_QUIET));

    const auto imported_rasters_available = [&]() {
	return terrain_matches(replacement, "imported_graded samples.bin", "imported_terrain.s") &&
	    volume_matches("imported_voxel samples.bin", "imported_volume.s") &&
	    bitmap_matches("imported_bitmap samples.bin", "imported_mask.s");
    };
    const char *concat[] = {"concat", subset_path.c_str(), "imported_", nullptr};
    CHECK(run(gedp, concat) == BRLCAD_OK && imported_rasters_available());
    const char *attribute[] = {"attr", "get", "imported_mask.s", "sample-role", nullptr};
    CHECK(run(gedp, attribute) == BRLCAD_OK && contains_result(gedp, "mask-fixture"));
    const auto imported = selected_id(gedp);
    CHECK(imported != renamed && run(gedp, undo) == BRLCAD_OK && selected_id(gedp) == renamed &&
	!db_lookup(gedp->dbip, "imported_bitmap samples.bin", LOOKUP_QUIET));
    CHECK(run(gedp, redo) == BRLCAD_OK && selected_id(gedp) == imported && imported_rasters_available());
    return 0;
}

static int
test_sketch_rename(struct ged *gedp)
{
    const char *make[] = {"make", "sketch-extrusion.s", "extrude", nullptr};
    const char *extrusion_ref[] = {"get", "sketch-extrusion.s", "S", nullptr};
    CHECK(run(gedp, make) == BRLCAD_OK && run(gedp, extrusion_ref) == BRLCAD_OK);
    std::string sketch = bu_vls_cstr(gedp->ged_result_str);
    CHECK(!sketch.empty());
    const auto create_revolve = [](struct ged *editor, void *data) {
	const auto &name = *static_cast<const std::string *>(data);
	struct rt_revolve_internal *revolve;
	BU_ALLOC(revolve, struct rt_revolve_internal);
	revolve->magic = RT_REVOLVE_INTERNAL_MAGIC;
	VSET(revolve->axis3d, 0.0, 0.0, 1.0);
	VSET(revolve->r, 1.0, 0.0, 0.0);
	revolve->ang = M_2PI;
	bu_vls_init(&revolve->sketch_name);
	bu_vls_strcpy(&revolve->sketch_name, name.c_str());
	auto *writer = wdb_dbopen(editor->dbip, RT_WDB_TYPE_DB_DEFAULT);
	return wdb_export(writer, "sketch-revolve.s", revolve, ID_REVOLVE, 1.0) == 0 ? BRLCAD_OK : BRLCAD_ERROR;
    };
    CHECK(ged_history_apply_edit(gedp, "Create revolve", create_revolve, &sketch) == BRLCAD_OK);
    const auto original = selected_id(gedp);
    const auto reference_matches = [gedp, &extrusion_ref](const char *name) {
	if (run(gedp, extrusion_ref) != BRLCAD_OK || !BU_STR_EQUAL(bu_vls_cstr(gedp->ged_result_str), name))
	    return false;
	auto *dp = db_lookup(gedp->dbip, "sketch-revolve.s", LOOKUP_QUIET);
	struct rt_db_internal internal = RT_DB_INTERNAL_INIT_ZERO;
	if (!dp || rt_db_get_internal(&internal, dp, gedp->dbip, nullptr) < 0)
	    return false;
	bool matches = false;
	if (internal.idb_type == ID_REVOLVE) {
	    const auto *revolve = static_cast<const struct rt_revolve_internal *>(internal.idb_ptr);
	    matches = revolve->skt && BU_STR_EQUAL(bu_vls_cstr(&revolve->sketch_name), name);
	}
	rt_db_free_internal(&internal);
	return matches;
    };
    CHECK(reference_matches(sketch.c_str()));

    const char *destination = "shared profile.s";
    const char *preview[] = {"mvall", "-n", sketch.c_str(), destination, nullptr};
    CHECK(run(gedp, preview) == BRLCAD_OK && contains_result(gedp, "sketch-revolve.s") &&
	contains_result(gedp, "sketch-extrusion.s") && selected_id(gedp) == original);
    const char *rename[] = {"mvall", sketch.c_str(), destination, nullptr};
    int staged_writes = 0;
    const int callback_status = ged_clbk_set(gedp, "mvall", BU_CLBK_PRE, fail_staged_edit, &staged_writes);
    CHECK(callback_status == BRLCAD_OK || callback_status == GED_OVERRIDE);
    CHECK(run(gedp, rename) == BRLCAD_ERROR && staged_writes == 1);
    CHECK(ged_clbk_set(gedp, "mvall", BU_CLBK_PRE, nullptr, nullptr) == GED_OVERRIDE);
    CHECK(selected_id(gedp) == original && reference_matches(sketch.c_str()));

    CHECK(run(gedp, rename) == BRLCAD_OK && reference_matches(destination));
    const auto renamed = selected_id(gedp);
    CHECK(renamed != original && !db_lookup(gedp->dbip, sketch.c_str(), LOOKUP_QUIET));
    const char *undo[] = {"db", "undo", nullptr};
    CHECK(run(gedp, undo) == BRLCAD_OK && selected_id(gedp) == original && reference_matches(sketch.c_str()));
    const char *redo[] = {"db", "redo", nullptr};
    CHECK(run(gedp, redo) == BRLCAD_OK && selected_id(gedp) == renamed && reference_matches(destination));
    return 0;
}

static int
test_opaque_rename(struct ged *gedp)
{
    const char *original = "opaque.payload";
    const char *renamed = "archived.payload";
    struct Fixture {
	const char *name;
	uint8_t body[5];
    } fixture{original, {0x10, 0xab, 0, 0x7f, 0xff}};
    const auto create_opaque = [](struct ged *ged, void *data) {
	auto *value = static_cast<Fixture *>(data);
	struct bu_external body = BU_EXTERNAL_INIT_ZERO;
	body.ext_nbytes = sizeof(value->body);
	body.ext_buf = value->body;

	struct bu_attribute_value_set attrs;
	bu_avs_init(&attrs, 1, "opaque rename fixture");
	bu_avs_add(&attrs, "description", "opaque test record");
	struct bu_external attr = BU_EXTERNAL_INIT_ZERO;
	db5_export_attributes(&attr, &attrs);
	bu_avs_free(&attrs);

	struct bu_external external = BU_EXTERNAL_INIT_ZERO;
	db5_export_object3(&external, DB5HDR_HFLAGS_DLI_APPLICATION_DATA_OBJECT, value->name,
	    DB5HDR_HFLAGS_HIDDEN_OBJECT, &attr, &body, DB5_MAJORTYPE_BINARY_MIME,
	    DB5_MINORTYPE_RESERVED, DB5_ZZZ_UNCOMPRESSED, DB5_ZZZ_UNCOMPRESSED);
	bu_free_external(&attr);
	auto *writer = wdb_dbopen(ged->dbip, RT_WDB_TYPE_DB_DEFAULT);
	const int result = wdb_export_external(writer, &external, value->name,
	    RT_DIR_NON_GEOM | RT_DIR_HIDDEN, DB5_MINORTYPE_RESERVED);
	bu_free_external(&external);
	return result == 0 ? BRLCAD_OK : BRLCAD_ERROR;
    };
    CHECK(ged_history_apply_edit(gedp, "Add opaque record", create_opaque, &fixture) == BRLCAD_OK);
    const auto initial = selected_id(gedp);
    const auto *entry = db_lookup(gedp->dbip, original, LOOKUP_QUIET);
    struct bu_external expected = BU_EXTERNAL_INIT_ZERO;
    CHECK(entry && db_get_external(&expected, entry, gedp->dbip) == 0 &&
	db_wrap_v5_external(&expected, renamed) == 0);

    const char *move[] = {"move", original, renamed, nullptr};
    CHECK(run(gedp, move) == BRLCAD_OK && selected_id(gedp) != initial);
    entry = db_lookup(gedp->dbip, renamed, LOOKUP_QUIET);
    struct bu_external moved = BU_EXTERNAL_INIT_ZERO;
    const bool matches = entry && db_get_external(&moved, entry, gedp->dbip) == 0 &&
	moved.ext_nbytes == expected.ext_nbytes &&
	std::memcmp(moved.ext_buf, expected.ext_buf, expected.ext_nbytes) == 0;
    bu_free_external(&moved);
    bu_free_external(&expected);
    CHECK(matches && !db_lookup(gedp->dbip, original, LOOKUP_QUIET));
    const char *undo[] = {"db", "undo", nullptr};
    CHECK(run(gedp, undo) == BRLCAD_OK && selected_id(gedp) == initial &&
	db_lookup(gedp->dbip, original, LOOKUP_QUIET) && !db_lookup(gedp->dbip, renamed, LOOKUP_QUIET));
    return 0;
}

static int
test_instance_history(struct ged *gedp)
{
    const char *make[] = {"in", "bake-source.s", "sph", "1", "2", "3", "5", nullptr};
    const char *inner[] = {"g", "bake-inner.c", "bake-source.s", nullptr};
    const char *outer[] = {"g", "bake-root.c", "bake-inner.c", nullptr};
    const char *place_outer[] = {"arced", "bake-root.c/bake-inner.c",
	"matrix", "rarc", "xlate", "100", "200", "300", nullptr};
    const char *set_attribute[] = {"attr", "set", "bake-source.s", "part-class", "instance-source", nullptr};
    CHECK(run(gedp, make) == BRLCAD_OK && run(gedp, inner) == BRLCAD_OK &&
	run(gedp, outer) == BRLCAD_OK && run(gedp, place_outer) == BRLCAD_OK && run(gedp, set_attribute) == BRLCAD_OK);

    const auto selected = selected_id(gedp);
    const auto matrix_matches = [gedp](fastf_t x, fastf_t y, fastf_t z) {
	struct db_full_path path = DB_FULL_PATH_INIT_ZERO;
	mat_t matrix;
	const bool ok = db_string_to_path(&path, gedp->dbip, "bake-root.c/bake-inner.c/bake-source.s") == 0 &&
	    db_path_to_mat(gedp->dbip, &path, matrix, 0);
	db_free_full_path(&path);
	return ok && EQUAL(matrix[MDX], x) && EQUAL(matrix[MDY], y) && EQUAL(matrix[MDZ], z);
    };
    CHECK(matrix_matches(100, 200, 300));

    const char *failed[] = {"db", "history", "batch", "Failed instance copy",
	"{arced bake-inner.c/bake-source.s matrix rarc xlate 10 20 30}"
	" {copyeval bake-root.c/bake-source.s invalid-bake.s}", nullptr};
    CHECK(run(gedp, failed) == BRLCAD_ERROR && contains_result(gedp, "Unable to resolve source path") &&
	selected_id(gedp) == selected && matrix_matches(100, 200, 300) &&
	!db_lookup(gedp->dbip, "invalid-bake.s", LOOKUP_QUIET));

    const char *batch[] = {"db", "history", "batch", "Move and bake an instance",
	"{arced bake-inner.c/bake-source.s matrix rarc xlate 10 20 30}"
	" {db copyeval /bake-root.c//bake-inner.c/bake-source.s/ baked.s}", nullptr};
    CHECK(run(gedp, batch) == BRLCAD_OK);
    const auto revised = selected_id(gedp);
    const auto baked_matches = [gedp]() {
	const char *center[] = {"get", "baked.s", "V", nullptr};
	const char *attribute[] = {"attr", "get", "baked.s", "part-class", nullptr};
	return run(gedp, center) == BRLCAD_OK && BU_STR_EQUAL(bu_vls_cstr(gedp->ged_result_str), "111 222 333") &&
	    run(gedp, attribute) == BRLCAD_OK && contains_result(gedp, "instance-source");
    };
    const char *source_center[] = {"get", "bake-source.s", "V", nullptr};
    CHECK(revised != selected && matrix_matches(110, 220, 330) && baked_matches() &&
	run(gedp, source_center) == BRLCAD_OK && BU_STR_EQUAL(bu_vls_cstr(gedp->ged_result_str), "1 2 3"));

    const char *undo[] = {"db", "undo", nullptr};
    CHECK(run(gedp, undo) == BRLCAD_OK && selected_id(gedp) == selected &&
	matrix_matches(100, 200, 300) && !db_lookup(gedp->dbip, "baked.s", LOOKUP_QUIET));
    const char *redo[] = {"db", "redo", nullptr};
    CHECK(run(gedp, redo) == BRLCAD_OK && selected_id(gedp) == revised &&
	matrix_matches(110, 220, 330) && baked_matches());
    return 0;
}

static int
test_push_history(struct ged *gedp)
{
    const char *setup[] = {"db", "history", "batch", "Place shared instances",
	"{in push-shared.s sph 0 0 0 2} {g push-left.c push-shared.s} {g push-right.c push-shared.s}"
	" {g push-outside.c push-shared.s} {g push-root.c push-left.c push-right.c}"
	" {arced push-left.c/push-shared.s matrix rarc xlate -10 0 0}"
	" {arced push-right.c/push-shared.s matrix rarc xlate 10 0 0}", nullptr};
    CHECK(run(gedp, setup) == BRLCAD_OK);
    const auto before = selected_id(gedp);

    const auto instance_matches = [](struct db_i *dbip, const char *group, const char *primitive, fastf_t center, fastf_t translate) {
	const auto *dp = db_lookup(dbip, group, LOOKUP_QUIET);
	struct rt_db_internal internal = RT_DB_INTERNAL_INIT_ZERO;
	if (!dp || rt_db_get_internal(&internal, dp, dbip, nullptr) < 0)
	    return false;
	const auto *comb = internal.idb_type == ID_COMBINATION ? static_cast<const struct rt_comb_internal *>(internal.idb_ptr) : nullptr;
	const auto *leaf = comb ? comb->tree : nullptr;
	std::string name;
	if (leaf && leaf->tr_op == OP_DB_LEAF && EQUAL(leaf->tr_l.tl_mat ? leaf->tr_l.tl_mat[MDX] : 0.0, translate))
	    name = leaf->tr_l.tl_name;
	rt_db_free_internal(&internal);
	if (primitive ? name != primitive : name.find("push-shared.s_") != 0)
	    return false;
	const auto *shape = db_lookup(dbip, name.c_str(), LOOKUP_QUIET);
	if (!shape || rt_db_get_internal(&internal, shape, dbip, nullptr) < 0)
	    return false;
	const auto *sphere = static_cast<const struct rt_ell_internal *>(internal.idb_ptr);
	const bool positioned = (internal.idb_type == ID_SPH || internal.idb_type == ID_ELL) && EQUAL(sphere->v[X], center);
	rt_db_free_internal(&internal);
	return positioned;
    };
    const auto original = [gedp, &instance_matches]() {
	return instance_matches(gedp->dbip, "push-left.c", "push-shared.s", 0, -10) &&
	    instance_matches(gedp->dbip, "push-right.c", "push-shared.s", 0, 10) &&
	    !db_lookup(gedp->dbip, "push-shared.s_0", LOOKUP_QUIET);
    };
    CHECK(original());

    const std::unique_ptr<struct ged, decltype(&ged_close)> reader(ged_create(), ged_close);
    const char *open[] = {"db", "open", "-r", gedp->dbip->dbi_filename, nullptr};
    const char *preview[] = {"npush", "push-root.c", "-vDLx", nullptr};
    const char *apply[] = {"npush", "-Lx", "push-root.c", nullptr};
    CHECK(reader && run(reader.get(), open) == BRLCAD_OK);
    CHECK(run(reader.get(), preview) == BRLCAD_OK && contains_result(reader.get(), "update 2 objects and create 2 objects"));
    CHECK(!db_lookup(reader->dbip, "push-shared.s_0", LOOKUP_QUIET) && run(reader.get(), apply) == BRLCAD_ERROR);
    CHECK(selected_id(gedp) == before && original());

    int commits = 0;
    const auto observer = ged_event_observer_add(gedp, GED_EVENT_OBSERVER_POST_RECONCILE, count_commit, &commits);
    CHECK(observer);
    int staged_writes = 0;
    CHECK(ged_clbk_set(gedp, "npush", BU_CLBK_PRE, fail_staged_edit, &staged_writes) == BRLCAD_OK);
    CHECK(run(gedp, apply) == BRLCAD_ERROR && contains_result(gedp, "Unable to store") && staged_writes == 1);
    CHECK(ged_clbk_set(gedp, "npush", BU_CLBK_PRE, nullptr, nullptr) == GED_OVERRIDE);
    CHECK(selected_id(gedp) == before && original() && commits == 0);

    CHECK(run(gedp, apply) == BRLCAD_OK && commits == 1);
    CHECK(ged_event_observer_remove(gedp, observer) == 1);
    const auto pushed = selected_id(gedp);
    const auto pushed_instances = [gedp, &instance_matches]() {
	return instance_matches(gedp->dbip, "push-left.c", nullptr, -10, 0) &&
	    instance_matches(gedp->dbip, "push-right.c", nullptr, 10, 0) &&
	    instance_matches(gedp->dbip, "push-outside.c", "push-shared.s", 0, 0);
    };
    CHECK(pushed != before && pushed_instances());
    const char *undo[] = {"db", "undo", nullptr};
    const char *redo[] = {"db", "redo", nullptr};
    CHECK(run(gedp, undo) == BRLCAD_OK && selected_id(gedp) == before && original());
    CHECK(run(gedp, redo) == BRLCAD_OK && selected_id(gedp) == pushed && pushed_instances());

    const char *cycle[] = {"db", "history", "batch", "Create nested cycle",
	"{g push-cycle-a.c push-shared.s} {g push-cycle-b.c push-cycle-a.c}"
	" {g push-cycle-a.c push-cycle-b.c} {g push-cycle-root.c push-cycle-a.c}", nullptr};
    CHECK(run(gedp, cycle) == BRLCAD_OK);
    const auto cycle_id = selected_id(gedp);
    const char *reject_cycle[] = {"npush", "push-cycle-root.c", nullptr};
    CHECK(run(gedp, reject_cycle) == BRLCAD_ERROR && contains_result(gedp, "Cyclic path found") &&
	selected_id(gedp) == cycle_id);
    return 0;
}

static int
test_history_selection(struct ged *gedp, bool checkpoints)
{
    const std::string root = selected_id(gedp);
    const char *first[] = {"make", "history-selected.s", "sph", nullptr};
    const char *second[] = {"make", "history-other.s", "sph", nullptr};
    const char *undo[] = {"db", "undo", nullptr};
    CHECK(!root.empty() && run(gedp, first) == BRLCAD_OK);
    const auto selected = selected_id(gedp);
    CHECK(!selected.empty() && selected != root);
    CHECK(run(gedp, undo) == BRLCAD_OK && selected_id(gedp) == root);
    CHECK(run(gedp, second) == BRLCAD_OK);
    const auto other = selected_id(gedp);
    CHECK(!other.empty() && other != root && other != selected);
    if (checkpoints) {
	const char *mark[] = {"db", "history", "mark", selected.c_str(), nullptr};
	CHECK(run(gedp, mark) == BRLCAD_OK && selected_id(gedp) == other);
	const char *show[] = {"db", "history", "show", selected.c_str(), "--json", nullptr};
	CHECK(run(gedp, show) == BRLCAD_OK && contains_result(gedp, "history-other.s"));
	const auto prefix = selected.substr(0, selected.length() - 1);
	const char *short_id[] = {"db", "history", "show", "--id", prefix.c_str(), nullptr};
	CHECK(run(gedp, short_id) == BRLCAD_ERROR);
	const char *mark_option[] = {"db", "history", "mark", "--id", nullptr};
	const char *show_option[] = {"db", "history", "show", "--id", "--json", nullptr};
	CHECK(run(gedp, mark_option) == BRLCAD_OK && run(gedp, show_option) == BRLCAD_OK &&
	    contains_result(gedp, "history-other.s"));
    }

    const char *show_exact[] = {"db", "history", "show", "--id", selected.c_str(), "--json", nullptr};
    CHECK(run(gedp, show_exact) == BRLCAD_OK && contains_result(gedp, "history-selected.s") &&
	!contains_result(gedp, "history-other.s"));
    const std::unique_ptr<struct db_i, decltype(&db_close)>
	preview(ged_history_open_revision_by_id(gedp, selected.c_str(), nullptr), db_close);
    CHECK(preview && preview->dbi_read_only && db_lookup(preview.get(), "history-selected.s", LOOKUP_QUIET) &&
	!db_lookup(preview.get(), "history-other.s", LOOKUP_QUIET) && selected_id(gedp) == other);

    using ObjectChanges = std::map<std::string, enum ged_history_change_type>;
    ObjectChanges changes;
    const auto collect = [](const struct ged_history_change *change, void *data) {
	auto &collected = *static_cast<ObjectChanges *>(data);
	return !change->new_name && collected.emplace(change->name, change->type).second ? BRLCAD_OK : BRLCAD_ERROR;
    };
    const ObjectChanges expected{{"history-other.s", GED_HISTORY_DELETE}, {"history-selected.s", GED_HISTORY_CREATE}};
    CHECK(ged_history_visit_diff_by_id(gedp, other.c_str(), selected.c_str(), nullptr, collect, &changes, nullptr) == BRLCAD_OK);
    CHECK(changes == expected && selected_id(gedp) == other);
    changes.clear();
    CHECK(ged_history_visit_diff_by_id(gedp, selected.c_str(), other.c_str(), "history-selected.s",
	    collect, &changes, nullptr) == BRLCAD_OK);
    const ObjectChanges filtered{{"history-selected.s", GED_HISTORY_DELETE}};
    CHECK(changes == filtered);
    changes.clear();
    CHECK(ged_history_visit_diff_by_id(gedp, other.c_str(), other.c_str(), nullptr, collect, &changes, nullptr) == BRLCAD_OK);
    CHECK(changes.empty() && selected_id(gedp) == other);

    const char *checkout[] = {"db", "history", "checkout", "--id", selected.c_str(), nullptr};
    CHECK(run(gedp, checkout) == BRLCAD_OK && selected_id(gedp) == selected);
    CHECK(run(gedp, undo) == BRLCAD_OK && selected_id(gedp) == root);
    if (checkpoints) {
	const char *redo_mark[] = {"db", "redo", selected.c_str(), nullptr};
	CHECK(run(gedp, redo_mark) == BRLCAD_OK && selected_id(gedp) == other);
	CHECK(run(gedp, undo) == BRLCAD_OK && selected_id(gedp) == root);
    }
    const char *redo_exact[] = {"db", "redo", "--id", selected.c_str(), nullptr};
    CHECK(run(gedp, redo_exact) == BRLCAD_OK && selected_id(gedp) == selected);
    return 0;
}

static int
test_backend_parity(const std::filesystem::path &directory)
{
    using Records = std::map<std::string, std::vector<uint8_t>>;
    Records plain_records;
    for (const char *mode : {"plain", "session", "embedded", "workspace"}) {
	const auto path = (directory / (std::string(mode) + "-parity.g")).string();
	auto *initial = db_create(path.c_str(), 5);
	CHECK(initial);
	db_close(initial);
	const std::unique_ptr<struct ged, decltype(&ged_close)> owner(ged_open("db", path.c_str(), 1), ged_close);
	auto *gedp = owner.get();
	CHECK(gedp);
	const char *set_defaults[] = {"regdef", "3100", "0", "68", "17", nullptr};
	CHECK(run(gedp, set_defaults) == BRLCAD_OK);
	if (BU_STR_EQUAL(mode, "workspace")) {
	    const auto workspace = (directory / "parity-workspace").string();
	    const char *create[] = {"db", "history", "workspace", workspace.c_str(), nullptr};
	    CHECK(run(gedp, create) == BRLCAD_OK);
	} else if (!BU_STR_EQUAL(mode, "plain")) {
	    const char *enable[] = {"db", "history", "enable", mode, nullptr};
	    const int enabled = run(gedp, enable);
	    if (enabled != BRLCAD_OK && contains_result(gedp, "Atomic DB5 replacement is unavailable"))
		continue;
	    CHECK(enabled == BRLCAD_OK);
	}

	const char *make_body[] = {"make", "body.s", "sph", nullptr};
	const char *make_filler[] = {"make", "filler.s", "sph", nullptr};
	const char *combine[] = {"comb", "assembly.c", "u", "body.s", "u", "filler.s", nullptr};
	const char *group[] = {"group", "model.c", "assembly.c", nullptr};
	const char *member_remove[] = {"rm", "assembly.c/filler.s", nullptr};
	const char *object_remove[] = {"rm", "filler.s", nullptr};
	const char *rename[] = {"mvall", "body.s", "renamed.s", nullptr};
	const char *attribute[] = {"attr", "set", "renamed.s", "rgb", "15/45/95", nullptr};
	const char *region[] = {"comb", "assembly.c", "region", nullptr};
	const char *make_region[] = {"r", "part.r", "u", "renamed.s", nullptr};
	const char *title[] = {"title", "An equivalent model", nullptr};
	const char *show_defaults[] = {"regdef", nullptr};
	CHECK(run(gedp, make_body) == BRLCAD_OK && run(gedp, make_filler) == BRLCAD_OK);
	CHECK(run(gedp, combine) == BRLCAD_OK && run(gedp, group) == BRLCAD_OK);
	CHECK(run(gedp, member_remove) == BRLCAD_OK && run(gedp, object_remove) == BRLCAD_OK);
	CHECK(run(gedp, rename) == BRLCAD_OK && run(gedp, attribute) == BRLCAD_OK &&
	    run(gedp, region) == BRLCAD_OK && run(gedp, make_region) == BRLCAD_OK && run(gedp, title) == BRLCAD_OK);
	CHECK(run(gedp, show_defaults) == BRLCAD_OK && contains_result(gedp, "ident 3101 air 0 los 68 material 17"));
	if (!BU_STR_EQUAL(mode, "plain")) {
	    const auto selected = selected_id(gedp);
	    const char *invalid[] = {"comb", "partial.c", "u", "renamed.s", "u", "missing.s", nullptr};
	    CHECK(run(gedp, invalid) == BRLCAD_ERROR && selected_id(gedp) == selected &&
		!db_lookup(gedp->dbip, "partial.c", LOOKUP_QUIET));
	}

	std::unique_ptr<struct rt_i, decltype(&rt_i_destroy)> rtip(rt_i_create(gedp->dbip), rt_i_destroy);
	CHECK(rtip && rt_gettree(rtip.get(), "model.c") == 0);
	rt_prep(rtip.get());
	struct application ray;
	RT_APPLICATION_INIT(&ray);
	ray.a_rt_i = rtip.get();
	ray.a_hit = [](struct application *, struct partition *, struct seg *) { return 1; };
	ray.a_miss = [](struct application *) { return 0; };
	VSET(ray.a_ray.r_pt, rtip->mdl_min[X] - 1.0, 0.0, 0.0);
	VSET(ray.a_ray.r_dir, 1.0, 0.0, 0.0);
	CHECK(rt_shootray(&ray) == 1);
	rtip.reset();

	const auto *archive = db_lookup(gedp->dbip, "_BRLCAD_HISTORY", LOOKUP_QUIET);
	const struct directory *dp;
	Records records;
	FOR_ALL_DIRECTORY_START(dp, gedp->dbip) {
	    if (dp == archive)
		continue;
	    struct bu_external raw = BU_EXTERNAL_INIT_ZERO;
	    CHECK(db_get_external(&raw, dp, gedp->dbip) == 0);
	    records.emplace(dp->d_namep, std::vector<uint8_t>(raw.ext_buf, raw.ext_buf + raw.ext_nbytes));
	    bu_free_external(&raw);
	} FOR_ALL_DIRECTORY_END;
	if (BU_STR_EQUAL(mode, "plain"))
	    plain_records = std::move(records);
	else {
	    CHECK(records == plain_records);
	    CHECK(test_versioned_region_defaults(gedp) == 0);
	    CHECK(test_reference_removal(gedp) == 0);
	    const auto assets = directory / (std::string(mode) + "-assets");
	    CHECK(std::filesystem::create_directory(assets));
	    CHECK(test_binary_asset_history(gedp, assets) == 0);
	    CHECK(test_opaque_rename(gedp) == 0);
	    CHECK(test_sketch_rename(gedp) == 0);
	    CHECK(test_instance_history(gedp) == 0);
	    CHECK(test_push_history(gedp) == 0);
	    CHECK(test_history_selection(gedp, !BU_STR_EQUAL(mode, "session")) == 0);
	}
    }
    return 0;
}

static int
test_workspace_history()
{
    char path[MAXPATHLEN] = {};
    FILE *temporary = bu_temp_file(path, sizeof(path));
    CHECK(temporary && fclose(temporary) == 0 && bu_file_delete(path));
    const struct TestDirectory {
	std::filesystem::path path;
	~TestDirectory()
	{
	    std::error_code error;
	    std::filesystem::remove_all(path, error);
	}
    } fixture{path};
    CHECK(std::filesystem::create_directory(fixture.path));
    CHECK(test_backend_parity(fixture.path) == 0);
    const auto source = (fixture.path / "source.g").string();
    const auto workspace = (fixture.path / "workspace").string();
    using Db = std::unique_ptr<struct db_i, decltype(&db_close)>;
    using Ged = std::unique_ptr<struct ged, decltype(&ged_close)>;
    Db initial(db_create(source.c_str(), 5), db_close);
    CHECK(initial);
    initial.reset();
    Ged writer(ged_open("db", source.c_str(), 1), ged_close);
    CHECK(writer);
    const char *make[] = {"make", "extrusion.s", "extrude", nullptr};
    const char *reference[] = {"get", "extrusion.s", "S", nullptr};
    CHECK(run(writer.get(), make) == BRLCAD_OK && run(writer.get(), reference) == BRLCAD_OK);
    const std::string old_name = bu_vls_addr(writer->ged_result_str);
    const char *assembly[] = {"g", "assembly.c", old_name.c_str(), "extrusion.s", nullptr};
    CHECK(run(writer.get(), assembly) == BRLCAD_OK);
    const char *create[] = {"db", "history", "workspace", workspace.c_str(), nullptr};
    const char *status[] = {"db", "history", "status", nullptr};
    CHECK(run(writer.get(), create) == BRLCAD_OK && contains_result(writer.get(), "mode workspace"));
    const auto root = selected_id(writer.get());
    CHECK(!root.empty());
    const char *compact[] = {"garbage_collect", "-c", nullptr};
    CHECK(run(writer.get(), compact) != BRLCAD_OK && contains_result(writer.get(), "db history workspace") &&
	selected_id(writer.get()) == root);
    using HistoryStatus = std::unique_ptr<const struct ged_history_status, decltype(&ged_history_status_destroy)>;
    HistoryStatus initial_status(ged_history_status_create(writer.get(), nullptr), ged_history_status_destroy);
    CHECK(initial_status && initial_status->mode == GED_HISTORY_WORKSPACE && initial_status->writable &&
	initial_status->state == GED_HISTORY_CURRENT && !initial_status->undo.revision &&
	!initial_status->redo_count && root == initial_status->selected_revision);
    CHECK(run(writer.get(), create) == BRLCAD_OK && selected_id(writer.get()) == root);
    const char *existing[] = {"db", "history", "workspace", path, nullptr};
    CHECK(run(writer.get(), existing) != BRLCAD_OK && selected_id(writer.get()) == root);

    const char *rename[] = {"db", "mvall", old_name.c_str(), "outline.s", nullptr};
    int failed_writes = 0;
    CHECK(ged_clbk_set(writer.get(), "mvall", BU_CLBK_PRE, fail_staged_edit, &failed_writes) == BRLCAD_OK);
    CHECK(run(writer.get(), rename) != BRLCAD_OK && failed_writes == 1 && selected_id(writer.get()) == root);
    CHECK(ged_clbk_set(writer.get(), "mvall", BU_CLBK_PRE, nullptr, nullptr) == GED_OVERRIDE);
    Ged stale(ged_open("db", workspace.c_str(), 1), ged_close);
    CHECK(stale && selected_id(stale.get()) == root);
    CHECK(run(writer.get(), rename) == BRLCAD_OK && run(writer.get(), reference) == BRLCAD_OK &&
	contains_result(writer.get(), "outline.s"));
    const auto renamed = selected_id(writer.get());
    CHECK(!renamed.empty() && renamed != root);
    std::string renamed_prefix = renamed.substr(0, 13);
    for (auto &c : renamed_prefix)
	if (c >= 'a' && c <= 'f')
	    c = static_cast<char>(c - 'a' + 'A');
    const Db initial_view(ged_history_open_revision(writer.get(), root.c_str(), nullptr), db_close);
    CHECK(initial_view && initial_view->dbi_read_only && db_lookup(initial_view.get(), old_name.c_str(), LOOKUP_QUIET) &&
	!db_lookup(initial_view.get(), "outline.s", LOOKUP_QUIET) && selected_id(writer.get()) == renamed);
    HistoryStatus renamed_status(ged_history_status_create(writer.get(), nullptr), ged_history_status_destroy);
    CHECK(renamed_status && renamed == renamed_status->selected_revision && root == renamed_status->undo.revision &&
	root == initial_status->selected_revision && !initial_status->undo.revision);
    const char *show[] = {"db", "history", "show", renamed_prefix.c_str(), nullptr};
    CHECK(run(writer.get(), show) == BRLCAD_OK && contains_result(writer.get(), " -> outline.s") &&
	contains_result(writer.get(), "update extrusion.s") && contains_result(writer.get(), "update assembly.c"));
    const char *mark[] = {"db", "history", "mark", "rename", nullptr};
    CHECK(run(writer.get(), mark) == BRLCAD_OK && selected_id(writer.get()) == renamed);
    CHECK(run(writer.get(), mark) != BRLCAD_OK && contains_result(writer.get(), "already exists"));
    const char *log[] = {"db", "history", "log", nullptr};
    CHECK(run(writer.get(), log) == BRLCAD_OK && contains_result(writer.get(), " rename"));
    const char *log_json[] = {"db", "history", "log", "--json", nullptr};
    CHECK(run(writer.get(), log_json) == BRLCAD_OK);
    const auto log_data = nlohmann::json::parse(bu_vls_addr(writer->ged_result_str), nullptr, false);
    CHECK(!log_data.is_discarded() && log_data["schema"] == 1 && log_data["revisions"].size() == 2 &&
	log_data["checkpoints"][0]["revision"] == renamed && log_data["checkpoints"][0]["name"] == "rename");
    const char *filter_json[] = {"db", "history", "log", "--object", "outline.s", "--json", nullptr};
    CHECK(run(writer.get(), filter_json) == BRLCAD_OK);
    const auto filtered = nlohmann::json::parse(bu_vls_addr(writer->ged_result_str), nullptr, false);
    CHECK(!filtered.is_discarded() && filtered["revisions"].size() == 1 &&
	filtered["revisions"][0]["id"] == renamed && filtered["revisions"][0]["selected"] == true &&
	filtered["checkpoints"].empty());
    const char *show_json[] = {"db", "history", "show", "rename", "--json", nullptr};
    CHECK(run(writer.get(), show_json) == BRLCAD_OK);
    const auto shown = nlohmann::json::parse(bu_vls_addr(writer->ged_result_str), nullptr, false);
    CHECK(!shown.is_discarded() && shown["schema"] == 1 && shown["revision"]["id"] == renamed &&
	shown["revision"]["parent"] == root && shown["changes"][0]["type"] == "rename" &&
	shown["changes"][0]["name"] == old_name && shown["changes"][0]["new_name"] == "outline.s");
    const char *diff[] = {"db", "history", "diff", root.c_str(), "rename", "--object", "extrusion.s", nullptr};
    CHECK(run(writer.get(), diff) == BRLCAD_OK && contains_result(writer.get(), "update extrusion.s") &&
	!contains_result(writer.get(), "outline.s"));

    const char *staged_title[] = {"title", "staged conflict", nullptr};
    CHECK(run(stale.get(), staged_title) != BRLCAD_OK && contains_result(stale.get(), "Workspace base changed") &&
	contains_result(stale.get(), root.c_str()) && contains_result(stale.get(), renamed.c_str()));
    CHECK(run(stale.get(), status) == BRLCAD_OK && contains_result(stale.get(), "status stale") &&
	contains_result(stale.get(), "staged conflict"));
    const char *status_json[] = {"db", "history", "status", "--json", nullptr};
    CHECK(run(stale.get(), status_json) == BRLCAD_OK);
    const auto stale_json = nlohmann::json::parse(bu_vls_addr(stale->ged_result_str), nullptr, false);
    CHECK(!stale_json.is_discarded() && stale_json["schema"] == 1 && stale_json["mode"] == "workspace" &&
	stale_json["state"] == "stale" && stale_json["revision"] == root && stale_json["workspace_head"] == renamed &&
	stale_json["staged_conflict"] == true && stale_json["undo"].is_null());
    const char *conflict[] = {"db", "history", "conflict", nullptr};
    CHECK(run(stale.get(), conflict) == BRLCAD_OK && contains_result(stale.get(), "update _GLOBAL"));
    CHECK(run(stale.get(), reference) == BRLCAD_OK && contains_result(stale.get(), old_name.c_str()));
    const auto recovered_path = (fixture.path / "recovered.g").string();
    const char *save_conflict[] = {"db", "history", "conflict", "export", recovered_path.c_str(), nullptr};
    const int recovered_status = run(stale.get(), save_conflict);
    if (recovered_status != BRLCAD_OK && contains_result(stale.get(), "Atomic DB5 export is unavailable"))
	return 0;
    CHECK(recovered_status == BRLCAD_OK);
    Ged recovered_conflict(ged_open("db", recovered_path.c_str(), 1), ged_close);
    const char *title[] = {"title", nullptr};
    const char *verify[] = {"db", "history", "verify", nullptr};
    CHECK(recovered_conflict && run(recovered_conflict.get(), verify) == BRLCAD_OK &&
	run(recovered_conflict.get(), title) == BRLCAD_OK && contains_result(recovered_conflict.get(), "staged conflict") &&
	run(recovered_conflict.get(), show) == BRLCAD_OK && contains_result(recovered_conflict.get(), "outline.s"));
    CHECK(selected_id(writer.get()) == renamed && selected_id(stale.get()) == root);
    const char *open[] = {"db", "open", workspace.c_str(), nullptr};
    CHECK(run(stale.get(), open) == BRLCAD_OK && selected_id(stale.get()) == renamed);
    CHECK(run(stale.get(), conflict) != BRLCAD_OK);

    const char *undo[] = {"db", "undo", nullptr};
    const char *redo[] = {"db", "redo", nullptr};
    Ged inspector(ged_create(), ged_close);
    const char *open_readonly[] = {"db", "open", "-r", workspace.c_str(), nullptr};
    CHECK(inspector && run(inspector.get(), open_readonly) == BRLCAD_OK &&
	run(inspector.get(), status) == BRLCAD_OK && contains_result(inspector.get(), "access read-only"));
    CHECK(run(inspector.get(), show) == BRLCAD_OK && run(inspector.get(), undo) != BRLCAD_OK &&
	contains_result(inspector.get(), "read-only") && selected_id(inspector.get()) == renamed);
    CHECK(run(writer.get(), undo) == BRLCAD_OK && selected_id(writer.get()) == root);
    writer.reset(ged_open("db", workspace.c_str(), 1));
    CHECK(writer && run(writer.get(), status) == BRLCAD_OK && contains_result(writer.get(), "(preferred)"));
    CHECK(run(writer.get(), redo) == BRLCAD_OK && selected_id(writer.get()) == renamed);
    const char *redo_mark[] = {"db", "redo", "rename", nullptr};
    CHECK(run(writer.get(), redo_mark) != BRLCAD_OK && contains_result(writer.get(), "must be a child"));
    CHECK(run(writer.get(), undo) == BRLCAD_OK && run(writer.get(), redo_mark) == BRLCAD_OK &&
	selected_id(writer.get()) == renamed);
    const char *checkout[] = {"db", "history", "checkout", root.c_str(), nullptr};
    CHECK(run(writer.get(), checkout) == BRLCAD_OK && selected_id(writer.get()) == root);
    CHECK(run(writer.get(), staged_title) == BRLCAD_OK);
    const auto branch = selected_id(writer.get());
    CHECK(!branch.empty() && branch != root && branch != renamed);
    const char *move_mark[] = {"db", "history", "mark", "rename", "--move", nullptr};
    CHECK(run(writer.get(), move_mark) == BRLCAD_OK && selected_id(writer.get()) == branch);
    CHECK(run(writer.get(), verify) == BRLCAD_OK && contains_result(writer.get(), "verified"));
    const char *invalid_dir[] = {"db", "open", path, nullptr};
    CHECK(run(writer.get(), invalid_dir) != BRLCAD_OK && contains_result(writer.get(), "no LMDB data file") &&
	selected_id(writer.get()) == branch);

    Ged legacy(ged_open("db", source.c_str(), 1), ged_close);
    CHECK(legacy && run(legacy.get(), status) == BRLCAD_OK && contains_result(legacy.get(), "mode disabled") &&
	run(legacy.get(), reference) == BRLCAD_OK && contains_result(legacy.get(), old_name.c_str()));
    const auto plain = (fixture.path / "plain.g").string();
    const char *export_plain[] = {"db", "history", "export", plain.c_str(), nullptr};
    const int exported = run(writer.get(), export_plain);
    if (exported != BRLCAD_OK && contains_result(writer.get(), "unavailable"))
	return 0;
    CHECK(exported == BRLCAD_OK);
    CHECK(run(writer.get(), status) == BRLCAD_OK && contains_result(writer.get(), ("last export " + branch).c_str()) &&
	contains_result(writer.get(), "last export time "));
    Ged plain_db(ged_open("db", plain.c_str(), 1), ged_close);
    CHECK(plain_db && run(plain_db.get(), status) == BRLCAD_OK && contains_result(plain_db.get(), "mode disabled") &&
	run(plain_db.get(), title) == BRLCAD_OK && contains_result(plain_db.get(), "staged conflict"));
    const auto archive = (fixture.path / "archive.g").string();
    const char *export_archive[] = {"db", "history", "export", "--with-history", archive.c_str(), nullptr};
    CHECK(run(writer.get(), export_archive) == BRLCAD_OK);
    const char *overwrite_archive[] = {"dump", archive.c_str(), nullptr};
    CHECK(run(legacy.get(), overwrite_archive) != BRLCAD_OK && contains_result(legacy.get(), "already exists"));
    const char *overwrite_source[] = {"dump", source.c_str(), nullptr};
    CHECK(run(legacy.get(), overwrite_source) != BRLCAD_OK && contains_result(legacy.get(), "already exists"));
    const auto dumped = (fixture.path / "dumped.g").string();
    const char *dump_workspace[] = {"dump", dumped.c_str(), nullptr};
    CHECK(run(writer.get(), dump_workspace) == BRLCAD_OK && selected_id(writer.get()) == branch);
    Ged dumped_db(ged_open("db", dumped.c_str(), 1), ged_close);
    CHECK(dumped_db && selected_id(dumped_db.get()) == branch && run(dumped_db.get(), verify) == BRLCAD_OK);
    writer.reset(ged_open("db", workspace.c_str(), 1));
    CHECK(writer && run(writer.get(), status) == BRLCAD_OK && contains_result(writer.get(), ("last export " + branch).c_str()));
    CHECK(run(writer.get(), status_json) == BRLCAD_OK);
    const auto exported_json = nlohmann::json::parse(bu_vls_addr(writer->ged_result_str), nullptr, false);
    CHECK(!exported_json.is_discarded() && exported_json["last_export"] == branch &&
	exported_json["last_export_time"].is_number_integer() && exported_json["last_export_time"].get<int64_t>() > 0);
    Ged archived(ged_open("db", archive.c_str(), 1), ged_close);
    CHECK(archived && selected_id(archived.get()) == branch && run(archived.get(), show) == BRLCAD_OK);
    const auto copy = (fixture.path / "copy").string();
    const char *import_archive[] = {"db", "history", "workspace", copy.c_str(), nullptr};
    CHECK(run(archived.get(), import_archive) == BRLCAD_OK && contains_result(archived.get(), "mode workspace") &&
	selected_id(archived.get()) == branch && run(archived.get(), show) == BRLCAD_OK);
    const auto cloned = (fixture.path / "cloned").string();
    const char *copy_workspace[] = {"db", "history", "workspace", cloned.c_str(), nullptr};
    CHECK(run(archived.get(), copy_workspace) == BRLCAD_OK && selected_id(archived.get()) == branch &&
	run(archived.get(), verify) == BRLCAD_OK);

    const char *preview[] = {"db", "history", "prune", "--keep", "1", "--drop", renamed.c_str(), nullptr};
    const int previewed = run(archived.get(), preview);
    if (previewed != BRLCAD_OK && contains_result(archived.get(), "unavailable on this platform"))
	return 0;
    CHECK(previewed == BRLCAD_OK && contains_result(archived.get(), ("expire revision " + renamed).c_str()) &&
	contains_result(archived.get(), ("retention boundary " + branch).c_str()));
    const std::string_view result(bu_vls_addr(archived->ged_result_str));
    constexpr std::string_view field = "preview ";
    CHECK(result.substr(0, field.size()) == field);
    const auto token = std::string(result.substr(field.size(), result.find('\n') - field.size()));
    CHECK(run(archived.get(), show) == BRLCAD_OK);
    const char *apply[] = {"db", "history", "prune", "--keep", "1", "--drop", renamed.c_str(),
	"--apply", token.c_str(), nullptr};
    CHECK(run(archived.get(), apply) == BRLCAD_OK && contains_result(archived.get(), "Prune applied") &&
	selected_id(archived.get()) == branch);
    CHECK(run(archived.get(), show) != BRLCAD_OK && contains_result(archived.get(), "Unknown revision"));
    const char *show_mark[] = {"db", "history", "show", "rename", nullptr};
    CHECK(run(archived.get(), show_mark) == BRLCAD_OK && contains_result(archived.get(), "(retention boundary)"));
    CHECK(run(archived.get(), undo) != BRLCAD_OK && contains_result(archived.get(), "retention boundary"));
    CHECK(run(archived.get(), verify) == BRLCAD_OK);
    CHECK(run(archived.get(), status_json) == BRLCAD_OK);
    const auto pruned_json = nlohmann::json::parse(bu_vls_addr(archived->ged_result_str), nullptr, false);
    CHECK(!pruned_json.is_discarded() && pruned_json["revision"] == branch &&
	pruned_json["retention_boundary"] == true && pruned_json["undo"].is_null());
    return 0;
}


static int
test_atomic_batch()
{
    char path[MAXPATHLEN] = {};
    FILE *file = bu_temp_file(path, sizeof(path));
    CHECK(file && fclose(file) == 0);
    auto *db = db_create(path, 5);
    CHECK(db);
    db_close(db);
    const std::unique_ptr<struct ged, decltype(&ged_close)> owner(ged_open("db", path, 1), ged_close);
    auto *gedp = owner.get();
    CHECK(gedp);
    const std::string original_title = gedp->dbip->dbi_title;

    const char *batch[] = {"db", "history", "batch", "Build assembly",
	"{make a.s sph} {make b.s sph} {g assembly.c a.s b.s} {title {An assembly with two parts}}", nullptr};
    CHECK(run(gedp, batch) != BRLCAD_OK && contains_result(gedp, "Enable database history"));
    CHECK(!db_lookup(gedp->dbip, "a.s", LOOKUP_QUIET));

    const char *enable[] = {"db", "history", "enable", "session", nullptr};
    const int enabled = run(gedp, enable);
    if (enabled != BRLCAD_OK && contains_result(gedp, "Atomic DB5 replacement is unavailable"))
	return 0;
    CHECK(enabled == BRLCAD_OK);
    int commits = 0;
    CHECK(ged_event_observer_add(gedp, GED_EVENT_OBSERVER_POST_RECONCILE, count_commit, &commits));
    CHECK(run(gedp, batch) == BRLCAD_OK && commits == 1 && selected_id(gedp) == "1");
    CHECK(db_lookup(gedp->dbip, "assembly.c", LOOKUP_QUIET) &&
	BU_STR_EQUAL(gedp->dbip->dbi_title, "An assembly with two parts"));
    const auto *status = ged_history_status_create(gedp, nullptr);
    const bool named_undo = status && BU_STR_EQUAL(status->undo.label, "Build assembly");
    ged_history_status_destroy(status);
    CHECK(named_undo);

    const char *failed[] = {"db", "history", "batch", "Late failure",
	"{make transient.s sph} {mv a.s assembly.c}", nullptr};
    CHECK(run(gedp, failed) != BRLCAD_OK && contains_result(gedp, "Batch command 2 failed"));
    CHECK(!db_lookup(gedp->dbip, "transient.s", LOOKUP_QUIET) && selected_id(gedp) == "1" && commits == 1);

    const char *prompt[] = {"db", "history", "batch", "Incomplete command",
	"{make transient.s sph} {in incomplete.s}", nullptr};
    CHECK(run(gedp, prompt) != BRLCAD_OK && contains_result(gedp, "Batch command 2 requires more input"));
    CHECK(!db_lookup(gedp->dbip, "transient.s", LOOKUP_QUIET) && selected_id(gedp) == "1" && commits == 1);

    const char *malformed[] = {"db", "history", "batch", "Invalid list", "{make transient.s sph", nullptr};
    CHECK(run(gedp, malformed) != BRLCAD_OK && contains_result(gedp, "Expected a nonempty list"));
    CHECK(!db_lookup(gedp->dbip, "transient.s", LOOKUP_QUIET) && selected_id(gedp) == "1" && commits == 1);

    const char *undo[] = {"db", "undo", nullptr};
    CHECK(run(gedp, undo) == BRLCAD_OK && commits == 2 && selected_id(gedp) == "0");
    CHECK(!db_lookup(gedp->dbip, "a.s", LOOKUP_QUIET) && !db_lookup(gedp->dbip, "b.s", LOOKUP_QUIET) &&
	!db_lookup(gedp->dbip, "assembly.c", LOOKUP_QUIET) && original_title == gedp->dbip->dbi_title);
    const char *redo[] = {"db", "redo", nullptr};
    CHECK(run(gedp, redo) == BRLCAD_OK && commits == 3 && selected_id(gedp) == "1");
    CHECK(db_lookup(gedp->dbip, "assembly.c", LOOKUP_QUIET));
    return 0;
}

static int
test_history_concat()
{
    char source_path[MAXPATHLEN] = {};
    FILE *source_file = bu_temp_file(source_path, sizeof(source_path));
    CHECK(source_file && fclose(source_file) == 0);
    auto *source_db = db_create(source_path, 5);
    CHECK(source_db);
    db_close(source_db);
    const char *enable[] = {"db", "history", "enable", "embedded", nullptr};
    {
	const std::unique_ptr<struct ged, decltype(&ged_close)> source(ged_open("db", source_path, 1), ged_close);
	CHECK(source);
	const char *make[] = {"make", "body.s", "sph", nullptr};
	const char *group[] = {"g", "assembly.c", "body.s", nullptr};
	CHECK(run(source.get(), make) == BRLCAD_OK && run(source.get(), group) == BRLCAD_OK);
	const int enabled = run(source.get(), enable);
	if (enabled != BRLCAD_OK && contains_result(source.get(), "Atomic DB5 replacement is unavailable"))
	    return 0;
	CHECK(enabled == BRLCAD_OK);
    }

    char target_path[MAXPATHLEN] = {};
    FILE *target_file = bu_temp_file(target_path, sizeof(target_path));
    CHECK(target_file && fclose(target_file) == 0);
    auto *target_db = db_create(target_path, 5);
    CHECK(target_db);
    db_close(target_db);
    const std::unique_ptr<struct ged, decltype(&ged_close)> owner(ged_open("db", target_path, 1), ged_close);
    auto *gedp = owner.get();
    CHECK(gedp && run(gedp, enable) == BRLCAD_OK);
    const auto root = selected_id(gedp);
    CHECK(!root.empty());

    const char *concat[] = {"concat", source_path, "imported_", nullptr};
    int staged_writes = 0;
    CHECK(ged_clbk_set(gedp, "concat", BU_CLBK_PRE, fail_staged_edit, &staged_writes) == BRLCAD_OK);
    CHECK(run(gedp, concat) != BRLCAD_OK && staged_writes == 1 && selected_id(gedp) == root);
    CHECK(!db_lookup(gedp->dbip, "imported_body.s", LOOKUP_QUIET) &&
	!db_lookup(gedp->dbip, "imported_assembly.c", LOOKUP_QUIET));
    CHECK(ged_clbk_set(gedp, "concat", BU_CLBK_PRE, nullptr, nullptr) == GED_OVERRIDE);

    int commits = 0;
    CHECK(ged_event_observer_add(gedp, GED_EVENT_OBSERVER_POST_RECONCILE, count_commit, &commits));
    CHECK(run(gedp, concat) == BRLCAD_OK && commits == 1);
    const auto tip = selected_id(gedp);
    auto *body = db_lookup(gedp->dbip, "imported_body.s", LOOKUP_QUIET);
    auto *assembly = db_lookup(gedp->dbip, "imported_assembly.c", LOOKUP_QUIET);
    CHECK(!tip.empty() && tip != root && body && assembly &&
	db_comb_has_instance(nullptr, gedp->dbip, assembly, body, 0));
    CHECK(!db_lookup(gedp->dbip, "imported__BRLCAD_HISTORY", LOOKUP_QUIET));

    const char *undo[] = {"db", "undo", nullptr};
    const char *redo[] = {"db", "redo", nullptr};
    CHECK(run(gedp, undo) == BRLCAD_OK && selected_id(gedp) == root &&
	!db_lookup(gedp->dbip, "imported_body.s", LOOKUP_QUIET));
    CHECK(run(gedp, redo) == BRLCAD_OK && selected_id(gedp) == tip &&
	db_lookup(gedp->dbip, "imported_body.s", LOOKUP_QUIET));

    const char *keep_to_source[] = {"keep", target_path, "imported_body.s", nullptr};
    const char *keep_to_archive[] = {"keep", source_path, "imported_body.s", nullptr};
    CHECK(run(gedp, keep_to_source) != BRLCAD_OK && contains_result(gedp, "other than the open database"));
    CHECK(run(gedp, keep_to_archive) != BRLCAD_OK && contains_result(gedp, "writable plain database"));
    char subset_path[MAXPATHLEN] = {};
    FILE *subset_file = bu_temp_file(subset_path, sizeof(subset_path));
    CHECK(subset_file && fclose(subset_file) == 0 && bu_file_delete(subset_path));
    body = db_lookup(gedp->dbip, "imported_body.s", LOOKUP_QUIET);
    CHECK(body);
    const auto references = body->d_nref;
    const char *keep[] = {"db", "keep", subset_path, "imported_assembly.c", "_BRLCAD_HISTORY", nullptr};
    CHECK(run(gedp, keep) == BRLCAD_OK && body->d_nref == references && selected_id(gedp) == tip);
    const std::unique_ptr<struct ged, decltype(&ged_close)> subset(ged_open("db", subset_path, 1), ged_close);
    CHECK(subset && db_lookup(subset->dbip, "imported_assembly.c", LOOKUP_QUIET) &&
	db_lookup(subset->dbip, "imported_body.s", LOOKUP_QUIET) &&
	!db_lookup(subset->dbip, "_BRLCAD_HISTORY", LOOKUP_QUIET));
    return 0;
}

static fastf_t
test_torus_radius(struct ged *gedp)
{
    auto *dp = db_lookup(gedp->dbip, "torus.s", LOOKUP_QUIET);
    struct rt_db_internal internal = RT_DB_INTERNAL_INIT_ZERO;
    if (!dp || rt_db_get_internal(&internal, dp, gedp->dbip, nullptr) < 0)
	return -1;
    const auto *torus = static_cast<const struct rt_tor_internal *>(internal.idb_ptr);
    const fastf_t radius = internal.idb_type == ID_TOR ? torus->r_a : -1;
    rt_db_free_internal(&internal);
    return radius;
}

static int
decline_replacement(struct ged *gedp, void *UNUSED(data))
{
    bu_vls_strcat(gedp->ged_result_str, "Test host declined database replacement");
    return BRLCAD_ERROR;
}

static int
test_history_edit_session()
{
    char path[MAXPATHLEN] = {};
    FILE *file = bu_temp_file(path, sizeof(path));
    CHECK(file && fclose(file) == 0);
    auto *db = db_create(path, 5);
    CHECK(db);
    db_close(db);
    const std::unique_ptr<struct ged, decltype(&ged_close)> owner(ged_open("db", path, 1), ged_close);
    auto *gedp = owner.get();
    const char *make[] = {"make", "torus.s", "tor", nullptr};
    const char *attribute[] = {"attr", "set", "torus.s", "test_note", "preserve", nullptr};
    const char *enable[] = {"db", "history", "enable", "embedded", nullptr};
    CHECK(gedp && run(gedp, make) == BRLCAD_OK && run(gedp, attribute) == BRLCAD_OK);
    const int enabled = run(gedp, enable);
    if (enabled != BRLCAD_OK && contains_result(gedp, "Atomic DB5 replacement is unavailable"))
	return 0;
    CHECK(enabled == BRLCAD_OK);
    const std::string root = selected_id(gedp);
    const fastf_t before = test_torus_radius(gedp);
    CHECK(!root.empty() && before > 0);

    ged_edit_session_ref edit = GED_EDIT_SESSION_REF_NULL;
    ged_edit_session_ref other = GED_EDIT_SESSION_REF_NULL;
    CHECK(ged_edit_session_begin(gedp, "torus.s", GED_EDIT_TARGET_OBJECT, &edit) == GED_EDIT_OK);
    constexpr int torus_major_radius = 1021;
    const fastf_t value[] = {before * 2};
    struct ged_edit_command_input input = {};
    input.command_id = torus_major_radius;
    input.values = value;
    input.value_count = 1;
    CHECK(ged_edit_session_apply(gedp, edit, &input) == GED_EDIT_OK);
    const auto *editing = ged_history_status_create(gedp, nullptr);
    const bool active = editing && editing->edit_active;
    ged_history_status_destroy(editing);
    CHECK(active && selected_id(gedp) == root && NEAR_EQUAL(test_torus_radius(gedp), before, SMALL_FASTF));
    CHECK(ged_edit_session_begin(gedp, "torus.s", GED_EDIT_TARGET_OBJECT, &other) == GED_EDIT_OK);
    CHECK(ged_edit_session_commit(gedp, edit) == GED_EDIT_CONFLICT);
    struct ged_edit_session_info info = {};
    CHECK(ged_edit_session_info_get(gedp, edit, &info) == GED_EDIT_OK && info.dirty && selected_id(gedp) == root);
    CHECK(ged_edit_session_cancel(gedp, other) == GED_EDIT_OK);

    ged_history_set_replacement_handlers(gedp, decline_replacement, nullptr, nullptr);
    CHECK(ged_edit_session_commit(gedp, edit) == GED_EDIT_ERROR && contains_result(gedp, "Test host declined"));
    CHECK(ged_edit_session_info_get(gedp, edit, &info) == GED_EDIT_OK && info.dirty && selected_id(gedp) == root);
    ged_history_set_replacement_handlers(gedp, nullptr, nullptr, nullptr);
    CHECK(ged_edit_session_commit(gedp, edit) == GED_EDIT_OK &&
	ged_edit_session_info_get(gedp, edit, &info) == GED_EDIT_STALE &&
	NEAR_EQUAL(test_torus_radius(gedp), value[0], SMALL_FASTF));
    const auto tip = selected_id(gedp);
    CHECK(!tip.empty() && tip != root);
    const char *get_attribute[] = {"attr", "get", "torus.s", "test_note", nullptr};
    CHECK(run(gedp, get_attribute) == BRLCAD_OK && contains_result(gedp, "preserve"));

    const char *undo[] = {"db", "undo", nullptr};
    const char *redo[] = {"db", "redo", nullptr};
    CHECK(run(gedp, undo) == BRLCAD_OK && selected_id(gedp) == root &&
	NEAR_EQUAL(test_torus_radius(gedp), before, SMALL_FASTF));
    CHECK(run(gedp, redo) == BRLCAD_OK && selected_id(gedp) == tip &&
	NEAR_EQUAL(test_torus_radius(gedp), value[0], SMALL_FASTF));
    CHECK(ged_edit_session_begin(gedp, "torus.s", GED_EDIT_TARGET_OBJECT, &edit) == GED_EDIT_OK &&
	ged_edit_session_commit(gedp, edit) == GED_EDIT_OK && selected_id(gedp) == tip);

    constexpr fastf_t accepted_radius = 3.0;
    const auto cli_radius = std::to_string(accepted_radius);
    const char *prepare[] = {"edit", "-i", "torus.s", "r1", cli_radius.c_str(), nullptr};
    const char *status[] = {"edit", "torus.s", "status", nullptr};
    const char *commit[] = {"edit", "torus.s", "commit", nullptr};
    const char *cancel[] = {"edit", "torus.s", "cancel", nullptr};
    const char *immediate[] = {"edit", "torus.s", "r1", cli_radius.c_str(), nullptr};
    CHECK(run(gedp, immediate) == BRLCAD_ERROR && contains_result(gedp, "Use edit -i") && selected_id(gedp) == tip);
    CHECK(run(gedp, prepare) == BRLCAD_OK && run(gedp, status) == BRLCAD_OK && contains_result(gedp, "dirty=1"));
    CHECK(selected_id(gedp) == tip && NEAR_EQUAL(test_torus_radius(gedp), value[0], SMALL_FASTF));
    CHECK(run(gedp, cancel) == BRLCAD_OK && selected_id(gedp) == tip);
    CHECK(run(gedp, prepare) == BRLCAD_OK && run(gedp, commit) == BRLCAD_OK &&
	NEAR_EQUAL(test_torus_radius(gedp), accepted_radius, SMALL_FASTF));
    CHECK(run(gedp, undo) == BRLCAD_OK && selected_id(gedp) == tip &&
	NEAR_EQUAL(test_torus_radius(gedp), value[0], SMALL_FASTF));
    return 0;
}


int
main(int argc, const char *argv[])
{
    if (argc > 0)
	bu_setprogname(argv[0]);

    auto *base = db_create_inmem();
    auto *seed = open_inmem(base);
    const char *make[] = {"make", "extrusion.s", "extrude", nullptr};
    const char *group[] = {"g", "assembly.c", "skt_0", "extrusion.s", nullptr};
    CHECK(seed && run(seed, make) == BRLCAD_OK && run(seed, group) == BRLCAD_OK);
    db_clone_dbi(base, nullptr);
    ged_close(seed);
    base->dbi_read_only = 1;

    const char *rename[] = {"db", "mvall", "skt_0", "outline.s", nullptr};
    auto *edit = db_edit_begin(base, "rename");
    CHECK(edit);
    auto *view = db_clone_dbi(db_edit_view(edit), nullptr);
    auto *gedp = open_inmem(view);
    const char *list[] = {"ls", nullptr};
    CHECK(gedp && run(gedp, list) == BRLCAD_OK);
    const std::string_view listed(bu_vls_addr(gedp->ged_result_str));
    CHECK(listed.find("skt_0") != std::string_view::npos);
    const char *temporary_rename[] = {"db", "mvall", "skt_0", "intermediate.s", nullptr};
    const char *final_rename[] = {"db", "mvall", "intermediate.s", "outline.s", nullptr};
    CHECK(run(gedp, temporary_rename) == BRLCAD_OK && run(gedp, final_rename) == BRLCAD_OK);

    int changed = 0;
    CHECK(db_edit_visit(edit, count_change, &changed) == 0 && changed == 4);
    std::string renamed;
    CHECK(db_edit_visit_renames(edit, record_rename, &renamed) == 0 && renamed == "skt_0 -> outline.s");
    CHECK(db_lookup(base, "skt_0", LOOKUP_QUIET));
    CHECK(!db_lookup(base, "outline.s", LOOKUP_QUIET));
    ged_close(gedp);

    auto *published = db_edit_commit(edit);
    CHECK(published && db_lookup(published, "outline.s", LOOKUP_QUIET));
    CHECK(!db_lookup(published, "skt_0", LOOKUP_QUIET));
    db_close(published);

    edit = db_edit_begin(base, "rename a replacement");
    CHECK(edit);
    auto *staged = db_edit_view(edit);
    auto *sketch = db_lookup(staged, "skt_0", LOOKUP_QUIET);
    CHECK(sketch && db_dirdelete(staged, sketch) == 0);
    gedp = open_inmem(db_clone_dbi(staged, nullptr));
    const char *make_replacement[] = {"make", "skt_0", "sph", nullptr};
    const char *move_replacement[] = {"mv", "skt_0", "outline.s", nullptr};
    CHECK(gedp && run(gedp, make_replacement) == BRLCAD_OK && run(gedp, move_replacement) == BRLCAD_OK);
    renamed.clear();
    CHECK(db_edit_visit_renames(edit, record_rename, &renamed) == 0 && renamed.empty());
    ged_close(gedp);
    db_edit_abort(edit);

    edit = db_edit_begin(base, "rename with write failure");
    CHECK(edit);
    view = db_clone_dbi(db_edit_view(edit), nullptr);
    gedp = open_inmem(view);
    int staged_writes = 0;
    CHECK(gedp && db_add_changed_clbk(view, fail_after_write, &staged_writes) == 0);
    CHECK(run(gedp, rename) != BRLCAD_OK && staged_writes == 1);
    ged_close(gedp);
    CHECK(!db_edit_commit(edit));
    db_edit_abort(edit);
    auto *original = open_inmem(base);
    const char *get[] = {"get", "extrusion.s", "S", nullptr};
    CHECK(original);
    const int get_result = run(original, get);
    if (get_result != BRLCAD_OK)
	bu_log("%s\n", bu_vls_addr(original->ged_result_str));
    CHECK(get_result == BRLCAD_OK);
    CHECK(BU_STR_EQUAL(bu_vls_addr(original->ged_result_str), "skt_0"));
    ged_close(original);
    const int session_status = test_disk_history();
    if (session_status)
	return session_status;
    const int embedded_status = test_embedded_history();
    if (embedded_status)
	return embedded_status;
    const int promotion_status = test_session_promotion();
    if (promotion_status)
	return promotion_status;
    const int batch_status = test_atomic_batch();
    if (batch_status)
	return batch_status;
    const int concat_status = test_history_concat();
    if (concat_status)
	return concat_status;
    const int edit_status = test_history_edit_session();
    return edit_status ? edit_status : test_workspace_history();
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
