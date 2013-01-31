/*
 * File:	stdvds.h
 * Description:	Declarations for the functions and globals used by the
 *		"standard" included routines for checking node visibility,
 *		and for deciding when to fold and unfold nodes.  You need
 *		only #include this file if using these standard routines.
 */
#include "vds.h"

#ifdef VDS_DOUBLE_PRECISION
#define GL_VERTEX3V(_v) glVertex3dv(_v)
#define GL_NORMAL3V(_n) glNormal3dv(_n)
#else
#define GL_VERTEX3V(_v) glVertex3fv(_v)
#define GL_NORMAL3V(_n) glNormal3fv(_n)
#endif

/* Public globals, used by the standard visibility and fold criterion tests */
extern vdsVec3 vdsViewPt;
extern vdsVec3 vdsLookVec;
extern vdsFloat vdsFOV;
extern vdsFloat vdsInvTanFOV;
extern vdsFloat vdsThreshold;

/* Common prototypes */
extern void vdsSetFOV(vdsFloat FOV);
extern void vdsSetViewpoint(vdsVec3 viewpt);
extern void vdsSetLookVec(vdsVec3 look);
extern void vdsSetThreshold(vdsFloat thresh);

/* Fold criterion prototypes */
extern int vdsThresholdTest(const vdsNode *node);

/* Visibility test prototypes */
extern int vdsSimpleVisibility(const vdsNode *node);

/* Rendering function prototypes */
extern void vdsRenderWireframe(const vdsNode *node);
extern void vdsRenderShaded(const vdsNode *node);
extern void vdsRenderShadedLit(const vdsNode *node);
extern void vdsRenderLit(const vdsNode *node);
extern unsigned int vdsCountTrisDrawn();



