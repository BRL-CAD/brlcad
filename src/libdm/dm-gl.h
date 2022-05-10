/*                          D M - G L . H
 * BRL-CAD
 *
 * Copyright (c) 1993-2022 United States Government as represented by
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
/** @addtogroup libstruct dm */
/** @{ */
/** @file dm-gl.h
 *
 */

#ifndef DM_GL_H
#define DM_GL_H

#include "common.h"

#include "bio.h"

#include "vmath.h"
#include "bu.h"
#include "bv/vlist.h"
#include "bv/defines.h"
#include "dm.h"

#define GL_SILENCE_DEPRECATION 1

#ifdef OSMESA
#  include "OSMesa/gl.h"
#else
#  ifdef HAVE_GL_GL_H
#    include <GL/gl.h>
#  endif

#  ifdef HAVE_OPENGL_GL_H
#    include <OpenGL/gl.h>
#  endif
#endif

#ifndef DMGL_EXPORT
#  if defined(DMGL_DLL_EXPORTS) && defined(DMGL_DLL_IMPORTS)
#    error "Only DMGL_DLL_EXPORTS or DMGL_DLL_IMPORTS can be defined, not both."
#  elif defined(DMGL_DLL_EXPORTS)
#    define DMGL_EXPORT COMPILER_DLLEXPORT
#  elif defined(DMGL_DLL_IMPORTS)
#    define DMGL_EXPORT COMPILER_DLLIMPORT
#  else
#    define DMGL_EXPORT
#endif
#endif

#define CMAP_BASE 40

/* Map +/-2048 GED space into -1.0..+1.0 :: x/2048*/
#define GED2IRIS(x)	(((float)(x))*0.00048828125)

#define DM_REVERSE_COLOR_BYTE_ORDER(_shift, _mask) {    \
        _shift = 24 - _shift;                           \
        switch (_shift) {                               \
            case 0:                                     \
                _mask >>= 24;                           \
                break;                                  \
            case 8:                                     \
                _mask >>= 8;                            \
                break;                                  \
            case 16:                                    \
                _mask <<= 8;                            \
                break;                                  \
            case 24:                                    \
                _mask <<= 24;                           \
                break;                                  \
        }                                               \
    }

#define gl_MV_O(_m) offsetof(struct gl_vars, _m)

struct gl_internal_vars {
    int faceFlag;
    GLdouble faceplate_mat[16];
    GLclampf r, g, b;

    fastf_t default_viewscale;
    double xlim_view;  /* args for glOrtho*/
    double ylim_view;

    /* lighting parameters */
    float amb_three[4];
    float light0_position[4];
    float light0_diffuse[4];
    float wireColor[4];
    float ambientColor[4];
    float specularColor[4];
    float diffuseColor[4];
    float backDiffuseColorDark[4];
    float backDiffuseColorLight[4];
};

struct gl_vars {
    struct dm *this_dm;
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
    struct bu_vls log;
    double bound;
    int boundFlag;
    struct gl_internal_vars i;
};


/* For debugging - print specified string, and additional state
 * information depending on dm_debugLevel settings (such as
 * GL_MODELVIEW and GL_PROJECTION matrices) */
DMGL_EXPORT extern void gl_debug_print(struct dm *dmp, const char *title, int lvl);

DMGL_EXPORT extern struct bu_structparse gl_vparse[];

DMGL_EXPORT extern void glvars_init(struct dm *dmp);

DMGL_EXPORT extern int drawLine2D(struct dm *dmp, fastf_t X1, fastf_t Y1, fastf_t X2, fastf_t Y2, const char *log_bu);
DMGL_EXPORT extern int drawLine3D(struct dm *dmp, point_t pt1, point_t pt2, const char *log_bu, float *wireColor);
DMGL_EXPORT extern int drawLines3D(struct dm *dmp, int npoints, point_t *points, int lflag, const char *log_bu, float *wireColor);

