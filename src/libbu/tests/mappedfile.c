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


struct bu_mapped_worker_info {
    int file_cnt;
    int status;
};


static int
test_mapped_file_serial(long int file_cnt)
{
    int status = 1;
    struct bu_vls fname = BU_VLS_INIT_ZERO;
    struct bu_mapped_file *mfp;

    for (long int i = 0; i < file_cnt; i++) {
	long int fint = -1;

	bu_vls_sprintf(&fname, "bu_mapped_file_input_%ld", i);
	mfp = bu_open_mapped_file(bu_vls_cstr(&fname), NULL);
	if (!mfp) {
	    bu_log("ERROR: failed to map file %s\n", bu_vls_cstr(&fname));
	    break;
	}

	if (mfp) {
	    fint = strtod(mfp->buf, NULL);
	}
	if (fint != i) {
	    bu_log("%s -> %ld [FAIL]  (should be: %ld)\n", bu_vls_cstr(&fname), fint, i);
	    status = 1;
	}
	bu_close_mapped_file(mfp);

	if (status)
	    break;
    }
    if (!status) {
	bu_log("Serial mapped file test: [PASS]\n");
    }

    bu_vls_free(&fname);

    return status;
}


static int
mapped_worker_read(int cpu, long int file_id)
{
    int status = 0;
    long int fint = -1;
    struct bu_vls fname = BU_VLS_INIT_ZERO;
    struct bu_mapped_file *mfp;

    bu_vls_sprintf(&fname, "bu_mapped_file_input_%ld", file_id);
    mfp = bu_open_mapped_file(bu_vls_cstr(&fname), NULL);

    if (mfp) {
	fint = strtod(mfp->buf, NULL);
    }
    if (fint != file_id) {
	bu_log("%s(%d) -> %ld [FAIL]  (should be: %ld)\n", bu_vls_cstr(&fname), cpu, fint, file_id);
	status = 1;
    }
    bu_close_mapped_file(mfp);

    return status;
}


static void
mapped_worker(int cpu, void *data)
{
    struct bu_mapped_worker_info *info = &(((struct bu_mapped_worker_info *)data)[cpu]);
    int order = cpu % 2;

    info->status = 0;

    if (order) {
	for (int i = 0; i < info->file_cnt; i++) {
	    info->status = mapped_worker_read(cpu, i);
	    if (info->status) {
		return;
	    }
	}
    } else {
	for (int i = info->file_cnt - 1; i >= 0; i--) {
	    info->status = mapped_worker_read(cpu, i);
	    if (info->status) {
		return;
	    }

	}
    }
}


static int
test_mapped_file_parallel(size_t file_cnt)
{
    int ret = 0;
    int ncpus = bu_avail_cpus();
    struct bu_mapped_worker_info *infos = (struct bu_mapped_worker_info *)bu_calloc(ncpus+1, sizeof(struct bu_mapped_worker_info), "parallel data");

    /* parallel mapped file IO */

    for (int i = 0; i < ncpus; i++) {
	infos[i].file_cnt = file_cnt;
	infos[i].status = 0;
    }
    bu_parallel(mapped_worker, ncpus, (void *)infos);
    for (int i = 0; i < ncpus; i++) {
	if (infos[i].status) {
	    ret = 1;
	}
    }
    bu_free(infos, "free parallel data");
    if (!ret) {
	bu_log("Parallel mapped file test: [PASS]\n");
    }

    return ret;
}


int
main(int ac, char *av[])
{
    FILE *fp = NULL;
    int ret;
    long int file_cnt = 100;
    long int test_num = 0;
    const char *file_prefix = "bu_mapped_file_";
    struct bu_vls fname = BU_VLS_INIT_ZERO;

    if (ac < 2 || ac > 3) {
	bu_exit(1, "Usage: %s {test_number} [file_count]\n", av[0]);
    }

    if (ac > 2)
	sscanf(av[2], "%ld", &file_cnt);
    if (file_cnt < 1)
	file_cnt = 1;

    /* Create our set of test input files */
    for (long int i = 0; i < file_cnt; i++) {
	bu_vls_sprintf(&fname, "%s%ld", file_prefix, i);
	fp = fopen(bu_vls_cstr(&fname), "wb");
	if (!fp)
	    bu_exit(1, "Unable to create test input file %s\n", bu_vls_cstr(&fname));
	fprintf(fp, "%ld", i);
	fclose(fp);
	if (!bu_file_exists(bu_vls_cstr(&fname), NULL))
	    bu_exit(1, "Unable to verify test input file %s\n", bu_vls_cstr(&fname));
    }

    /* Run our test */
    sscanf(av[1], "%ld", &test_num);
    switch(test_num) {
    	case 1:
	    ret = test_mapped_file_serial(file_cnt);
	    break;
    	case 2:
	    ret = test_mapped_file_parallel(file_cnt);
	    break;
    }

    /* Delete our set of test input files */
    for (long int i = 0; i < file_cnt; i++) {
	bu_vls_sprintf(&fname, "%s%ld", file_prefix, i);
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
