/*                            D B . H
 * BRL-CAD
 *
 * Copyright (c) 1985-2007 United States Government as represented by
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
/** @addtogroup db4 */
/** @{ */
/** @file db.h
 * @brief
 *		GED Database Format v4
 *
 * All records are rounded up to have a fixed length;  each such
 * database record is also known as a "granule", and is the smallest
 * unit of database storage.
 *
 * Every granule can be identified by the first byte, which can
 * be accessed by the u_id name.  Note that the u_id field is not
 * valid when writing the actual data for splines.
 *
 * Each granule is read into a "union record", and is then processed
 * based on the value of u_id.  Each granule will have one of these formats:
 *	A Free record
 *	An ID record
 *	A SOLID record
 *	A COMBINATION record, followed by multiple
 *		MEMBER records
 *	An ARS `A' (header) record, followed by multiple
 *		ARS `B' (data) records
 *	A Polygon header record, followed by multiple
 *		Polygon data records
 *      A B-spline solid header record, followed by multiple
 *		B-spline surface records, followed by
 *			d_kv_size[0] floats,
 *			d_kv_size[1] floats,
 *			padded to d_nknots granules, followed by
 *			ctl_size[0]*ctl_size[1]*geom_type floats,
 *			padded to d_nctls granules.
 *
 * The records are stored as binary records corresponding to PDP-11 and
 * VAX C structs, so padding must be supplied explicitly for alignment.
 *
 * For the time being, the representation of the floating point numbers
 * in the database is machine-specific, requiring conversion to ASCII
 * (via g2asc) and back to binary (via asc2g) when exchanging between
 * machines of dissimilar types.  In time, an external representation
 * for floats might be implemented.
 *
 *  @author
 *	Michael John Muuss
 *
 *  @par Source
 *	SECAD/VLD Computing Consortium, Bldg 394
 *@n	The U. S. Army Ballistic Research Laboratory
 *@n	Aberdeen Proving Ground, Maryland  21005
 *
 *  $Header$
 */

#ifndef DB_H
#define DB_H seen

#define NAMESIZE		16

/*
 *  Define the database format for storing binary floating point values.
 *  The ultimate intention is to store everything in 8 bytes, using
 *  IEEE double precision in network order.
 */
#if defined(CRAY)
typedef double dbfloat_t;
#else
typedef float dbfloat_t;
#endif

#define NAMEMOVE(from,to)	(void)strncpy(to, from, NAMESIZE)

#define DB_MINREC	128

#if !defined(RECORD_DEFINED) || !defined(__STDC__)
#define RECORD_DEFINED
union record  {

	char	u_id;		/* To differentiate record types */
#define ID_IDENT	'I'
#define ID_SOLID	'S'
#define ID_COMB		'C'
#define ID_MEMB		'M'
#define ID_ARS_A	'A'
#define ID_ARS_B	'B'
#define ID_FREE		'F'	/* Free record -- ignore */
#define ID_P_HEAD	'P'	/* Polygon header */
#define ID_P_DATA	'Q'	/* Polygon data record */
#define ID_BSOLID	'b'	/* B-spline solid.  multiple surfs */
#define ID_BSURF	'D'     /* d_spline surface header */
#define ID_MATERIAL	'm'	/* Material description record */
#define DBID_STRSOL	's'	/* String solid description */
#define DBID_ARBN	'n'	/* Convex polyhedron with N faces */
#define DBID_PIPE	'w'	/* pipe (wire) solid */
#define DBID_PARTICLE	'p'	/* a particle (lozenge) */
#define DBID_NMG	'N'	/* NMG solid */
#define	DBID_SKETCH	'd'	/* 2D sketch */
#define	DBID_EXTR	'e'	/* solid of extrusion */
#define DBID_CLINE	'c'	/* FASTGEN4 CLINE solid */
#define	DBID_BOT	't'	/* Bag o' triangles */

	char	u_size[DB_MINREC];	/* Minimum record size */

