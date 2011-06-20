/*                   	P C _ C O N S T R A I N T S . C
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
/** @addtogroup libpc */
/** @{ */
/** @file libpc/pc_constraints.c
 *
 * @brief Constraint evaluation functions in C
 *
 */

#include "common.h"

#include "vmath.h"
#include "pc.h"

/**
 * PC_ISPERPENDICULAR
 * The double pointer is expected to point to a properly initialized array
 */

int
pc_isperpendicular(double **v)
{
    /*
      bu_log("Dot product of the Vectors:\n%f %f %f \n%f %f %f\nis %f\n", \
      v[0][0], v[0][1], v[0][2], v[1][0], v[1][1], v[1][2], \
      (v[0][0]*v[1][0] + v[0][1]*v[1][1] +v[0][2]*v[1][2]));
    */
    if (ZERO((VDOT(v[0], v[1])))) /* TODO: this needs to be toleranced properly */
    	return 0;
    else
    	return -1;
}


void
pc_mk_isperpendicular(struct pc_constrnt **c, const char *name, const char **args)
{
    int i;
    
    pc_getconstraint_struct(c, 2);
    bu_vls_strcat(&((*c)->name), name);
    (*c)->data.cf.fp = &pc_isperpendicular;
    (*c)->data.cf.nargs = 2;
    (*c)->data.cf.dimension = 3;
    for (i = 0; i < 2; i++)
	(*c)->args[i] = args[i];
}


int
pc_isfixed(double **v)
{
    
    if (ZERO(MAGSQ(v[0]))) /* TODO: this needs to be toleranced properly */
    	return 0;
    else
    	return -1;
}


/**
 * PC_ISMODPOSITIVE
 *
 * Checks if the modulus of a given vector is positive
 * The double pointer is expected to point to a properly initialized array
 */

void
pc_mk_ismodpositive(struct pc_constrnt **c, const char *name, const char **args)
{
    pc_getconstraint_struct(c, 1);
    bu_vls_strcat(&((*c)->name), name);
    (*c)->data.cf.fp = &pc_isperpendicular;
    (*c)->data.cf.nargs = 1;
    (*c)->data.cf.dimension = 3;
    (*c)->args[0] = args[0];
}


/** @} */
/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
