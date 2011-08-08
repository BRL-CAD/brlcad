/*                           W D B . H
 * BRL-CAD
 *
 * Copyright (c) 1988-2011 United States Government as represented by
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
/** @addtogroup wdb */
/** @{ */
/** @file wdb.h
 *
 *  Interface structures and routines for libwdb
 *
 *  Note -
 *	Rather than using a stdio (FILE *),
 *	we now use a (struct rt_wdb *) parameter.
 *	Rather than calling fopen(), call wdb_fopen();
 *
 */

#ifndef __WDB_H__
#define __WDB_H__

#include "common.h"

/* interface headers */
#include "bu.h"
#include "bn.h"
#include "raytrace.h"
#include "rtgeom.h"

__BEGIN_DECLS

#ifndef WDB_EXPORT
#  if defined(_WIN32) && !defined(__CYGWIN__) && defined(BRLCAD_DLL)
#    ifdef WDB_EXPORT_DLL
#      define WDB_EXPORT __declspec(dllexport)
#    else
#      define WDB_EXPORT __declspec(dllimport)
#    endif
#  else
#    define WDB_EXPORT
#  endif
#endif

/*
 *  Macros for providing function prototypes, regardless of whether
 *  the compiler understands them or not.
 *  It is vital that the argument list given for "args" be enclosed
 *  in parens.
 */
#define WDB_EXTERN(type_and_name, args) extern type_and_name args
#define WDB_ARGS(args) args

/*
 * In-memory form of database combinations
 */
struct wmember {
    struct bu_list l;
    int wm_op;		/**< @brief  Boolean operation */
    mat_t wm_mat;	/**< @brief  FIXME: Should be a matp_t */
    char *wm_name;
};
#define WMEMBER_NULL	((struct wmember *)0)
#define WDB_CK_WMEMBER(_p)	BU_CKMAG(_p, WMEMBER_MAGIC, "wmember" );

/*
 * Definitions for pipe (wire) segments
 * FIXME: move to rtgeom.h?
 */

struct wdb_pipept {
    struct bu_list	l;		/**< @brief  doubly linked list support */
    point_t		pp_coord;	/**< @brief  "control" point for pipe solid */
    fastf_t		pp_id;		/**< @brief  inner diam, <=0 if solid (wire) */
    fastf_t		pp_od;		/**< @brief  pipe outer diam */
    fastf_t		pp_bendradius;	/**< @brief  bend radius to use for a bend at this point */
};

#define WDB_PIPESEG_NULL	((struct wdb_pipeseg *)0)

struct wdb_metaballpt {
    struct bu_list	l;
    int		type;
    fastf_t		fldstr;		/**< @brief  field strength */
    fastf_t		sweat;		/**< @brief  beta value used for metaball and blob evaluation */
    point_t		coord;
    point_t		coord2;
};
#define WDB_METABALLPT_TYPE_POINT 0x0
#define WDB_METABALLPT_TYPE_LINE 0x1
#define WDB_METABALLPT_NULL	((struct wdb_metaballpt *)0)


WDB_EXPORT WDB_EXTERN(int mk_id, (struct rt_wdb *fp, const char *title) );
WDB_EXPORT WDB_EXTERN(int mk_id_units, (struct rt_wdb *fp, const char *title, const char *units) );
WDB_EXPORT int mk_id_editunits(
    struct rt_wdb *fp,
    const char *title,
    double local2mm );

/*
 *  Solid conversion routines
 */
/* libwdb/wdb.c */
WDB_EXPORT WDB_EXTERN(int mk_half, (struct rt_wdb *fp, const char *name, const vect_t norm, double d) );
WDB_EXPORT int mk_grip(
    struct rt_wdb *wdbp,
    const char *name,
    const point_t center,
    const vect_t normal,
    const fastf_t magnitude );
WDB_EXPORT WDB_EXTERN(int mk_rpp, (struct rt_wdb *fp, const char *name, const point_t min,
				   const point_t max) );
WDB_EXPORT WDB_EXTERN(int mk_wedge, (struct rt_wdb *fp, const char *name, const point_t vert,
				     const vect_t xdirv, const vect_t zdirv,
				     fastf_t xlen, fastf_t ylen, fastf_t zlen,
				     fastf_t x_top_len) );
