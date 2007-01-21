/*                           B O T . H
 * BRL-CAD
 *
 * Copyright (c) 2001-2007 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2 of
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
    int bot_ntri;
    fastf_t *bot_thickness;
    struct bu_bitv *bot_facemode;
    genptr_t bot_facelist;	/* head of linked list */
    genptr_t *bot_facearray;	/* head of face array */
    unsigned int bot_tri_per_piece;	/* log # tri per peice. 1 << bot_ltpp is tri per piece */

};

RT_EXPORT BU_EXTERN(void rt_bot_prep_pieces,
		    (struct bot_specific	*bot,
		     struct soltab		*stp,
		     int			ntri,
		     const struct bn_tol	*tol));

RT_EXPORT BU_EXTERN(int rt_botface,
		    (struct soltab		*stp,
		     struct bot_specific	*bot,
		     fastf_t			*ap,
		     fastf_t			*bp,
		     fastf_t			*cp,
		     int			face_no,
		     const struct bn_tol	*tol));

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
