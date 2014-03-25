/*                       D B _ A N I M . C
 * BRL-CAD
 *
 * Copyright (c) 1987-2014 United States Government as represented by
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
/** @addtogroup dbio */
/** @{ */
/** @file librt/db_anim.c
 *
 * Routines to apply animation directives to geometry database.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "bio.h"

#include "vmath.h"

#include "raytrace.h"

int
db_add_anim(struct db_i *dbip, register struct animate *anp, int root)
{
    register struct animate **headp;
    struct directory *dp;

    /* Could validate an_type here */

    RT_CK_ANIMATE(anp);
    anp->an_forw = ANIM_NULL;
    if (root) {
	if (RT_G_DEBUG&DEBUG_ANIM)
	    bu_log("db_add_anim(%p) root\n", (void *)anp);

	if (!dbip)
	    bu_bomb("Unexpected NULL dbip encountered in db_add_anim\n");

	headp = &(dbip->dbi_anroot);
    } else {
	dp = DB_FULL_PATH_CUR_DIR(&anp->an_path);
	if (!dp)
	    return 1;
	if (RT_G_DEBUG&DEBUG_ANIM)
	    bu_log("db_add_anim(%p) arc %s\n", (void *)anp,
		   dp->d_namep);
	headp = &(dp->d_animate);
    }

    /* Append to list */
    while (*headp != ANIM_NULL) {
	RT_CK_ANIMATE(*headp);
	headp = &((*headp)->an_forw);
    }
    *headp = anp;
    return 0;			/* OK */
}


static char *db_anim_matrix_strings[] = {
    "(nope)",
    "ANM_RSTACK",
    "ANM_RARC",
    "ANM_LMUL",
    "ANM_RMUL",
    "ANM_RBOTH",
    "eh?"
};


int
db_do_anim(register struct animate *anp, mat_t stack, mat_t arc, struct mater_info *materp)
{
    mat_t temp;

    if (RT_G_DEBUG&DEBUG_ANIM)
	bu_log("db_do_anim(%p) ", (void *)anp);
    if (RT_G_DEBUG&DEBUG_ANIM && !materp) bu_log("(null materp) ");
    RT_CK_ANIMATE(anp);
    switch (anp->an_type) {
	case RT_AN_MATRIX:
	    if (RT_G_DEBUG&DEBUG_ANIM) {
		int op = anp->an_u.anu_m.anm_op;
		if (op < 0) op = 0;
		bu_log("matrix, op=%s (%d)\n",
		       db_anim_matrix_strings[op], op);
		if (RT_G_DEBUG&DEBUG_ANIM_FULL) {
		    bn_mat_print("on original arc", arc);
		    bn_mat_print("on original stack", stack);
		}
	    }
	    switch (anp->an_u.anu_m.anm_op) {
		case ANM_RSTACK:
		    MAT_COPY(stack, anp->an_u.anu_m.anm_mat);
		    break;
		case ANM_RARC:
		    MAT_COPY(arc, anp->an_u.anu_m.anm_mat);
		    break;
		case ANM_RBOTH:
		    MAT_COPY(stack, anp->an_u.anu_m.anm_mat);
		    MAT_IDN(arc);
		    break;
		case ANM_LMUL:
		    /* arc = DELTA * arc */
		    bn_mat_mul(temp, anp->an_u.anu_m.anm_mat, arc);
		    MAT_COPY(arc, temp);
		    break;
		case ANM_RMUL:
		    /* arc = arc * DELTA */
		    bn_mat_mul(temp, arc, anp->an_u.anu_m.anm_mat);
		    MAT_COPY(arc, temp);
		    break;
		default:
		    return -1;		/* BAD */
	    }
	    if (RT_G_DEBUG&DEBUG_ANIM_FULL) {
		bn_mat_print("arc result", arc);
		bn_mat_print("stack result", stack);
	    }
	    break;
	case RT_AN_MATERIAL:
	    if (RT_G_DEBUG&DEBUG_ANIM)
		bu_log("property\n");
	    /*
	     * if the caller does not care about property, a null
	     * mater pointer is given.
	     */
	    if (!materp) {
		char *sofar = db_path_to_string(&anp->an_path);
		bu_log("ERROR db_do_anim(%s) property animation below region, ignored\n", sofar);
		bu_free(sofar, "path string");
		break;
	    }
	    if (anp->an_u.anu_p.anp_op == RT_ANP_REPLACE) {
		if (materp->ma_shader) bu_free((genptr_t)materp->ma_shader, "ma_shader");
		materp->ma_shader = bu_vls_strdup(&anp->an_u.anu_p.anp_shader);
	    } else if (anp->an_u.anu_p.anp_op == RT_ANP_APPEND) {
		struct bu_vls str = BU_VLS_INIT_ZERO;

		bu_vls_strcpy(&str, materp->ma_shader);
		bu_vls_putc(&str, ' ');
		bu_vls_vlscat(&str, &anp->an_u.anu_p.anp_shader);
		if (materp->ma_shader) bu_free((genptr_t)materp->ma_shader, "ma_shader");
		materp->ma_shader = bu_vls_strgrab(&str);
		/* bu_vls_free(&str) is done by bu_vls_strgrab() */
	    } else
		bu_log("Unknown anp_op=%d\n", anp->an_u.anu_p.anp_op);
	    break;
	case RT_AN_COLOR:
	    if (RT_G_DEBUG&DEBUG_ANIM)
		bu_log("color\n");
	    /*
	     * if the caller does not care about property, a null
	     * mater pointer is given.
	     */
	    if (!materp) {
		char *sofar = db_path_to_string(&anp->an_path);
		bu_log("ERROR db_do_anim(%s) color animation below region, ignored\n", sofar);
		bu_free(sofar, "path string");
		break;
	    }
	    materp->ma_color_valid = 1;	/* XXX - really override? */
	    materp->ma_color[0] =
		(((float)anp->an_u.anu_c.anc_rgb[0])+0.5) / 255.0;
	    materp->ma_color[1] =
		(((float)anp->an_u.anu_c.anc_rgb[1])+0.5) / 255.0;
	    materp->ma_color[2] =
		(((float)anp->an_u.anu_c.anc_rgb[2])+0.5) / 255.0;
	    break;
	case RT_AN_TEMPERATURE:
	    if (RT_G_DEBUG&DEBUG_ANIM)
		bu_log("temperature = %g\n", anp->an_u.anu_t);
	    if (!materp) {
		char *sofar = db_path_to_string(&anp->an_path);
		bu_log("ERROR db_do_anim(%s) temperature animation below region, ignored\n", sofar);
		bu_free(sofar, "path string");
		break;
	    }
	    materp->ma_temperature = anp->an_u.anu_t;
	    break;
	default:
	    if (RT_G_DEBUG&DEBUG_ANIM)
		bu_log("unknown op\n");
	    /* Print something here? */
	    return -1;			/* BAD */
    }
    return 0;				/* OK */
}

