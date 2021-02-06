/*                        T A B D A T A . H
 * BRL-CAD
 *
 * Copyright (c) 2004-2021 United States Government as represented by
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

/*----------------------------------------------------------------------*/
/** @addtogroup bn_tabdata
 *
 * @brief
 * Routines for processing tables (curves) of data with one independent
 * parameter which is common to many sets of dependent data values.
 *
 * Data structures to assist with recording many sets of data sampled
 * along the same set of independent variables.
 *
 * The overall notion is that each sample should be as compact as
 * possible (an array of measurements), with all the context stored in
 * one place.
 *
 * These structures and support routines apply to any measured "curve"
 * or "function" or "table" with one independent variable and one or
 * more scalar dependent variable(s).
 *
 * The context is kept in an 'bn_table' structure, and the data for
 * one particular sample are kept in an 'bn_tabdata' structure.
 *
 * The contents of the sample in val[j] are interpreted in the
 * interval (wavel[j]..wavel[j+1]).  This value could be power,
 * albedo, absorption, refractive index, or any other
 * wavelength-specific parameter.
 *
 * For example, if the val[] array contains power values, then val[j]
 * contains the integral of the power from wavel[j] to wavel[j+1]
 *
 * As an example, assume nwave=2, wavel[0]=500, wavel[1]=600, wavel[2]=700.
 * Then val[0] would contain data for the 500 to 600nm interval,
 * and val[1] would contain data for the 600 to 700nm interval.
 * There would be no storage allocated for val[2] -- don't use it!
 * There are several interpretations of this:
 *	1)  val[j] stores the total (integral, area) value for the interval, or
 *	2)  val[j] stores the average value across the interval.
 *
 * The intervals need not be uniformly spaced; it is acceptable to
 * increase wavelength sampling density around "important"
 * frequencies.
 *
 * Operates on bn_table (independent var) and
 * bn_tabdata (dependent variable) structures.
 *
 * One application is for storing spectral curves, see spectrum.c
 *
 * @par Inspired by -
 *     Roy Hall and his book "Illumination and Color in Computer
 *@n   Generated Imagery", Springer Verlag, New York, 1989.
 *@n   ISBN 0-387-96774-5
 *
 * With thanks to Russ Moulton Jr, EOSoft Inc. for his "rad.c" module.
 */
/** @{ */
/* @file bn/tabdata.h */

#ifndef BN_TABDATA_H
#define BN_TABDATA_H

#include "common.h"

#include "vmath.h"

#include "bn/defines.h"
#include "bu/magic.h"
#include "bu/vls.h"

__BEGIN_DECLS

struct bn_table {
    uint32_t magic;
    size_t nx;
    fastf_t x[1];	/**< @brief array of nx+1 wavelengths, dynamically sized */
};

#define BN_CK_TABLE(_p)	BU_CKMAG(_p, BN_TABLE_MAGIC, "bn_table")
#define BN_TABLE_NULL	((struct bn_table *)NULL)

/* Gets an bn_table, with x[] having size _nx+1 */
#ifndef NO_BOMBING_MACROS
#  define BN_GET_TABLE(_table, _nx) { \
	if ((_nx) < 1)  bu_bomb("RT_GET_TABLE() _nx < 1\n"); \
	_table = (struct bn_table *)bu_calloc(1, \
					      sizeof(struct bn_table) + sizeof(fastf_t)*(_nx), \
					      "struct bn_table"); \
	_table->magic = BN_TABLE_MAGIC; \
	_table->nx = (_nx);  }
#else
#  define BN_GET_TABLE(_table, _nx) { \
	_table = (struct bn_table *)bu_calloc(1, \
					      sizeof(struct bn_table) + sizeof(fastf_t)*(_nx), \
					      "struct bn_table"); \
	_table->magic = BN_TABLE_MAGIC; \
	_table->nx = (_nx);  }
#endif

struct bn_tabdata {
    uint32_t magic;
    size_t ny;
    const struct bn_table *table;	/**< @brief Up pointer to definition of X axis */
    fastf_t y[1];			/**< @brief array of ny samples, dynamically sized */
};
#define BN_CK_TABDATA(_p)	BU_CKMAG(_p, BN_TABDATA_MAGIC, "bn_tabdata")
#define BN_TABDATA_NULL		((struct bn_tabdata *)NULL)

