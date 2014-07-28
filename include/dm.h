/*                          D M . H
 * BRL-CAD
 *
 * Copyright (c) 1993-2014 United States Government as represented by
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
/** @file dm.h
 *
 */

#ifndef DM_H
#define DM_H

#include "common.h"

#include "vmath.h"
#include "icv.h"

#include "./dm/defines.h"

#if 0
#define USE_FBSERV 1

#ifdef USE_FBSERV
#  include "fbserv_obj.h"
#endif


#define DM_NULL (struct dm *)NULL
#define DM_MIN (-2048)
#define DM_MAX (2047)

#define DM_O(_m) offsetof(struct dm, _m)

#define GED_MAX 2047.0
#define GED_MIN -2048.0
#define GED_RANGE 4095.0
#define INV_GED 0.00048828125
#define INV_4096 0.000244140625

/*
 * Display coordinate conversion:
 * GED is using -2048..+2048,
 * X is 0..width, 0..height
 */
#define DIVBY4096(x) (((double)(x))*INV_4096)
#define GED_TO_Xx(_dmp, x) ((int)((DIVBY4096(x)+0.5)*_dmp->dm_width))
#define GED_TO_Xy(_dmp, x) ((int)((0.5-DIVBY4096(x))*_dmp->dm_height))
#define Xx_TO_GED(_dmp, x) ((int)(((x)/(double)_dmp->dm_width - 0.5) * GED_RANGE))
#define Xy_TO_GED(_dmp, x) ((int)((0.5 - (x)/(double)_dmp->dm_height) * GED_RANGE))

/* +-2048 to +-1 */
#define GED_TO_PM1(x) (((fastf_t)(x))*INV_GED)

#ifdef IR_KNOBS
#  define NOISE 16		/* Size of dead spot on knob */
#endif

#define DM_COPY_COLOR(_dr, _dg, _db, _sr, _sg, _sb) {\
	(_dr) = (_sr);\
	(_dg) = (_sg);\
	(_db) = (_sb); }
#define DM_SAME_COLOR(_dr, _dg, _db, _sr, _sg, _sb)(\
	(_dr) == (_sr) &&\
	(_dg) == (_sg) &&\
	(_db) == (_sb))
#define DM_REVERSE_COLOR_BYTE_ORDER(_shift, _mask) {	\
	_shift = 24 - _shift;				\
	switch (_shift) {				\
	    case 0:					\
		_mask >>= 24;				\
		break;					\
	    case 8:					\
		_mask >>= 8;				\
		break;					\
	    case 16:					\
		_mask <<= 8;				\
		break;					\
	    case 24:					\
		_mask <<= 24;				\
		break;					\
	}						\
    }
#endif
/**********************************************************************************************/
/** EXPERIMENTAL - considering a new design for the display
 * manager structure.  DO NOT USE!! */

/* Display Manager Types */
#define DM_TYPE_BAD     -1
#define DM_TYPE_NULL	0
#define DM_TYPE_TXT	1
#define DM_TYPE_QT	2
#define DM_TYPE_OSG	3

#define DM_STYLE_TXT 0
#define DM_STYLE_WIREFRAME 1
#define DM_STYLE_POINTS 2
#define DM_STYLE_TRIANGLES 3
#define DM_STYLE_HIDDEN_LINE 4
struct dm_db_obj {
    int draw_style;      	/**< @brief bitmask for points, wireframe, solid, hidden line, etc. */
    mat_t matrix;          	/**< @brief local matrix for individual in-memory object manipulation during editing, when per-change disk I/O is too expensive */
};

