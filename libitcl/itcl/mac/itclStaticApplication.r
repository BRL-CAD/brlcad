/*
 * itkStaticPkgIndex.r --
 *
 *	This file creates resources which bind in the static version of the
 *  pkgIndex files.
 *
 * Copyright (c) 1996 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * SCCS: @(#) tkMacLibrary.r 1.5 96/10/03 17:54:21
 */

#include <Types.r>
#include <SysTypes.r>
#include <AEUserTermTypes.r>

#define ITCL_LIBRARY_RESOURCES 2500

#include "itclMacTclCode.r"

data 'TEXT' (ITCL_LIBRARY_RESOURCES+20,"itcl:pkgIndex",purgeable, preload) {
	"# Tcl package index file, version 1.0\n"
	"package ifneeded Itcl 2.2 {load {} Itcl}\n"
};
