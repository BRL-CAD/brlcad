/*                        T Y P E I N . C
 * BRL-CAD
 *
 * Copyright (c) 1985-2011 United States Government as represented by
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

/** @file libged/typein.c
 *
 * The "in" command allows solid parameters to be entered via the keyboard.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include "bio.h"

#include "rtgeom.h"
#include "wdb.h"

#include "./ged_private.h"


static char *p_half[] = {
    "Enter X, Y, Z of outward pointing normal vector: ",
    "Enter Y: ",
    "Enter Z: ",
    "Enter the distance from the origin: "
};


static char *p_dsp_v4[] = {
    "Enter name of displacement-map file: ",
    "Enter width of displacement-map (number of values): ",
    "Enter length of displacement-map (number of values): ",
    "Normal Interpolation? 0=no 1=yes: ",
    "Cell size: ",
    "Unit elevation: "
};


static char *p_dsp_v5[] = {
    "Take data from file or database binary object [f|o]:",
    "Enter name of file/object: ",
    "Enter width of displacement-map (number of values): ",
    "Enter length of displacement-map (number of values): ",
    "Normal Interpolation? 0=no 1=yes: ",
    "Cut direction [ad|lR|Lr] ",
    "Cell size: ",
    "Unit elevation: "
};


static char *p_hf[] = {
    "Enter name of control file (or \"\" for none): ",
    "Enter name of data file (containing heights): ",
    "Enter 'cv' style format of data [h|n][s|u]c|s|i|l|d|8|16|32|64: ",
    "Enter number of values in 'x' direction: ",
    "Enter number of values in 'y' direction: ",
    "Enter '1' if data can be stored as 'short' in memory, or 0: ",
    "Enter factor to convert file data to mm: ",
    "Enter coordinates to position HF solid: ",
    "Enter Y coordinate: ",
    "Enter Z coordinate: ",
    "Enter direction vector for 'x' direction: ",
    "Enter Y coordinate: ",
    "Enter Z coordinate: ",
    "Enter direction vector for 'y' direction: ",
    "Enter Y coordinate: ",
    "Enter Z coordinate: ",
    "Enter length of HF in 'x' direction: ",
    "Enter width of HF in 'y' direction: ",
    "Enter scale factor for height (after conversion to mm): "
};


static char *p_ebm[] = {
    "Enter name of bit-map file: ",
    "Enter width of bit-map (number of cells): ",
    "Enter height of bit-map (number of cells): ",
    "Enter extrusion distance: "
};


static char *p_submodel[] = {
    "Enter name of treetop: ",
    "Enter space partitioning method: ",
    "Enter name of .g file (or \"\" for none): "
};


static char *p_vol[] = {
    "Enter name of file containing voxel data: ",
    "Enter X, Y, Z dimensions of file (number of cells): ",
    "Enter Y dimension of file (number of cells): ",
    "Enter Z dimension of file (number of cells): ",
    "Enter lower threshold value: ",
    "Enter upper threshold value: ",
    "Enter X, Y, Z dimensions of a cell: ",
    "Enter Y dimension of a cell: ",
    "Enter Z dimension of a cell: ",
};


static char *p_bot[] = {
    "Enter number of vertices: ",
    "Enter number of triangles: ",
    "Enter mode (1->surface, 2->solid, 3->plate): ",
    "Enter triangle orientation (1->unoriented, 2->counter-clockwise, 3->clockwise): ",
    "Enter X, Y, Z",
    "Enter Y",
    "Enter Z",
    "Enter three vertex numbers",
    "Enter second vertex number",
    "Enter third vertex number",
    "Enter face_mode (0->centered, 1->appended) and thickness",
    "Enter thickness"
};


static char *p_arbn[] = {
    "Enter number of planes: ",
    "Enter coefficients",
    "Enter Y-coordinate of normal",
    "Enter Z-coordinate of normal",
    "Enter distance of plane along normal from origin"
};


static char *p_pipe[] = {
    "Enter number of points: ",
    "Enter X, Y, Z, inner diameter, outer diameter, and bend radius for first point: ",
    "Enter Y: ",
    "Enter Z: ",
    "Enter inner diameter: ",
    "Enter outer diameter: ",
    "Enter bend radius: ",
    "Enter X, Y, Z, inner diameter, outer diameter, and bend radius",
    "Enter Y",
    "Enter Z",
    "Enter inner diameter",
    "Enter outer diameter",
    "Enter bend radius"
};


static char *p_ars[] = {
    "Enter number of points per waterline, and number of waterlines: ",
    "Enter number of waterlines: ",
    "Enter X, Y, Z for First row point: ",
    "Enter Y for First row point: ",
    "Enter Z for First row point: ",
    "Enter X  Y  Z",
    "Enter Y",
    "Enter Z",
};


static char *p_arb[] = {
    "Enter X, Y, Z for point 1: ",
    "Enter Y: ",
    "Enter Z: ",
    "Enter X, Y, Z for point 2: ",
    "Enter Y: ",
    "Enter Z: ",
    "Enter X, Y, Z for point 3: ",
    "Enter Y: ",
    "Enter Z: ",
    "Enter X, Y, Z for point 4: ",
    "Enter Y: ",
    "Enter Z: ",
    "Enter X, Y, Z for point 5: ",
    "Enter Y: ",
    "Enter Z: ",
    "Enter X, Y, Z for point 6: ",
    "Enter Y: ",
    "Enter Z: ",
    "Enter X, Y, Z for point 7: ",
    "Enter Y: ",
    "Enter Z: ",
    "Enter X, Y, Z for point 8: ",
    "Enter Y: ",
    "Enter Z: "
};


static char *p_sph[] = {
    "Enter X, Y, Z of vertex: ",
    "Enter Y: ",
    "Enter Z: ",
    "Enter radius: "
};


static char *p_ellg[] = {
    "Enter X, Y, Z of focus point 1: ",
    "Enter Y: ",
    "Enter Z: ",
    "Enter X, Y, Z of focus point 2: ",
    "Enter Y: ",
    "Enter Z: ",
    "Enter axis length L: "
};


static char *p_ell1[] = {
    "Enter X, Y, Z of vertex: ",
    "Enter Y: ",
    "Enter Z: ",
    "Enter X, Y, Z of vector A: ",
    "Enter Y: ",
    "Enter Z: ",
    "Enter radius of revolution: "
};


static char *p_ell[] = {
    "Enter X, Y, Z of vertex: ",
    "Enter Y: ",
    "Enter Z: ",
    "Enter X, Y, Z of vector A: ",
    "Enter Y: ",
    "Enter Z: ",
    "Enter X, Y, Z of vector B: ",
    "Enter Y: ",
    "Enter Z: ",
    "Enter X, Y, Z of vector C: ",
    "Enter Y: ",
    "Enter Z: "
};


static char *p_tor[] = {
    "Enter X, Y, Z of vertex: ",
    "Enter Y: ",
    "Enter Z: ",
    "Enter X, Y, Z of normal vector: ",
    "Enter Y: ",
    "Enter Z: ",
    "Enter radius 1: ",
    "Enter radius 2: "
};


static char *p_rcc[] = {
    "Enter X, Y, Z of vertex: ",
    "Enter Y: ",
    "Enter Z: ",
    "Enter X, Y, Z of height (H) vector: ",
    "Enter Y: ",
    "Enter Z: ",
    "Enter radius: "
};


static char *p_tec[] = {
    "Enter X, Y, Z of vertex: ",
    "Enter Y: ",
    "Enter Z: ",
    "Enter X, Y, Z of height (H) vector: ",
    "Enter Y: ",
    "Enter Z: ",
    "Enter X, Y, Z of vector A: ",
    "Enter Y: ",
    "Enter Z: ",
    "Enter X, Y, Z of vector B: ",
    "Enter Y: ",
    "Enter Z: ",
    "Enter ratio: "
};


static char *p_rec[] = {
    "Enter X, Y, Z of vertex: ",
    "Enter Y: ",
    "Enter Z: ",
    "Enter X, Y, Z of height (H) vector: ",
    "Enter Y: ",
    "Enter Z: ",
    "Enter X, Y, Z of vector A: ",
    "Enter Y: ",
    "Enter Z: ",
    "Enter X, Y, Z of vector B: ",
    "Enter Y: ",
    "Enter Z: "
};


static char *p_trc[] = {
    "Enter X, Y, Z of vertex: ",
    "Enter Y: ",
    "Enter Z: ",
    "Enter X, Y, Z of height (H) vector: ",
    "Enter Y: ",
    "Enter Z: ",
    "Enter radius of base: ",
    "Enter radius of top: "
};


static char *p_tgc[] = {
    "Enter X, Y, Z of vertex: ",
    "Enter Y: ",
    "Enter Z: ",
    "Enter X, Y, Z of height (H) vector: ",
    "Enter Y: ",
    "Enter Z: ",
    "Enter X, Y, Z of vector A: ",
    "Enter Y: ",
    "Enter Z: ",
    "Enter X, Y, Z of vector B: ",
    "Enter Y: ",
    "Enter Z: ",
    "Enter scalar c: ",
    "Enter scalar d: "
};


static char *p_box[] = {
    "Enter X, Y, Z of vertex: ",
    "Enter Y: ",
    "Enter Z: ",
    "Enter X, Y, Z of vector H: ",
    "Enter Y: ",
    "Enter Z: ",
    "Enter X, Y, Z of vector W: ",
    "Enter Y: ",
    "Enter Z: ",
    "Enter X, Y, Z of vector D: ",
    "Enter Y: ",
    "Enter Z: "
};


static char *p_rpp[] = {
    "Enter XMIN, XMAX, YMIN, YMAX, ZMIN, ZMAX: ",
    "Enter XMAX: ",
    "Enter YMIN, YMAX, ZMIN, ZMAX: ",
    "Enter YMAX: ",
    "Enter ZMIN, ZMAX: ",
    "Enter ZMAX: "
};


static char *p_orpp[] = {
    "Enter XMAX, YMAX, ZMAX: ",
    "Enter YMAX, ZMAX: ",
    "Enter ZMAX: "
};


static char *p_rpc[] = {
    "Enter X, Y, Z of vertex: ",
    "Enter Y: ",
    "Enter Z: ",
    "Enter X, Y, Z, of vector H: ",
    "Enter Y: ",
    "Enter Z: ",
    "Enter X, Y, Z, of vector B: ",
    "Enter Y: ",
    "Enter Z: ",
    "Enter rectangular half-width, r: "
};


static char *p_part[] = {
    "Enter X, Y, Z of vertex: ",
    "Enter Y: ",
    "Enter Z: ",
    "Enter X, Y, Z, of vector H: ",
    "Enter Y: ",
    "Enter Z: ",
    "Enter v end radius: ",
    "Enter h end radius: "
};


static char *p_rhc[] = {
    "Enter X, Y, Z of vertex: ",
    "Enter Y: ",
    "Enter Z: ",
    "Enter X, Y, Z, of vector H: ",
    "Enter Y: ",
    "Enter Z: ",
    "Enter X, Y, Z, of vector B: ",
    "Enter Y: ",
    "Enter Z: ",
    "Enter rectangular half-width, r: ",
    "Enter apex-to-asymptotes distance, c: "
};


static char *p_epa[] = {
    "Enter X, Y, Z of vertex: ",
    "Enter Y: ",
    "Enter Z: ",
    "Enter X, Y, Z, of vector H: ",
    "Enter Y: ",
    "Enter Z: ",
    "Enter X, Y, Z, of vector A: ",
    "Enter Y: ",
    "Enter Z: ",
    "Enter magnitude of vector B: "
};


static char *p_ehy[] = {
    "Enter X, Y, Z of vertex: ",
    "Enter Y: ",
    "Enter Z: ",
    "Enter X, Y, Z, of vector H: ",
    "Enter Y: ",
    "Enter Z: ",
    "Enter X, Y, Z, of vector A: ",
    "Enter Y: ",
    "Enter Z: ",
    "Enter magnitude of vector B: ",
    "Enter apex-to-asymptotes distance, c: "
};


static char *p_hyp[] = {
    "Enter X, Y, Z of vertex: ",
    "Enter Y: ",
    "Enter Z: ",
    "Enter X, Y, Z, of vector H: ",
    "Enter Y: ",
    "Enter Z: ",
    "Enter X, Y, Z, of vector A: ",
    "Enter Y: ",
    "Enter Z: ",
    "Enter magnitude of vector B: ",
    "Enter neck to base ratio, c (0, 1): "
};


static char *p_eto[] = {
    "Enter X, Y, Z of vertex: ",
    "Enter Y: ",
    "Enter Z: ",
    "Enter X, Y, Z, of normal vector: ",
    "Enter Y: ",
    "Enter Z: ",
    "Enter radius of revolution, r: ",
    "Enter X, Y, Z, of vector C: ",
    "Enter Y: ",
    "Enter Z: ",
    "Enter magnitude of elliptical semi-minor axis, d: "
};


static char *p_binunif[] = {
    "Enter minor type (f, d, c, s, i, L, C, S, I, or L): ",
    "Enter name of file containing the data: ",
    "Enter number of values to read (-1 for entire file): "
};


static char *p_extrude[] = {
    "Enter X, Y, Z of vertex: ",
    "Enter Y: ",
    "Enter Z: ",
    "Enter X, Y, Z of H: ",
    "Enter Y: ",
    "Enter Z: ",
    "Enter X, Y, Z of A: ",
    "Enter Y: ",
    "Enter Z: ",
    "Enter X, Y, Z of B: ",
    "Enter Y: ",
    "Enter Z: ",
    "Enter name of sketch: ",
    NULL
};


static char *p_grip[] = {
    "Enter X, Y, Z of center: ",
    "Enter Y: ",
    "Enter Z: ",
    "Enter X, Y, Z of normal: ",
    "Enter Y: ",
    "Enter Z: ",
    "Enter Magnitude: ",
    NULL
};


static char *p_superell[] = {
    "Enter X, Y, Z of superellipse vertex: ",
    "Enter Y: ",
    "Enter Z: ",
    "Enter X, Y, Z of vector A: ",
    "Enter Y: ",
    "Enter Z: ",
    "Enter X, Y, Z of vector B: ",
    "Enter Y: ",
    "Enter Z: ",
    "Enter X, Y, Z of vector C: ",
    "Enter Y: ",
    "Enter Z: ",
    "Enter n, e of north-south and east-west power: ",
    "Enter e: "
};


static char *p_metaball[] = {
    "Enter render method: ",
    "Enter threshold: ",
    "Enter number of points: ",
    "Enter X, Y, Z, field strength: ",
    "Enter Y: ",
    "Enter Z: ",
    "Enter field strength: ",
    "Enter X, Y, Z, field strength",
    "Enter Y",
    "Enter Z",
    "Enter field strength"
};


static char *p_revolve[] = {
    "Enter X, Y, Z of vertex: ",
    "Enter Y: ",
    "Enter Z: ",
    "Enter X, Y, Z of revolve axis: ",
    "Enter Y: ",
    "Enter Z: ",
    "Enter X, Y, Z of vector in start plane: ",
    "Enter Y: ",
    "Enter Z: ",
    "Enter angle: ",
    "Enter name of sketch: "
};


static char *p_pnts[] = {
    "Are points in a file (yes/no)? ",
    "Enter number of points (-1 for auto): ",
    "Are the points orientated (yes/no)? ",
    "Do the points have color values (yes/no)? ",
    "Do the points differ in size (yes/no)? ",
    "Enter default point size (>= 0.0): ",
    "Enter X, Y, Z position",
    "Enter Y position component",
    "Enter Z position component",
    "Enter X, Y, Z orientation vector",
    "Enter Y orientation vector component",
    "Enter Z orientation vector component",
    "Enter R, G, B color values (0 to 255)",
    "Enter G component color value",
    "Enter B component color value",
    "Enter point size (>= 0.0, -1 for default)",
    "Enter point file path and name: ",
    "Enter file data format (px, py, pz, cr, cg, cb, s, nx, ny, nz): ",
    "Enter file data units ([mm|cm|m|in|ft]): "
};


/**
 * helper function that infers a boolean value from a given string
 * returning 0 or 1 for false and true respectively.
 *
 * False values are any answers that are case-insensitive variants of
 * 0, "no", "false", and whitespace-only, NULL, or empty strings.
 *
 * True values are any answers that are not false.
 */
