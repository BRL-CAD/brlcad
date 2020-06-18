/*                   C H E C K _ N A M E S . C
 * BRL-CAD
 *
 * Copyright (c) 1993-2020 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */

#include "./iges_struct.h"
#include "./iges_extern.h"
#include <ctype.h>

char *
Add_brl_name(char *name)
{
    struct name_list *ptr;
    size_t namelen;
    size_t i;

    /* replace white space */
    namelen = strlen(name);
    if (namelen > NAMESIZE) {
	namelen = NAMESIZE;
	name[namelen] = '\0';
    }
    for (i = 0; i < namelen; i++) {
	if (isspace((int)name[i]) || name[i] == '/')
	    name[i] = '_';
    }

    /* Check if name already in list */
    ptr = name_root;
    while (ptr) {
	if (BU_STR_EQUAL(ptr->name, name)) {
	    return ptr->name;
	}
	ptr = ptr->next;
    }

    /* add this name to the list */
    BU_ALLOC(ptr, struct name_list);
    bu_strlcpy(ptr->name, name, namelen+1);
    ptr->next = name_root;
    name_root = ptr;

    return ptr->name;
}


char *
Make_unique_brl_name(char *name)
{

    int found_str_end, name_unique;
    size_t namelen, i, idx;
    struct name_list *ptr;
    char char_value;

    if (!name) {
	bu_exit(1, "iges-g; name is null pointer\n");
	return (char *)NULL;
    }

    found_str_end = 0;
    for (i = 0; i < NAMESIZE + 1; i++) {
	if (name[i] == '\0') {
	    found_str_end = 1;
	    namelen = i;
	    break;
	}
    }

    if (!found_str_end) {
	bu_exit(1, "iges-g; corrupt name string\n");
	return (char *)NULL;
    }

    /* replace white space */
    for (i = 0; i < namelen; i++) {
	if (isspace((int)name[i]) || name[i] == '/') {
	    name[i] = '_';
	}
    }

    if (namelen > 0) {
	ptr = name_root;
	name_unique = 1;
	while (ptr) {
	    if (BU_STR_EQUAL(name, ptr->name)) {
		name_unique = 0;
		break;
	    }
	    ptr = ptr->next;
	}
	if (name_unique) {
	    return Add_brl_name(name);
	}
    }

    idx = namelen;
    char_value = 'A';
    name_unique = 0;
    while (!name_unique && idx < NAMESIZE) {
	if (idx == 0 && char_value == 'A') {
	    name[idx] = char_value;
	    name[idx+1] = '\0';
	    char_value++;
	}
	ptr = name_root;
	name_unique = 1;
	while (ptr) {
	    if (BU_STR_EQUAL(name, ptr->name)) {
		name_unique = 0;
		break;
	    }
	    ptr = ptr->next;
	}
	if (!name_unique) {
	    name[idx] = char_value;
	    name[idx+1] = '\0';
	    if (char_value == 'Z') {
		char_value = 'a';
	    } else if (char_value == 'z') {
		idx++;
		char_value = 'A';
	    } else {
		char_value++;
	    }
	}
    }

    if (name_unique) {
	return Add_brl_name(name);
    }

    bu_exit(1, "Could not make name unique: (%s)\n", name);
    return (char *)NULL; /* make compilers happy */
}


void
Skip_field()
{
    int done = 0;
    int lencard;

    if (card[counter] == eofd) {
	/* This is an empty field */
	counter++;
	return;
    } else if (card[counter] == eord) /* Up against the end of record */
	return;

    if (card[72] == 'P')
	lencard = PARAMLEN;
    else
	lencard = CARDLEN;

    if (counter >= lencard)
	Readrec(++currec);

    while (!done) {
	while (card[counter++] != eofd && card[counter] != eord &&
	       counter <= lencard);
	if (counter > lencard && card[counter] != eord && card[counter] != eofd)
	    Readrec(++currec);
	else
	    done = 1;
    }

    if (card[counter] == eord)
	counter--;
}


