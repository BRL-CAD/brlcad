/*                      B O T _ E D G E . H
 * BRL-CAD
 *
 * Copyright (c) 1999-2021 United States Government as represented by
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

#include "common.h"

/* interface headers */
#include "raytrace.h"


struct bot_edge {
    int v;
    size_t use_count;
    struct bot_edge *next;
};


/**
 * Builds a table of all unique edges in bot, storing the result in edges.
 * Returns number of edges.
 *
 * Edge i to j (i < j) is represented as the bot_edge struct with v = j,
 * stored in the list headed by (struct bot_edge*) edges[i].
 */
extern int
bot_edge_table(struct rt_bot_internal *bot, struct bot_edge ***edges);


/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
