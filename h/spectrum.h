/*
 *			S P E C T R U M . H
 *
 *  A header file containing data structures to assist with
 *  recording spectral data.
 *  The overall notion is that each spectral sample should be
 *  as compact as possible (typically just an array of power levels),
 *  with all the context stored in one place.
 *
 *  The context is kept in an 'rt_spectrum' structure, and
 *  the data for one particular sample are kept in an 'rt_spectral_sample'
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
 *  Because there are likely to be very few rt_spectrum structures
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

struct rt_spectrum {
	long	magic;
	int	nwave;
	fastf_t	wavel[1];	/* array of nwave+1 wavelengths, dynamically sized */
};
#define RT_SPECTRUM_MAGIC	0x53706374
#define RT_CK_SPECTRUM(_p)	RT_CKMAG(_p, RT_SPECTRUM_MAGIC, "rt_spectrum")

/* Gets an rt_spectrum, with wavel[] having size _nwave+1 */
#define RT_GET_SPECTRUM(_spect, _nwave)  { \
	if( (_nwave) < 1 )  rt_bomb("RT_GET_SPECTRUM() _nwave < 1\n"); \
	_spect = (struct rt_spectrum *)rt_calloc( 1, \
		sizeof(struct rt_spectrum) + sizeof(fastf_t)*(_nwave), \
		"struct rt_spectrum" ); \
	_spect->magic = RT_SPECTRUM_MAGIC; \
	_spect->nwave = (_nwave);  }


struct rt_spect_sample {
	long	magic;
	int	nwave;
	/* Next item is dubious, it might force keeping use counts in rt_spectrum */
	CONST struct rt_spectrum *spectrum;	/* Up pointer to "struct spectrum" */
	fastf_t	val[1];		/* array of nwave samples, dynamically sized */
};
#define RT_SPECT_SAMPLE_MAGIC	0x53736d70
#define RT_CK_SPECT_SAMPLE(_p)	RT_CKMAG(_p, RT_SPECT_SAMPLE_MAGIC, "rt_spect_sample")

/* Gets an rt_spect_sample, with val[] having size _nwave */
#define RT_GET_SPECT_SAMPLE(_ssamp, _spect)  { \
	RT_CK_SPECTRUM(_spect);\
	_ssamp = (struct rt_spect_sample *)rt_calloc( 1, \
		sizeof(struct rt_spect_sample) + \
			sizeof(fastf_t)*((_spect)->nwave-1), \
		"struct rt_spect_sample" ); \
	_ssamp->magic = RT_SPECT_SAMPLE_MAGIC; \
	_ssamp->nwave = (_spect)->nwave; \
	_ssamp->spectrum = (_spect); }