void
Get_name(int entityno, int skip)
{
    int sol_num;
    int i, j, k;
    int no_of_assoc = 0;
    int no_of_props = 0;
    int name_de = 0;
    char *name;

    if (dir[entityno]->param <= pstart) {
	bu_log("Illegal parameter pointer for entity D%07d (%s)\n" ,
	       dir[entityno]->direct, dir[entityno]->name);
	return;
    }

    Readrec(dir[entityno]->param);
    Readint(&sol_num, "");
    for (i = 0; i < skip; i++)
	Skip_field();

    /* skip over the associativities */
    Readint(&no_of_assoc, "");
    for (k = 0; k < no_of_assoc; k++)
	Readint(&j, "");

    /* get property entity DE's */
    Readint(&no_of_props, "");
    for (k = 0; k < no_of_props; k++) {
	j = 0;
	Readint(&j, "");
	if (dir[(j-1)/2]->type == 406 &&
	    dir[(j-1)/2]->form == 15) {
	    /* this is a name */
	    name_de = j;
	    break;
	}
    }

    if (!name_de)
	return;

    Readrec(dir[(name_de-1)/2]->param);
    Readint(&sol_num, "");
    if (sol_num != 406) {
	/* this is not a property entity */
	bu_log("Check_names: entity at DE %d is not a property entity\n", name_de);
	return;
    }

    Readint(&i, "");
    if (i != 1) {
	bu_log("Bad property entity, form 15 (name) should have only one value, not %d\n", i);
	return;
    }

    Readname(&name, "");
    dir[entityno]->name = Make_unique_brl_name(name);
    bu_free((char *)name, "Get_name: name");

}


void
Get_drawing_name(int entityno)
{
    int entity_type;
    int no_of_views;
    int no_of_annot;
    int no_of_assoc;
    int no_of_props;
    int i, j, k;
    int name_de = 0;
    char *name;

    if (dir[entityno]->param <= pstart) {
	bu_log("Illegal parameter pointer for entity D%07d (%s)\n" ,
	       dir[entityno]->direct, dir[entityno]->name);
	return;
    }

    Readrec(dir[entityno]->param);
    Readint(&entity_type, "");
    if (entity_type != 404) {
	bu_log("Get_drawing_name: entity at P%07d (type %d) is not a drawing entity\n", dir[entityno]->param, entity_type);
	return;
    }

    Readint(&no_of_views, "");
    for (i = 0; i < no_of_views; i++) {
	for (j = 0; j < 3; j++)
	    Skip_field();
    }

    Readint(&no_of_annot, "");
    for (i = 0; i < no_of_annot; i++)
	Skip_field();
    /* skip over the associativities */
    Readint(&no_of_assoc, "");
    for (k = 0; k < no_of_assoc; k++)
	Readint(&j, "");

    /* get property entity DE's */
    Readint(&no_of_props, "");
    for (k = 0; k < no_of_props; k++) {
	j = 0;
	Readint(&j, "");
	if (dir[(j-1)/2]->type == 406 &&
	    dir[(j-1)/2]->form == 15) {
	    /* this is a name */
	    name_de = j;
	    break;
	}
    }

    if (!name_de)
	return;

    Readrec(dir[(name_de-1)/2]->param);
    Readint(&entity_type, "");
    if (entity_type != 406) {
	/* this is not a property entity */
	bu_log("Get_drawing_name: entity at DE %d is not a property entity\n", name_de);
	return;
    }

    Readint(&i, "");
    if (i != 1) {
	bu_log("Bad property entity, form 15 (name) should have only one value, not %d\n", i);
	return;
    }

    Readname(&name, "");
    dir[entityno]->name = Make_unique_brl_name(name);
    bu_free((char *)name, "Get_name: name");
}


