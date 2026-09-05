/*                    I G E S _ O U T P U T . H
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#ifndef CONV_IGES_OUTPUT_H
#define CONV_IGES_OUTPUT_H

#include "common.h"

struct rt_wdb;

__BEGIN_DECLS

int iges_output_begin(const char *input, const char *output, const char *report, int strict);
const char *iges_output_database_path(void);
const char *iges_output_report_path(void);
void iges_output_legacy_entity(int id, int type, const char *name, int written);
/* Finalize diagnostics, close the database, and publish only a successful
 * conversion.  Failed imports leave any existing database untouched. */
int iges_output_finish(struct rt_wdb *wdbp, int success);

__END_DECLS

#endif
