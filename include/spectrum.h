/*                      S P E C T R U M . H
 * BRL-CAD
 *
 * Copyright (c) 2004-2014 United States Government as represented by
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

/** @addtogroup libmultispectral */
/** @{ */
/** @file spectrum.h
 *
 *  A header file containing data structures to assist with
 *  recording spectral data.
 *  The overall notion is that each spectral sample should be
 *  as compact as possible (typically just an array of power levels),
 *  with all the context stored in one place.
 *
 *  The context is kept in an 'bn_table' structure, and
 *  the data for one particular sample are kept in an 'bn_tabdata'
 *  structure.
 *  tabdata.h provides the data structures,
 *  librt/tabdata.c provides the routines.
 *
 *  The contents of the spectral sample in val[j] are interpreted
 *  in the interval (wavel[j]..wavel[j+1]).
 *  This value could be power, albedo, absorption, refractive index,
 *  or any other wavelength-specific parameter.
 *
 *  For example, if the val[] array contains power values, then
 *  val[j] contains the integral of the power from wavel[j] to wavel[j+1]
 *
 *  As an example, assume nwave=2, wavel[0]=500, wavel[1]=600, wavel[2]=700.
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
 *	mm = 10** -3 m	millimeters
 *	um = 10** -6 m	micrometers, or microns
 *	nm = 10** -9 m	nanometers
 *
 *  The Spectrum in nm -
 *	< 10 nm			X-Rays
 *	10 to 390		Ultraviolet
 *	390 to 770		Visible
 *	770 to 1500		Near IR
 *	1500 to 6, 000		Middle IR	(includes 3-5 um band)
 *	6, 000 to 40, 000		Far IR		(includes 8-12 um band)
 *	40, 000 to 1, 000, 000	Extreme IR
 *	> 10**6 nm		Microwaves	(300 GHz to 300 MHz)
 *	> 10**9 nm		Radio waves	(300 MHz and down)
 *
 *  It is tempting to store the wavelength in nanometers as an integer,
 *  but that might preclude very narrow-band calculations, such as might
 *  be required to study a single spectral line,
 *  and it might also preclude future extensions to X-rays.
 *  Because there are likely to be very few bn_table structures
 *  in use, the extra storage isn't a likely problem.
 *  The worst effect of this decision will be floating-point grunge
 *  when printing wavelengths, e.g. 650nm might print as 649.99nm.
 *  On the other hand, non-integer values might make it difficult to
 *  determine if two wavelengths from different curves were the "same",
 *  without introducing a wavelength "tolerance" notion.  Ugh.
 *
 */

#ifndef SPECTRUM_H
#define SPECTRUM_H

#include "common.h"

#include "bn.h"
#include "raytrace.h"

__BEGIN_DECLS

/*
 * Routines
 */
RT_EXPORT extern void			rt_spect_make_CIE_XYZ(struct bn_tabdata		**x,
							      struct bn_tabdata		**y,
							      struct bn_tabdata		**z,
							      const struct bn_table	*tabp);

RT_EXPORT extern void			rt_spect_black_body(struct bn_tabdata		*data,
							    double			temp,
							    unsigned int		n);
RT_EXPORT extern void			rt_spect_black_body_fast(struct bn_tabdata		*data,
								 double			temp);

RT_EXPORT extern void			rt_spect_reflectance_rgb(struct bn_tabdata		*curve,
								 const float		rgb[3]);
__END_DECLS

#endif /* SPECTRUM_H */

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
