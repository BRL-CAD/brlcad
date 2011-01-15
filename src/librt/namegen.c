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
/** @file namegen.c
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


#if 0
static void
count_if_region(struct db_i *dbip, struct directory *dp, genptr_t rcount)
{
    int *counter = (int*)rcount;
    RT_CK_DBI(dbip);
    if (dp->d_flags & DIR_REGION) {
	(*counter)++;
    }
}
static int
determine_object_type(struct db_i *dbip, struct directory *dp)
{
    struct resource *resp = &rt_uniresource;
    int rcount = 0;
    int object_type;
    if (!dp) {
	bu_log("Null directory pointer passed to determine_object_type - returning object type 0\n");
	return 0;
    }
    db_functree(dbip, dp, count_if_region, NULL, resp, &rcount);

    if (dp->d_flags & DIR_SOLID) {
	object_type = 1;
    }
    if (dp->d_flags & DIR_REGION) {
	if (rcount > 1) {
	    bu_log("WARNING - detected region flag set in subtree of region."
		   "Returning type as region - if assembly type is intended "
		   "re-structure model to eliminate nested regions.\n");
	    object_type = 3;
	} else {
	    object_type = 3;
	}
    }
    if ((dp->d_flags & DIR_COMB) && !(dp->d_flags & DIR_REGION)) {
	if (rcount > 0) {
	    object_type = 4;
	} else {
	    object_type = 2;
	}
    }
    return object_type;
}
#endif

struct formatting_style {
    struct bu_vls regex_spec;
    int pos_of_type_id_char;
};

static int
count_format_blocks(char *formatstring)
{
    size_t i;
    int components = 0;
    for (i=0; i < strlen(formatstring); i++){
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

    BU_GETSTRUCT(standard1, formatting_style);
    bu_vls_init(&(standard1->regex_spec));
    bu_vls_strcat(&(standard1->regex_spec), "([rcs][.])?([^0-9^.]*)?([0-9]*)?([.][oicb])?([0-9]*)?([+u-])?([0-9]*)?");
    standard1->pos_of_type_id_char = 1;

    BU_GETSTRUCT(standard2, formatting_style);
    bu_vls_init(&(standard2->regex_spec));
    bu_vls_strcat(&(standard2->regex_spec), "([^0-9^.]*)?([0-9]*)?([^.]*)?([.][rcs])?([0-9]*)?([+u-])?([0-9]*)?");
    standard2->pos_of_type_id_char = 5;

    if (style == 1) {
	ret=regcomp(&compiled_regex, bu_vls_addr(&(standard1->regex_spec)), REG_EXTENDED);
	components = count_format_blocks(bu_vls_addr(&(standard1->regex_spec)));
    }

    if (style == 2) {
	ret=regcomp(&compiled_regex, bu_vls_addr(&(standard2->regex_spec)), REG_EXTENDED);
	components = count_format_blocks(bu_vls_addr(&(standard2->regex_spec)));
    }

    result_locations = (regmatch_t *)bu_calloc(components + 1, sizeof(regmatch_t), "array to hold answers from regex");

    bu_log("components: %d\n",components);

    iterators = (int *)bu_calloc(components, sizeof(int), "array for iterator status of results");

    ret=regexec(&compiled_regex, name, components+1, result_locations, 0);

    bu_vls_init(&testresult);
    for (i=1; i<=components; i++) {
	bu_vls_trunc(&testresult,0);
	bu_vls_strncpy(&testresult, name+result_locations[i].rm_so, result_locations[i].rm_eo - result_locations[i].rm_so);
	if (is_number(bu_vls_addr(&testresult)) == 1) {
	    iterators[i-1] = 1;
	} else {
	    if  (contains_number(bu_vls_addr(&testresult)) == 1) {
		iterators[i-1] = 2;
	    }
	}
	bu_log("%s\n",bu_vls_addr(&testresult));
    }

    for (i=0; i<components; i++) {
	bu_log("%d ", iterators[i]);
    }
    bu_log("\n");

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

#if 0
/*NOTE - this needs to be redone to use regex instead of the custom parse!!!*/
static struct object_name_data *
parse_obj_name(struct db_i *dbip, char *fmt, char *name)
{
    struct object_name_data *objcomponents;

    struct object_name_item *objname;
    struct increment_data *incdata;

    struct directory *dp = (struct directory *)NULL;
    struct resource *resp = &rt_uniresource;
    int rcount = 0;

    char *stringholder;
    int ignore_separator_flag = 0;

    size_t len = 0;
    size_t i;

    BU_GETSTRUCT(objcomponents, object_name_data);
    BU_LIST_INIT(&(objcomponents->name_components));
    BU_LIST_INIT(&(objcomponents->separators));
    BU_LIST_INIT(&(objcomponents->incrementals));
    bu_vls_init(&(objcomponents->extension));

    if (!fmt || fmt[0] == '\0' || !name || name[0] == '\0') {
	bu_log("ERROR: empty name or format string passed to parse_name\n");
	return 0;
    }

    dp = db_lookup(dbip, name, LOOKUP_QUIET);
    if (!dp) {
	bu_log("ERROR:  No object named %s found in database.\n", name);
	return 0;
    }

    objcomponents->object_type = determine_object_type(dbip, dp);

    for (i = 0; i < strlen(fmt); i++) {
	if (len <= strlen(name)) {
	    switch ( fmt[i] ) {
		case 'n':
		{
		    BU_GETSTRUCT(objname, object_name_item);
		    BU_LIST_INIT(&(objname->l));
		    bu_vls_init(&(objname->namestring));
		    bu_vls_trunc(&(objname->namestring),0);
		    if (fmt[i+1] == 'i' ) {
			while (name[len] != '.' && name[len] != '_' && name[len] != '-' && name[len] != '\0' && !isdigit(name[len])){
			    bu_vls_putc(&(objname->namestring), name[len]);
			    len++;
			}
		    }else {
			while (name[len] != '.' && name[len] != '_' && name[len] != '-' && name[len] != '\0'){
			    bu_vls_putc(&(objname->namestring), name[len]);
			    len++;
			}
		    }
		    BU_LIST_INSERT(&(objcomponents->name_components), &(objname->l));
		}
		break;
		case 'i':
		{
		    BU_GETSTRUCT(incdata, increment_data);
		    BU_LIST_INIT(&(incdata->l));
		    bu_vls_init(&(incdata->namestring));
		    bu_vls_trunc(&(incdata->namestring),0);
		    if (isdigit(name[len])) {
			while (isdigit(name[len])) {
			    bu_vls_putc(&(incdata->namestring), name[len]);
			    len++;
			}
		    } else {
			bu_log("Note: naming convention requres incrementor here, but no digit found - setting a default value of 1\n");
			bu_vls_putc(&(incdata->namestring), '1');
		    }
		    incdata->numerval = atoi(bu_vls_addr(&(incdata->namestring)));
		    BU_LIST_INSERT(&(objcomponents->incrementals), &(incdata->l));

		}
		break;
		case 's':
		{
		    if (ignore_separator_flag == 0) {
			BU_GETSTRUCT(objname, object_name_item);
			BU_LIST_INIT(&(objname->l));
			bu_vls_init(&(objname->namestring));
			bu_vls_trunc(&(objname->namestring),0);
			if (name[len] == '.' || name[len] == '_' || name[len] == '-'){
			    bu_vls_putc(&(objname->namestring), name[len]);
			    len++;
			} else {
			    bu_vls_putc(&(objname->namestring), '.');
			    bu_log("Note: naming convention requires separator but none found at designated point in supplied object name - using '.'\n");
			}
			BU_LIST_INSERT(&(objcomponents->separators), &(objname->l));
		    } else {
			ignore_separator_flag = 0;
			len++;
		    }
		}
		break;
		case 'e':
		{
		    if (objcomponents->object_type == 4  && ASSEM_EXT==' ' && fmt[i+1] == 's') {
			ignore_separator_flag = 1;
			len++;
		    } else {
			switch (name[len]) {
			    case PRIM_EXT:
			    {
				if (objcomponents->object_type == 1) {
				    bu_vls_putc(&(objcomponents->extension), name[len]);
				} else {
				    bu_vls_putc(&(objcomponents->extension), name[len]);
				    bu_log("Warning: primitive found with extension other than %c\n", PRIM_EXT);
				}
			    }
			    break;
			    case COMB_EXT:
			    {
				if (objcomponents->object_type == 2) {
				    bu_vls_putc(&(objcomponents->extension), name[len]);
				} else {
				    bu_vls_putc(&(objcomponents->extension), name[len]);
				    bu_log("Warning: combination found with extension other than %c\n", COMB_EXT);
				}
			    }
			    break;

			    case REGION_EXT:
			    {
				if (objcomponents->object_type == 3) {
				    bu_vls_putc(&(objcomponents->extension), name[len]);
				} else {
				    bu_vls_putc(&(objcomponents->extension), name[len]);
				    bu_log("Warning: region found with extension other than %c\n", REGION_EXT);
				}
			    }
			    break;

			    case ASSEM_EXT:
			    {
				if (objcomponents->object_type == 1) {
				    bu_vls_putc(&(objcomponents->extension), name[len]);
				} else {
				    bu_vls_putc(&(objcomponents->extension), name[len]);
				    bu_log("Warning: assembly found with extension other than %c\n", ASSEM_EXT);
				}
			    }
			    break;
			    default:
			    {
				switch (objcomponents->object_type) {
				    case 1:
				    {
					bu_vls_putc(&(objcomponents->extension), PRIM_EXT);
				    }
				    break;
				    case 2:
				    {
					bu_vls_putc(&(objcomponents->extension), COMB_EXT);
				    }
				    break;
				    case 3:
				    {
					bu_vls_putc(&(objcomponents->extension), REGION_EXT);
				    }
				    break;
				    case 4:
				    {
					bu_vls_putc(&(objcomponents->extension), ASSEM_EXT);
				    }
				    break;
				    default:
				    {
					bu_log("Error - invalid object type returned by determine_object_type\n");
				    }
				    break;
				}
				bu_log("Note: naming convention requres extension here, but char at designated point is not a valid extension.  Selecting extension char %s based on object type.\n", bu_vls_addr(&(objcomponents->extension)));
			    }
			}
			len++;
		    }
		}
		break;
		default:
		{
		    bu_log("Error - invalid character in object formatting string\n");
		}
		break;
	    }
	}
    }

    return objcomponents;
}

