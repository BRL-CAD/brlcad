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

#include <filesystem>
#include <memory>
#include <string>
#include <string_view>

#include "bu/app.h"
#include "bu/file.h"
#include "ged.h"
#include "ged/event.h"

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
    CHECK(external && run(external.get(), title) == BRLCAD_OK);
    CHECK(run(gedp, undo) != BRLCAD_OK && contains_result(gedp, "changed since staging began"));
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
    const char *expired[] = {"db", "history", "checkout", "0", nullptr};
    CHECK(run(bounded.get(), expired) != BRLCAD_OK && contains_result(bounded.get(), "expired"));
    CHECK(run(bounded.get(), undo) == BRLCAD_OK);
    CHECK(run(bounded.get(), query_title) == BRLCAD_OK && contains_result(bounded.get(), "five"));
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

    Db readonly(db_open(path, DB_OPEN_READONLY), db_close);
    CHECK(readonly && db_dirbuild(readonly.get()) == 0);
    Ged inspector(open_handle("disk", readonly.get()), ged_close);
    CHECK(inspector);
    readonly.release();
    CHECK(run(inspector.get(), status) == BRLCAD_OK && contains_result(inspector.get(), "access read-only"));
    CHECK(run(inspector.get(), show) == BRLCAD_OK);
    const char *verify[] = {"db", "history", "verify", nullptr};
    CHECK(run(inspector.get(), verify) == BRLCAD_OK && contains_result(inspector.get(), "Archive verified"));
    CHECK(run(inspector.get(), undo) != BRLCAD_OK && contains_result(inspector.get(), "writable"));
    const char *missing_path[] = {"db", "history", "export", "--with-history", nullptr};
    CHECK(run(inspector.get(), missing_path) == GED_HELP);
    char archive_copy[MAXPATHLEN] = {};
    file = bu_temp_file(archive_copy, sizeof(archive_copy));
    CHECK(file && fclose(file) == 0);
    const char *export_archive[] = {"db", "history", "export", archive_copy, "--with-history", nullptr};
    CHECK(run(inspector.get(), export_archive) != BRLCAD_OK && contains_result(inspector.get(), "already exists"));
    CHECK(bu_file_delete(archive_copy));
    CHECK(run(inspector.get(), export_archive) == BRLCAD_OK);
    Ged archived(ged_open("db", archive_copy, 1), ged_close);
    CHECK(archived && selected_id(archived.get()) == fork && run(archived.get(), show) == BRLCAD_OK);
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
    CHECK(run(gedp, undo) == BRLCAD_OK && run(gedp, undo) == BRLCAD_OK);
    CHECK(run(gedp, second) == BRLCAD_OK && run(gedp, undo) == BRLCAD_OK);
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
    const char *show[] = {"db", "history", "show", renamed.c_str(), nullptr};
    CHECK(run(writer.get(), show) == BRLCAD_OK && contains_result(writer.get(), " -> outline.s") &&
	contains_result(writer.get(), "update extrusion.s") && contains_result(writer.get(), "update assembly.c"));
    const char *mark[] = {"db", "history", "mark", "rename", nullptr};
    CHECK(run(writer.get(), mark) == BRLCAD_OK && selected_id(writer.get()) == renamed);
    CHECK(run(writer.get(), mark) != BRLCAD_OK && contains_result(writer.get(), "already exists"));
    const char *log[] = {"db", "history", "log", nullptr};
    CHECK(run(writer.get(), log) == BRLCAD_OK && contains_result(writer.get(), " rename"));
    const char *diff[] = {"db", "history", "diff", root.c_str(), "rename", "--object", "extrusion.s", nullptr};
    CHECK(run(writer.get(), diff) == BRLCAD_OK && contains_result(writer.get(), "update extrusion.s") &&
	!contains_result(writer.get(), "outline.s"));

    const char *staged_title[] = {"title", "staged conflict", nullptr};
    CHECK(run(stale.get(), staged_title) != BRLCAD_OK && contains_result(stale.get(), "Workspace base changed") &&
	contains_result(stale.get(), root.c_str()) && contains_result(stale.get(), renamed.c_str()));
    CHECK(run(stale.get(), status) == BRLCAD_OK && contains_result(stale.get(), "status stale") &&
	contains_result(stale.get(), "staged conflict"));
    const char *conflict[] = {"db", "history", "conflict", nullptr};
    CHECK(run(stale.get(), conflict) == BRLCAD_OK && contains_result(stale.get(), "update _GLOBAL"));
    CHECK(run(stale.get(), reference) == BRLCAD_OK && contains_result(stale.get(), old_name.c_str()));
    const char *open[] = {"db", "open", workspace.c_str(), nullptr};
    CHECK(run(stale.get(), open) == BRLCAD_OK && selected_id(stale.get()) == renamed);
    CHECK(run(stale.get(), conflict) != BRLCAD_OK);

    const char *undo[] = {"db", "undo", nullptr};
    const char *redo[] = {"db", "redo", nullptr};
    CHECK(run(writer.get(), undo) == BRLCAD_OK && selected_id(writer.get()) == root);
    writer.reset(ged_open("db", workspace.c_str(), 1));
    CHECK(writer && run(writer.get(), status) == BRLCAD_OK && contains_result(writer.get(), "(preferred)"));
    CHECK(run(writer.get(), redo) == BRLCAD_OK && selected_id(writer.get()) == renamed);
    const char *checkout[] = {"db", "history", "checkout", root.c_str(), nullptr};
    CHECK(run(writer.get(), checkout) == BRLCAD_OK && selected_id(writer.get()) == root);
    CHECK(run(writer.get(), staged_title) == BRLCAD_OK);
    const auto branch = selected_id(writer.get());
    CHECK(!branch.empty() && branch != root && branch != renamed);
    const char *move_mark[] = {"db", "history", "mark", "rename", "--move", nullptr};
    CHECK(run(writer.get(), move_mark) == BRLCAD_OK && selected_id(writer.get()) == branch);
    const char *verify[] = {"db", "history", "verify", nullptr};
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
    CHECK(run(writer.get(), status) == BRLCAD_OK && contains_result(writer.get(), ("last export " + branch).c_str()));
    Ged plain_db(ged_open("db", plain.c_str(), 1), ged_close);
    const char *title[] = {"title", nullptr};
    CHECK(plain_db && run(plain_db.get(), status) == BRLCAD_OK && contains_result(plain_db.get(), "mode disabled") &&
	run(plain_db.get(), title) == BRLCAD_OK && contains_result(plain_db.get(), "staged conflict"));
    const auto archive = (fixture.path / "archive.g").string();
    const char *export_archive[] = {"db", "history", "export", "--with-history", archive.c_str(), nullptr};
    CHECK(run(writer.get(), export_archive) == BRLCAD_OK);
    writer.reset(ged_open("db", workspace.c_str(), 1));
    CHECK(writer && run(writer.get(), status) == BRLCAD_OK && contains_result(writer.get(), ("last export " + branch).c_str()));
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
    return promotion_status ? promotion_status : test_workspace_history();
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
