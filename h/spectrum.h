/*
 *			T A B L E . H
 * and
 *			S P E C T R U M . H
    ...soon to be separated...
 *
 *  A header file containing data structures to assist with
 *  recording spectral data.
 *  The overall notion is that each spectral sample should be
 *  as compact as possible (typically just an array of power levels),
 *  with all the context stored in one place.
 *
 *  NOTE that this is really a much more general mechanism than originally
 *  envisioned -- these structures and support routines apply to
 *  any measured "curve" or "function" or "table" with one independent
 *  variable and scalar dependent variable(s).
 *  It is unclear how to properly generalize the names....  Ahh, packaging.
 *  Perhaps the "table" package?
 *
 *  The context is kept in an 'rt_table' structure, and
 *  the data for one particular sample are kept in an 'rt_tabdata'
 *  structure.
 *
 *  The contents of the spectral sample in val[j] are interpreted
 *  in the interval (wavel[j]..wavel[j+1]).
 *  This value could be power, albedo, absorption, refractive index,
 *  or any other wavelength-specific parameter.
 *
 *  For example, if the val[] array contains power values, then
 *  val[j] contains the integral of the power from wavel[j] to wavel[j+1]
 *
 *  As an exmple, assume nwave=2, wavel[0]=500, wavel[1]=600, wavel[2]=700.
 *  Then val[0] would contain data for the 500 to 600nm interval,
 *  and val[1] would contain data for the 600 to 700nm interval.
 *  There would be no storage allocated for val[2] -- don't use it!
 *  There are several interpretations of this:
 *	1)  val[j] stores the total (integral, area) value for the interval, or
 *	2)  val[j] stores the average value across the interval.
 *
 *  The intervals need not be uniformly spaced; it is acceptable to
 *  increase wavelength sampling density around "important" frequencies.
 *  This will undoubtably complicate certain functions.
 *
 *  By convention, wavelength is stored in nanometers as an integer,
 *  and power is stored in Watts.
 *
 *  Wavelength Units -
 *	cm = 10** -2 m	centimeters
 *	mm = 10** -3 m	milimeters
 *	um = 10** -6 m	micrometers, or microns
 *	nm = 10** -9 m	nanometers
 *
 *  The Spectrum in nm -
 *	< 10 nm			X-Rays
 *	10 to 390		Ultraviolet
 *	390 to 770		Visible
 *	770 to 1500		Near IR
 *	1500 to 6,000		Middle IR	(includes 3-5 um band)
 *	6,000 to 40,000		Far IR		(includes 8-12 um band)
 *	40,000 to 1,000,000	Extreme IR
 *	> 10**6 nm		Microwaves	(300 GHz to 300 MHz)
 *	> 10**9 nm		Radio waves	(300 MHz and down)
 *
 *  It is tempting to store the wavelength in nanometers as an integer,
 *  but that might preclude very narrow-band calculations, such as might
 *  be required to study a single spectral line,
 *  and it might also preclude future extensions to X-rays.
 *  Because there are likely to be very few rt_table structures
 *  in use, the extra storage isn't a likely problem.
 *  The worst effect of this decision will be floating-point grunge
 *  when printing wavelengths, e.g. 650nm might print as 649.99nm.
 *  On the other hand, non-integer values might make it difficult to
 *  determine if two wavelengths from different curves were the "same",
 *  without introducing a wavelength "tolerance" notion.  Ugh.
 *
 *  Author -
 *	Michael John Muuss
 *  
 *  Source -
 *	The U. S. Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5068  USA
 *  
 *  Distribution Status -
 *	Public Domain, Distribution Unlimited.
 *
 *  $Header$
 */

struct rt_table {
	long		magic;
	int		nx;
	fastf_t		x[1];	/* array of nx+1 wavelengths, dynamically sized */
};
#define RT_TABLE_MAGIC	0x53706374
#define RT_CK_TABLE(_p)	RT_CKMAG(_p, RT_TABLE_MAGIC, "rt_table")

