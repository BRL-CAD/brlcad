/*                        H E A L . H
 * BRL-CAD
 *
 * Copyright (c) 2008-2021 United States Government as represented by
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
/** @addtogroup libanalyze
 *
 */
/** @{ */
/** @file analyze/heal.h */

#ifndef ANALYZE_HEAL_H
#define ANALYZE_HEAL_H

#include "common.h"
#include "raytrace.h"

#include "analyze/defines.h"

__BEGIN_DECLS

ANALYZE_EXPORT void
analyze_heal_bot(struct rt_bot_internal *bot, double zipper_tol);

__END_DECLS

#endif /* ANALYZE_HEAL_H */

/** @} */

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
