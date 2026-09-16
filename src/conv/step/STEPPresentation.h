/*                 S T E P P R E S E N T A T I O N . H
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 */

#ifndef CONV_STEP_STEPPRESENTATION_H
#define CONV_STEP_STEPPRESENTATION_H

class STEPWrapper;

/**
 * Extract schema-neutral presentation styles and layer membership from the
 * lazy Part 21 index.  Returns false when no lazy index is available, allowing
 * a schema module to supply a generated-binding fallback if it has one.
 */
bool ExtractSTEPPresentation(STEPWrapper &wrapper);

#endif /* CONV_STEP_STEPPRESENTATION_H */
