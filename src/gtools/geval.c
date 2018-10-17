/*                         G E V A L . C
 * BRL-CAD
 *
 * Copyright (c) 2018 United States Government as represented by
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
/** @file geval.c
 *
 * Run any GED command on the fly.
 *
 */

#include "common.h"

/* system headers */
#include "bio.h"
#include <string.h>

/* implementation headers */
#include <bu.h>
#include <ged.h>


int
main(int ac, char *av[]) {
    int i, ret;
    size_t len = 0;
    struct ged *dbp;
    void *libged = bu_dlopen(NULL, BU_RTLD_LAZY);
    char gedfunc[MAXPATHLEN] = {0};
    int (*func)(struct ged *, int, char *[]);

    if (ac < 2) {
	printf("Usage: %s file.g [command]\n", av[0]);
	return -1;
    }
    if (!bu_file_exists(av[1], NULL)) {
	printf("ERROR: [%s] does not exist, expecting .g file\n", av[1]);
	return -2;
    }
    printf("Opening %s\n", av[1]);
    dbp = ged_open("db", av[1], 1);
    ac-=2;
    av+=2;

    printf("Running");
    len += strlen("Running");
    for (i = 0; i < ac; i++) {
	printf(" %s", av[i]);
	len += strlen(av[i]) + 1;
    }
    printf("\n");
    for (i = 0; i < (int) len; i++)
	printf("=");
    printf("\n");

    if (!libged) {
	printf("ERROR: %s\n", bu_dlerror());
	return -3;
    }

    bu_strlcat(gedfunc, "ged_", MAXPATHLEN);
    bu_strlcat(gedfunc, av[0], MAXPATHLEN - strlen(gedfunc));
    if (strlen(gedfunc) < 1 || gedfunc[0] != 'g') {
	printf("ERROR: couldn't get GED command name from [%s]\n", av[0]);
	return -4;
    }

    *(void **)(&func) = bu_dlsym(libged, gedfunc);
    if (!func) {
	printf("ERROR: unrecognized command [%s]\n", av[0]);
	return -5;
    }
    ret = func(dbp, ac, av);

    bu_dlclose(libged);

    printf("%s", bu_vls_addr(dbp->ged_result_str));
    ged_close(dbp);
    BU_PUT(dbp, struct ged);

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
