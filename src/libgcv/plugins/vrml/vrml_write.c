/*                    V R M L _ W R I T E . C
 * BRL-CAD
 *
 * Copyright (c) 1995-2024 United States Government as represented by
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
 *
 */
/** @file vrml_write.c
 *
 * Convert a BRL-CAD model (in a .g file) to a VRML (2.0)
 * faceted model by calling on the NMG booleans.
 *
 */

#include "common.h"

/* system headers */
#include <stdlib.h>
#include <stddef.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <math.h>
#include <string.h>
#include "bio.h"

/* interface headers */
#include "vmath.h"
#include "bu/getopt.h"
#include "bu/units.h"
#include "gcv/api.h"
#include "nmg.h"
#include "rt/geom.h"
#include "raytrace.h"
#include "wdb.h"

#define TXT_BUF_LEN 512
#define TXT_NAME_SIZE 128

struct plate_mode {
    int num_bots;
    int num_nonbots;
    int array_size;
    struct rt_bot_internal **bots;
};


struct vrml_mat {
    /* typical shader parameters */
    char shader[TXT_NAME_SIZE];
    int shininess;
    double transparency;

    /* light parameters */
    fastf_t lt_fraction;
    vect_t lt_dir;
    fastf_t lt_angle;

    /* texture parameters */
    char tx_file[TXT_NAME_SIZE];
    int tx_w;
    int tx_n;
};


#define PL_O(_m) bu_offsetof(struct vrml_mat, _m)