static int
booleanize(const char *answer)
{
    int idx = 0;
    const char *ap;
    static const char *noes[] = {
	"0",
	"n",
	"no",
	"false",
	NULL
    };

    if (!answer || (strlen(answer) <= 0)) {
	return 0;
    }

    ap = answer;
    while (ap[0] != '\0' && isspace(ap[0])) {
	ap++;
    }
    if (ap[0] == '\0') {
	return 0;
    }

    ap = noes[idx];
    while (ap && ap[0] != '\0') {
	if ((strcasecmp(ap, answer) == 0)) {
	    return 0;
	}
	idx++;
	ap = noes[idx];
    }

    /* true! */
    return 1;
}


static int
binunif_in(struct ged *gedp, const char **cmd_argvs, struct rt_db_internal *intern, const char *name)
{
    unsigned int minor_type;

    intern->idb_ptr = NULL;

    if (strlen(cmd_argvs[3]) != 1) {
	bu_vls_printf(gedp->ged_result_str, "Unrecognized minor type (%s)\n", cmd_argvs[3]);
	return GED_ERROR;
    }

    switch (*cmd_argvs[3]) {
	case 'f':
	    minor_type = DB5_MINORTYPE_BINU_FLOAT;
	    break;
	case 'd':
	    minor_type = DB5_MINORTYPE_BINU_DOUBLE;
	    break;
	case 'c':
	    minor_type = DB5_MINORTYPE_BINU_8BITINT;
	    break;
	case 's':
	    minor_type = DB5_MINORTYPE_BINU_16BITINT;
	    break;
	case 'i':
	    minor_type = DB5_MINORTYPE_BINU_32BITINT;
	    break;
	case 'l':
	    minor_type = DB5_MINORTYPE_BINU_64BITINT;
	    break;
	case 'C':
	    minor_type = DB5_MINORTYPE_BINU_8BITINT_U;
	    break;
	case 'S':
	    minor_type = DB5_MINORTYPE_BINU_16BITINT_U;
	    break;
	case 'I':
	    minor_type = DB5_MINORTYPE_BINU_32BITINT_U;
	    break;
	case 'L':
	    minor_type = DB5_MINORTYPE_BINU_64BITINT_U;
	    break;
	default:
	    bu_log("Unrecognized minor type (%c)\n", *cmd_argvs[3]);
	    return GED_ERROR;
    }
    if (rt_mk_binunif (gedp->ged_wdbp, name, cmd_argvs[4], minor_type, atol(cmd_argvs[5]))) {
	bu_vls_printf(gedp->ged_result_str,
		      "Failed to create binary object %s from file %s\n",
		      name, cmd_argvs[4]);
	return GED_ERROR;
    }

    return GED_OK;
}


/* E B M _ I N
 *
 * Read EBM solid from keyboard
 *
 */
static int
ebm_in(struct ged *gedp, const char **cmd_argvs, struct rt_db_internal *intern)
{
    struct rt_ebm_internal *ebm;

    BU_GETSTRUCT(ebm, rt_ebm_internal);
    intern->idb_major_type = DB5_MAJORTYPE_BRLCAD;
    intern->idb_type = ID_EBM;
    intern->idb_meth = &rt_functab[ID_EBM];
    intern->idb_ptr = (genptr_t)ebm;
    ebm->magic = RT_EBM_INTERNAL_MAGIC;

    bu_strlcpy(ebm->file, cmd_argvs[3], RT_EBM_NAME_LEN);
    ebm->xdim = atoi(cmd_argvs[4]);
    ebm->ydim = atoi(cmd_argvs[5]);
    ebm->tallness = atof(cmd_argvs[6]) * gedp->ged_wdbp->dbip->dbi_local2base;
    MAT_IDN(ebm->mat);

    return GED_OK;
}


/* S U B M O D E L _ I N
 *
 * Read submodel from keyboard
 *
 */
static int
submodel_in(struct ged *UNUSED(gedp), const char **cmd_argvs, struct rt_db_internal *intern)
{
    struct rt_submodel_internal *sip;

    BU_GETSTRUCT(sip, rt_submodel_internal);
    intern->idb_major_type = DB5_MAJORTYPE_BRLCAD;
    intern->idb_type = ID_SUBMODEL;
    intern->idb_meth = &rt_functab[ID_SUBMODEL];
    intern->idb_ptr = (genptr_t)sip;
    sip->magic = RT_SUBMODEL_INTERNAL_MAGIC;

    bu_vls_init(&sip->treetop);
    bu_vls_strcpy(&sip->treetop, cmd_argvs[3]);
    sip->meth = atoi(cmd_argvs[4]);
    bu_vls_init(&sip->file);
    bu_vls_strcpy(&sip->file, cmd_argvs[5]);

    return GED_OK;
}


/* D S P _ I N
 *
 * Read DSP solid from keyboard
 */
static int
dsp_in_v4(struct ged *gedp, const char **cmd_argvs, struct rt_db_internal *intern)
{
    struct rt_dsp_internal *dsp;

    BU_GETSTRUCT(dsp, rt_dsp_internal);
    intern->idb_major_type = DB5_MAJORTYPE_BRLCAD;
    intern->idb_type = ID_DSP;
    intern->idb_meth = &rt_functab[ID_DSP];
    intern->idb_ptr = (genptr_t)dsp;
    dsp->magic = RT_DSP_INTERNAL_MAGIC;

    bu_vls_init(&dsp->dsp_name);
    bu_vls_strcpy(&dsp->dsp_name, cmd_argvs[3]);

    dsp->dsp_xcnt = atoi(cmd_argvs[4]);
    dsp->dsp_ycnt = atoi(cmd_argvs[5]);
    dsp->dsp_smooth = atoi(cmd_argvs[6]);
    MAT_IDN(dsp->dsp_stom);

    dsp->dsp_stom[0] = dsp->dsp_stom[5] =
	atof(cmd_argvs[7]) * gedp->ged_wdbp->dbip->dbi_local2base;

    dsp->dsp_stom[10] = atof(cmd_argvs[8]) * gedp->ged_wdbp->dbip->dbi_local2base;

    bn_mat_inv(dsp->dsp_mtos, dsp->dsp_stom);

    return GED_OK;
}


extern void dsp_dump(struct rt_dsp_internal *dsp);

/* D S P _ I N
 *
 * Read DSP solid from keyboard
 */
static int
dsp_in_v5(struct ged *gedp, const char **cmd_argvs, struct rt_db_internal *intern)
{
    struct rt_dsp_internal *dsp;

    BU_GETSTRUCT(dsp, rt_dsp_internal);
    intern->idb_major_type = DB5_MAJORTYPE_BRLCAD;
    intern->idb_type = ID_DSP;
    intern->idb_meth = &rt_functab[ID_DSP];
    intern->idb_ptr = (genptr_t)dsp;
    dsp->magic = RT_DSP_INTERNAL_MAGIC;

    if (*cmd_argvs[3] == 'f' || *cmd_argvs[3] == 'F')
	dsp->dsp_datasrc = RT_DSP_SRC_FILE;
    else if (*cmd_argvs[3] == 'O' || *cmd_argvs[3] == 'o')
	dsp->dsp_datasrc = RT_DSP_SRC_OBJ;
    else
	return GED_ERROR;

    bu_vls_init(&dsp->dsp_name);
    bu_vls_strcpy(&dsp->dsp_name, cmd_argvs[4]);

    dsp->dsp_xcnt = atoi(cmd_argvs[5]);
    dsp->dsp_ycnt = atoi(cmd_argvs[6]);
    dsp->dsp_smooth = atoi(cmd_argvs[7]);
    switch (*cmd_argvs[8]) {
	case 'a':	/* adaptive */
	case 'A':
	    dsp->dsp_cuttype = DSP_CUT_DIR_ADAPT;
	    break;
	case 'l':	/* lower left to upper right */
	    dsp->dsp_cuttype = DSP_CUT_DIR_llUR;
	    break;
	case 'L':	/* Upper Left to lower right */
	    dsp->dsp_cuttype = DSP_CUT_DIR_ULlr;
	    break;
	default:
	    bu_log("Error: dsp_cuttype:\"%s\"\n", cmd_argvs[8]);
	    return GED_ERROR;
	    break;
    }

    MAT_IDN(dsp->dsp_stom);

    dsp->dsp_stom[0] = dsp->dsp_stom[5] =
	atof(cmd_argvs[9]) * gedp->ged_wdbp->dbip->dbi_local2base;

    dsp->dsp_stom[10] = atof(cmd_argvs[10]) * gedp->ged_wdbp->dbip->dbi_local2base;

    bn_mat_inv(dsp->dsp_mtos, dsp->dsp_stom);

    return GED_OK;
}


/* H F _ I N
 *
 * Read HF solid from keyboard
 *
 */
static int
hf_in(struct ged *gedp, const char **cmd_argvs, struct rt_db_internal *intern)
{
    struct rt_hf_internal *hf;

    BU_GETSTRUCT(hf, rt_hf_internal);
    intern->idb_major_type = DB5_MAJORTYPE_BRLCAD;
    intern->idb_type = ID_HF;
    intern->idb_meth = &rt_functab[ID_HF];
    intern->idb_ptr = (genptr_t)hf;
    hf->magic = RT_HF_INTERNAL_MAGIC;

    bu_strlcpy(hf->cfile, cmd_argvs[3], sizeof(hf->cfile));
    bu_strlcpy(hf->dfile, cmd_argvs[4], sizeof(hf->dfile));
    bu_strlcpy(hf->fmt, cmd_argvs[5], sizeof(hf->fmt));

    hf->w = atoi(cmd_argvs[6]);
    hf->n = atoi(cmd_argvs[7]);
    hf->shorts = atoi(cmd_argvs[8]);
    hf->file2mm = atof(cmd_argvs[9]);
    hf->v[0] = atof(cmd_argvs[10]) * gedp->ged_wdbp->dbip->dbi_local2base;
    hf->v[1] = atof(cmd_argvs[11]) * gedp->ged_wdbp->dbip->dbi_local2base;
    hf->v[2] = atof(cmd_argvs[12]) * gedp->ged_wdbp->dbip->dbi_local2base;
    hf->x[0] = atof(cmd_argvs[13]);
    hf->x[1] = atof(cmd_argvs[14]);
    hf->x[2] = atof(cmd_argvs[15]);
    hf->y[0] = atof(cmd_argvs[16]);
    hf->y[1] = atof(cmd_argvs[17]);
    hf->y[2] = atof(cmd_argvs[18]);
    hf->xlen = atof(cmd_argvs[19]) * gedp->ged_wdbp->dbip->dbi_local2base;
    hf->ylen = atof(cmd_argvs[20]) * gedp->ged_wdbp->dbip->dbi_local2base;
    hf->zscale = atof(cmd_argvs[21]);

    if (hf->w < 2 || hf->n < 2) {
	bu_vls_printf(gedp->ged_result_str, "ERROR: length or width of fta file is too small\n");
	return GED_ERROR;
    }

    if (hf->xlen <= 0 || hf->ylen <= 0) {
	bu_vls_printf(gedp->ged_result_str, "ERROR: length and width of HF solid must be greater than 0\n");
	return GED_ERROR;
    }

    /* XXXX should check for orthogonality of 'x' and 'y' vectors */

    hf->mp = bu_open_mapped_file(hf->dfile, "hf");
    if (!hf->mp) {
	bu_vls_printf(gedp->ged_result_str, "ERROR: cannot open data file\n");
	hf->mp = (struct bu_mapped_file *)NULL;
	return GED_ERROR;
    }

    return GED_OK;
}


/* V O L _ I N
 *
 * Read VOL solid from keyboard
 *
 */
static int
vol_in(struct ged *gedp, const char **cmd_argvs, struct rt_db_internal *intern)
{
    struct rt_vol_internal *vol;

    BU_GETSTRUCT(vol, rt_vol_internal);
    intern->idb_major_type = DB5_MAJORTYPE_BRLCAD;
    intern->idb_type = ID_VOL;
    intern->idb_meth = &rt_functab[ID_VOL];
    intern->idb_ptr = (genptr_t)vol;
    vol->magic = RT_VOL_INTERNAL_MAGIC;

    bu_strlcpy(vol->file, cmd_argvs[3], sizeof(vol->file));
    vol->xdim = atoi(cmd_argvs[4]);
    vol->ydim = atoi(cmd_argvs[5]);
    vol->zdim = atoi(cmd_argvs[6]);
    vol->lo = atoi(cmd_argvs[7]);
    vol->hi = atoi(cmd_argvs[8]);
    vol->cellsize[0] = atof(cmd_argvs[9]) * gedp->ged_wdbp->dbip->dbi_local2base;
    vol->cellsize[1] = atof(cmd_argvs[10]) * gedp->ged_wdbp->dbip->dbi_local2base;
    vol->cellsize[2] = atof(cmd_argvs[11]) * gedp->ged_wdbp->dbip->dbi_local2base;
    MAT_IDN(vol->mat);

    return GED_OK;
}


/*
 * B O T _ I N
 */
static int
bot_in(struct ged *gedp, int argc, const char **argv, struct rt_db_internal *intern, char **prompt)
{
    int i;
    int num_verts, num_faces;
    int mode, orientation;
    int arg_count;
    struct rt_bot_internal *bot;

    if (argc < 7) {
	bu_vls_printf(gedp->ged_result_str, "%s", prompt[argc-3]);
	return GED_MORE;
    }

    num_verts = atoi(argv[3]);
    if (num_verts < 3) {
	bu_vls_printf(gedp->ged_result_str, "Invalid number of vertices (must be at least 3)\n");
	return GED_ERROR;
    }

    num_faces = atoi(argv[4]);
    if (num_faces < 1) {
	bu_vls_printf(gedp->ged_result_str, "Invalid number of triangles (must be at least 1)\n");
	return GED_ERROR;
    }

    mode = atoi(argv[5]);
    if (mode < 1 || mode > 3) {
	bu_vls_printf(gedp->ged_result_str, "Invalid mode (must be 1, 2, or 3)\n");
	return GED_ERROR;
    }

    orientation = atoi(argv[6]);
    if (orientation < 1 || orientation > 3) {
	bu_vls_printf(gedp->ged_result_str, "Invalid orientation (must be 1, 2, or 3)\n");
	return GED_ERROR;
    }

    arg_count = argc - 7;
    if (arg_count < num_verts*3) {
	bu_vls_printf(gedp->ged_result_str, "%s for vertex %d : ", prompt[4+arg_count%3], arg_count/3);
	return GED_MORE;
    }

    arg_count = argc - 7 - num_verts*3;
    if (arg_count < num_faces*3) {
	bu_vls_printf(gedp->ged_result_str, "%s for triangle %d : ", prompt[7+arg_count%3], arg_count/3);
	return GED_MORE;
    }

    if (mode == RT_BOT_PLATE) {
	arg_count = argc - 7 - num_verts*3 - num_faces*3;
	if (arg_count < num_faces*2) {
	    bu_vls_printf(gedp->ged_result_str, "%s for face %d : ", prompt[10+arg_count%2], arg_count/2);
	    return GED_MORE;
	}
    }

    intern->idb_major_type = DB5_MAJORTYPE_BRLCAD;
    intern->idb_type = ID_BOT;
    intern->idb_meth = &rt_functab[ID_BOT];
    bot = (struct rt_bot_internal *)bu_calloc(1, sizeof(struct rt_bot_internal), "rt_bot_internal");
    intern->idb_ptr = (genptr_t)bot;
    bot->magic = RT_BOT_INTERNAL_MAGIC;
    bot->num_vertices = num_verts;
    bot->num_faces = num_faces;
    bot->mode = mode;
    bot->orientation = orientation;
    bot->faces = (int *)bu_calloc(bot->num_faces * 3, sizeof(int), "bot faces");
    bot->vertices = (fastf_t *)bu_calloc(bot->num_vertices * 3, sizeof(fastf_t), "bot vertices");
    bot->thickness = (fastf_t *)NULL;
    bot->face_mode = (struct bu_bitv *)NULL;

    for (i=0; i<num_verts; i++) {
	bot->vertices[i*3] = atof(argv[7+i*3]) * gedp->ged_wdbp->dbip->dbi_local2base;
	bot->vertices[i*3+1] = atof(argv[8+i*3]) * gedp->ged_wdbp->dbip->dbi_local2base;
	bot->vertices[i*3+2] = atof(argv[9+i*3]) * gedp->ged_wdbp->dbip->dbi_local2base;
    }

    arg_count = 7 + num_verts*3;
    for (i=0; i<num_faces; i++) {
	bot->faces[i*3] = atoi(argv[arg_count + i*3]);
	bot->faces[i*3+1] = atoi(argv[arg_count + i*3 + 1]);
	bot->faces[i*3+2] = atoi(argv[arg_count + i*3 + 2]);
    }

    if (mode == RT_BOT_PLATE) {
	arg_count = 7 + num_verts*3 + num_faces*3;
	bot->thickness = (fastf_t *)bu_calloc(num_faces, sizeof(fastf_t), "bot thickness");
	bot->face_mode = bu_bitv_new(num_faces);
	for (i=0; i<num_faces; i++) {
	    int j;

	    j = atoi(argv[arg_count + i*2]);
	    if (j == 1)
		BU_BITSET(bot->face_mode, i);
	    else if (j != 0) {
		bu_vls_printf(gedp->ged_result_str, "Invalid face mode (must be 0 or 1)\n");
		return GED_ERROR;
	    }
	    bot->thickness[i] = atof(argv[arg_count + i*2 + 1]) * gedp->ged_wdbp->dbip->dbi_local2base;
	}
    }

    return GED_OK;
}


