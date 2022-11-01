/*                        R E D U C E . C
 * BRL-CAD
 *
 * Copyright (c) 2015-2022 United States Government as represented by
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
/** @file reduce.c
 *
 * Reduce an object into some form of simpler representation.
 *
 */


#include "common.h"

#include "rt/functab.h"
#include "rt/primitives/bot.h"
#include "rt/misc.h"
#include "rt/db5.h"


HIDDEN fastf_t
_determine_bot_min_edge_length(const struct rt_bot_internal *bot,
			       fastf_t reduction_level)
{
    size_t i;
    fastf_t min_len_sq = INFINITY, max_len_sq = -INFINITY;

    RT_BOT_CK_MAGIC(bot);

    for (i = 0; i < bot->num_faces; ++i) {
	vect_t temp;
	const int * const face = &bot->faces[i * 3];
	const fastf_t * const p1 = &bot->vertices[face[0] * 3];
	const fastf_t * const p2 = &bot->vertices[face[1] * 3];
	const fastf_t * const p3 = &bot->vertices[face[2] * 3];

	VSUB2(temp, p1, p2);
	min_len_sq = FMIN(min_len_sq, MAGSQ(temp));
	max_len_sq = FMAX(max_len_sq, MAGSQ(temp));

	VSUB2(temp, p1, p3);
	min_len_sq = FMIN(min_len_sq, MAGSQ(temp));
	max_len_sq = FMAX(max_len_sq, MAGSQ(temp));

	VSUB2(temp, p2, p3);
	min_len_sq = FMIN(min_len_sq, MAGSQ(temp));
	max_len_sq = FMAX(max_len_sq, MAGSQ(temp));
    }

    return sqrt(min_len_sq) + reduction_level * (sqrt(max_len_sq) - sqrt(
		min_len_sq));
}


HIDDEN void
_rt_reduce_bot(struct rt_db_internal *dest,
	       const struct rt_bot_internal *bot, fastf_t reduction_level, unsigned flags)
{
    struct rt_bot_internal *result;
    fastf_t max_chord_error, max_normal_error, min_edge_length;

    RT_CK_DB_INTERNAL(dest);
    RT_BOT_CK_MAGIC(bot);

    min_edge_length = _determine_bot_min_edge_length(bot, reduction_level);

    if (flags & RT_REDUCE_OBJ_PRESERVE_VOLUME)
	max_chord_error = max_normal_error = 0.0;
    else {
	/* arbitrary */
	max_chord_error = min_edge_length * reduction_level;
	max_normal_error = 22.5 * reduction_level;
    }

    min_edge_length = FMAX(RT_LEN_TOL, min_edge_length);
    max_chord_error = FMAX(RT_LEN_TOL, max_chord_error);
    max_normal_error = FMAX(90.0 - RAD2DEG * acos(RT_DOT_TOL), max_normal_error);

    /* clone bot */
    result = rt_bot_merge(1, (const struct rt_bot_internal * const *)&bot);

    /* reduce */
    {
	struct bn_tol tol = BN_TOL_INIT_ZERO;
	tol.dist = max_chord_error;
	tol.dist_sq = tol.dist * tol.dist;
	tol.perp = tol.para = cos(DEG2RAD * max_normal_error);
	rt_bot_vertex_fuse(result, &tol);
    }
    rt_bot_face_fuse(result);
    rt_bot_condense(result);
    rt_bot_decimate(result, max_chord_error, max_normal_error, min_edge_length);

    /* store result */
    dest->idb_major_type = DB5_MAJORTYPE_BRLCAD;
    dest->idb_minor_type = ID_BOT;
    dest->idb_meth = &OBJ[dest->idb_minor_type];
    dest->idb_ptr = result;
}


void
rt_reduce_obj(struct rt_db_internal *dest,
	      const struct rt_db_internal *internal, fastf_t reduction_level, unsigned flags)
{
    RT_CK_DB_INTERNAL(dest);
    RT_CK_DB_INTERNAL(internal);

    if (reduction_level < 0.0 || reduction_level > 1.0)
	bu_bomb("rt_reduce_obj(): invalid reduction_level");

    if (internal->idb_major_type == DB5_MAJORTYPE_BRLCAD)
	switch (internal->idb_minor_type) {
	    case ID_BOT: {
		const struct rt_bot_internal *bot = (struct rt_bot_internal *)internal->idb_ptr;
		RT_BOT_CK_MAGIC(bot);
		_rt_reduce_bot(dest, bot, reduction_level, flags);
		return;
	    }

	    default:
		break;
	}

    bu_log("rt_reduce_obj(): no reduction function for the given object\n");
}


/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