#define BN_SIZEOF_TABDATA_Y(_tabdata)	sizeof(fastf_t)*((_tabdata)->ny)
#define BN_SIZEOF_TABDATA(_table)	(sizeof(struct bn_tabdata) + \
					 sizeof(fastf_t)*((_table)->nx-1))

/* Gets an bn_tabdata, with y[] having size _ny */
#define BN_GET_TABDATA(_data, _table) { \
	BN_CK_TABLE(_table);\
	_data = (struct bn_tabdata *)bu_calloc(1, \
					       BN_SIZEOF_TABDATA(_table), "struct bn_tabdata"); \
	_data->magic = BN_TABDATA_MAGIC; \
	_data->ny = (_table)->nx; \
	_data->table = (_table); }

/*
 * Routines
 */


BN_EXPORT extern void bn_table_free(struct bn_table *tabp);

BN_EXPORT extern void bn_tabdata_free(struct bn_tabdata *data);

BN_EXPORT extern void bn_ck_table(const struct bn_table *tabp);

/*
 *@brief
 *  Set up an independent "table margin" from 'first' to 'last',
 *  inclusive, using 'num' uniformly spaced samples.  Num >= 1.
 */
BN_EXPORT extern struct bn_table *bn_table_make_uniform(size_t num,
							double first,
							double last);

/*
 *@brief
 *  Sum the values from two data tables.
 */
BN_EXPORT extern void bn_tabdata_add(struct bn_tabdata *out,
				     const struct bn_tabdata *in1,
				     const struct bn_tabdata *in2);

/*
 *@brief
 *  Element-by-element multiply the values from two data tables.
 */
BN_EXPORT extern void bn_tabdata_mul(struct bn_tabdata *out,
				     const struct bn_tabdata *in1,
				     const struct bn_tabdata *in2);

/*
 *@brief
 *  Element-by-element multiply the values from three data tables.
 */
BN_EXPORT extern void bn_tabdata_mul3(struct bn_tabdata *out,
				      const struct bn_tabdata *in1,
				      const struct bn_tabdata *in2,
				      const struct bn_tabdata *in3);

/*
 *@brief
 *  Element-by-element multiply the values from three data tables and a scalar.
 *
 *	out += in1 * in2 * in3 * scale
 */
BN_EXPORT extern void bn_tabdata_incr_mul3_scale(struct bn_tabdata *out,
						 const struct bn_tabdata *in1,
						 const struct bn_tabdata *in2,
						 const struct bn_tabdata *in3,
						 double scale);

/*
 *@brief
 *  Element-by-element multiply the values from two data tables and a scalar.
 *
 *	out += in1 * in2 * scale
 */
BN_EXPORT extern void bn_tabdata_incr_mul2_scale(struct bn_tabdata *out,
						 const struct bn_tabdata *in1,
						 const struct bn_tabdata *in2,
						 double scale);

/*
 *@brief
 *  Multiply every element in a data table by a scalar value 'scale'.
 */
BN_EXPORT extern void bn_tabdata_scale(struct bn_tabdata *out,
				       const struct bn_tabdata *in1,
				       double scale);

/*
 *@brief
 *  Scale the independent axis of a table by 'scale'.
 */
BN_EXPORT extern void bn_table_scale(struct bn_table *tabp,
				     double scale);

/*
 *@brief
 *  Multiply every element in data table in2 by a scalar value 'scale',
 *  add it to the element in in1, and store in 'out'.
 *  'out' may overlap in1 or in2.
 */
BN_EXPORT extern void bn_tabdata_join1(struct bn_tabdata *out,
				       const struct bn_tabdata *in1,
				       double scale,
				       const struct bn_tabdata *in2);

/*
 *@brief
 *  Multiply every element in data table in2 by a scalar value 'scale2',
 *  plus in3 * scale3, and
 *  add it to the element in in1, and store in 'out'.
 *  'out' may overlap in1 or in2.
 */