/*
 * A R B N _ I N
 */
static int
arbn_in(struct ged *gedp, int argc, const char **argv, struct rt_db_internal *intern, char **prompt)
{
    struct rt_arbn_internal *arbn;
    int num_planes=0;
    size_t i;

    if (argc < 4) {
	bu_vls_printf(gedp->ged_result_str, "%s", prompt[argc-3]);
	return GED_MORE;
    }

    num_planes = atoi(argv[3]);

    if (argc < num_planes * 4 + 4) {
	bu_vls_printf(gedp->ged_result_str, "%s for plane %d : ", prompt[(argc-4)%4 + 1], 1+(argc-4)/4);
	return GED_MORE;
    }

    intern->idb_major_type = DB5_MAJORTYPE_BRLCAD;
    intern->idb_type = ID_ARBN;
    intern->idb_meth = &rt_functab[ID_ARBN];
    intern->idb_ptr = (genptr_t)bu_malloc(sizeof(struct rt_arbn_internal),
					  "rt_arbn_internal");
    arbn = (struct rt_arbn_internal *)intern->idb_ptr;
    arbn->magic = RT_ARBN_INTERNAL_MAGIC;
    arbn->neqn = num_planes;
    arbn->eqn = (plane_t *)bu_calloc(arbn->neqn, sizeof(plane_t), "arbn planes");

    /* Normal is unscaled, should have unit length; d is scaled */
    for (i=0; i<arbn->neqn; i++) {
	arbn->eqn[i][X] = atof(argv[4+i*4]);
	arbn->eqn[i][Y] = atof(argv[4+i*4+1]);
	arbn->eqn[i][Z] = atof(argv[4+i*4+2]);
	arbn->eqn[i][W] = atof(argv[4+i*4+3]) * gedp->ged_wdbp->dbip->dbi_local2base;
    }

    return GED_OK;
}


/*
 * P I P E _ I N
 */
static int
pipe_in(struct ged *gedp, int argc, const char **argv, struct rt_db_internal *intern, char **prompt)
{
    struct rt_pipe_internal *pipeip;
    int i, num_points;

    if (argc < 4) {
	bu_vls_printf(gedp->ged_result_str, "%s", prompt[argc-3]);
	return GED_MORE;
    }

    num_points = atoi(argv[3]);
    if (num_points < 2) {
	bu_vls_printf(gedp->ged_result_str, "Invalid number of points (must be at least 2)\n");
	return GED_ERROR;
    }

    if (argc < 10) {
	bu_vls_printf(gedp->ged_result_str, "%s", prompt[argc-3]);
	return GED_MORE;
    }

    if (argc < 4 + num_points*6) {
	bu_vls_printf(gedp->ged_result_str, "%s for point %d : ", prompt[7+(argc-10)%6], 1+(argc-4)/6);
	return GED_MORE;
    }

    intern->idb_major_type = DB5_MAJORTYPE_BRLCAD;
    intern->idb_type = ID_PIPE;
    intern->idb_meth = &rt_functab[ID_PIPE];
    intern->idb_ptr = (genptr_t)bu_malloc(sizeof(struct rt_pipe_internal), "rt_pipe_internal");
    pipeip = (struct rt_pipe_internal *)intern->idb_ptr;
    pipeip->pipe_magic = RT_PIPE_INTERNAL_MAGIC;
    BU_LIST_INIT(&pipeip->pipe_segs_head);
    for (i=4; i<argc; i+= 6) {
	struct wdb_pipept *pipept;

	pipept = (struct wdb_pipept *)bu_malloc(sizeof(struct wdb_pipept), "wdb_pipept");
	pipept->pp_coord[0] = atof(argv[i]) * gedp->ged_wdbp->dbip->dbi_local2base;
	pipept->pp_coord[1] = atof(argv[i+1]) * gedp->ged_wdbp->dbip->dbi_local2base;
	pipept->pp_coord[2] = atof(argv[i+2]) * gedp->ged_wdbp->dbip->dbi_local2base;
	pipept->pp_id = atof(argv[i+3]) * gedp->ged_wdbp->dbip->dbi_local2base;
	pipept->pp_od = atof(argv[i+4]) * gedp->ged_wdbp->dbip->dbi_local2base;
	pipept->pp_bendradius = atof(argv[i+5]) * gedp->ged_wdbp->dbip->dbi_local2base;

	BU_LIST_INSERT(&pipeip->pipe_segs_head, &pipept->l);
    }

    if (rt_pipe_ck(&pipeip->pipe_segs_head)) {
	bu_vls_printf(gedp->ged_result_str, "Illegal pipe, solid not made!!\n");
	return GED_ERROR;
    }

    return GED_OK;
}


/*
 * A R S _ I N
 */
static int
ars_in(struct ged *gedp, int argc, const char **argv, struct rt_db_internal *intern, char **prompt)
{
    struct rt_ars_internal *arip;
    size_t i;
    size_t total_points;
    int cv;	/* current curve (waterline) # */
    size_t axis;	/* current fastf_t in waterline */
    int ncurves_minus_one;
    int num_pts = 0;
    int num_curves = 0;
    int vals_present, total_vals_needed;

    vals_present = argc - 3;

    if (vals_present > 0) {
	num_pts = atoi(argv[3]);
	if (num_pts < 3) {
	    bu_vls_printf(gedp->ged_result_str, "points per waterline must be >= 3\n");
	    intern->idb_meth = &rt_functab[ID_ARS];
	    return GED_ERROR;
	}
    }

    if (vals_present > 1) {
	num_curves = atoi(argv[4]);
	if (num_curves < 3) {
	    bu_vls_printf(gedp->ged_result_str, "points per waterline must be >= 3\n");
	    intern->idb_meth = &rt_functab[ID_ARS];
	    return GED_ERROR;
	}
    }

    if (vals_present < 5) {
	/* for #rows, #pts/row & first point,
	 * pre-formatted prompts exist
	 */
	bu_vls_printf(gedp->ged_result_str, "%s", prompt[vals_present]);
	return GED_MORE;
    }

    total_vals_needed = 2 +		/* #rows, #pts/row */
	(ELEMENTS_PER_POINT * 2) +	/* the first point, and very last */
	(num_pts * ELEMENTS_PER_POINT * (num_curves-2)); /* the curves */

    if (vals_present < (total_vals_needed - ELEMENTS_PER_POINT)) {
	/* if we're looking for points on the curves, and not
	 * the last point which makes up the last curve, we
	 * have to format up a prompt string
	 */

	switch ((vals_present-2) % 3) {
	    case 0:
		bu_vls_printf(gedp->ged_result_str, "%s for Waterline %d, Point %d : ",
			      prompt[5],
			      1+(argc-8)/3/num_pts,
			      ((argc-8)/3)%num_pts);
		break;
	    case 1:
		bu_vls_printf(gedp->ged_result_str, "%s for Waterline %d, Point %d : ",
			      prompt[6],
			      1+(argc-8)/3/num_pts,
			      ((argc-8)/3)%num_pts);
		break;
	    case 2:
		bu_vls_printf(gedp->ged_result_str, "%s for Waterline %d, Point %d : ",
			      prompt[7],
			      1+(argc-8)/3/num_pts,
			      ((argc-8)/3)%num_pts);
		break;
	}

	return GED_MORE;
    } else if (vals_present < total_vals_needed) {
	/* we're looking for the last point which is used for all points
	 * on the last curve
	 */

	switch ((vals_present-2) % 3) {
	    case 0:
		bu_vls_printf(gedp->ged_result_str, "%s for pt of last Waterline : %d, %d",
			      prompt[5],
			      1+(argc-8)/3/num_pts,
			      ((argc-8)/3)%num_pts);
		break;
	    case 1:
		bu_vls_printf(gedp->ged_result_str, "%s for pt of last Waterline : %d, %d",
			      prompt[6],
			      1+(argc-8)/3/num_pts,
			      ((argc-8)/3)%num_pts);
		break;
	    case 2:
		bu_vls_printf(gedp->ged_result_str, "%s for pt of last Waterline : %d, %d",
			      prompt[7],
			      1+(argc-8)/3/num_pts,
			      ((argc-8)/3)%num_pts);
		break;
	}

	return GED_MORE;
    }

    intern->idb_major_type = DB5_MAJORTYPE_BRLCAD;
    intern->idb_type = ID_ARS;
    intern->idb_meth = &rt_functab[ID_ARS];
    intern->idb_ptr = (genptr_t)bu_malloc(sizeof(struct rt_ars_internal), "rt_ars_internal");
    arip = (struct rt_ars_internal *)intern->idb_ptr;
    arip->magic = RT_ARS_INTERNAL_MAGIC;
    arip->pts_per_curve = num_pts;
    arip->ncurves = num_curves;
    ncurves_minus_one = arip->ncurves - 1;
    total_points = arip->ncurves * arip->pts_per_curve;

    arip->curves = (fastf_t **)bu_malloc(
	(arip->ncurves+1) * sizeof(fastf_t **), "ars curve ptrs");
    for (i=0; i < arip->ncurves+1; i++) {
	/* Leave room for first point to be repeated */
	arip->curves[i] = (fastf_t *)bu_malloc(
	    (arip->pts_per_curve+1) * sizeof(point_t),
	    "ars curve");
    }

    /* fill in the point of the first row */
    arip->curves[0][0] = atof(argv[5]) * gedp->ged_wdbp->dbip->dbi_local2base;
    arip->curves[0][1] = atof(argv[6]) * gedp->ged_wdbp->dbip->dbi_local2base;
    arip->curves[0][2] = atof(argv[7]) * gedp->ged_wdbp->dbip->dbi_local2base;

    /* The first point is duplicated across the first curve */
    for (i=1; i < arip->pts_per_curve; ++i) {
	VMOVE(arip->curves[0]+3*i, arip->curves[0]);
    }

    cv = 1;
    axis = 0;
    /* scan each of the other points we've already got */
    for (i=8; i < (size_t)argc && i < total_points * ELEMENTS_PER_POINT; ++i) {
	arip->curves[cv][axis] = atof(argv[i]) * gedp->ged_wdbp->dbip->dbi_local2base;
	if (++axis >= arip->pts_per_curve * ELEMENTS_PER_POINT) {
	    axis = 0;
	    cv++;
	}
    }

    /* The first point is duplicated across the last curve */
    for (i=1; i < arip->pts_per_curve; ++i) {
	VMOVE(arip->curves[ncurves_minus_one]+3*i,
	      arip->curves[ncurves_minus_one]);
    }

    return GED_OK;
}


/* H A L F _ I N () :   	reads halfspace parameters from keyboard
 * returns 0 if successful read
 * 1 if unsuccessful read
 */
static int
half_in(struct ged *gedp, const char **cmd_argvs, struct rt_db_internal *intern, const char *name)
{
    vect_t norm;
    double d;

    intern->idb_ptr = NULL;

    norm[X] = atof(cmd_argvs[3+0]);
    norm[Y] = atof(cmd_argvs[3+1]);
    norm[Z] = atof(cmd_argvs[3+2]);
    d = atof(cmd_argvs[3+3]) * gedp->ged_wdbp->dbip->dbi_local2base;

    if (MAGNITUDE(norm) < RT_LEN_TOL) {
	bu_vls_printf(gedp->ged_result_str, "ERROR, normal vector is too small!\n");
	return GED_ERROR;
    }

    VUNITIZE(norm);
    if (mk_half(gedp->ged_wdbp, name, norm, d) < 0)
	return GED_ERROR;

    return GED_OK;
}


/* A R B _ I N () :   	reads arb parameters from keyboard
 * returns 0 if successful read
 * 1 if unsuccessful read
 */
static int
arb_in(struct ged *gedp, const char **cmd_argvs, struct rt_db_internal *intern)
{
    int i, j, n;
    struct rt_arb_internal *aip;

    intern->idb_major_type = DB5_MAJORTYPE_BRLCAD;
    intern->idb_type = ID_ARB8;
    intern->idb_meth = &rt_functab[ID_ARB8];
    intern->idb_ptr = (genptr_t)bu_malloc(sizeof(struct rt_arb_internal),
					  "rt_arb_internal");
    aip = (struct rt_arb_internal *)intern->idb_ptr;
    aip->magic = RT_ARB_INTERNAL_MAGIC;

    n = atoi(&cmd_argvs[2][3]);	/* get # from "arb#" */
    for (j = 0; j < n; j++)
	for (i = 0; i < ELEMENTS_PER_POINT; i++)
	    aip->pt[j][i] = atof(cmd_argvs[3+i+3*j]) * gedp->ged_wdbp->dbip->dbi_local2base;

    if (BU_STR_EQUAL("arb4", cmd_argvs[2])) {
	VMOVE(aip->pt[7], aip->pt[3]);
	VMOVE(aip->pt[6], aip->pt[3]);
	VMOVE(aip->pt[5], aip->pt[3]);
	VMOVE(aip->pt[4], aip->pt[3]);
	VMOVE(aip->pt[3], aip->pt[0]);
    } else if (BU_STR_EQUAL("arb5", cmd_argvs[2])) {
	VMOVE(aip->pt[7], aip->pt[4]);
	VMOVE(aip->pt[6], aip->pt[4]);
	VMOVE(aip->pt[5], aip->pt[4]);
    } else if (BU_STR_EQUAL("arb6", cmd_argvs[2])) {
	VMOVE(aip->pt[7], aip->pt[5]);
	VMOVE(aip->pt[6], aip->pt[5]);
	VMOVE(aip->pt[5], aip->pt[4]);
    } else if (BU_STR_EQUAL("arb7", cmd_argvs[2])) {
	VMOVE(aip->pt[7], aip->pt[4]);
    }

    return GED_OK;	/* success */
}


/* S P H _ I N () :   	reads sph parameters from keyboard
 * returns 0 if successful read
 * 1 if unsuccessful read
 */
