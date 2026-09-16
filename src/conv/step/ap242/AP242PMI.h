/*                    A P 2 4 2 P M I . H
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 */

#ifndef CONV_STEP_AP242_PMI_H
#define CONV_STEP_AP242_PMI_H

class BRLCADWrapper;
class STEPWrapper;

/** Retain the connected AP242 semantic/presentation PMI graph and create
 * native datum objects for exactly resolved point, axis, and plane datums. */
void ImportAP242PMI(STEPWrapper &wrapper, BRLCADWrapper &database);

#endif /* CONV_STEP_AP242_PMI_H */
