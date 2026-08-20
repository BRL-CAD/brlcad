/*                         S T E P S W E P T S O L I D . H
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 */

#ifndef CONV_STEP_STEPSWEPTSOLID_H
#define CONV_STEP_STEPSWEPTSOLID_H

class STEPWrapper;
class BRLCADWrapper;

/** Convert compatible exact swept solids retained by a mechanical schema. */
void ConvertSTEPSweptSolids(STEPWrapper &wrapper, BRLCADWrapper &database);

#endif /* CONV_STEP_STEPSWEPTSOLID_H */