void
Get_csg_name(int entityno)
{
    int sol_num;
    int i, j, k;
    int skip;
    int num = 0;
    int no_of_assoc = 0;
    int no_of_props = 0;
    int name_de = 0;
    char *name;

    if (dir[entityno]->param <= pstart) {
	bu_log("Illegal parameter pointer for entity D%07d (%s)\n" ,
	       dir[entityno]->direct, dir[entityno]->name);
	return;
    }

    Readrec(dir[entityno]->param);
    Readint(&sol_num, "");
    Readint(&num, "");
    if (sol_num == 180)
	skip = num;
    else if (sol_num == 184)
	skip = 2*num;
    else {
	bu_log("Get_csg_name: entity (type %d), not a CSG\n", sol_num);
	return;
    }

    for (i = 0; i < skip; i++)
	Skip_field();

    /* skip over the associativities */
    Readint(&no_of_assoc, "");
    for (k = 0; k < no_of_assoc; k++)
	Readint(&j, "");

    /* get property entity DE's */
    Readint(&no_of_props, "");
    for (k = 0; k < no_of_props; k++) {
	j = 0;
	Readint(&j, "");
	if (dir[(j-1)/2]->type == 406 &&
	    dir[(j-1)/2]->form == 15) {
	    /* this is a name */
	    name_de = j;
	    break;
	}
    }

    if (!name_de)
	return;

    Readrec(dir[(name_de-1)/2]->param);
    Readint(&sol_num, "");
    if (sol_num != 406) {
	/* this is not a property entity */
	bu_log("Check_names: entity at DE %d is not a property entity\n", name_de);
	return;
    }

    Readint(&i, "");
    if (i != 1) {
	bu_log("Bad property entity, form 15 (name) should have only one value, not %d\n", i);
	return;
    }

    Readname(&name, "");
    dir[entityno]->name = Make_unique_brl_name(name);
    bu_free((char *)name, "Get_name: name");
}


void
Get_brep_name(int entityno)
{
    int sol_num;
    int i, j, k;
    int num;
    int skip;
    int no_of_assoc = 0;
    int no_of_props = 0;
    int name_de = 0;
    char *name;

    if (dir[entityno]->param <= pstart) {
	bu_log("Illegal parameter pointer for entity D%07d (%s)\n" ,
	       dir[entityno]->direct, dir[entityno]->name);
	return;
    }

    Readrec(dir[entityno]->param);
    Readint(&sol_num, "");
    if (sol_num != 186) {
	bu_log("Get_brep_name: Entity (type %d) is not a BREP\n", sol_num);
	return;
    }
    Skip_field();
    Skip_field();
    Readint(&num, "");
    skip = 2*num;

    for (i = 0; i < skip; i++)
	Skip_field();

    /* skip over the associativities */
    Readint(&no_of_assoc, "");
    for (k = 0; k < no_of_assoc; k++)
	Readint(&j, "");

    /* get property entity DE's */
    Readint(&no_of_props, "");
    for (k = 0; k < no_of_props; k++) {
	j = 0;
	Readint(&j, "");
	if (dir[(j-1)/2]->type == 406 &&
	    dir[(j-1)/2]->form == 15) {
	    /* this is a name */
	    name_de = j;
	    break;
	}
    }

    if (!name_de)
	return;

    Readrec(dir[(name_de-1)/2]->param);
    Readint(&sol_num, "");
    if (sol_num != 406) {
	/* this is not a property entity */
	bu_log("Check_names: entity at DE %d is not a property entity\n", name_de);
	return;
    }

    Readint(&i, "");
    if (i != 1) {
	bu_log("Bad property entity, form 15 (name) should have only one value, not %d\n", i);
	return;
    }

    Readname(&name, "");
    dir[entityno]->name = Make_unique_brl_name(name);
    bu_free((char *)name, "Get_name: name");
}


void
Get_subfig_name(size_t entityno)
{
    int i;
    int entity_type = 0;
    char *name;

    if (entityno >= totentities)
	bu_exit(1, "Get_subfig_name: entityno too big!\n");

    if (dir[entityno]->type != 308) {
	bu_exit(1, "Get_subfig_name called with entity type %s, should be Subfigure Definition\n",
		iges_type(dir[entityno]->type));
    }

    if (dir[entityno]->param <= pstart) {
	bu_exit(1, "Illegal parameter pointer for entity D%07d (%s)\n" ,
		dir[entityno]->direct, dir[entityno]->name);
    }

    Readrec(dir[entityno]->param);

    Readint(&entity_type, "");
    if (entity_type != 308) {
	bu_exit(1, "Get_subfig_name: Read entity type %s, should be Subfigure Definition\n",
		iges_type(dir[entityno]->type));
    }

    Readint(&i, "");	/* ignore depth */
    Readname(&name, "");	/* get subfigure name */

    dir[entityno]->name = Add_brl_name(name);
    bu_free((char *)name, "Get_name: name");
}


