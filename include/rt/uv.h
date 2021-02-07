/*                            U V . H
 * BRL-CAD
 *
 * Copyright (c) 2019-2021 United States Government as represented by
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
/** @file uv.h
 *
 * This file is API pertaining to uv parameterization on objects.
 *
 */


#ifndef RT_UV_H
#define RT_UV_H


#include "common.h"

#include "bu/vls.h"
#include "bu/mapped_file.h"
#include "vmath.h"
#include "rt/geom.h"
#include "rt/functab.h"


__BEGIN_DECLS


/*
 * // Setup:
 *
 * RT_APPLICATION_INIT();
 * rt_dirbuild();
 * rt_gettree();
 * rt_prep();
 * rt_texture_load(); // load texture image(s)
 *
 * // Shoot:
 *
 * rt_shootray();
 *
 * // Lookup:
 *
 * RT_HIT_UV(); // get UV-coordinate
 * rt_texture_lookup(); // get corresponding data
 *
 */


/**
 * This is a ray tracing texture image context.  Basically, it's a
 * handle to an image stored in a file or in a database object with
 * additional parameters specific to a given ray tracing use such as
 * scaling and mirroring.
 *
 * TODO: replace txt_specific in liboptical
 */
struct rt_texture {
    struct bu_vls tx_name;  /* name of object or file (depending on tx_datasrc flag) */
    int tx_w;		/* Width of texture in pixels */
    int tx_n;		/* Number of scanlines */
    int tx_trans_valid;	/* boolean: is tx_transp valid ? */
    int tx_transp[3];	/* RGB for transparency */
    fastf_t tx_scale[2];	/* replication factors in U, V */
    int tx_mirror;	/* flag: repetitions are mirrored */
#define TXT_SRC_FILE 'f'
#define TXT_SRC_OBJECT 'o'
#define TXT_SRC_AUTO 0
    char tx_datasrc; /* which type of datasource */

    /* internal state */
    struct rt_binunif_internal *tx_binunifp;  /* db internal object when TXT_SRC_OBJECT */
    struct bu_mapped_file *tx_mp;    /* mapped file when TXT_SRC_FILE */
};


/**
 * initialize an rt_texture to zero
 */
#define RT_TEXTURE_INIT_ZERO {{0,0,0}, BU_VLS_INIT_ZERO, 0, 0, 0, {0.0, 0.0}, 0, 0, NULL, NULL}


/**
 * loads a texture from either a file or database object.
 *
 * TODO: replace txt_load_datasource in liboptical
 */
RT_EXPORT int
rt_texture_load(struct rt_texture *tp, const char *name, struct db_i *dbip);


/**
 * As rt_shootray() only calculates hit points and returns a list of
 * partitions, applications must request that the corresponding UV
 * coordinate be computed via: RT_HIT_UV(NULL, hitp, stp, rayp, 0);
 *
 * These calculations are deferred to user code to avoid needless
 * computation in other ray situations.
 */
#define RT_HIT_UV(_ap, _stp, _hitp, _uvp) { \
	RT_CK_HIT(_hitp); \
	RT_CK_SOLTAB(_stp); \
	RT_CK_FUNCTAB((_stp)->st_meth); \
	if ((_stp)->st_meth->ft_uv) { \
	    (_stp)->st_meth->ft_uv(ap, _stp, _hitp, uvp); \
	} \
    }


/* TODO: move from liboptical */
struct uvcoord;


/**
 * Given a uv coordinate (0.0 <= u, v <= 1.0) and a texture, return a
 * pointer to the corresponding texture data (as a fastf_t[3]).
 *
 * TODO: replace txt_render in liboptical
 */
RT_EXPORT int
rt_texture_lookup(fastf_t *data, const struct rt_texture *tp, const struct uvcoord *uvp);


__END_DECLS


#endif


/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
