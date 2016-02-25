/*                       H A S H . C
 * BRL-CAD
 *
 * Copyright (c) 2015-2016 United States Government as represented by
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
#include <limits.h>
#include <string.h>
#include "bu.h"

/* generated from libsum.com - 275 words */
const char *lorem_ipsum[] = {"Lorem", "ipsum", "dolor", "sit", "amet,",
    "consectetur", "adipiscing", "elit.", "Nulla", "quis", "neque", "egetdui",
    "consequat", "sodales.", "Cras", "vitae", "quam", "ut", "neque", "rutrum",
    "posuere", "gravida", "a", "nisi.Pellentesque", "quis", "eros",
    "lobortis,", "consectetur", "ex", "sed,", "molestie", "enim.",
    "Sedpellentesque", "metus", "tempor", "eleifend", "gravida.", "Donec",
    "id", "nibh", "id", "nunc", "interdumlobortis.", "Cras", "facilisis",
    "dictum", "malesuada.", "Aliquam", "erat", "volutpat.", "Donecultrices",
    "nunc", "arcu,", "at", "scelerisque", "purus", "laoreet", "non.",
    "Quisque", "aliquet", "tortoreget", "tellus", "iaculis", "condimentum.",
    "Duis", "egestas", "ligula", "nec", "ultricies", "tincidunt.Vestibulum",
    "sagittis", "maximus", "pharetra.", "In", "finibus", "egestas", "turpis",
    "vitaepellentesque.", "Donec", "auctor", "mollis", "ultricies.", "Aliquam",
    "lobortis", "eros", "at", "massatristique", "aliquet.", "Pellentesque",
    "nunc", "arcu,", "interdum", "ac", "varius", "et,", "dapibus",
    "vitaetortor.", "Morbi", "tempor", "felis", "in", "justo", "maximus",
    "vulputate.", "Aliquam", "libero", "nunc,mollis", "vitae", "est", "vel,",
    "pretium", "auctor", "libero.", "Suspendisse", "blandit", "dui", "at",
    "finibuspulvinar.", "Duis", "porta,", "urna", "dignissim", "commodo",
    "commodo,", "mi", "purus", "maximus", "enim,ornare", "imperdiet", "mauris",
    "eros", "nec", "justo.", "Etiam", "sit", "amet", "blandit", "leo.",
    "Maecenas", "aturna", "a", "ipsum", "rhoncus", "hendrerit.", "Integer",
    "ante", "ante,", "fermentum", "et", "semper", "et,aliquet", "gravida",
    "velit.", "Sed", "eget", "placerat", "velit.", "Vivamus", "et", "enim",
    "nec", "urnarhoncus", "varius.", "Maecenas", "pretium", "elit", "ac",
    "molestie", "aliquam.", "Fusce", "vel", "euismodex,", "eu", "fringilla",
    "nunc.", "Nam", "hendrerit,", "elit", "at", "scelerisque", "laoreet,",
    "nibh", "liberopellentesque", "turpis,", "vitae", "volutpat", "arcu",
    "eros", "in", "nulla.", "Integer", "vehiculatincidunt", "tortor", "nec",
    "feugiat.", "", "Etiam", "eu", "sagittis", "mi.", "Duis", "quis",
    "placerat", "nunc,eget", "euismod", "nulla.", "Proin", "malesuada,",
    "velit", "ullamcorper", "maximus", "lacinia,", "liberomi", "varius",
    "tellus,", "ut", "dignissim", "turpis", "risus", "eu", "enim.", "Cras",
    "nec", "ex", "id", "orciporttitor", "elementum.", "Etiam", "interdum",
    "quis", "arcu", "dapibus", "molestie.", "Vestibulum", "estdiam,", "tempus",
    "nec", "urna", "in,", "dictum", "porta", "sem.", "Sed", "venenatis",
    "consectetur", "dui,", "atsuscipit", "sem", "mollis", "id.", "Sed",
    "suscipit", "ex", "leo,", "sit", "amet", "sollicitudin", "odio",
    "laoreetvel.", "Phasellus", "tellus", "felis,", "blandit", "vitae", "est",
    "vel,", "consectetur", "molestie", "dolor.Donec", "vitae", "eros", "odio.",
    "Nullam", "tempus", "auctor."};

const char *array1[] = {
    "1",
    "2",
    "3",
    "4",
    "5",
    "6",
    "7"
};

const char *array2[] = {
    "r1",
    "r2",
    "r3",
    "r4",
    "r5",
    "r6",
    "r7"
};

int indices[] = {7,6,4,3,5,1,2};

int hash_noop_test() {
    bu_nhash_tbl *t = bu_nhash_tbl_create(0);
    bu_nhash_tbl_destroy(t);
    return 0;
}

int hash_add_del_one() {
    int *val = NULL;
    bu_nhash_tbl *t = bu_nhash_tbl_create(0);
    if (bu_nhash_set(t, (const uint8_t *)array2[0], strlen(array2[0]), (void *)&indices[0]) == -1) return 1;
    val = (int *)bu_nhash_get(t, (const uint8_t *)"r1", strlen("r1"));
    if (*val != 7) return 1;
    bu_nhash_del(t, (const uint8_t *)"r1", strlen("r1"));
    if (bu_nhash_get(t, (const uint8_t *)"r1", strlen("r1"))) return 1;
    bu_nhash_tbl_destroy(t);
    return 0;
}

int hash_loremipsum() {
    int i = 0;
    int lorem_nums[275];
    bu_nhash_tbl *t = bu_nhash_tbl_create(0);
    for (i = 0; i < 275; i++) lorem_nums[i] = strlen(lorem_ipsum[i]) + i;
    for (i = 0; i < 275; i++) {
	int r = bu_nhash_set(t, (const uint8_t *)lorem_ipsum[i], strlen(lorem_ipsum[i]), (void *)&lorem_nums[i]);
	if (r == 1) bu_log("new entry: %s -> %d\n", lorem_ipsum[i], lorem_nums[i]);
	if (r == 0) bu_log("updated entry: %s -> %d\n", lorem_ipsum[i], lorem_nums[i]);
    }
    return 0;
}

int
main(int argc, const char **argv)
{
    int ret = 0;
    long test_num;
    char *endptr = NULL;

    /* Sanity checks */
    if (argc < 2) {
        bu_exit(1, "ERROR: wrong number of parameters - need test num");
    }
    test_num = strtol(argv[1], &endptr, 0);
    if (endptr && strlen(endptr) != 0) {
        bu_exit(1, "Invalid test number: %s\n", argv[1]);
    }

    switch (test_num) {
    case 0:
        ret = hash_noop_test();
    case 1:
        ret = hash_add_del_one();
        break;
    case 2:
	return hash_loremipsum();
        break;
    }

    return ret;
}

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