void
db_free_1anim(struct animate *anp)
{
    RT_CK_ANIMATE(anp);

    switch (anp->an_type) {
	case RT_AN_MATERIAL:
	    bu_vls_free(&anp->an_u.anu_p.anp_shader);
	    break;
    }

    db_free_full_path(&anp->an_path);
    bu_free((char *)anp, "animate");
}

void
db_free_anim(struct db_i *dbip)
{
    register struct animate *anp;
    register struct directory *dp;
    register int i;

    if (!dbip)
	return;

    /* Rooted animations */
    for (anp = dbip->dbi_anroot; anp != ANIM_NULL;) {
	register struct animate *nextanp;
	RT_CK_ANIMATE(anp);
	nextanp = anp->an_forw;

	db_free_1anim(anp);
	anp = nextanp;
    }
    dbip->dbi_anroot = ANIM_NULL;

    /* Node animations */
    for (i=0; i < RT_DBNHASH; i++) {
	dp = dbip->dbi_Head[i];
	for (; dp != RT_DIR_NULL; dp = dp->d_forw) {
	    for (anp = dp->d_animate; anp != ANIM_NULL;) {
		register struct animate *nextanp;
		RT_CK_ANIMATE(anp);
		nextanp = anp->an_forw;

		db_free_1anim(anp);
		anp = nextanp;
	    }
	    dp->d_animate = ANIM_NULL;
	}
    }
}