	struct ident  {
		char	i_id;		/* ID_IDENT */
		char	i_units;	/* units */
/* Values of 0..5 are fixed for file compat with Release 2.3 through 4.5 */
#define ID_NO_UNIT	0		/* unspecified */
#define ID_MM_UNIT	1		/* milimeters (preferred) */
#define ID_CM_UNIT	2		/* centimeters */
#define ID_M_UNIT	3		/* meters */
#define ID_IN_UNIT	4		/* inches */
#define ID_FT_UNIT	5		/* feet */
/* These new values 6..9 added in Release 5.0 */
#define ID_UM_UNIT	6		/* micrometers */
#define ID_KM_UNIT	7		/* kilometers */
#define ID_YD_UNIT	8		/* yards */
#define ID_MI_UNIT	9		/* miles */
		char	i_version[6];	/* Version code of Database format */
#define ID_VERSION	"v4"		/* Current Version */
		char	i_title[72];	/* Title or description */
		/* For the future */
		char	i_byteorder;	/* Byte ordering */
#define ID_BY_UNKNOWN	0		/* unknown */
#define ID_BY_VAX	1		/* VAX (Little Endian) */
#define ED_BY_IBM	2		/* IBM (Big Endian) */
		char	i_floattype;	/* Floating point type */
#define ID_FT_UNKNOWN	0		/* unknown */
#define ID_FT_VAX	1		/* VAX */
#define ID_FT_IBM	2		/* IBM */
#define ID_FT_IEEE	3		/* IEEE */
#define ID_FT_CRAY	4		/* Cray */
	} i;

	struct solidrec  {
		char	s_id;		/* ID_SOLID */
		char	s_type;		/* GED primitive type */
/* also TOR 	16	   toroid */
#define GENTGC	18	/* supergeneralized TGC; internal form */
#define GENELL	19	/* ready for drawing ELL:  V,A,B,C */
#define GENARB8	20	/* generalized ARB8:  V, and 7 other vectors */
#define	ARS	21	/* HACK arbitrary triangular-surfaced polyhedron */
#define ARSCONT 22	/* HACK extension record type for ARS solid */
#define HALFSPACE 24	/* half-space */
#define SPLINE   25	/* HACK and trouble */
#define RPC	26	/* Right Parabolic Cylinder */
#define RHC	27	/* Right Parabolic Cylinder */
#define EPA	28	/* Elliptical Paraboloid */
#define EHY	29	/* Elliptical Hyperboloid */
#define ETO	30	/* Elliptical Torus */
#define GRP	31	/* Grip pseudo solid */
#define SUPERELL 32     /* Superquadtratic Ellipsoid */
		char	s_name[NAMESIZE];	/* unique name */
		short	s_cgtype;		/* COMGEOM solid type */
#define RPP	1	/* axis-aligned rectangular parallelopiped */
#define BOX	2	/* arbitrary rectangular parallelopiped */
#define RAW	3	/* right-angle wedge */
#define ARB4	4	/* tetrahedron */
#define ARB5	5	/* pyramid */
#define ARB6	6	/* extruded triangle */
#define ARB7	7	/* weird 7-vertex shape */
#define ARB8	8	/* hexahedron */
#define ELL	9	/* ellipsoid */
#define ELL1	10	/* another ellipsoid definition */
#define SPH	11	/* sphere */
#define RCC	12	/* right circular cylinder */
#define REC	13	/* right elliptic sylinder */
#define TRC	14	/* truncated regular cone */
#define TEC	15	/* truncated elliptic cone */
#define TOR	16	/* toroid */
#define TGC	17	/* truncated general cone */
#define ELLG	23	/* comgeom version of GENELL ellipsoid */
		dbfloat_t	s_values[24];		/* parameters */
#define s_tgc_V	s_values[0]
#define s_tgc_H	s_values[3]
#define s_tgc_A s_values[6]
#define s_tgc_B s_values[9]
#define s_tgc_C s_values[12]
#define s_tgc_D s_values[15]

#define s_tor_V	s_values[0]
#define s_tor_H	s_values[3]
#define s_tor_A	s_values[6]
#define s_tor_B	s_values[9]
#define s_tor_C	s_values[12]
#define s_tor_D	s_values[15]
#define s_tor_E	s_values[18]
#define s_tor_F	s_values[21]

#define s_ell_V s_values[0]
#define s_ell_A s_values[3]
#define s_ell_B s_values[6]
#define s_ell_C s_values[9]

#define s_superell_V s_values[0]
#define s_superell_A s_values[3]
#define s_superell_B s_values[6]
#define s_superell_C s_values[9]
#define s_superell_n s_values[12]
#define s_superell_e s_values[13]

#define s_half_N s_values[0]
#define s_half_d s_values[3]

#define s_grip_C s_values[0]
#define s_grip_N s_values[3]
#define s_grip_m s_values[6]
	}  s;