WDB_EXPORT WDB_EXTERN(int mk_arb4, (struct rt_wdb *fp, const char *name, const fastf_t *pts4) );
WDB_EXPORT WDB_EXTERN(int mk_arb5, (struct rt_wdb *fp, const char *name, const fastf_t *pts5) );
WDB_EXPORT WDB_EXTERN(int mk_arb6, (struct rt_wdb *fp, const char *name, const fastf_t *pts6) );
WDB_EXPORT WDB_EXTERN(int mk_arb7, (struct rt_wdb *fp, const char *name, const fastf_t *pts7) );
WDB_EXPORT WDB_EXTERN(int mk_arb8, (struct rt_wdb *fp, const char *name, const fastf_t *pts8) );
WDB_EXPORT WDB_EXTERN(int mk_sph, (struct rt_wdb *fp, const char *name, const point_t center,
				   fastf_t radius) );
WDB_EXPORT WDB_EXTERN(int mk_ell, (struct rt_wdb *fp, const char *name, const point_t center,
				   const vect_t a, const vect_t b, const vect_t c) );
WDB_EXPORT WDB_EXTERN(int mk_tor, (struct rt_wdb *fp, const char *name, const point_t center,
				   const vect_t inorm, double r1, double r2) );
WDB_EXPORT WDB_EXTERN(int mk_rcc, (struct rt_wdb *fp, const char *name, const point_t base,
				   const vect_t height, fastf_t radius) );
WDB_EXPORT WDB_EXTERN(int mk_tgc, (struct rt_wdb *fp, const char *name, const point_t base,
				   const vect_t height, const vect_t a, const vect_t b,
				   const vect_t c, const vect_t d) );
WDB_EXPORT WDB_EXTERN(int mk_cone, (struct rt_wdb *fp, const char *name, const point_t base,
				    const vect_t dirv, fastf_t height, fastf_t rad1,
				    fastf_t rad2) );
WDB_EXPORT WDB_EXTERN(int mk_trc_h, (struct rt_wdb *fp, const char *name, const point_t base,
				     const vect_t height, fastf_t radbase, fastf_t radtop) );
WDB_EXPORT WDB_EXTERN(int mk_trc_top, (struct rt_wdb *fp, const char *name, const point_t ibase,
				       const point_t itop, fastf_t radbase, fastf_t radtop) );
WDB_EXPORT int mk_rpc(
    struct rt_wdb *wdbp,
    const char *name,
    const point_t vert,
    const vect_t height,
    const vect_t breadth,
    double half_w );
WDB_EXPORT int mk_rhc(
    struct rt_wdb *wdbp,
    const char *name,
    const point_t vert,
    const vect_t height,
    const vect_t breadth,
    fastf_t	half_w,
    fastf_t asymp );
WDB_EXPORT int mk_epa(
    struct rt_wdb *wdbp,
    const char *name,
    const point_t vert,
    const vect_t height,
    const vect_t breadth,
    fastf_t r1,
    fastf_t r2 );
WDB_EXPORT int mk_ehy(
    struct rt_wdb *wdbp,
    const char *name,
    const point_t vert,
    const vect_t height,
    const vect_t breadth,
    fastf_t r1,
    fastf_t r2,
    fastf_t c );
WDB_EXPORT int mk_hyp(
    struct rt_wdb *wdbp,
    const char *name,
    const point_t vert,
    const vect_t height_vector,
    const vect_t vectA,
    fastf_t magB,
    fastf_t base_neck_ratio );
WDB_EXPORT int mk_eto(
    struct rt_wdb *wdbp,
    const char *name,
    const point_t vert,
    const vect_t norm,
    const vect_t smajor,
    fastf_t rrot,
    fastf_t sminor );
WDB_EXPORT int mk_metaball(
    struct rt_wdb *wdbp,
    const char *name,
    const size_t nctlpt,	/* number of control points */
    const int method,		/* metaball rendering method */
    const fastf_t threshold,
    const fastf_t *verts[5] );	/* X, Y, Z, fldstr, goo/Beta */

