/*                           C O L U M N P A R S E . C
 * BRL-CAD
 *
 * Copyright (c) 2008-2010 United States Government as represented by
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
/** @file columnparse.c
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

static void
test_regex(char *name)
{
    regex_t compiled_regex;
    regmatch_t *result_locations;
    int ret, components, count;
    struct bu_vls modelregex;
    struct bu_vls attrregex;
    struct bu_vls workingstring1;
    struct bu_vls workingstring2;
    struct bu_vls testresult;

    bu_vls_init(&modelregex);
    bu_vls_init(&attrregex);
    bu_vls_init(&workingstring1);
    bu_vls_init(&workingstring2);
    bu_vls_sprintf(&modelregex, "([ ]*Model Name[ ]*)(.*)");
    bu_vls_sprintf(&attrregex, "([a-zA-Z0-9]*[ ]*)([a-zA-Z0-9].*$)?");
    bu_vls_sprintf(&workingstring1, "%s", name);

    ret=regcomp(&compiled_regex, bu_vls_addr(&modelregex), REG_EXTENDED);
    components = 2;
    result_locations = (regmatch_t *)bu_calloc(components + 1, sizeof(regmatch_t), "array to hold answers from regex");

    ret=regexec(&compiled_regex, bu_vls_addr(&workingstring1), components+1, result_locations, 0);

    bu_vls_init(&testresult);
    bu_vls_trunc(&testresult,0);
    bu_vls_strncpy(&testresult, bu_vls_addr(&workingstring1)+result_locations[1].rm_so, result_locations[1].rm_eo - result_locations[1].rm_so);
    bu_log("\n%s\n",bu_vls_addr(&testresult));
    bu_log("stringlength:%d\n",bu_vls_strlen(&testresult));
    
    bu_vls_trunc(&workingstring2,0);
    bu_vls_strncpy(&workingstring2, bu_vls_addr(&workingstring1)+result_locations[2].rm_so, result_locations[2].rm_eo - result_locations[2].rm_so); 
 
    while ((0 < bu_vls_strlen(&workingstring2)) && (ret != REG_NOMATCH)) {
        bu_vls_sprintf(&workingstring1, "%s", bu_vls_addr(&workingstring2));
        ret=regcomp(&compiled_regex, bu_vls_addr(&attrregex), REG_EXTENDED);
        ret=regexec(&compiled_regex, bu_vls_addr(&workingstring1), components+1, result_locations, 0);
	bu_vls_trunc(&testresult, 0);
        bu_vls_strncpy(&testresult, bu_vls_addr(&workingstring1)+result_locations[1].rm_so, result_locations[1].rm_eo - result_locations[1].rm_so);
        bu_log("\n%s\n",bu_vls_addr(&testresult));
        bu_log("stringlength:%d\n",bu_vls_strlen(&testresult));
    
        bu_vls_trunc(&workingstring2,0);
        bu_vls_strncpy(&workingstring2, bu_vls_addr(&workingstring1)+result_locations[2].rm_so, result_locations[2].rm_eo - result_locations[2].rm_so); 
        count++;
    }


    bu_log("\n");

    bu_free(result_locations, "free regex results");
}

int
main()
{

    test_regex("       Model Name         ATTR1                                     ATTR2        ATTR3  ATTR4");
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
