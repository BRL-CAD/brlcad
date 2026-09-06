/*                   C H E C K _ N A M E S . C
 * BRL-CAD
 *
 * Copyright (c) 1993-2026 United States Government as represented by
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
#include "iges_output.h"

char *
Add_brl_name(char *name)
{
    /* NOTE: this is obviously not thread-safe */
    static size_t nocnt = 0;
#define NOBUF_SZ 32
    char nobuf[NOBUF_SZ] = {0};

    struct name_list *ptr;
    size_t namelen;
    size_t i;

    if (!name) {
	snprintf(nobuf, NOBUF_SZ, "noname_%zu", nocnt++);
	name = nobuf;
    }

    /* replace white space */
    namelen = strlen(name);
    if (namelen > NAMESIZE) {
	namelen = NAMESIZE;
	name[namelen] = '\0';
    }
    for (i = 0; i < namelen; i++) {
	if (isspace((unsigned char)name[i]) || name[i] == '/')
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


static int
name_in_use(const char *name)
{
    if (fdout && db_lookup(fdout->dbip, name, LOOKUP_QUIET) != RT_DIR_NULL)
	return 1;
    for (const struct name_list *entry = name_root; entry; entry = entry->next)
	if (BU_STR_EQUAL(entry->name, name))
	    return 1;
    return 0;
}

char *
Make_unique_brl_name(char *name)
{
    char stem[NAMESIZE + 1];
    char candidate[NAMESIZE + 1];
    bu_strlcpy(stem, name && name[0] ? name : "unnamed", sizeof(stem));
    for (size_t i = 0; stem[i]; ++i)
	if (isspace((unsigned char)stem[i]) || stem[i] == '/')
	    stem[i] = '_';
    bu_strlcpy(candidate, stem, sizeof(candidate));
    for (size_t suffix = 1; name_in_use(candidate); ++suffix) {
	char ending[NAMESIZE + 1];
	const int length = snprintf(ending, sizeof(ending), "_%zu", suffix);
	if (length < 0 || length >= NAMESIZE)
	    bu_exit(1, "iges-g: exhausted unique object names\n");
	/* Truncation must leave room for a suffix even when the source
	 * name already fills the legacy field.  Also reserve names from
	 * the modern importer, not just the legacy name list. */
	snprintf(candidate, sizeof(candidate), "%.*s%s", NAMESIZE - length, stem, ending);
    }
    return Add_brl_name(candidate);
}


void
Skip_field(void)
{
    const int last_column = card[IGES_SECTION_COL] == 'P' ? PARAMLEN : CARDLEN;
    for (;;) {
	if (counter > last_column && Readrec(currec + 1)) {
	    iges_output_legacy_warning(0, "truncated_legacy_parameter",
		"parameter field extends past the end of the file");
	    return;
	}
	/* Leave the record terminator in place so an absent optional
	 * property list is not mistaken for the last digit of the geometry. */
	if (card[counter] == eord)
	    return;
	if (card[counter++] == eofd)
	    return;
    }
}


static void
read_entity_name(size_t entityno)
{
    const int name_de = Read_property(entityno, 406, 15);
    if (!name_de)
	return;
    int type = 0;
    int count = 0;
    char *name = NULL;
    Readrec(dir[IGES_DE2INDEX(name_de)]->param);
    Readint(&type, "");
    Readint(&count, "");
    if (type != 406 || count != 1) {
	iges_output_legacy_warning(name_de, "invalid_name_property",
	    "name property must contain exactly one name");
	return;
    }
    Readname(&name, "");
    if (name && name[0])
	dir[entityno]->name = Make_unique_brl_name(name);
    bu_free(name, "IGES property name");
}

static int
read_name_parameters(size_t entityno, int *type)
{
    if (dir[entityno]->param <= pstart) {
	iges_output_legacy_warning(dir[entityno]->direct, "invalid_name_parameters",
	    "entity has no readable name parameters");
	return 0;
    }
    Readrec(dir[entityno]->param);
    Readint(type, "");
    return 1;
}

static void
Get_name(size_t entityno, int skip)
{
    int type = 0;
    if (!read_name_parameters(entityno, &type))
	return;
    for (int i = 0; i < skip; ++i)
	Skip_field();
    read_entity_name(entityno);
}

void
Get_drawing_name(size_t entityno)
{
    int type = 0;
    int views = 0;
    int annotations = 0;
    if (!read_name_parameters(entityno, &type) || type != 404)
	return;
    Readint(&views, "");
    if (!iges_legacy_count(entityno, views, 3))
	return;
    for (int i = 0; i < views; ++i)
	for (int j = 0; j < 3; ++j)
	    Skip_field();
    Readint(&annotations, "");
    if (!iges_legacy_count(entityno, annotations, 1))
	return;
    for (int i = 0; i < annotations; ++i)
	Skip_field();
    read_entity_name(entityno);
}

void
Get_csg_name(size_t entityno)
{
    int type = 0;
    int count = 0;
    if (!read_name_parameters(entityno, &type) || (type != 180 && type != 184))
	return;
    Readint(&count, "");
    const size_t stride = type == 184 ? 2 : 1;
    if (!iges_legacy_count(entityno, count, stride))
	return;
    for (size_t i = 0; i < (size_t)count * stride; ++i)
	Skip_field();
    read_entity_name(entityno);
}

void
Get_brep_name(size_t entityno)
{
    int type = 0;
    int count = 0;
    if (!read_name_parameters(entityno, &type) || type != 186)
	return;
    Skip_field();
    Skip_field();
    Readint(&count, "");
    if (!iges_legacy_count(entityno, count, 2))
	return;
    for (size_t i = 0; i < (size_t)count * 2; ++i)
	Skip_field();
    read_entity_name(entityno);
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
	bu_exit(1, "Illegal parameter pointer for entity D%07d (%s)\n",
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

    dir[entityno]->name = Make_unique_brl_name(name);
    bu_free(name, "Get_name: name");
}


void
Check_names(void)
{
    size_t i;

    bu_log("Looking for Name Entities...\n");
    for (i = 0; i < totentities; i++) {
	if (dir[i]->direct_imported)
	    continue;
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
	char tmp_name[NAMESIZE+1] = {0};

	if (dir[i]->name == (char *)NULL) {
	    switch (dir[i]->type) {
		case 150:
		    snprintf(tmp_name, sizeof(tmp_name), "block.%d", (int)i);
		    dir[i]->name = Make_unique_brl_name(tmp_name);
		    break;
		case 152:
		    snprintf(tmp_name, sizeof(tmp_name), "wedge.%d", (int)i);
		    dir[i]->name = Make_unique_brl_name(tmp_name);
		    break;
		case 154:
		    snprintf(tmp_name, sizeof(tmp_name), "cyl.%d", (int)i);
		    dir[i]->name = Make_unique_brl_name(tmp_name);
		    break;
		case 156:
		    snprintf(tmp_name, sizeof(tmp_name), "cone.%d", (int)i);
		    dir[i]->name = Make_unique_brl_name(tmp_name);
		    break;
		case 158:
		    snprintf(tmp_name, sizeof(tmp_name), "sphere.%d", (int)i);
		    dir[i]->name = Make_unique_brl_name(tmp_name);
		    break;
		case 160:
		    snprintf(tmp_name, sizeof(tmp_name), "torus.%d", (int)i);
		    dir[i]->name = Make_unique_brl_name(tmp_name);
		    break;
		case 162:
		    snprintf(tmp_name, sizeof(tmp_name), "revolution.%d", (int)i);
		    dir[i]->name = Make_unique_brl_name(tmp_name);
		    break;
		case 164:
		    snprintf(tmp_name, sizeof(tmp_name), "extrusion.%d", (int)i);
		    dir[i]->name = Make_unique_brl_name(tmp_name);
		    break;
		case 168:
		    snprintf(tmp_name, sizeof(tmp_name), "ell.%d", (int)i);
		    dir[i]->name = Make_unique_brl_name(tmp_name);
		    break;
		case 180:
		    snprintf(tmp_name, sizeof(tmp_name), "region.%d", (int)i);
		    dir[i]->name = Make_unique_brl_name(tmp_name);
		    break;
		case 184:
		    snprintf(tmp_name, sizeof(tmp_name), "group.%d", (int)i);
		    dir[i]->name = Make_unique_brl_name(tmp_name);
		    break;
		case 186:
		    snprintf(tmp_name, sizeof(tmp_name), "brep.%d", (int)i);
		    dir[i]->name = Make_unique_brl_name(tmp_name);
		    break;
		case 404:
		    snprintf(tmp_name, sizeof(tmp_name), "drawing.%d", (int)i);
		    dir[i]->name = Make_unique_brl_name(tmp_name);
		    break;
		case 410:
		    snprintf(tmp_name, sizeof(tmp_name), "view.%d", (int)i);
		    dir[i]->name = Make_unique_brl_name(tmp_name);
		    break;
		case 430:
		    snprintf(tmp_name, sizeof(tmp_name), "inst.%d", (int)i);
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