void
Check_names()
{
    size_t i;

    bu_log("Looking for Name Entities...\n");
    for (i = 0; i < totentities; i++) {
	switch (dir[i]->type) {
	    case 152:
		Get_name(i, 13);
		break;
	    case 150:
	    case 168:
		Get_name(i, 12);
		break;
	    case 156:
		Get_name(i, 9);
		break;
	    case 154:
	    case 160:
	    case 162:
		Get_name(i, 8);
		break;
	    case 164:
		Get_name(i, 5);
		break;
	    case 158:
		Get_name(i, 4);
		break;
	    case 180:
	    case 184:
		Get_csg_name(i);
		break;
	    case 186:
		Get_brep_name(i);
		break;
	    case 308:
		Get_subfig_name(i);
		break;
	    case 404:
		Get_drawing_name(i);
		break;
	    case 410:
		if (dir[i]->form == 0)
		    Get_name(i, 8);
		else if (dir[i]->form == 1)
		    Get_name(i, 22);
		break;
	    case 430:
		Get_name(i, 1);
		break;
	    default:
		break;
	}
    }

    bu_log("Assigning names to entities without names...\n");

    for (i = 0; i < totentities; i++) {
	char tmp_name[NAMESIZE+1];

	if (dir[i]->name == (char *)NULL) {
	    switch (dir[i]->type) {
		case 150:
		    sprintf(tmp_name, "block.%d", (int)i);
		    dir[i]->name = Make_unique_brl_name(tmp_name);
		    break;
		case 152:
		    sprintf(tmp_name, "wedge.%d", (int)i);
		    dir[i]->name = Make_unique_brl_name(tmp_name);
		    break;
		case 154:
		    sprintf(tmp_name, "cyl.%d", (int)i);
		    dir[i]->name = Make_unique_brl_name(tmp_name);
		    break;
		case 156:
		    sprintf(tmp_name, "cone.%d", (int)i);
		    dir[i]->name = Make_unique_brl_name(tmp_name);
		    break;
		case 158:
		    sprintf(tmp_name, "sphere.%d", (int)i);
		    dir[i]->name = Make_unique_brl_name(tmp_name);
		    break;
		case 160:
		    sprintf(tmp_name, "torus.%d", (int)i);
		    dir[i]->name = Make_unique_brl_name(tmp_name);
		    break;
		case 162:
		    sprintf(tmp_name, "revolution.%d", (int)i);
		    dir[i]->name = Make_unique_brl_name(tmp_name);
		    break;
		case 164:
		    sprintf(tmp_name, "extrusion.%d", (int)i);
		    dir[i]->name = Make_unique_brl_name(tmp_name);
		    break;
		case 168:
		    sprintf(tmp_name, "ell.%d", (int)i);
		    dir[i]->name = Make_unique_brl_name(tmp_name);
		    break;
		case 180:
		    sprintf(tmp_name, "region.%d", (int)i);
		    dir[i]->name = Make_unique_brl_name(tmp_name);
		    break;
		case 184:
		    sprintf(tmp_name, "group.%d", (int)i);
		    dir[i]->name = Make_unique_brl_name(tmp_name);
		    break;
		case 186:
		    sprintf(tmp_name, "brep.%d", (int)i);
		    dir[i]->name = Make_unique_brl_name(tmp_name);
		    break;
		case 404:
		    sprintf(tmp_name, "drawing.%d", (int)i);
		    dir[i]->name = Make_unique_brl_name(tmp_name);
		    break;
		case 410:
		    sprintf(tmp_name, "view.%d", (int)i);
		    dir[i]->name = Make_unique_brl_name(tmp_name);
		    break;
		case 430:
		    sprintf(tmp_name, "inst.%d", (int)i);
		    dir[i]->name = Make_unique_brl_name(tmp_name);
		    break;
	    }
	}
    }
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
