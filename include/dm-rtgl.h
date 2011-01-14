/*                          D M - R T G L . H
 * BRL-CAD
 *
 * Copyright (c) 1993-2011 United States Government as represented by
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

#include "common.h"
#include "vmath.h"

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

#define RTGL_MVARS (((struct rtgl_vars *)dmp->dm_vars.priv_vars)->mvars)
#define RTGL_GEDP      RTGL_MVARS.gedp
#define RTGL_DIRTY     RTGL_MVARS.needRefresh

struct modifiable_rtgl_vars {
    struct ged *gedp;  /* used to set up ray tracing */
    int needRefresh;
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

#define JOB_ARRAY_SIZE 1000

struct job {
    point_t pt;
    vect_t dir;
};

struct jobList {
    struct bu_list l;
    int used;
    struct job jobs[JOB_ARRAY_SIZE];
};

#define COPY_JOB(a, b) \
    VMOVE((a).pt, (b).pt);	\
    VMOVE((a).dir, (b).dir);

#define START_TABLE_SIZE 64
#define KEY_LENGTH 3
#define PT_ARRAY_SIZE 999

struct ptInfoList {
    struct bu_list l;
    int used;
    float points[PT_ARRAY_SIZE];
    float norms[PT_ARRAY_SIZE];
};

struct colorBin {
    float color[3];
    struct ptInfoList *list;
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

/**
 *  Structure to hold information specific to running the incremental
 *  ray firings and drawing events needed for rtgl
 */
struct rtglJobs {
    int controlClip;
    int calls;
    int jobsDone;
    char **oldTrees;
    size_t numTrees;
    size_t treeCapacity;
    struct bu_hash_tbl *colorTable;
    struct ptInfoList *currItem;
    struct jobList *currJob;
    size_t numJobs;
    int rtglWasClosed;
};


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