static int
sph_in(struct ged *gedp, const char **cmd_argvs, struct rt_db_internal *intern, const char *name)
{
    point_t center;
    fastf_t r;
    int i;

    intern->idb_ptr = NULL;

    for (i = 0; i < ELEMENTS_PER_POINT; i++) {
	center[i] = atof(cmd_argvs[3+i]) * gedp->ged_wdbp->dbip->dbi_local2base;
    }
    r = atof(cmd_argvs[6]) * gedp->ged_wdbp->dbip->dbi_local2base;

    if (r < RT_LEN_TOL) {
	bu_vls_printf(gedp->ged_result_str, "ERROR, radius must be greater than zero!\n");
	return GED_ERROR;
    }

    if (mk_sph(gedp->ged_wdbp, name, center, r) < 0)
	return GED_ERROR;
    return GED_OK;
}


/* E L L _ I N () :   	reads ell parameters from keyboard
 * returns 0 if successful read
 * 1 if unsuccessful read
 */
static int
ell_in(struct ged *gedp, const char **cmd_argvs, struct rt_db_internal *intern)
{
    fastf_t len, mag_b, r_rev, vals[12];
    int i, n;
    struct rt_ell_internal *eip;

    n = 12;				/* ELL has twelve params */
    if (cmd_argvs[2][3] != '\0')	/* ELLG and ELL1 have seven */
	n = 7;

    intern->idb_major_type = DB5_MAJORTYPE_BRLCAD;
    intern->idb_type = ID_ELL;
    intern->idb_meth = &rt_functab[ID_ELL];
    intern->idb_ptr = (genptr_t)bu_malloc(sizeof(struct rt_ell_internal),
					  "rt_ell_internal");
    eip = (struct rt_ell_internal *)intern->idb_ptr;
    eip->magic = RT_ELL_INTERNAL_MAGIC;

    /* convert typed in args to reals */
    for (i = 0; i < n; i++) {
	vals[i] = atof(cmd_argvs[3+i]) * gedp->ged_wdbp->dbip->dbi_local2base;
    }

    if (BU_STR_EQUAL("ell", cmd_argvs[2])) {
	/* everything's ok */
	/* V, A, B, C */
	VMOVE(eip->v, &vals[0]);
	VMOVE(eip->a, &vals[3]);
	VMOVE(eip->b, &vals[6]);
	VMOVE(eip->c, &vals[9]);
	return GED_OK;
    }

    if (BU_STR_EQUAL("ellg", cmd_argvs[2])) {
	/* V, f1, f2, len */
	/* convert ELLG format into ELL1 format */
	len = vals[6];
	/* V is halfway between the foci */
	VADD2(eip->v, &vals[0], &vals[3]);
	VSCALE(eip->v, eip->v, 0.5);
	VSUB2(eip->b, &vals[3], &vals[0]);
	mag_b = MAGNITUDE(eip->b);
	if (NEAR_ZERO(mag_b, RT_LEN_TOL)) {
	    bu_vls_printf(gedp->ged_result_str, "ERROR, foci are coincident!\n");
	    return GED_ERROR;
	}
	/* calculate A vector */
	VSCALE(eip->a, eip->b, .5*len/mag_b);
	/* calculate radius of revolution (for ELL1 format) */
	r_rev = sqrt(MAGSQ(eip->a) - (mag_b*.5)*(mag_b*.5));
    } else if (BU_STR_EQUAL("ell1", cmd_argvs[2])) {
	/* V, A, r */
	VMOVE(eip->v, &vals[0]);
	VMOVE(eip->a, &vals[3]);
	r_rev = vals[6];
    } else {
	r_rev = 0;
    }

    /* convert ELL1 format into ELLG format */
    /* calculate B vector */
    bn_vec_ortho(eip->b, eip->a);
    VUNITIZE(eip->b);
    VSCALE(eip->b, eip->b, r_rev);

    /* calculate C vector */
    VCROSS(eip->c, eip->a, eip->b);
    VUNITIZE(eip->c);
    VSCALE(eip->c, eip->c, r_rev);
    return GED_OK;
}


/* T O R _ I N () :   	reads tor parameters from keyboard
 * returns 0 if successful read
 * 1 if unsuccessful read
 */
static int
tor_in(struct ged *gedp, const char **cmd_argvs, struct rt_db_internal *intern)
{
    int i;
    struct rt_tor_internal *tip;

    intern->idb_major_type = DB5_MAJORTYPE_BRLCAD;
    intern->idb_type = ID_TOR;
    intern->idb_meth = &rt_functab[ID_TOR];
    intern->idb_ptr = (genptr_t)bu_malloc(sizeof(struct rt_tor_internal),
					  "rt_tor_internal");
    tip = (struct rt_tor_internal *)intern->idb_ptr;
    tip->magic = RT_TOR_INTERNAL_MAGIC;

    for (i = 0; i < ELEMENTS_PER_POINT; i++) {
	tip->v[i] = atof(cmd_argvs[3+i]) * gedp->ged_wdbp->dbip->dbi_local2base;
	tip->h[i] = atof(cmd_argvs[6+i]) * gedp->ged_wdbp->dbip->dbi_local2base;
    }
    tip->r_a = atof(cmd_argvs[9]) * gedp->ged_wdbp->dbip->dbi_local2base;
    tip->r_h = atof(cmd_argvs[10]) * gedp->ged_wdbp->dbip->dbi_local2base;
    /* Check for radius 2 >= radius 1 */
    if (tip->r_a <= tip->r_h) {
	bu_vls_printf(gedp->ged_result_str, "ERROR, radius 2 >= radius 1 ....\n");
	return GED_ERROR;
    }

    if (MAGNITUDE(tip->h) < RT_LEN_TOL) {
	bu_vls_printf(gedp->ged_result_str, "ERROR, normal must be greater than zero!\n");
	return GED_ERROR;
    }

    return GED_OK;
}


/* T G C _ I N () :   	reads tgc parameters from keyboard
 * returns 0 if successful read
 * 1 if unsuccessful read
 */
static int
tgc_in(struct ged *gedp, const char **cmd_argvs, struct rt_db_internal *intern)
{
    fastf_t r1, r2;
    int i;
    struct rt_tgc_internal *tip;

    intern->idb_major_type = DB5_MAJORTYPE_BRLCAD;
    intern->idb_type = ID_TGC;
    intern->idb_meth = &rt_functab[ID_TGC];
    intern->idb_ptr = (genptr_t)bu_malloc(sizeof(struct rt_tgc_internal),
					  "rt_tgc_internal");
    tip = (struct rt_tgc_internal *)intern->idb_ptr;
    tip->magic = RT_TGC_INTERNAL_MAGIC;

    for (i = 0; i < ELEMENTS_PER_POINT; i++) {
	tip->v[i] = atof(cmd_argvs[3+i]) * gedp->ged_wdbp->dbip->dbi_local2base;
	tip->h[i] = atof(cmd_argvs[6+i]) * gedp->ged_wdbp->dbip->dbi_local2base;
	tip->a[i] = atof(cmd_argvs[9+i]) * gedp->ged_wdbp->dbip->dbi_local2base;
	tip->b[i] = atof(cmd_argvs[12+i]) * gedp->ged_wdbp->dbip->dbi_local2base;
    }
    r1 = atof(cmd_argvs[15]) * gedp->ged_wdbp->dbip->dbi_local2base;
    r2 = atof(cmd_argvs[16]) * gedp->ged_wdbp->dbip->dbi_local2base;

    if (MAGNITUDE(tip->h) < RT_LEN_TOL
	|| MAGNITUDE(tip->a) < RT_LEN_TOL
	|| MAGNITUDE(tip->b) < RT_LEN_TOL
	|| r1 < RT_LEN_TOL || r2 < RT_LEN_TOL) {
	bu_vls_printf(gedp->ged_result_str, "ERROR, all dimensions must be greater than zero!\n");
	return GED_ERROR;
    }

    /* calculate C */
    VMOVE(tip->c, tip->a);
    VUNITIZE(tip->c);
    VSCALE(tip->c, tip->c, r1);

    /* calculate D */
    VMOVE(tip->d, tip->b);
    VUNITIZE(tip->d);
    VSCALE(tip->d, tip->d, r2);

    return GED_OK;
}


/* R C C _ I N () :   	reads rcc parameters from keyboard
 * returns 0 if successful read
 * 1 if unsuccessful read
 */
static int
rcc_in(struct ged *gedp, const char **cmd_argvs, struct rt_db_internal *intern)
{
    fastf_t r;
    int i;
    struct rt_tgc_internal *tip;

    intern->idb_major_type = DB5_MAJORTYPE_BRLCAD;
    intern->idb_type = ID_TGC;
    intern->idb_meth = &rt_functab[ID_TGC];
    intern->idb_ptr = (genptr_t)bu_malloc(sizeof(struct rt_tgc_internal),
					  "rt_tgc_internal");
    tip = (struct rt_tgc_internal *)intern->idb_ptr;
    tip->magic = RT_TGC_INTERNAL_MAGIC;

    for (i = 0; i < ELEMENTS_PER_POINT; i++) {
	tip->v[i] = atof(cmd_argvs[3+i]) * gedp->ged_wdbp->dbip->dbi_local2base;
	tip->h[i] = atof(cmd_argvs[6+i]) * gedp->ged_wdbp->dbip->dbi_local2base;
    }
    r = atof(cmd_argvs[9]) * gedp->ged_wdbp->dbip->dbi_local2base;

    if (MAGNITUDE(tip->h) < RT_LEN_TOL || r < RT_LEN_TOL) {
	bu_vls_printf(gedp->ged_result_str, "ERROR, all dimensions must be greater than zero!\n");
	return GED_ERROR;
    }

    bn_vec_ortho(tip->a, tip->h);
    VUNITIZE(tip->a);
    VCROSS(tip->b, tip->h, tip->a);
    VUNITIZE(tip->b);

    VSCALE(tip->a, tip->a, r);
    VSCALE(tip->b, tip->b, r);
    VMOVE(tip->c, tip->a);
    VMOVE(tip->d, tip->b);

    return GED_OK;
}


/* T E C _ I N () :   	reads tec parameters from keyboard
 * returns 0 if successful read
 * 1 if unsuccessful read
 */
static int
tec_in(struct ged *gedp, const char **cmd_argvs, struct rt_db_internal *intern)
{
    fastf_t ratio;
    int i;
    struct rt_tgc_internal *tip;

    intern->idb_major_type = DB5_MAJORTYPE_BRLCAD;
    intern->idb_type = ID_TGC;
    intern->idb_meth = &rt_functab[ID_TGC];
    intern->idb_ptr = (genptr_t)bu_malloc(sizeof(struct rt_tgc_internal),
					  "rt_tgc_internal");
    tip = (struct rt_tgc_internal *)intern->idb_ptr;
    tip->magic = RT_TGC_INTERNAL_MAGIC;

    for (i = 0; i < ELEMENTS_PER_POINT; i++) {
	tip->v[i] = atof(cmd_argvs[3+i]) * gedp->ged_wdbp->dbip->dbi_local2base;
	tip->h[i] = atof(cmd_argvs[6+i]) * gedp->ged_wdbp->dbip->dbi_local2base;
	tip->a[i] = atof(cmd_argvs[9+i]) * gedp->ged_wdbp->dbip->dbi_local2base;
	tip->b[i] = atof(cmd_argvs[12+i]) * gedp->ged_wdbp->dbip->dbi_local2base;
    }
    ratio = atof(cmd_argvs[15]);
    if (MAGNITUDE(tip->h) < RT_LEN_TOL
	|| MAGNITUDE(tip->a) < RT_LEN_TOL
	|| MAGNITUDE(tip->b) < RT_LEN_TOL
	|| ratio < RT_LEN_TOL) {
	bu_vls_printf(gedp->ged_result_str, "ERROR, all dimensions must be greater than zero!\n");
	return GED_ERROR;
    }

    VSCALE(tip->c, tip->a, 1./ratio);	/* C vector */
    VSCALE(tip->d, tip->b, 1./ratio);	/* D vector */

    return GED_OK;
}


/* R E C _ I N () :   	reads rec parameters from keyboard
 * returns 0 if successful read
 * 1 if unsuccessful read
 */
static int
rec_in(struct ged *gedp, const char **cmd_argvs, struct rt_db_internal *intern)
{
    int i;
    struct rt_tgc_internal *tip;

    intern->idb_major_type = DB5_MAJORTYPE_BRLCAD;
    intern->idb_type = ID_TGC;
    intern->idb_meth = &rt_functab[ID_TGC];
    intern->idb_ptr = (genptr_t)bu_malloc(sizeof(struct rt_tgc_internal),
					  "rt_tgc_internal");
    tip = (struct rt_tgc_internal *)intern->idb_ptr;
    tip->magic = RT_TGC_INTERNAL_MAGIC;

    for (i = 0; i < ELEMENTS_PER_POINT; i++) {
	tip->v[i] = atof(cmd_argvs[3+i]) * gedp->ged_wdbp->dbip->dbi_local2base;
	tip->h[i] = atof(cmd_argvs[6+i]) * gedp->ged_wdbp->dbip->dbi_local2base;
	tip->a[i] = atof(cmd_argvs[9+i]) * gedp->ged_wdbp->dbip->dbi_local2base;
	tip->b[i] = atof(cmd_argvs[12+i]) * gedp->ged_wdbp->dbip->dbi_local2base;
    }

    if (MAGNITUDE(tip->h) < RT_LEN_TOL
	|| MAGNITUDE(tip->a) < RT_LEN_TOL
	|| MAGNITUDE(tip->b) < RT_LEN_TOL) {
	bu_vls_printf(gedp->ged_result_str, "ERROR, all dimensions must be greater than zero!\n");
	return GED_ERROR;
    }

    VMOVE(tip->c, tip->a);		/* C vector */
    VMOVE(tip->d, tip->b);		/* D vector */

    return GED_OK;
}


/* T R C _ I N () :   	reads trc parameters from keyboard
 * returns 0 if successful read
 * 1 if unsuccessful read
 */
static int
trc_in(struct ged *gedp, const char **cmd_argvs, struct rt_db_internal *intern)
{
    fastf_t r1, r2;
    int i;
    struct rt_tgc_internal *tip;

    intern->idb_major_type = DB5_MAJORTYPE_BRLCAD;
    intern->idb_type = ID_TGC;
    intern->idb_meth = &rt_functab[ID_TGC];
    intern->idb_ptr = (genptr_t)bu_malloc(sizeof(struct rt_tgc_internal),
					  "rt_tgc_internal");
    tip = (struct rt_tgc_internal *)intern->idb_ptr;
    tip->magic = RT_TGC_INTERNAL_MAGIC;

    for (i = 0; i < ELEMENTS_PER_POINT; i++) {
	tip->v[i] = atof(cmd_argvs[3+i]) * gedp->ged_wdbp->dbip->dbi_local2base;
	tip->h[i] = atof(cmd_argvs[6+i]) * gedp->ged_wdbp->dbip->dbi_local2base;
    }
    r1 = atof(cmd_argvs[9]) * gedp->ged_wdbp->dbip->dbi_local2base;
    r2 = atof(cmd_argvs[10]) * gedp->ged_wdbp->dbip->dbi_local2base;

    if (MAGNITUDE(tip->h) < RT_LEN_TOL
	|| r1 < RT_LEN_TOL || r2 < RT_LEN_TOL) {
	bu_vls_printf(gedp->ged_result_str, "ERROR, all dimensions must be greater than zero!\n");
	return GED_ERROR;
    }

    bn_vec_ortho(tip->a, tip->h);
    VUNITIZE(tip->a);
    VCROSS(tip->b, tip->h, tip->a);
    VUNITIZE(tip->b);
    VMOVE(tip->c, tip->a);
    VMOVE(tip->d, tip->b);

    VSCALE(tip->a, tip->a, r1);
    VSCALE(tip->b, tip->b, r1);
    VSCALE(tip->c, tip->c, r2);
    VSCALE(tip->d, tip->d, r2);

    return GED_OK;
}


