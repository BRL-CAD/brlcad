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
#include "bn.h"
#include "db.h"
#include "raytrace.h"
#include "bu.h"

#define ASSEM_EXT ' '
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
	BU_GETSTRUCT(objcomponents, object_name_data);
	BU_LIST_INIT(&(objcomponents->name_components));
	BU_LIST_INIT(&(objcomponents->separators));
	BU_LIST_INIT(&(objcomponents->incrementals));
	bu_vls_init(&(objcomponents->extension));

	struct object_name_item *objname;
	
	struct db_i *dbip = DBI_NULL;
	struct directory *dp = (struct directory *)NULL;
	union tree *ckregtree;
	struct db_full_path  path;
	struct resource *resp = &rt_uniresource;
	struct db_tree_state ts = rt_initial_tree_state;	
	struct combined_tree_state *region_start_statep;
	int rcount = 0;

	char *stringholder;
	int ignore_separator_flag = 0;
	int object_type; /* 0 = unknown, 1 = solid, 2 = comb, 3 = region, 4 = assembly*/

	int len = 0;
	int i;

	if (!fmt || fmt[0] == '\0' || !name || name[0] == '\0') { 
            bu_log("ERROR: empty name or format string passed to parse_name\n");
	    return;
        }

	/*Need logic here to deterine what this object is
	
	* use db_count_subtree_regions to distinguish assemblies and combinations	

	*/
	dbip = db_open( "./test.g", "r" );
	db_dirbuild(dbip);
	dp = db_lookup(dbip, name, LOOKUP_NOISY);
	
	if (!(dp->d_flags & DIR_SOLID)) {
		region_start_statep = (struct combined_tree_state *)0;
		db_full_path_init( &path );
		ts.ts_dbip = dbip;
		ts.ts_resp = resp;
		db_follow_path_for_state(&ts, &path, name, LOOKUP_NOISY);
		ckregtree = db_recurse(&ts, &path, &region_start_statep, NULL );
		rcount = db_count_subtree_regions(ckregtree);
	}

	bu_log("rcount is %d\n",rcount);	

       	if (!dp) {
		object_type = 0;
		bu_log("Object %s not found in database.\n",name);
	}
	if (dp->d_flags & DIR_SOLID) {
		object_type = 1;
	}
	if (dp->d_flags & DIR_REGION) {
		object_type = 3;
	}
	if ((dp->d_flags & DIR_COMB) && !(dp->d_flags & DIR_REGION)) {
		if (rcount > 0) {
			object_type = 4;
		} else {
			object_type = 2;
		}
	}

	bu_log("Object type is %d\n",object_type);


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
					while (name[len] != '\0' && !isdigit(name[len])){
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
				/*objtype = dblookup...*/
				/*if (objtype == assembly && ASSEM_EXT==' ' && fmt[i+1] == 's') {
					ignore_separator_flag = 1;
					len++;
				} else {
				*/	if (name[len] == REGION_EXT || name[len] == COMB_EXT || name[len] == PRIM_EXT) {
						bu_vls_putc(&(objcomponents->extension), name[len]);
						len++;
					} else {
					/*** add logic to check type of object named by supplied name and use default ***/
					}
				/*}*/
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
	db_close(dbip);
	return objcomponents;
};		


void test_obj_struct(const char *fmt, struct bu_vls *testvls){
	
	struct object_name_item *testitem;
	struct object_name_data *test;
	test = parse_obj_name(fmt, bu_vls_addr(testvls));
	for ( BU_LIST_FOR( testitem, object_name_item, &(test->name_components) ) )  {
		bu_log("%s ",bu_vls_addr(&(testitem->namestring)));
	}
        bu_log("\n");
	for ( BU_LIST_FOR( testitem, object_name_item, &(test->separators) ) )  {
     		bu_log("%s ",bu_vls_addr(&(testitem->namestring)));
 	};
	bu_log("\n");
	for ( BU_LIST_FOR( testitem, object_name_item, &(test->incrementals) ) )  {
	    bu_log("%s ",bu_vls_addr(&(testitem->namestring)));
	}
	bu_log("\n");
	bu_log("%s ",bu_vls_addr(&(test->extension)));
	bu_log("\n\n");
}



struct bu_vls *
assem_obj_name(const char *fmt, struct object_name_data *templatedata, int currentcount)
{
	struct bu_vls stringassembly;
	bu_vls_init(&stringassembly);
}

main(int argc, char **argv)
{
 struct bu_vls temp;
 bu_vls_init(&temp);
 
 bu_vls_trunc(&temp,0);
 bu_vls_printf(&temp,"%s","core-001a.s");
 test_obj_struct("nsinse", &temp);

 bu_vls_trunc(&temp,0);
 bu_vls_printf(&temp,"%s","s.bcore12.b3");
 test_obj_struct("esnisni", &temp);

 bu_vls_trunc(&temp,0);
 bu_vls_printf(&temp,"%s","comb1.c");
 test_obj_struct("nise", &temp);

 bu_vls_trunc(&temp,0);
 bu_vls_printf(&temp,"%s","comb2.r");
 test_obj_struct("nise", &temp);

 bu_vls_trunc(&temp,0);
 bu_vls_printf(&temp,"%s","comb3.r");
 test_obj_struct("nise", &temp);

 bu_vls_trunc(&temp,0);
 bu_vls_printf(&temp,"%s","assem1");
 test_obj_struct("ni", &temp);


};