struct animate *
db_parse_1anim(struct db_i *dbip, int argc, const char *argv[])
{
    struct db_tree_state ts;
    struct animate *anp;
    int i;

    if (!dbip)
	return NULL;

    if (argc < 4) {
	bu_log("db_parse_1anim:  not enough arguments\n");
	return (struct animate *)NULL;
    }

    BU_ALLOC(anp, struct animate);
    anp->magic = ANIMATE_MAGIC;

    db_init_db_tree_state(&ts, dbip, &rt_uniresource);
    db_full_path_init(&anp->an_path);
    if (db_follow_path_for_state(&ts, &(anp->an_path), argv[1], LOOKUP_NOISY) < 0)
	goto bad;

    if (BU_STR_EQUAL(argv[2], "matrix")) {
	if (argc < 5) {
	    bu_log("db_parse_1anim:  matrix does not have enough arguments\n");
	    goto bad;
	}

	anp->an_type = RT_AN_MATRIX;
	if (BU_STR_EQUAL(argv[3], "rstack"))
	    anp->an_u.anu_m.anm_op = ANM_RSTACK;
	else if (BU_STR_EQUAL(argv[3], "rarc"))
	    anp->an_u.anu_m.anm_op = ANM_RARC;
	else if (BU_STR_EQUAL(argv[3], "lmul"))
	    anp->an_u.anu_m.anm_op = ANM_LMUL;
	else if (BU_STR_EQUAL(argv[3], "rmul"))
	    anp->an_u.anu_m.anm_op = ANM_RMUL;
	else if (BU_STR_EQUAL(argv[3], "rboth"))
	    anp->an_u.anu_m.anm_op = ANM_RBOTH;
	else {
	    bu_log("db_parse_1anim:  Matrix op '%s' unknown\n",
		   argv[3]);
	    goto bad;
	}

	/* Allow some shorthands for the matrix spec */
	if (BU_STR_EQUAL(argv[4], "translate") ||
	    BU_STR_EQUAL(argv[4], "xlate")) {
	    if (argc < 5+3) {
		bu_log("db_parse_1anim:  matrix %s translate does not have enough arguments, only %d\n",
		       argv[3], argc);
		goto bad;
	    }
	    MAT_IDN(anp->an_u.anu_m.anm_mat);
	    MAT_DELTAS(anp->an_u.anu_m.anm_mat,
		       atof(argv[5+0]),
		       atof(argv[5+1]),
		       atof(argv[5+2]));
	} else if (BU_STR_EQUAL(argv[4], "rot")) {
	    if (argc < 5+3) {
		bu_log("db_parse_1anim:  matrix %s rot does not have enough arguments, only %d\n",
		       argv[3], argc);
		goto bad;
	    }
	    MAT_IDN(anp->an_u.anu_m.anm_mat);
	    bn_mat_angles(anp->an_u.anu_m.anm_mat,
			  atof(argv[5+0]),
			  atof(argv[5+1]),
			  atof(argv[5+2]));
	} else if (BU_STR_EQUAL(argv[4], "scale")) {
	    fastf_t scale;
	    if (argc < 6) {
		bu_log("db_parse_1anim:  matrix %s scale does not have enough arguments, only %d\n",
		       argv[3], argc);
		goto bad;
	    }
	    scale = atof(argv[5]);
	    if (ZERO(scale)) {
		bu_log("db_parse_1anim:  matrix %s scale factor is zero\n",
		       argv[3]);
		goto bad;
	    }
	    MAT_IDN(anp->an_u.anu_m.anm_mat);
	    anp->an_u.anu_m.anm_mat[15] = 1/scale;
	} else if (BU_STR_EQUAL(argv[4], "scale_about")) {
	    point_t pt;
	    fastf_t scale;
	    if (argc < 5+4) {
		bu_log("db_parse_1anim:  matrix %s scale_about does not have enough arguments, only %d\n",
		       argv[3], argc);
		goto bad;
	    }
	    VSET(pt,
		 atof(argv[5+0]),
		 atof(argv[5+1]),
		 atof(argv[5+2]));
	    scale = atof(argv[5+3]);
	    if (bn_mat_scale_about_pt(anp->an_u.anu_m.anm_mat,
				      pt, scale) < 0) {
		bu_log("db_parse_1anim: matrix %s scale_about (%g, %g, %g) scale=%g failed\n",
		       argv[3], V3ARGS(pt), scale);
		goto bad;
	    }
	} else {
	    if (argc < 20) {
		bu_log("db_parse_1anim:  matrix %s does not have enough arguments, only %d\n",
		       argv[3], argc);
		goto bad;
	    }

	    /* No keyword, assume full 4x4 matrix */
	    for (i=0; i<16; i++)
		anp->an_u.anu_m.anm_mat[i] = atof(argv[i+4]);
	}
    } else if (BU_STR_EQUAL(argv[2], "material")) {
	if (argc < 5) {
	    bu_log("db_parse_1anim:  material does not have enough arguments, only %d\n", argc);
	    goto bad;
	}

	anp->an_type = RT_AN_MATERIAL;
	bu_vls_init(&anp->an_u.anu_p.anp_shader);
	if ((BU_STR_EQUAL(argv[3], "replace")) ||
	    (BU_STR_EQUAL(argv[3], "rboth"))) {
	    bu_vls_from_argv(&anp->an_u.anu_p.anp_shader,
			     argc-4, (const char **)&argv[4]);
	    anp->an_u.anu_p.anp_op = RT_ANP_REPLACE;
	} else if (BU_STR_EQUAL(argv[3], "append")) {
	    bu_vls_from_argv(&anp->an_u.anu_p.anp_shader,
			     argc-4, (const char **)&argv[4]);
	    anp->an_u.anu_p.anp_op = RT_ANP_APPEND;
	} else {
	    bu_log("db_parse_1anim:  material animation '%s' unknown\n",
		   argv[3]);
	    goto bad;
	}
    } else if (BU_STR_EQUAL(argv[2], "color")) {
	if (argc < 6) {
	    bu_log("db_parse_1anim:  color does not have enough arguments, only %d\n", argc);
	    goto bad;
	}

	anp->an_type = RT_AN_COLOR;
	anp->an_u.anu_c.anc_rgb[0] = atoi(argv[3+0]);
	anp->an_u.anu_c.anc_rgb[1] = atoi(argv[3+1]);
	anp->an_u.anu_c.anc_rgb[2] = atoi(argv[3+2]);
    } else if (BU_STR_EQUAL(argv[2], "temperature") ||
	       BU_STR_EQUAL(argv[2], "temp")) {
	anp->an_type = RT_AN_TEMPERATURE;
	anp->an_u.anu_t = atof(argv[3]);
    } else {
	bu_log("db_parse_1anim:  animation type '%s' unknown\n", argv[2]);
	goto bad;
    }
    db_free_db_tree_state(&ts);
    return anp;
bad:
    db_free_db_tree_state(&ts);
    db_free_1anim(anp);		/* Does db_free_full_path() for us */
    return (struct animate *)NULL;
}