	struct combination  {
		char	c_id;			/* ID_COMB */
		char	c_flags;		/* `R' if region, else ` ' */
#define DBV4_NON_REGION			' '
#define DBV4_NON_REGION_NULL		'\0'
#define DBV4_REGION			'R'
#define DBV4_REGION_FASTGEN_PLATE	'P'
#define DBV4_REGION_FASTGEN_VOLUME	'V'
		char	c_name[NAMESIZE];	/* unique name */
		short	c_regionid;		/* region ID code */
		short	c_aircode;		/* air space code */
		short	c_pad1;		/* was c_length, DEPRECATED: # of members */
		short	c_pad2;		/* was c_num, DEPRECATED */
		short	c_material;		/* (GIFT) material code */
		short	c_los;			/* equivalent LOS estimate */
		char	c_override;		/* non-0 ==> c_rgb is color */
		unsigned char c_rgb[3];		/* 0..255 color override */
		char	c_matname[32];		/* Reference: Material name */
		char	c_matparm[60];		/* String Material parms */
		char	c_inherit;		/* Inheritance property */
#define DB_INH_LOWER	0			/* Lower nodes override */
#define DB_INH_HIGHER	1			/* Higher nodes override */
	}  c;
	struct member  {
		char	m_id;			/* ID_MEMB */
		char	m_relation;		/* boolean operation */
#define INTERSECT	'+'
#define SUBTRACT	'-'
#define UNION		'u'
		char	m_brname[NAMESIZE];	/* DEPRECATED: name of this branch */
		char	m_instname[NAMESIZE];	/* name of referred-to obj. */
		short	m_pad1;
		dbfloat_t m_mat[16];		/* homogeneous trans matrix */
		short	m_pad2;	/* was m_num, DEPRECATED: COMGEOM solid # ref */
	}  M;

	struct material_rec {		/* whole record is DEPRECATED */
		char	md_id;		/* = ID_MATERIAL color override */
		char	md_flags;	/* UNUSED */
		short	md_low;		/* lower end of region IDs affected */
		short	md_hi;		/* upper end of region IDs affected */
		unsigned char md_r;
		unsigned char md_g;	/* color of these regions:  0..255 */
		unsigned char md_b;
		char	md_material[100]; /* UNUSED now */
	} md;

	struct B_solid {
		char	B_id;		/* = ID_BSOLID */
		char	B_pad;
		char	B_name[NAMESIZE];
		short	B_nsurf;	/* #  of surfaces in this solid */
		dbfloat_t B_resolution;	/* resolution of flatness */
	} B;
	struct b_surf {
		char    d_id;		/* = ID_BSURF */
		short   d_order[2];	/* order of u and v directions */
		short   d_kv_size[2];	/* knot vector size  (u and v) */
		short   d_ctl_size[2];  /* control mesh size ( u and v) */
		short   d_geom_type;	/* geom type 3 or 4 */
		short	d_nknots;	/* # granules of knots */
		short	d_nctls;	/* # granules of ctls */
	} d;
	/*
	 * The b_surf record is followed by
	 * d_nknots granules of knot vectors (first u, then v),
	 * and then by d_nctls granules of control mesh information.
	 * Note that neither of these have an ID field!
	 */