WDB_EXPORT WDB_EXTERN(int mk_arbn, (struct rt_wdb *fp, const char *name, size_t neqn, plane_t eqn[]) );
WDB_EXPORT WDB_EXTERN(int mk_ars, (struct rt_wdb *fp, const char *name, size_t ncurves, size_t pts_per_curve, fastf_t *curves[]) );

/**
 * Given the appropriate parameters, makes the non-geometric
 * constraint object and writes it to the database using
 * wdb_put_internal.  Only supported on database version 5 or above
 */
WDB_EXPORT WDB_EXTERN(int mk_constraint, (struct rt_wdb *wdbp, const char *name, const char *expr));


typedef enum {
    WDB_BINUNIF_FLOAT,
    WDB_BINUNIF_DOUBLE,
    WDB_BINUNIF_CHAR,
    WDB_BINUNIF_UCHAR,
    WDB_BINUNIF_SHORT,
    WDB_BINUNIF_USHORT,
    WDB_BINUNIF_INT,
    WDB_BINUNIF_UINT,
    WDB_BINUNIF_LONG,
    WDB_BINUNIF_ULONG,
    WDB_BINUNIF_LONGLONG,
    WDB_BINUNIF_ULONGLONG,
    WDB_BINUNIF_INT8,
    WDB_BINUNIF_UINT8,
    WDB_BINUNIF_INT16,
    WDB_BINUNIF_UINT16,
    WDB_BINUNIF_INT32,
    WDB_BINUNIF_UINT32,
    WDB_BINUNIF_INT64,
    WDB_BINUNIF_UINT64,
    WDB_BINUNIF_FILE_FLOAT,
    WDB_BINUNIF_FILE_DOUBLE,
    WDB_BINUNIF_FILE_CHAR,
    WDB_BINUNIF_FILE_UCHAR,
    WDB_BINUNIF_FILE_SHORT,
    WDB_BINUNIF_FILE_USHORT,
    WDB_BINUNIF_FILE_INT,
    WDB_BINUNIF_FILE_UINT,
    WDB_BINUNIF_FILE_LONG,
    WDB_BINUNIF_FILE_ULONG,
    WDB_BINUNIF_FILE_LONGLONG,
    WDB_BINUNIF_FILE_ULONGLONG,
    WDB_BINUNIF_FILE_INT8,
    WDB_BINUNIF_FILE_UINT8,
    WDB_BINUNIF_FILE_INT16,
    WDB_BINUNIF_FILE_UINT16,
    WDB_BINUNIF_FILE_INT32,
    WDB_BINUNIF_FILE_UINT32,
    WDB_BINUNIF_FILE_INT64,
    WDB_BINUNIF_FILE_UINT64
} wdb_binunif;
WDB_EXPORT WDB_EXTERN(int mk_binunif, (struct rt_wdb *fp, const char *name, const genptr_t data, wdb_binunif data_type, long count) );

/* bot.c */
WDB_EXPORT int
mk_bot(
    struct rt_wdb *fp,			/**< database file pointer to write to */
    const char *name,			/**< name of bot object to write out */
    unsigned char	mode,		/**< bot mode */
    unsigned char	orientation,	/**< bot orientation */
    unsigned char	error_mode,	/**< may be used to indicate error handling (ignored for now) */
    size_t		num_vertices,	/**< number of vertices */
    size_t		num_faces,	/**< number of faces */
    fastf_t		*vertices,	/**< array of floats for vertices [num_vertices*3] */
    int			*faces,		/**< array of ints for faces [num_faces*3] */
    fastf_t		*thickness,	/**< array of plate mode
					 * thicknesses (corresponds to
					 * array of faces) NULL for
					 * modes RT_BOT_SURFACE and
					 * RT_BOT_SOLID.
					 */
    struct bu_bitv	*face_mode 	/**< a flag for each face
					 * indicating thickness is
					 * appended to hit point,
					 * otherwise thickness is
					 * centered about hit point
					 */
    );
