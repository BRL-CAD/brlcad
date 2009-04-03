#include <limits.h>
#include <cfloat>

#include <sdai.h>

//  The values used here to represent NULL values for numeric types come from 
//  the ANSI C standard
//  LONG_MAX: ex MAXLONG
//  FLT_MIN: ex MINFLOAT

const char *SCLversion = "STEP Class Library version 3.1";

const SCLP23(Integer) SCLP23(INT_NULL) = LONG_MAX;
const SCLP23(Real)    SCLP23(REAL_NULL) = FLT_MIN;
const SCLP23(Real)    SCLP23(NUMBER_NULL) = FLT_MIN;