#define DM_VIEW_OBJ_TYPE_3DLINES 0
#define DM_VIEW_OBJ_TYPE_2DLINES 1
#define DM_VIEW_OBJ_TYPE_TRIANGLES 2
#define DM_VIEW_OBJ_TYPE_TEXT 4
#define DM_VIEW_OBJ_TYPE_FB 5
struct dm_view_obj {
    int              draw_type;   	/**< @brief obj type - framebuffer, 2D lines, 3D lines, triangles, text, grouping object, etc. */
    struct bn_vlist *vlist;  		/**< @brief If the object defines specific linear or triangular geometry for drawing
					            (text may but is not required to, and a framebuffer uses image data) it is here. */
    struct bu_ptbl  *obj_set;		/**< @brief A grouping object may define a union of other objects (view and/or db) on which actions will be performed */
    void 	    *image_data;
    mat_t 	     matrix;
};


/* Use a union to allow the display list object to hold both types */
#define DM_DB_OBJ 1
#define DM_VIEW_OBJ 2
union dm_object {
    struct dm_db_obj   db_obj;
    struct dm_view_obj view_obj;
};

struct dm_display_list {
    int obj_type;
    union dm_object     obj;
    struct bu_vls      *handle;   		/**< @brief For non-geometry view objects, need a string handle.  For geometry objects, full path string */
    int                 dirty_flag;      	/**< @brief If set, need to (re)generate the drawing content for this object */
    int			visibility_flag;        /**< @brief Allows users to hide/view objects without needing to recreate them */
    int			highlight_flag;         /**< @brief Identify whether the object is highlighted within the view */
    fastf_t		draw_width;		/**< @brief !0 override's the display manager's default Point radius/line width */
    int 		fontsize;		/**< @brief !0 override's the display manager's default font size when obj labeling is performed */
    unsigned char	rgb[3];			/**< @brief local color override */
    struct bu_attribute_value_set *obj_extra_settings;	/**< @brief Different types of display managers (X, OSG, etc.) can optionally do more with individual objects (e.g., transparency). DMTYPE specific settings here. */
    void 		*client_data;		/**< @brief Slot to allow applications to supply custom data */
};

struct dm {
    const char *dm_name;			/**< @brief short name of device */
    const char *dm_lname;			/**< @brief long name of device */
    int				 dm_type;	/**< @brief drawing canvas type (X, OSG, Qt, txt, etc.) currently in use by display manager */
    int 			 perspective;	/**< @brief !0 means perspective on */
    mat_t			 view_matrix;   /**< @brief view matrix for the default camera */
    mat_t			 proj_matrix;   /**< @brief projection matrix for the default camera */
    void			*dm_canvas;     /**< @brief pointer to the actual low level drawing canvas (X window, OSG viewer, etc.) */
    int				 is_embedded;   /**< @brief determine if the display manager is stand-alone or embedded (impacts event handling) */
    void			*parent_info;   /**< @brief if dm is embedded, parent_info must contain all the info necessary for libdm to embed the Window */
    struct bu_ptbl		*dm_l;		/**< @brief Display list for this view */
    int 			 fontsize;	/**< @brief !0 override's the auto font size */
    unsigned char 		 dm_bg[3];	/**< @brief background color */
    unsigned char 		 dm_fg[3];	/**< @brief default foreground color */
    fastf_t			 draw_width;	/**< @brief Default point radius/line width */
    int 			 width;
    int 			 height;
    int 			 light;		/**< @brief !0 means lighting on */
    int  			 zclip;		/**< @brief !0 means zclipping */
    vect_t 			 clipmin;	/**< @brief minimum clipping vector */
    vect_t 			 clipmax;	/**< @brief maximum clipping vector */
    int				 debug_level;
    struct bu_attribute_value_set *dm_extra_settings;	/**< @brief Different types of display managers (X, OSG, etc.) can optionally expose additional, DMTYPE specific settings here. */
    void 			*client_data;	/**< @brief Slot to allow applications to store custom data */
};

