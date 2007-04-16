/*                            O P T I C A L . H
 * BRL-CAD
 *
 * Copyright (c) 2004-2007 United States Government as represented by
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
/** @addtogroup liboptical */
/** @{ */
/** @file optical.h
 *
 * @brief
 *  Header file for the BRL-CAD Optical Library, LIBOPTICAL.
 *
 */

#ifndef SEEN_OPTICAL_H
#define SEEN_OPTICAL_H seen

#include "common.h"

#include "bu.h"
#include "shadefuncs.h"
#include "shadework.h"

__BEGIN_DECLS

#ifndef OPTICAL_EXPORT
#  if defined(_WIN32) && !defined(__CYGWIN__) && defined(BRLCAD_DLL)
#    ifdef OPTICAL_EXPORT_DLL
#      define OPTICAL_EXPORT __declspec(dllexport)
#    else
#      define OPTICAL_EXPORT __declspec(dllimport)
#    endif
#  else
#    define OPTICAL_EXPORT
#  endif
#endif


/* defined in init.c */
OPTICAL_EXPORT extern void optical_shader_init(struct mfuncs **headp);

/* stub functions useful for debugging */
/* defined in sh_text.c */
OPTICAL_EXPORT extern int mlib_zero();
OPTICAL_EXPORT extern int mlib_one();
OPTICAL_EXPORT extern void mlib_void();


/* defined in refract.c */
OPTICAL_EXPORT extern int
rr_render(register struct application *ap, struct partition *pp, struct shadework *swp);

/* defined in shade.c */
OPTICAL_EXPORT extern void
shade_inputs(struct application	*ap, const struct partition *pp, struct shadework *swp, int want);

/* defined in wray.c */
OPTICAL_EXPORT extern void
wray(register struct partition *pp, register struct application *ap, FILE *fp, const vect_t inormal);

OPTICAL_EXPORT extern void
wraypts(vect_t in, vect_t inorm, vect_t out, int id, struct application *ap, FILE *fp);

OPTICAL_EXPORT extern void
wraypaint(vect_t start, vect_t norm, int paint, struct application *ap, FILE *fp);

/* defined in shade.c */
OPTICAL_EXPORT extern int
viewshade(struct application *ap, register const struct partition *pp, register struct shadework *swp);

/* defined in vers.c */
OPTICAL_EXPORT extern const char *optical_version(void);


OPTICAL_EXPORT extern int	rdebug;

/* When in production mode, no debug checking is performed, hence the
 * R_DEBUG define causes sections of debug code to go "poof"
 */
#ifdef NO_DEBUG_CHECKING
#	define	R_DEBUG	0
#else
#	define	R_DEBUG	rdebug
#endif

/*
 *
 *
 *  Debugging flags for thr RT program itself.
 *  These flags follow the "-X" (cap X) option to the RT program.
 *  librt debugging is separately controlled.
 *
 *  @author
 *	Michael John Muuss
 *
 *  @par Source
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005
 */

/* These definitions are each for one bit */
/* Should be reogranized to put most useful ones first */
#define RDEBUG_HITS	0x00000001	/* 1 Print hits used by view() */
#define RDEBUG_MATERIAL	0x00000002	/* 2 Material properties */
#define RDEBUG_SHOWERR	0x00000004	/* 3 Colorful markers on errors */
#define RDEBUG_RTMEM	0x00000008	/* 4 Debug librt mem after startup */
#define RDEBUG_SHADE	0x00000010	/* 5 Shading calculation */
#define RDEBUG_PARSE	0x00000020	/* 6 Command parsing */
#define RDEBUG_LIGHT	0x00000040	/* 7 Debug lighting */
#define RDEBUG_REFRACT	0x00000080	/* 8 Debug reflection & refraction */

#define RDEBUG_STATS	0x00000200	/* 10 Print more statistics */
#define RDEBUG_RTMEM_END 0x00000400	/* 11 Print librt mem use on 'clean' */

/* These will cause binary debugging output */
#define RDEBUG_MISSPLOT	0x20000000	/* 30 plot(5) missed rays to stdout */
#define RDEBUG_RAYWRITE	0x40000000	/* 31 Ray(5V) view rays to stdout */
#define RDEBUG_RAYPLOT	0x80000000	/* 32 plot(5) rays to stdout */

/* Format string for rt_printb() */
#define RDEBUG_FORMAT	\
"\020\040RAYPLOT\037RAYWRITE\036MISSPLOT\
\013RTMEM_END\
\012STATS\010REFRACT\
\7LIGHT\6PARSE\5SHADE\4RTMEM\3SHOWERR\2MATERIAL\1HITS"


/*
 *	A Bit vector to determine how much stuff rt prints when not in
 *	debugging mode.
 *
 */
OPTICAL_EXPORT extern int	rt_verbosity;
/*	   flag_name		value		prints */
#define VERBOSE_LIBVERSIONS  0x00000001	/* Library version strings */
#define VERBOSE_MODELTITLE   0x00000002	/* model title */
#define VERBOSE_TOLERANCE    0x00000004	/* model tolerance */
#define VERBOSE_STATS	     0x00000008	/* stats about rt_gettrees() */
#define VERBOSE_FRAMENUMBER  0x00000010	/* current frame number */
#define VERBOSE_VIEWDETAIL   0x00000020	/* view specifications */
#define VERBOSE_LIGHTINFO    0x00000040	/* scene lights */
#define VERBOSE_INCREMENTAL  0x00000080	/* progressive/incremental state */
#define VERBOSE_MULTICPU     0x00000100	/* #  of CPU's to be used */
#define VERBOSE_OUTPUTFILE   0x00000200	/* name of output image */

#define VERBOSE_FORMAT \
"\012OUTPUTFILE\011MULTICPU\010INCREMENTAL\7LIGHTINFO\6VIEWDETAIL\
\5FRAMENUMBER\4STATS\3TOLERANCE\2MODELTITLE\1LIBVERSIONS"

OPTICAL_EXPORT extern double AmbientIntensity;
#ifdef RT_MULTISPECTRAL
OPTICAL_EXPORT extern struct bn_tabdata *background;
#else
OPTICAL_EXPORT extern vect_t background;
#endif

#if 0
OPTICAL_EXPORT
OPTICAL_EXPORT extern
#endif
/* defined in sh_text.c */
OPTICAL_EXPORT extern struct region env_region; /* environment map region */

/* defined in refract.c */
OPTICAL_EXPORT extern int max_bounces;
OPTICAL_EXPORT extern int max_ireflect;

struct floatpixel {
	double	ff_dist;		/**< @brief range to ff_hitpt[], <-INFINITY for miss */
	float	ff_hitpt[3];
	struct region *ff_regp;
	int	ff_frame;		/**< @brief >= 0 means pixel was reprojected */
	short	ff_x;			/**< @brief screen x,y coords of first location */
	short	ff_y;
	char	ff_color[3];
};

__END_DECLS

#endif /* SEEN_OPTICAL_H */
/** @} */
/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
