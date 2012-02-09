/*                      B R L C A D . I
 * BRL-CAD
 *
 * Copyright (c) United States Government as represented by
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

/**
 *
 * SWIG input file for BRL-CAD. Here be dragons.
 *
 * invocation should be something along the lines of
 * swig -<lang> -outdir <langdir> -I/usr/brlcad/include -I/usr/brlcad/include/brlcad /usr/brlcad/include/brlcad/brlcad.i
 *
 */

%module brlcad

%{
#include "brlcad/common.h"
#include "brlcad/bu.h"
#include "brlcad/vmath.h"
#include "brlcad/nmg.h"
#include "brlcad/bn.h"
#include "brlcad/db.h"
#include "brlcad/raytrace.h"
#include "brlcad/nurb.h"
#include "brlcad/wdb.h"
#include "brlcad/rtgeom.h"
#include "brlcad/rtfunc.h"
#include "brlcad/tie.h"
#include "brlcad/gcv.h"
#include "brlcad/icv.h"
#include "brlcad/dm.h"
#include "brlcad/fbio.h"
#include "brlcad/fb.h"
#include "brlcad/analyze.h"
#include "tie/adrt.h"
#include "tie/adrt_struct.h"
#include "tie/camera.h"
#include "tie/render.h"
#include "tie/render_internal.h"
#include "tie/render_util.h"
#include "tie/texture.h"
#include "tie/texture_internal.h"
%}

%include "brlcad/common.h"
%include "brlcad/bu.h"
%include "brlcad/vmath.h"
%include "brlcad/nmg.h"
%include "brlcad/bn.h"
%include "brlcad/db.h"
%include "brlcad/raytrace.h"
%include "brlcad/nurb.h"
%include "brlcad/wdb.h"
%include "brlcad/rtgeom.h"
%include "brlcad/rtfunc.h"
%include "brlcad/tie.h"
%include "brlcad/gcv.h"
%include "brlcad/icv.h"
%include "brlcad/dm.h"
%include "brlcad/fbio.h"
%include "brlcad/fb.h"
%include "brlcad/analyze.h"
%include "tie/adrt.h"
%include "tie/adrt_struct.h"
%include "tie/camera.h"
%include "tie/render.h"
%include "tie/render_internal.h"
%include "tie/render_util.h"
%include "tie/texture.h"

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
