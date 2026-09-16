/*                 A P 2 4 2 T E S S E L L A T E D . H
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 */

#ifndef CONV_STEP_AP242_TESSELLATED_H
#define CONV_STEP_AP242_TESSELLATED_H

class BRLCADWrapper;
class STEPWrapper;

/** Convert product-bound AP242 tessellated representations to BRL-CAD BOTs. */
void ImportAP242Tessellated(STEPWrapper &wrapper, BRLCADWrapper &database);

#endif /* CONV_STEP_AP242_TESSELLATED_H */
