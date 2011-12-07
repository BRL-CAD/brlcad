/*                           I C V . H
 * BRL-CAD
 *
 * Copyright (c) 2011 United States Government as represented by
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
/** @file icv.h
 *
 * Functions provided by the LIBICV image conversion library.
 *
 */

#ifndef __ICV_H__
#define __ICV_H__

__BEGIN_DECLS

#ifndef ICV_EXPORT
#  if defined(ICV_DLL_EXPORTS) && defined(ICV_DLL_IMPORTS)
#    error "Only ICV_DLL_EXPORTS or ICV_DLL_IMPORTS can be defined, not both."
#  elif defined(ICV_DLL_EXPORTS)
#    define ICV_EXPORT __declspec(dllexport)
#  elif defined(ICV_DLL_IMPORTS)
#    define ICV_EXPORT __declspec(dllimport)
#  else
#    define ICV_EXPORT
#  endif
#endif

/**
 * I C V _ R O T
 *
 * Rotate an image.
 * %s [-rifb | -a angle] [-# bytes] [-s squaresize] [-w width] [-n height] [-o outputfile] inputfile [> outputfile]
 *
 */
ICV_EXPORT extern int icv_rot(int argv, char **argc);


__END_DECLS

#endif /* __ICV_H__ */

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