WDB_EXPORT int
mk_bot_w_normals(
    struct rt_wdb *fp,			/**< database file pointer to write to */
    const char *name,			/**< name of bot object to write out */
    unsigned char	mode,		/**< bot mode */
    unsigned char	orientation,	/**< bot orientation */
    unsigned char	flags,		/**< additional bot flags */
    size_t		num_vertices,	/**< number of bot vertices */
    size_t		num_faces,	/**< number of bot faces */
    fastf_t		*vertices,	/**< array of floats for vertices [num_vertices*3] */
    int			*faces,		/**< array of ints for faces [num_faces*3] */
    fastf_t		*thickness,	/**< array of plate mode
					 * thicknesses (corresponds to
					 * array of faces) NULL for
					 * modes RT_BOT_SURFACE and
					 * RT_BOT_SOLID.
					 */
    struct bu_bitv	*face_mode,	/**< a flag for each face
					 * indicating thickness is
					 * appended to hit point,
					 * otherwise thickness is
					 * centered about hit point
					 */
    size_t		num_normals,	/**< number of unit normals in normals array */
    fastf_t		*normals,	/**< array of floats for normals [num_normals*3] */
    int			*face_normals	/**< array of ints (indices
					 * into normals array), must
					 * have 3*num_faces entries
					 */
    );

/* brep.cpp */
WDB_EXPORT int mk_brep( struct rt_wdb* wdbp, const char* name, ON_Brep* brep );

/* nurb.c */
WDB_EXPORT int mk_bspline( struct rt_wdb *wdbp, const char *name, struct face_g_snurb **surfs );

/* nmg.c */
WDB_EXPORT int mk_nmg( struct rt_wdb *filep, const char *name, struct model *m );
WDB_EXPORT int mk_bot_from_nmg( struct rt_wdb *ofp, const char *name, struct shell *s );


/* skt.c */
WDB_EXPORT int mk_sketch(
    struct rt_wdb *fp,
    const char *name,
    const struct rt_sketch_internal *skt );

/* extr.c */
WDB_EXPORT int mk_extrusion(
    struct rt_wdb *fp,
    const char *name,
    const char *sketch_name,
    const point_t V,
    const vect_t h,
    const vect_t u_vec,
    const vect_t v_vec,
    int keypoint );

/* cline.c */
WDB_EXPORT int mk_cline(
    struct rt_wdb *fp,
    const char *name,
    const point_t V,
    const vect_t height,
    fastf_t radius,
    fastf_t thickness );

/* pipe.c */
WDB_EXPORT WDB_EXTERN(int mk_particle, (struct rt_wdb *fp, const char *name, point_t vertex,
					vect_t height, double vradius, double hradius) );
WDB_EXPORT WDB_EXTERN(int mk_pipe, (struct rt_wdb *fp, const char *name, struct bu_list *headp) );
WDB_EXPORT void mk_pipe_free( struct bu_list *headp );
WDB_EXPORT void mk_add_pipe_pt(
    struct bu_list *headp,
    const point_t coord,
    double od,
    double id,
    double bendradius );
WDB_EXPORT void mk_pipe_init( struct bu_list *headp );

/* strsol primitives */
WDB_EXPORT WDB_EXTERN(int mk_dsp, (struct rt_wdb *fp, const char *name, const char *file,
				   size_t xdim, size_t ydim, const matp_t mat));
WDB_EXPORT WDB_EXTERN(int mk_ebm, (struct rt_wdb *fp, const char *name, const char *file,
				   size_t xdim, size_t ydim, fastf_t tallness, const matp_t mat));
WDB_EXPORT WDB_EXTERN(int mk_vol, (struct rt_wdb *fp, const char *name, const char *file,
				   size_t xdim, size_t ydim, size_t zdim, size_t lo, size_t hi,
				   const vect_t cellsize, const matp_t mat));
WDB_EXPORT WDB_EXTERN(int mk_submodel, (struct rt_wdb *fp, const char *name, const char *file,
					const char *treetop, int meth));


/* mater.c */
WDB_EXPORT int mk_write_color_table( struct rt_wdb *ofp );


/*
 *  Combination (region&group) construction routines
 *
 *  First you build a list of nodes with mk_addmember,
 *  then you output the combination.
 */