/* B O X _ I N () :   	reads box parameters from keyboard
 * returns 0 if successful read
 * 1 if unsuccessful read
 */
static int
box_in(struct ged *gedp, const char **cmd_argvs, struct rt_db_internal *intern)
{
    int i;
    struct rt_arb_internal *aip;
    vect_t Dpth, Hgt, Vrtx, Wdth;

    intern->idb_major_type = DB5_MAJORTYPE_BRLCAD;
    intern->idb_type = ID_ARB8;
    intern->idb_meth = &rt_functab[ID_ARB8];
    intern->idb_ptr = (genptr_t)bu_malloc(sizeof(struct rt_arb_internal),
					  "rt_arb_internal");
    aip = (struct rt_arb_internal *)intern->idb_ptr;
    aip->magic = RT_ARB_INTERNAL_MAGIC;

    for (i = 0; i < ELEMENTS_PER_POINT; i++) {
	Vrtx[i] = atof(cmd_argvs[3+i]) * gedp->ged_wdbp->dbip->dbi_local2base;
	Hgt[i] = atof(cmd_argvs[6+i]) * gedp->ged_wdbp->dbip->dbi_local2base;
	Wdth[i] = atof(cmd_argvs[9+i]) * gedp->ged_wdbp->dbip->dbi_local2base;
	Dpth[i] = atof(cmd_argvs[12+i]) * gedp->ged_wdbp->dbip->dbi_local2base;
    }

    if (MAGNITUDE(Dpth) < RT_LEN_TOL || MAGNITUDE(Hgt) < RT_LEN_TOL
	|| MAGNITUDE(Wdth) < RT_LEN_TOL) {
	bu_vls_printf(gedp->ged_result_str, "ERROR, all dimensions must be greater than zero!\n");
	return GED_ERROR;
    }

    if (BU_STR_EQUAL("box", cmd_argvs[2])) {
	VMOVE(aip->pt[0], Vrtx);
	VADD2(aip->pt[1], Vrtx, Wdth);
	VADD3(aip->pt[2], Vrtx, Wdth, Hgt);
	VADD2(aip->pt[3], Vrtx, Hgt);
	VADD2(aip->pt[4], Vrtx, Dpth);
	VADD3(aip->pt[5], Vrtx, Dpth, Wdth);
	VADD4(aip->pt[6], Vrtx, Dpth, Wdth, Hgt);
	VADD3(aip->pt[7], Vrtx, Dpth, Hgt);
    } else {
	/* "raw" */
	VADD2(aip->pt[0], Vrtx, Wdth);
	VADD2(aip->pt[1], Vrtx, Hgt);
	VADD2(aip->pt[2], aip->pt[1], Dpth);
	VADD2(aip->pt[3], aip->pt[0], Dpth);
	VMOVE(aip->pt[4], Vrtx);
	VMOVE(aip->pt[5], Vrtx);
	VADD2(aip->pt[6], Vrtx, Dpth);
	VMOVE(aip->pt[7], aip->pt[6]);
    }

    return GED_OK;
}


/* R P P _ I N () :   	reads rpp parameters from keyboard
 * returns GED_OK if successful read
 * GED_ERROR if unsuccessful read
 */
static int
rpp_in(struct ged *gedp, const char **cmd_argvs, struct rt_db_internal *intern, const char *name)
{
    point_t min, max;

    intern->idb_ptr = NULL;

    min[X] = atof(cmd_argvs[3+0]) * gedp->ged_wdbp->dbip->dbi_local2base;
    max[X] = atof(cmd_argvs[3+1]) * gedp->ged_wdbp->dbip->dbi_local2base;
    min[Y] = atof(cmd_argvs[3+2]) * gedp->ged_wdbp->dbip->dbi_local2base;
    max[Y] = atof(cmd_argvs[3+3]) * gedp->ged_wdbp->dbip->dbi_local2base;
    min[Z] = atof(cmd_argvs[3+4]) * gedp->ged_wdbp->dbip->dbi_local2base;
    max[Z] = atof(cmd_argvs[3+5]) * gedp->ged_wdbp->dbip->dbi_local2base;

    if (min[X] >= max[X]) {
	bu_vls_printf(gedp->ged_result_str, "ERROR, XMIN:(%lg) greater than XMAX:(%lg) !\n", min[X], max[X]);
	return GED_ERROR;
    }
    if (min[Y] >= max[Y]) {
	bu_vls_printf(gedp->ged_result_str, "ERROR, YMIN:(%lg) greater than YMAX:(%lg) !\n", min[Y], max[Y]);
	return GED_ERROR;
    }
    if (min[Z] >= max[Z]) {
	bu_vls_printf(gedp->ged_result_str, "ERROR, ZMIN:(%lg) greater than ZMAX:(%lg)!\n", min[Z], max[Z]);
	return GED_ERROR;
    }

    if (mk_rpp(gedp->ged_wdbp, name, min, max) < 0)
	return 1;

    return GED_OK;
}


/*
 * O R P P _ I N ()
 *
 * Reads origin-min rpp (box) parameters from keyboard
 * returns 0 if successful read
 * 1 if unsuccessful read
 */
static int
orpp_in(struct ged *gedp, const char **cmd_argvs, struct rt_db_internal *intern, const char *name)
{
    point_t min, max;

    intern->idb_ptr = NULL;

    VSETALL(min, 0);
    max[X] = atof(cmd_argvs[3+0]) * gedp->ged_wdbp->dbip->dbi_local2base;
    max[Y] = atof(cmd_argvs[3+1]) * gedp->ged_wdbp->dbip->dbi_local2base;
    max[Z] = atof(cmd_argvs[3+2]) * gedp->ged_wdbp->dbip->dbi_local2base;

    if (min[X] >= max[X]) {
	bu_vls_printf(gedp->ged_result_str, "ERROR, XMIN greater than XMAX!\n");
	return GED_ERROR;
    }
    if (min[Y] >= max[Y]) {
	bu_vls_printf(gedp->ged_result_str, "ERROR, YMIN greater than YMAX!\n");
	return GED_ERROR;
    }
    if (min[Z] >= max[Z]) {
	bu_vls_printf(gedp->ged_result_str, "ERROR, ZMIN greater than ZMAX!\n");
	return GED_ERROR;
    }

    if (mk_rpp(gedp->ged_wdbp, name, min, max) < 0)
	return GED_ERROR;

    return GED_OK;
}


/* P A R T _ I N () :	reads particle parameters from keyboard
 * returns 0 if successful read
 * 1 if unsuccessful read
 */
static int
part_in(struct ged *gedp, const char **cmd_argvs, struct rt_db_internal *intern)
{
    int i;
    struct rt_part_internal *part_ip;

    intern->idb_major_type = DB5_MAJORTYPE_BRLCAD;
    intern->idb_type = ID_PARTICLE;
    intern->idb_meth = &rt_functab[ID_PARTICLE];
    intern->idb_ptr = (genptr_t)bu_malloc(sizeof(struct rt_part_internal),
					  "rt_part_internal");
    part_ip = (struct rt_part_internal *)intern->idb_ptr;
    part_ip->part_magic = RT_PART_INTERNAL_MAGIC;

    for (i = 0; i < ELEMENTS_PER_POINT; i++) {
	part_ip->part_V[i] = atof(cmd_argvs[3+i]) * gedp->ged_wdbp->dbip->dbi_local2base;
	part_ip->part_H[i] = atof(cmd_argvs[6+i]) * gedp->ged_wdbp->dbip->dbi_local2base;
    }
    part_ip->part_vrad = atof(cmd_argvs[9]) * gedp->ged_wdbp->dbip->dbi_local2base;
    part_ip->part_hrad = atof(cmd_argvs[10]) * gedp->ged_wdbp->dbip->dbi_local2base;

    if (MAGNITUDE(part_ip->part_H) < RT_LEN_TOL
	|| part_ip->part_vrad <= RT_LEN_TOL
	|| part_ip->part_hrad <= RT_LEN_TOL) {
	bu_vls_printf(gedp->ged_result_str, "ERROR, height, v radius and h radius must be greater than zero!\n");
	return GED_ERROR;
    }

    return GED_OK;
}


/* R P C _ I N () :   	reads rpc parameters from keyboard
 * returns 0 if successful read
 * 1 if unsuccessful read
 */
static int
rpc_in(struct ged *gedp, const char **cmd_argvs, struct rt_db_internal *intern)
{
    int i;
    struct rt_rpc_internal *rip;

    intern->idb_major_type = DB5_MAJORTYPE_BRLCAD;
    intern->idb_type = ID_RPC;
    intern->idb_meth = &rt_functab[ID_RPC];
    intern->idb_ptr = (genptr_t)bu_malloc(sizeof(struct rt_rpc_internal),
					  "rt_rpc_internal");
    rip = (struct rt_rpc_internal *)intern->idb_ptr;
    rip->rpc_magic = RT_RPC_INTERNAL_MAGIC;

    for (i = 0; i < ELEMENTS_PER_POINT; i++) {
	rip->rpc_V[i] = atof(cmd_argvs[3+i]) * gedp->ged_wdbp->dbip->dbi_local2base;
	rip->rpc_H[i] = atof(cmd_argvs[6+i]) * gedp->ged_wdbp->dbip->dbi_local2base;
	rip->rpc_B[i] = atof(cmd_argvs[9+i]) * gedp->ged_wdbp->dbip->dbi_local2base;
    }
    rip->rpc_r = atof(cmd_argvs[12]) * gedp->ged_wdbp->dbip->dbi_local2base;

    if (MAGNITUDE(rip->rpc_H) < RT_LEN_TOL
	|| MAGNITUDE(rip->rpc_B) < RT_LEN_TOL
	|| rip->rpc_r <= RT_LEN_TOL) {
	bu_vls_printf(gedp->ged_result_str, "ERROR, height, breadth, and width must be greater than zero!\n");
	return GED_ERROR;
    }

    return GED_OK;
}


/* R H C _ I N () :   	reads rhc parameters from keyboard
 * returns 0 if successful read
 * 1 if unsuccessful read
 */
static int
rhc_in(struct ged *gedp, const char **cmd_argvs, struct rt_db_internal *intern)
{
    int i;
    struct rt_rhc_internal *rip;

    intern->idb_major_type = DB5_MAJORTYPE_BRLCAD;
    intern->idb_type = ID_RHC;
    intern->idb_meth = &rt_functab[ID_RHC];
    intern->idb_ptr = (genptr_t)bu_malloc(sizeof(struct rt_rhc_internal),
					  "rt_rhc_internal");
    rip = (struct rt_rhc_internal *)intern->idb_ptr;
    rip->rhc_magic = RT_RHC_INTERNAL_MAGIC;

    for (i = 0; i < ELEMENTS_PER_POINT; i++) {
	rip->rhc_V[i] = atof(cmd_argvs[3+i]) * gedp->ged_wdbp->dbip->dbi_local2base;
	rip->rhc_H[i] = atof(cmd_argvs[6+i]) * gedp->ged_wdbp->dbip->dbi_local2base;
	rip->rhc_B[i] = atof(cmd_argvs[9+i]) * gedp->ged_wdbp->dbip->dbi_local2base;
    }
    rip->rhc_r = atof(cmd_argvs[12]) * gedp->ged_wdbp->dbip->dbi_local2base;
    rip->rhc_c = atof(cmd_argvs[13]) * gedp->ged_wdbp->dbip->dbi_local2base;

    if (MAGNITUDE(rip->rhc_H) < RT_LEN_TOL
	|| MAGNITUDE(rip->rhc_B) < RT_LEN_TOL
	|| rip->rhc_r <= RT_LEN_TOL || rip->rhc_c <= RT_LEN_TOL) {
	bu_vls_printf(gedp->ged_result_str, "ERROR, height, breadth, and width must be greater than zero!\n");
	return GED_ERROR;
    }

    return GED_OK;
}


/* E P A _ I N () :   	reads epa parameters from keyboard
 * returns 0 if successful read
 * 1 if unsuccessful read
 */
static int
epa_in(struct ged *gedp, const char **cmd_argvs, struct rt_db_internal *intern)
{
    int i;
    struct rt_epa_internal *rip;

    intern->idb_major_type = DB5_MAJORTYPE_BRLCAD;
    intern->idb_type = ID_EPA;
    intern->idb_meth = &rt_functab[ID_EPA];
    intern->idb_ptr = (genptr_t)bu_malloc(sizeof(struct rt_epa_internal),
					  "rt_epa_internal");
    rip = (struct rt_epa_internal *)intern->idb_ptr;
    rip->epa_magic = RT_EPA_INTERNAL_MAGIC;

    for (i = 0; i < ELEMENTS_PER_POINT; i++) {
	rip->epa_V[i] = atof(cmd_argvs[3+i]) * gedp->ged_wdbp->dbip->dbi_local2base;
	rip->epa_H[i] = atof(cmd_argvs[6+i]) * gedp->ged_wdbp->dbip->dbi_local2base;
	rip->epa_Au[i] = atof(cmd_argvs[9+i]) * gedp->ged_wdbp->dbip->dbi_local2base;
    }
    rip->epa_r1 = MAGNITUDE(rip->epa_Au);
    rip->epa_r2 = atof(cmd_argvs[12]) * gedp->ged_wdbp->dbip->dbi_local2base;
    VUNITIZE(rip->epa_Au);

    if (MAGNITUDE(rip->epa_H) < RT_LEN_TOL
	|| rip->epa_r1 <= RT_LEN_TOL || rip->epa_r2 <= RT_LEN_TOL) {
	bu_vls_printf(gedp->ged_result_str, "ERROR, height and axes must be greater than zero!\n");
	return GED_ERROR;
    }

    if (rip->epa_r2 > rip->epa_r1) {
	bu_vls_printf(gedp->ged_result_str, "ERROR, |A| must be greater than |B|!\n");
	return GED_ERROR;
    }

    return GED_OK;
}


/* E H Y _ I N () :   	reads ehy parameters from keyboard
 * returns 0 if successful read
 * 1 if unsuccessful read
 */
static int
ehy_in(struct ged *gedp, const char **cmd_argvs, struct rt_db_internal *intern)
{
    int i;
    struct rt_ehy_internal *rip;

    intern->idb_major_type = DB5_MAJORTYPE_BRLCAD;
    intern->idb_type = ID_EHY;
    intern->idb_meth = &rt_functab[ID_EHY];
    intern->idb_ptr = (genptr_t)bu_malloc(sizeof(struct rt_ehy_internal),
					  "rt_ehy_internal");
    rip = (struct rt_ehy_internal *)intern->idb_ptr;
    rip->ehy_magic = RT_EHY_INTERNAL_MAGIC;

    for (i = 0; i < ELEMENTS_PER_POINT; i++) {
	rip->ehy_V[i] = atof(cmd_argvs[3+i]) * gedp->ged_wdbp->dbip->dbi_local2base;
	rip->ehy_H[i] = atof(cmd_argvs[6+i]) * gedp->ged_wdbp->dbip->dbi_local2base;
	rip->ehy_Au[i] = atof(cmd_argvs[9+i]) * gedp->ged_wdbp->dbip->dbi_local2base;
    }
    rip->ehy_r1 = MAGNITUDE(rip->ehy_Au);
    rip->ehy_r2 = atof(cmd_argvs[12]) * gedp->ged_wdbp->dbip->dbi_local2base;
    rip->ehy_c = atof(cmd_argvs[13]) * gedp->ged_wdbp->dbip->dbi_local2base;
    VUNITIZE(rip->ehy_Au);

    if (MAGNITUDE(rip->ehy_H) < RT_LEN_TOL
	|| rip->ehy_r1 <= RT_LEN_TOL || rip->ehy_r2 <= RT_LEN_TOL
	|| rip->ehy_c <= RT_LEN_TOL) {
	bu_vls_printf(gedp->ged_result_str, "ERROR, height, axes, and distance to asymptotes must be greater than zero!\n");
	return GED_ERROR;
    }

    if (!NEAR_ZERO(VDOT(rip->ehy_H, rip->ehy_Au), RT_DOT_TOL)) {
	bu_vls_printf(gedp->ged_result_str, "ERROR, major axis must be perpendicular to height vector!\n");
	return GED_ERROR;
    }

    if (rip->ehy_r2 > rip->ehy_r1) {
	bu_vls_printf(gedp->ged_result_str, "ERROR, |A| must be greater than |B|!\n");
	return GED_ERROR;
    }

    return GED_OK;
}


