/*
 *			T A B D A T A . H
 *
 *  A header file containing data structures to assist with
 *  recording many sets of data sampled along the same set of independent
 *  variables.
 *  The overall notion is that each sample should be
 *  as compact as possible (an array of measurements),
 *  with all the context stored in one place.
 *
 *  These structures and support routines apply to
 *  any measured "curve" or "function" or "table" with one independent
 *  variable and one or more scalar dependent variable(s).
 *
 *  The context is kept in an 'rt_table' structure, and
 *  the data for one particular sample are kept in an 'rt_tabdata'
 *  structure.
 *
 *  The contents of the sample in val[j] are interpreted
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
 *
 *  See Also -
 *	spectrum.h, spectrum.c
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

#ifndef TABDATA_H_SEEN
#define TABDATA_H_SEEN seen

#ifdef __cplusplus
extern "C" {
#endif

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
 * Routines
 */

BU_EXTERN( void			rt_table_free, (struct rt_table	*tabp));
BU_EXTERN( void			rt_tabdata_free, (struct rt_tabdata *data));
BU_EXTERN( void			rt_ck_table, (CONST struct rt_table *tabp));
BU_EXTERN( struct rt_table	*rt_table_make_uniform, (int num, double first,
					double last));
BU_EXTERN( void			rt_tabdata_add, (struct rt_tabdata *out,
					CONST struct rt_tabdata *in1,
					CONST struct rt_tabdata *in2));
BU_EXTERN( void			rt_tabdata_mul, (struct rt_tabdata *out,
					CONST struct rt_tabdata *in1,
					CONST struct rt_tabdata *in2));
BU_EXTERN( void			rt_tabdata_scale, (struct rt_tabdata *out,
					CONST struct rt_tabdata *in1,
					double scale));
BU_EXTERN( void			rt_table_scale, (struct rt_table *tabp,
					double scale));
BU_EXTERN( void			rt_tabdata_join1, (struct rt_tabdata *out,
					CONST struct rt_tabdata *in1,
					double scale,
					CONST struct rt_tabdata *in2));
BU_EXTERN( void			rt_tabdata_blend3, (struct rt_tabdata *out,
					double scale1,
					CONST struct rt_tabdata *in1,
					double scale2,
					CONST struct rt_tabdata *in2,
					double scale3,
					CONST struct rt_tabdata *in3));
BU_EXTERN( double		rt_tabdata_area1, (CONST struct rt_tabdata *in));
BU_EXTERN( double		rt_tabdata_area2, (CONST struct rt_tabdata *in));
BU_EXTERN( double		rt_tabdata_mul_area1, (CONST struct rt_tabdata *in1,
					CONST struct rt_tabdata	*in2));
BU_EXTERN( double		rt_tabdata_mul_area2, (CONST struct rt_tabdata *in1,
					CONST struct rt_tabdata	*in2));
BU_EXTERN( fastf_t		rt_table_lin_interp, (CONST struct rt_tabdata *samp,
					double wl));
BU_EXTERN( struct rt_tabdata	*rt_tabdata_resample_max, (
					CONST struct rt_table *newtable,
					CONST struct rt_tabdata *olddata));
BU_EXTERN( struct rt_tabdata	*rt_tabdata_resample_avg, (
					CONST struct rt_table *newtable,
					CONST struct rt_tabdata *olddata));
BU_EXTERN( int			rt_table_write, (CONST char *filename,
					CONST struct rt_table *tabp));
BU_EXTERN( struct rt_table	*rt_table_read, (CONST char *filename));
BU_EXTERN( int			rt_pr_table_and_tabdata, (CONST char *filename,
					CONST struct rt_tabdata *data));
BU_EXTERN( struct rt_tabdata	*rt_read_table_and_tabdata, (
					CONST char *filename));
BU_EXTERN( struct rt_tabdata	*rt_tabdata_binary_read, (CONST char *filename,
					int num,
					CONST struct rt_table *tabp));
BU_EXTERN( struct rt_tabdata	*rt_tabdata_malloc_array, (
					CONST struct rt_table *tabp,
					int num));
BU_EXTERN( void			rt_tabdata_copy, (struct rt_tabdata *out,
					CONST struct rt_tabdata *in));
BU_EXTERN(struct rt_tabdata	*rt_tabdata_dup, (CONST struct rt_tabdata *in));
BU_EXTERN(struct rt_tabdata	*rt_tabdata_get_constval, (double val,
					CONST struct rt_table	*tabp));
BU_EXTERN(void			rt_tabdata_constval, (struct rt_tabdata	*data, double val));
BU_EXTERN(struct rt_tabdata	*rt_tabdata_from_array, (CONST double *array));
BU_EXTERN(struct rt_table	*rt_table_merge2, (CONST struct rt_table *a,
				CONST struct rt_table *b));


#ifdef __cplusplus
}
#endif

#endif /* TABDATA_H_SEEN */
