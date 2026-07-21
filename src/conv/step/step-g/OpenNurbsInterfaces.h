/*                 O P E N N U R B S I N T E R F A C E S . H
 * BRL-CAD
 *
 * Copyright (c) 1994-2026 United States Government as represented by the
 * U.S. Army Research Laboratory.
 *
 * Internal interfaces shared by the STEP topology construction and bounded
 * post-construction repair passes.
 */

#ifndef CONV_STEP_OPENNURBSINTERFACES_H
#define CONV_STEP_OPENNURBSINTERFACES_H

class ON_Brep;
class ON_BrepLoop;
class ON_Surface;
class ON_BrepTrim;
class ON_2dPoint;

bool step_insert_periodic_pole_cut(ON_Brep *brep, ON_BrepLoop &loop,
    const ON_Surface *surface, const ON_BrepTrim &boundary_trim,
    const ON_2dPoint &boundary_end, const ON_2dPoint &boundary_start,
    double tolerance);

#endif /* CONV_STEP_OPENNURBSINTERFACES_H */