/* H Y P _ I N () :   	reads hyp parameters from keyboard
 * returns 0 if successful read
 * 1 if unsuccessful read
 */
static int
hyp_in(struct ged *gedp, const char **cmd_argvs, struct rt_db_internal *intern)
{
    int i;
    struct rt_hyp_internal *rip;
    vect_t inH, inAu;
    point_t inV;
    fastf_t inB, inC;

    intern->idb_major_type = DB5_MAJORTYPE_BRLCAD;
    intern->idb_type = ID_HYP;
    intern->idb_meth = &rt_functab[ID_HYP];
    intern->idb_ptr = (genptr_t)bu_malloc(sizeof(struct rt_hyp_internal),
					  "rt_hyp_internal");
    rip = (struct rt_hyp_internal *)intern->idb_ptr;
    rip->hyp_magic = RT_HYP_INTERNAL_MAGIC;

    for (i = 0; i < ELEMENTS_PER_POINT; i++) {
	inV[i] = atof(cmd_argvs[3+i]) * gedp->ged_wdbp->dbip->dbi_local2base;
	inH[i] = atof(cmd_argvs[6+i]) * gedp->ged_wdbp->dbip->dbi_local2base;
	inAu[i] = atof(cmd_argvs[9+i]) * gedp->ged_wdbp->dbip->dbi_local2base;
    }
    inB = atof(cmd_argvs[12]) * gedp->ged_wdbp->dbip->dbi_local2base;
    inC = atof(cmd_argvs[13]) * gedp->ged_wdbp->dbip->dbi_local2base;

    rip->hyp_b = inB;
    rip->hyp_bnr = inC;
    VMOVE(rip->hyp_Hi, inH);
    VMOVE(rip->hyp_Vi, inV);
    VMOVE(rip->hyp_A, inAu);

    if (rip->hyp_b > MAGNITUDE(rip->hyp_A)) {
	vect_t majorAxis;
	fastf_t minorLen;

	minorLen = MAGNITUDE(rip->hyp_A);
	VCROSS(majorAxis, rip->hyp_Hi, rip->hyp_A);
	VSCALE(rip->hyp_A, majorAxis, rip->hyp_b);
	rip->hyp_b = minorLen;
    }


    if (MAGNITUDE(rip->hyp_Hi)*0.5 < RT_LEN_TOL
	|| MAGNITUDE(rip->hyp_A) * rip->hyp_bnr <= RT_LEN_TOL
	|| rip->hyp_b <= RT_LEN_TOL) {
	bu_vls_printf(gedp->ged_result_str, "ERROR, height, axes, and distance to asymptotes must be greater than zero!\n");
	return GED_ERROR;
    }

    if (!NEAR_ZERO(VDOT(rip->hyp_Hi, rip->hyp_A), RT_DOT_TOL)) {
	bu_vls_printf(gedp->ged_result_str, "ERROR, major axis must be perpendicular to height vector!\n");
	return GED_ERROR;
    }

    if (inC >= 1 || inC <=0) {
	bu_vls_printf(gedp->ged_result_str, "ERROR, neck to base ratio must be between 0 and 1!\n");
	return GED_ERROR;

    }

    return GED_OK;
}


/* E T O _ I N () :   	reads eto parameters from keyboard
 * returns 0 if successful read
 * 1 if unsuccessful read
 */
static int
eto_in(struct ged *gedp, const char **cmd_argvs, struct rt_db_internal *intern)
{
    int i;
    struct rt_eto_internal *eip;

    intern->idb_major_type = DB5_MAJORTYPE_BRLCAD;
    intern->idb_type = ID_ETO;
    intern->idb_meth = &rt_functab[ID_ETO];
    intern->idb_ptr = (genptr_t)bu_malloc(sizeof(struct rt_eto_internal),
					  "rt_eto_internal");
    eip = (struct rt_eto_internal *)intern->idb_ptr;
    eip->eto_magic = RT_ETO_INTERNAL_MAGIC;

    for (i = 0; i < ELEMENTS_PER_POINT; i++) {
	eip->eto_V[i] = atof(cmd_argvs[3+i]) * gedp->ged_wdbp->dbip->dbi_local2base;
	eip->eto_N[i] = atof(cmd_argvs[6+i]) * gedp->ged_wdbp->dbip->dbi_local2base;
	eip->eto_C[i] = atof(cmd_argvs[10+i]) * gedp->ged_wdbp->dbip->dbi_local2base;
    }
    eip->eto_r = atof(cmd_argvs[9]) * gedp->ged_wdbp->dbip->dbi_local2base;
    eip->eto_rd = atof(cmd_argvs[13]) * gedp->ged_wdbp->dbip->dbi_local2base;

    if (MAGNITUDE(eip->eto_N) < RT_LEN_TOL
	|| MAGNITUDE(eip->eto_C) < RT_LEN_TOL
	|| eip->eto_r <= RT_LEN_TOL || eip->eto_rd <= RT_LEN_TOL) {
	bu_vls_printf(gedp->ged_result_str, "ERROR, normal, axes, and radii must be greater than zero!\n");
	return GED_ERROR;
    }

    if (eip->eto_rd > MAGNITUDE(eip->eto_C)) {
	bu_vls_printf(gedp->ged_result_str, "ERROR, |C| must be greater than |D|!\n");
	return GED_ERROR;
    }

    return GED_OK;
}


/* E X T R U D E _ I N () :   	reads extrude parameters from keyboard
 * returns 0 if successful read
 * 1 if unsuccessful read
 */
static int
extrude_in(struct ged *gedp, const char **cmd_argvs, struct rt_db_internal *intern)
{
    int i;
    struct rt_extrude_internal *eip;
    struct rt_db_internal tmp_ip;
    struct directory *dp;

    intern->idb_major_type = DB5_MAJORTYPE_BRLCAD;
    intern->idb_type = ID_EXTRUDE;
    intern->idb_meth = &rt_functab[ID_EXTRUDE];
    intern->idb_ptr = (genptr_t)bu_malloc(sizeof(struct rt_extrude_internal),
					  "rt_extrude_internal");
    eip = (struct rt_extrude_internal *)intern->idb_ptr;
    eip->magic = RT_EXTRUDE_INTERNAL_MAGIC;

    for (i = 0; i < ELEMENTS_PER_POINT; i++) {
	eip->V[i] = atof(cmd_argvs[3+i]) * gedp->ged_wdbp->dbip->dbi_local2base;
	eip->h[i] = atof(cmd_argvs[6+i]) * gedp->ged_wdbp->dbip->dbi_local2base;
	eip->u_vec[i] = atof(cmd_argvs[9+i]) * gedp->ged_wdbp->dbip->dbi_local2base;
	eip->v_vec[i] = atof(cmd_argvs[12+i]) * gedp->ged_wdbp->dbip->dbi_local2base;
    }
    eip->sketch_name = bu_strdup(cmd_argvs[15]);
    /* eip->keypoint = atoi(cmd_argvs[16]); */

    if ((dp=db_lookup(gedp->ged_wdbp->dbip, eip->sketch_name, LOOKUP_NOISY)) == RT_DIR_NULL) {
	bu_vls_printf(gedp->ged_result_str, "Cannot find sketch (%s) for extrusion (%s)\n",
		      eip->sketch_name, cmd_argvs[1]);
	eip->skt = (struct rt_sketch_internal *)NULL;
	return GED_ERROR;
    }

    if (rt_db_get_internal(&tmp_ip, dp, gedp->ged_wdbp->dbip, bn_mat_identity, &rt_uniresource) != ID_SKETCH) {
	bu_vls_printf(gedp->ged_result_str, "Cannot import sketch (%s) for extrusion (%s)\n",
		      eip->sketch_name, cmd_argvs[1]);
	eip->skt = (struct rt_sketch_internal *)NULL;
	return GED_ERROR;
    } else
	eip->skt = (struct rt_sketch_internal *)tmp_ip.idb_ptr;

    return GED_OK;
}


/* R E V O L V E _ I N () :   	reads extrude parameters from keyboard
 * returns 0 if successful read
 * 1 if unsuccessful read
 */
static int
revolve_in(struct ged *gedp, const char **cmd_argvs, struct rt_db_internal *intern)
{
    int i;
    struct rt_revolve_internal *rip;
    struct rt_db_internal tmp_ip;
    struct directory *dp;

    intern->idb_major_type = DB5_MAJORTYPE_BRLCAD;
    intern->idb_type = ID_REVOLVE;
    intern->idb_meth = &rt_functab[ID_REVOLVE];
    intern->idb_ptr = (genptr_t)bu_calloc(1, sizeof(struct rt_revolve_internal),
					  "rt_revolve_internal");
    rip = (struct rt_revolve_internal *)intern->idb_ptr;
    rip->magic = RT_REVOLVE_INTERNAL_MAGIC;

    for (i = 0; i < ELEMENTS_PER_POINT; i++) {
	rip->v3d[i] = atof(cmd_argvs[3+i]) * gedp->ged_wdbp->dbip->dbi_local2base;
	rip->axis3d[i] = atof(cmd_argvs[6+i]) * gedp->ged_wdbp->dbip->dbi_local2base;
	rip->r[i] = atof(cmd_argvs[9+i]) * gedp->ged_wdbp->dbip->dbi_local2base;
    }
    rip->ang = atof(cmd_argvs[12]) * DEG2RAD;

    bu_vls_init(&rip->sketch_name);
    bu_vls_strcpy(&rip->sketch_name, cmd_argvs[13]);

    VUNITIZE(rip->r);
    VUNITIZE(rip->axis3d);

    if ((dp=db_lookup(gedp->ged_wdbp->dbip, bu_vls_addr(&rip->sketch_name), LOOKUP_NOISY)) == RT_DIR_NULL) {
	bu_vls_printf(gedp->ged_result_str, "Cannot find sketch (%s) for revolve (%s)\n",
		      bu_vls_addr(&rip->sketch_name), cmd_argvs[1]);
	rip->sk = (struct rt_sketch_internal *)NULL;
	return GED_ERROR;
    }

    if (rt_db_get_internal(&tmp_ip, dp, gedp->ged_wdbp->dbip, bn_mat_identity, &rt_uniresource) != ID_SKETCH) {
	bu_vls_printf(gedp->ged_result_str, "Cannot import sketch (%s) for revolve (%s)\n",
		      bu_vls_addr(&rip->sketch_name), cmd_argvs[1]);
	rip->sk = (struct rt_sketch_internal *)NULL;
	return GED_ERROR;
    } else
	rip->sk = (struct rt_sketch_internal *)tmp_ip.idb_ptr;

    return GED_OK;
}


/* G R I P _ I N () :   	reads grip parameters from keyboard
 * returns 0 if successful read
 * 1 if unsuccessful read
 */
static int
grip_in(struct ged *gedp, const char **cmd_argvs, struct rt_db_internal *intern)
{
    int i;
    struct rt_grip_internal *gip;

    intern->idb_major_type = DB5_MAJORTYPE_BRLCAD;
    intern->idb_type = ID_GRIP;
    intern->idb_meth = &rt_functab[ID_GRIP];
    intern->idb_ptr = (genptr_t)bu_malloc(sizeof(struct rt_grip_internal),
					  "rt_grip_internal");
    gip = (struct rt_grip_internal *)intern->idb_ptr;
    gip->magic = RT_GRIP_INTERNAL_MAGIC;

    for (i = 0; i < ELEMENTS_PER_POINT; i++) {
	gip->center[i] = atof(cmd_argvs[3+i]) * gedp->ged_wdbp->dbip->dbi_local2base;
	gip->normal[i] = atof(cmd_argvs[6+i]) * gedp->ged_wdbp->dbip->dbi_local2base;
    }

    gip->mag = atof(cmd_argvs[9]);

    return GED_OK;
}


/* S U P E R E L L _ I N () :   	reads superell parameters from keyboard
 * returns 0 if successful read
 * 1 if unsuccessful read
 */
static int
superell_in(struct ged *gedp, char *cmd_argvs[], struct rt_db_internal *intern)
{
    fastf_t vals[14];
    int i, n;
    struct rt_superell_internal *eip;

    n = 14;				/* SUPERELL has 12 (same as ELL) + 2 (for <n, e>) params */

    intern->idb_type = ID_SUPERELL;
    intern->idb_meth = &rt_functab[ID_SUPERELL];
    intern->idb_ptr = (genptr_t)bu_malloc(sizeof(struct rt_superell_internal),
					  "rt_superell_internal");
    eip = (struct rt_superell_internal *)intern->idb_ptr;
    eip->magic = RT_SUPERELL_INTERNAL_MAGIC;

    /* convert typed in args to reals and convert to local units */
    for (i = 0; i < n - 2; i++) {
	vals[i] = atof(cmd_argvs[3+i]) * gedp->ged_wdbp->dbip->dbi_local2base;
    }
    vals[12] = atof(cmd_argvs[3 + 12]);
    vals[13] = atof(cmd_argvs[3 + 13]);

    /* V, A, B, C */
    VMOVE(eip->v, &vals[0]);
    VMOVE(eip->a, &vals[3]);
    VMOVE(eip->b, &vals[6]);
    VMOVE(eip->c, &vals[9]);
    eip->n = vals[12];
    eip->e = vals[13];

    return GED_OK;
}


/*
 * M E T A B A L L _ I N
 *
 * This is very much not reentrant, and probably a good deal uglier than it
 * should be.
 */
static int
metaball_in(struct ged *gedp, int argc, const char **argv, struct rt_db_internal *intern, char **prompt)
{
    struct rt_metaball_internal *metaball;
    static int i, num_points;
    static fastf_t threshold = -1.0;
    static long method = 0;

    if (argc < 4) {
	bu_vls_printf(gedp->ged_result_str, "%s", prompt[argc-3]);
	return GED_MORE;
    }
    method = atof(argv[3]);

    if (argc < 5) {
	bu_vls_printf(gedp->ged_result_str, "%s", prompt[argc-3]);
	return GED_MORE;
    }
    threshold = atof(argv[4]);

    if (threshold < 0.0) {
	bu_vls_printf(gedp->ged_result_str, "Threshold may not be negative.\n");
	return GED_ERROR;
    }

    if (argc < 6) {
	bu_vls_printf(gedp->ged_result_str, "%s", prompt[argc-3]);
	return GED_MORE;
    }

    num_points = atoi(argv[5]);
    if (num_points < 1) {
	bu_vls_printf(gedp->ged_result_str, "Invalid number of points (must be at least 1)\n");
	return GED_ERROR;
    }

    if (argc < 10) {
	bu_vls_printf(gedp->ged_result_str, "%s", prompt[argc-3]);
	return GED_MORE;
    }

    if (argc < 6 + num_points*4) {
	bu_vls_printf(gedp->ged_result_str, "%s for point %d : ", prompt[6+(argc-9)%4], 1+(argc-5)/4);
	return GED_MORE;
    }

    intern->idb_major_type = DB5_MAJORTYPE_BRLCAD;
    intern->idb_type = ID_METABALL;
    intern->idb_meth = &rt_functab[ID_METABALL];
    intern->idb_ptr = (genptr_t)bu_malloc(sizeof(struct rt_metaball_internal), "rt_metaball_internal");
    metaball = (struct rt_metaball_internal *)intern->idb_ptr;
    metaball->magic = RT_METABALL_INTERNAL_MAGIC;
    metaball->threshold = threshold;
    metaball->method = method;
    BU_LIST_INIT(&metaball->metaball_ctrl_head);

    /*
     * since we use args instead of the num_points, it's possible to have
     * MORE points than the value in the num_points field if it's all on one
     * line. Is that a bug, or a feature?
     */
    for (i=6; i<argc; i+= 4) {
	struct wdb_metaballpt *metaballpt;

	metaballpt = (struct wdb_metaballpt *)bu_malloc(sizeof(struct wdb_metaballpt), "wdb_metaballpt");
	metaballpt->coord[0] = atof(argv[i]) * gedp->ged_wdbp->dbip->dbi_local2base;
	metaballpt->coord[1] = atof(argv[i+1]) * gedp->ged_wdbp->dbip->dbi_local2base;
	metaballpt->coord[2] = atof(argv[i+2]) * gedp->ged_wdbp->dbip->dbi_local2base;
	metaballpt->fldstr = atof(argv[i+3]) * gedp->ged_wdbp->dbip->dbi_local2base;
	metaballpt->sweat = 1.0;

	BU_LIST_INSERT(&metaball->metaball_ctrl_head, &metaballpt->l);
    }

    return GED_OK;
}