BN_EXPORT extern void bn_tabdata_join2(struct bn_tabdata *out,
				       const struct bn_tabdata *in1,
				       double scale2,
				       const struct bn_tabdata *in2,
				       double scale3,
				       const struct bn_tabdata *in3);

BN_EXPORT extern void bn_tabdata_blend2(struct bn_tabdata *out,
					double scale1,
					const struct bn_tabdata *in1,
					double scale2,
					const struct bn_tabdata *in2);

BN_EXPORT extern void bn_tabdata_blend3(struct bn_tabdata *out,
					double scale1,
					const struct bn_tabdata *in1,
					double scale2,
					const struct bn_tabdata *in2,
					double scale3,
					const struct bn_tabdata *in3);

/*
 *@brief
 *  Following interpretation #1, where y[j] stores the total (integral
 *  or area) value within the interval, return the area under the whole curve.
 *  This is simply totaling up the areas from each of the intervals.
 */
BN_EXPORT extern double bn_tabdata_area1(const struct bn_tabdata *in);

/*
 *@brief
 *  Following interpretation #2, where y[j] stores the average
 *  value for the interval, return the area under
 *  the whole curve.  Since the interval spacing need not be uniform,
 *  sum the areas of the rectangles.
 */
BN_EXPORT extern double bn_tabdata_area2(const struct bn_tabdata *in);

/*
 *@brief
 *  Following interpretation #1, where y[j] stores the total (integral
 *  or area) value within the interval, return the area under the whole curve.
 *  This is simply totaling up the areas from each of the intervals.
 *  The curve value is found by multiplying corresponding entries from
 *  in1 and in2.
 */
BN_EXPORT extern double bn_tabdata_mul_area1(const struct bn_tabdata *in1,
					     const struct bn_tabdata *in2);

/*
 *@brief
 *  Following interpretation #2,
 *  return the area under the whole curve.
 *  The curve value is found by multiplying corresponding entries from
 *  in1 and in2.
 */
BN_EXPORT extern double bn_tabdata_mul_area2(const struct bn_tabdata *in1,
					     const struct bn_tabdata *in2);

/*
 *@brief
 *  Return the value of the curve at independent parameter value 'wl'.
 *  Linearly interpolate between values in the input table.
 *  Zero is returned for values outside the sampled range.
 */
BN_EXPORT extern fastf_t bn_table_lin_interp(const struct bn_tabdata *samp,
					     double wl);

/*
 *@brief
 *  Given a set of sampled data 'olddata', resample it for different
 *  spacing, by linearly interpolating the values when an output span
 *  is entirely contained within an input span, and by taking the
 *  maximum when an output span covers more than one input span.
 *
 *  This assumes interpretation (2) of the data, i.e. that the values
 *  are the average value across the interval.
 */
BN_EXPORT extern struct bn_tabdata *bn_tabdata_resample_max(const struct bn_table *newtable,
							    const struct bn_tabdata *olddata);

/*
 *@brief
 *  Given a set of sampled data 'olddata', resample it for different
 *  spacing, by linearly interpolating the values when an output span
 *  is entirely contained within an input span, and by taking the
 *  average when an output span covers more than one input span.
 *
 *  This assumes interpretation (2) of the data, i.e. that the values
 *  are the average value across the interval.
 */
BN_EXPORT extern struct bn_tabdata *bn_tabdata_resample_avg(const struct bn_table *newtable,
							    const struct bn_tabdata *olddata);

/*
 *@brief
 *  Write out the table structure in an ASCII file,
 *  giving the number of values (minus 1), and the
 *  actual values.
 */
BN_EXPORT extern int bn_table_write(const char *filename,
				    const struct bn_table *tabp);

/*
 *@brief
 *  Allocate and read in the independent variable values from an ASCII file,
 *  giving the number of samples (minus 1), and the
 *  actual values.
 */
BN_EXPORT extern struct bn_table *bn_table_read(const char *filename);

BN_EXPORT extern void bn_pr_table(const char *title,
				  const struct bn_table *tabp);

BN_EXPORT extern void bn_pr_tabdata(const char *title,
				    const struct bn_tabdata *data);

/*
 *@brief
 *  Write out a given data table into an ASCII file,
 *  suitable for input to GNUPLOT.
 *
 *	(set term postscript)
 *	(set output "|print-postscript")
 *	(plot "filename" with lines)
 */
