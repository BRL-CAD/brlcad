#include <limits.h>
#include <cfloat>

#include <sdai.h>

//  The values used here to represent NULL values for numeric types come from
//  the ANSI C standard
//  LONG_MAX: ex MAXLONG
//  FLT_MIN: ex MINFLOAT

const char * SCLversion = "STEP Class Library version 3.1";

const SDAI_Integer  SDAI_INT_NULL  = LONG_MAX;
const SDAI_Real     SDAI_REAL_NULL  = FLT_MIN;
const SDAI_Real     SDAI_NUMBER_NULL  = FLT_MIN;
