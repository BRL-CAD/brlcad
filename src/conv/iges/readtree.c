/*                      R E A D T R E E . C
 * BRL-CAD
 *
 * Copyright (c) 1990-2011 United States Government as represented by
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

union tree *
Readtree(mat_t *matp)
{
    int length, i, k, op;
    union tree *ptr, *Pop();
    matp_t new_mat;

    Readint(&i, "");
    if (i != 180) {
	bu_log("Expecting a Boolean Tree, found type %d\n", i);
	return (union tree *)NULL;
    }

    Freestack();
    Readint(&length, "");
    for (i=0; i<length; i++) {
	Readint(&op, "");
	if (op < 0) {
	    /* This is an operand */
	    BU_GET(ptr, union tree);
	    RT_TREE_INIT(ptr);
	    ptr->tr_l.tl_op = OP_DB_LEAF;
	    k = ((-op)-1)/2;
	    if (k < 0 || k >= totentities) {
		bu_log("Readtree(): pointer in tree is out of bounds (%d)\n", -op);
		return (union tree *)NULL;
	    }
	    if (dir[k]->type <= 0) {
		bu_log("Unknown entity type (%d) at D%07d\n", -dir[k]->type, dir[k]->direct);
		return (union tree *)NULL;
	    }
	    ptr->tr_l.tl_name = bu_strdup(dir[k]->name);
	    if (matp && dir[k]->rot) {
		new_mat = (matp_t)bu_malloc(sizeof(mat_t), "new_mat");
		bn_mat_mul(new_mat, *matp, *dir[k]->rot);
	    } else if (dir[k]->rot)
		new_mat = *dir[k]->rot;
	    else if (matp)
		new_mat = *matp;
	    else
		new_mat = (matp_t)NULL;
	    ptr->tr_l.tl_mat = new_mat;
	    Push(ptr);
	} else {
	    /* This is an operator */
	    BU_GET(ptr, union tree);
	    RT_TREE_INIT(ptr);
	    switch (op) {
		case 1:
		    ptr->tr_b.tb_op = OP_UNION;
		    break;
		case 2:
		    ptr->tr_b.tb_op = OP_INTERSECT;
		    break;
		case 3:
		    ptr->tr_b.tb_op = OP_SUBTRACT;
		    break;
		default:
		    bu_exit(1, "Readtree(): illegal operator code (%d)\n", op);
	    }
	    ptr->tr_b.tb_right = Pop();
	    ptr->tr_b.tb_left = Pop();
	    Push(ptr);
	}
    }

    return Pop();
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