int db_parse_anim(struct db_i *dbip,
		  int argc,
		  const char **argv)
{
    struct animate *anp;
    int at_root = 0;

    anp = db_parse_1anim(dbip, argc, argv);
    if (!anp)
	return -1;	/* BAD */

    if (argv[1][0] == '/')
	at_root = 1;

    if (anp->an_path.fp_len > 1)
	at_root = 0;

    if (db_add_anim(dbip, anp, at_root) < 0) {
	return -1;	/* BAD */
    }
    return 0;		/* OK */
}
void
db_write_anim(FILE *fop, struct animate *anp)
{
    char *thepath;
    int i;

    if (!fop)
	return;

    RT_CK_ANIMATE(anp);

    thepath  = db_path_to_string(&(anp->an_path));
    if (RT_G_DEBUG&DEBUG_ANIM) {
	bu_log("db_write_anim: Writing %s\n", thepath);
    }

    fprintf(fop, "anim %s ", thepath);
    bu_free(thepath, "path string");

    switch (anp->an_type) {
	case RT_AN_MATRIX:
	    fputs("matrix ", fop);
	    switch (anp->an_u.anu_m.anm_op) {
		case ANM_RSTACK:
		    fputs("rstack\n", fop);
		    break;
		case ANM_RARC:
		    fputs("rarc\n", fop);
		    break;
		case ANM_LMUL:
		    fputs("lmul\n", fop);
		    break;
		case ANM_RMUL:
		    fputs("rmul\n", fop);
		    break;
		case ANM_RBOTH:
		    fputs("rboth\n", fop);
		    break;
		default:
		    fputs("unknown\n", fop);
		    bu_log("db_write_anim: unknown matrix operation\n");
	    }
	    for (i=0; i<16; i++) {
		fprintf(fop, " %.15e", anp->an_u.anu_m.anm_mat[i]);
		if ((i == 15) || ((i&3) == 3)) {
		    fputs("\n", fop);
		}
	    }
	    break;
	case RT_AN_MATERIAL:
	    fputs("material ", fop);
	    switch (anp->an_u.anu_p.anp_op) {
		case RT_ANP_REPLACE:
		    fputs("replace ", fop);
		    break;
		case RT_ANP_APPEND:
		    fputs("append ", fop);
		    break;
		default:
		    bu_log("db_write_anim: unknown property operation.\n");
		    break;
	    }
	    break;
	case RT_AN_COLOR:
	    fprintf(fop, "color %d %d %d", anp->an_u.anu_c.anc_rgb[0],
		    anp->an_u.anu_c.anc_rgb[1], anp->an_u.anu_c.anc_rgb[2]);
	    break;
	case RT_AN_SOLID:
	    break;
	default:
	    bu_log("db_write_anim: Unknown animate type.\n");
    }
    fputs(";\n", fop);
    return;
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
