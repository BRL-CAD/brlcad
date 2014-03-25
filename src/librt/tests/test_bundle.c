/*                   T E S T _ B U N D L E . C
 * BRL-CAD
 *
 * Copyright (c) 2013-2014 United States Government as represented by
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


#include "bn.h"
#include "vmath.h"
#include "plot3.h"
#include "raytrace.h"


int
main(int ac, char *av[])
{
    FILE *fp;
    int rays_per_ring=50;
    int nring=30;
    fastf_t bundle_radius=1000.0;
    int i;
    vect_t avec, bvec;
    struct xray *rp;
    vect_t dir;

    if (ac > 1)
	bu_exit(1, "Usage: %s\n", av[0]);

    fp = fopen("bundle.plot3", "wb");

    VSET(dir, 0, 0, -1);

    /* create orthogonal rays for basis of bundle */
    bn_vec_ortho(avec, dir);
    VCROSS(bvec, dir, avec);
    VUNITIZE(bvec);

    rp = (struct xray *)bu_calloc(sizeof(struct xray),
				  (rays_per_ring * nring) + 1,
				  "ray bundle");
    VSET(rp[0].r_pt, 0, 0, 2000);
    VMOVE(rp[0].r_dir, dir);
    rt_raybundle_maker(rp, bundle_radius, avec, bvec, rays_per_ring, nring);


    for (i=0; i <= rays_per_ring * nring; i++) {
	point_t tip;
	VJOIN1(tip, rp[i].r_pt, 3500, rp[i].r_dir);
	pdv_3line(fp, rp[i].r_pt, tip);
    }
    fclose(fp);
    bu_free(rp, "ray bundle");

    return 0;
}

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
