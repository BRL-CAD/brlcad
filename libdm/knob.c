/*
 *			K N O B . C
 *
 * Utilities for dealing with knobs.
 * 
 * Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *      The U. S. Army Ballistic Research Laboratory
 *      Aberdeen Proving Ground, Maryland  21005-5066
 *
 * Copyright Notice -
 *      This software is Copyright (C) 1988 by the United States Army.
 *      All rights reserved.
 *
 * Distribution Status -
 *      Public Domain, Distribution Unlimitied.
 */
#if IR_KNOBS
#include "conf.h"
#include "tcl.h"
#include "machine.h"
#include "bu.h"
#include "dm.h"

int dm_limit();  /* provides knob dead spot */
int dm_unlimit();  /* */
fastf_t dm_wrap(); /* wrap given value to new value in the range (-1.0, 1.0) */

/*
 *                      D M _ L I M I T
 *
 * Because the dials are so sensitive, setting them exactly to
 * zero is very difficult.  This function can be used to extend the
 * location of "zero" into "the dead zone".
 */

int
dm_limit(i)
int i;
{
  if( i > NOISE )
    return( i-NOISE );
  if( i < -NOISE )
    return( i+NOISE );
  return(0);
}

/*
 *			D M _ U N L I M I T
 *
 * This function reverses the effects of dm_limit.
 */
int
dm_unlimit(i)
int i;
{
  if( i > 0 )
    return( i + NOISE );
  if( i < 0 )
    return( i - NOISE );
  return(0);
}

/*			D M _ W R A P
 *
 * Wrap the given value "f" to a new value in the range (-1.0, 1.0).
 * The value of f is assumed to be in the range (-2.0, 2.0).
 */
fastf_t
dm_wrap(f)
fastf_t f;
{
  if(f > 1.0)
    return(f - 2.0);
  if(f < -1.0)
    return(f + 2.0);
  return f;
}
#endif
