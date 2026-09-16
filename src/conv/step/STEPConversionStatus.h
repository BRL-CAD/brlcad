/* BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by the
 * U.S. Army Research Laboratory.
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
 *
 * Internal status shared by the schema-neutral STEP import build units.
 */

#ifndef CONV_STEP_STEPCONVERSIONSTATUS_H
#define CONV_STEP_STEPCONVERSIONSTATUS_H

enum BrepWriteStatus {
    BREP_WRITE_SUCCESS = 0,
    BREP_CONVERSION_FAILED,
    BREP_INVALID_STRUCTURE,
    BREP_NOT_SOLID,
    BREP_OUTPUT_FAILED
};

#endif /* CONV_STEP_STEPCONVERSIONSTATUS_H */
