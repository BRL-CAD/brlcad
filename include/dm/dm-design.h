/*                     D M - D E S I G N . H
 * BRL-CAD
 *
 * Copyright (c) 2016 United States Government as represented by
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
/** @file dm-design.h
 *
 * Design work on an updated API for libdm.  This is currently just
 * speculative work and should not be regarded as any sort of active,
 * usable API.
 *
 */

#ifndef DM_DM_DESIGN_H
#define DM_DM_DESIGN_H

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

/* TODO - this will probably need to be in bview.h for common access */
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
    struct bu_attribute_value_set *obj_extra_settings;	/**< @brief All settings (generic and DMTYPE specific) listed here. */
    void 		*client_data;		/**< @brief Slot to allow applications to supply custom data */
};

struct dm {
    uint32_t dm_magic;
    int				 dm_type;	/**< @brief drawing canvas type (X, OSG, Qt, txt, etc.) currently in use by display manager */
    char 			*handle;        /**< @brief short name of device */
    int 			 perspective;	/**< @brief !0 means perspective on */
    mat_t			 view_matrix;   /**< @brief view matrix for the default camera */
    mat_t			 proj_matrix;   /**< @brief projection matrix for the default camera */
    void			*dm_data;       /**< @brief pointer to the actual low level, platform specific data (X window, OSG viewer, internal lists, etc.) */
    int				 is_embedded;   /**< @brief determine if the display manager is stand-alone or embedded (impacts event handling) */
    void			*parent_info;   /**< @brief if dm is embedded, parent_info must contain all the info necessary for libdm to embed the Window */
    struct bu_ptbl		*dm_l;		/**< @brief Display list for this view */
    int 			 fontsize;	/**< @brief !0 override's the auto font size */
    unsigned char 		 dm_bg[3];	/**< @brief background color */
    unsigned char 		 dm_fg[3];	/**< @brief default foreground color */
    fastf_t			 draw_width;	/**< @brief Default point radius/line width */
    int 			 width;
    int 			 height;
    struct bu_attribute_value_set *dm_settings;	/**< @brief All settings (generic and DMTYPE specific) listed here. */
    void 			*client_data;	/**< @brief Slot to allow applications to store custom data */
};

const char *dm_common_reserved_settings[] = {
    "perspective"    "Enable/disable perspective mode.  Specifics of the perspective mode are controlled by the projection matrix."
    "proj_mat"       "Projection matrix, used for perspective mode."
    "view_mat"       "View matrix - controls the \"camera\" position in space."
    "background_rgb" "Background color, specified using Red/Green/Blue color values"
    "foreground_rgb" "Default color for foreground objects, specified using Red/Green/Blue color values"
    "draw_width"     "Default line width/point radius used when drawing objects."
    "fontsize"       "Default font size for text rendering."
    "width"          "Width of display window."
    "height"         "Height of display window."
    "\0"
}

const char *dm_obj_common_reserved_settings[] = {
    "local_mat"      "Local positioning matrix, used (for example) during object editing manipulations."
    "rgb"            "Object color, specified using Red/Green/Blue color values.  Defaults to geometry object color, if present."
    "draw_width"     "Local line width/point radius used when drawing objects."
    "fontsize"       "Local font size for text rendering."
    "dirty"          "Flag telling the display manager that the object state is out of sync with the visible state."
    "visible"        "Flag telling the display manager that the object is (or isn't) supposed to be visible in the view."
    "highlight"      "Flag telling the display manager to highlight this object."
    "\0"
}

/* Structure of dm will (hopefully) be internal to libdm, so use a typedef for the functions */
typdef struct dm dm_s;

/* Generic functions for all display managers */
DM_EXPORT extern void		dm_set_handle(dm_s *dmp, const char *handle);
DM_EXPORT extern char 	       *dm_get_handle(dm_s *dmp);
DM_EXPORT extern void           dm_set_perspective(dm_s *dmp, int perspective_flag);
DM_EXPORT extern int            dm_get_perspective(dm_s *dmp);
DM_EXPORT extern void           dm_set_proj_mat(dm_s *dmp, mat_t pmat);
DM_EXPORT extern matp_t         dm_get_proj_mat(dm_s *dmp);
DM_EXPORT extern void           dm_set_view_mat(dm_s *dmp, mat_t vmat);
DM_EXPORT extern matp_t         dm_get_view_mat(dm_s *dmp);
DM_EXPORT extern void           dm_set_background_rgb(dm_s *dmp, unsigned char r, unsigned char g, unsigned char b);
DM_EXPORT extern unsigned char *dm_get_background_rgb(dm_s *dmp);
DM_EXPORT extern void           dm_set_foreground_rgb(dm_s *dmp, unsigned char r, unsigned char g, unsigned char b);
DM_EXPORT extern unsigned char *dm_get_foreground_rgb(dm_s *dmp);
DM_EXPORT extern void           dm_set_default_draw_width(dm_s *dmp, fastf_t draw_width);
DM_EXPORT extern fastf_t        dm_get_default_draw_width(dm_s *dmp, fastf_t draw_width);
DM_EXPORT extern void           dm_set_default_fontsize(dm_s *dmp, int fontsize);
DM_EXPORT extern int            dm_get_default_fontsize(dm_s *dmp);
DM_EXPORT extern void           dm_set_width(dm_s *dmp, int width);
DM_EXPORT extern int		dm_get_width(dm_s *dmp);
DM_EXPORT extern void           dm_set_height(dm_s *dmp, int height);
DM_EXPORT extern int		dm_get_height(dm_s *dmp);