DMGL_EXPORT extern int gl_beginDList(struct dm *dmp, unsigned int list);
DMGL_EXPORT extern int gl_debug(struct dm *dmp, int vl);
DMGL_EXPORT extern int gl_draw(struct dm *dmp, struct bv_vlist *(*callback_function)(void *), void **data);
DMGL_EXPORT extern int gl_drawBegin(struct dm *dmp);
DMGL_EXPORT extern int gl_drawDList(unsigned int list);
DMGL_EXPORT extern int gl_drawEnd(struct dm *dmp);
DMGL_EXPORT extern int gl_drawLine2D(struct dm *dmp, fastf_t X1, fastf_t Y1, fastf_t X2, fastf_t Y2);
DMGL_EXPORT extern int gl_drawLine3D(struct dm *dmp, point_t pt1, point_t pt2);
DMGL_EXPORT extern int gl_drawLines3D(struct dm *dmp, int npoints, point_t *points, int sflag);
DMGL_EXPORT extern int gl_drawPoint2D(struct dm *dmp, fastf_t x, fastf_t y);
DMGL_EXPORT extern int gl_drawPoint3D(struct dm *dmp, point_t point);
DMGL_EXPORT extern int gl_drawPoints3D(struct dm *dmp, int npoints, point_t *points);
DMGL_EXPORT extern int gl_drawVList(struct dm *dmp, struct bv_vlist *vp);
DMGL_EXPORT extern int gl_drawVListHiddenLine(struct dm *dmp, struct bv_vlist *vp);
DMGL_EXPORT extern int gl_draw_obj(struct dm *dmp, struct bv_scene_obj *s);
DMGL_EXPORT extern int gl_draw_data_axes(struct dm *dmp, fastf_t sf,  struct bv_data_axes_state *bndasp);
DMGL_EXPORT extern int gl_draw_display_list(struct dm *dmp, struct display_list *obj);
DMGL_EXPORT extern int gl_endDList(struct dm *dmp);
DMGL_EXPORT extern int gl_freeDLists(struct dm *dmp, unsigned int list, int range);
DMGL_EXPORT extern int gl_genDLists(struct dm *dmp, size_t range);
DMGL_EXPORT extern int gl_getDisplayImage(struct dm *dmp, unsigned char **image, int flip, int alpha);
DMGL_EXPORT extern int gl_get_internal(struct dm *dmp);
DMGL_EXPORT extern int gl_loadMatrix(struct dm *dmp, fastf_t *mat, int which_eye);
DMGL_EXPORT extern int gl_loadPMatrix(struct dm *dmp, const fastf_t *mat);
DMGL_EXPORT extern void gl_popPMatrix(struct dm *dmp);
DMGL_EXPORT extern int gl_logfile(struct dm *dmp, const char *filename);
DMGL_EXPORT extern int gl_hud_begin(struct dm *dmp);
DMGL_EXPORT extern int gl_hud_end(struct dm *dmp);
DMGL_EXPORT extern int gl_put_internal(struct dm *dmp);
DMGL_EXPORT extern int gl_reshape(struct dm *dmp, int width, int height);
DMGL_EXPORT extern int gl_setBGColor(struct dm *dmp, unsigned char r, unsigned char g, unsigned char b);
DMGL_EXPORT extern int gl_setDepthMask(struct dm *dmp, int depthMask_on);
DMGL_EXPORT extern int gl_setFGColor(struct dm *dmp, unsigned char r, unsigned char g, unsigned char b, int strict, fastf_t transparency);
DMGL_EXPORT extern int gl_setLight(struct dm *dmp, int lighting_on);
DMGL_EXPORT extern int gl_getLight(struct dm *dmp);
DMGL_EXPORT extern int gl_setLineAttr(struct dm *dmp, int width, int style);
DMGL_EXPORT extern int gl_setTransparency(struct dm *dmp, int transparency_on);
DMGL_EXPORT extern int gl_getTransparency(struct dm *dmp);
DMGL_EXPORT extern int gl_setWinBounds(struct dm *dmp, fastf_t *w);
DMGL_EXPORT extern int gl_setZBuffer(struct dm *dmp, int zbuffer_on);
DMGL_EXPORT extern int gl_getZBuffer(struct dm *dmp);
DMGL_EXPORT extern int gl_setZClip(struct dm *dmp, int zclip);
DMGL_EXPORT extern int gl_getZClip(struct dm *dmp);
DMGL_EXPORT extern void gl_bound_flag_hook(const struct bu_structparse *sdp, const char *name, void *base, const char *value, void *data);
DMGL_EXPORT extern void gl_bound_hook(const struct bu_structparse *sdp, const char *name, void *base, const char *value, void *data);
DMGL_EXPORT extern void gl_colorchange(const struct bu_structparse *sdp, const char *name, void *base, const char *value, void *data);
DMGL_EXPORT extern void gl_debug_hook(const struct bu_structparse *sdp, const char *name, void *base, const char *value, void *data);
DMGL_EXPORT extern void gl_fogHint(struct dm *dmp, int fastfog);
DMGL_EXPORT extern void gl_fog_hook(const struct bu_structparse *sdp, const char *name, void *base, const char *value, void *data);
DMGL_EXPORT extern void gl_lighting_hook(const struct bu_structparse *sdp, const char *name, void *base, const char *value, void *data);
DMGL_EXPORT extern void gl_logfile_hook(const struct bu_structparse *sdp, const char *name, void *base, const char *value, void *data);
DMGL_EXPORT extern void gl_printglmat(struct bu_vls *tmp_vls, GLfloat *m);
DMGL_EXPORT extern void gl_printmat(struct bu_vls *tmp_vls, fastf_t *mat);
DMGL_EXPORT extern void gl_transparency_hook(const struct bu_structparse *sdp, const char *name, void *base, const char *value, void *data);
DMGL_EXPORT extern void gl_zbuffer_hook(const struct bu_structparse *sdp, const char *name, void *base, const char *value, void *data);
DMGL_EXPORT extern void gl_zclip_hook(const struct bu_structparse *sdp, const char *name, void *base, const char *value, void *data);

#endif /* DM_OGL_H */

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
