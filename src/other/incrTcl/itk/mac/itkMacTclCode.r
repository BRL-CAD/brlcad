/*
 *  itkMacTclCode.r
 *
 *  This file includes the Itk code that is needed to startup Itk.
 *  It is to be included either in the resource fork of the shared library, or in the
 *  resource fork of the application for a statically bound application.
 *
 *  Jim Ingham
 *  Lucent Technologies 1996
 *
 */
 

#define ITK_LIBRARY_RESOURCES 3500

/* 
 * We now load the Itk library into the resource fork of the application.
 */

read 'TEXT' (ITK_LIBRARY_RESOURCES+1, "itk", purgeable) 
	"::library:itk.tcl";
read 'TEXT' (ITK_LIBRARY_RESOURCES+3, "Itk_Archetype", purgeable) 
	"::library:Archetype.itk";
read 'TEXT' (ITK_LIBRARY_RESOURCES+4, "Itk_Widget", purgeable) 
	"::library:Widget.itk";
read 'TEXT' (ITK_LIBRARY_RESOURCES+5, "Itk_Toplevel", purgeable) 
	"::library:Toplevel.itk";