WDB_EXPORT WDB_EXTERN (struct wmember *mk_addmember,
		       (const char	*name,
			struct bu_list	*headp,
			mat_t		mat,
			int		op));

#define mk_lcomb(_fp, _name, _headp, _rf, _shadername, _shaderargs, _rgb, _inh)	\
	mk_comb(_fp, _name, &((_headp)->l), _rf, _shadername, _shaderargs, \
		_rgb, 0, 0, 0, 0, _inh, 0, 0)

/* mk_lrcomb() would not append, and did not have GIFT semantics */
/* mk_lrcomb() had (struct wmember *) head, need (struct bu_list *) */
#define mk_lrcomb(fp, name, _headp, region_flag, shadername, shaderargs, rgb, id, air, material, los, inherit_flag )	\
	mk_comb( fp, name, &((_headp)->l), region_flag, shadername, shaderargs, \
		rgb, id, air, material, los, inherit_flag, 0, 0 )

WDB_EXPORT int mk_comb(
    struct rt_wdb	*wdbp,			/**< database to write to */
    const char		*combname,		/**< name of the combination */
    struct bu_list	*headp,			/**< Made by mk_addmember() */
    int			region_kind,		/**< 1 => region.  'P' and 'V' for FASTGEN */
    const char		*shadername,		/**< shader name, or NULL */
    const char		*shaderargs,		/**< shader args, or NULL */
    const unsigned char	*rgb,			/**< NULL => no color */
    int			id,			/**< region_id */
    int			air,			/**< aircode */
    int			material,		/**< GIFTmater */
    int			los,			/**< line-of-sight thickness equivalence */
    int			inherit,		/**< whether objects below inherit from this comb */
    int			append_ok,		/**< 0 = obj must not exit */
    int			gift_semantics);	/**<  0 = pure, 1 = gift */

/** Convenience routines for quickly making combinations */
WDB_EXPORT int mk_comb1( struct rt_wdb *fp,
			 const char *combname,
			 const char *membname,
			 int regflag );
WDB_EXPORT int
mk_region1(
    struct rt_wdb *fp,
    const char *combname,
    const char *membname,
    const char *shadername,
    const char *shaderargs,
    const unsigned char *rgb );

/* Values for wm_op.  These must track db.h */
#define WMOP_INTERSECT	'+'
#define WMOP_SUBTRACT	'-'
#define WMOP_UNION	'u'

/* Convienient definitions */
#define mk_lfcomb(fp, name, headp, region)		mk_lcomb( fp, name, headp, \
	region, (char *)0, (char *)0, (unsigned char *)0, 0 );

/*
 *  Routines to establish conversion factors
 */
WDB_EXPORT WDB_EXTERN(int mk_conversion, (char *units_string) );
WDB_EXPORT WDB_EXTERN(int mk_set_conversion, (double val) );

/**
 * This internal variable should not be directly modified;
 * call mk_conversion() or mk_set_conversion() instead.
 */
WDB_EXPORT extern double	mk_conv2mm;		/**< @brief Conversion factor to mm */

/**
 *  Set this variable to either 4 or 5, depending on which version of
 *  the database you wish to write.
 */
WDB_EXPORT extern int	mk_version;		/**< @brief  Which version database to write */

/*
 *  Internal routines
 */
WDB_EXPORT void mk_freemembers( struct bu_list *headp );

#define mk_export_fwrite(wdbp, name, gp, id)	wdb_export(wdbp, name, gp, id, mk_conv2mm)

/*
 *	Dynamic geometry routines
 */
WDB_EXPORT WDB_EXTERN( int make_hole, ( struct rt_wdb *wdbp,
					point_t hole_start,
					vect_t hole_depth,
					fastf_t hole_radius,
					int num_objs,
					struct directory **dp ) );

WDB_EXPORT WDB_EXTERN( int make_hole_in_prepped_regions, ( struct rt_wdb *wdbp,
							   struct rt_i *rtip,
							   point_t hole_start,
							   vect_t hole_depth,
							   fastf_t radius,
							   struct bu_ptbl *regions ) );


__END_DECLS

#endif /* __WDB_H__ */

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
