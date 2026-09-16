/*                 T E S T _ S T A T I C _ I N I T . C P P
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 *
 * 3. The name of the author may not be used to endorse or promote
 * products derived from this software without specific prior written
 * permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "common.h"

#include <cstdio>
#include <cstdlib>

#include "bu/app.h"
#include "bu/log.h"
#include "bu/parallel.h"


extern "C" {
extern int BU_SEM_THREAD;
extern int BU_SEM_MALLOC;
extern int BU_SEM_DATETIME;
extern int BU_SEM_DIR;
extern int BU_SEM_LOG_HOOK;
}


static int constructor_status = 0;
static int early_user_semaphore = -1;
static int hook_calls = 0;


static int
test_hook(void *, void *)
{
    hook_calls++;
    return 0;
}


struct early_libbu_client {
    early_libbu_client()
    {
	if (BU_SEM_GENERAL != BU_SEM_ID_GENERAL
	    || BU_SEM_SYSCALL != BU_SEM_ID_SYSCALL
	    || BU_SEM_MAPPEDFILE != BU_SEM_ID_MAPPEDFILE
	    || BU_SEM_THREAD != BU_SEM_ID_THREAD
	    || BU_SEM_MALLOC != BU_SEM_ID_MALLOC
	    || BU_SEM_DATETIME != BU_SEM_ID_DATETIME
	    || BU_SEM_DIR != BU_SEM_ID_DIR
	    || BU_SEM_LOG_HOOK != BU_SEM_ID_LOG_HOOK)
	    constructor_status = 1;

	early_user_semaphore = bu_semaphore_register("bu_static_init_test");
	if (early_user_semaphore < BU_SEM_DYNAMIC_BASE)
	    constructor_status = 2;

	/* The historical initialization-order bug deadlocked in this call. */
	bu_log_add_hook(test_hook, NULL);
    }
};


/*
 * Construct this object before the first registry user so its destructor runs
 * after any ordinary function-local registry object would have been
 * destroyed.  The registry is intentionally process-lifetime state, so this
 * late call must remain valid.
 */
struct late_destructor_client {
    ~late_destructor_client()
    {
	int semaphore = bu_semaphore_register("bu_static_destructor_test");
	if (semaphore < BU_SEM_DYNAMIC_BASE)
	    std::abort();
	bu_semaphore_acquire(semaphore);
	bu_semaphore_release(semaphore);
    }
};


#if defined(_MSC_VER)
#  pragma init_seg(lib)
#endif

#if defined(__GNUC__) && !defined(_WIN32)
static late_destructor_client destructor_client __attribute__((init_priority(101)));
static early_libbu_client early_client __attribute__((init_priority(102)));
#else
static late_destructor_client destructor_client;
static early_libbu_client early_client;
#endif


int
main(int UNUSED(argc), char **argv)
{
    bu_setprogname(argv[0]);

    int calls_before;

    if (constructor_status) {
	std::fprintf(stderr, "early initialization failed: %d\n", constructor_status);
	return 1;
    }

#define BU_SEMAPHORE_CHECK_NAME(_id, _symbol, _name) \
    if (bu_semaphore_register((_name)) != (_id)) { \
	std::fprintf(stderr, "reserved semaphore mapping failed for %s\n", (_name)); \
	return 1; \
    }
    BU_SEMAPHORE_RESERVED_LIST(BU_SEMAPHORE_CHECK_NAME)
#undef BU_SEMAPHORE_CHECK_NAME

    calls_before = hook_calls;
    bu_log("static initialization test\n");
    if (hook_calls != calls_before + 1) {
	std::fprintf(stderr, "early log hook was not called\n");
	return 1;
    }

    /* Native locks may be freed and recreated without changing IDs. */
    bu_semaphore_free();
    if (bu_semaphore_register("bu_static_init_test") != early_user_semaphore) {
	std::fprintf(stderr, "dynamic semaphore mapping changed after free\n");
	return 1;
    }

    bu_log_delete_hook(test_hook, NULL);
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
