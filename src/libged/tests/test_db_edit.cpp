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

#include <memory>
#include <string>
#include <string_view>

#include "bu/app.h"
#include "ged.h"
#include "ged/event.h"

#define CHECK(expr) do { \
    if (!(expr)) { \
	bu_log("GED database edit test failed at line %d: %s\n", __LINE__, #expr); \
	return 1; \
    } \
} while (0)

static struct ged *
open_inmem(struct db_i *dbip)
{
    struct bu_vls address = BU_VLS_INIT_ZERO;
    bu_vls_sprintf(&address, "%p", static_cast<void *>(dbip));
    struct ged *gedp = ged_open("inmem", bu_vls_cstr(&address), 0);
    bu_vls_free(&address);
    return gedp;
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
    CHECK(run(gedp, rename) == BRLCAD_OK);

    int changed = 0;
    CHECK(db_edit_visit(edit, count_change, &changed) == 0 && changed == 4);
    CHECK(db_lookup(base, "skt_0", LOOKUP_QUIET));
    CHECK(!db_lookup(base, "outline.s", LOOKUP_QUIET));
    ged_close(gedp);

    auto *published = db_edit_commit(edit);
    CHECK(published && db_lookup(published, "outline.s", LOOKUP_QUIET));
    CHECK(!db_lookup(published, "skt_0", LOOKUP_QUIET));
    db_close(published);

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
    return test_disk_history();
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
