#ifndef BASETYPE_H
#define	BASETYPE_H

/*
* NIST STEP Core Class Library
* clstepcore/baseType.h
* April 1997
* David Sauder
* KC Morris

* Development of this software was funded by the United States Government,
* and is not subject to copyright.
*/

/* $Id: baseType.h,v 3.0.1.2 1997/11/05 21:59:29 sauderd DP3.1 $ */

#ifdef __O3DB__
#include <OpenOODB.h>
#endif

//	**************  TYPES of attributes

// IMS, 9 Aug 95: changed values to make these values usable in a bitmask

enum PrimitiveType {
	sdaiINTEGER     = 0x0001,
	sdaiREAL        = 0x0002,
	sdaiBOOLEAN     = 0x0004,
	sdaiLOGICAL     = 0x0008,
	sdaiSTRING      = 0x0010,
	sdaiBINARY      = 0x0020,
	sdaiENUMERATION = 0x0040,
	sdaiSELECT      = 0x0080,
	sdaiINSTANCE    = 0x0100,
	sdaiAGGR        = 0x0200,
	sdaiNUMBER      = 0x0400,
// The elements defined below are not part of part 23
// (IMS: these should not be used as bitmask fields)
	ARRAY_TYPE,		// DAS
	BAG_TYPE,		// DAS
	SET_TYPE,		// DAS
	LIST_TYPE,		// DAS
	GENERIC_TYPE,
	REFERENCE_TYPE,
	UNKNOWN_TYPE
 };

// for backwards compatibility with our previous implementation
typedef PrimitiveType BASE_TYPE;

// the previous element types of the enum BASE_TYPE that have been redefined
#define INTEGER_TYPE sdaiINTEGER
#define REAL_TYPE sdaiREAL
#define BOOLEAN_TYPE sdaiBOOLEAN
#define LOGICAL_TYPE sdaiLOGICAL
#define STRING_TYPE sdaiSTRING
#define BINARY_TYPE sdaiBINARY
#define ENUM_TYPE sdaiENUMERATION
#define SELECT_TYPE sdaiSELECT
#define ENTITY_TYPE sdaiINSTANCE
#define AGGREGATE_TYPE sdaiAGGR
#define NUMBER_TYPE sdaiNUMBER

/* not defined in part 23
	ARRAY_TYPE,		// DAS
	BAG_TYPE,		// DAS
	SET_TYPE,		// DAS
	LIST_TYPE,		// DAS
	GENERIC_TYPE,
	REFERENCE_TYPE,
	UNKNOWN_TYPE
*/

#endif 