/* P N T S _ I N */
static int
pnts_in(struct ged *gedp, int argc, const char **argv, struct rt_db_internal *intern, char **prompt) {
    unsigned long i;
    unsigned long numPoints;
    long readPoints;
    struct rt_pnts_internal *pnts;
    void *headPoint = NULL;

    rt_pnt_type type;

    int oriented = 0;
    int hasColor = 0;
    int hasScale = 0;
    double defaultSize = 0.0;

    int valuesPerPoint;
    int nextPrompt;

    double local2base = gedp->ged_wdbp->dbip->dbi_local2base;

    /* prompt if points file */
    if (argc < 4) {
	bu_vls_printf(gedp->ged_result_str, "%s", prompt[0]);
	return GED_MORE;
    }

    /* if points are in a file */
    if ((BU_STR_EQUAL(argv[3], "yes")) || (BU_STR_EQUAL(argv[3], "y"))) {

	/* prompt for point file path and name */
	if (argc < 5) {
	    bu_vls_printf(gedp->ged_result_str, "%s", prompt[16]);
	    return GED_MORE;
	}

	/* prompt for file data format */
	if (argc < 6) {
	    bu_vls_printf(gedp->ged_result_str, "%s", prompt[17]);
	    return GED_MORE;
	}

	/* prompt for file data units */
	if (argc < 7) {
	    bu_vls_printf(gedp->ged_result_str, "%s", prompt[18]);
	    return GED_MORE;
	}

	/* prompt for default point size */
	if (argc < 8) {
	    bu_vls_printf(gedp->ged_result_str, "%s", prompt[5]);
	    return GED_MORE;
	}

	/* call function(s) to validate 'point file data format string' and return the
	 * point-cloud type.
	 */

	/* call function(s) to validate the units string and return the converion factor to
	 * milimeters.
	 */

	/* call function(s) to read point cloud data and save into database.
	 */

	bu_log("The ability to create a pnts primitive from a data file is not yet implemented.\n");

	return GED_ERROR;

    } /* endif to process point data file */


    /* prompt for readPoints if not entered */
    if (argc < 5) {
	bu_vls_printf(gedp->ged_result_str, "%s", prompt[1]);
	return GED_MORE;
    }
    readPoints = atol(argv[4]);
    if (readPoints < 0) {
	/* negative means automatically figure out how many points */
	readPoints = -1;
    }

    /* prompt for orientation */
    if (argc < 6) {
	bu_vls_printf(gedp->ged_result_str, "%s", prompt[2]);
	return GED_MORE;
    }
    oriented = booleanize(argv[5]);

    /* prompt for color */
    if (argc < 7) {
	bu_vls_printf(gedp->ged_result_str, "%s", prompt[3]);
	return GED_MORE;
    }
    hasColor = booleanize(argv[6]);

    /* prompt for uniform scale */
    if (argc < 8) {
	bu_vls_printf(gedp->ged_result_str, "%s", prompt[4]);
	return GED_MORE;
    }
    hasScale = booleanize(argv[7]); /* has scale if not uniform */

    /* prompt for size of points if not entered */
    if (argc < 9) {
	bu_vls_printf(gedp->ged_result_str, "%s", prompt[5]);
	return GED_MORE;
    }
    defaultSize = atof(argv[8]);
    if (defaultSize < 0.0) {
	defaultSize = 0.0;
	bu_log("WARNING: default point size must be non-negative, using zero\n");
    }

    /* how many values are we expecting per point */
    valuesPerPoint = ELEMENTS_PER_POINT;
    type = RT_PNT_TYPE_PNT;
    if (hasColor) {
	/* R G B */
	type |= RT_PNT_TYPE_COL;
	valuesPerPoint += 3;
    }
    if (hasScale) {
	/* scale value */
	type |= RT_PNT_TYPE_SCA;
	valuesPerPoint += 1;
    }
    if (oriented) {
	/* vector */
	valuesPerPoint += ELEMENTS_PER_VECT;
	type |= RT_PNT_TYPE_NRM;
    }

    /* reset argc/argv to be just point data */
    argc -= 9;
    argv += 9;
    nextPrompt = argc % valuesPerPoint;

    /* determine the number of points */
    if (readPoints < 0) {
	/* determine count from argc */
	if (nextPrompt != 0) {
	    bu_log("WARNING: Data mismatch.\n"
		   "\tFound %d extra values after reading %d points.\n"
		   "\tExpecting %d values per point.\n"
		   "\tOnly using %d points.\n",
		   nextPrompt,
		   argc / valuesPerPoint,
		   valuesPerPoint,
		   argc / valuesPerPoint);
	}
	numPoints = argc / valuesPerPoint;
    } else {
	numPoints = (unsigned long)readPoints;
    }

    /* prompt for X, Y, Z of points */
    if ((unsigned long)argc < numPoints * (unsigned long)valuesPerPoint) {
	int nextAsk = nextPrompt + 6;
	struct bu_vls vls;
	bu_vls_init(&vls);

	switch (type) {
	    case RT_PNT_TYPE_PNT:
		/* do nothing, they're in order */
		break;
	    case RT_PNT_TYPE_COL:
		if (nextPrompt > 2) {
		    nextAsk += 3;
		}
		break;
	    case RT_PNT_TYPE_SCA:
		if (nextPrompt > 2) {
		    nextAsk += 6;
		}
		break;
	    case RT_PNT_TYPE_NRM:
		/* do nothing, they're in order */
		break;
	    case RT_PNT_TYPE_COL_SCA:
		if (nextPrompt > 2) {
		    nextAsk += 3;
		}
		break;
	    case RT_PNT_TYPE_COL_NRM:
		/* do nothing, they're in order */
		break;
	    case RT_PNT_TYPE_SCA_NRM:
		if (nextPrompt > 5) {
		    nextAsk += 3;
		}
		break;
	    case RT_PNT_TYPE_COL_SCA_NRM:
		/* do nothing, they're in order */
		break;
	}

	bu_vls_printf(&vls, "%s for point %d: ",
		      prompt[nextAsk],
		      (argc + valuesPerPoint) / valuesPerPoint);

	bu_vls_printf(gedp->ged_result_str, "%s", bu_vls_addr(&vls));

	bu_vls_free(&vls);
	return GED_MORE;
    }

    /* now we have everything we need to allocate an internal */

    /* init database structure */
    intern->idb_major_type = DB5_MAJORTYPE_BRLCAD;
    intern->idb_type = ID_PNTS;
    intern->idb_meth = &rt_functab[ID_PNTS];
    intern->idb_ptr = (genptr_t) bu_malloc(sizeof(struct rt_pnts_internal), "rt_pnts_internal");

    /* init internal structure */
    pnts = (struct rt_pnts_internal *) intern->idb_ptr;
    pnts->magic = RT_PNTS_INTERNAL_MAGIC;
    pnts->scale = defaultSize;
    pnts->type = type;
    pnts->count = numPoints;
    pnts->point = NULL;

    /* empty list head */
    switch (type) {
	case RT_PNT_TYPE_PNT:
	    BU_GETSTRUCT(headPoint, pnt);
	    BU_LIST_INIT(&(((struct pnt *)headPoint)->l));
	    break;
	case RT_PNT_TYPE_COL:
	    BU_GETSTRUCT(headPoint, pnt_color);
	    BU_LIST_INIT(&(((struct pnt_color *)headPoint)->l));
	    break;
	case RT_PNT_TYPE_SCA:
	    BU_GETSTRUCT(headPoint, pnt_scale);
	    BU_LIST_INIT(&(((struct pnt_scale *)headPoint)->l));
	    break;
	case RT_PNT_TYPE_NRM:
	    BU_GETSTRUCT(headPoint, pnt_normal);
	    BU_LIST_INIT(&(((struct pnt_normal *)headPoint)->l));
	    break;
	case RT_PNT_TYPE_COL_SCA:
	    BU_GETSTRUCT(headPoint, pnt_color_scale);
	    BU_LIST_INIT(&(((struct pnt_color_scale *)headPoint)->l));
	    break;
	case RT_PNT_TYPE_COL_NRM:
	    BU_GETSTRUCT(headPoint, pnt_color_normal);
	    BU_LIST_INIT(&(((struct pnt_color_normal *)headPoint)->l));
	    break;
	case RT_PNT_TYPE_SCA_NRM:
	    BU_GETSTRUCT(headPoint, pnt_scale_normal);
	    BU_LIST_INIT(&(((struct pnt_scale_normal *)headPoint)->l));
	    break;
	case RT_PNT_TYPE_COL_SCA_NRM:
	    BU_GETSTRUCT(headPoint, pnt_color_scale_normal);
	    BU_LIST_INIT(&(((struct pnt_color_scale_normal *)headPoint)->l));
	    break;
    }
    pnts->point = headPoint;

    /* store points in list */
    for (i = 0; i < numPoints * valuesPerPoint; i += valuesPerPoint) {
	void *point;

	/* bu_log("%d: [%s, %s, %s]\n", ((i-5)/3)+1, argv[i], argv[i+1], argv[i+2]); */
	switch (type) {
	    case RT_PNT_TYPE_PNT:
		BU_GETSTRUCT(point, pnt);
		((struct pnt *)point)->v[X] = strtod(argv[i + 0], NULL) * local2base;
		((struct pnt *)point)->v[Y] = strtod(argv[i + 1], NULL) * local2base;
		((struct pnt *)point)->v[Z] = strtod(argv[i + 2], NULL) * local2base;
		BU_LIST_PUSH(&(((struct pnt *)headPoint)->l), &((struct pnt *)point)->l);
		break;
	    case RT_PNT_TYPE_COL:
		BU_GETSTRUCT(point, pnt_color);
		((struct pnt_color *)point)->v[X] = strtod(argv[i + 0], NULL) * local2base;
		((struct pnt_color *)point)->v[Y] = strtod(argv[i + 1], NULL) * local2base;
		((struct pnt_color *)point)->v[Z] = strtod(argv[i + 2], NULL) * local2base;
		((struct pnt_color *)point)->c.buc_magic = BU_COLOR_MAGIC;
		((struct pnt_color *)point)->c.buc_rgb[0 /* RED */] = strtod(argv[i + 3], NULL);
		((struct pnt_color *)point)->c.buc_rgb[1 /* GRN */] = strtod(argv[i + 4], NULL);
		((struct pnt_color *)point)->c.buc_rgb[2 /* BLU */] = strtod(argv[i + 5], NULL);
		BU_LIST_PUSH(&(((struct pnt_color *)headPoint)->l), &((struct pnt_color *)point)->l);
		break;
	    case RT_PNT_TYPE_SCA:
		BU_GETSTRUCT(point, pnt_scale);
		((struct pnt_scale *)point)->v[X] = strtod(argv[i + 0], NULL) * local2base;
		((struct pnt_scale *)point)->v[Y] = strtod(argv[i + 1], NULL) * local2base;
		((struct pnt_scale *)point)->v[Z] = strtod(argv[i + 2], NULL) * local2base;
		((struct pnt_scale *)point)->s = strtod(argv[i + 3], NULL) * local2base;
		BU_LIST_PUSH(&(((struct pnt_scale *)headPoint)->l), &((struct pnt_scale *)point)->l);
		break;
	    case RT_PNT_TYPE_NRM:
		BU_GETSTRUCT(point, pnt_normal);
		((struct pnt_normal *)point)->v[X] = strtod(argv[i + 0], NULL) * local2base;
		((struct pnt_normal *)point)->v[Y] = strtod(argv[i + 1], NULL) * local2base;
		((struct pnt_normal *)point)->v[Z] = strtod(argv[i + 2], NULL) * local2base;
		((struct pnt_normal *)point)->n[X] = strtod(argv[i + 3], NULL) * local2base;
		((struct pnt_normal *)point)->n[Y] = strtod(argv[i + 4], NULL) * local2base;
		((struct pnt_normal *)point)->n[Z] = strtod(argv[i + 5], NULL) * local2base;
		BU_LIST_PUSH(&(((struct pnt_normal *)headPoint)->l), &((struct pnt_normal *)point)->l);
		break;
	    case RT_PNT_TYPE_COL_SCA:
		BU_GETSTRUCT(point, pnt_color_scale);
		((struct pnt_color_scale *)point)->v[X] = strtod(argv[i + 0], NULL) * local2base;
		((struct pnt_color_scale *)point)->v[Y] = strtod(argv[i + 1], NULL) * local2base;
		((struct pnt_color_scale *)point)->v[Z] = strtod(argv[i + 2], NULL) * local2base;
		((struct pnt_color_scale *)point)->c.buc_magic = BU_COLOR_MAGIC;
		((struct pnt_color_scale *)point)->c.buc_rgb[0 /* RED */] = strtod(argv[i + 3], NULL);
		((struct pnt_color_scale *)point)->c.buc_rgb[1 /* GRN */] = strtod(argv[i + 4], NULL);
		((struct pnt_color_scale *)point)->c.buc_rgb[2 /* BLU */] = strtod(argv[i + 5], NULL);
		((struct pnt_color_scale *)point)->s = strtod(argv[i + 6], NULL) * local2base;
		BU_LIST_PUSH(&(((struct pnt_color_scale *)headPoint)->l), &((struct pnt_color_scale *)point)->l);
		break;
	    case RT_PNT_TYPE_COL_NRM:
		BU_GETSTRUCT(point, pnt_color_normal);
		((struct pnt_color_normal *)point)->v[X] = strtod(argv[i + 0], NULL) * local2base;
		((struct pnt_color_normal *)point)->v[Y] = strtod(argv[i + 1], NULL) * local2base;
		((struct pnt_color_normal *)point)->v[Z] = strtod(argv[i + 2], NULL) * local2base;
		((struct pnt_color_normal *)point)->n[X] = strtod(argv[i + 3], NULL) * local2base;
		((struct pnt_color_normal *)point)->n[Y] = strtod(argv[i + 4], NULL) * local2base;
		((struct pnt_color_normal *)point)->n[Z] = strtod(argv[i + 5], NULL) * local2base;
		((struct pnt_color_normal *)point)->c.buc_magic = BU_COLOR_MAGIC;
		((struct pnt_color_normal *)point)->c.buc_rgb[0 /* RED */] = strtod(argv[i + 6], NULL);
		((struct pnt_color_normal *)point)->c.buc_rgb[1 /* GRN */] = strtod(argv[i + 7], NULL);
		((struct pnt_color_normal *)point)->c.buc_rgb[2 /* BLU */] = strtod(argv[i + 8], NULL);
		BU_LIST_PUSH(&(((struct pnt_color_normal *)headPoint)->l), &((struct pnt_color_normal *)point)->l);
		break;
	    case RT_PNT_TYPE_SCA_NRM:
		BU_GETSTRUCT(point, pnt_scale_normal);
		((struct pnt_scale_normal *)point)->v[X] = strtod(argv[i + 0], NULL) * local2base;
		((struct pnt_scale_normal *)point)->v[Y] = strtod(argv[i + 1], NULL) * local2base;
		((struct pnt_scale_normal *)point)->v[Z] = strtod(argv[i + 2], NULL) * local2base;
		((struct pnt_scale_normal *)point)->n[X] = strtod(argv[i + 3], NULL) * local2base;
		((struct pnt_scale_normal *)point)->n[Y] = strtod(argv[i + 4], NULL) * local2base;
		((struct pnt_scale_normal *)point)->n[Z] = strtod(argv[i + 5], NULL) * local2base;
		((struct pnt_scale_normal *)point)->s = strtod(argv[i + 6], NULL) * local2base;
		BU_LIST_PUSH(&(((struct pnt_scale_normal *)headPoint)->l), &((struct pnt_scale_normal *)point)->l);
		break;
	    case RT_PNT_TYPE_COL_SCA_NRM:
		BU_GETSTRUCT(point, pnt_color_scale_normal);
		((struct pnt_color_scale_normal *)point)->v[X] = strtod(argv[i + 0], NULL) * local2base;
		((struct pnt_color_scale_normal *)point)->v[Y] = strtod(argv[i + 1], NULL) * local2base;
		((struct pnt_color_scale_normal *)point)->v[Z] = strtod(argv[i + 2], NULL) * local2base;
		((struct pnt_color_scale_normal *)point)->n[X] = strtod(argv[i + 3], NULL) * local2base;
		((struct pnt_color_scale_normal *)point)->n[Y] = strtod(argv[i + 4], NULL) * local2base;
		((struct pnt_color_scale_normal *)point)->n[Z] = strtod(argv[i + 5], NULL) * local2base;
		((struct pnt_color_scale_normal *)point)->c.buc_magic = BU_COLOR_MAGIC;
		((struct pnt_color_scale_normal *)point)->c.buc_rgb[0 /* RED */] = strtod(argv[i + 6], NULL);
		((struct pnt_color_scale_normal *)point)->c.buc_rgb[1 /* GRN */] = strtod(argv[i + 7], NULL);
		((struct pnt_color_scale_normal *)point)->c.buc_rgb[2 /* BLU */] = strtod(argv[i + 8], NULL);
		((struct pnt_color_scale_normal *)point)->s = strtod(argv[i + 9], NULL) * local2base;
		BU_LIST_PUSH(&(((struct pnt_color_scale_normal *)headPoint)->l), &((struct pnt_color_scale_normal *)point)->l);
		break;
	}
    }

    return GED_OK;
}


