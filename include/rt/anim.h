/*                          A N I M . H
 * BRL-CAD
 *
 * Copyright (c) 1993-2022 United States Government as represented by
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
/** @file rt/anim.h
 *
 */

#ifndef RT_ANIM_H
#define RT_ANIM_H

#include "common.h"

/* system headers */
#include <stdio.h> /* for FILE */

/* interface headers */
#include "vmath.h"
#include "bu/vls.h"
#include "bu/file.h"
#include "rt/defines.h"
#include "rt/mater.h"
#include "rt/db_fullpath.h"

__BEGIN_DECLS

struct db_i; /* forward declaration */

/**
 * Each one of these structures specifies an arc in the tree that is
 * to be operated on for animation purposes.  More than one animation
 * operation may be applied at any given arc.  The directory structure
 * points to a linked list of animate structures (built by
 * rt_anim_add()), and the operations are processed in the order
 * given.
 */
struct anim_mat {
    int         anm_op;                 /**< @brief  ANM_RSTACK, ANM_RARC... */
    mat_t       anm_mat;                /**< @brief  Matrix */
};
#define ANM_RSTACK      1               /**< @brief  Replace stacked matrix */
#define ANM_RARC        2               /**< @brief  Replace arc matrix */
#define ANM_LMUL        3               /**< @brief  Left (root side) mul */
#define ANM_RMUL        4               /**< @brief  Right (leaf side) mul */
#define ANM_RBOTH       5               /**< @brief  Replace stack, arc=Idn */

struct rt_anim_property {
    uint32_t magic;
    int                 anp_op;         /**< @brief  RT_ANP_REPLACE, etc. */
    struct bu_vls       anp_shader;     /**< @brief  Update string */
};
#define RT_ANP_REPLACE  1               /**< @brief  Replace shader string */
#define RT_ANP_APPEND   2               /**< @brief  Append to shader string */
#define RT_CK_ANP(_p) BU_CKMAG((_p), RT_ANP_MAGIC, "rt_anim_property")

struct rt_anim_color {
    int anc_rgb[3];                     /**< @brief  New color */
};


struct animate {
    uint32_t            magic;          /**< @brief  magic number */
    struct animate *    an_forw;        /**< @brief  forward link */
    struct db_full_path an_path;        /**< @brief  (sub)-path pattern */
    int                 an_type;        /**< @brief  AN_MATRIX, AN_COLOR... */
    union animate_specific {
	struct anim_mat         anu_m;
	struct rt_anim_property anu_p;
	struct rt_anim_color    anu_c;
	float                   anu_t;
    } an_u;
};
#define RT_AN_MATRIX      1             /**< @brief  Matrix animation */
#define RT_AN_MATERIAL    2             /**< @brief  Material property anim */
#define RT_AN_COLOR       3             /**< @brief  Material color anim */
#define RT_AN_SOLID       4             /**< @brief  Solid parameter anim */
#define RT_AN_TEMPERATURE 5             /**< @brief  Region temperature */

#define ANIM_NULL       ((struct animate *)0)
#define RT_CK_ANIMATE(_p) BU_CKMAG((_p), ANIMATE_MAGIC, "animate")

__END_DECLS

/* db_anim.c */
RT_EXPORT extern struct animate *db_parse_1anim(struct db_i     *dbip,
						int             argc,
						const char      **argv);


/**
 * A common parser for mged and rt.  Experimental.  Not the best name
 * for this.
 */
RT_EXPORT extern int db_parse_anim(struct db_i     *dbip,
				   int             argc,
				   const char      **argv);

/**
 * Add a user-supplied animate structure to the end of the chain of
 * such structures hanging from the directory structure of the last
 * node of the path specifier.  When 'root' is non-zero, this matrix
 * is located at the root of the tree itself, rather than an arc, and
 * is stored differently.
 *
 * In the future, might want to check to make sure that callers
 * directory references are in the right database (dbip).
 */
RT_EXPORT extern int db_add_anim(struct db_i *dbip,
				 struct animate *anp,
				 int root);

/**
 * Perform the one animation operation.  Leave results in form that
 * additional operations can be cascaded.
 *
 * Note that 'materp' may be a null pointer, signifying that the
 * region has already been finalized above this point in the tree.
 */
RT_EXPORT extern int db_do_anim(struct animate *anp,
				mat_t stack,
				mat_t arc,
				struct mater_info *materp);

/**
 * Release chain of animation structures
 *
 * An unfortunate choice of name.
 */
RT_EXPORT extern void db_free_anim(struct db_i *dbip);

/**
 * Writes 'count' bytes into at file offset 'offset' from buffer at
 * 'addr'.  A wrapper for the UNIX write() sys-call that takes into
 * account syscall semaphores, stdio-only machines, and in-memory
 * buffering.
 *
 * Returns -
 * 0 OK
 * -1 FAILURE
 */
/* should be HIDDEN */
RT_EXPORT extern void db_write_anim(FILE *fop,
				    struct animate *anp);

/**
 * Parse one "anim" type command into an "animate" structure.
 *
 * argv[1] must be the "a/b" path spec,
 * argv[2] indicates what is to be animated on that arc.
 */
RT_EXPORT extern struct animate *db_parse_1anim(struct db_i *dbip,
						int argc,
						const char **argv);


/**
 * Free one animation structure
 */
RT_EXPORT extern void db_free_1anim(struct animate *anp);


/**
 * 'arc' may be a null pointer, signifying an identity matrix.
 * 'materp' may be a null pointer, signifying that the region has
 * already been finalized above this point in the tree.
 */
RT_EXPORT extern void db_apply_anims(struct db_full_path *pathp,
				     struct directory *dp,
				     mat_t stck,
				     mat_t arc,
				     struct mater_info *materp);


#endif /* RT_ANIM_H */

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