static void
test_obj_struct(struct db_i *dbip, char *fmt, char *testname)
{
    struct object_name_item *testitem;
    struct increment_data *inctest;
    struct object_name_data *test;

    bu_log("\n***Example: %s ***\n\n",testname);
    test = parse_obj_name(dbip, fmt, testname);
    bu_log("\nParsing Results:\nName strings: ");
    for ( BU_LIST_FOR( testitem, object_name_item, &(test->name_components) ) )  {
	bu_log("%s ",bu_vls_addr(&(testitem->namestring)));
    }
    bu_log("\nSeparators:   ");
    for ( BU_LIST_FOR( testitem, object_name_item, &(test->separators) ) )  {
	bu_log("%s ",bu_vls_addr(&(testitem->namestring)));
    }
    bu_log("\nIncrementals: ");
    for ( BU_LIST_FOR( inctest, increment_data, &(test->incrementals) ) )  {
	bu_log("%s, %d ",bu_vls_addr(&(inctest->namestring)),inctest->numerval);
    }

    bu_log("\nExtension:    ");
    bu_log("%s ",bu_vls_addr(&(test->extension)));
    bu_log("\n\n");

    for ( BU_LIST_FOR( testitem, object_name_item,  &(test->name_components) ) ) {
	bu_free(testitem, "free names");
    }

    for ( BU_LIST_FOR( testitem, object_name_item, &(test->separators) ) )  {
	bu_free(testitem, "free separators");
    }
    for ( BU_LIST_FOR( inctest, increment_data, &(test->incrementals) ) )  {
	bu_free(inctest, "free incrementals");
    }

    bu_vls_free(&(test->extension));
    bu_free(test, "free name structure");
}