	struct polyhead  {
		char	p_id;		/* = POLY_HEAD */
		char	p_pad1;
		char	p_name[NAMESIZE];
	} p;
	struct polydata  {
		char	q_id;		/* = POLY_DATA */
		char	q_count;	/* # of vertices <= 5 */
		dbfloat_t q_verts[5][3]; /* Actual vertices for this polygon */
		dbfloat_t q_norms[5][3]; /* Normals at each vertex */
	} q;

	struct ars_rec  {
		char	a_id;			/* ID_ARS_A */
		char	a_type;			/* primitive type */
		char	a_name[NAMESIZE];	/* unique name */
		short	a_m;			/* # of curves */
		short	a_n;			/* # of points per curve */
		short	a_curlen;		/* # of granules per curve */
		short	a_totlen;		/* # of granules for ARS */
		/* Remainder are unused, and exist for ?compatability */
		short	a_pad;
		dbfloat_t a_xmax;		/* max x coordinate */
		dbfloat_t a_xmin;		/* min x coordinate */
		dbfloat_t a_ymax;		/* max y coordinate */
		dbfloat_t a_ymin;		/* min y coordinate */
		dbfloat_t a_zmax;		/* max z coordinate */
		dbfloat_t a_zmin;		/* min z coordinate */
	}  a;
	struct ars_ext  {
		char	b_id;			/* ID_ARS_B */
		char	b_type;			/* primitive type */
		short	b_n;			/* current curve # */
		short	b_ngranule;		/* curr. granule for curve */
		short	b_pad;
		dbfloat_t	b_values[8*3];		/* vectors */
	}  b;
	/*
	 *  All records below here are in machine-independent format.
	 */
	struct strsol  {
		char	ss_id;			/* ID_STRSOL */
		char	ss_pad;
		char	ss_name[NAMESIZE];	/* solid name */
#define DB_SS_NGRAN	8			/* All STRSOL's have this many granules */
#define DB_SS_LEN	(DB_SS_NGRAN*DB_MINREC-2*NAMESIZE-2)
		char	ss_keyword[NAMESIZE];	/* solid keyword */
		char	ss_args[4];		/* DB_SS_LEN bytes of str args */
	}  ss;
	struct arbn_rec  {
		char	n_id;			/* DBID_ARBN */
		char	n_pad;
		char	n_name[NAMESIZE];
		unsigned char	n_neqn[4];	/* # equations which follow */
		unsigned char	n_grans[4];	/* # eqn granules to follow */
		/* Note that eqn granules are in "network" byte order */
	}  n;
	/* new pipe solid */
	struct pipewire_rec  {
		char	pwr_id;			/* DBID_PIPE */
		char	pwr_pad;
		char	pwr_name[NAMESIZE];
		unsigned char	pwr_pt_count[4]; /* number of points in this pipe solid */
		unsigned char	pwr_count[4];	/* # additional granules */
		struct exported_pipept  {
			unsigned char	epp_coord[8*3];
			unsigned char	epp_bendradius[8];
			unsigned char	epp_id[8];
			unsigned char	epp_od[8];
		} pwr_data[1];			/* mach indep segments */
	}  pwr;
	struct particle_rec  {
		char	p_id;			/* DBID_PARTICLE */
		char	p_pad;
		char	p_name[NAMESIZE];
		unsigned char	p_v[8*3];	/* vertex (mach indep fmt) */
		unsigned char	p_h[8*3];	/* height vector */
		unsigned char	p_vrad[8];	/* radius at vertex */
		unsigned char	p_hrad[8];	/* radius at end of height */
	}  part;
	/* Version 0 is Release 4.0 to 4.2, Version 1 is Release 4.4 */
	struct nmg_rec  {
		char	N_id;			/* DBID_NMG */
		char	N_version;		/* Version indicator */
		char	N_name[NAMESIZE];
		char	N_pad2[2];		/* neatness */
		unsigned char	N_count[4];	/* # additional granules */
		unsigned char	N_structs[26*4];/* # of structs needed */
	} nmg;

