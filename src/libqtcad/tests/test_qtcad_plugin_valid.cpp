/*        T E S T _ Q T C A D _ P L U G I N _ V A L I D . C P P
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 */

#include "common.h"

#include "test_qtcad_plugin_base.h"

class QgValidTestPlugin : public QgTestCommandPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID QGTCAD_IID("IQgCommand")
	FILE "test_qtcad_plugin_valid.json")
};

#include "test_qtcad_plugin_valid.moc"
