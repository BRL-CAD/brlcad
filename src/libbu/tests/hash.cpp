/*                    B U _ H A S H . C P P
 * BRL-CAD
 *
 * Copyright (c) 2015-2022 United States Government as represented by
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

#include <map>
#include <string>

#include <limits.h>
#include <string.h>
#include "bu.h"

/* Slightly tweaked and reformatted text originally generated from libsum.com -
 * 275 words, one of which is an empty string (position 195 after "feugiat.")
 * to test behavior in the NULL conditions */
const char *lorem_ipsum[] = {
    "Lorem", "ipsum", "dolor", "sit", "amet,",
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
    "Nullam", "tempus", "auctor."
};

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
    bu_hash_tbl *t = bu_hash_create(0);
    bu_hash_destroy(t);
    return 0;
}

int hash_add_del_one() {
    int *val = NULL;
    bu_hash_tbl *t = bu_hash_create(0);
    if (bu_hash_set(t, (const uint8_t *)array2[0], strlen(array2[0]), (void *)&indices[0]) == -1) return 1;
    val = (int *)bu_hash_get(t, (const uint8_t *)"r1", strlen("r1"));
    if (*val != 7) return 1;
    bu_hash_rm(t, (const uint8_t *)"r1", strlen("r1"));
    if (bu_hash_get(t, (const uint8_t *)"r1", strlen("r1"))) return 1;
    bu_hash_destroy(t);
    return 0;
}

/* Compare the behavior of libbu's hash to that of the C++ map using lorem
 * ipsum strings as keys */
int hash_loremipsum() {
    int i = 0;
    int ret = 0;
    std::map<std::string, int> cppmap;
    std::map<std::string, int>::iterator c_it;
    int lorem_nums[275];

    // Initialize table (will create default 64 bins
    bu_hash_tbl *t = bu_hash_create(0);

    // Set up non-sequential numbers to use as values - easy to compare but
    // shouldn't have any accidental alignments that could obscure bugs.
    for (i = 0; i < 275; i++) lorem_nums[i] = strlen(lorem_ipsum[i]) + i;

    // Initial population of hash set and C++ map.  The hash table will refuse
    // a null key, so make sure we skip putting that in the C++ map.  The key
    // set deliberately contains duplicate keys with different associated
    // values, and both assignments (C++ and hash table) should result in a
    // "last assignment wins" value being stored for subsequent lookups.
    for (i = 0; i < 275; i++) {
	int r = bu_hash_set(t, (const uint8_t *)lorem_ipsum[i], strlen(lorem_ipsum[i]), (void *)&lorem_nums[i]);
	if (strlen(lorem_ipsum[i]) == 0 && r != -1) {
	    bu_log("Error: %s -> %d should have failed to set and didn't!\n", lorem_ipsum[i], lorem_nums[i]);
	    ret = 1;
	}
	if (r >= 0 && strlen(lorem_ipsum[i]) > 0) cppmap[std::string(lorem_ipsum[i])] = lorem_nums[i];
    }

    // Using the C++ map as an oracle, check the values that got stored in the hash
    // table.  All values should match, and all should be present.
    for (c_it = cppmap.begin(); c_it != cppmap.end(); c_it++) {
	int *val = NULL;
	const char *key = ((*c_it).first).c_str();
	val = (int *)bu_hash_get(t, (const uint8_t *)key, strlen(key));
	//bu_log("%s reports %d in C++ map and %d in hash\n", key, (*c_it).second, *val);
	if (*val != (*c_it).second) {
	    bu_log("Error: %s reports %d in C++ map but %d in hash!\n", key, (*c_it).second, *val);
	    ret = 1;
	}
    }

    // Iterate over hash and use find on the C++ map to validate each entry. Also
    // confirms bu hash key copies are correct since any key in the hash table
    // not found in the C++ map is reported as an error.
    struct bu_hash_entry *e = bu_hash_next(t, NULL);
    while (e) {
	struct bu_vls key_str = BU_VLS_INIT_ZERO;
	size_t keylen;
	uint8_t *key;
	void *val;
	(void)bu_hash_key(e, &key, &keylen);
	val = bu_hash_value(e, NULL);
	bu_vls_strncat(&key_str, (const char *)key, keylen);
	c_it = cppmap.find(std::string(bu_vls_addr(&key_str)));
	//bu_log("%s reports %d in C++ map and %d in hash\n", bu_vls_addr(&key_str), (*c_it).second, *(int *)val);
	if (c_it == cppmap.end()) {
	    bu_log("Error: key %s found in hash table iteration was not found in C++ map.\n", bu_vls_addr(&key_str));
	    ret = 1;
	} else {
	    if (*(int *)val != (*c_it).second) {
		bu_log("Error: %s reports %d in C++ map but %d in hash!\n", bu_vls_addr(&key_str), (*c_it).second, *(int *)val);
		ret = 1;
	    }
	}
	bu_vls_free(&key_str);
	e = bu_hash_next(t, e);
    }

    return ret;
}

int
main(int argc, const char **argv)
{
    int ret = 0;
    long test_num;
    char *endptr = NULL;

    bu_setprogname(argv[0]);

    /* Sanity checks */
    if (argc < 2) {
	bu_log("Usage: %s {test_number}\n", argv[0]);
	bu_exit(1, "ERROR: missing test number\n");
    }

    test_num = strtol(argv[1], &endptr, 0);
    if (endptr && strlen(endptr) != 0) {
	bu_exit(2, "Invalid test number: %s\n", argv[1]);
    }

    switch (test_num) {
	case 0:
	    ret = hash_noop_test();
	    break;
	case 1:
	    ret = hash_add_del_one();
	    break;
	case 2:
	    ret = hash_loremipsum();
	    break;
    }

    return ret;
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8

