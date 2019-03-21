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


const char *FILE_PREFIX = "bu_mapped_file_";

struct mapped_file_worker_info {
    long int file_cnt;
    long int value;
};


static long int
mapped_file_read_number(long int i)
{
    char filename[MAXPATHLEN] = {0};
    struct bu_mapped_file *mfp;
    double num = -1.0;
    double zero = 0.0;
    char *endptr;

    snprintf(filename, MAXPATHLEN, "%s%ld", FILE_PREFIX, i);

    mfp = bu_open_mapped_file(filename, NULL);
    if (!mfp) {
	bu_log("%s -> [FAIL]  (unable to open mapped file)\n", filename);
	return -1;
    }

    num = strtod((char*)mfp->buf, &endptr);
    bu_close_mapped_file(mfp);

    if (memcmp(&num, &zero, sizeof(double)) == 0 && endptr == mfp->buf) {
	bu_log("%s -> [FAIL]  (unable to read number from file)\n", filename);
	return -2;
    }

    return (long int)num;
}


static int
test_mapped_file_serial(long int file_cnt)
{
    long int num = 0;
    long int i;

    for (i = 0; i < file_cnt; i++) {

	num = mapped_file_read_number(i);
	if (num != i) {
	    bu_log("%s%ld -> %ld [FAIL]  (should be: %ld)\n", FILE_PREFIX, i, num, i);
	    break;
	}
    }
    if (num == file_cnt-1) {
	bu_log("Mapped file serial test: [PASS]\n");
	return 0;
    }

    return 1;
}


static void
mapped_file_worker(int cpu, void *data)
{
    int order = cpu % 2;
    long int i;
    struct mapped_file_worker_info *info = &(((struct mapped_file_worker_info *)data)[cpu]);

    info->value = 0;

    if (order) {
	for (i = 0; i < info->file_cnt; i++) {
	    info->value = mapped_file_read_number(i);
	    if (info->value != i) {
		bu_log("%s%ld -> %ld [FAIL]  (should be: %ld)\n", FILE_PREFIX, i, info->value, i);
		break;
	    }
	}
    } else {
	for (i = info->file_cnt - 1; i >= 0; i--) {
	    info->value = mapped_file_read_number(i);
	    if (info->value != i) {
		bu_log("%s%ld -> %ld [FAIL]  (should be: %ld)\n", FILE_PREFIX, i, info->value, i);
		break;
	    }

	}
    }

    if ((order && info->value == info->file_cnt-1) || (!order && info->value == 0)) {
	info->value = 0;
    } else {
	info->value = 1;
    }
}


static int
test_mapped_file_parallel(size_t file_cnt)
{
    int ret = 0;
    int ncpus = bu_avail_cpus();
    struct mapped_file_worker_info *infos;

    infos = (struct mapped_file_worker_info *)bu_calloc(ncpus+1, sizeof(struct mapped_file_worker_info), "parallel data");

    for (int i = 0; i < ncpus; i++) {
	infos[i].file_cnt = file_cnt;
	infos[i].value = 0;
    }

    bu_parallel(mapped_file_worker, ncpus, (void *)infos);

    for (int i = 0; i < ncpus; i++) {
	if (infos[i].value) {
	    ret = 1;
	    break;
	}
    }

    bu_free(infos, "free parallel data");
    if (ret == 0) {
	bu_log("Mapped file parallel test: [PASS]\n");
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
    struct bu_vls fname = BU_VLS_INIT_ZERO;

    /* bu_debug |= BU_DEBUG_MAPPED_FILE; */

    if (ac < 2 || ac > 3) {
	bu_exit(1, "Usage: %s {test_number} [file_count]\n", av[0]);
    }

    if (ac > 2)
	sscanf(av[2], "%ld", &file_cnt);
    if (file_cnt < 1)
	file_cnt = 1;

    /* Create our set of test input files */
    for (long int i = 0; i < file_cnt; i++) {
	bu_vls_sprintf(&fname, "%s%ld", FILE_PREFIX, i);
	fp = fopen(bu_vls_cstr(&fname), "wb");
	if (!fp)
	    bu_exit(1, "Unable to create test input file %s\n", bu_vls_cstr(&fname));
	fprintf(fp, "%ld\n", i);
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
	bu_vls_sprintf(&fname, "%s%ld", FILE_PREFIX, i);
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
