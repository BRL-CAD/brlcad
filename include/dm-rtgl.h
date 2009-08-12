/*                          D M - R T G L . H
 * BRL-CAD
 *
 * Copyright (c) 1993-2009 United States Government as represented by
 * the U.S. Army Research Laboratory.
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
 */
/** @addtogroup libdm */
/** @{ */
/** @file dm-rtgl.h
 *
 */

#ifndef __DM_RTGL__
#define __DM_RTGL__

#include "dm_color.h"

#ifdef HAVE_GL_GLX_H
#  include <GL/glx.h>
#endif
#ifdef HAVE_GL_GL_H
#  include <GL/gl.h>
#endif
#define CMAP_BASE 40

/* Map +/-2048 GED space into -1.0..+1.0 :: x/2048*/
#define GED2IRIS(x)	(((float)(x))*0.00048828125)

#define Rtgl_MV_O(_m) offsetof(struct modifiable_rtgl_vars, _m)

#define DM_PRIV_VARS (dmp->dm_vars.priv_vars)
#define RTGL_GEDP ((struct rtgl_vars *)dmp->dm_vars.priv_vars)->mvars.gedp
#define RTGL_LASTJOBS ((struct rtgl_vars *)dmp->dm_vars.priv_vars)->mvars.lastJobs
#define RTGL_JOBSDONE ((struct rtgl_vars *)dmp->dm_vars.priv_vars)->mvars.jobsDone
#define RTGL_DOJOBS ((struct rtgl_vars *)dmp->dm_vars.priv_vars)->mvars.doJobs

struct modifiable_rtgl_vars {
    struct ged *gedp;  /* used to set up ray tracing */
    time_t lastJobs;
    int jobsDone;
    int doJobs;
    int cueing_on;
    int zclipping_on;
    int zbuffer_on;
    int lighting_on;
    int transparency_on;
    int fastfog;
    double fogdensity;
    int zbuf;
    int rgb;
    int doublebuffer;
    int depth;
    int debug;
    double bound;
    int boundFlag;
};

struct rtgl_vars {
    GLXContext glxc;
    GLdouble faceplate_mat[16];
    int face_flag;
    int *perspective_mode;
    int fontOffset;
    int ovec;		/* Old color map entry number */
    char is_direct;
    GLclampf r, g, b;
    struct modifiable_rtgl_vars mvars;
};

extern void rtgl_fogHint();

struct jobList {
    struct bu_list l;
    point_t pt;
    vect_t dir;
};

#define PT_ARRAY_SIZE 999

struct ptInfoList {
    struct bu_list l;
    int used;
    fastf_t points[PT_ARRAY_SIZE];
    fastf_t norms[PT_ARRAY_SIZE];
    float color[3];
};

struct objTree {
    char *name;
    int numChildren;
    struct objTree *children;
    struct objTree *parent;
    struct ptInfoList *ptInfo;
};

#define INIT_OBJTREE(p) { \
	((struct objTree *)(p))->name = NULL;	\
	((struct objTree *)(p))->numChildren = 0;	\
	((struct objTree *)(p))->children = NULL;	\
	((struct objTree *)(p))->parent = NULL;		\
	((struct objTree *)(p))->ptInfo = NULL;		\
}

#endif /* __DM_RTGL__ */

/** @} */
/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