int
ged_in(struct ged *gedp, int argc, const char *argv[])
{
    struct directory *dp;
    char *name;
    struct rt_db_internal internal;
    char **menu;
    int nvals, (*fn_in)();

    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_READ_ONLY(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* Get the name of the solid to be created */
    if (argc < 2) {
	bu_vls_printf(gedp->ged_result_str, "Enter name of solid: ");
	return GED_MORE;
    }
    if (db_lookup(gedp->ged_wdbp->dbip,  argv[1], LOOKUP_QUIET) != RT_DIR_NULL) {
	bu_vls_printf(gedp->ged_result_str, "%s: %s already exists", argv[0], argv[1]);
	return GED_ERROR;
    }
    if (db_version(gedp->ged_wdbp->dbip) < 5 && (int)strlen(argv[1]) > NAMESIZE) {
	bu_vls_printf(gedp->ged_result_str, "%s: ERROR, v4 names are limited to %d characters\n", argv[0], NAMESIZE);
	return GED_ERROR;
    }
    /* Save the solid name */
    name = (char *)argv[1];

    /* Get the solid type to be created and make it */
    if (argc < 3) {
	bu_vls_printf(gedp->ged_result_str, "Enter solid type: ");
	return GED_MORE;
    }

    RT_DB_INTERNAL_INIT(&internal);

    /*
     * Decide which solid to make and get the rest of the args
     * make name <half|arb[4-8]|sph|ell|ellg|ell1|tor|tgc|tec|
     rec|trc|rcc|box|raw|rpp|rpc|rhc|epa|ehy|hyp|eto|superell>
    */
    if (BU_STR_EQUAL(argv[2], "ebm")) {
	nvals = 4;
	menu = p_ebm;
	fn_in = ebm_in;
    } else if (BU_STR_EQUAL(argv[2], "arbn")) {
	switch (arbn_in(gedp, argc, argv, &internal, &p_arbn[0])) {
	    case GED_ERROR:
		bu_vls_printf(gedp->ged_result_str, "%s: ERROR, ARBN not made!\n", argv[0]);
		rt_db_free_internal(&internal);
		return GED_ERROR;
	    case GED_MORE:
		return GED_MORE;
	}
	goto do_new_update;
    } else if (BU_STR_EQUAL(argv[2], "bot")) {
	switch (bot_in(gedp, argc, argv, &internal, &p_bot[0])) {
	    case GED_ERROR:
		bu_vls_printf(gedp->ged_result_str, "%s: ERROR, BOT not made!\n", argv[0]);
		rt_db_free_internal(&internal);
		return GED_ERROR;
	    case GED_MORE:
		return GED_MORE;
	}
	goto do_new_update;
    } else if (BU_STR_EQUAL(argv[2], "submodel")) {
	nvals = 3;
	menu = p_submodel;
	fn_in = submodel_in;
    } else if (BU_STR_EQUAL(argv[2], "vol")) {
	nvals = 9;
	menu = p_vol;
	fn_in = vol_in;
    } else if (BU_STR_EQUAL(argv[2], "hf")) {
	if (db_version(gedp->ged_wdbp->dbip) < 5) {
	    nvals = 19;
	    menu = p_hf;
	    fn_in = hf_in;
	    bu_vls_printf(gedp->ged_result_str, "%s: the height field is deprecated. Use the dsp primitive.\n", argv[0]);
	} else {
	    bu_vls_printf(gedp->ged_result_str, "%s: the height field is deprecated. Use the dsp primitive.\n", argv[0]);
	    return GED_ERROR;
	}
    } else if (BU_STR_EQUAL(argv[2], "poly") ||
	       BU_STR_EQUAL(argv[2], "pg")) {
	bu_vls_printf(gedp->ged_result_str, "%s: the polysolid is deprecated and not supported by this command.\nUse the bot primitive.\n", argv[0]);
	return GED_ERROR;
    } else if (BU_STR_EQUAL(argv[2], "dsp")) {
	if (db_version(gedp->ged_wdbp->dbip) < 5) {
	    nvals = 6;
	    menu = p_dsp_v4;
	    fn_in = dsp_in_v4;
	} else {
	    nvals = 8;
	    menu = p_dsp_v5;
	    fn_in = dsp_in_v5;
	}

    } else if (BU_STR_EQUAL(argv[2], "pipe")) {
	switch (pipe_in(gedp, argc, argv, &internal, &p_pipe[0])) {
	    case GED_ERROR:
		bu_vls_printf(gedp->ged_result_str, "%s: ERROR, pipe not made!\n", argv[0]);
		rt_db_free_internal(&internal);
		return GED_ERROR;
	    case GED_MORE:
		return GED_MORE;
	}
	goto do_new_update;
    } else if (BU_STR_EQUAL(argv[2], "metaball")) {
	switch (metaball_in(gedp, argc, argv, &internal, &p_metaball[0])) {
	    case GED_ERROR:
		bu_vls_printf(gedp->ged_result_str, "%s: ERROR, metaball not made!\n", argv[0]);
		rt_db_free_internal(&internal);
		return GED_ERROR;
	    case GED_MORE:
		return GED_MORE;
	}
	goto do_new_update;
    } else if (BU_STR_EQUAL(argv[2], "ars")) {
	switch (ars_in(gedp, argc, argv, &internal, &p_ars[0])) {
	    case GED_ERROR:
		bu_vls_printf(gedp->ged_result_str, "%s: ERROR, ars not made!\n", argv[0]);
		rt_db_free_internal(&internal);
		return GED_ERROR;
	    case GED_MORE:
		return GED_MORE;
	}
	goto do_new_update;
    } else if (BU_STR_EQUAL(argv[2], "half")) {
	nvals = 3*1 + 1;
	menu = p_half;
	fn_in = half_in;
    } else if (strncmp(argv[2], "arb", 3) == 0) {
	int n = atoi(&argv[2][3]);

	if (n < 4 || 8 < n) {
	    bu_vls_printf(gedp->ged_result_str,
			  "%s: ERROR: %s not supported!\nsupported arbs: arb4 arb5 arb6 arb7 arb8\n",
			  argv[0], argv[2]);
	    return GED_ERROR;
	}

	nvals = 3*n;
	menu = p_arb;
	fn_in = arb_in;
    } else if (BU_STR_EQUAL(argv[2], "sph")) {
	nvals = 3*1 + 1;
	menu = p_sph;
	fn_in = sph_in;
    } else if (BU_STR_EQUAL(argv[2], "ellg")) {
	nvals = 3*2 + 1;
	menu = p_ellg;
	fn_in = ell_in;
    } else if (BU_STR_EQUAL(argv[2], "ell")) {
	nvals = 3*4;
	menu = p_ell;
	fn_in = ell_in;
    } else if (BU_STR_EQUAL(argv[2], "ell1")) {
	nvals = 3*2 + 1;
	menu = p_ell1;
	fn_in = ell_in;
    } else if (BU_STR_EQUAL(argv[2], "tor")) {
	nvals = 3*2 + 2;
	menu = p_tor;
	fn_in = tor_in;
    } else if (BU_STR_EQUAL(argv[2], "tgc")) {
	nvals = 3*4 + 2;
	menu = p_tgc;
	fn_in = tgc_in;
    } else if (BU_STR_EQUAL(argv[2], "tec")) {
	nvals = 3*4 + 1;
	menu = p_tec;
	fn_in = tec_in;
    } else if (BU_STR_EQUAL(argv[2], "rec")) {
	nvals = 3*4;
	menu = p_rec;
	fn_in = rec_in;
    } else if (BU_STR_EQUAL(argv[2], "trc")) {
	nvals = 3*2 + 2;
	menu = p_trc;
	fn_in = trc_in;
    } else if (BU_STR_EQUAL(argv[2], "rcc")) {
	nvals = 3*2 + 1;
	menu = p_rcc;
	fn_in = rcc_in;
    } else if (BU_STR_EQUAL(argv[2], "box")
	       || BU_STR_EQUAL(argv[2], "raw")) {
	nvals = 3*4;
	menu = p_box;
	fn_in = box_in;
    } else if (BU_STR_EQUAL(argv[2], "rpp")) {
	nvals = 3*2;
	menu = p_rpp;
	fn_in = rpp_in;
    } else if (BU_STR_EQUAL(argv[2], "orpp")) {
	nvals = 3*1;
	menu = p_orpp;
	fn_in = orpp_in;
    } else if (BU_STR_EQUAL(argv[2], "rpc")) {
	nvals = 3*3 + 1;
	menu = p_rpc;
	fn_in = rpc_in;
    } else if (BU_STR_EQUAL(argv[2], "rhc")) {
	nvals = 3*3 + 2;
	menu = p_rhc;
	fn_in = rhc_in;
    } else if (BU_STR_EQUAL(argv[2], "epa")) {
	nvals = 3*3 + 1;
	menu = p_epa;
	fn_in = epa_in;
    } else if (BU_STR_EQUAL(argv[2], "ehy")) {
	nvals = 3*3 + 2;
	menu = p_ehy;
	fn_in = ehy_in;
    } else if (BU_STR_EQUAL(argv[2], "hyp")) {
	nvals = 3*3 + 2;
	menu = p_hyp;
	fn_in = hyp_in;
    } else if (BU_STR_EQUAL(argv[2], "eto")) {
	nvals = 3*3 + 2;
	menu = p_eto;
	fn_in = eto_in;
    } else if (BU_STR_EQUAL(argv[2], "part")) {
	nvals = 2*3 + 2;
	menu = p_part;
	fn_in = part_in;
    } else if (BU_STR_EQUAL(argv[2], "binunif")) {
	if (db_version(gedp->ged_wdbp->dbip) < 5) {
	    bu_vls_printf(gedp->ged_result_str,
			  "%s: the binunif primitive is not supported by this command when using an old style database",
			  argv[0]);
	    return GED_ERROR;
	} else {
	    nvals = 3;
	    menu = p_binunif;
	    fn_in = binunif_in;
	}
    } else if (BU_STR_EQUAL(argv[2], "extrude")) {
	nvals = 4*3 + 1;
	menu = p_extrude;
	fn_in = extrude_in;
    } else if (BU_STR_EQUAL(argv[2], "revolve")) {
	nvals = 3*3 + 2;
	menu = p_revolve;
	fn_in = revolve_in;
    } else if (BU_STR_EQUAL(argv[2], "grip")) {
	nvals = 2*3 + 1;
	menu = p_grip;
	fn_in = grip_in;
    } else if (BU_STR_EQUAL(argv[2], "superell")) {
	nvals = 3*4 + 2;
	menu = p_superell;
	fn_in = superell_in;
    } else if (BU_STR_EQUAL(argv[2], "pnts")) {
	switch (pnts_in(gedp, argc, argv, &internal, p_pnts)) {
	    case GED_ERROR:
		bu_vls_printf(gedp->ged_result_str, "%s: ERROR, pnts not made!\n", argv[0]);
		rt_db_free_internal(&internal);
		return GED_ERROR;
	    case GED_MORE:
		return GED_MORE;
	}

	goto do_new_update;
    } else if (BU_STR_EQUAL(argv[2], "cline") ||
	       BU_STR_EQUAL(argv[2], "grip") ||
	       BU_STR_EQUAL(argv[2], "nmg") ||
	       BU_STR_EQUAL(argv[2], "nurb") ||
	       BU_STR_EQUAL(argv[2], "sketch") ||
	       BU_STR_EQUAL(argv[2], "spline")) {
	bu_vls_printf(gedp->ged_result_str, "%s: the %s primitive is not supported by this command", argv[0], argv[2]);
	return GED_ERROR;
    } else {
	bu_vls_printf(gedp->ged_result_str, "%s: %s is not a known primitive\n", argv[0], argv[2]);
	return GED_ERROR;
    }

    /* Read arguments */
    if (argc < 3+nvals) {
	bu_vls_printf(gedp->ged_result_str, "%s", menu[argc-3]);
	return GED_MORE;
    }

    if (fn_in(gedp, argv, &internal, name) != 0) {
	bu_vls_printf(gedp->ged_result_str, "%s: ERROR %s not made!\n", argv[0], argv[2]);
	if (internal.idb_ptr) {
	    /* a few input functions do not use the internal pointer
	     * only free it, if it has been used
	     */
	    rt_db_free_internal(&internal);
	}
	return GED_ERROR;
    }

do_new_update:
    /* The function may have already written via LIBWDB */
    if (internal.idb_ptr != NULL) {
	dp=db_diradd(gedp->ged_wdbp->dbip, name, RT_DIR_PHONY_ADDR, 0, RT_DIR_SOLID, (genptr_t)&internal.idb_type);
	if (dp == RT_DIR_NULL) {
	    rt_db_free_internal(&internal);
	    bu_vls_printf(gedp->ged_result_str, "%s: Cannot add '%s' to directory\n", argv[0], name);
	    return GED_ERROR;
	}
	if (rt_db_put_internal(dp, gedp->ged_wdbp->dbip, &internal, &rt_uniresource) < 0) {
	    rt_db_free_internal(&internal);
	    bu_vls_printf(gedp->ged_result_str, "%s: Database write error, aborting\n", argv[0]);
	    return GED_ERROR;
	}
    }

    bu_vls_printf(gedp->ged_result_str, "%s", argv[1]);
    return GED_OK;
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
