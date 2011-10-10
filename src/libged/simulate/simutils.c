/*                         S I M U T I L S . C
 * BRL-CAD
 *
 * Copyright (c) 2011 United States Government as represented by
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
/*
 * Utilities for the simulate command.
 *
 * Used for creating bounding boxes, applying materials, arrows etc.
 *
 */

/* Private Header */
#include "simutils.h"

#define NORMAL_SCALE_FACTOR 1
#define MAX_FLOATING_POINT_STRLEN 20
#define ARROW_BASE_RADIUS 0.02
#define ARROW_TIP_RADIUS  0.001


void
print_usage(struct bu_vls *str)
{
    bu_vls_printf(str, "Usage: simulate <steps>\n\n");
    bu_vls_printf(str, "Currently this command adds all regions in the model database to a \n\
    simulation having only gravity as a force. The objects should fall towards the ground plane XY.\n");
    bu_vls_printf(str, "The positions of the regions are set after <steps> number of simulation steps.\n");
    bu_vls_printf(str, "-f <n> <x> <y> <z>\t- Specifies frequency of update(eg 1/60 Hz)(WIP)\n");
    bu_vls_printf(str, "-t <x> <y> <z>\t\t  - Specifies time for which to run(alternative to -n)(WIP)\n");
    return;
}


void
print_matrix(char *rb_namep, mat_t t)
{
    int i, j;
    struct bu_vls buffer = BU_VLS_INIT_ZERO;

    bu_vls_sprintf(&buffer, "------------Transformation matrix(%s)--------------\n",
		   rb_namep);

    for (i=0 ; i<4 ; i++) {
	for (j=0 ; j<4 ; j++) {
	    bu_vls_sprintf(&buffer, "t[%d]: %f\t", (i*4 + j), t[i*4 + j]);
	}
	bu_vls_strcat(&buffer, "\n");
    }

    bu_vls_strcat(&buffer, "-------------------------------------------------------\n");
    bu_log("%V", &buffer);
    bu_vls_free(&buffer);
}


void
print_rigid_body(struct rigid_body *rb)
{
    bu_log("print_rigid_body : \"%s\", state = %d\n", rb->rb_namep, rb->state);
}


void
print_manifold_list(struct rigid_body *rb)
{
    struct sim_manifold *current_manifold;
    int i;

    bu_log("print_manifold_list: %s\n", rb->rb_namep);

    for (current_manifold = rb->head_manifold; current_manifold != NULL;
	 current_manifold = current_manifold->next) {

    	bu_log("--Manifold between %s & %s has %d contacts--\n",
	       current_manifold->rbA->rb_namep,
	       current_manifold->rbB->rb_namep,
	       current_manifold->num_bt_contacts);

    	for (i=0; i<current_manifold->num_bt_contacts; i++) {
	    bu_log("%d, (%f, %f, %f):(%f, %f, %f), n(%f, %f, %f)\n",
		   i+1,
		   V3ARGS(current_manifold->bt_contacts[i].ptA),
		   V3ARGS(current_manifold->bt_contacts[i].ptB),
		   V3ARGS(current_manifold->bt_contacts[i].normalWorldOnB));
    	}
    }
}


void
print_command(char* cmd_args[], int argc)
{
    int i;
    char buffer[500] = "";
    for (i=0; i<argc; i++) {
    	sprintf(buffer, "%s %s", buffer, cmd_args[i]);
    }

    bu_log(buffer);
}


int
kill(struct ged *gedp, char *name)
{
    char *cmd_args[5];
    int argc = 2;

    /* Check if the duplicate already exists, and kill it if so */
    if (db_lookup(gedp->ged_wdbp->dbip, name, LOOKUP_QUIET) != RT_DIR_NULL) {
    	bu_log("kill: WARNING \"%s\" exists, deleting it\n", name);
    	cmd_args[0] = bu_strdup("kill");
    	cmd_args[1] = bu_strdup(name);
    	cmd_args[2] = (char *)0;

    	if (ged_kill(gedp, argc, (const char **)cmd_args) != GED_OK) {
	    bu_log("kill: ERROR Could not delete existing \"%s\"\n", name);
	    return GED_ERROR;
    	}

    	bu_free_array(argc, cmd_args, "kill: free cmd_args");
    }

    return GED_OK;
}


