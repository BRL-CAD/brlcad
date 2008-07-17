/*                           N A M E G E N . C
 * BRL-CAD
 *
 * Copyright (c) 2008 United States Government as represented by
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
 * @brief Implements parser and next-name routines for BRL-CAD object names
 *
 * There are several tools in BRL-CAD which require automatic naming
 * of auto-generated objects, such as clone and mirror.  The routines
 * defined here provide a general and universal method for taking a
 * supplied object name and using it as a basis for generating new
 * names, based on a printf-like syntax specific to BRL-CAD names:
 *
 * n - naming component of the primitive, defined as either the part of the primitive name
 *     from the first character up to the first separator, a non-incremental, non-extension
 *     string between separators in the name, or a non-incremental non-extension string
 *     between a separator and the end of the primitive name.
 * s - a separator between naming elements, one of "-", "_" or ".".  Note that if
 *     one or more of these characters is present but no separator has been specified, the
 *     presence of these characters is not regarded as indicating a separator.
 * i - an incremental section, changed when a new name is to be generated.
 * e - an extension - like %n, but can optionally be assigned based on the type of
 *     object being created.
 * 
 *
 * A formatting string for a primitive name MUST have at least one incremental section. If
 * no separators are specified between a %n and a %i, it is assumed that the first digit
 * character encountered is part of the %i and the previous non-digit character is the final
 * character in that %n or %e region. To have digits present in a %n which is to be followed
 * by a %i it is required to have a separator between the %n region and the %i region.
 */

#include "common.h"

#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <stdarg.h>
#include "bio.h"

#include "bu.h"

#define ASSEM_EXT ''
#define REGION_EXT 'r'
#define COMB_EXT 'c'
#define PRIM_EXT 's'

struct object_name_data {
    struct bu_list name_components;
    struct bu_list separators;
    struct bu_list incrementals;
    struct bu_vls extension;
};

struct object_name_item {
   struct bu_list l;
   struct bu_vls namestring;
};

struct object_name_data *
parse_obj_name(const char *fmt, const char *name)
{
    struct object_name_data *objcomponents;
	struct object_name_item *objname;
	char *stringholder;
    int len = 0;
	int i;

	BU_GETSTRUCT(objcomponents, object_name_data);
	BU_LIST_INIT(&(objcomponents->name_components));
	BU_LIST_INIT(&(objcomponents->separators));
	BU_LIST_INIT(&(objcomponents->incrementals));
	bu_vls_init(&(objcomponents->extension));

	if (!fmt || fmt[0] == '\0' || !name || name[0] == '\0') { 
            bu_log("ERROR: empty name or format string passed to parse_name\n");
	    return;
        }
  	for (i = 0; i < strlen(fmt); i++) {
		switch ( fmt[i] ) {
			case 'n':
			{
				BU_GETSTRUCT(objname, object_name_item);
				BU_LIST_INIT(&(objname->l));
				bu_vls_init(&(objname->namestring));
				bu_vls_trunc(&(objname->namestring),0);
				if (fmt[i+1] == 'i' ) {
					while (name[len] != '.' && name[len] != '_' && name[len] != '-' && name[len] != '\0' && !isdigit(name[len])){
						bu_log("%c\n",name[len]);
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
				BU_GETSTRUCT(objname, object_name_item);
				BU_LIST_INIT(&(objname->l));
				bu_vls_init(&(objname->namestring));
				bu_vls_trunc(&(objname->namestring),0);
                                while (isdigit(name[len])) {
                                       bu_vls_putc(&(objname->namestring), name[len]);
                                       len++;
				}
				BU_LIST_INSERT(&(objcomponents->incrementals), &(objname->l));
			}
			break;
			case 's':
			{
				BU_GETSTRUCT(objname, object_name_item);
				BU_LIST_INIT(&(objname->l));
				bu_vls_init(&(objname->namestring));
                                bu_vls_trunc(&(objname->namestring),0);
				if (name[len] == '.' || name[len] == '_' || name[len] == '-'){
                                       bu_vls_putc(&(objname->namestring), name[len]);
                                       len++;
				} else {
                                       bu_vls_putc(&(objname->namestring), '-');
				       bu_log("Note: naming convention requires separator but none found at designated point in supplied object name - using '-'\n");
				}
				BU_LIST_INSERT(&(objcomponents->separators), &(objname->l));
			}
			break;
			case 'e':
			{
				if (i != strlen(fmt) && name[len] != '\0') {
					bu_log("Error - extension specified at position other than end of object name.\n");
				} else {
					if (name[len] == REGION_EXT || name[len] == COMB_EXT || name[len] == PRIM_EXT) {
						bu_vls_printf(&(objcomponents->extension), "%s", name[len]);
					} else {
					/*** add logic to check type of object named by supplied name and use default ***/
					}
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
	return objcomponents;
};		



main(int argc, char **argv)
{
 struct bu_vls temp;
 struct object_name_item *testitem;
 char corename[12]="core-001a.s";
 struct object_name_data *test;
 register struct bn_vlist *vp;
 test = parse_obj_name("nsins",corename);

 for ( BU_LIST_FOR( testitem, object_name_item, &(test->name_components) ) )  {
     bu_log("%s ",bu_vls_addr(&(testitem->namestring)));
 };
 bu_log("\n");
 for ( BU_LIST_FOR( testitem, object_name_item, &(test->separators) ) )  {
     bu_log("%s ",bu_vls_addr(&(testitem->namestring)));
 };
 bu_log("\n");
 for ( BU_LIST_FOR( testitem, object_name_item, &(test->incrementals) ) )  {
     bu_log("%s ",bu_vls_addr(&(testitem->namestring)));
 };
 bu_log("\n");

};
