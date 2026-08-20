/*                    S T E P M A T E R I A L I M P O R T . H
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 */

#ifndef CONV_STEP_STEP_MATERIAL_IMPORT_H
#define CONV_STEP_STEP_MATERIAL_IMPORT_H

class STEPWrapper;

/** Extract material assignments and represented product properties from the
 * selected mechanical schema. */
void ExtractSTEPMaterialMetadata(STEPWrapper &wrapper);

#endif /* CONV_STEP_STEP_MATERIAL_IMPORT_H */