int
kill_copy(struct ged *gedp, struct directory *dp, char* new_name)
{
    char *cmd_args[5];
    int rv, argc = 3;

    if (kill(gedp, new_name) != GED_OK) {
    	bu_log("kill_copy: ERROR Could not delete existing \"%s\"\n", new_name);
    	return GED_ERROR;
    }

    /* Copy the passed prim/comb */
    cmd_args[0] = bu_strdup("copy");
    cmd_args[1] = bu_strdup(dp->d_namep);
    cmd_args[2] = bu_strdup(new_name);
    cmd_args[3] = (char *)0;
    rv = ged_copy(gedp, 3, (const char **)cmd_args);
    if (rv != GED_OK) {
    	bu_log("kill_copy: ERROR Could not copy \"%s\" to \"%s\"\n", dp->d_namep,
	       new_name);
    	return GED_ERROR;
    }

    bu_free_array(argc, cmd_args, "kill_copy: free cmd_args");

    return GED_OK;
}


int
add_to_comb(struct ged *gedp, char *target, char *add)
{
    char *cmd_args[5];
    int rv, argc = 4;

    cmd_args[0] = bu_strdup("comb");
    cmd_args[1] = bu_strdup(target);
    cmd_args[2] = bu_strdup("u");
    cmd_args[3] = bu_strdup(add);
    cmd_args[4] = (char *)0;
    rv = ged_comb(gedp, 4, (const char **)cmd_args);
    if (rv != GED_OK) {
        bu_log("add_to_comb: ERROR Could not add \"%s\" to the combination \"%s\"\n",
	       add, target);
        return GED_ERROR;
    }

    bu_free_array(argc, cmd_args, "add_to_comb: free cmd_args");

    return GED_OK;
}


int
line(struct ged *gedp, char* name, point_t from, point_t to,
		unsigned char r,
	    unsigned char g,
	    unsigned char b)
{
    char *cmd_args[20];
    int rv, argc = 19;
    char buffer_str[MAX_FLOATING_POINT_STRLEN];
    char *suffix_reg = "_reg";
    struct bu_vls reg_vls = BU_VLS_INIT_ZERO;

    /* Arrow line primitive name */
    bu_vls_sprintf(&reg_vls, "%s%s", name, suffix_reg);

    if (kill(gedp, name) != GED_OK) {
	bu_log("line: ERROR Could not delete existing \"%s\"\n", name);
	return GED_ERROR;
    }

    if (kill(gedp, bu_vls_addr(&reg_vls)) != GED_OK) {
	bu_log("line: ERROR Could not delete existing \"%s\"\n", bu_vls_addr(&reg_vls));
	return GED_ERROR;
    }

    cmd_args[0] = bu_strdup("in");
    cmd_args[1] = bu_strdup(name);
    cmd_args[2] = bu_strdup("bot");
    cmd_args[3] = bu_strdup("3");
    cmd_args[4] = bu_strdup("1");
    cmd_args[5] = bu_strdup("1");
    cmd_args[6] = bu_strdup("1");

    sprintf(buffer_str, "%f", from[0]); cmd_args[7] = bu_strdup(buffer_str);
    sprintf(buffer_str, "%f", from[1]); cmd_args[8] = bu_strdup(buffer_str);
    sprintf(buffer_str, "%f", from[2]); cmd_args[9] = bu_strdup(buffer_str);

    sprintf(buffer_str, "%f", to[0]); cmd_args[10] = bu_strdup(buffer_str);
    sprintf(buffer_str, "%f", to[1]); cmd_args[11] = bu_strdup(buffer_str);
    sprintf(buffer_str, "%f", to[2]); cmd_args[12] = bu_strdup(buffer_str);

    sprintf(buffer_str, "%f", from[0]); cmd_args[13] = bu_strdup(buffer_str);
    sprintf(buffer_str, "%f", from[1]); cmd_args[14] = bu_strdup(buffer_str);
    sprintf(buffer_str, "%f", from[2]); cmd_args[15] = bu_strdup(buffer_str);

    cmd_args[16] = bu_strdup("0");
    cmd_args[17] = bu_strdup("1");
    cmd_args[18] = bu_strdup("2");

    cmd_args[19] = (char *)0;

    print_command(cmd_args, 19);

    rv = ged_in(gedp, argc, (const char **)cmd_args);
    if (rv != GED_OK) {
	bu_log("line: ERROR Could not draw line \"%s\" (%f,%f,%f)-(%f,%f,%f) \n",
	       name, V3ARGS(from), V3ARGS(to));
	return GED_ERROR;
    }

    bu_free_array(argc, cmd_args, "line: free cmd_args");

    add_to_comb(gedp, bu_vls_addr(&reg_vls), name);
    apply_material(gedp, bu_vls_addr(&reg_vls), "plastic tr 0.9", r, g, b);
    apply_color(gedp, bu_vls_addr(&reg_vls), r, g, b);

    return GED_OK;
}


