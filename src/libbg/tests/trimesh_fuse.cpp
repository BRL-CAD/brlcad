/*                    T R I M E S H _ F U S E . C
 * BRL-CAD
 *
 * Copyright (c) 2011-2024 United States Government as represented by
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

#include "common.h"

#include <stdio.h>

#include <set>

#include "bu.h"
#include "bg.h"

int arb4_faces_ctrl[] = {
    0, 1, 2,
    2, 1, 3,
    0, 3, 1,
    0, 2, 3
};

fastf_t arb4_verts_ctrl[] = {
    1.5, -0.5, -1.5,
    1.5, 1.5, -1.5,
    1.5, 1.5, 0.5,
    -0.5, 1.5, -1.5
};

int arb4_faces_test[] = {
    0, 1, 2,
    3, 4, 5,
    6, 7, 8,
    9, 10, 11
};

fastf_t arb4_verts_test[] = {
    1.5, -0.5, -1.5,
    1.5, 1.5, -1.5,
    1.5, 1.5, 0.5,
    1.5, 1.5, 0.5,
    1.5, 1.5, -1.5,
    -0.5, 1.5, -1.5,
    1.5, 1.5, 0.5,
    -0.5, 1.5, -1.5,
    1.5, -0.5, -1.5,
    1.5, -0.5, -1.5,
    1.5, 1.5, 0.5,
    -0.5, 1.5, -1.5
};


class tface {
    public:
        tface(int vert1, int vert2, int vert3) {
	    int smallest = 0;
	    if (vert1 < vert2 && vert1 < vert3)
		smallest = 0;
	    if (vert2 < vert1 && vert2 < vert3)
		smallest = 1;
	    if (vert3 < vert1 && vert3 < vert2)
		smallest = 2;
	    if (smallest == 0) {
		v[0] = vert1;
		v[1] = vert2;
		v[2] = vert3;
	    }
	    if (smallest == 1) {
		v[0] = vert2;
		v[1] = vert3;
		v[2] = vert1;
	    }
	    if (smallest == 2) {
		v[0] = vert3;
		v[1] = vert1;
		v[2] = vert2;
	    }
	}

        tface(const tface &other) {
	    v[0] = other.v[0];
	    v[1] = other.v[1];
	    v[2] = other.v[2];
	}

        ~tface() {};

        bool operator==(tface other) const
        {
            bool c1 = (v[0] == other.v[0]);
            bool c2 = (v[1] == other.v[1]);
            bool c3 = (v[2] == other.v[2]);
            return (c1 && c2 && c3);
        }

        bool operator<(tface other) const
        {
            bool c1 = (v[0] < other.v[0]);
            bool c1e = (v[0] == other.v[0]);
	    bool c2 = (v[1] < other.v[1]);
	    bool c2e = (v[1] == other.v[1]);
	    bool c3 = (v[2] < other.v[2]);
     	    return (c1 || (c1e && c2) || (c1e && c2e && c3));
        }

	int v[3];
};

void
parray(std::set<tface> &f, const char *prefix)
{
    struct bu_vls faces = BU_VLS_INIT_ZERO;
    bu_vls_sprintf(&faces, "%s ", prefix);
    std::set<tface>::iterator f_it;
    for (f_it = f.begin(); f_it != f.end(); f_it++)
	bu_vls_printf(&faces, "%d,%d,%d  ", (*f_it).v[0], (*f_it).v[1], (*f_it).v[2]);
    bu_log("%s\n", bu_vls_cstr(&faces));
    bu_vls_free(&faces);
}

// As long as the three verts for a face are in the same relative order,
// it doesn't matter which one comes first.
int
fcheck(const char *tname, int *ff, int fcnt, int *ctrl, int ctrl_cnt)
{
    if (!ff || !ctrl || !tname || fcnt == 0 || ctrl_cnt == 0)
	return -1;

    if (fcnt != ctrl_cnt)
	return -1;

    std::set<tface> ctrl_fs;
    for (int i = 0; i < ctrl_cnt; i++) {
	tface nf(ctrl[3*i+0], ctrl[3*i+1], ctrl[3*i+2]);
	ctrl_fs.insert(nf);
    }

    std::set<tface> test_fs;
    for (int i = 0; i < fcnt; i++) {
	tface nf(ff[3*i+0], ff[3*i+1], ff[3*i+2]);
	test_fs.insert(nf);
    }

    if (ctrl_fs == test_fs)
	return 0;

    bu_log("%s: triangle sets did not match\n", tname);
    parray(ctrl_fs, "Ctrl:");
    parray(test_fs, "Test:");
    return -1;
}


int
main(int UNUSED(argc), const char *argv[])
{
    int ret = 0;

    bu_setprogname(argv[0]);

    int *ofaces = NULL;
    fastf_t *overts = NULL;
    int ofcnt = 0;
    int ovcnt = 0;
    if (bg_trimesh_fuse(&ofaces, &ofcnt, &overts, &ovcnt, arb4_faces_test, 4, arb4_verts_test, 12, 0.0005) < 0)
	ret = -1;

    if (fcheck("ARB4_fuse_small", ofaces, ofcnt, arb4_faces_ctrl, 4))
	ret = -1;

    return ret;
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
