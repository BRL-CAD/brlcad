/*                      C O N V I N S T . C
 * BRL-CAD
 *
 * Copyright (c) 1990-2025 United States Government as represented by
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

void
Convinst(void)
{

    size_t i = 0;
    int j = 0;
    int k = 0;
    int type = 0;
    int pointer = 0;
    int conv = 0;
    int totinst = 0;
    int no_of_assoc = 0;
    int no_of_props = 0;
    int att_de = 0;
    struct brlcad_att brl_att;
    mat_t *rot;

    for (i = 0; i < totentities; i++) {
	if (dir[i]->type != 430) /* This is not an instance */
	    continue;

	totinst++;

	/* read parameters */
	if (dir[i]->param <= pstart) {
	    bu_log("Illegal parameter pointer for entity D%07d (%s)\n" ,
		   dir[i]->direct, dir[i]->name);
	    continue;
	}
	Readrec(dir[i]->param);
	Readint(&type, "");
	Readint(&pointer, "");

	/* convert pointer to a "dir" index */
	pointer = (pointer - 1)/2;
	if (pointer < 0 || (size_t)pointer >= totentities) {
	    bu_log("Primitive instance D%07d (%s) does not point to a legal primitive\n",
		   dir[i]->direct, dir[i]->name);
	    continue;
	}

	/* skip over the associativities */
	Readint(&no_of_assoc, "");
	for (k = 0; k < no_of_assoc; k++)
	    Readint(&j, "");

	/* get property entity DE's */
	att_de = 0;
	Readint(&no_of_props, "");
	for (k = 0; k < no_of_props; k++) {
	    Readint(&j, "");
	    if (dir[(j-1)/2]->type == 422 &&
		dir[(j-1)/2]->referenced == brlcad_att_de) {
		/* this is one of our attribute instances */
		att_de = j;
	    }
	}

	memset(&brl_att, 0, sizeof(struct brlcad_att));
	Read_att(att_de, &brl_att);

	if (att_de) {
	    /* This is actually a region or a group with just one member */
	    unsigned char *rgb;
	    struct wmember head;

	    BU_LIST_INIT(&head.l);
	    (void)mk_addmember(dir[pointer]->name, &head.l, NULL, WMOP_INTERSECT);

	    /* Make the object */
	    if (dir[i]->colorp != 0)
		rgb = (unsigned char*)dir[i]->rgb;
	    else
		rgb = (unsigned char *)0;

	    mk_lrcomb(fdout ,
		      dir[i]->name,		/* name */
		      &head,			/* members */
		      brl_att.region_flag,	/* region flag */
		      brl_att.material_name,	/* material name */
		      brl_att.material_params, /* material parameters */
		      rgb,			/* color */
		      brl_att.ident,		/* ident */
		      brl_att.air_code,	/* air code */
		      brl_att.material_code,	/* GIFT material */
		      brl_att.los_density,	/* los density */
		      brl_att.inherit);	/* inherit */

	} else {
	    /* copy pointed to object info to replace instance entity */
	    dir[i]->type = dir[pointer]->type;
	    dir[i]->form = dir[pointer]->form;
	    dir[i]->param = dir[pointer]->param;
	    dir[i]->name = dir[pointer]->name;

	    /* increment reference count for pointed to entity */
	    dir[pointer]->referenced++;

	    /* fix up transformation matrix if needed */
	    if (dir[i]->trans == 0 && dir[pointer]->trans == 0) {
		continue;	/* nothing to do */
	    } else if (dir[i]->trans == 0) {
		dir[i]->trans = dir[pointer]->trans;	/* same as instanced */
	    } else if (dir[i]->trans != 0) {
		/* this instance refers to a transformation entity
		   but the original instanced object does too,
		   these matrices need to be combined */

		BU_ALLOC(rot, mat_t);
#if defined(USE_BN_MULT_)
		/* o <= a X b */
		bn_mat_mul(*rot, *(dir[i]->rot), *(dir[pointer]->rot));
#else
		/* a X b => o */
		Matmult(*(dir[i]->rot), *(dir[pointer]->rot), *rot);
#endif
		dir[i]->rot = rot;
	    }
	}
	conv++;
    }

    bu_log("\nConverted %d solid instances out of %d total instances\n", conv, totinst);
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
