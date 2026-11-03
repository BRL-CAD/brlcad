/**
 *               C R E O _ T O O L K I T . H
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 */

#ifndef CREO_FRONTEND_CREO_TOOLKIT_H
#define CREO_FRONTEND_CREO_TOOLKIT_H

#include "common.h"

#ifndef _WSTUDIO_DEFINED
#  define _WSTUDIO_DEFINED
#endif

#ifdef __cplusplus
extern "C" {
#endif

#if !defined(TEST_BUILD)
#  include <pd_proto.h>
#  include <ProToolkit.h>
#  include <ProArray.h>
#  include <ProAsmcomp.h>
#  include <ProAsmcomppath.h>
#  include <ProElement.h>
#  include <ProFeatType.h>
#  include <ProFeature.h>
#  include <ProHole.h>
#  include <ProMaterial.h>
#  include <ProMdl.h>
#  include <ProMdlUnits.h>
#  include <ProPart.h>
#  include <ProParameter.h>
#  include <ProSizeConst.h>
#  include <ProSkeleton.h>
#  include <ProSolid.h>
#  include <ProSolidBody.h>
#  include <ProSurface.h>
#  include <ProUICheckbutton.h>
#  include <ProUICmd.h>
#  include <ProUIDialog.h>
#  include <ProUIInputpanel.h>
#  include <ProUILabel.h>
#  include <ProUIPushbutton.h>
#  include <ProUIRadiogroup.h>
#  include <ProUtil.h>
#else
#  include "shim.h"
#endif

#include "vmath.h"

#ifdef __cplusplus
}
#endif

#define CREO_UI_NAME "creo_brl"
#define CREO_NAME_MAX (240 * 2)
#define CREO_MSG_MAX 4096

#endif /* CREO_FRONTEND_CREO_TOOLKIT_H */
