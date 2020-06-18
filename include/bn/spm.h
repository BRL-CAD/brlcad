/*                           S P M . H
 * BRL-CAD
 *
 * Copyright (c) 1986-2020 United States Government as represented by
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
/** @addtogroup bn_sphmap
 *
 * @brief Sphere data structure and function declarations.  Provides
 * spherical data structures for texture mapping.
 *
 */
/** @{ */
/** @file spm.h */

#ifndef BN_SPM_H
#define BN_SPM_H

#include "common.h"
#include "bn/defines.h"

typedef struct {
    uint32_t magic;
    int ny;		/**< @brief Number of "y" bins */
    int *nx;		/**< @brief Number of "x" bins per "y" bin */
    int elsize;		/**< @brief Size of each bin element */
    unsigned char **xbin;	/**< @brief staring addresses of "x" bins */
    unsigned char *_data;	/**< @brief For freeing purposes, start of data */
} bn_spm_map_t;

#define BN_SPM_MAP_NULL (bn_spm_map_t *)0
#define BN_CK_SPM_MAP(_p) BU_CKMAG(_p, BN_SPM_MAGIC, "bn_spm_map_t")

__BEGIN_DECLS

/**
 *@brief
 * Return a sphere map structure initialized for N points around the
 * equator.
 *
 * Malloc the storage and fill in the pointers.  This code leaves a
 * ring of "triangular" pixels at the poles.  An alternative would be
 * to have the pole region map to a single pixel.
 *
 * Returns BN_SPM_NULL on error.
 */
BN_EXPORT extern bn_spm_map_t *bn_spm_init(int N, int elsize);

/**
 *@brief
 * Free the storage associated with a sphere structure.
 */
BN_EXPORT extern void bn_spm_free(bn_spm_map_t *mp);

/**
 *@brief
 * Read the value of the pixel at the given normalized (u, v)
 * coordinates.  It does NOT check the sanity of the coords.
 *
 *@n 0.0 <= u < 1.0 Left to Right
 *@n 0.0 <= v < 1.0 Bottom to Top
 */
BN_EXPORT extern void bn_spm_read(register bn_spm_map_t *mapp, register unsigned char *valp, double u, double v);

/**
 *@brief
 * Write the value of the pixel at the given normalized (u, v)
 * coordinates.  It does NOT check the sanity of the coords.
 *
 *@n 0.0 <= u < 1.0 Left to Right
 *@n 0.0 <= v < 1.0 Bottom to Top
 */
BN_EXPORT extern void bn_spm_write(register bn_spm_map_t *mapp, register unsigned char *valp, double u, double v);

/**
 *@brief
 * Return a pointer to the storage element indexed by (u, v)
 * coordinates.  It does NOT check the sanity of the coords.
 *
 *@n 0.0 <= u < 1.0 Left to Right
 *@n 0.0 <= v < 1.0 Bottom to Top
 */
BN_EXPORT extern char *bn_spm_get(register bn_spm_map_t *mapp, double u, double v);

/**
 *@brief
 * Read a saved sphere map from a file ("-" for stdin) into the given
 * map structure.  This does not check for conformity of size, etc.
 *
 * @return -1 on error, else 0.
 */
BN_EXPORT extern int bn_spm_load(bn_spm_map_t *mapp, char *filename);

/**
 *@brief
 * Write a loaded sphere map to the given file ("-" for stdout).
 * Returns -1 on error, else 0.
 */
BN_EXPORT extern int bn_spm_save(bn_spm_map_t *mapp, char *filename);

/**
 *@brief
 * Load an 'nx' by 'ny' pix file and filter it into the
 * given sphere structure.
 *
 * @return -1 on error, else 0.
 */
BN_EXPORT extern int bn_spm_pix_load(bn_spm_map_t *mapp, char *filename, int nx, int ny);

/**
 *@brief
 * Save a sphere structure as an 'nx' by 'ny' pix file.
 * @return -1 on error, else 0.
 */
BN_EXPORT extern int bn_spm_pix_save(bn_spm_map_t *mapp, char *filename, int nx, int ny);

/**
 *@brief
 * Display a sphere structure on stderr.  Used for debugging.
 */
BN_EXPORT extern void bn_spm_dump(bn_spm_map_t *mp, int verbose);

__END_DECLS

#endif /* BN_SPM_H */

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
