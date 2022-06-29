/*                           T E A . H
 * BRL-CAD
 *
 * Copyright (c) 2004-2022 United States Government as represented by
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
/** @file proc-db/brep_arbintersection.h
 *
 * Test arb8 intersection cases
 *
 */

#ifndef PROC_DB_BREP_ARBINTERSECTION_H
#define PROC_DB_BREP_ARBINTERSECTION_H


struct rt_arb_internal* arb_0[2];
struct ON_3dPoint v_arb_0[2][8] = { {
    { 0.0,  0.0,  1.0 },
    { 1.5,  0.0,  1.0 },
    { 1.5,  1.2,  1.0 },
    { 0.0,  1.0,  1.0 },
    { 0.0,  0.0,  0.0 },
    { 1.5,  0.0,  0.0 },
    { 1.5,  1.0,  0.0 },
    { 0.0,  1.0,  0.0 },
},
    {{ 1.0,  0.0,  1.0 },
    { 2.0,  0.0,  1.0 },
    { 2.0,  1.2,  1.0 },
    { 1.0,  1.0,  1.0 },
    { 1.0,  0.0,  0.0 },
    { 2.0,  0.0,  0.0 },
    { 2.0,  1.0,  0.0 },
    { 1.0,  1.0,  0.0 },
}
};


// case 1: two arb8 intersect at one point (outer + OP_UNION£©
int arb_1_num = 2;
struct rt_arb_internal* arb_1[2];
struct ON_3dPoint arb_1_v[2][8] = { {
    { 0,  0,  0 },
    { 0,  1,  0 },
    { 1,  1,  0 },
    { 1,  0,  0 },
    { 0,  0,  1 },
    { 0,  1,  1 },
    { 1,  1,  1 },
    { 1,  0,  1 },
},
   {{ 1,  1,  1 },
    { 1,  2,  1 },
    { 2,  2,  1 },
    { 2,  1,  1 },
    { 1,  1,  2 },
    { 1,  2,  2 },
    { 2,  2,  2 },
    { 2,  1,  2 },
}
};

// case 2: two arb8 intersect at one line (outer + OP_UNION£©
int arb_2_num = 2;
struct rt_arb_internal* arb_2[2];
struct ON_3dPoint arb_2_v[2][8] = { {
    { 0,  0,  0 },
    { 0,  1,  0 },
    { 1,  1,  0 },
    { 1,  0,  0 },
    { 0,  0,  1 },
    { 0,  1,  1 },
    { 1,  1,  1 },
    { 1,  0,  1 },
},
   {{ 0,  1,  1 },
    { 0,  2,  1 },
    { 1,  2,  1 },
    { 1,  1,  1 },
    { 0,  1,  2 },
    { 0,  2,  2 },
    { 1,  2,  2 },
    { 1,  1,  2 },
}
};


// case 3: two arb8 intersect at one face (outer + OP_UNION£©
int arb_3_num = 2;
struct rt_arb_internal* arb_3[2];
struct ON_3dPoint arb_3_v[2][8] = { {
    { 0,  0,  0 },
    { 0,  1,  0 },
    { 1,  1,  0 },
    { 1,  0,  0 },
    { 0,  0,  1 },
    { 0,  1,  1 },
    { 1,  1,  1 },
    { 1,  0,  1 },
},
   {{ -1,  1, 0 },
    { -1,  2, 0 },
    { 2,  2,  0 },
    { 2,  1,  0 },
    { -1,  1, 1 },
    { -1,  2, 1 },
    { 2,  2,  1 },
    { 2,  1,  1 },
}
};


// case 4: two arb8 intersect (intersect + OP_UNION£©
int arb_4_num = 2;
struct rt_arb_internal* arb_4[2];
struct ON_3dPoint arb_4_v[2][8] = { {
    { 0,  0,  0 },
    { 0,  1.5,  0 },
    { 1,  1.5,  0 },
    { 1,  0,  0 },
    { 0,  0,  1 },
    { 0,  1.5,  1 },
    { 1,  1.5,  1 },
    { 1,  0,  1 },
},
   {{ -1,  1, 0 },
    { -1,  2, 0 },
    { 2,  2,  0 },
    { 2,  1,  0 },
    { -1,  1, 1 },
    { -1,  2, 1 },
    { 2,  2,  1 },
    { 2,  1,  1 },
}
};


#endif /* PROC_DB_BREP_ARBINTERSECTION_H */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
