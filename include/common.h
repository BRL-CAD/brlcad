/*
 *			C O M M O N . H
 *
 *  Header file for the BRL-CAD common definitions.
 *
 *  This header wraps the system-specific encapsulation of config.h
 *  and removes need to conditionally include config.h everywhere.
 *  The common definitions are symbols common to the platform being
 *  built that are either detected via configure or hand crafted, as
 *  is the case for the win32 platform.
 *
 *  Author -
 *	Christopher Sean Morrison
 *  
 *  Source -
 *	The U. S. Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5068  USA
 *  
 *  $Header$
 */

#ifndef __COMMON_H__
#define __COMMON_H__

#include "common.h"

#ifdef __win32
#  include "config_win.h"
#endif

#endif  /* __COMMON_H__ */