int
arrow(struct ged *gedp, char* name, point_t from, point_t to)
{
    char *cmd_args[20];
    int rv, argc = 19;
    char buffer_str[MAX_FLOATING_POINT_STRLEN];
    char *prefix_arrow_line = "arrow_line_";
    char *prefix_arrow_head = "arrow_head_";
    struct bu_vls arrow_line_vls = BU_VLS_INIT_ZERO, arrow_head_vls = BU_VLS_INIT_ZERO;
    char *prefixed_arrow_line, *prefixed_arrow_head;
    vect_t v;

    /* Arrow line primitive name */
    bu_vls_sprintf(&arrow_line_vls, "%s%s", prefix_arrow_line, name);
    prefixed_arrow_line = bu_vls_addr(&arrow_line_vls);

    /* Arrow line primitive name */
    bu_vls_sprintf(&arrow_head_vls, "%s%s", prefix_arrow_head, name);
    prefixed_arrow_head = bu_vls_addr(&arrow_head_vls);

    if (kill(gedp, prefixed_arrow_line) != GED_OK) {
	bu_log("line: ERROR Could not delete existing \"%s\"\n", prefixed_arrow_line);
	return GED_ERROR;
    }

    if (kill(gedp, prefixed_arrow_head) != GED_OK) {
	bu_log("line: ERROR Could not delete existing \"%s\"\n", prefixed_arrow_head);
	return GED_ERROR;
    }

    cmd_args[0] = bu_strdup("in");
    cmd_args[1] = bu_strdup(prefixed_arrow_line);
    cmd_args[2] = bu_strdup("bot");
    cmd_args[3] = bu_strdup("3");
    cmd_args[4] = bu_strdup("1");
    cmd_args[5] = bu_strdup("1");
    cmd_args[6] = bu_strdup("1");

    sprintf(buffer_str, "%f", from[0]); cmd_args[7] = bu_strdup(buffer_str);
    sprintf(buffer_str, "%f", from[1]); cmd_args[8] = bu_strdup(buffer_str);
    sprintf(buffer_str, "%f", from[2]); cmd_args[9] = bu_strdup(buffer_str);

    sprintf(buffer_str, "%f", to[0]); cmd_args[10] = bu_strdup(buffer_str);
    sprintf(buffer_str, "%f", to[1]); cmd_args[11] = bu_strdup(buffer_str);
    sprintf(buffer_str, "%f", to[2]); cmd_args[12] = bu_strdup(buffer_str);

    sprintf(buffer_str, "%f", from[0]); cmd_args[13] = bu_strdup(buffer_str);
    sprintf(buffer_str, "%f", from[1]); cmd_args[14] = bu_strdup(buffer_str);
    sprintf(buffer_str, "%f", from[2]); cmd_args[15] = bu_strdup(buffer_str);

    cmd_args[16] = bu_strdup("0");
    cmd_args[17] = bu_strdup("1");
    cmd_args[18] = bu_strdup("2");

    cmd_args[19] = (char *)0;

    print_command(cmd_args, 19);

    rv = ged_in(gedp, argc, (const char **)cmd_args);
    if (rv != GED_OK) {
	bu_log("line: ERROR Could not draw arrow line \"%s\" (%f,%f,%f)-(%f,%f,%f) \n",
	       prefixed_arrow_line, V3ARGS(from), V3ARGS(to));
	return GED_ERROR;
    }

    bu_free_array(argc, cmd_args, "arrow: free cmd_args");

    add_to_comb(gedp, name, prefixed_arrow_line);

    VSUB2(v, to, from);
    VUNITIZE(v);
    VSCALE(v, v, 0.1);
    bu_log("line: Unit vector (%f,%f,%f)\n", V3ARGS(v));

    argc = 11;
    cmd_args[0] = bu_strdup("in");
    cmd_args[1] = bu_strdup(prefixed_arrow_head);
    cmd_args[2] = bu_strdup("trc");

    sprintf(buffer_str, "%f", to[0]); cmd_args[3] = bu_strdup(buffer_str);
    sprintf(buffer_str, "%f", to[1]); cmd_args[4] = bu_strdup(buffer_str);
    sprintf(buffer_str, "%f", to[2]); cmd_args[5] = bu_strdup(buffer_str);

    sprintf(buffer_str, "%f", v[0]); cmd_args[6] = bu_strdup(buffer_str);
    sprintf(buffer_str, "%f", v[1]); cmd_args[7] = bu_strdup(buffer_str);
    sprintf(buffer_str, "%f", v[2]); cmd_args[8] = bu_strdup(buffer_str);


    sprintf(buffer_str, "%f", ARROW_BASE_RADIUS); cmd_args[9] = bu_strdup(buffer_str);
    sprintf(buffer_str, "%f", ARROW_TIP_RADIUS);  cmd_args[10] = bu_strdup(buffer_str);

    cmd_args[11] = (char *)0;

    print_command(cmd_args, 11);

    rv = ged_in(gedp, argc, (const char **)cmd_args);
    if (rv != GED_OK) {
	bu_log("line: ERROR Could not draw arrow head \"%s\" (%f,%f,%f)-(%f,%f,%f) \n",
	       prefixed_arrow_head, V3ARGS(from), V3ARGS(to));
	return GED_ERROR;
    }

    bu_free_array(argc, cmd_args, "apply_material: free cmd_args");

    add_to_comb(gedp, name, prefixed_arrow_head);

    return GED_OK;
}


