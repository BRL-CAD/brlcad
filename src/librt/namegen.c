/*                           N A M E G E N . C
 * BRL-CAD
 *
 * Copyright (c) 2008-2011 United States Government as represented by
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
/** @addtogroup librt */
/** @{ */
/** @file librt/namegen.c
 *
 * Implements parser and next-name routines for BRL-CAD object names
 *
 * There are several tools in BRL-CAD which require automatic naming
 * of auto-generated objects, such as clone and mirror.  The routines
 * defined here provide a general and universal method for taking a
 * supplied object name and using it as a basis for generating new
 * names, based on a printf-like syntax specific to BRL-CAD names:
 *
 * n - naming component of the primitive, defined as either the part
 * of the primitive name from the first character up to the first
 * separator, a non-incremental, non-extension string between
 * separators in the name, or a non-incremental non-extension string
 * between a separator and the end of the primitive name.
 *
 * s - a separator between naming elements, one of "-", "_" or ".".
 * Note that if one or more of these characters is present but no
 * separator has been specified, the presence of these characters is
 * not regarded as indicating a separator.
 *
 * i - an incremental section, changed when a new name is to be
 * generated.
 *
 * e - an extension - like %n, but can optionally be assigned based on
 * the type of object being created.
 *
 * A formatting string for a primitive name MUST have at least one
 * incremental section. If no separators are specified between a %n
 * and a %i, it is assumed that the first digit character encountered
 * is part of the %i and the previous non-digit character is the final
 * character in that %n or %e region. To have digits present in a %n
 * which is to be followed by a %i it is required to have a separator
 * between the %n region and the %i region.
 */

#include "common.h"

#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <stdarg.h>
#include <regex.h>
#include "bio.h"

#include "bn.h"
#include "db.h"
#include "raytrace.h"
#include "bu.h"


#define ASSEM_EXT ' '
#define REGION_EXT 'r'
#define COMB_EXT 'c'
#define PRIM_EXT 's'


struct formatting_style {
    struct bu_vls regex_spec;
    int pos_of_type_id_char;
};


static int
count_format_blocks(char *formatstring)
{
    size_t i;
    int components = 0;
    for (i=0; i < strlen(formatstring); i++) {
	if (formatstring[i] == '(') {
	    components++;
	}
    }
    return components;
}


static int
is_number(char *substring)
{
    int ret = -1;
    regex_t compiled_regex;
    ret=regcomp(&compiled_regex, "^[0-9]+$", REG_EXTENDED);
    ret=regexec(&compiled_regex, substring, 0, 0, 0);
    if (ret == 0) {
	return 1;
    } else {
	return -1;
    }
}


static int
contains_number(char *substring)
{
    int ret = -1;
    regex_t compiled_regex;
    ret=regcomp(&compiled_regex, "[0-9]+", REG_EXTENDED);
    ret=regexec(&compiled_regex, substring, 0, 0, 0);
    if (ret == 0) {
	return 1;
    } else {
	return -1;
    }
}


static void
test_regex(char *name, int style)
{
    struct formatting_style *standard1;
    struct formatting_style *standard2;

    regex_t compiled_regex;
    regmatch_t *result_locations;
    int i, ret, components = 0;
    struct bu_vls testresult;

    int *iterators;

    BU_GET(standard1, struct formatting_style);
    bu_vls_init(&(standard1->regex_spec));
    bu_vls_strcat(&(standard1->regex_spec), "([rcs][.])?([^0-9^.]*)?([0-9]*)?([.][oicb])?([0-9]*)?([+u-])?([0-9]*)?");
    standard1->pos_of_type_id_char = 1;

    BU_GET(standard2, struct formatting_style);
    bu_vls_init(&(standard2->regex_spec));
    bu_vls_strcat(&(standard2->regex_spec), "([^0-9^.]*)?([0-9]*)?([^.]*)?([.][rcs])?([0-9]*)?([+u-])?([0-9]*)?");
    standard2->pos_of_type_id_char = 5;

    if (style == 1) {
	ret = regcomp(&compiled_regex, bu_vls_addr(&(standard1->regex_spec)), REG_EXTENDED);
	if (ret != 0) {
	    perror("regcomp");
	}
	components = count_format_blocks(bu_vls_addr(&(standard1->regex_spec)));
    }

    if (style == 2) {
	ret = regcomp(&compiled_regex, bu_vls_addr(&(standard2->regex_spec)), REG_EXTENDED);
	if (ret != 0) {
	    perror("regcomp");
	}
	components = count_format_blocks(bu_vls_addr(&(standard2->regex_spec)));
    }

    result_locations = (regmatch_t *)bu_calloc(components + 1, sizeof(regmatch_t), "array to hold answers from regex");

    bu_log("components: %d\n", components);

    iterators = (int *)bu_calloc(components, sizeof(int), "array for iterator status of results");

    ret = regexec(&compiled_regex, name, components+1, result_locations, 0);
    if (ret == 0) {
	bu_vls_init(&testresult);
	for (i=1; i<=components; i++) {
	    bu_vls_trunc(&testresult, 0);
	    bu_vls_strncpy(&testresult, name+result_locations[i].rm_so, result_locations[i].rm_eo - result_locations[i].rm_so);
	    if (is_number(bu_vls_addr(&testresult)) == 1) {
		iterators[i-1] = 1;
	    } else {
		if (contains_number(bu_vls_addr(&testresult)) == 1) {
		    iterators[i-1] = 2;
		}
	    }
	    bu_log("%s\n", bu_vls_addr(&testresult));
	}

	for (i=0; i<components; i++) {
	    bu_log("%d ", iterators[i]);
	}
	bu_log("\n");
    }

    bu_free(result_locations, "free regex results");
}


struct increment_data {
    struct bu_list l;
    struct bu_vls namestring;
    int numerval;
};


struct object_name_data {
    struct bu_list name_components;
    struct bu_list separators;
    struct bu_list incrementals;
    struct bu_vls extension;
    int object_type; /* 0 = unknown, 1 = solid, 2 = comb, 3 = region, 4 = assembly*/
};


struct object_name_item {
    struct bu_list l;
    struct bu_vls namestring;
};


int
main()
{
    /*
      int num_of_copies = 10;
      int j;
      char **av;
      struct db_i *dbip;
    */
    struct bu_vls temp;
    bu_vls_init(&temp);
    bu_vls_trunc(&temp, 0);
    bu_vls_printf(&temp, "%s", "s.bcore12.b3");

    test_regex("core-001a1b.s1+1", 1);
    test_regex("s.bcore12.b3", 1);
    test_regex("comb1.c", 1);
    test_regex("comb2.r", 1);
    test_regex("comb3.r", 1);
    test_regex("assem1", 1);
    test_regex("test.q", 1);
    test_regex("core-001a1b.s1+1", 2);
    test_regex("s.bcore12.b3", 2);
    test_regex("comb1.c", 2);
    test_regex("comb2.r", 2);
    test_regex("assem1", 2);
    test_regex("test.q", 2);

    return 1;
}


/** @} */
/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