DM_EXPORT extern const char 		      **dm_get_reserved_settings(dm_s *dmp); /* Will be a combination of global and dm specific reserved settings */
DM_EXPORT extern int				dm_is_reserved_setting(dm_s *dmp, const char *key);
DM_EXPORT extern const char		       *dm_about_reserved_setting(dm_s *dmp, const char *key);
DM_EXPORT extern struct bu_attribute_value_set *dm_get_settings(dm_s *dmp, const char *key);
DM_EXPORT extern int                            dm_set_setting(dm_s *dmp, const char *key, const char *val);
DM_EXPORT extern const char                    *dm_get_setting(dm_s *dmp, const char *key);

/* Object manipulators */
DM_EXPORT extern int  dm_obj_add(dm_s *dmp, const char *handle, int style_type, struct bn_vlist *vlist, struct bu_ptbl *obj_set);
DM_EXPORT extern int  dm_obj_find(dm_s *dmp, const char *handle);
DM_EXPORT extern void dm_obj_remove(dm_s *dmp, const char *handle);

DM_EXPORT extern void           dm_set_obj_localmat(dm_s *dmp, const char *handle, mat_t matrix);
DM_EXPORT extern matp_t         dm_get_obj_localmat(dm_s *dmp, const char *handle);
DM_EXPORT extern void           dm_set_obj_rgb(dm_s *dmp, const char *handle, unsigned char r, unsigned char g, unsigned char b);
DM_EXPORT extern unsigned char *dm_get_obj_rgb(dm_s *dmp, const char *handle);
DM_EXPORT extern void           dm_set_obj_draw_width(dm_s *dmp, const char *handle, fastf_t draw_width);
DM_EXPORT extern fastf_t        dm_get_obj_draw_width(dm_s *dmp, const char *handle);
DM_EXPORT extern void           dm_set_obj_fontsize(dm_s *dmp, const char *handle, int fontsize);
DM_EXPORT extern int            dm_get_obj_fontsize(dm_s *dmp, const char *handle);
DM_EXPORT extern void           dm_set_obj_dirty(dm_s *dmp, const char *handle, int flag);
DM_EXPORT extern int            dm_get_obj_dirty(dm_s *dmp, const char *handle);
DM_EXPORT extern void           dm_set_obj_visible(dm_s *dmp, const char *handle, int flag);
DM_EXPORT extern int            dm_get_obj_visible(dm_s *dmp, const char *handle);
DM_EXPORT extern void           dm_set_obj_highlight(dm_s *dmp, const char *handle, int flag);
DM_EXPORT extern int            dm_get_obj_highlight(dm_s *dmp, const char *handle);

DM_EXPORT extern const char 		      **dm_get_obj_reserved_settings(dm_s *dmp);  /* Will be a combination of global and dm specific reserved settings */
DM_EXPORT extern int				dm_is_obj_reserved_setting(dm_s *dmp, const char *key);
DM_EXPORT extern const char 		       *dm_about_obj_reserved_setting(dm_s *dmp, const char *key);
DM_EXPORT extern struct bu_attribute_value_set *dm_get_obj_settings(dm_s *dmp, const char *handle);
DM_EXPORT extern int                            dm_set_obj_setting(dm_s *dmp, const char *handle, const char *key, const char *val);
DM_EXPORT extern const char                    *dm_get_obj_setting(dm_s *dmp, const char *handle, const char *key);

/* TODO The visibility of the framebuffer is handled like any other object, but it is likely necessary
 * to expose more of the details of the object to allow libfb to work properly?*/
/* Idle though - could an ascii raytrace (like the old GIFT output) be useful for "txt mode" debugging of raytracing? */
DM_EXPORT extern void 		*dm_get_framebuffer(dm_s *dmp);


/* Display Manager / OS type aware functions */
DM_EXPORT extern int   dm_init(dm_s *dmp, int dm_t, int embedded, void *parent_info);  /* TODO - need an actual public struct to hold parent info? */
DM_EXPORT extern int   dm_close(dm_s *dmp);
DM_EXPORT extern int   dm_refresh(dm_s *dmp);
DM_EXPORT extern void *dm_canvas(dm_s *dmp);  /* Exposes the low level drawing object (X window, OpenGL context, etc.) for custom drawing */
DM_EXPORT extern int   dm_get_type(dm_s *dmp);
DM_EXPORT extern int   dm_set_type(dm_s *dmp, int dm_t);
DM_EXPORT extern int   dm_get_image(dm_s *dmp, icv_image_t *image);
DM_EXPORT extern int   dm_get_obj_image(dm_s *dmp, const char *handle, icv_image_t *image);

#endif /* DM_DM_DESIGN_H */

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