/* Per discussion with Sean - use argc, argv pair for actual requesting, generation and return of names.
 */

static void
get_next_name(struct db_i *dbip, char *fmt, char *basename, int pos_iterator, int inc_iterator, int argc, char *argv[])
{
    struct bu_vls stringassembly;

    struct object_name_data *parsed_name;
    struct object_name_item *objname;
    struct increment_data *incdata;
    struct object_name_item *objsep;
    size_t i;
    int j;
    int iterator_pos;
    int iterator_count = 0;
    int ignore_separator_flag = 0;

    char *finishedname;

    bu_vls_init(&stringassembly);

    parsed_name = parse_obj_name(dbip, fmt, basename);

    for (j = 0; j < argc; j++) {

	bu_vls_trunc(&stringassembly, 0);
	iterator_count++;
	iterator_pos = 0;
	objname = BU_LIST_NEXT(object_name_item,  &(parsed_name->name_components));
	incdata = BU_LIST_NEXT(increment_data, &(parsed_name->incrementals));
	objsep = BU_LIST_NEXT(object_name_item, &(parsed_name->separators));
	for (i = 0; i < strlen(fmt); i++) {
	    switch ( fmt[i] ) {
		case 'n':
		{
		    bu_vls_printf(&stringassembly, "%s", bu_vls_addr(&(objname->namestring)));
		    if (!(BU_LIST_NEXT_IS_HEAD(objname,  &(parsed_name->name_components)))){
			objname = BU_LIST_PNEXT(object_name_item, objname);
		    }
		}
		break;
		case 'i':
		{
		    iterator_pos++;
		    if (iterator_pos == pos_iterator) {
			bu_vls_printf(&stringassembly, "%d", incdata->numerval + iterator_count*inc_iterator);
		    } else {
			bu_vls_printf(&stringassembly, "%d", incdata->numerval);
		    }
		    if (!(BU_LIST_NEXT_IS_HEAD(incdata, &(parsed_name->incrementals)))){
			incdata = BU_LIST_PNEXT(increment_data, incdata);
		    }
		}
		break;
		case 's':
		{
		    if (ignore_separator_flag == 0) {
			bu_vls_printf(&stringassembly, "%s", bu_vls_addr(&(objsep->namestring)));
		    } else {
			ignore_separator_flag = 0;
		    }
		    if (!(BU_LIST_NEXT_IS_HEAD(objsep, &(parsed_name->separators)))){
			objsep = BU_LIST_PNEXT(object_name_item, objsep);
		    }
		}
		break;
		case 'e':
		{
		    if (parsed_name->object_type == 4 && ASSEM_EXT==' ' && fmt[i+1] == 's') {
			ignore_separator_flag = 1;
		    } else {
			bu_vls_printf(&stringassembly, "%s", bu_vls_addr(&(parsed_name->extension)));
		    }
		}
		break;
		default:
		{
		    bu_log("Error - invalid character in object formatting string\n");
		}
		break;
	    }
	}
	finishedname = (char *)bu_malloc(sizeof(char) * strlen(bu_vls_addr(&stringassembly)), "memory for returned name");
	argv[j] = strcpy(finishedname, bu_vls_addr(&stringassembly));
    }
}
#endif

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
#if 0
    dbip = db_open( "./test.g", "r" );
    db_dirbuild(dbip);


    /*	av = (char **)bu_malloc(sizeof(char *) * num_of_copies, "array to hold answers from get_next_name");
     */
    bu_vls_trunc(&temp,0);
    bu_vls_printf(&temp,"%s","core-001a.s");

    /*	test_obj_struct(dbip, "nsinse", bu_vls_addr(&temp));
	get_next_name(dbip, "nsinse", bu_vls_addr(&temp), 1, 3, num_of_copies, (char **)av);
	for (j=0; j < num_of_copies; j++){
	bu_log("%s\n",av[j]);
	}
	bu_free(av, "done with name strings, free memory");

	av = (char **)bu_malloc(sizeof(char *) * num_of_copies, "array to hold answers from get_next_name");
    */
