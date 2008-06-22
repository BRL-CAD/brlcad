/*              	     P C _ C O N S T R A I N T . H
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
/** @addtogroup libpc */
/** @{ */
/** @file pc_constraint.h
 *
 * Structures required for implementing Constraint Solution
 * 
 * @author Dawn Thomas
 */
#ifndef __PC_CONSTRAINT_H__
#define __PC_CONSTRAINT_H__

#include "common.h"
#include "bu.h"
#include "bn.h"


/**
 * Structures required for describing solution sets, parameter instantiation
 *
 */

struct parameter_domain {
    double min;
    double max;
};

struct parameter_solution {
    int n_domains;
    struct  parameter_domain * domain;
};

struct constraint_solution {
    int n;
    struct parameter_solution * param;
};

/**
 * Structures required for describing constraint networks
 *
 */

struct cn_relation {

};

struct cn_node {
    struct parameter_domain pd;
    struct directory * dp;
    int solved_status;
    int n_relations;
    struct cn_edge * edge[];
};

struct cn_edge {
    int solved_status;
    int n_params;
    struct cn_node * node[];
};

__END_DECLS

#endif
/** @} */
/*
 * Local Variables:
 * mode: C++
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