int
apply_material(struct ged *gedp,
	       char* comb,
	       char* material,
	       unsigned char r,
	       unsigned char g,
	       unsigned char b)
{
    int rv, argc = 7;
    char buffer_str[MAX_FLOATING_POINT_STRLEN];
    char* cmd_args[28];

    cmd_args[0] = bu_strdup("mater");
    cmd_args[1] = bu_strdup(comb);
    cmd_args[2] = bu_strdup(material);

    sprintf(buffer_str, "%d", r); cmd_args[3] = bu_strdup(buffer_str);
    sprintf(buffer_str, "%d", g); cmd_args[4] = bu_strdup(buffer_str);
    sprintf(buffer_str, "%d", b); cmd_args[5] = bu_strdup(buffer_str);

    cmd_args[6] = bu_strdup("0");
    cmd_args[7] = (char *)0;

    rv = ged_mater(gedp, argc, (const char **)cmd_args);
    if (rv != GED_OK) {
	bu_log("apply_material: WARNING Could not adjust the material to %s(%d, %d, %d) for \"%s\"\n",
	       material, r, g, b, comb);
	return GED_ERROR;
    }

    bu_free_array(argc, cmd_args, "apply_material: free cmd_args");

    return GED_OK;
}


int
apply_color(struct ged *gedp,
	    char* name,
	    unsigned char r,
	    unsigned char g,
	    unsigned char b)
{
    struct directory *dp = NULL;
    struct rt_comb_internal *comb = NULL;
    struct rt_db_internal intern;
    struct bu_attribute_value_set avs;

    /* Look up directory pointer for the passed comb name */
    GED_DB_LOOKUP(gedp, dp, name, LOOKUP_NOISY, GED_ERROR);
    GED_CHECK_COMB(gedp, dp, GED_ERROR);
    GED_DB_GET_INTERNAL(gedp, &intern, dp, (fastf_t *)NULL, &rt_uniresource, GED_ERROR);

    /* Get a comb from the internal format */
    comb = (struct rt_comb_internal *)intern.idb_ptr;
    RT_CK_COMB(comb);

    /* Set the color related members */
    comb->rgb[0] = r;
    comb->rgb[1] = g;
    comb->rgb[2] = b;
    comb->rgb_valid = 1;
    comb->inherit = 0;

    /* Get the current attribute set of the comb from the db */
    bu_avs_init_empty(&avs);
    if (db5_get_attributes(gedp->ged_wdbp->dbip, &avs, dp)) {
	bu_vls_printf(gedp->ged_result_str, "apply_transforms: ERROR Cannot get attributes for object %s\n", dp->d_namep);
	bu_avs_free(&avs);
	return GED_ERROR;
    }

    /* Sync the changed attributes with the old ones */
    db5_standardize_avs(&avs);
    db5_sync_comb_to_attr(&avs, comb);
    db5_standardize_avs(&avs);

    /* Put back in db to allow drawing */
    GED_DB_PUT_INTERNAL(gedp, dp, &intern, &rt_uniresource, GED_ERROR);
    if (db5_update_attributes(dp, &avs, gedp->ged_wdbp->dbip)) {
	bu_vls_printf(gedp->ged_result_str, "apply_transforms: ERROR failed to update attributes\n");
	bu_avs_free(&avs);
	return GED_ERROR;
    }

    bu_avs_free(&avs);
    return GED_OK;
}