#endif
    bu_vls_trunc(&temp,0);
    bu_vls_printf(&temp,"%s","s.bcore12.b3");

    test_regex("core-001a1b.s1+1",1);
    test_regex("s.bcore12.b3",1);
    test_regex("comb1.c",1);
    test_regex("comb2.r",1);
    test_regex("comb3.r",1);
    test_regex("assem1",1);
    test_regex("test.q",1);
    test_regex("core-001a1b.s1+1",2);
    test_regex("s.bcore12.b3",2);
    test_regex("comb1.c",2);
    test_regex("comb2.r",2);
    test_regex("assem1",2);
    test_regex("test.q",2);

    /*	test_obj_struct(dbip, "esnisni", bu_vls_addr(&temp));
	get_next_name(dbip, "esnisni", bu_vls_addr(&temp), 1, 3, num_of_copies, (char **)av);
	for (j=0; j < num_of_copies; j++){
	bu_log("%s\n",av[j]);
	}
	bu_free(av, "done with name strings, free memory");

	av = (char **)bu_malloc(sizeof(char *) * num_of_copies, "array to hold answers from get_next_name");
	bu_vls_trunc(&temp,0);
	bu_vls_printf(&temp,"%s","comb1.c");
	test_obj_struct(dbip, "nise", bu_vls_addr(&temp));
	get_next_name(dbip, "nise", bu_vls_addr(&temp), 1, 3, num_of_copies, (char **)av);
	for (j=0; j < num_of_copies; j++){
	bu_log("%s\n",av[j]);
	}
	bu_free(av, "done with name strings, free memory");

	av = (char **)bu_malloc(sizeof(char *) * num_of_copies, "array to hold answers from get_next_name");
	bu_vls_trunc(&temp,0);
	bu_vls_printf(&temp,"%s","comb2.r");
	test_obj_struct(dbip, "nise", bu_vls_addr(&temp));
	get_next_name(dbip, "nise", bu_vls_addr(&temp), 1, 3, num_of_copies, (char **)av);
	for (j=0; j < num_of_copies; j++){
	bu_log("%s\n",av[j]);
	}
	bu_free(av, "done with name strings, free memory");

	av = (char **)bu_malloc(sizeof(char *) * num_of_copies, "array to hold answers from get_next_name");
	bu_vls_trunc(&temp,0);
	bu_vls_printf(&temp,"%s","comb3.r");
	test_obj_struct(dbip, "nise", bu_vls_addr(&temp));
	get_next_name(dbip, "nise", bu_vls_addr(&temp), 1, 3, num_of_copies, (char **)av);
	for (j=0; j < num_of_copies; j++){
	bu_log("%s\n",av[j]);
	}
	bu_free(av, "done with name strings, free memory");

	av = (char **)bu_malloc(sizeof(char *) * num_of_copies, "array to hold answers from get_next_name");
	bu_vls_trunc(&temp,0);
	bu_vls_printf(&temp,"%s","assem1");
	test_obj_struct(dbip, "ni", bu_vls_addr(&temp));
	get_next_name(dbip, "ni", bu_vls_addr(&temp), 1, 3, num_of_copies, (char **)av);
	for (j=0; j < num_of_copies; j++){
	bu_log("%s\n",av[j]);
	}
	bu_free(av, "done with name strings, free memory");

	av = (char **)bu_malloc(sizeof(char *) * num_of_copies, "array to hold answers from get_next_name");
	bu_vls_trunc(&temp,0);
	bu_vls_printf(&temp,"%s","test.q");
	test_obj_struct(dbip, "nise", bu_vls_addr(&temp));
	get_next_name(dbip, "nise", bu_vls_addr(&temp), 1, 3, num_of_copies, (char **)av);
	for (j=0; j < num_of_copies; j++){
	bu_log("%s\n",av[j]);
	}
	bu_free(av, "done with name strings, free memory");
    */
/*    db_close(dbip);*/
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
