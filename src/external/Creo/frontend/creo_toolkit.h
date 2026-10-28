#ifndef CREO_FRONTEND_CREO_TOOLKIT_H
#define CREO_FRONTEND_CREO_TOOLKIT_H

#include "common.h"

#ifndef _WSTUDIO_DEFINED
#  define _WSTUDIO_DEFINED
#endif

#if !defined(TEST_BUILD)
extern "C" {
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
}
#else
extern "C" {
#  include "shim.h"
}
#endif

extern "C" {
#include "vmath.h"
}

#define CREO_UI_NAME "creo_brl"
#define CREO_NAME_MAX (240 * 2)
#define CREO_MSG_MAX 4096

#endif /* CREO_FRONTEND_CREO_TOOLKIT_H */