/* Generic functions for all display managers */
DM_EXPORT extern void dm_set_perspective(struct dm *dmp, int perspective_flag);
DM_EXPORT extern void dm_set_proj_mat(struct dm *dmp, mat_t pmat);
DM_EXPORT extern void dm_set_view_mat(struct dm *dmp, mat_t vmat);
DM_EXPORT extern void dm_set_background_rgb(struct dm *dmp, unsigned char r, unsigned char g, unsigned char b);
DM_EXPORT extern void dm_set_foreground_rgb(struct dm *dmp, unsigned char r, unsigned char g, unsigned char b);
DM_EXPORT extern void dm_set_default_draw_width(struct dm *dmp, fastf_t draw_width);
DM_EXPORT extern void dm_set_default_fontsize(struct dm *dmp, int fontsize);
DM_EXPORT extern int            dm_get_perspective(struct dm *dmp);
DM_EXPORT extern mat_t          dm_get_proj_mat(struct dm *dmp);
DM_EXPORT extern mat_t          dm_get_view_mat(struct dm *dmp);
DM_EXPORT extern unsigned char *dm_get_background_rgb(struct dm *dmp);
DM_EXPORT extern unsigned char *dm_get_foreground_rgb(struct dm *dmp);
DM_EXPORT extern fastf_t        dm_get_default_draw_width(struct dm *dmp, fastf_t draw_width);
DM_EXPORT extern int            dm_get_default_fontsize(struct dm *dmp);

DM_EXPORT extern struct bu_attribute_value_set *dm_get_extra_settings(struct dm *dmp, const char *key);
DM_EXPORT extern const char *dm_get_extra_setting(struct dm *dmp, const char *key);
DM_EXPORT extern int         dm_set_extra_setting(struct dm *dmp, const char *key, const char *val);

/* Object manipulators */
DM_EXPORT extern struct dm_display_list *dm_obj_add(struct dm *dmp, const char *handle, int style_type, struct bn_vlist *vlist);
DM_EXPORT extern struct dm_display_list *dm_obj_find(struct dm *dmp, const char *handle);
DM_EXPORT extern void                    dm_obj_remove(struct dm *dmp, const char *handle);

DM_EXPORT extern void   dm_toggle_obj_dirty(struct dm *dmp, const char *handle, int dirty_flag_val);
DM_EXPORT extern void   dm_toggle_obj_visible(struct dm *dmp, const char *handle, int visibility_flag_val);
DM_EXPORT extern void   dm_toggle_obj_highlight(struct dm *dmp, const char *handle, int highlight_flag_val);
DM_EXPORT extern void   dm_toggle_obj_transparency(struct dm *dmp, const char *handle, int visibility_flag_val);

DM_EXPORT extern void   dm_set_obj_rgb(struct dm *dmp, const char *handle, unsigned char r, unsigned char g, unsigned char b);
DM_EXPORT extern void   dm_set_obj_draw_width(struct dm *dmp, const char *handle, fastf_t draw_width);
DM_EXPORT extern void   dm_set_obj_fontsize(struct dm *dmp, const char *handle, int fontsize);
DM_EXPORT extern void   dm_set_obj_localmat(struct dm *dmp, const char *handle, mat_t matrix);

DM_EXPORT extern struct bu_attribute_value_set *dm_get_obj_extra_settings(struct dm *dmp, const char *handle);
DM_EXPORT extern int dm_set_obj_extra_setting(struct dm *dmp, const char *handle, const char *key, const char *val);

/* Display Manager / OS type aware functions */
DM_EXPORT extern int   dm_init(struct dm *dmp);
DM_EXPORT extern int   dm_close(struct dm *dmp);
DM_EXPORT extern int   dm_refresh(struct dm *dmp);
DM_EXPORT extern void *dm_canvas(struct dm *dmp);
DM_EXPORT extern int   dm_change_type(struct dm *dmp, int dm_t);
DM_EXPORT extern int   dm_get_image(struct dm *dmp, icv_image_t *image);
DM_EXPORT extern int   dm_get_obj_image(struct dm *dmp, const char *handle, icv_image_t *image);

#endif /* DM_H */

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