/* Gets an rt_table, with x[] having size _nx+1 */
#define RT_GET_TABLE(_table, _nx)  { \
	if( (_nx) < 1 )  rt_bomb("RT_GET_TABLE() _nx < 1\n"); \
	_table = (struct rt_table *)rt_calloc( 1, \
		sizeof(struct rt_table) + sizeof(fastf_t)*(_nx), \
		"struct rt_table" ); \
	_table->magic = RT_TABLE_MAGIC; \
	_table->nx = (_nx);  }


struct rt_tabdata {
	long		magic;
	int		ny;
	CONST struct rt_table *table;	/* Up pointer to definition of X axis */
	fastf_t		y[1];		/* array of ny samples, dynamically sized */
};
#define RT_TABDATA_MAGIC	0x53736d70
#define RT_CK_TABDATA(_p)	RT_CKMAG(_p, RT_TABDATA_MAGIC, "rt_tabdata")

#define RT_SIZEOF_TABDATA(_table)	( sizeof(struct rt_tabdata) + \
			sizeof(fastf_t)*((_table)->nx-1) )

/* Gets an rt_tabdata, with y[] having size _ny */
#define RT_GET_TABDATA(_data, _table)  { \
	RT_CK_TABLE(_table);\
	_data = (struct rt_tabdata *)rt_calloc( 1, \
		RT_SIZEOF_TABDATA(_table), "struct rt_tabdata" ); \
	_data->magic = RT_TABDATA_MAGIC; \
	_data->ny = (_table)->nx; \
	_data->table = (_table); }

/*
 * *** Table-generic routines
 */

RT_EXTERN( void			rt_ck_table, (CONST struct rt_table *tabp));
RT_EXTERN( struct rt_table	*rt_table_make_uniform, (int num, double first,
					double last));
RT_EXTERN( void			rt_tabdata_add, (struct rt_tabdata *out,
					CONST struct rt_tabdata *in1,
					CONST struct rt_tabdata *in2));
RT_EXTERN( void			rt_tabdata_mul, (struct rt_tabdata *out,
					CONST struct rt_tabdata *in1,
					CONST struct rt_tabdata *in2));
RT_EXTERN( void			rt_tabdata_scale, (struct rt_tabdata *out,
					CONST struct rt_tabdata *in1,
					double scale));
RT_EXTERN( double		rt_tabdata_area1, (CONST struct rt_tabdata *in));
RT_EXTERN( double		rt_tabdata_area2, (CONST struct rt_tabdata *in));
RT_EXTERN( fastf_t		rt_table_lin_interp, (CONST struct rt_tabdata *samp,
					double wl));
RT_EXTERN( struct rt_tabdata	*rt_tabdata_resample, (
					CONST struct rt_table *newtable,
					CONST struct rt_tabdata *olddata));
RT_EXTERN( int			rt_table_write, (CONST char *filename,
					CONST struct rt_table *tabp));
RT_EXTERN( struct rt_table	*rt_table_read, (CONST char *filename));
RT_EXTERN( int			rt_pr_table_and_tabdata, (CONST char *filename,
					CONST struct rt_tabdata *data));
RT_EXTERN( struct rt_tabdata	*rt_read_table_and_tabdata, (
					CONST char *filename));
RT_EXTERN( struct rt_tabdata	*rt_tabdata_binary_read, (CONST char *filename,
					int num,
					CONST struct rt_table *tabp));
RT_EXTERN( struct rt_tabdata	*rt_tabdata_malloc_array, (
					CONST struct rt_table *tabp,
					int num));
RT_EXTERN( void			rt_tabdata_copy, (struct rt_tabdata *out,
					CONST struct rt_tabdata *in));


/*
 * *** Spectrum-specific routines
 */
RT_EXTERN( void			rt_spect_make_CIE_XYZ, (
					struct rt_tabdata **x,
					struct rt_tabdata **y,
					struct rt_tabdata **z,
					CONST struct rt_table *tabp));

RT_EXTERN( void			rt_spect_black_body, (struct rt_tabdata *data,
					double temp, unsigned int n));
RT_EXTERN( void			rt_spect_black_body_fast, (
					struct rt_tabdata *data,
					double temp));
