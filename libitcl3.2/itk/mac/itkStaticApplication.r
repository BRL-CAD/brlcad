/*
 * itkStaticApplication.r --
 *
 *	This file creates resources which bind in the static version of the
 *  pkgIndex tclIndex and itk's Tcl code files.
 *
 * Jim Ingham for Itcl 2.2
 * 
 * Copyright (c) 1996 Lucent Technologies
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * SCCS: @(#) itkStaticApplication.r 1.5 96/10/03 17:54:21
 */

#include <Types.r>
#include <SysTypes.r>

#define ITK_LIBRARY_RESOURCES 3500

#include "itkMacTclCode.r"

data 'TEXT' (ITK_LIBRARY_RESOURCES+6,"itk:pkgIndex",purgeable, preload) {
	"# Tcl package index file, version 1.0\n"
	"package ifneeded Itk 2.2 {load {} Itk}\n"
};
read 'TEXT' (ITK_LIBRARY_RESOURCES+2, "Itk:tclIndex", purgeable) 
	"::mac:tclIndex";
