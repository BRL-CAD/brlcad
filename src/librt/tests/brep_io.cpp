/*                         B R E P _ I O . C P P
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 */
/** @file brep_io.cpp
 *
 * Exercise BREP archive round trips, malformed input handling, and
 * concurrent imports.
 */

#include "common.h"

#include <atomic>
#include <thread>
#include <vector>

#include "bu/app.h"
#include "bu/cv.h"
#include "bu/log.h"
#include "raytrace.h"
#include "rt/geom.h"
#include "brep.h"


static int
make_brep_archive(struct bu_external *external)
{
    struct rt_db_internal intern;
    RT_DB_INTERNAL_INIT(&intern);
    intern.idb_major_type = DB5_MAJORTYPE_BRLCAD;
    intern.idb_type = ID_BREP;
    intern.idb_meth = &OBJ[ID_BREP];

    BU_ALLOC(intern.idb_ptr, struct rt_brep_internal);
    struct rt_brep_internal *brep_internal =
	(struct rt_brep_internal *)intern.idb_ptr;
    brep_internal->magic = RT_BREP_INTERNAL_MAGIC;
    brep_internal->brep = ON_BrepSphere(ON_Sphere(ON_3dPoint::Origin, 10.0));
    if (!brep_internal->brep) {
	rt_db_free_internal(&intern);
	return BRLCAD_ERROR;
    }

    int ret = OBJ[ID_BREP].ft_export5(external, &intern, 1.0, NULL);
    rt_db_free_internal(&intern);
    return ret;
}


static int
import_brep_archive(const struct bu_external *external, bool validate)
{
    struct rt_db_internal intern;
    RT_DB_INTERNAL_INIT(&intern);
    if (OBJ[ID_BREP].ft_import5(&intern, external, NULL, NULL) < 0)
	return BRLCAD_ERROR;

    struct rt_brep_internal *brep_internal =
	(struct rt_brep_internal *)intern.idb_ptr;
    int ret = BRLCAD_OK;
    if (!brep_internal || brep_internal->magic != RT_BREP_INTERNAL_MAGIC ||
	!brep_internal->brep || brep_internal->brep->m_F.Count() <= 0 ||
	brep_internal->brep->m_S.Count() <= 0) {
	ret = BRLCAD_ERROR;
    } else if (validate) {
	ON_wString messages;
	ON_TextLog log(messages);
	if (!brep_internal->brep->IsValid(&log) || !brep_internal->brep->IsSolid()) {
	    bu_log("Imported BREP is invalid: %s\n", ON_String(messages).Array());
	    ret = BRLCAD_ERROR;
	}
    }

    rt_db_free_internal(&intern);
    return ret;
}


static int
expect_import_failure(const struct bu_external *external)
{
    struct rt_db_internal intern;
    RT_DB_INTERNAL_INIT(&intern);
    int ret = OBJ[ID_BREP].ft_import5(&intern, external, NULL, NULL);
    if (ret >= 0 || intern.idb_ptr) {
	bu_log("Malformed BREP archive was accepted\n");
	if (intern.idb_ptr)
	    rt_db_free_internal(&intern);
	return BRLCAD_ERROR;
    }
    return BRLCAD_OK;
}


static int
test_adjust(const struct bu_external *external)
{
    struct rt_db_internal intern;
    RT_DB_INTERNAL_INIT(&intern);
    if (OBJ[ID_BREP].ft_import5(&intern, external, NULL, NULL) < 0)
	return BRLCAD_ERROR;

    signed char *encoded = bu_b64_encode_block((const signed char *)external->ext_buf,
	external->ext_nbytes);
    if (!encoded) {
	rt_db_free_internal(&intern);
	return BRLCAD_ERROR;
    }

    struct bu_vls messages = BU_VLS_INIT_ZERO;
    const char *valid_args[] = {(const char *)encoded};
    int ret = OBJ[ID_BREP].ft_adjust(&messages, &intern, 1, valid_args);
    struct rt_brep_internal *brep_internal =
	(struct rt_brep_internal *)intern.idb_ptr;
    if (ret != BRLCAD_OK || !brep_internal->brep ||
	!brep_internal->brep->IsSolid()) {
	bu_log("Valid BREP adjustment failed: %s\n", bu_vls_cstr(&messages));
	ret = BRLCAD_ERROR;
    }

    ON_Brep *valid_brep = brep_internal->brep;
    const char *invalid_args[] = {"not a BREP archive"};
    bu_vls_trunc(&messages, 0);
    if (OBJ[ID_BREP].ft_adjust(&messages, &intern, 1, invalid_args) != BRLCAD_ERROR ||
	brep_internal->brep != valid_brep) {
	bu_log("Invalid BREP adjustment changed the object\n");
	ret = BRLCAD_ERROR;
    }

    bu_vls_free(&messages);
    bu_free(encoded, "encoded BREP archive");
    rt_db_free_internal(&intern);
    return ret;
}


static int
test_concurrent_imports(const struct bu_external *external)
{
    constexpr size_t thread_count = 8;
    constexpr size_t imports_per_thread = 16;
    std::atomic<size_t> ready_count(0);
    std::atomic<bool> start(false);
    std::atomic<size_t> failure_count(0);
    std::vector<std::thread> threads;
    threads.reserve(thread_count);

    for (size_t i = 0; i < thread_count; ++i) {
	threads.emplace_back([&]() {
	    ready_count.fetch_add(1, std::memory_order_release);
	    while (!start.load(std::memory_order_acquire))
		std::this_thread::yield();

	    for (size_t j = 0; j < imports_per_thread; ++j) {
		if (import_brep_archive(external, false) != BRLCAD_OK)
		    failure_count.fetch_add(1, std::memory_order_relaxed);
	    }
	});
    }

    while (ready_count.load(std::memory_order_acquire) != thread_count)
	std::this_thread::yield();
    start.store(true, std::memory_order_release);

    for (std::thread &thread : threads)
	thread.join();

    if (failure_count.load(std::memory_order_relaxed)) {
	bu_log("Concurrent BREP imports failed %zu times\n",
		failure_count.load(std::memory_order_relaxed));
	return BRLCAD_ERROR;
    }
    return BRLCAD_OK;
}


int
main(int argc, char **argv)
{
    bu_setprogname(argv[0]);
    if (argc != 1) {
	bu_log("Usage: %s\n", argv[0]);
	return 1;
    }

    struct bu_external external = BU_EXTERNAL_INIT_ZERO;
    if (make_brep_archive(&external) != BRLCAD_OK) {
	bu_log("Unable to create BREP test archive\n");
	return 1;
    }

    int ret = test_concurrent_imports(&external);
    if (import_brep_archive(&external, true) != BRLCAD_OK)
	ret = BRLCAD_ERROR;
    if (test_adjust(&external) != BRLCAD_OK)
	ret = BRLCAD_ERROR;

    struct bu_external truncated = external;
    truncated.ext_nbytes = external.ext_nbytes / 2;
    if (expect_import_failure(&truncated) != BRLCAD_OK)
	ret = BRLCAD_ERROR;

    uint8_t invalid_data[16] = {0};
    struct bu_external invalid = BU_EXTERNAL_INIT_ZERO;
    invalid.ext_nbytes = sizeof(invalid_data);
    invalid.ext_buf = invalid_data;
    if (expect_import_failure(&invalid) != BRLCAD_OK)
	ret = BRLCAD_ERROR;

    bu_free_external(&external);
    return ret == BRLCAD_OK ? 0 : 1;
}


/*
 * Local Variables:
 * mode: C++
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