	/* Solid of extrusion */
	struct extr_rec	{
		char	ex_id;			/* DBID_EXTR */
		char	ex_pad;
		char	ex_name[NAMESIZE];
		unsigned char	ex_V[8*3];	/* Vertex */
		unsigned char	ex_h[8*3];	/* extrusion vector */
		unsigned char	ex_uvec[8*3];	/* vector in U parameter direction */
		unsigned char	ex_vvec[8*3];	/* vector in V parameter direction */
		unsigned char	ex_key[4];	/* keypoint index */
		unsigned char	ex_count[4];	/* number of additional granules
						 * (for now, this will always be 1 ) */
	} extr;

	/* sketch */
	struct sketch_rec	{
		char	skt_id;			/* DBID_SKETCH */
		char	skt_pad;
		char	skt_name[NAMESIZE];
		unsigned char	skt_V[8*3];	/* vertex */
		unsigned char	skt_uvec[8*3];	/* vector in U parameter direction */
		unsigned char	skt_vvec[8*3];	/* vector in V parameter direction */
		unsigned char	skt_vert_count[4];	/* number of vertices in sketch */
		unsigned char	skt_seg_count[4];	/* number of segments in sketch */
		unsigned char	skt_count[4];		/* number of additional granules */
	} skt;

	/* FASTGEN4 CLINE element */
	struct cline_rec {
		char	cli_id;			/* DBID_CLINE */
		char	cli_pad;
		char	cli_name[NAMESIZE];
		unsigned char	cli_V[8*3];	/* vertex */
		unsigned char	cli_h[8*3];	/* height vector */
		unsigned char	cli_radius[8];	/* radius */
		unsigned char	cli_thick[8];	/* plate thickness (0 means volume mode) */
	} cli;

	/* Bag o' triangles
	 *  The vertices are stored in an array
	 *  The faces are arrays of three ints each indexing into the array of vertices
	 */
	struct bot_rec {
		char		bot_id;
		char		bot_pad;
		char		bot_name[NAMESIZE];
		unsigned char	bot_nrec[4];		/* number of additional records required */
		unsigned char	bot_orientation;	/* unoriented, ccw, or cw */
		unsigned char	bot_mode;		/* surface or volume */
		unsigned char	bot_err_mode;
		unsigned char	bot_num_verts[4];	/* number of vertices */
		unsigned char	bot_num_triangles[4];	/* number of triangles */
		unsigned char	bot_data[1];		/* the start of the data:
							 * num_verts*3*8 for the vertex floats
							 * num_triangles*3*4 for the triangle ints
							 * if mode is RT_BOT_PLATE, then additional data is:
							 * num_triangles*8 for the face thicknesses
							 * followed by a null terminated hex digit string "face_mode" (bit vector) */
	} bot;

};
#endif /* !RECORD_DEFINED || !__STDC__ */
#define DB_RECORD_NULL	((union record *)0)

/*
 *  Macros for providing function prototypes, regardless of whether
 *  the compiler understands them or not.
 *  It is vital that the argument list given for "args" be enclosed
 *  in parens.
 */
#if __STDC__
#	define	DB_ARGS(args)			args
#else
#	define	DB_ARGS(args)			()
#endif

/* convert dbfloat->fastf_t */
void rt_fastf_float DB_ARGS( (fastf_t *ff, const dbfloat_t *fp, int n) );

/* convert dbfloat mat->fastf_t */
void rt_mat_dbmat DB_ARGS( (fastf_t *ff, const dbfloat_t *dbp) );
void rt_dbmat_mat DB_ARGS( (dbfloat_t *dbp, const fastf_t *ff) );

#endif	/* DB_H */
/** @} */
/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
