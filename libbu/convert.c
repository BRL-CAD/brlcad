/*
 *			V E R T . C
 * Author -
 *	Christopher T. Johnson
 *
 * Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066
 *  
 *  Copyright Notice -
 *	This software is Copyright (C) 1986 by the United States Army.
 *	All rights reserved.
 *
 *
 * Vert.c is a routine to translate data formats.  The data formats are:
 *
 *	Host/Network		is the data in host format or local format
 *	signed/unsigned		Is the data signed?
 *	char/short/int/long/double
 *				Is the data 8bits, 16bits, 32bits, 64bits
 *				or a double?
 *
 *
 * The method of conversion is to convert up to double then back down the
 * the expected output format.
 *
 */
#ifndef line
static char RCSid[] = "$Header$ (BRL)";
#endif
/* vertinit	initialize the converter routine.
 *
 * Set up the conversion tables/flags for vert.
 *
 * Entry:
 *	in	the input format description.
 *	out	the output format description.
 *
 * Exit:
 *
 * Format description:
 *	[channels][h|n][s|u] c|s|i|l|d|8|16|32|64
 *
 * channels must be null or 1
 * Host | Network
 * signed | unsigned
 * char | short | integer | long | double | number of bits of integer
 */
vertinit(in,out)
char *in;			/* input format */
char *out;			/* output format */
{
}
/* convert - convert from one format to another.
 *
 * Entry:
 *	in	input pointer
 *	out	output pointer
 *	count	number of entries to convert.
 *	size	size of output buffer.
 *
 */
convert(out, size, in, count)
genptr_t out;
int	size;
getptr_t in;
int	count;
{
}
