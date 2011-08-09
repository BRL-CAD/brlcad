/*			S H A D E F U N C S . H
 * BRL-CAD
 *
 * Copyright (c) 1993-2011 United States Government as represented by
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
/** @addtogroup rt */
/** @{ */
/** @file shadefuncs.h
 *
 */

#ifndef SHADEFUNCS
#define SHADEFUNCS

#include "shadework.h"


#ifndef OPTICAL_EXPORT
#   if defined(_WIN32) && !defined(__CYGWIN__) && defined(BRLCAD_DLL)
#      ifdef OPTICAL_EXPORT_DLL
#         define OPTICAL_EXPORT __declspec(dllexport)
#      else
#         define OPTICAL_EXPORT __declspec(dllimport)
#      endif
#   else
#      define OPTICAL_EXPORT
#   endif
#endif

/**
 *			M F U N C S
 *
 *  The interface to the various material property & texture routines.
 */
struct mfuncs {
    uint32_t mf_magic;		/**< @brief To validate structure */
    char *mf_name;		/**< @brief Keyword for material */
    struct mfuncs *mf_forw;	/**< @brief Forward link */
    int mf_inputs;		/**< @brief shadework inputs needed */
    int mf_flags;		/**< @brief Flags describing shader */
    int (*mf_setup)(struct region *rp,
		    struct bu_vls *matparm,
		    genptr_t *dpp,
		    const struct mfuncs *mfp,
		    struct rt_i *rtip); /**< @brief Routine for preparing */
    int (*mf_render)(struct application *ap,
		     const struct partition *pp,
		     struct shadework *swp,
		     genptr_t dp);	/**< @brief Routine for rendering */
    void (*mf_print)(struct region *rp,
		     genptr_t dp);	/**< @brief Routine for printing */
    void (*mf_free)(genptr_t cp);	/**< @brief Routine for releasing storage */
};
#define MF_NULL		((struct mfuncs *)0)
#define RT_CK_MF(_p)	BU_CKMAG(_p, MF_MAGIC, "mfuncs")

/*
 *  mf_inputs lists what optional shadework fields are needed.
 *  dist, point, color, & default(trans, reflect, ri) are always provided
 */
#define MFI_NORMAL	0x01		/**< @brief  Need normal */
#define MFI_UV		0x02		/**< @brief  Need uv */
#define MFI_LIGHT	0x04		/**< @brief  Need light visibility */
#define MFI_HIT		0x08		/**< @brief  Need just hit point */

/** for bu_printb() */
#define MFI_FORMAT	"\020\4HIT\3LIGHT\2UV\1NORMAL"


/* mf_flags lists important details about individual shaders */
#define MFF_PROC	0x01		/**< @brief  shader is procedural, computes tr/re/hits */

/* defined in material.c */
OPTICAL_EXPORT extern void mlib_add_shader(struct mfuncs **headp,
					   struct mfuncs *mfp1);

OPTICAL_EXPORT extern int mlib_setup(struct mfuncs **headp,
				     struct region *rp,
				     struct rt_i *rtip);

OPTICAL_EXPORT extern void mlib_free(struct region *rp);

/**
 * L O A D _ D Y N A M I C _ S H A D E R
 *
 * Given a shader/material name, try to find a DSO to supply the
 * shader.
 */
OPTICAL_EXPORT extern struct mfuncs *load_dynamic_shader(const char *material);

#endif
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
