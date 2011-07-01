/*                           B O T . H
 * BRL-CAD
 *
 * Copyright (c) 2001-2011 United States Government as represented by
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
/** @addtogroup g_ */
/** @{ */
/** @file bot.h
 *
 *  Header file for the "bot" specific structure.  This is shared between
 *  the bot, and ars at the moment.
 */
/** @} */


struct bot_specific {
    unsigned char bot_mode;
    unsigned char bot_orientation;
    unsigned char bot_flags;
    size_t bot_ntri;
    fastf_t *bot_thickness;
    struct bu_bitv *bot_facemode;
    genptr_t bot_facelist;	/* head of linked list */
    genptr_t *bot_facearray;	/* head of face array */
    size_t bot_tri_per_piece;	/* log # tri per peice. 1 << bot_ltpp is tri per piece */
    void *tie; /* FIXME: horrible blind cast, points to one in rt_bot_internal */
};

RT_EXPORT extern void rt_bot_prep_pieces(struct bot_specific	*bot,
					 struct soltab		*stp,
					 size_t			ntri,
					 const struct bn_tol	*tol);

RT_EXPORT extern size_t rt_botface(struct soltab		*stp,
				   struct bot_specific	*bot,
				   fastf_t			*ap,
				   fastf_t			*bp,
				   fastf_t			*cp,
				   size_t			face_no,
				   const struct bn_tol	*tol);

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
