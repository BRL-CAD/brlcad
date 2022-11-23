/*                      H T B L . H
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
/** @file rt/htbl.h
 *
 */

#ifndef RT_HTBL_H
#define RT_HTBL_H

#include "common.h"
#include "vmath.h"
#include "rt/defines.h"

__BEGIN_DECLS

/* htbl.c */

RT_EXPORT extern void rt_htbl_init(struct rt_htbl *b, size_t len, const char *str);

/**
 * Reset the table to have no elements, but retain any existing storage.
 */
RT_EXPORT extern void rt_htbl_reset(struct rt_htbl *b);

/**
 * Deallocate dynamic hit buffer and render unusable without a
 * subsequent rt_htbl_init().
 */
RT_EXPORT extern void rt_htbl_free(struct rt_htbl *b);

/**
 * Allocate another hit structure, extending the array if necessary.
 */
RT_EXPORT extern struct hit *rt_htbl_get(struct rt_htbl *b);


__END_DECLS

#endif /* RT_HTBL_H */

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