BN_EXPORT extern int bn_print_table_and_tabdata(const char *filename,
						const struct bn_tabdata *data);

/*
 *@brief
 *  Read in a file which contains two columns of numbers, the first
 *  column being the wavelength, the second column being the sample value
 *  at that wavelength.
 *
 *  A new bn_table structure and one bn_tabdata structure
 *  are created, a pointer to the bn_tabdata structure is returned.
 *  The final wavelength is guessed at.
 */
BN_EXPORT extern struct bn_tabdata *bn_read_table_and_tabdata(const char *filename);

BN_EXPORT extern struct bn_tabdata *bn_tabdata_binary_read(const char *filename,
							   size_t num,
							   const struct bn_table *tabp);

/*
 *@brief
 *  Allocate storage for, and initialize, an array of 'num' data table
 *  structures.
 *  This subroutine is provided because the bn_tabdata structures
 *  are variable length.
 */
BN_EXPORT extern struct bn_tabdata *bn_tabdata_malloc_array(const struct bn_table *tabp,
							    size_t num);

BN_EXPORT extern void bn_tabdata_copy(struct bn_tabdata *out,
				      const struct bn_tabdata *in);

BN_EXPORT extern struct bn_tabdata *bn_tabdata_dup(const struct bn_tabdata *in);

/*
 *@brief
 *  For a given table, allocate and return a tabdata structure
 *  with all elements initialized to 'val'.
 */
BN_EXPORT extern struct bn_tabdata *bn_tabdata_get_constval(double val,
							    const struct bn_table *tabp);

/*
 *@brief
 *  Set all the tabdata elements to 'val'
 */
BN_EXPORT extern void bn_tabdata_constval(struct bn_tabdata *data,
					  double val);

/*
 *@brief
 *  Convert an bn_tabdata/bn_table pair into a Tcl compatible string
 *  appended to a VLS.  It will have form:
 *	x {...} y {...} nx # ymin # ymax #
 */
BN_EXPORT extern void bn_tabdata_to_tcl(struct bu_vls *vp,
					const struct bn_tabdata *data);

/*
 *@brief
 *  Given an array of (x, y) pairs, build the relevant bn_table and
 *  bn_tabdata structures.
 *
 *  The table is terminated by an x value <= 0.
 *  Consistent with the interpretation of the spans,
 *  invent a final span ending x value.
 */
BN_EXPORT extern struct bn_tabdata *bn_tabdata_from_array(const double *array);

/*
 *@brief
 *  Shift the data by a constant offset in the independent variable
 *  (often frequency), interpolating new sample values.
 */
BN_EXPORT extern void bn_tabdata_freq_shift(struct bn_tabdata *out,
					    const struct bn_tabdata *in,
					    double offset);

/*
 *@brief
 *  Returns number of sample points between 'low' and 'hi', inclusive.
 */
BN_EXPORT extern size_t bn_table_interval_num_samples(const struct bn_table *tabp,
						      double low,
						      double hi);

/*
 *@brief
 *  Remove all sampling points between subscripts i and j, inclusive.
 *  Don't bother freeing the tiny bit of storage at the end of the array.
 *  Returns number of points removed.
 */
BN_EXPORT extern size_t bn_table_delete_sample_pnts(struct bn_table *tabp,
						   size_t i,
						   size_t j);

/*
 *@brief
 *  A new table is returned which has sample points at places from
 *  each of the input tables.
 */
BN_EXPORT extern struct bn_table *bn_table_merge2(const struct bn_table *a,
						  const struct bn_table *b);

/*
 *@brief
 *  Create a filter to accept power in a given band.
 *  The first and last filter values will be in the range 0..1,
 *  while all the internal filter values will be 1.0,
 *  and all samples outside the given band will be 0.0.
 *
 *
 *  @return	NULL	if given band does not overlap input spectrum
 *  @return	tabdata*
 */
BN_EXPORT extern struct bn_tabdata *bn_tabdata_mk_linear_filter(const struct bn_table *spectrum,
								double lower_wavelen,
								double upper_wavelen);

__END_DECLS

#endif  /* BN_TABDATA_H */
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
