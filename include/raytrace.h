/*                      R A Y T R A C E . H
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
/** @addtogroup librt */
/** @{ */
/** @file raytrace.h
 *
 * All the data structures and manifest constants necessary for
 * interacting with the BRL-CAD LIBRT ray-tracing library.
 *
 * Note that this header file defines many internal data structures,
 * as well as the library's external (interface) data structures.
 * These are provided for the convenience of applications builders.
 * However, the internal data structures are subject to change in each
 * release.
 *
 */

/* TODO - put together a dot file mapping the relationships between
 * high level rt structures and include it in the doxygen comments
 * with the \dotfile command:
 * http://www.stack.nl/~dimitri/doxygen/manual/commands.html#cmddotfile*/

#ifndef RAYTRACE_H
#define RAYTRACE_H

#include "common.h"

/* interface headers */
#include "bu/avs.h"
#include "bu/bitv.h"
#include "bu/file.h"
#include "bu/hash.h"
#include "bu/hist.h"
#include "bu/malloc.h"
#include "bu/mapped_file.h"
#include "bu/list.h"
#include "bu/log.h"
#include "bu/parallel.h" /* needed for BU_SEM_LAST */
#include "bu/parse.h"
#include "bu/ptbl.h"
#include "bu/str.h"
#include "bu/vls.h"
#include "bn.h"
#include "./rt/db5.h"
#include "nmg.h"
#include "pc.h"
#include "./rt/geom.h"

__BEGIN_DECLS

#include "./rt/defines.h"

#include "./rt/db_fullpath.h"

#include "./rt/debug.h"

#include "./rt/tol.h"

#include "./rt/db_internal.h"

#include "./rt/xray.h"

#include "./rt/hit.h"

#include "./rt/seg.h"

#include "./rt/soltab.h"

#include "./rt/mater.h"

#include "./rt/region.h"

#include "./rt/ray_partition.h"

#include "./rt/space_partition.h"

#include "./rt/mem.h"

#include "./rt/db_instance.h"

#include "./rt/directory.h"

#include "./rt/nongeom.h"

#include "./rt/tree.h"

#include "./rt/wdb.h"

#include "./rt/anim.h"

#include "./rt/piece.h"

#include "./rt/resource.h"

#include "./rt/application.h"

#include "./rt/global.h"

#include "./rt/rt_instance.h"

#include "./rt/view.h"

#include "./rt/func.h"

#include "./rt/functab.h"

#include "./rt/private.h"

#include "./rt/nmg.h"

#include "./rt/overlap.h"

#include "./rt/pattern.h"

#include "./rt/shoot.h"

#include "./rt/timer.h"

#include "./rt/boolweave.h"

#include "./rt/calc.h"

#include "./rt/cmd.h"

#include "./rt/search.h"

#include "./rt/db_io.h"

#include "./rt/primitives/arb8.h"
#include "./rt/primitives/epa.h"
#include "./rt/primitives/pipe.h"
#include "./rt/primitives/metaball.h"
#include "./rt/primitives/rpc.h"
#include "./rt/primitives/pg.h"
#include "./rt/primitives/hf.h"
#include "./rt/primitives/dsp.h"
#include "./rt/primitives/ell.h"
#include "./rt/primitives/tgc.h"
#include "./rt/primitives/sketch.h"
#include "./rt/primitives/annot.h"
#include "./rt/primitives/script.h"
#include "./rt/primitives/bot.h"
#include "./rt/primitives/brep.h"
#include "./rt/primitives/tor.h"
#include "./rt/primitives/rhc.h"
#include "./rt/primitives/cline.h"

#include "./rt/comb.h"

#include "./rt/misc.h"

#include "./rt/prep.h"

#include "./rt/vlist.h"

#include "./rt/htbl.h"

#include "./rt/dspline.h"

#include "./rt/db_attr.h"

#include "./rt/binunif.h"

#include "./rt/version.h"


__END_DECLS

#endif /* RAYTRACE_H */
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
