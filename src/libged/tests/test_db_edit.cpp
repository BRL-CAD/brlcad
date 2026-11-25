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

#include <string_view>

#include "bu/app.h"
#include "ged.h"

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
    return 0;
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
