/*                            O P T I C A L . H
 * BRL-CAD
 *
 * Copyright (c) 2004-2022 United States Government as represented by
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

#ifndef OPTICAL_H
#define OPTICAL_H

#include "common.h"

#include "bu/vls.h"
__BEGIN_DECLS

#include "optical/defines.h"
#include "optical/debug.h"
#include "optical/shadework.h"
#include "optical/shadefuncs.h"

/**
 * this function sets the provided mfuncs head pointer to the list of
 * available shaders.  the provided mfuncs head pointer should point
 * to MF_NULL prior to getting passed to optical_shader_init() so that
 * the same shader list may be returned repeatably.
 */
OPTICAL_EXPORT extern void optical_shader_init(struct mfuncs **headp);

/* stub functions useful for debugging */
/* defined in sh_text.c */
OPTICAL_EXPORT extern int mlib_zero(struct application *, const struct partition *, struct shadework *, void *);
OPTICAL_EXPORT extern int mlib_one(struct region *, struct bu_vls *, void **, const struct mfuncs *, struct rt_i *);
OPTICAL_EXPORT extern void mlib_void(struct region *, void *);


/* defined in refract.c */
OPTICAL_EXPORT extern int
rr_render(struct application *app, const struct partition *pp, struct shadework *swp);

/* defined in shade.c */
OPTICAL_EXPORT extern void
shade_inputs(struct application	*app, const struct partition *pp, struct shadework *swp, int want);

/* defined in wray.c */
OPTICAL_EXPORT extern void
wray(struct partition *pp, struct application *app, FILE *fp, const vect_t inormal);

OPTICAL_EXPORT extern void
wraypts(vect_t in, vect_t inorm, vect_t out, int id, struct application *app, FILE *fp);

OPTICAL_EXPORT extern void
wraypaint(vect_t start, vect_t norm, int paint, struct application *app, FILE *fp);

/* defined in shade.c */
OPTICAL_EXPORT extern int
viewshade(struct application *app, const struct partition *pp, struct shadework *swp);

/* defined in vers.c */
OPTICAL_EXPORT extern const char *optical_version(void);


__END_DECLS

#endif /* OPTICAL_H */

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