const struct bu_structparse vrml_mat_parse[]={
    {"%s", TXT_NAME_SIZE, "ma_shader", PL_O(shader), BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    {"%d", 1, "shine", PL_O(shininess), BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    {"%d", 1, "sh", PL_O(shininess), BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    {"%g", 1, "transmit", PL_O(transparency), BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    {"%g", 1, "tr", PL_O(transparency), BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    {"%f", 1, "angle", PL_O(lt_angle), BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    {"%f", 1, "fract", PL_O(lt_fraction), BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    {"%f", 3, "aim", PL_O(lt_dir), BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    {"%d", 1, "w", PL_O(tx_w), BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    {"%d", 1, "n", PL_O(tx_n), BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    {"%s", TXT_NAME_SIZE, "file", PL_O(tx_file), BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    {"", 0, (char *)0, 0, BU_STRUCTPARSE_FUNC_NULL, NULL, NULL }
};


static union tree *do_region_end1(struct db_tree_state *tsp, const struct db_full_path *pathp, union tree *curtree, void *client_data);
static union tree *do_region_end2(struct db_tree_state *tsp, const struct db_full_path *pathp, union tree *curtree, void *client_data);
static union tree *nmg_region_end(struct db_tree_state *tsp, const struct db_full_path *pathp, union tree *curtree, void *client_data);


struct vrml_write_options
{
    int bot_dump;
    int eval_all;
};


struct conversion_state {
    const struct gcv_opts *gcv_options;
    const struct vrml_write_options *vrml_write_options;

    FILE *fp_out;
    struct db_i *dbip;

    int nmg_debug;
    int regions_tried;
    int regions_converted;
    int bomb_cnt;
};


static void
clean_pmp(struct plate_mode *pmp)
{
    int i;

    for (i = 0; i < pmp->num_bots; i++) {
	bu_free(pmp->bots[i]->faces, "pmp->bots[i]->faces");
	bu_free(pmp->bots[i]->vertices, "pmp->bots[i]->vertices");
	if ((pmp->bots[i]->bot_flags & RT_BOT_PLATE) ||
	    (pmp->bots[i]->bot_flags & RT_BOT_PLATE_NOCOS)) {
	    bu_free(pmp->bots[i]->thickness, "pmp->bots[i]->thickness");
	}
	if (pmp->bots[i]->bot_flags & RT_BOT_HAS_SURFACE_NORMALS) {
	    bu_free(pmp->bots[i]->normals, "pmp->bots[i]->normals");
	}
	if ((pmp->bots[i]->bot_flags & RT_BOT_PLATE) ||
	    (pmp->bots[i]->bot_flags & RT_BOT_PLATE_NOCOS)) {
	    bu_bomb("about to free pmp->bots[i]->face_mode\n");
	    bu_bitv_free(pmp->bots[i]->face_mode);
	}
	bu_free(pmp->bots[i], "bots");
	pmp->bots[i] = (struct rt_bot_internal *)NULL;
    }

    pmp->num_bots = 0;
    pmp->num_nonbots = 0;
}


/* duplicate bot */
static struct rt_bot_internal *
dup_bot(struct rt_bot_internal *bot_in)
{
    struct rt_bot_internal *bot;
    size_t i;

    RT_BOT_CK_MAGIC(bot_in);

    BU_ALLOC(bot, struct rt_bot_internal);

    bot->magic = bot_in->magic;
    bot->mode = bot_in->mode;
    bot->orientation = bot_in->orientation;
    bot->bot_flags = bot_in->bot_flags;
    bot->num_vertices = bot_in->num_vertices;
    bot->num_faces = bot_in->num_faces;
    bot->num_normals = bot_in->num_normals;
    bot->num_face_normals = bot_in->num_face_normals;

    bot->faces = (int *)bu_calloc(bot_in->num_faces * 3, sizeof(int), "bot faces");
    for (i = 0; i < bot_in->num_faces * 3; i++) {
	bot->faces[i] = bot_in->faces[i];
    }

    bot->vertices = (fastf_t *)bu_calloc(bot_in->num_vertices * 3, sizeof(fastf_t), "bot verts");
    for (i = 0; i < bot_in->num_vertices * 3; i++) {
	bot->vertices[i] = bot_in->vertices[i];
    }

    if ((bot_in->bot_flags & RT_BOT_PLATE) || (bot_in->bot_flags & RT_BOT_PLATE_NOCOS)) {
	if (bot_in->thickness) {
	    bot->thickness = (fastf_t *)bu_calloc(bot_in->num_faces, sizeof(fastf_t), "bot thickness");
	    for (i = 0; i < bot_in->num_faces; i++) {
		bot->thickness[i] = bot_in->thickness[i];
	    }
	} else {
	    bu_bomb("dup_bot(): flag should not say plate but thickness is null\n");
	}
    }

    if (bot_in->face_mode) {
	bot->face_mode = bu_bitv_dup(bot_in->face_mode);
    }

    if (bot_in->bot_flags & RT_BOT_HAS_SURFACE_NORMALS) {
	bot->num_normals = bot_in->num_normals;
	bot->normals = (fastf_t *)bu_calloc(bot_in->num_normals * 3, sizeof(fastf_t), "BOT normals");
	bot->face_normals = (int *)bu_calloc(bot_in->num_faces * 3, sizeof(int), "BOT face normals");
	memcpy(bot->face_normals, bot_in->face_normals, bot_in->num_faces * 3 * sizeof(int));
    }

    return bot;
}


struct region_end_data
{
    struct conversion_state *pstate;
    struct plate_mode *pmp;
};


/* return 0 when object is NOT a light or an error occurred. regions
 * are skipped when this function returns 0.
 */
static int
select_lights(struct db_tree_state *UNUSED(tsp), const struct db_full_path *pathp, const struct rt_comb_internal *UNUSED(combp), void *client_data)
{
    const struct region_end_data * const data = (struct region_end_data *)client_data;
    struct directory *dp;
    struct rt_db_internal intern;
    struct rt_comb_internal *comb;
    int id;

    RT_CK_FULL_PATH(pathp);
    dp = DB_FULL_PATH_CUR_DIR(pathp);

    if (!(dp->d_flags & RT_DIR_COMB)) {
	/* object is not a combination therefore can not be a light */
	return 0;
    }

    id = rt_db_get_internal(&intern, dp, data->pstate->dbip, (matp_t)NULL, &rt_uniresource);
    if (id < 0) {
	/* error occurred retrieving object */
	bu_log("Warning: Can not load internal form of %s\n", dp->d_namep);
	return 0;
    }

    if (id != ID_COMBINATION) {
	/* object is not a combination therefore can not be a light */
	return 0;
    }

    comb = (struct rt_comb_internal *)intern.idb_ptr;
    RT_CK_COMB(comb);

    if (BU_STR_EQUAL(bu_vls_addr(&comb->shader), "light")) {
	rt_db_free_internal(&intern);
	return 1;
    } else {
	rt_db_free_internal(&intern);
	return 0;
    }
}


/* return 0 when IS a light or an error occurred. regions are skipped
 * when this function returns 0.
 */
static int
select_non_lights(struct db_tree_state *UNUSED(tsp), const struct db_full_path *pathp, const struct rt_comb_internal *UNUSED(combp), void *client_data)
{
    const struct region_end_data * const data = (struct region_end_data *)client_data;
    struct directory *dp;
    struct rt_db_internal intern;
    struct rt_comb_internal *comb;
    int id;

    RT_CK_FULL_PATH(pathp);
    dp = DB_FULL_PATH_CUR_DIR(pathp);

    id = rt_db_get_internal(&intern, dp, data->pstate->dbip, (matp_t)NULL, &rt_uniresource);
    if (id < 0) {
	/* error occurred retrieving object */
	bu_log("Warning: Can not load internal form of %s\n", dp->d_namep);
	return 0;
    }

    if ((dp->d_flags & RT_DIR_COMB) && (id == ID_COMBINATION)) {
	comb = (struct rt_comb_internal *)intern.idb_ptr;
	RT_CK_COMB(comb);
	if (BU_STR_EQUAL(bu_vls_addr(&comb->shader), "light")) {
	    rt_db_free_internal(&intern);
	    return 0;
	}
    }

    return 1;
}


/* CSG objects are tessellated and stored in the tree. BOTs are
 * processed but stored outside tree. This leaf-tess function is
 * used when we want CSG objects tessellated and evaluated but we
 * want to output BOTs without boolean evaluation.
 */
static union tree *
leaf_tess1(struct db_tree_state *tsp, const struct db_full_path *pathp, struct rt_db_internal *ip, void *client_data)
{
    const struct region_end_data * const data = (struct region_end_data *)client_data;

    struct rt_bot_internal *bot;

    if (ip->idb_type != ID_BOT) {
	data->pmp->num_nonbots++;
	return rt_booltree_leaf_tess(tsp, pathp, ip, client_data);
    }

    bot = (struct rt_bot_internal *)ip->idb_ptr;
    RT_BOT_CK_MAGIC(bot);

    if (data->pmp->array_size <= data->pmp->num_bots) {
	struct rt_bot_internal **bots_tmp;
	data->pmp->array_size += 5;
	bots_tmp = (struct rt_bot_internal **)bu_realloc((void *)data->pmp->bots,
							 data->pmp->array_size * sizeof(struct rt_bot_internal *), "data->pmp->bots");
	data->pmp->bots = bots_tmp;
    }

    /* walk tree will free the bot, so we need a copy */
    data->pmp->bots[data->pmp->num_bots] = dup_bot(bot);
    data->pmp->num_bots++;

    return (union tree *)NULL;
}


/* CSG objects are skipped. BOTs are processed but stored outside
 * tree. This leaf-tess function is used when we want to output
 * BOTs directly to the VRML file without performing a boolean
 * evaluation.
 */
static union tree *
leaf_tess2(struct db_tree_state *UNUSED(tsp), const struct db_full_path *UNUSED(pathp), struct rt_db_internal *ip, void *client_data)
{
    const struct region_end_data * const data = (struct region_end_data *)client_data;
    struct rt_bot_internal *bot;

    if (ip->idb_type != ID_BOT) {
	return (union tree *)NULL;
    }

    bot = (struct rt_bot_internal *)ip->idb_ptr;
    RT_BOT_CK_MAGIC(bot);

    if (data->pmp->array_size <= data->pmp->num_bots) {
	struct rt_bot_internal **bots_tmp;
	data->pmp->array_size += 5;
	bots_tmp = (struct rt_bot_internal **)bu_realloc((void *)data->pmp->bots,
							 data->pmp->array_size * sizeof(struct rt_bot_internal *), "data->pmp->bots");
	data->pmp->bots = bots_tmp;
    }

    /* walk tree will free the bot, so we need a copy */
    data->pmp->bots[data->pmp->num_bots] = dup_bot(bot);
    data->pmp->num_bots++;

    return (union tree *)NULL;
}


/* converts a geometry path to a vrml-compliant id.  a buffer is
 * allocated for use, it's the responsibility of the caller to free
 * it.
 *
 * fortunately '/' is valid, so the paths should convert mostly
 * untouched.  it is probably technically possible to name something
 * in mged such that two conversions will result in the same name, but
 * it should be an extremely rare situation.
 */
static void path_2_vrml_id(struct bu_vls *id, const char *path) {
    static int counter = 0;
    unsigned int i;
    char c;

    /* poof go the previous contents just in case */
    bu_vls_trunc(id, 0);

    if (path == NULL) {
	bu_vls_printf(id, "NO_PATH_%d", counter++);
	return;
    }

    /* disallow any character from the
     * ISO-IEC-14772-IS-VRML97WithAmendment1 spec that's not
     * allowed for the first char.
     */
    c = *path;
    switch (c) {
	/* numbers */
	case 0x30:
	    bu_vls_strcat(id, "_ZERO");
	    break;
	case 0x31:
	    bu_vls_strcat(id, "_ONE");
	    break;
	case 0x32:
	    bu_vls_strcat(id, "_TWO");
	    break;
	case 0x33:
	    bu_vls_strcat(id, "_THREE");
	    break;
	case 0x34:
	    bu_vls_strcat(id, "_FOUR");
	    break;
	case 0x35:
	    bu_vls_strcat(id, "_FIVE");
	    break;
	case 0x36:
	    bu_vls_strcat(id, "_SIX");
	    break;
	case 0x37:
	    bu_vls_strcat(id, "_SEVEN");
	    break;
	case 0x38:
	    bu_vls_strcat(id, "_EIGHT");
	    break;
	case 0x39:
	    bu_vls_strcat(id, "_NINE");
	    break;
	case 0x0:
	case 0x1:
	case 0x2:
	case 0x3:
	case 0x4:
	case 0x5:
	case 0x6:
	case 0x7:
	case 0x8:
	case 0x9:
	case 0x10:
	case 0x11:
	case 0x12:
	case 0x13:
	case 0x14:
	case 0x15:
	case 0x16:
	case 0x17:
	case 0x18:
	case 0x19:
	case 0x20:
	    /* control codes */
	    bu_vls_strcat(id, "_CTRL_");
	    break;
	case 0x22:
	    /* " */
	    bu_vls_strcat(id, "_QUOT_");
	    break;
	case 0x23:
	    /* # */
	    bu_vls_strcat(id, "_NUM_");
	    break;
	case 0x27:
	    /* ' */
	    bu_vls_strcat(id, "_APOS_");
	    break;
	case 0x2b:
	    /* + */
	    bu_vls_strcat(id, "_PLUS_");
	    break;
	case 0x2c:
	    /*, */
	    bu_vls_strcat(id, "_COMMA_");
	    break;
	case 0x2d:
	    /* - */
	    bu_vls_strcat(id, "_MINUS_");
	    break;
	case 0x2e:
	    /* . */
	    bu_vls_strcat(id, "_DOT_");
	    break;
	case 0x5b:
	    /* [ */
	    bu_vls_strcat(id, "_LBRK_");
	    break;
	case 0x5c:
	    /* \ */
	    bu_vls_strcat(id, "_BACK_");
	    break;
	case 0x5d:
	    /* ] */
	    bu_vls_strcat(id, "_RBRK_");
	    break;
	case 0x7b:
	    /* { */
	    bu_vls_strcat(id, "_LBRC_");
	    break;
	case 0x7d:
	    /* } */
	    bu_vls_strcat(id, "_RBRC_");
	    break;
	case 0x7f:
	    /* DEL */
	    bu_vls_strcat(id, "_DEL_");
	    break;
	default:
	    bu_vls_putc(id, c);
	    break;
    }

    /* convert the invalid path characters to something valid */
    for (i = 1; i < strlen(path); i++) {
	c = *(path+i);

	/* disallow any character from the
	 * ISO-IEC-14772-IS-VRML97WithAmendment1 spec that's not
	 * allowed for subsequent characters.  only difference is that
	 * #'s and numbers are allowed.
	 */
	switch (c) {
	    case 0x0:
	    case 0x1:
	    case 0x2:
	    case 0x3:
	    case 0x4:
	    case 0x5:
	    case 0x6:
	    case 0x7:
	    case 0x8:
	    case 0x9:
	    case 0x10:
	    case 0x11:
	    case 0x12:
	    case 0x13:
	    case 0x14:
	    case 0x15:
	    case 0x16:
	    case 0x17:
	    case 0x18:
	    case 0x19:
	    case 0x20:
		/* control codes */
		bu_vls_strcat(id, "_CTRL_");
		break;
	    case 0x22:
		/* " */
		bu_vls_strcat(id, "_QUOT_");
		break;
	    case 0x27:
		/* ' */
		bu_vls_strcat(id, "_APOS_");
		break;
	    case 0x2b:
		/* + */
		bu_vls_strcat(id, "_PLUS_");
		break;
	    case 0x2c:
		/*, */
		bu_vls_strcat(id, "_COMMA_");
		break;
	    case 0x2d:
		/* - */
		bu_vls_strcat(id, "_MINUS_");
		break;
	    case 0x2e:
		/* . */
		bu_vls_strcat(id, "_DOT_");
		break;
	    case 0x5b:
		/* [ */
		bu_vls_strcat(id, "_LBRK_");
		break;
	    case 0x5c:
		/* \ */
		bu_vls_strcat(id, "_BACK_");
		break;
	    case 0x5d:
		/* ] */
		bu_vls_strcat(id, "_RBRK_");
		break;
	    case 0x7b:
		/* { */
		bu_vls_strcat(id, "_LBRC_");
		break;
	    case 0x7d:
		/* } */
		bu_vls_strcat(id, "_RBRC_");
		break;
	    case 0x7f:
		/* DEL */
		bu_vls_strcat(id, "_DEL_");
		break;
	    default:
		bu_vls_putc(id, c);
		break;
	}  /* switch c */
    }  /* loop over chars */
}


static void
nmg_2_vrml(const struct conversion_state *pstate, struct db_tree_state *tsp, const struct db_full_path *pathp, struct model *m)
{
    struct mater_info *mater = &tsp->ts_mater;
    const struct bn_tol *tol2 = tsp->ts_tol;
    struct nmgregion *reg;
    struct bu_ptbl verts;
    struct vrml_mat mat;
    struct bu_vls vls = BU_VLS_INIT_ZERO;
    char *tok;
    size_t i;
    int first = 1;
    int is_light = 0;
    point_t ave_pt = VINIT_ZERO;
    struct bu_vls shape_name = BU_VLS_INIT_ZERO;
    /* There may be a better way to capture the region_id, than
     * getting the rt_comb_internal structure, (and may be a better
     * way to capture the rt_comb_internal struct), but for now I just
     * copied the method used in select_lights/select_non_lights above,
     * could have used a global variable but I noticed none other were
     * used, so I didn't want to be the first
     */
    struct directory *dp;
    struct rt_db_internal intern;
    struct rt_comb_internal *comb;
    int id;

    /* static due to libbu exception handling */
    static float r, g, b;

    NMG_CK_MODEL(m);

    RT_CK_FULL_PATH(pathp);
    dp = DB_FULL_PATH_CUR_DIR(pathp);

    if (!(dp->d_flags & RT_DIR_COMB)) {
	return;
    }

    id = rt_db_get_internal(&intern, dp, pstate->dbip, (matp_t)NULL, &rt_uniresource);
    if (id < 0) {
	bu_log("Cannot internal form of %s\n", dp->d_namep);
	return;
    }

    if (id != ID_COMBINATION) {
	bu_log("Directory/database mismatch!\n\t is '%s' a combination or not?\n",
	       dp->d_namep);
	return;
    }

    comb = (struct rt_comb_internal *)intern.idb_ptr;
    RT_CK_COMB(comb);

    if (mater->ma_color_valid) {
	r = mater->ma_color[0];
	g = mater->ma_color[1];
	b = mater->ma_color[2];
    } else {
	r = g = b = 0.5;
    }

    if (mater->ma_shader) {
	tok = strtok(mater->ma_shader, " \t");
	bu_strlcpy(mat.shader, tok, TXT_NAME_SIZE);
    } else {
	mat.shader[0] = '\0';
    }

    mat.shininess = -1;
    mat.transparency = -1.0;
    mat.lt_fraction = -1.0;
    VSETALL(mat.lt_dir, 0.0);
    mat.lt_angle = -1.0;
    mat.tx_file[0] = '\0';
    mat.tx_w = -1;
    mat.tx_n = -1;
    bu_vls_strcpy(&vls, &mater->ma_shader[strlen(mat.shader)]);
    (void)bu_struct_parse(&vls, vrml_mat_parse, (char *)&mat, NULL);

    if (bu_strncmp("light", mat.shader, 5) == 0) {
	/* this is a light source */
	is_light = 1;
    } else {
	char *full_path = db_path_to_string(pathp);
	path_2_vrml_id(&shape_name, full_path);
	fprintf(pstate->fp_out, "\t\tDEF %s Shape {\n", bu_vls_addr(&shape_name));

	fprintf(pstate->fp_out, "\t\t\t# Component_ID: %ld   %s\n", comb->region_id, full_path);
	bu_free(full_path, "db_path_to_string");
	fprintf(pstate->fp_out, "\t\t\tappearance Appearance {\n");

	if (bu_strncmp("plastic", mat.shader, 7) == 0) {
	    if (mat.shininess < 0) {
		mat.shininess = 10;
	    }
	    if (mat.transparency < SMALL_FASTF) {
		mat.transparency = 0.0;
	    }

	    fprintf(pstate->fp_out, "\t\t\t\tmaterial Material {\n");
	    fprintf(pstate->fp_out, "\t\t\t\t\tdiffuseColor %g %g %g \n", r, g, b);
	    fprintf(pstate->fp_out, "\t\t\t\t\tshininess %g\n", 1.0-exp(-(double)mat.shininess/20.0));
	    if (mat.transparency > SMALL_FASTF) {
		fprintf(pstate->fp_out, "\t\t\t\t\ttransparency %g\n", mat.transparency);
	    }
	    fprintf(pstate->fp_out, "\t\t\t\t\tspecularColor %g %g %g \n\t\t\t\t}\n", 1.0, 1.0, 1.0);
	} else if (bu_strncmp("glass", mat.shader, 5) == 0) {
	    if (mat.shininess < 0) {
		mat.shininess = 4;
	    }
	    if (mat.transparency < SMALL_FASTF) {
		mat.transparency = 0.8;
	    }

	    fprintf(pstate->fp_out, "\t\t\t\tmaterial Material {\n");
	    fprintf(pstate->fp_out, "\t\t\t\t\tdiffuseColor %g %g %g \n", r, g, b);
	    fprintf(pstate->fp_out, "\t\t\t\t\tshininess %g\n", 1.0-exp(-(double)mat.shininess/20.0));
	    if (mat.transparency > SMALL_FASTF) {
		fprintf(pstate->fp_out, "\t\t\t\t\ttransparency %g\n", mat.transparency);
	    }
	    fprintf(pstate->fp_out, "\t\t\t\t\tspecularColor %g %g %g \n\t\t\t\t}\n", 1.0, 1.0, 1.0);
	} else if (bu_strncmp("texture", mat.shader, 7) == 0) {
	    if (mat.tx_w < 0) {
		mat.tx_w = 512;
	    }
	    if (mat.tx_n < 0) {
		mat.tx_n = 512;
	    }
	    if (strlen(mat.tx_file)) {
		int tex_fd;
		unsigned char tex_buf[TXT_BUF_LEN * 3];

		if ((tex_fd = open(mat.tx_file, O_RDONLY | O_BINARY)) == (-1)) {
		    bu_log("Cannot open texture file (%s)\n", mat.tx_file);
		    perror("g-vrml: ");
		} else {
		    size_t tex_len;
		    size_t bytes_read = 0;
		    size_t bytes_to_go = 0;

		    /* Johns note - need to check (test) the texture stuff */
		    fprintf(pstate->fp_out, "\t\t\t\ttextureTransform TextureTransform {\n");
		    fprintf(pstate->fp_out, "\t\t\t\t\tscale 1.33333 1.33333\n\t\t\t\t}\n");
		    fprintf(pstate->fp_out, "\t\t\t\ttexture PixelTexture {\n");
		    fprintf(pstate->fp_out, "\t\t\t\t\trepeatS TRUE\n");
		    fprintf(pstate->fp_out, "\t\t\t\t\trepeatT TRUE\n");
		    fprintf(pstate->fp_out, "\t\t\t\t\timage %d %d %d\n", mat.tx_w, mat.tx_n, 3);
		    tex_len = mat.tx_w*mat.tx_n * 3;
		    while (bytes_read < tex_len) {
			size_t nbytes;
			long readval;

			bytes_to_go = tex_len - bytes_read;
			V_MIN(bytes_to_go, TXT_BUF_LEN * 3);

			nbytes = 0;
			while (nbytes < bytes_to_go) {
			    readval = read(tex_fd, &tex_buf[nbytes], bytes_to_go-nbytes);
			    if (readval < 0) {
				perror("READ ERROR");
				break;
			    } else {
				nbytes += readval;
			    }
			}
			bytes_read += nbytes;
			for (i = 0; i < nbytes; i += 3) {
			    fprintf(pstate->fp_out, "\t\t\t0x%02x%02x%02x\n",
				    tex_buf[i], tex_buf[i+1], tex_buf[i+2]);
			}
		    }
		    fprintf(pstate->fp_out, "\t\t\t\t}\n");

		    close(tex_fd);
		}
	    }
	} else if (mater->ma_color_valid) {
	    /* no shader specified, but a color is assigned */
	    fprintf(pstate->fp_out, "\t\t\t\tmaterial Material {\n");
	    fprintf(pstate->fp_out, "\t\t\t\t\tdiffuseColor %g %g %g }\n", r, g, b);
	} else {
	    /* If no color was defined set the colors according to the thousands groups */
	    int thou = comb->region_id / 1000;
	    thou == 0 ? fprintf(pstate->fp_out, "\t\t\tmaterial USE Material_999\n")
		: thou == 1 ? fprintf(pstate->fp_out, "\t\t\tmaterial USE Material_1999\n")
		: thou == 2 ? fprintf(pstate->fp_out, "\t\t\tmaterial USE Material_2999\n")
		: thou == 3 ? fprintf(pstate->fp_out, "\t\t\tmaterial USE Material_3999\n")
		: thou == 4 ? fprintf(pstate->fp_out, "\t\t\tmaterial USE Material_4999\n")
		: thou == 5 ? fprintf(pstate->fp_out, "\t\t\tmaterial USE Material_5999\n")
		: thou == 6 ? fprintf(pstate->fp_out, "\t\t\tmaterial USE Material_6999\n")
		: thou == 7 ? fprintf(pstate->fp_out, "\t\t\tmaterial USE Material_7999\n")
		: thou == 8 ? fprintf(pstate->fp_out, "\t\t\tmaterial USE Material_8999\n")
		: fprintf(pstate->fp_out, "\t\t\tmaterial USE Material_9999\n");
	}
    }

    if (!is_light) {
	nmg_triangulate_model(m, &RTG.rtg_vlfree, tol2);
	fprintf(pstate->fp_out, "\t\t\t}\n");
	fprintf(pstate->fp_out, "\t\t\tgeometry IndexedFaceSet {\n");
	fprintf(pstate->fp_out, "\t\t\t\tcoord Coordinate {\n");
    }

    /* get list of vertices */
    nmg_vertex_tabulate(&verts, &m->magic, &RTG.rtg_vlfree);
    if (!is_light) {
	fprintf(pstate->fp_out, "\t\t\t\t\tpoint [");
    } else {
	VSETALL(ave_pt, 0.0);
    }

    for (i = 0; i < BU_PTBL_LEN(&verts); i++) {
	struct vertex *v;
	struct vertex_g *vg;
	point_t pt_meters;

	v = (struct vertex *)BU_PTBL_GET(&verts, i);
	NMG_CK_VERTEX(v);
	vg = v->vg_p;
	NMG_CK_VERTEX_G(vg);

	/* convert to desired units */
	VSCALE(pt_meters, vg->coord, pstate->gcv_options->scale_factor);

	if (is_light) {
	    VADD2(ave_pt, ave_pt, pt_meters);
	}

	if (first) {
	    if (!is_light) {
		fprintf(pstate->fp_out, " %10.10e %10.10e %10.10e, # point %lu\n", V3ARGS(pt_meters), (unsigned long)i);
	    }
	    first = 0;
	} else if (!is_light) {
	    fprintf(pstate->fp_out, "\t\t\t\t\t%10.10e %10.10e %10.10e, # point %lu\n", V3ARGS(pt_meters), (unsigned long)i);
	}
    }

    if (!is_light) {
	fprintf(pstate->fp_out, "\t\t\t\t\t]\n\t\t\t\t}\n");
    } else {
	fastf_t one_over_count;
	one_over_count = 1.0/(fastf_t)BU_PTBL_LEN(&verts);
	VSCALE(ave_pt, ave_pt, one_over_count);
    }

    first = 1;
    if (!is_light) {
	fprintf(pstate->fp_out, "\t\t\t\tcoordIndex [\n");
	for (BU_LIST_FOR(reg, nmgregion, &m->r_hd)) {
	    struct shell *s;

	    NMG_CK_REGION(reg);
	    for (BU_LIST_FOR(s, shell, &reg->s_hd)) {
		struct faceuse *fu;

		NMG_CK_SHELL(s);
		for (BU_LIST_FOR(fu, faceuse, &s->fu_hd)) {
		    struct loopuse *lu;

		    NMG_CK_FACEUSE(fu);

		    if (fu->orientation != OT_SAME) {
			continue;
		    }

		    for (BU_LIST_FOR(lu, loopuse, &fu->lu_hd)) {
			struct edgeuse *eu;

			NMG_CK_LOOPUSE(lu);
			if (BU_LIST_FIRST_MAGIC(&lu->down_hd) != NMG_EDGEUSE_MAGIC) {
			    continue;
			}

			if (!first) {
			    fprintf(pstate->fp_out, ", \n");
			} else {
			    first = 0;
			}

			fprintf(pstate->fp_out, "\t\t\t\t\t");
			for (BU_LIST_FOR(eu, edgeuse, &lu->down_hd)) {
			    struct vertex *v;

			    NMG_CK_EDGEUSE(eu);
			    v = eu->vu_p->v_p;
			    NMG_CK_VERTEX(v);
			    fprintf(pstate->fp_out, " %jd, ", bu_ptbl_locate(&verts, (long *)v));
			}
			fprintf(pstate->fp_out, "-1");
		    }
		}
	    }
	}
	fprintf(pstate->fp_out, "\n\t\t\t\t]\n\t\t\t\tnormalPerVertex FALSE\n");
	fprintf(pstate->fp_out, "\t\t\t\tconvex FALSE\n");
	fprintf(pstate->fp_out, "\t\t\t\tcreaseAngle 0.5\n");
	fprintf(pstate->fp_out, "\t\t\t}\n\t\t}\n");
    } else {
	mat.lt_fraction = 0.0;
	mat.lt_angle = 180.0;
	VSETALL(mat.lt_dir, 0.0);

	if (!ZERO(mat.lt_dir[X]) || !ZERO(mat.lt_dir[Y]) || !ZERO(mat.lt_dir[Z])) {
	    fprintf(pstate->fp_out, "\t\tSpotLight {\n");
	    fprintf(pstate->fp_out, "\t\t\ton \tTRUE\n");
	    if (mat.lt_fraction > 0.0) {
		fprintf(pstate->fp_out, "\t\t\tintensity \t%g\n", mat.lt_fraction);
	    }
	    fprintf(pstate->fp_out, "\t\t\tcolor \t%g %g %g\n", r, g, b);
	    fprintf(pstate->fp_out, "\t\t\tlocation \t%g %g %g\n", V3ARGS(ave_pt));
	    fprintf(pstate->fp_out, "\t\t\tdirection \t%g %g %g\n", V3ARGS(mat.lt_dir));
	    fprintf(pstate->fp_out, "\t\t\tcutOffAngle \t%g }\n", mat.lt_angle);
	} else {
	    fprintf(pstate->fp_out, "\t\tPointLight {\n\t\t\ton TRUE\n\t\t\tintensity 1\n\t\t\tcolor %g %g %g\n\t\t\tlocation %g %g %g\n\t\t}\n",
		    r, g, b, V3ARGS(ave_pt));
	}
    }

    bu_vls_free(&vls);
    bu_vls_free(&shape_name);
}


static void
bot2vrml(const struct conversion_state *pstate, struct plate_mode *pmp, const struct db_full_path *pathp, int region_id)
{
    struct bu_vls shape_name = BU_VLS_INIT_ZERO;
    char *path_str;
    int appearance;
    struct rt_bot_internal *bot;
    int bot_num;
    size_t i;
    size_t vert_count=0;

    path_str = db_path_to_string(pathp);

    path_2_vrml_id(&shape_name, path_str);

    fprintf(pstate->fp_out, "\t\tDEF %s Shape {\n\t\t\t# Component_ID: %d   %s\n",
	    bu_vls_addr(&shape_name), region_id, path_str);

    bu_free(path_str, "result of db_path_to_string");
    bu_vls_free(&shape_name);

    appearance = region_id / 1000;
    appearance = appearance * 1000 + 999;
    fprintf(pstate->fp_out, "\t\t\tappearance Appearance {\n\t\t\tmaterial USE Material_%d\n\t\t\t}\n", appearance);
    fprintf(pstate->fp_out, "\t\t\tgeometry IndexedFaceSet {\n\t\t\t\tcoord Coordinate {\n\t\t\t\tpoint [\n");

    for (bot_num = 0; bot_num < pmp->num_bots; bot_num++) {
	bot = pmp->bots[bot_num];
	RT_BOT_CK_MAGIC(bot);
	for (i = 0; i < bot->num_vertices; i++) {
	    point_t pt;

	    VSCALE(pt, &bot->vertices[i*3], pstate->gcv_options->scale_factor);
	    fprintf(pstate->fp_out, "\t\t\t\t\t%10.10e %10.10e %10.10e, # point %lu\n", V3ARGS(pt), (long unsigned int)vert_count);
	    vert_count++;
	}
    }
    fprintf(pstate->fp_out, "\t\t\t\t\t]\n\t\t\t\t}\n\t\t\t\tcoordIndex [\n");
    vert_count = 0;
    for (bot_num = 0; bot_num < pmp->num_bots; bot_num++) {
	bot = pmp->bots[bot_num];
	RT_BOT_CK_MAGIC(bot);
	for (i = 0; i < bot->num_faces; i++) {
	    fprintf(pstate->fp_out, "\t\t\t\t\t%lu, %lu, %lu, -1, \n",
		    (long unsigned int)vert_count+bot->faces[i*3],
		    (long unsigned int)vert_count+bot->faces[i*3+1],
		    (long unsigned int)vert_count+bot->faces[i*3+2]);
	}
	vert_count += bot->num_vertices;
    }
    fprintf(pstate->fp_out, "\t\t\t\t]\n\t\t\t\tnormalPerVertex FALSE\n");
    fprintf(pstate->fp_out, "\t\t\t\tconvex TRUE\n");
    fprintf(pstate->fp_out, "\t\t\t\tcreaseAngle 0.5\n");
    fprintf(pstate->fp_out, "\t\t\t\tsolid FALSE\n");
    fprintf(pstate->fp_out, "\t\t\t}\n\t\t}\n");
}


/*
 * Called from db_walk_tree().
 * This routine must be prepared to run in parallel.
 */
static union tree *
do_region_end1(struct db_tree_state *tsp, const struct db_full_path *pathp, union tree *curtree, void *client_data)
{
    const struct region_end_data * const data = (struct region_end_data *)client_data;

    if (data->pmp->num_bots > 0 && data->pmp->num_nonbots > 0) {
	bu_log("data->pmp->num_bots = %d data->pmp->num_nonbots = %d\n", data->pmp->num_bots, data->pmp->num_nonbots);
	bu_bomb("region was both bot and non-bot objects\n");
    }
    if (RT_G_DEBUG&RT_DEBUG_TREEWALK || data->pstate->gcv_options->verbosity_level) {
	bu_log("\nConverted %d%% so far (%d of %d)\n",
	       data->pstate->regions_tried > 0 ? (data->pstate->regions_converted * 100) / data->pstate->regions_tried : 0,
	       data->pstate->regions_converted, data->pstate->regions_tried);
    }

    if (data->pmp->num_bots > 0) {
	data->pstate->regions_tried++;

	if (data->pstate->gcv_options->verbosity_level) {
	    char *name = db_path_to_string(pathp);
	    bu_log("Attempting %s\n", name);
	    bu_free(name, "db_path_to_string");
	}

	bot2vrml(data->pstate, data->pmp, pathp, tsp->ts_regionid);
	clean_pmp(data->pmp);
	data->pstate->regions_converted++;
	return (union tree *)NULL;
    } else {
	return nmg_region_end(tsp, pathp, curtree, client_data);
    }
}


/*
 * Called from db_walk_tree().
 * This routine must be prepared to run in parallel.
 *
 * Only send bots from structure outside tree to vrml file.
 */
static union tree *
do_region_end2(struct db_tree_state *tsp, const struct db_full_path *pathp, union tree *UNUSED(curtree), void *client_data)
{
    const struct region_end_data * const data = (struct region_end_data *)client_data;

    if ((data->pmp->num_bots > 0) && (RT_G_DEBUG&RT_DEBUG_TREEWALK || data->pstate->gcv_options->verbosity_level)) {
	bu_log("\nConverted %d%% so far (%d of %d)\n",
	       data->pstate->regions_tried > 0 ? (data->pstate->regions_converted * 100) / data->pstate->regions_tried : 0,
	       data->pstate->regions_converted, data->pstate->regions_tried);
    }

    if (data->pmp->num_bots > 0) {
	data->pstate->regions_tried++;

	if (data->pstate->gcv_options->verbosity_level) {
	    char *name = db_path_to_string(pathp);
	    bu_log("Attempting %s\n", name);
	    bu_free(name, "db_path_to_string");
	}

	bot2vrml(data->pstate, data->pmp, pathp, tsp->ts_regionid);
	clean_pmp(data->pmp);
	data->pstate->regions_converted++;
    }

    return (union tree *)NULL;
}


static union tree *
vrml_write_process_boolean(struct conversion_state *pstate, union tree *curtree, struct db_tree_state *tsp, const struct db_full_path *pathp)
{
    static union tree *ret_tree = TREE_NULL;

    /* Begin bomb protection */
    if (!BU_SETJUMP) {
	/* try */
	ret_tree = nmg_booltree_evaluate(curtree, &RTG.rtg_vlfree, tsp->ts_tol, &rt_uniresource);
    } else {
	/* catch */
	char *name = db_path_to_string(pathp);

	bu_log("Conversion of %s FAILED due to error!!!\n", name);

	pstate->bomb_cnt++;

	/* Sometimes the NMG library adds debugging bits when
	 * it detects an internal error, before before bombing out.
	 */
	nmg_debug = pstate->nmg_debug; /* restore mode */

	/* Release any intersector 2d tables */
	nmg_isect2d_final_cleanup();

	bu_free(name, "db_path_to_string");
    } BU_UNSETJUMP; /* Relinquish the protection */

    return ret_tree;
}


static union tree *
nmg_region_end(struct db_tree_state *tsp, const struct db_full_path *pathp, union tree *curtree, void *client_data)
{
    const struct region_end_data * const data = (struct region_end_data *)client_data;

    struct nmgregion *r;
    struct bu_list vhead;
    union tree *ret_tree;
    char *name;

    BG_CK_TESS_TOL(tsp->ts_ttol);
    BN_CK_TOL(tsp->ts_tol);
    NMG_CK_MODEL(*tsp->ts_m);

    BU_LIST_INIT(&vhead);

    if (RT_G_DEBUG&RT_DEBUG_TREEWALK || data->pstate->gcv_options->verbosity_level) {
	bu_log("\nConverted %d%% so far (%d of %d)\n",
	       data->pstate->regions_tried > 0 ? (data->pstate->regions_converted * 100) / data->pstate->regions_tried : 0,
	       data->pstate->regions_converted, data->pstate->regions_tried);
    }

    if (curtree->tr_op == OP_NOP) {
	return curtree;
    }

    name = db_path_to_string(pathp);

    if (data->pstate->gcv_options->verbosity_level)
	bu_log("Attempting %s\n", name);

    data->pstate->regions_tried++;
    ret_tree = vrml_write_process_boolean(data->pstate, curtree, tsp, pathp);

    if (ret_tree) {
	r = ret_tree->tr_d.td_r;
    } else {
	r = (struct nmgregion *)NULL;
    }

    if (r != (struct nmgregion *)NULL) {
	struct shell *s;
	int empty_region = 0;

	/* kill zero length edgeuse and cracks */
	s = BU_LIST_FIRST(shell, &r->s_hd);
	while (BU_LIST_NOT_HEAD(&s->l, &r->s_hd)) {
	    struct shell *next_s;
	    next_s = BU_LIST_PNEXT(shell, &s->l);
	    (void)nmg_keu_zl(s, tsp->ts_tol); /* kill zero length edgeuse */
	    if (nmg_kill_cracks(s)) {
		/* true when shell is empty */
		if (nmg_ks(s)) {
		    /* true when nmg region is empty */
		    empty_region = 1;
		    break;
		}
	    }
	    s = next_s;
	}

	if (!empty_region) {
	    /* Write the nmgregion to the output file */
	    nmg_2_vrml(data->pstate, tsp, pathp, r->m_p);
	    data->pstate->regions_converted++;
	} else {
	    bu_log("WARNING: Nothing left after Boolean evaluation of %s (due to cleanup)\n", name);
	}

    } else {
	bu_log("WARNING: Nothing left after Boolean evaluation of %s (due to error or null result)\n", name);
    }

    NMG_CK_MODEL(*tsp->ts_m);

    /* Dispose of original tree, so that all associated dynamic
     * memory is released now, not at the end of all regions.
     * A return of TREE_NULL from this routine signals an error,
     * so we need to cons up an OP_NOP node to return.
     */
    db_free_tree(curtree, &rt_uniresource); /* does a nmg_kr (i.e. kill nmg region) */
    bu_free(name, "db_path_to_string");

    BU_ALLOC(curtree, union tree);
    RT_TREE_INIT(curtree);
    curtree->tr_op = OP_NOP;
    return curtree;
}


static char *
vrml_write_make_units_str(double scale_factor)
{
    const char *bu_units = bu_units_string(scale_factor);

    if (bu_units)
	return bu_strdup(bu_units);
    else {
	struct bu_vls temp = BU_VLS_INIT_ZERO;
	bu_vls_printf(&temp, "%g units per mm", scale_factor);
	return bu_vls_strgrab(&temp);
    }
}


static void
vrml_write_create_opts(struct bu_opt_desc **options_desc, void **dest_options_data)
{
    struct vrml_write_options *options_data;

    BU_ALLOC(options_data, struct vrml_write_options);
    *dest_options_data = options_data;
    *options_desc = (struct bu_opt_desc *)bu_malloc(3 * sizeof(struct bu_opt_desc), "options_desc");

    BU_OPT((*options_desc)[0], NULL, "bot-dump", NULL,
	    NULL, &options_data->bot_dump,
	    "BoT dump");

    BU_OPT((*options_desc)[1], NULL, "evaluate-all", NULL,
	    NULL, &options_data->eval_all,
	    "evaluate all, CSG and BoTs");

    BU_OPT_NULL((*options_desc)[2]);
}


static void
vrml_write_free_opts(void *options_data)
{
    bu_free(options_data, "options_data");
}


static int
vrml_write(struct gcv_context *context, const struct gcv_opts *gcv_options, const void *options_data, const char *dest_path)
{
    struct conversion_state state;
    struct db_tree_state tree_state;
    struct model *the_model;
    struct plate_mode pm;
    size_t i;
    struct region_end_data region_end_data;

    memset(&state, 0, sizeof(state));

    state.gcv_options = gcv_options;
    state.vrml_write_options = (struct vrml_write_options *)options_data;
    state.dbip = context->dbip;
    state.nmg_debug = nmg_debug;

    region_end_data.pstate = &state;
    region_end_data.pmp = &pm;

    if (state.vrml_write_options->bot_dump && state.vrml_write_options->eval_all) {
	bu_log("Bot Dump and Evaluate All are mutually exclusive\n");
	return 0;
    }

    if (!(state.fp_out = fopen(dest_path, "wb"))) {
	perror("libgcv");
	bu_log("failed to open output file (%s)\n", dest_path);
	return 0;
    }

    tree_state = rt_initial_tree_state;
    tree_state.ts_tol = &gcv_options->calculational_tolerance;
    tree_state.ts_ttol = &gcv_options->tessellation_tolerance;
    tree_state.ts_m = &the_model;
    the_model = nmg_mm();

    /* NOTE: For visualization purposes, in the debug plot files */
    {
	/* WTF: This value is specific to the Bradley */
	nmg_eue_dist = 2.0;
    }

    BU_LIST_INIT(&RTG.rtg_vlfree);	/* for vlist macros */

    fprintf(state.fp_out, "#VRML V2.0 utf8\n");
    fprintf(state.fp_out, "#Units are %s\n", vrml_write_make_units_str(gcv_options->scale_factor));
    /* NOTE: We may want to inquire about bounding boxes for the
     * various groups and add Viewpoints nodes that point the camera
     * to the center and orient for Top, Side, etc. Views. We will add
     * some default Material Color definitions (for thousands groups)
     * before we start defining the geometry.
     */
    fprintf(state.fp_out, "Shape { appearance Appearance { material DEF Material_999 Material { diffuseColor 0.78 0.78 0.78 } } }\n");
    fprintf(state.fp_out, "Shape { appearance Appearance { material DEF Material_1999 Material { diffuseColor 0.88 0.29 0.29 } } }\n");
    fprintf(state.fp_out, "Shape { appearance Appearance { material DEF Material_2999 Material { diffuseColor 0.82 0.53 0.54 } } }\n");
    fprintf(state.fp_out, "Shape { appearance Appearance { material DEF Material_3999 Material { diffuseColor 0.39 0.89 0.00 } } }\n");
    fprintf(state.fp_out, "Shape { appearance Appearance { material DEF Material_4999 Material { diffuseColor 1.00 0.00 0.00 } } }\n");
    fprintf(state.fp_out, "Shape { appearance Appearance { material DEF Material_5999 Material { diffuseColor 0.82 0.00 0.82 } } }\n");
    fprintf(state.fp_out, "Shape { appearance Appearance { material DEF Material_6999 Material { diffuseColor 0.62 0.62 0.62 } } }\n");
    fprintf(state.fp_out, "Shape { appearance Appearance { material DEF Material_7999 Material { diffuseColor 0.49 0.49 0.49 } } }\n");
    fprintf(state.fp_out, "Shape { appearance Appearance { material DEF Material_8999 Material { diffuseColor 0.18 0.31 0.31 } } }\n");
    fprintf(state.fp_out, "Shape { appearance Appearance { material DEF Material_9999 Material { diffuseColor 0.00 0.41 0.82 } } }\n");

    /* I had hoped to create a separate sub-tree (using the Transform
     * node) for each group name argument however, it appears they are
     * all handled at the same time so I will only have one Transform
     * for the complete conversion. Later on switch nodes may be added
     * to turn on and off the groups (via ROUTE nodes).
     */
    fprintf(state.fp_out, "Transform {\n");
    fprintf(state.fp_out, "\tchildren [\n");

    pm.num_bots = 0;
    pm.num_nonbots = 0;

    if (!state.vrml_write_options->eval_all) {
	pm.array_size = 5;
	pm.bots = (struct rt_bot_internal **)bu_calloc(pm.array_size,
						       sizeof(struct rt_bot_internal *), "pm.bots");
    }

    if (state.vrml_write_options->eval_all) {
	(void)db_walk_tree(context->dbip, gcv_options->num_objects, (const char **)gcv_options->object_names,
			   1,		/* ncpu */
			   &tree_state,
			   0,
			   nmg_region_end,
			   rt_booltree_leaf_tess,
			   (void *)&region_end_data);	/* in librt/nmg_bool.c */
	goto out;
    }

    if (state.vrml_write_options->bot_dump) {
	(void)db_walk_tree(context->dbip, gcv_options->num_objects, (const char **)gcv_options->object_names,
			   1,		/* ncpu */
			   &tree_state,
			   0,
			   do_region_end2,
			   leaf_tess2,
			   (void *)&region_end_data);	/* in librt/nmg_bool.c */
	goto out;
    }

    for (i = 0; i < gcv_options->num_objects; i++) {
	struct directory *dp;

	dp = db_lookup(context->dbip, gcv_options->object_names[i], LOOKUP_QUIET);
	if (dp == RT_DIR_NULL) {
	    bu_log("Cannot find %s\n", gcv_options->object_names[i]);
	    continue;
	}

	/* light source must be a combination */
	if (!(dp->d_flags & RT_DIR_COMB)) {
	    continue;
	}

	fprintf(state.fp_out, "# Includes group %s\n", gcv_options->object_names[i]);

	/* walk trees selecting only light source regions */
	(void)db_walk_tree(context->dbip, 1, (const char **)(&gcv_options->object_names[i]),
			   1,			/* ncpu */
			   &tree_state,
			   select_lights,
			   do_region_end1,
			   leaf_tess1,
			   (void *)&region_end_data);	/* in librt/nmg_bool.c */
    }

    /* Walk indicated tree(s).  Each non-light-source region will be output separately */
    (void)db_walk_tree(context->dbip, gcv_options->num_objects, (const char **)gcv_options->object_names,
		       1,		/* ncpu */
		       &tree_state,
		       select_non_lights,
		       do_region_end1,
		       leaf_tess1,
		       (void *)&region_end_data);	/* in librt/nmg_bool.c */

    /* Release dynamic storage */
    nmg_km(the_model);

    if (!state.vrml_write_options->eval_all) {
	bu_free(pm.bots, "pm.bots");
    }

out:
    /* Now we need to close each group set */
    fprintf(state.fp_out, "\t]\n}\n");

    bu_log("\nTotal of %d regions converted of %d regions attempted.\n",
	   state.regions_converted, state.regions_tried);

    if (state.regions_converted != state.regions_tried) {
	bu_log("Of the %d which failed conversion, %d of these failed due to conversion error.\n",
	       state.regions_tried - state.regions_converted, state.bomb_cnt);
    }

    fclose(state.fp_out);
    bu_log("Done.\n");

    return 1;
}

static const struct gcv_filter gcv_conv_vrml_write = {
    "VRML Writer", GCV_FILTER_WRITE, BU_MIME_MODEL_VRML, NULL,
    vrml_write_create_opts, vrml_write_free_opts, vrml_write
};

extern const struct gcv_filter gcv_conv_vrml_read;
static const struct gcv_filter * const filters[] = {&gcv_conv_vrml_read, &gcv_conv_vrml_write, NULL};

const struct gcv_plugin gcv_plugin_info_s = { filters };

COMPILER_DLLEXPORT const struct gcv_plugin *
gcv_plugin_info(void){ return &gcv_plugin_info_s; }

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