int
make_rpp(struct ged *gedp, vect_t min, vect_t max, char* name)
{
	int rv, argc = 9;
	char buffer_str[MAX_FLOATING_POINT_STRLEN];
	char* cmd_args[28];

	if (kill(gedp, name) != GED_OK) {
		bu_log("line: ERROR Could not delete existing \"%s\"\n", name);
		return GED_ERROR;
	}

	cmd_args[0] = bu_strdup("in");
	cmd_args[1] = bu_strdup(name);
	cmd_args[2] = bu_strdup("rpp");

	sprintf(buffer_str, "%f", min[0]); cmd_args[3] = bu_strdup(buffer_str);
	sprintf(buffer_str, "%f", max[0]); cmd_args[4] = bu_strdup(buffer_str);

	sprintf(buffer_str, "%f", min[1]); cmd_args[5] = bu_strdup(buffer_str);
	sprintf(buffer_str, "%f", max[1]); cmd_args[6] = bu_strdup(buffer_str);

	sprintf(buffer_str, "%f", min[2]); cmd_args[7] = bu_strdup(buffer_str);
	sprintf(buffer_str, "%f", max[2]); cmd_args[8] = bu_strdup(buffer_str);


	cmd_args[9] = (char *)0;

	rv = ged_in(gedp, argc, (const char **)cmd_args);
	if (rv != GED_OK) {
		bu_log("make_rpp: WARNING Could not insert RPP %s (%f, %f, %f):(%f, %f, %f)\n",
		   name, V3ARGS(min), V3ARGS(max));
		return GED_ERROR;
	}

	bu_free_array(argc, cmd_args, "make_rpp: free cmd_args");



	return GED_OK;
}


int
insert_AABB(struct ged *gedp, struct simulation_params *sim_params, struct rigid_body *current_node)
{
    char* cmd_args[28];
    char buffer[MAX_FLOATING_POINT_STRLEN];
    int rv, argc = 27;
    char *prefix = "bb_";
    char *prefix_reg = "bb_reg_";
    char *prefixed_name, *prefixed_reg_name;
    struct bu_vls buffer1 = BU_VLS_INIT_ZERO, buffer2 = BU_VLS_INIT_ZERO;
    point_t v;

    /* Prepare prefixed bounding box primitive name */
    bu_vls_sprintf(&buffer1, "%s%s", prefix, current_node->rb_namep);
   	prefixed_name = bu_vls_addr(&buffer1);

    /* Prepare prefixed bounding box region name */
    bu_vls_sprintf(&buffer2, "%s%s", prefix_reg, current_node->rb_namep);
  	prefixed_reg_name = bu_vls_addr(&buffer2);

    /* Delete existing bb prim and region */
    rv = kill(gedp, prefixed_name);
    if (rv != GED_OK) {
	bu_log("insertAABB: ERROR Could not delete existing bounding box arb8 : %s \
		so NOT attempting to add new bounding box\n", prefixed_name);
	return GED_ERROR;
    }

    rv = kill(gedp, prefixed_reg_name);
    if (rv != GED_OK) {
	bu_log("insertAABB: ERROR Could not delete existing bounding box region : %s \
		so NOT attempting to add new region\n", prefixed_reg_name);
	return GED_ERROR;
    }

    /* Setup the simulation result group union-ing the new objects */
    cmd_args[0] = bu_strdup("in");
    cmd_args[1] = bu_strdup(prefixed_name);
    cmd_args[2] = bu_strdup("arb8");

    /* Front face vertices */
    /* v1 */
    v[0] = current_node->btbb_center[0] + current_node->btbb_dims[0]/2;
    v[1] = current_node->btbb_center[1] + current_node->btbb_dims[1]/2;
    v[2] = current_node->btbb_center[2] - current_node->btbb_dims[2]/2;
    sprintf(buffer, "%f", v[0]); cmd_args[3] = bu_strdup(buffer);
    sprintf(buffer, "%f", v[1]); cmd_args[4] = bu_strdup(buffer);
    sprintf(buffer, "%f", v[2]); cmd_args[5] = bu_strdup(buffer);

    /* v2 */
    v[0] = current_node->btbb_center[0] + current_node->btbb_dims[0]/2;
    v[1] = current_node->btbb_center[1] + current_node->btbb_dims[1]/2;
    v[2] = current_node->btbb_center[2] + current_node->btbb_dims[2]/2;
    sprintf(buffer, "%f", v[0]); cmd_args[6] = bu_strdup(buffer);
    sprintf(buffer, "%f", v[1]); cmd_args[7] = bu_strdup(buffer);
    sprintf(buffer, "%f", v[2]); cmd_args[8] = bu_strdup(buffer);

    /* v3 */
    v[0] = current_node->btbb_center[0] + current_node->btbb_dims[0]/2;
    v[1] = current_node->btbb_center[1] - current_node->btbb_dims[1]/2;
    v[2] = current_node->btbb_center[2] + current_node->btbb_dims[2]/2;
    sprintf(buffer, "%f", v[0]); cmd_args[9]  = bu_strdup(buffer);
    sprintf(buffer, "%f", v[1]); cmd_args[10] = bu_strdup(buffer);
    sprintf(buffer, "%f", v[2]); cmd_args[11] = bu_strdup(buffer);

    /* v4 */
    v[0] = current_node->btbb_center[0] + current_node->btbb_dims[0]/2;
    v[1] = current_node->btbb_center[1] - current_node->btbb_dims[1]/2;
    v[2] = current_node->btbb_center[2] - current_node->btbb_dims[2]/2;
    sprintf(buffer, "%f", v[0]); cmd_args[12] = bu_strdup(buffer);
    sprintf(buffer, "%f", v[1]); cmd_args[13] = bu_strdup(buffer);
    sprintf(buffer, "%f", v[2]); cmd_args[14] = bu_strdup(buffer);

    /* Back face vertices */
    /* v5 */
    v[0] = current_node->btbb_center[0] - current_node->btbb_dims[0]/2;
    v[1] = current_node->btbb_center[1] + current_node->btbb_dims[1]/2;
    v[2] = current_node->btbb_center[2] - current_node->btbb_dims[2]/2;
    sprintf(buffer, "%f", v[0]); cmd_args[15] = bu_strdup(buffer);
    sprintf(buffer, "%f", v[1]); cmd_args[16] = bu_strdup(buffer);
    sprintf(buffer, "%f", v[2]); cmd_args[17] = bu_strdup(buffer);

    /* v6 */
    v[0] = current_node->btbb_center[0] - current_node->btbb_dims[0]/2;
    v[1] = current_node->btbb_center[1] + current_node->btbb_dims[1]/2;
    v[2] = current_node->btbb_center[2] + current_node->btbb_dims[2]/2;
    sprintf(buffer, "%f", v[0]); cmd_args[18] = bu_strdup(buffer);
    sprintf(buffer, "%f", v[1]); cmd_args[19] = bu_strdup(buffer);
    sprintf(buffer, "%f", v[2]); cmd_args[20] = bu_strdup(buffer);

    /* v7 */
    v[0] = current_node->btbb_center[0] - current_node->btbb_dims[0]/2;
    v[1] = current_node->btbb_center[1] - current_node->btbb_dims[1]/2;
    v[2] = current_node->btbb_center[2] + current_node->btbb_dims[2]/2;
    sprintf(buffer, "%f", v[0]); cmd_args[21] = bu_strdup(buffer);
    sprintf(buffer, "%f", v[1]); cmd_args[22] = bu_strdup(buffer);
    sprintf(buffer, "%f", v[2]); cmd_args[23] = bu_strdup(buffer);

    /* v8 */
    v[0] = current_node->btbb_center[0] - current_node->btbb_dims[0]/2;
    v[1] = current_node->btbb_center[1] - current_node->btbb_dims[1]/2;
    v[2] = current_node->btbb_center[2] - current_node->btbb_dims[2]/2;
    sprintf(buffer, "%f", v[0]); cmd_args[24] = bu_strdup(buffer);
    sprintf(buffer, "%f", v[1]); cmd_args[25] = bu_strdup(buffer);
    sprintf(buffer, "%f", v[2]); cmd_args[26] = bu_strdup(buffer);

    /* Finally make the bb primitive, phew ! */
    cmd_args[27] = (char *)0;
    rv = ged_in(gedp, argc, (const char **)cmd_args);
    if (rv != GED_OK) {
	bu_log("insertAABB: WARNING Could not draw bounding box for \"%s\"\n",
	       current_node->rb_namep);
    }

    bu_free_array(argc, cmd_args, "make_rpp: free cmd_args");

    /* Make the region for the bb primitive */
    add_to_comb(gedp, prefixed_reg_name, prefixed_name);

    /* Adjust the material for region to be almost transparent */
    apply_material(gedp, prefixed_reg_name, "plastic tr 0.9", 210, 0, 100);

    /* Add the region to the result of the sim so it will be drawn too */
    add_to_comb(gedp, sim_params->sim_comb_name, prefixed_reg_name);

    bu_vls_free(&buffer1);
    bu_vls_free(&buffer2);

    return GED_OK;

}


int
insert_manifolds(struct ged *gedp, struct simulation_params *sim_params, struct rigid_body *rb)
{
    char* cmd_args[28];
    char buffer[20];
    int rv, argc;
    char *prefix = "mf_";
    char *prefix_reg = "mf_reg_";
    char *prefix_normal = "normal_";
    struct bu_vls name = BU_VLS_INIT_ZERO,
    			  prefixed_name = BU_VLS_INIT_ZERO,
    			  prefixed_reg_name = BU_VLS_INIT_ZERO,
    			  prefixed_normal_name = BU_VLS_INIT_ZERO;
    vect_t scaled_normal;
    point_t from, to;
    struct sim_manifold *current_manifold;
    int i;

    for (current_manifold = rb->head_manifold; current_manifold != NULL;
	 current_manifold = current_manifold->next) {


	if(current_manifold->num_bt_contacts > 0){

	    /* Prepare prefixed bounding box primitive name */
	    bu_vls_sprintf(&name, "%s_%s", current_manifold->rbA->rb_namep, current_manifold->rbB->rb_namep);

	    /* Prepare the manifold shape name */
	    bu_vls_sprintf(&prefixed_name, "%s%s", prefix, bu_vls_addr(&name));

	    /* Prepare prefixed manifold region name */
	    bu_vls_sprintf(&prefixed_reg_name, "%s%s", prefix_reg, bu_vls_addr(&name));

	    /* Prepare prefixed manifold region name */
	    bu_vls_sprintf(&prefixed_normal_name, "%s%s", prefix_normal, bu_vls_addr(&name));

	    /* Delete existing manifold prim and region */
	    rv = kill(gedp, bu_vls_addr(&prefixed_name));
	    if (rv != GED_OK) {
		bu_log("insert_manifolds: ERROR Could not delete existing bounding box arb8 : %s \
					so NOT attempting to add new bounding box\n", bu_vls_addr(&prefixed_name));
		return GED_ERROR;
	    }

	    rv = kill(gedp, bu_vls_addr(&prefixed_reg_name));
	    if (rv != GED_OK) {
		bu_log("insert_manifolds: ERROR Could not delete existing bounding box region : %s \
					so NOT attempting to add new region\n", bu_vls_addr(&prefixed_reg_name));
		return GED_ERROR;
	    }

	    /* Setup the simulation result group union-ing the new objects */
	    cmd_args[0] = bu_strdup("in");
	    cmd_args[1] = bu_strdup(bu_vls_addr(&prefixed_name));
	    cmd_args[2] = (char *)0;
	    argc = 2;

	    switch(current_manifold->num_bt_contacts) {
		case 1:
		    bu_log("1 contact got, no manifold drawn");
		    break;

		case 2:
		    cmd_args[2] = bu_strdup("arb4");
		    sprintf(buffer, "%f", current_manifold->bt_contacts[0].ptA[0]);
		    cmd_args[3] = bu_strdup(buffer);
		    sprintf(buffer, "%f", current_manifold->bt_contacts[0].ptA[1]);
		    cmd_args[4] = bu_strdup(buffer);
		    sprintf(buffer, "%f", current_manifold->bt_contacts[0].ptA[2]);
		    cmd_args[5] = bu_strdup(buffer);

		    sprintf(buffer, "%f", current_manifold->bt_contacts[1].ptA[0]);
		    cmd_args[6] = bu_strdup(buffer);
		    sprintf(buffer, "%f", current_manifold->bt_contacts[1].ptA[1]);
		    cmd_args[7] = bu_strdup(buffer);
		    sprintf(buffer, "%f", current_manifold->bt_contacts[1].ptA[2]);
		    cmd_args[8] = bu_strdup(buffer);

		    sprintf(buffer, "%f", current_manifold->bt_contacts[1].ptB[0]);
		    cmd_args[9] = bu_strdup(buffer);
		    sprintf(buffer, "%f", current_manifold->bt_contacts[1].ptB[1]);
		    cmd_args[10] = bu_strdup(buffer);
		    sprintf(buffer, "%f", current_manifold->bt_contacts[1].ptB[2]);
		    cmd_args[11] = bu_strdup(buffer);

		    sprintf(buffer, "%f", current_manifold->bt_contacts[0].ptB[0]);
		    cmd_args[12] = bu_strdup(buffer);
		    sprintf(buffer, "%f", current_manifold->bt_contacts[0].ptB[1]);
		    cmd_args[13] = bu_strdup(buffer);
		    sprintf(buffer, "%f", current_manifold->bt_contacts[0].ptB[2]);
		    cmd_args[14] = bu_strdup(buffer);

		    cmd_args[15] = (char *)0;
		    argc = 15;

		    VADD2SCALE(from, current_manifold->bt_contacts[0].ptA,
			       current_manifold->bt_contacts[1].ptB, 0.5);
		    break;

		case 3:
		    bu_log("3 contacts got, no manifold drawn");
		    break;

		case 4:
		    cmd_args[2] = bu_strdup("arb8");
		    for (i=0; i<4; i++) {
			sprintf(buffer, "%f", current_manifold->bt_contacts[i].ptA[0]);
			cmd_args[3+i*3] = bu_strdup(buffer);
			sprintf(buffer, "%f", current_manifold->bt_contacts[i].ptA[1]);
			cmd_args[4+i*3] = bu_strdup(buffer);
			sprintf(buffer, "%f", current_manifold->bt_contacts[i].ptA[2]);
			cmd_args[5+i*3] = bu_strdup(buffer);

			sprintf(buffer, "%f", current_manifold->bt_contacts[i].ptB[0]);
			cmd_args[15+i*3] = bu_strdup(buffer);
			sprintf(buffer, "%f", current_manifold->bt_contacts[i].ptB[1]);
			cmd_args[16+i*3] = bu_strdup(buffer);
			sprintf(buffer, "%f", current_manifold->bt_contacts[i].ptB[2]);
			cmd_args[17+i*3] = bu_strdup(buffer);

			/* current_manifold->bt_contacts[i].ptA[0],
			   current_manifold->bt_contacts[i].ptA[1],
			   current_manifold->bt_contacts[i].ptA[2],
			   current_manifold->bt_contacts[i].ptB[0],
			   current_manifold->bt_contacts[i].ptB[1],
			   current_manifold->bt_contacts[i].ptB[2],
			   current_manifold->bt_contacts[i].normalWorldOnB[0],
			   current_manifold->bt_contacts[i].normalWorldOnB[1],
			   current cmd_argsrent_manifold->bt_contacts[i].normalWorldOnB[2]);*/
		    }
		    cmd_args[27] = (char *)0;
		    argc = 27;

		    VADD2SCALE(from, current_manifold->bt_contacts[0].ptA,
			       current_manifold->bt_contacts[2].ptB, 0.5);
		    break;

		default:
		    bu_log("%d contacts got, no manifold drawn", current_manifold->num_bt_contacts);
		    cmd_args[2] = (char *)0;
		    argc = 2;
		    break;
	    }

	    print_command(cmd_args, argc);

	    /* Finally make the manifold primitive, if proper command generated */
	    if (argc > 2) {
	    	rv = ged_in(gedp, argc, (const char **)cmd_args);
	    	if (rv != GED_OK) {
	    		bu_log("insert_manifolds: WARNING Could not draw manifold for \"%s\"\n", rb->rb_namep);
	    	}

	    	bu_free_array(argc, cmd_args, "insert_manifolds: free cmd_args");

			/* Make the region for the manifold primitive */
			add_to_comb(gedp, bu_vls_addr(&prefixed_reg_name), bu_vls_addr(&prefixed_name));

			/* Adjust the material for region to be visible */
			apply_material(gedp, bu_vls_addr(&prefixed_reg_name), "plastic tr 0.9", 210, 210, 0);

			/* Add the region to the result of the sim so it will be drawn too */
			add_to_comb(gedp, sim_params->sim_comb_name, bu_vls_addr(&prefixed_reg_name));

			/* Finally draw the normal */
			VSCALE(scaled_normal, current_manifold->bt_contacts[0].normalWorldOnB, NORMAL_SCALE_FACTOR);
			VADD2(to, scaled_normal, from);

			bu_log("insert_manifolds: line (%f,%f,%f)-> (%f,%f,%f)-> (%f,%f,%f) \n",
				   V3ARGS(current_manifold->bt_contacts[0].normalWorldOnB),
				   V3ARGS(to),
				   V3ARGS(scaled_normal));

			arrow(gedp, bu_vls_addr(&prefixed_normal_name), from, to);
			add_to_comb(gedp, sim_params->sim_comb_name, bu_vls_addr(&prefixed_normal_name));

	    }/*  if-argc */

	}/* if-num_bt_contacts */

    } /* end for-manifold */

    bu_vls_free(&name);
    bu_vls_free(&prefixed_name);
    bu_vls_free(&prefixed_reg_name);
    bu_vls_free(&prefixed_normal_name);


    return GED_OK;

}


/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
