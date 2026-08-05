/*                     S T A T I C _ I N I T . C P P
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

#include "bu/app.h"
#include "bu/parallel.h"
#include "rt/resource.h"


static int constructor_status = 0;


struct early_librt_client {
    early_librt_client()
    {
	const int semaphores[] = {
	    RT_SEM_WORKER, RT_SEM_RESULTS, RT_SEM_MODEL,
	    RT_SEM_TREE0, RT_SEM_TREE1, RT_SEM_TREE2, RT_SEM_TREE3
	};

	if (RT_SEM_WORKER != BU_SEM_ID_RT_WORKER
	    || RT_SEM_RESULTS != BU_SEM_ID_RT_RESULTS
	    || RT_SEM_MODEL != BU_SEM_ID_RT_MODEL
	    || RT_SEM_TREE0 != BU_SEM_ID_RT_TREE0
	    || RT_SEM_TREE1 != BU_SEM_ID_RT_TREE1
	    || RT_SEM_TREE2 != BU_SEM_ID_RT_TREE2
	    || RT_SEM_TREE3 != BU_SEM_ID_RT_TREE3) {
	    constructor_status = 1;
	    return;
	}

	for (int semaphore : semaphores) {
	    bu_semaphore_acquire(semaphore);
	    bu_semaphore_release(semaphore);
	}
    }
};


#if defined(_MSC_VER)
#  pragma init_seg(lib)
#endif

#if defined(__GNUC__) && !defined(_WIN32)
static early_librt_client early_client __attribute__((init_priority(101)));
#else
static early_librt_client early_client;
#endif


int
main(int UNUSED(argc), char **argv)
{
    bu_setprogname(argv[0]);

    if (constructor_status)
	return 1;

    if (bu_semaphore_register("RT_SEM_WORKER") != BU_SEM_ID_RT_WORKER
	|| bu_semaphore_register("RT_SEM_RESULTS") != BU_SEM_ID_RT_RESULTS
	|| bu_semaphore_register("RT_SEM_MODEL") != BU_SEM_ID_RT_MODEL
	|| bu_semaphore_register("RT_SEM_TREE0") != BU_SEM_ID_RT_TREE0
	|| bu_semaphore_register("RT_SEM_TREE1") != BU_SEM_ID_RT_TREE1
	|| bu_semaphore_register("RT_SEM_TREE2") != BU_SEM_ID_RT_TREE2
	|| bu_semaphore_register("RT_SEM_TREE3") != BU_SEM_ID_RT_TREE3)
	return 1;

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
