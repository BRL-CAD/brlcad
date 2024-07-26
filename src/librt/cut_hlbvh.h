/* 
 * BRL-CAD
 *
 * Copyright (c) 1999-2024 United States Government as represented by
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

struct bvh_build_node {
    fastf_t bounds[6];
    struct bvh_build_node *children[2];
    long first_prim_offset, n_primitives;
    uint8_t split_axis;
};

struct bvh_flat_node {
    fastf_t bounds[6];
    long n_primitives;
    union {
	long first_prim_offset;
	struct bvh_flat_node *other_child;
    } data;
};

#ifndef HLBVH_IMPLEMENTATION

extern struct bu_pool *
hlbvh_init_pool(size_t n_primatives);

extern struct bvh_build_node *
hlbvh_create(long max_prims_in_node, struct bu_pool *pool, const fastf_t *centroids_prims,
	     const fastf_t *bounds_prims, long *total_nodes,
	     const long n_primitives, long **ordered_prims);

extern struct bvh_flat_node *
hlbvh_flatten(const struct bvh_build_node *root, long nodes_created);

extern void
hlbvh_shot_raw(struct bvh_build_node* root, struct xray* rp, long** check_tris, size_t* num_check_tris);

extern void
hlbvh_shot_flat(struct bvh_flat_node* root, struct xray* rp, long** check_tris, size_t* num_check_tris);

#endif // HLBVH_IMPLEMENTATION


/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */

