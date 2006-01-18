/*			S H A D E F U N C S . H
 * BRL-CAD
 *
 * Copyright (c) 1993-2006 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2.1 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file shadefuncs.h
 *
 *  Source -
 *	The U. S. Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5068  USA
 *
 *  $Header$
 */
#ifndef SHADEFUNCS
#define SHADEFUNCS

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

/*
 *			M F U N C S
 *
 *  The interface to the various material property & texture routines.
 */
struct mfuncs {
	long		mf_magic;	/* To validate structure */
	char		*mf_name;	/* Keyword for material */
	struct mfuncs	*mf_forw;	/* Forward link */
	int		mf_inputs;	/* shadework inputs needed */
	int		mf_flags;	/* Flags describing shader */
	int		(*mf_setup)();	/* Routine for preparing */
	int		(*mf_render)();	/* Routine for rendering */
	void		(*mf_print)();	/* Routine for printing */
	void		(*mf_free)();	/* Routine for releasing storage */
};
#define MF_MAGIC	0x55968058
#define MF_NULL		((struct mfuncs *)0)
#define RT_CK_MF(_p)	BU_CKMAG(_p, MF_MAGIC, "mfuncs")

/*
 *  mf_inputs lists what optional shadework fields are needed.
 *  dist, point, color, & default(trans,reflect,ri) are always provided
 */
#define MFI_NORMAL	0x01		/* Need normal */
#define MFI_UV		0x02		/* Need uv */
#define MFI_LIGHT	0x04		/* Need light visibility */
#define MFI_HIT		0x08		/* Need just hit point */

/* for bu_printb() */
#define MFI_FORMAT	"\020\4HIT\3LIGHT\2UV\1NORMAL"


/* mf_flags lists important details about individual shaders */
#define MFF_PROC	0x01		/* shader is procedural, computes tr/re/hits */

/* defined in material.c */
OPTICAL_EXPORT BU_EXTERN(void mlib_add_shader,
			 (struct mfuncs **headp,
			  struct mfuncs *mfp1));

OPTICAL_EXPORT BU_EXTERN(int mlib_setup,
			 (struct mfuncs **headp,
			  struct region *rp,
			  struct rt_i *rtip));

OPTICAL_EXPORT BU_EXTERN(void mlib_free,
			 (struct region *rp));

OPTICAL_EXPORT BU_EXTERN(struct mfuncs *load_dynamic_shader,
			 (const char *material,
			  const int mlen));

#endif

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
