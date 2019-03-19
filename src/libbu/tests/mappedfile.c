/*                 M A P P E D F I L E . C
 * BRL-CAD
 *
 * Copyright (c) 2011-2019 United States Government as represented by
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

#include <stdio.h>
#include <string.h>

#include "bu.h"

int
test_bu_mapped_file_basic(int long file_cnt)
{
    int status = 0;
    struct bu_vls fname = BU_VLS_INIT_ZERO;
    struct bu_mapped_file *mfp;
    for (long int i = 0; i < file_cnt; i++) {
	long int fint;
	bu_vls_sprintf(&fname, "bu_mapped_file_input_%d", i);
	mfp = bu_open_mapped_file(bu_vls_cstr(&fname), NULL);
	if (!mfp || bu_opt_long(NULL, 1, (const char **)&(mfp->buf), (void *)&fint) == -1 || fint != i) {
	    bu_log("%s -> %ld [FAIL]  (should be: {%ld})\n", bu_vls_cstr(&fname), fint, i);
	    status = 1;
	}
	if (mfp) {
	    bu_close_mapped_file(mfp);
	}
	if (status) {
	    break;
	}
    }
    if (!status) {
	bu_log("Basic bu_mapped_file test: [PASS]\n");
    }
    bu_vls_free(&fname);
    return status;
}

struct bu_mapped_worker_info {
    int file_cnt;
    int status;
};

int
bu_mapped_worker_read(int cpu, long int file_id)
{
    int status = 0;
    long int fint = -1;
    struct bu_vls fname = BU_VLS_INIT_ZERO;
    struct bu_mapped_file *mfp;
    bu_vls_sprintf(&fname, "bu_mapped_file_input_%d", file_id);
    mfp = bu_open_mapped_file(bu_vls_cstr(&fname), NULL);
    if (!mfp || bu_opt_long(NULL, 1, (const char **)&(mfp->buf), (void *)&fint) == -1 || fint != file_id) {
	bu_log("%s(%d) -> %d [FAIL]  (should be: {%d})\n", bu_vls_cstr(&fname), cpu, fint, file_id);
	status = 1;
    }
    if (mfp) {
	bu_close_mapped_file(mfp);
    }
    return status;
}

void
bu_mapped_worker(int cpu, void *data)
{
    struct bu_mapped_worker_info *info = &(((struct bu_mapped_worker_info *)data)[cpu]);
    int order = cpu % 2;
    info->status = 0;
    if (order) {
	for (int i = 0; i < info->file_cnt; i++) {
	    info->status = bu_mapped_worker_read(cpu, i);
	    if (info->status) {
		return;
	    }
	}
    } else {
	for (int i = info->file_cnt - 1; i >= 0; i--) {
	    info->status = bu_mapped_worker_read(cpu, i);
	    if (info->status) {
		return;
	    }

	}
    }
}


int
main(int ac, char *av[])
{
    int file_cnt = 1000;
    int ret = 0;
    FILE *fp = NULL;
    int test_num = 0;
    struct bu_vls fname = BU_VLS_INIT_ZERO;

    if (ac < 2) {
	bu_exit(1, "Usage: %s {test_num}\n", av[0]);
    }

    /* Prepare a set of test input files */
    for (int i = 0; i < file_cnt; i++) {
	bu_vls_sprintf(&fname, "bu_mapped_file_input_%d", i);
	fp = fopen(bu_vls_cstr(&fname), "wb");
	if (!fp) {
	    bu_exit(1, "Unable to create test input file %s\n", bu_vls_cstr(&fname));
	}
	fprintf(fp, "%d", i);
	fclose(fp);
    }

    sscanf(av[1], "%d", &test_num);

    /* basic mapped file IO */
    if (test_num == 1) {
	ret = test_bu_mapped_file_basic(file_cnt);
    }

    /* parallel mapped file IO */
    if (test_num == 2) {
	int ncpus = bu_avail_cpus();
	struct bu_mapped_worker_info *infos = (struct bu_mapped_worker_info *)bu_calloc(ncpus+1, sizeof(struct bu_mapped_worker_info), "parallel data");
	for (int i = 0; i < ncpus; i++) {
	    infos[i].file_cnt = file_cnt;
	    infos[i].status = 0;
	}
	bu_parallel(bu_mapped_worker, ncpus, (void *)infos);
	for (int i = 0; i < ncpus; i++) {
	    if (infos[i].status) {
		ret = 1;
	    }
	}
	bu_free(infos, "free parallel data");
	if (!ret) {
	    bu_log("Parallel bu_mapped_file test: [PASS]\n");
	}
    }

    for (int i = 0; i < file_cnt; i++) {
	bu_vls_sprintf(&fname, "bu_mapped_file_input_%d", i);
	bu_file_delete(bu_vls_cstr(&fname));
    }

    return ret;
}


/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
