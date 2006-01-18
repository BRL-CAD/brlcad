/* BRL-CAD                 F E N C E . C
 *
 *
 * Copyright (c) 2004-2006 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this file; see the file named COPYING for more
 * information.
 *
 */
/** @file fence.c
/*
 *      This program generages a chain-link fence.  Every parameter of
 *      the fence may be adjusted.  Default values are held in fence.h
 *      Be wary of long fences...
 *
 *      Note: It would be much more optimal for there to be a fence
 *      object type (or a mesh object type) so that memory usage is
 *      minimized.  I.e. instead of creating a thousand little
 *      translations, rotations, and copies of basic primitives for a
 *      short 5 foot fence, we would just store the height, width,
 *      and mesh parameters (angle of twists in xyz, vertical separation,
 *      horizontal separation, and depth).
 *
 *  Author -
 *      Christopher Sean Morrison
 *
 *  Source -
 *      The U. S. Army Research Laboratory
 *      Aberdeen Proving Ground, Maryland  21005-5068  USA
 *
 ***********************************************************************/

#include "./fence.h"


/* command-line options are described in the parseArguments function
 */
char *options="IiDdVvO:o:N:n:U:u:H:h:L:l:R:r:J:j:A:a:T:t:B:b:C:c:F:f:P:p:M:m:W:w:S:s:E:e:G:g:XxZz";
extern char *optarg;
extern int optind, opterr, getopt(int, char *const *, const char *);

/* these variables control the "behavior" of this program's output
 *   if debug is set, debug information (extra "useful" output is given) is
 *     displayed on stdout.
 *   if verbose is set, then all prompting and output that may be shown
 *     (such as results and errors) will be sent to stderr.  this should
 *     probably be switched with debug (maybe) but convention was taken
 *     from another db program.
 *   if interactive is set, then the user will be prompted for values from
 *     stdin.
 *
 *   the default output file pointers for debug and verbose can be set
 *     in the header file
 */
int debug=DEFAULT_DEBUG;
int verbose=DEFAULT_VERBOSE;
int interactive=DEFAULT_INTERACTIVE;

/* start of variable definitions for the fence
 *
 * see the fence.h header file for a description of each of the default values
 * that may be set.  all default values should be set in the fence.h header
 * file, not here.
 */
char outputFilename[DEFAULT_MAXNAMELENGTH]=DEFAULT_OUTPUTFILENAME;
char id[DEFAULT_MAXNAMELENGTH]=DEFAULT_ID;
char units[DEFAULT_MAXNAMELENGTH]=DEFAULT_UNITS;

char fenceName[DEFAULT_MAXNAMELENGTH]=DEFAULT_FENCENAME;
point_t fenceStartPosition=DEFAULT_FENCESTARTPOSITION;
point_t fenceEndPosition=DEFAULT_FENCEENDPOSITION;
vect_t fenceWidth=DEFAULT_FENCEWIDTH;
vect_t fenceHeight=DEFAULT_FENCEHEIGHT;
double fencePoleSpacing=DEFAULT_FENCEPOLESPACING;
char fenceMaterial[DEFAULT_MAXNAMELENGTH*3]=DEFAULT_FENCEMATERIAL;
char fenceMaterialParams[DEFAULT_MAXNAMELENGTH*3]=DEFAULT_FENCEMATERIALPARAMS;
unsigned char fenceMaterialColor[3]=DEFAULT_FENCEMATERIALCOLOR;
int fenceFence=DEFAULT_FENCEFENCE;
int fencePoles=DEFAULT_FENCEPOLES;
int fenceMesh=DEFAULT_FENCEMESH;
int fenceWire=DEFAULT_FENCEWIRE;
int generateFenceParam=DEFAULT_GENERATEFENCE;
int generatePolesParam=DEFAULT_GENERATEPOLES;
int generateMeshParam=DEFAULT_GENERATEMESH;

char poleName[DEFAULT_MAXNAMELENGTH]=DEFAULT_POLENAME;
double poleHeight=DEFAULT_POLEHEIGHT;
double poleRadius=DEFAULT_POLERADIUS;
char poleMaterial[DEFAULT_MAXNAMELENGTH*3]=DEFAULT_POLEMATERIAL;
char poleMaterialParams[DEFAULT_MAXNAMELENGTH*3]=DEFAULT_POLEMATERIALPARAMS;
unsigned char poleMaterialColor[3]=DEFAULT_POLEMATERIALCOLOR;

char meshName[DEFAULT_MAXNAMELENGTH]=DEFAULT_MESHNAME;
double meshHeight=DEFAULT_MESHHEIGHT;
double meshWidth=DEFAULT_MESHWIDTH;
char meshMaterial[DEFAULT_MAXNAMELENGTH*3]=DEFAULT_MESHMATERIAL;
char meshMaterialParams[DEFAULT_MAXNAMELENGTH*3]=DEFAULT_MESHMATERIALPARAMS;
unsigned char meshMaterialColor[3]=DEFAULT_MESHMATERIALCOLOR;

char wireName[DEFAULT_MAXNAMELENGTH]=DEFAULT_WIRENAME;
char segmentName[DEFAULT_MAXNAMELENGTH]=DEFAULT_SEGMENTNAME;
double wireRadius=DEFAULT_WIRERADIUS;
double wireAngle=DEFAULT_WIREANGLE;
double wireSegmentLength=DEFAULT_WIRESEGMENTLENGTH;
double wireSegmentSeparation=DEFAULT_WIRESEGMENTSEPARATION;
char wireMaterial[DEFAULT_MAXNAMELENGTH*3]=DEFAULT_WIREMATERIAL;
char wireMaterialParams[DEFAULT_MAXNAMELENGTH*3]=DEFAULT_WIREMATERIALPARAMS;
unsigned char wireMaterialColor[3]=DEFAULT_WIREMATERIALCOLOR;

size_t maxWireSegments=DEFAULT_MAXWIRESEGMENTS;
/* end of variable definitions used for the fence generation
 */


/*
 * argumentHelp() is the all encompassing help message that is displayed when
 * an invalid command line argument is entered or if the user explicitly
 * requests assistance.
 ***************************************/
void argumentHelp(FILE *fp, char *progname, char *message)
{

  fflush(stdout);
  if (message) {
    fprintf(fp, "%s\n", message);
  }
  fflush(stdout);

  fprintf(fp, "Usage Format: \n%s %s\n\n", progname, \
	   "-[ivdonuhHlLrRjatTbBcCfpmwseEgGxXzZ]" \
	  );
  fprintf(fp, "\t-[ivd]\n\t\tspecifies interactive, verbose, and/or debug modes\n");
  fprintf(fp, "\t-[IVD]\n\t\ttoggles interactive, verbose, and/or debug modes\n");
  fprintf(fp, "\t-o filename\n\t\tspecifies the name of the file to output to\n");
  fprintf(fp, "\t-n 'string'\n\t\tthe 'string' name of the csg database\n");
  fprintf(fp, "\t-u 'units'\n\t\tthe units of the data in the csg database\n");
  fprintf(fp, "\t-[hH] ['xval yval zval' | val]\n\t\tspecifies the height as either vector or single value (z dir is up)\n");
  fprintf(fp, "\t-[lL] ['xval yval zval' | val]\n\t\tspecifies the length as either vector or single value (x dir is long)\n");
  fprintf(fp, "\t-r radius\n\t\tthe radius of the fence's mesh wires\n");
  fprintf(fp, "\t-R radius\n\t\tthe radius of the fence poles\n");
  fprintf(fp, "\t-a angle\n\t\tthe primary angle of the wire 'zig-zagging'\n");
  fprintf(fp, "\t-j distance\n\t\tthe maximum spacing between the poles\n");
  fprintf(fp, "\t-[tT] 'material'\n\t\tthe material of the fence (t) or \n\t\tthe material of all generated regions (T)\n");
  fprintf(fp, "\t-[bB] 'parameters'\n\t\tthe parameter string for the fence material(b)\n\t\tor of all region materials (B)\n");
  fprintf(fp, "\t-[cC] 'rval gval bval'\b\t\tthe RGB color of the fence (c)\n\t\tor of all region materials (C)\n\t\t(0 <= values <= 255)\n");
  fprintf(fp, "\t-f fencename\n\t\tthe base name of the fence objects in the database\n");
  fprintf(fp, "\t-p polename\n\t\tthe base name of the pole objects in the database\n");
  fprintf(fp, "\t-m meshname\n\t\tthe base name of the mesh objects in the database\n");
  fprintf(fp, "\t-w wirename\n\t\tthe base name of the wire objects in the database\n");
  fprintf(fp, "\t-s segmentname\n\t\tthe base name of the segment objects in the database\n");
  fprintf(fp, "\t-[eE] [fpmw]\n\t\tthe values for the fence, poles, mesh, and wires may be edited\n\t\t'e' specifies to edit while 'E' specifies the opposite of the defaults\n\t\tuseful for interactive mode only\n");
  fprintf(fp, "\t-[gG] [fpm]\n\t\tspecifies which parts of the fence object(s) to generate\n\t\t'g' specifies to generate the object(s)\n\t\t'G' specifies to do the opposite of the default(s)\n");
  fprintf(fp, "\t-[xX]\n\t\tdisplays some command-line parameter examples\n");
  fprintf(fp, "\t-[zZ]\n\t\tdisplays the default settings\n");
  fprintf(fp, "\n");

  fflush(stdout);

  return;
}

/*
 * argumentExamples() outputs some examples of using the command line
 * arguments in a useful manner
 **********************************/
void argumentExamples(FILE *fp, char *progname)
{
  fprintf(fp, "Usage Examples: \n\n");

  fprintf(fp, "Simple Interactive-Mode Example: \n%s %s %s %s %s %s\n",
	   progname, \
	   "-i -v", \
	   "-o", outputFilename, \
	   "-e", "f" \
	  );
  fprintf(fp, "\n");

  fprintf(fp, "Full Interactive-Mode Example: \n%s %s %s %s %s %s\n",
	   progname, \
	   "-i -v", \
	   "-o", outputFilename, \
	   "-e", "fpmw" \
	  );
  fprintf(fp, "\n");

  fprintf(fp, "Simple Parameter-Specified Example: \n%s %s %s %s %.1f %s %.1f\n",
	   progname, \
	   "-o", outputFilename, \
	   "-H", MAGNITUDE(fenceHeight), \
	   "-L", MAGNITUDE(fenceWidth) \
	  );
  fprintf(fp, "\n");

  fprintf(fp, "Extended Parameter-Specified Example: \n%s %s %s %s %s %s %s %s '%.1f %.1f %.1f' %s '%.1f %.1f %.1f' %s %.1f %s %.1f %s %.1f %s %.1f %s '%d %d %d' %s %s\n",
	   progname, \
	   "-n", id, \
	   "-u", units, \
	   "-o", outputFilename, \
	   "-h", fenceHeight[0], fenceHeight[1], fenceHeight[2], \
	   "-l", fenceWidth[0], fenceWidth[1], fenceWidth[2], \
	   "-r", wireRadius, \
	   "-R", poleRadius, \
	   "-a", wireAngle, \
	   "-j", fencePoleSpacing, \
	   "-c", fenceMaterialColor[0], fenceMaterialColor[1], fenceMaterialColor[2], \
	   "-e", "fpm" \
	  );
  fprintf(fp, "\n");

  return;
}



/*
 * defaultSettings() outputs the default values of the program to a
 * given file pointer.
 *
 ***********************/
void defaultSettings(FILE *fp)
{
  fprintf(fp, "Default values:\n\n");
  fprintf(fp, "\tVerbose mode is %s\n", verbose ? "on" : "off");
  fprintf(fp, "\tDebug mode is %s\n", debug ? "on" : "off");
  fprintf(fp, "\tInteractive mode is %s\n", interactive ? "on" : "off");
  if (interactive) fprintf(fp, "\t\tInteractive parameters for fence=%d poles=%d mesh=%d wire=%d\n", fenceFence, fencePoles, fenceMesh, fenceWire);
  fprintf(fp, "\n\tOutput file name is %s\n", outputFilename);
  fprintf(fp, "\t\tid='%s' units='%s'\n", id, units);
  fprintf(fp, "\n\tSet to generate %s%s%s object(s)\n", generateFenceParam ? "fence " : "", generatePolesParam ? "poles " : "", generateMeshParam ? "& mesh" : "");
  fprintf(fp, "\n\tFence Properties:\n");
  fprintf(fp, "\t\tStart Position[%.1f %.1f %.1f] \n\t\tEnd Position[%.1f %.1f %.1f]\n", fenceStartPosition[0], fenceStartPosition[1], fenceStartPosition[2], fenceEndPosition[0], fenceEndPosition[1], fenceEndPosition[2]);
  fprintf(fp, "\t\tHeight Vector[%.1f %.1f %.1f] \n\t\tWidth Vector[%.1f %.1f %.1f]\n", fenceHeight[0], fenceHeight[1], fenceHeight[2], fenceWidth[0], fenceWidth[1], fenceWidth[2]);
  fprintf(fp, "\t\tHeight Magnitude[%f] \n\t\tWidth Magnitude[%f]\n", MAGNITUDE(fenceHeight), MAGNITUDE(fenceWidth));
  fprintf(fp, "\t\tMaterial[%s] \n\t\tMaterial Parameters[%s] \n\t\tMaterial Color[%d %d %d]\n\n", fenceMaterial, fenceMaterialParams, fenceMaterialColor[0], fenceMaterialColor[1], fenceMaterialColor[2]);
  fprintf(fp, "\t\tPole:\n");
  fprintf(fp, "\t\t\tHeight[%f] \n\t\t\tRadius[%f] \n\t\t\tSpacing[%f] \n\n", poleHeight, poleRadius, fencePoleSpacing);
  fprintf(fp, "\t\t\tMaterial[%s] \n\t\t\tMaterial Parameters[%s] \n\t\t\tMaterial Color[%d %d %d]\n\n", poleMaterial, poleMaterialParams, poleMaterialColor[0], poleMaterialColor[1], poleMaterialColor[2]);
  fprintf(fp, "\t\tMesh:\n");
  fprintf(fp, "\t\t\tHeight[%f] \n\t\t\tWidth[%f] \n\n", meshHeight, meshWidth);
  fprintf(fp, "\t\t\tMaterial[%s] \n\t\t\tMaterial Parameters[%s] \n\t\t\tMaterial Color[%d %d %d]\n\n", meshMaterial, meshMaterialParams, meshMaterialColor[0], meshMaterialColor[1], meshMaterialColor[2]);
  fprintf(fp, "\t\t\tWire:\n");
  fprintf(fp, "\t\t\t\tWireRadius[%f] \n\t\t\t\tWireAngle[%f] \n\t\t\t\tWireSegmentLength[%f] \n\t\t\t\tWireSegmentSeparation[%f]\n\n", wireRadius, wireAngle, wireSegmentLength, wireSegmentSeparation);
  fprintf(fp, "\t\t\t\tMaterial[%s] \n\t\t\t\tMaterial Parameters[%s] \n\t\t\t\tMaterial Color[%d %d %d]\n\n", wireMaterial, wireMaterialParams, wireMaterialColor[0], wireMaterialColor[1], wireMaterialColor[2]);
  fprintf(fp, "\tCombination Names: \n");
  fprintf(fp, "\t\tFence: [%s] \n\t\tPoles: [%s] \n\t\tMesh: [%s] \n\t\tWires: [%s] \n\t\tSegments: [%s] \n\n", fenceName, poleName, meshName, wireName, segmentName);
  fprintf(fp, "\n");
  fprintf(fp, "No action performed.\n");
}


/*
 * parseArguments() is called by main to take care of all of the command-line
 * arguments that the user specifies.  The args that are read set variables
 * which, in turn, are used as fence parameters.
 *****************************/
int parseArguments(int argc, char **argv)
{
  int c = 0;

  double d=0.0;
  char *progname;
  int color[3];

  if ((progname = (char *) calloc(DEFAULT_MAXNAMELENGTH,sizeof(char))) == NULL) {
    if (debug) fprintf(DEFAULT_DEBUG_OUTPUT, "parseArguments:(char *)progname calloc FAILED\n");
    exit(1);
  }

  fflush(stdout);

  if (argc > 1) {
    strncpy(progname, argv[0], (strlen(argv[0])>DEFAULT_MAXNAMELENGTH?DEFAULT_MAXNAMELENGTH:strlen(argv[0])));
  }
  else {
    strncpy(progname, "fence\0", 6);
  }
  fflush(stdout);

  opterr = 0;

  while ((c=getopt(argc,argv,options)) != EOF){
    switch (c) {
    case 'I' :
      interactive=(DEFAULT_INTERACTIVE) ? 0 : 1;
      break;

    case 'i' :
      interactive=1;
      break;

    case 'D' :
      debug=(DEFAULT_DEBUG) ? 0 : 1;
      break;

    case 'd' :
      debug=1;
      break;

    case 'V' :
      verbose=(DEFAULT_VERBOSE) ? 0 : 1;
      break;

    case 'v' :
      verbose=1;
      break;

    case 'o' :
      memset(outputFilename, 0, DEFAULT_MAXNAMELENGTH);
      strncpy(outputFilename, optarg, DEFAULT_MAXNAMELENGTH);
      break;
    case 'O' :
      memset(outputFilename, 0, DEFAULT_MAXNAMELENGTH);
      strncpy(outputFilename, optarg, DEFAULT_MAXNAMELENGTH);
      break;

    case 'n' :
      memset(id, 0, DEFAULT_MAXNAMELENGTH);
      strncpy(id, optarg, DEFAULT_MAXNAMELENGTH);
      break;
    case 'N' :
      memset(id, 0, DEFAULT_MAXNAMELENGTH);
      strncpy(id, optarg, DEFAULT_MAXNAMELENGTH);
      break;

    case 'u' :
      memset(units, 0, DEFAULT_MAXNAMELENGTH);
      strncpy(units, optarg, DEFAULT_MAXNAMELENGTH);
      break;
    case 'U' :
      memset(units, 0, DEFAULT_MAXNAMELENGTH);
      strncpy(units, optarg, DEFAULT_MAXNAMELENGTH);
      break;

    case 'h' :
      if ((sscanf(optarg, "%lf %lf %lf", &fenceHeight[0], &fenceHeight[1], &fenceHeight[2]))!=3) {
	(void)argumentHelp(DEFAULT_VERBOSE_OUTPUT, progname, "Invalid number of parameters to height: need x, y, z values");
	exit(1);
      }
      if ((double)MAGNITUDE(fenceHeight) == 0.0) {
	(void)argumentHelp(DEFAULT_VERBOSE_OUTPUT, progname, "Fence height may not be set to zero");
	exit(1);
      }
      poleHeight = (double) MAGNITUDE(fenceHeight);
      meshHeight = (double) poleHeight;
      break;
    case 'H' :
      if ((d=(double)atof(optarg))!=0.0) {
	fenceHeight[0]=0.0;
	fenceHeight[1]=0.0;
	fenceHeight[2]=d;
	poleHeight=(double)MAGNITUDE(fenceHeight);
	meshHeight=(double)poleHeight;
      }
      else {
	(void)argumentHelp(DEFAULT_VERBOSE_OUTPUT, progname, "Fence height may not be set to zero");
	exit(1);
      }
      break;

    case 'l' :
      if ((sscanf(optarg, "%lf %lf %lf", &fenceWidth[0], &fenceWidth[1], &fenceWidth[2]))!=3) {
	(void)argumentHelp(DEFAULT_VERBOSE_OUTPUT, progname, "Invalid number of parameters to width: need x, y, z values");
	exit(1);
      }
      if ((double)MAGNITUDE(fenceWidth) == 0.0) {
	(void)argumentHelp(DEFAULT_VERBOSE_OUTPUT, progname, "Fence width may not be set to zero");
	exit(1);
      }
      meshWidth = (double) MAGNITUDE(fenceWidth);
      break;
    case 'L' :
      if ((d=(double)atof(optarg))!=0.0) {
	fenceWidth[0]=d;
	fenceWidth[1]=0.0;
	fenceWidth[2]=0.0;
	meshWidth=(double)MAGNITUDE(fenceWidth);
      }
      else {
	(void)argumentHelp(DEFAULT_VERBOSE_OUTPUT, progname, "Fence width may not be set to zero");
	exit(1);
      }
      break;

    case 'r' :
      if ((d=(double)atof(optarg))!=0.0) {
	wireRadius=d;
      }
      else {
	(void)argumentHelp(DEFAULT_VERBOSE_OUTPUT, progname, "Wire radius may not be set to zero");
	exit(1);
      }
      break;
    case 'R' :
      if ((d=(double)atof(optarg))!=0.0) {
	poleRadius=d;
      }
      else {
	(void)argumentHelp(DEFAULT_VERBOSE_OUTPUT, progname, "Pole radius may not be set to zero");
	exit(1);
      }
      break;

    case 'a' :
      if ((d=(double)atof(optarg))!=0.0) {
	wireAngle=d;
      }
      else {
	(void)argumentHelp(DEFAULT_VERBOSE_OUTPUT, progname, "Wire angle may not be set to zero");
	exit(1);
      }
      break;
    case 'A' :
      if ((d=(double)atof(optarg))!=0.0) {
	wireAngle=d;
      }
      else {
	(void)argumentHelp(DEFAULT_VERBOSE_OUTPUT, progname, "Wire angle may not be set to zero");
	exit(1);
      }
      break;

    case 'j' :
      if ((d=(double)atof(optarg))!=0.0) {
	fencePoleSpacing=d;
      }
      else {
	(void)argumentHelp(DEFAULT_VERBOSE_OUTPUT, progname, "Pole spacing may not be set to zero");
	exit(1);
      }
      break;
    case 'J' :
      if ((d=(double)atof(optarg))!=0.0) {
	fencePoleSpacing=d;
      }
      else {
	(void)argumentHelp(DEFAULT_VERBOSE_OUTPUT, progname, "Pole spacing may not be set to zero");
	exit(1);
      }
      break;

    case 't' :
      memset(fenceMaterial, 0, DEFAULT_MAXNAMELENGTH*3);
      strncpy(fenceMaterial, optarg, DEFAULT_MAXNAMELENGTH*3);
      break;
    case 'T' :
      memset(fenceMaterial, 0, DEFAULT_MAXNAMELENGTH*3);
      memset(poleMaterial, 0, DEFAULT_MAXNAMELENGTH*3);
      memset(meshMaterial, 0, DEFAULT_MAXNAMELENGTH*3);
      memset(wireMaterial, 0, DEFAULT_MAXNAMELENGTH*3);
      strncpy(fenceMaterial, optarg, DEFAULT_MAXNAMELENGTH*3);
      strncpy(poleMaterial, optarg, DEFAULT_MAXNAMELENGTH*3);
      strncpy(meshMaterial, optarg, DEFAULT_MAXNAMELENGTH*3);
      strncpy(wireMaterial, optarg, DEFAULT_MAXNAMELENGTH*3);
      break;

    case 'b' :
      memset(fenceMaterialParams, 0, DEFAULT_MAXNAMELENGTH*3);
      strncpy(fenceMaterialParams, optarg, DEFAULT_MAXNAMELENGTH*3);
      break;
    case 'B' :
      memset(fenceMaterialParams, 0, DEFAULT_MAXNAMELENGTH*3);
      memset(poleMaterialParams, 0, DEFAULT_MAXNAMELENGTH*3);
      memset(meshMaterialParams, 0, DEFAULT_MAXNAMELENGTH*3);
      memset(wireMaterialParams, 0, DEFAULT_MAXNAMELENGTH*3);
      strncpy(fenceMaterialParams, optarg, DEFAULT_MAXNAMELENGTH*3);
      strncpy(poleMaterialParams, optarg, DEFAULT_MAXNAMELENGTH*3);
      strncpy(meshMaterialParams, optarg, DEFAULT_MAXNAMELENGTH*3);
      strncpy(wireMaterialParams, optarg, DEFAULT_MAXNAMELENGTH*3);
      break;

    case 'c' :
      if ((sscanf(optarg, "%u %u %u", (unsigned int *)&color[0], (unsigned int *)&color[1], (unsigned int *)&color[2]))!=3) {
	(void)argumentHelp(DEFAULT_VERBOSE_OUTPUT, progname, "Invalid number of parameters for material color: need r, g, b values");
	exit(1);
      }
      if ((color[0]<0)|(color[0]>255)|(color[1]<0)|(color[1]>255)|(color[2]<0)|(color[2]>255)) {
	(void)argumentHelp(DEFAULT_VERBOSE_OUTPUT, progname, "Fence material color values must be in the range of 0 to 255 inclusive");
	exit(1);
      }
      fenceMaterialColor[0] = (unsigned char)color[0];
      fenceMaterialColor[1] = (unsigned char)color[1];
      fenceMaterialColor[2] = (unsigned char)color[2];
      break;
    case 'C' :
      if ((sscanf(optarg, "%u %u %u", (unsigned int *)&color[0], (unsigned int *)&color[1], (unsigned int *)&color[2]))!=3) {
	(void)argumentHelp(DEFAULT_VERBOSE_OUTPUT, progname, "Invalid number of parameters for material color: need r, g, b values");
	exit(1);
      }
      if ((color[0]<0)|(color[0]>255)|(color[1]<0)|(color[1]>255)|(color[2]<0)|(color[2]>255)) {
	(void)argumentHelp(DEFAULT_VERBOSE_OUTPUT, progname, "Fence material color values must be in the range of 0 to 255 inclusive");
	exit(1);
      }
      fenceMaterialColor[0] = (unsigned char)color[0];
      fenceMaterialColor[1] = (unsigned char)color[1];
      fenceMaterialColor[2] = (unsigned char)color[2];
      poleMaterialColor[0] = fenceMaterialColor[0];
      poleMaterialColor[1] = fenceMaterialColor[1];
      poleMaterialColor[2] = fenceMaterialColor[2];
      meshMaterialColor[0] = fenceMaterialColor[0];
      meshMaterialColor[1] = fenceMaterialColor[1];
      meshMaterialColor[2] = fenceMaterialColor[2];
      wireMaterialColor[0] = fenceMaterialColor[0];
      wireMaterialColor[1] = fenceMaterialColor[1];
      wireMaterialColor[2] = fenceMaterialColor[2];
      break;


    case 'f' :
      memset(fenceName, 0, DEFAULT_MAXNAMELENGTH);
      strncpy(fenceName, optarg, DEFAULT_MAXNAMELENGTH);
      break;
    case 'F' :
      memset(fenceName, 0, DEFAULT_MAXNAMELENGTH);
      strncpy(fenceName, optarg, DEFAULT_MAXNAMELENGTH);
      break;

    case 'p' :
      memset(poleName, 0, DEFAULT_MAXNAMELENGTH);
      strncpy(poleName, optarg, DEFAULT_MAXNAMELENGTH);
      break;
    case 'P' :
      memset(poleName, 0, DEFAULT_MAXNAMELENGTH);
      strncpy(poleName, optarg, DEFAULT_MAXNAMELENGTH);
      break;

    case 'm' :
      memset(meshName, 0, DEFAULT_MAXNAMELENGTH);
      strncpy(meshName, optarg, DEFAULT_MAXNAMELENGTH);
      break;
    case 'M' :
      memset(meshName, 0, DEFAULT_MAXNAMELENGTH);
      strncpy(meshName, optarg, DEFAULT_MAXNAMELENGTH);
      break;

    case 'w' :
      memset(wireName, 0, DEFAULT_MAXNAMELENGTH);
      strncpy(wireName, optarg, DEFAULT_MAXNAMELENGTH);
      break;
    case 'W' :
      memset(wireName, 0, DEFAULT_MAXNAMELENGTH);
      strncpy(wireName, optarg, DEFAULT_MAXNAMELENGTH);
      break;

    case 's' :
      memset(segmentName, 0, DEFAULT_MAXNAMELENGTH);
      strncpy(segmentName, optarg, DEFAULT_MAXNAMELENGTH);
      break;
    case 'S' :
      memset(segmentName, 0, DEFAULT_MAXNAMELENGTH);
      strncpy(segmentName, optarg, DEFAULT_MAXNAMELENGTH);
      break;

    case 'e' :
      fenceFence = 0;
      fencePoles = 0;
      fenceMesh = 0;
      fenceWire = 0;
      if (strchr(optarg, 'f')!=NULL) fenceFence = 1;
      if (strchr(optarg, 'p')!=NULL) fencePoles = 1;
      if (strchr(optarg, 'm')!=NULL) fenceMesh = 1;
      if (strchr(optarg, 'w')!=NULL) fenceWire = 1;
      if (strchr(optarg, 'F')!=NULL) fenceFence = 1;
      if (strchr(optarg, 'P')!=NULL) fencePoles = 1;
      if (strchr(optarg, 'M')!=NULL) fenceMesh = 1;
      if (strchr(optarg, 'W')!=NULL) fenceWire = 1;
      break;
    case 'E' :
      if (strchr(optarg, 'f')!=NULL) fenceFence = (DEFAULT_FENCEFENCE) ? 0 : 1;
      if (strchr(optarg, 'p')!=NULL) fencePoles = (DEFAULT_FENCEPOLES) ? 0 : 1;
      if (strchr(optarg, 'm')!=NULL) fenceMesh = (DEFAULT_FENCEMESH) ? 0 : 1;
      if (strchr(optarg, 'w')!=NULL) fenceWire = (DEFAULT_FENCEWIRE) ? 0 : 1;
      if (strchr(optarg, 'F')!=NULL) fenceFence = (DEFAULT_FENCEFENCE) ? 0 : 1;
      if (strchr(optarg, 'P')!=NULL) fencePoles = (DEFAULT_FENCEPOLES) ? 0 : 1;
      if (strchr(optarg, 'M')!=NULL) fenceMesh = (DEFAULT_FENCEMESH) ? 0 : 1;
      if (strchr(optarg, 'W')!=NULL) fenceWire = (DEFAULT_FENCEWIRE) ? 0 : 1;
      break;

    case 'g' :
      generateFenceParam = 0;
      generatePolesParam = 0;
      generateMeshParam = 0;
      if (strchr(optarg, 'f')!=NULL) generateFenceParam = 1;
      if (strchr(optarg, 'p')!=NULL) generatePolesParam = 1;
      if (strchr(optarg, 'm')!=NULL) generateMeshParam = 1;
      if (strchr(optarg, 'F')!=NULL) generateFenceParam = 1;
      if (strchr(optarg, 'P')!=NULL) generatePolesParam = 1;
      if (strchr(optarg, 'M')!=NULL) generateMeshParam = 1;
      if (generateFenceParam == generatePolesParam == generateMeshParam == 0) {
	(void)argumentHelp(DEFAULT_VERBOSE_OUTPUT, progname, "Invalid generate parameters specified");
	exit(1);
      }
      break;
    case 'G' :
      if (strchr(optarg, 'f')!=NULL) generateFenceParam = (DEFAULT_GENERATEFENCE) ? 0 : 1;
      if (strchr(optarg, 'p')!=NULL) generatePolesParam = (DEFAULT_GENERATEPOLES) ? 0 : 1;
      if (strchr(optarg, 'm')!=NULL) generateMeshParam = (DEFAULT_GENERATEMESH) ? 0 : 1;
      if (strchr(optarg, 'F')!=NULL) generateFenceParam = (DEFAULT_GENERATEFENCE) ? 0 : 1;
      if (strchr(optarg, 'P')!=NULL) generatePolesParam = (DEFAULT_GENERATEPOLES) ? 0 : 1;
      if (strchr(optarg, 'M')!=NULL) generateMeshParam = (DEFAULT_GENERATEMESH) ? 0 : 1;
      if (generateFenceParam == generatePolesParam == generateMeshParam == 0) {
	(void)argumentHelp(DEFAULT_VERBOSE_OUTPUT, progname, "Invalid generate parameters specified or all specified to zero");
	exit(1);
      }
      break;

    case 'x' :
      (void)argumentExamples(DEFAULT_VERBOSE_OUTPUT, progname);
      exit(1);
      break;
    case 'X' :
      (void)argumentHelp(DEFAULT_VERBOSE_OUTPUT, progname, "Example assistance");
      (void)argumentExamples(DEFAULT_VERBOSE_OUTPUT, progname);
      exit(1);
      break;

    case 'z' :
      (void)defaultSettings(DEFAULT_VERBOSE_OUTPUT);
      exit(1);
      break;
    case 'Z' :
      (void)argumentHelp(DEFAULT_VERBOSE_OUTPUT, progname, "Full parameter assistance");
      (void)defaultSettings(DEFAULT_VERBOSE_OUTPUT);
      exit(1);
      break;

    case '?' :
  fflush(stdout);
      (void)argumentHelp(DEFAULT_VERBOSE_OUTPUT, progname, "Command-line argument assistance");
      exit(1);
      break;

    default  : /*shouldn't be reached since getopt throws a ? for args not found*/
  fflush(stdout);
      (void)argumentHelp(DEFAULT_VERBOSE_OUTPUT, progname, "Illegal command-line argument");
      exit(1);
      break;
    }
  }
  fflush(stdout);

  return(optind);
}

/*
 * printMatrix() does just that, it prints out a matrix and a label passed to
 * some fp (usually DEFAULT_DEBUG_OUTPUT or DEFAULT_VERBOSE_OUTPUT).
 *************************/
void printMatrix(FILE *fp, char *n, fastf_t *m)
{
  int i = 0;
  fprintf(fp, "\n-----%s------\n", n);
  for (i = 0; i < 16; i++) {
    if (i%4 == 0 && i != 0) fprintf(fp, "\n");
    fprintf(fp, "%6.2f ", m[i]);
  }
  fprintf(fp, "\n-----------\n");
}


/*
 * getName() returns a name back based on a base string, a numerical id and a
 * parameter string for merging those two parameters.  Basically it adds the
 * id number to the end of the base so that we can set unique ids for our
 * objects and groups.  (i.e. base="rcc", id="5"==> returns "rcc005.s" or
 * something like that)
 ***********************************/
char *getName(char *base, int id, char *paramstring)
{
  static char name[DEFAULT_MAXNAMELENGTH];

  memset(name, 0, DEFAULT_MAXNAMELENGTH);

  if (id>=0) sprintf(name, paramstring, base, id);
  else sprintf(name, paramstring, base);

  if (debug) fprintf(DEFAULT_DEBUG_OUTPUT, "getName(): base[%s], id[%d]\n", base, id);
  if (verbose) fprintf(DEFAULT_VERBOSE_OUTPUT, "Using name[%s]\n", name);

  return name;
}


/*
 * getPrePostName() returns a name for a region.  the name is formed by
 * simply appending three given strings passed.  the three strings comprise
 * the base, prefix, and suffix of the name.  any three are optional by
 * sending a NULL parameter
 *****************************************/
char *getPrePostName(char *prefix, char *base, char *suffix)
{
  static char newname[DEFAULT_MAXNAMELENGTH];

  memset(newname, 0, DEFAULT_MAXNAMELENGTH);

  if (prefix) sprintf(newname, "%s", prefix);
  if (base) sprintf(newname, "%s%s", newname, base);
  if (suffix) sprintf(newname, "%s%s", newname, suffix);

  return newname;
}


/*
 * generateFence_s() is a simplified version of gererateFence() (this is the same for all
 * members of the "generate" family).  It's function is to provide a default-value based
 * interface to generating fence.  That is, the function may be called with as few parameters
 * as necessary to actually generate the fence.
 *************************************************************/
int generateFence_s(struct rt_wdb *fp, char *fencename, fastf_t *startposition, fastf_t *endposition)
{
  vect_t widthvector;
  vect_t heightvector;

  VSUB2(widthvector, endposition, startposition);
  VMOVE(heightvector, fenceHeight);

  return generateFence(fp, fencename, startposition, heightvector, widthvector);
}

int generateFence(struct rt_wdb *fp, char *fencename, fastf_t *startposition, fastf_t *heightvector, fastf_t *widthvector)
{
  int errors=0;
  int poleerrors=0;
  int mesherrors=0;

  struct wmember fencemembers;
  struct wmember fenceregionmembers;

  BU_LIST_INIT(&fencemembers.l);
  BU_LIST_INIT(&fenceregionmembers.l);

  VMOVE(fenceStartPosition, startposition);
  VMOVE(fenceHeight, heightvector);
  VMOVE(fenceWidth, widthvector);

  if (verbose) fprintf(DEFAULT_VERBOSE_OUTPUT, "\nBeginning fence [%s] generation...\n", fencename);

  if ((mk_id_units(fp, id, units))==0) {
    if (debug) fprintf(DEFAULT_DEBUG_OUTPUT, "generateFence:id[%s], units[%s]\n", id, units);
    if (verbose) fprintf(DEFAULT_VERBOSE_OUTPUT, "\"%s\" (units==\"%s\")\n", id, units);
  }
  else {
    if (debug) fprintf(DEFAULT_DEBUG_OUTPUT, "generateFence:mk_id_units FAILED with id[%s], units[%s]", id, units);
    errors++;
  }

  if (debug) fprintf(DEFAULT_DEBUG_OUTPUT, "generateFence:generatePolesParam[%d], generateMeshParam[%d], generateFenceParam[%d]\n", generatePolesParam, generateMeshParam, generateFenceParam);
  if (debug) fprintf(DEFAULT_DEBUG_OUTPUT, "generateFence:startposition[%f][%f][%f]\n", startposition[0], startposition[1], startposition[2]);
  if (debug) fprintf(DEFAULT_DEBUG_OUTPUT, "generateFence:fencewidth[%f][%f][%f]\n", widthvector[0], widthvector[1], widthvector[2]);
  if (debug) fprintf(DEFAULT_DEBUG_OUTPUT, "generateFence:fenceheight[%f][%f][%f]\n", heightvector[0], heightvector[1], heightvector[2]);

  if (generatePolesParam) {
    if ((poleerrors=generatePoles_s(fp, poleName))==0){
      if (verbose) fprintf(DEFAULT_VERBOSE_OUTPUT, "...Pole generated\n");

      if ((mk_addmember(poleName, &fencemembers.l, NULL, WMOP_UNION))!=NULL){
	if (verbose) fprintf(DEFAULT_VERBOSE_OUTPUT, "...building fence...poles added...\n");
      }
      else {
	if (verbose) fprintf(DEFAULT_VERBOSE_OUTPUT, "...building fence...ERROR adding poles to fence combination\n");
	if (debug) fprintf(DEFAULT_DEBUG_OUTPUT, "generateFence:mk_addmember poleName[%s] FAILED\n", poleName);
	errors++;
      }
      if ((mk_addmember(poleName, &fenceregionmembers.l, NULL, WMOP_UNION))!=NULL){
	if (verbose) fprintf(DEFAULT_VERBOSE_OUTPUT, "...building fence region...poles added...\n");
      }
      else {
	if (verbose) fprintf(DEFAULT_VERBOSE_OUTPUT, "...building fence region...ERROR adding poles to fence region\n");
	if (debug) fprintf(DEFAULT_DEBUG_OUTPUT, "generateFence:mk_addmember poleName[%s] FAILED\n", poleName);
	errors++;
      }
    }
    else {
      if (verbose) fprintf(DEFAULT_VERBOSE_OUTPUT, "...Pole generation FAILED\n");
      errors+=poleerrors;
    }


  }
  if (generateMeshParam) {
    if ((mesherrors+=generateMesh_s(fp, meshName))==0) {
      if (verbose) fprintf(DEFAULT_VERBOSE_OUTPUT, "...Mesh generated\n");

      if ((mk_addmember(meshName, &fencemembers.l, NULL, WMOP_UNION))!=NULL){
	if (verbose) fprintf(DEFAULT_VERBOSE_OUTPUT, "...building fence...mesh added...\n");
      }
      else {
	if (verbose) fprintf(DEFAULT_VERBOSE_OUTPUT, "...building fence...ERROR adding mesh to fence combination\n");
	if (debug) fprintf(DEFAULT_DEBUG_OUTPUT, "generateFence:mk_addmember meshName[%s] FAILED\n", meshName);
      }
      if ((mk_addmember(meshName, &fenceregionmembers.l, NULL, WMOP_UNION))!=NULL){
	if (verbose) fprintf(DEFAULT_VERBOSE_OUTPUT, "...building fence region...mesh added...\n");
      }
      else {
	if (verbose) fprintf(DEFAULT_VERBOSE_OUTPUT, "...building fence region...ERROR adding mesh to fence region\n");
	if (debug) fprintf(DEFAULT_DEBUG_OUTPUT, "generateFence:mk_addmember meshName[%s] FAILED\n", meshName);
      }

    }
    else {
      if (verbose) fprintf(DEFAULT_VERBOSE_OUTPUT, "...Mesh generation FAILED\n");
      errors+=mesherrors;
    }
  }

  if (generateFenceParam) {
    if (mk_lcomb (fp, fencename, &fencemembers, 0, NULL, NULL, NULL, 0)==0) {
      if (verbose) fprintf(DEFAULT_VERBOSE_OUTPUT, "...completed fence [%s] combination\n", fencename);
      if (debug) fprintf(DEFAULT_DEBUG_OUTPUT, "generateFence:fencename[%s]\n", fencename);
    }
    else {
      if (debug) fprintf(DEFAULT_DEBUG_OUTPUT, "generateFence:mk_lcomb fencename[%s] FAILED\n", fencename);
      errors++;
    }

    if (mk_lcomb (fp, getPrePostName(NULL, fencename, DEFAULT_REGIONSUFFIX), &fenceregionmembers, 1, fenceMaterial, fenceMaterialParams, fenceMaterialColor, 0)==0) {
      if (verbose) fprintf(DEFAULT_VERBOSE_OUTPUT, "...completed fence [%s] region\n", getPrePostName(NULL, fencename, DEFAULT_REGIONSUFFIX));
      if (debug) fprintf(DEFAULT_DEBUG_OUTPUT, "generateFence:fencename[%s]\n", getPrePostName(NULL, fencename, DEFAULT_REGIONSUFFIX));
    }
    else {
      if (debug) fprintf(DEFAULT_DEBUG_OUTPUT, "generateFence:mk_lcomb fencename[%s] FAILED (region)\n", getPrePostName(NULL, fencename, DEFAULT_REGIONSUFFIX));
      errors++;
    }
  }


  return errors;
}


/*
 * generatePoles() is the function that actually generates all of the
 * poles for the scene
 ********************************/
int generatePoles_s(struct rt_wdb *fp, char *polename)
{
  vect_t polevector;

  VMOVE(polevector, fenceHeight);
  VUNITIZE(polevector);
  VSCALE(polevector, polevector, poleHeight);

  return generatePoles(fp, polename, fenceStartPosition, polevector, fenceWidth, poleRadius);
}

int generatePoles(struct rt_wdb *fp, char *polename, fastf_t *startposition, fastf_t *heightvector, fastf_t *widthvector, double radius)
{
  int count=0;
  int errors=0;
  double step=0.0;
  double steplocationlimit=0.0;
  double fencepolespacing=(double)fencePoleSpacing;
  point_t tempposition;
  vect_t polespacingvector;

  struct wmember polemembers;
  struct wmember poleregionmembers;

  BU_LIST_INIT(&polemembers.l);
  BU_LIST_INIT(&poleregionmembers.l);

  if (verbose) fprintf(DEFAULT_VERBOSE_OUTPUT, "\nBeginning pole [%s] generation...\n", polename);

  if (debug) fprintf(DEFAULT_DEBUG_OUTPUT, "generatePoles:polename[%s], startposition[%f][%f][%f], heightvector[%f][%f][%f], radius[%f]\n", polename, startposition[0], startposition[1], startposition[2], heightvector[0], heightvector[1], heightvector[2], radius);


  VMOVE(tempposition, startposition);
  steplocationlimit=MAGNITUDE(widthvector);
  VMOVE(polespacingvector, widthvector);
  VUNITIZE(polespacingvector);
  VSCALE(polespacingvector, polespacingvector, fencepolespacing);

  if (debug) fprintf(DEFAULT_DEBUG_OUTPUT, "generatePoles:steplocationlimit[%f], polespacingvector[%f][%f][%f]\n", steplocationlimit, polespacingvector[0], polespacingvector[1], polespacingvector[2]);

  for (step=0.0, count=0; step<(steplocationlimit-(fencepolespacing/3)); step+=fencepolespacing, count++){

    if (debug) fprintf(DEFAULT_DEBUG_OUTPUT, "generatePoles: tempposition[%f][%f][%f]\n", tempposition[0], tempposition[1], tempposition[2]);

    if ((mk_rcc(fp, getName(polename, count, DEFAULT_POLEBASICPARAM), tempposition, heightvector, radius))==0) {
      if (verbose) fprintf(DEFAULT_VERBOSE_OUTPUT, "...Pole [%d] generated\n", count);
    }
    else {
      if (verbose) fprintf(DEFAULT_VERBOSE_OUTPUT, "...Pole [%d] generation FAILED\n", count);
      if (debug) fprintf(DEFAULT_DEBUG_OUTPUT, "generatePoles: polename[%s], count[%d], tempposition[%f][%f][%f], heightvector[%f][%f][%f], radius[%f]\n", polename, count, tempposition[0], tempposition[1], tempposition[2], heightvector[0], heightvector[1], heightvector[2], radius);
      errors++;
    }

    if ((mk_addmember(getName(polename, count, DEFAULT_POLEBASICPARAM), &polemembers.l, NULL, WMOP_UNION))!=NULL){
      if (verbose) fprintf(DEFAULT_VERBOSE_OUTPUT, "...adding pole [%d] to pole list\n", count);
    }
    else {
      if (debug) fprintf(DEFAULT_DEBUG_OUTPUT, "generatePoles:mk_addmember count[%d] FAILED\n", count);
      errors++;
    }
    if ((mk_addmember(getName(polename, count, DEFAULT_POLEBASICPARAM), &poleregionmembers.l, NULL, WMOP_UNION))!=NULL){
      if (verbose) fprintf(DEFAULT_VERBOSE_OUTPUT, "...adding pole [%d] to pole region list\n", count);
    }
    else {
      if (debug) fprintf(DEFAULT_DEBUG_OUTPUT, "generatePoles:mk_addmember count[%d] FAILED(region)\n", count);
      errors++;
    }

    VADD2(tempposition, tempposition, polespacingvector);
  }
  VMOVE(tempposition, startposition);
  VADD2(tempposition, tempposition, widthvector);

  if (debug) fprintf(DEFAULT_DEBUG_OUTPUT, "generatePoles: tempposition[%f][%f][%f]\n", tempposition[0], tempposition[1], tempposition[2]);

  if ((mk_rcc(fp, getName(polename, count, DEFAULT_POLEBASICPARAM), tempposition, heightvector, radius))==0) {
    if (verbose) fprintf(DEFAULT_VERBOSE_OUTPUT, "...Pole [%d] generated\n", count);
  }
  else {
    if (verbose) fprintf(DEFAULT_VERBOSE_OUTPUT, "...Pole [%d] generation FAILED\n", count);
    if (debug) fprintf(DEFAULT_DEBUG_OUTPUT, "generatePoles: polename[%s], count[%d], tempposition[%f][%f][%f], heightvector[%f][%f][%f], radius[%f]\n", polename, count, tempposition[0], tempposition[1], tempposition[2], heightvector[0], heightvector[1], heightvector[2], radius);
    errors++;
  }

  if ((mk_addmember(getName(polename, count, DEFAULT_POLEBASICPARAM), &polemembers.l, NULL, WMOP_UNION))!=NULL){
    if (verbose) fprintf(DEFAULT_VERBOSE_OUTPUT, "...adding pole [%d] to pole list\n", count);
  }
  else {
    if (debug) fprintf(DEFAULT_DEBUG_OUTPUT, "generatePoles:mk_addmember count[%d] FAILED\n", count);
    errors++;
  }
  if ((mk_addmember(getName(polename, count, DEFAULT_POLEBASICPARAM), &poleregionmembers.l, NULL, WMOP_UNION))!=NULL){
    if (verbose) fprintf(DEFAULT_VERBOSE_OUTPUT, "...adding pole [%d] to pole region list\n", count);
  }
  else {
    if (debug) fprintf(DEFAULT_DEBUG_OUTPUT, "generatePoles:mk_addmember count[%d] FAILED (region)\n", count);
    errors++;
  }


  if (mk_lcomb (fp, poleName, &polemembers, 0, NULL, NULL, NULL,  0)==0) {
    if (verbose) fprintf(DEFAULT_VERBOSE_OUTPUT, "...pole [%s] combination completed\n", poleName);
  }
  else {
    if (debug) fprintf(DEFAULT_DEBUG_OUTPUT, "generatePoles:mk_lcomb poleName[%s] FAILED\n", poleName);
    errors++;
  }
  if (mk_lcomb (fp, getPrePostName(NULL, poleName, DEFAULT_REGIONSUFFIX), &poleregionmembers, 1, poleMaterial, poleMaterialParams, poleMaterialColor,  0)==0) {
    if (verbose) fprintf(DEFAULT_VERBOSE_OUTPUT, "...pole [%s] region completed\n", getPrePostName(NULL, poleName, DEFAULT_REGIONSUFFIX));
  }
  else {
    if (debug) fprintf(DEFAULT_DEBUG_OUTPUT, "generatePoles:mk_lcomb poleName[%s] FAILED (region)\n", getPrePostName(NULL, poleName, DEFAULT_REGIONSUFFIX));
    errors++;
  }


  return errors;
}


/*
 * generateMesh() generates the wire mesh (the actual fence mesh) that
 * gets attached to the poles (if given).
 *******************************/
int generateMesh_s(struct rt_wdb *fp, char *meshname)
{
  point_t meshstartposition;
  vect_t meshheightvector;
  vect_t meshwidthvector;

  VMOVE(meshstartposition, fenceStartPosition);
  VMOVE(meshheightvector, fenceHeight);
  VMOVE(meshwidthvector, fenceWidth);
  VUNITIZE(meshheightvector);
  VUNITIZE(meshwidthvector);
  VSCALE(meshheightvector, meshheightvector, meshHeight);
  VSCALE(meshwidthvector, meshwidthvector, meshWidth);

  if (debug) printf("generateMesh_s:meshheightvector[%f][%f][%f], meshheight[%f]\n", meshheightvector[0], meshheightvector[1], meshheightvector[2], meshHeight);

  return generateMesh(fp, meshname, meshstartposition, meshheightvector, meshwidthvector);
}

int generateMesh(struct rt_wdb *fp, char *meshname, fastf_t *startposition, fastf_t *heightvector, fastf_t *widthvector)
{
  int count=0;
  int count2=0;
  int errors=0;
  double step=0.0;
  double dx=0.0;
  double dy=0.0;
  double dz=0.0;
  double width=0.0;
  double angle=wireAngle;
  double radius=wireRadius;
  point_t tempposition;
  vect_t incrementvector;

  struct wmember *matrixextractor;
  struct wmember meshmembers;
  struct wmember meshregionmembers;

  BU_LIST_INIT(&meshmembers.l);
  BU_LIST_INIT(&meshregionmembers.l);

  if (verbose) fprintf(DEFAULT_VERBOSE_OUTPUT, "\nBeginning mesh [%s] generation...\n", meshname);

  if (debug) fprintf(DEFAULT_DEBUG_OUTPUT, "generateMesh:startposition[%f][%f][%f]\n", startposition[0], startposition[1], startposition[2]);
  if (debug) fprintf(DEFAULT_DEBUG_OUTPUT, "generateMesh:widthvector[%f][%f][%f]\n", widthvector[0], widthvector[1], widthvector[2]);
  if (debug) fprintf(DEFAULT_DEBUG_OUTPUT, "generateMesh:heightvector[%f][%f][%f]\n", heightvector[0], heightvector[1], heightvector[2]);


  VMOVE(tempposition, startposition);
  if ((generateWire_s(fp, wireName, tempposition))==0){
    if (verbose) fprintf(DEFAULT_VERBOSE_OUTPUT, "...Base wire generated\n");
  }
  else {
    if (verbose) fprintf(DEFAULT_VERBOSE_OUTPUT, "...Base wire generation FAILED\n");
    errors++;
  }

  if (verbose) fprintf(DEFAULT_VERBOSE_OUTPUT, "\nWeaving mesh...\n");

  width=MAGNITUDE(widthvector);
  VMOVE(incrementvector, widthvector);
  VUNITIZE(incrementvector);
  VSCALE(incrementvector, incrementvector, (double) 2.0 * ( cos((double)RADIAN(angle)) * (wireSegmentLength-(2*radius)) ) );

  if (debug) fprintf(DEFAULT_DEBUG_OUTPUT, "generateMesh: angle[%f], radius[%f], wireSegmentLength[%f]\n", angle, radius, wireSegmentLength);
  if (debug) fprintf(DEFAULT_DEBUG_OUTPUT, "generateMesh: width[%f], MAGNITUDE(incrementvector)[%f]\n", width, MAGNITUDE(incrementvector));
  if (debug) fprintf(DEFAULT_DEBUG_OUTPUT, "generateMesh: incrementvector[%f][%f][%f]\n", incrementvector[0], incrementvector[1], incrementvector[2]);

  if (MAGNITUDE(incrementvector)!=0){
    for (count2=0, step=0.0, dx=0, dy=0, dz=0; step <= width; step += (double) MAGNITUDE(incrementvector), count2++);
    if (verbose) fprintf(DEFAULT_VERBOSE_OUTPUT, "Mesh will require [%d] basic mesh wire-pairs to reach width [%f]\n", count2, width);

    for (count=0, step=0.0; step <= width; step += (double) MAGNITUDE(incrementvector), count++) {

      if ((matrixextractor=mk_addmember(wireName, &meshmembers.l, NULL, WMOP_UNION))!=NULL){
	if (verbose) fprintf(DEFAULT_VERBOSE_OUTPUT, "...building mesh combination: wire [%d] of [%d]\n", count+1, count2);
      }
      else {
	if (debug) fprintf(DEFAULT_DEBUG_OUTPUT, "generateMesh:mk_addmember wireName[%s],count2[%d] FAILED\n", wireName, count);
	errors++;
      }
      matrixextractor->wm_mat[3]  = dx;
      matrixextractor->wm_mat[7]  = dy;
      matrixextractor->wm_mat[11] = dz;

      if ((matrixextractor=mk_addmember(wireName, &meshregionmembers.l, NULL, WMOP_UNION))!=NULL){
	if (verbose) fprintf(DEFAULT_VERBOSE_OUTPUT, "...building mesh region: wire [%d] of [%d]\n", count+1, count2);
      }
      else {
	if (debug) fprintf(DEFAULT_DEBUG_OUTPUT, "generateMesh:mk_addmember wireName[%s],count2[%d] FAILED (region)\n", wireName, count);
	errors++;
      }
      matrixextractor->wm_mat[3]  = dx;
      matrixextractor->wm_mat[7]  = dy;
      matrixextractor->wm_mat[11] = dz;

      if (debug) fprintf(DEFAULT_DEBUG_OUTPUT, "generateMesh: dx[%f], dy[%f], dz[%f]\n", dx, dy, dz);

      dx+=incrementvector[0];
      dy+=incrementvector[1];
      dz+=incrementvector[2];
      if (debug) printMatrix(DEFAULT_DEBUG_OUTPUT, "generateMesh: matrixextractor->wm_mat", matrixextractor->wm_mat);
      else if (verbose) printMatrix(DEFAULT_VERBOSE_OUTPUT, "Translation matrix for mesh wire-pairs", matrixextractor->wm_mat);
    }

    if (mk_lcomb (fp, meshname, &meshmembers, 0, NULL, NULL, NULL, 0)==0) {
      if (verbose) fprintf(DEFAULT_VERBOSE_OUTPUT, "...completed mesh [%s] combination\n", meshname);
      if (debug) fprintf(DEFAULT_DEBUG_OUTPUT, "generateMesh:meshname[%s]\n", meshname);
    }
    else {
      if (debug) fprintf(DEFAULT_DEBUG_OUTPUT, "generateMesh:mk_lcomb meshname[%s] FAILED\n", meshname);
      errors++;
    }
    if (mk_lcomb (fp, getPrePostName(NULL,meshname,DEFAULT_REGIONSUFFIX), &meshregionmembers, 1, meshMaterial, meshMaterialParams, meshMaterialColor, 0)==0) {
      if (verbose) fprintf(DEFAULT_VERBOSE_OUTPUT, "...completed mesh [%s] region\n", getPrePostName(NULL, meshname, DEFAULT_REGIONSUFFIX));
      if (debug) fprintf(DEFAULT_DEBUG_OUTPUT, "generateMesh:meshname[%s]\n", meshname);
    }
    else {
      if (debug) fprintf(DEFAULT_DEBUG_OUTPUT, "generateMesh:mk_lcomb meshname[%s] FAILED (region)\n", getPrePostName(NULL, meshname, DEFAULT_REGIONSUFFIX));
      errors++;
    }
  }
  else {
    if (verbose) fprintf(DEFAULT_VERBOSE_OUTPUT, "Error, incrementvector magnitude is zero:Cannot create mesh\n");
    if (debug) fprintf(DEFAULT_DEBUG_OUTPUT, "generateMesh:MAGNITUDE(incrementvector)==[%f]\n", MAGNITUDE(incrementvector));
    errors++;
  }
  return errors;
}


/*
 * generateWire() generates a wire-pair (two wires intertwining) that serves to form the entire
 * mesh of the fence.  The two wires are generated as a single process and are generated by creating
 * a basic segment of the fence mesh.
 *
 *****************************************/

int generateWire_s(struct rt_wdb *fp, char *wirename, fastf_t *position)
{
  vect_t wirevector;
  vect_t widthvector;

  VMOVE(wirevector, fenceHeight);
  VMOVE(widthvector, fenceWidth);
  VUNITIZE(wirevector);
  VSCALE(wirevector, wirevector, meshHeight);

  return generateWire(fp, wirename, position, wirevector, widthvector, wireRadius, wireAngle, wireSegmentLength);
}

int generateWire(struct rt_wdb *fp, char *wirename, fastf_t *position, fastf_t *fenceheightvector, fastf_t *fencewidthvector, double radius, double angle, double segmentlength)
{
  double height;
  vect_t heightvector, widthvector;
  vect_t incrementvector;
  int count=0;
  int count2=0;
  int errors=0;
  double step=0.0;
  double dx=0.0;
  double dy=0.0;
  double dz=0.0;

  struct wmember basicmeshmembers;
  struct wmember wiremembers;
  struct wmember basicmeshregionmembers;
  struct wmember wireregionmembers;
  struct wmember *matrixextractor;



  if ((matrixextractor = (struct wmember *) malloc(sizeof(struct wmember)))==NULL) {
    if (debug) fprintf(DEFAULT_DEBUG_OUTPUT, "generateWire:(struct wmember *)matrixextractor malloc FAILED\n");
    errors++;
  }
  BU_LIST_INIT(&basicmeshmembers.l);
  BU_LIST_INIT(&wiremembers.l);
  BU_LIST_INIT(&basicmeshregionmembers.l);
  BU_LIST_INIT(&wireregionmembers.l);

  if (verbose) fprintf(DEFAULT_VERBOSE_OUTPUT, "\nBeginning wire [%s] generation...\n", wirename);

  if (debug) fprintf(DEFAULT_DEBUG_OUTPUT, "generateWire:wirename[%s], position[%f][%f][%f]\n", wirename, position[0], position[1], position[2]);
  if (debug) fprintf(DEFAULT_DEBUG_OUTPUT, "generateWire:fenceheightvector[%f][%f][%f], fencewidthvector[%f][%f][%f]\n", fenceheightvector[0], fenceheightvector[1], fenceheightvector[2], fencewidthvector[0], fencewidthvector[1], fencewidthvector[2]);
  if (debug) fprintf(DEFAULT_DEBUG_OUTPUT, "generateWire:radius[%f], angle[%f], segmentlength[%f]\n", radius, angle, segmentlength);


  VMOVE(heightvector, fenceheightvector);
  VUNITIZE(heightvector);
  VMOVE(widthvector, fencewidthvector);
  VUNITIZE(widthvector);

  if ((errors+=createWire(fp, segmentName, heightvector, widthvector, radius, angle, segmentlength, wireSegmentSeparation))!=0) {
    if (verbose) fprintf(DEFAULT_VERBOSE_OUTPUT, "There were [%d] errors creating the basic unit...attempting to resolve with fewer units\n", errors);
    if (debug) fprintf(DEFAULT_DEBUG_OUTPUT, "generateWire:createWire:errors[%d]\n", errors);
  }
  for (count=0; count < DEFAULT_MESHPIECECOUNT; count++){
    if ((mk_addmember(getName(segmentName, count, DEFAULT_WIREBASICPARAM), &basicmeshmembers.l, NULL, WMOP_UNION))!=NULL){
      if (verbose) fprintf(DEFAULT_VERBOSE_OUTPUT, "...adding basic mesh segment component [%d] of [%d]\n", count, DEFAULT_MESHPIECECOUNT-1);
    }
    else {
      if (debug) fprintf(DEFAULT_DEBUG_OUTPUT, "generateWire:mk_addmember count[%d] FAILED\n", count);
      errors++;
    }
    if ((mk_addmember(getName(segmentName, count, DEFAULT_WIREBASICPARAM), &basicmeshregionmembers.l, NULL, WMOP_UNION))!=NULL){
      if (verbose) fprintf(DEFAULT_VERBOSE_OUTPUT, "...adding basic mesh segment region component [%d] of [%d]\n", count, DEFAULT_MESHPIECECOUNT-1);
    }
    else {
      if (debug) fprintf(DEFAULT_DEBUG_OUTPUT, "generateWire:mk_addmember count[%d] FAILED (region)\n", count);
      errors++;
    }
  }
  if (mk_lcomb (fp, segmentName, &basicmeshmembers, 0, NULL, NULL, NULL,  0)==0) {
    if (verbose) fprintf(DEFAULT_VERBOSE_OUTPUT, "...basic mesh segment [%s] combination completed\n", segmentName);
  }
  else {
    if (debug) fprintf(DEFAULT_DEBUG_OUTPUT, "generateWire:mk_lcomb segmentName[%s] FAILED\n", segmentName);
    errors++;
  }
  if (mk_lcomb (fp, getPrePostName(NULL, segmentName, DEFAULT_REGIONSUFFIX), &basicmeshregionmembers, 1, wireMaterial, wireMaterialParams, wireMaterialColor, 0)==0) {
    if (verbose) fprintf(DEFAULT_VERBOSE_OUTPUT, "...basic mesh segment [%s] region completed\n", getPrePostName(NULL, segmentName, DEFAULT_REGIONSUFFIX));
  }
  else {
    if (debug) fprintf(DEFAULT_DEBUG_OUTPUT, "generateWire:mk_lcomb segmentName[%s] FAILED (region)\n", getPrePostName(NULL, segmentName, DEFAULT_REGIONSUFFIX));
    errors++;
  }

  height=MAGNITUDE(fenceheightvector);
  VSCALE(incrementvector, heightvector, (double) 2.0 * ( sin((double)RADIAN(angle)) * (segmentlength+((double)2.0 * radius)) ) );

  if (debug) fprintf(DEFAULT_DEBUG_OUTPUT, "generateWire: height[%f], incrementvector[%f][%f][%f], MAGNITUDE(incrementvector)[%f]\n", height, incrementvector[0], incrementvector[1], incrementvector[2], MAGNITUDE(incrementvector));

  for (count2=0, step=0.0; step <= height; step += (double) MAGNITUDE(incrementvector), count2++);
  if (verbose) fprintf(DEFAULT_VERBOSE_OUTPUT, "Wire will require [%d] basic mesh pieces to reach height [%f]\n", count2, height);

  for (count=0, dx=0, dy=0, dz=0, step=0.0; step <= height; step += (double) MAGNITUDE(incrementvector), count++) {

    if ((matrixextractor=mk_addmember(segmentName, &wiremembers.l, NULL, WMOP_UNION))!=NULL){
      if (verbose) fprintf(DEFAULT_VERBOSE_OUTPUT, "...building base wire combination: piece [%d] of [%d]\n", count+1, count2);
    }
    else {
      if (debug) fprintf(DEFAULT_DEBUG_OUTPUT, "generateWire:mk_addmember wirename[%s],count2[%d] FAILED\n", wirename, count);
      errors++;
    }
    matrixextractor->wm_mat[3]  = dx;
    matrixextractor->wm_mat[7]  = dy;
    matrixextractor->wm_mat[11] = dz;

    if ((matrixextractor=mk_addmember(segmentName, &wireregionmembers.l, NULL, WMOP_UNION))!=NULL){
      if (verbose) fprintf(DEFAULT_VERBOSE_OUTPUT, "...building base wire region: piece [%d] of [%d]\n", count+1, count2);
    }
    else {
      if (debug) fprintf(DEFAULT_DEBUG_OUTPUT, "generateWire:mk_addmember wirename[%s],count2[%d] FAILED (region)\n", wirename, count);
      errors++;
    }
    matrixextractor->wm_mat[3]  = dx;
    matrixextractor->wm_mat[7]  = dy;
    matrixextractor->wm_mat[11] = dz;

    if (debug) fprintf(DEFAULT_DEBUG_OUTPUT, "generateWire: dx[%f], dy[%f], dz[%f]\n", dx, dy, dz);

    dx+=incrementvector[0];
    dy+=incrementvector[1];
    dz+=incrementvector[2];
    if (debug) printMatrix(DEFAULT_DEBUG_OUTPUT, "generateWire: matrixextractor->wm_mat", matrixextractor->wm_mat);
    else if (verbose) printMatrix(DEFAULT_VERBOSE_OUTPUT, "Translation matrix of wire segments", matrixextractor->wm_mat);
  }

  if (mk_lcomb (fp, wirename, &wiremembers, 0, NULL, NULL, NULL, 0)==0) {
    if (verbose) fprintf(DEFAULT_VERBOSE_OUTPUT, "...completed basic wire [%s] combination\n", wirename);
    if (debug) fprintf(DEFAULT_DEBUG_OUTPUT, "generateWire:wirename[%s]\n", wirename);
  }
  else {
    if (debug) fprintf(DEFAULT_DEBUG_OUTPUT, "generateWire:mk_lcomb wirename[%s] FAILED\n", wirename);
    errors++;
  }
  if (mk_lcomb (fp, getPrePostName(NULL, wirename, DEFAULT_REGIONSUFFIX), &wireregionmembers, 1, wireMaterial, wireMaterialParams, wireMaterialColor, 0)==0) {
    if (verbose) fprintf(DEFAULT_VERBOSE_OUTPUT, "...completed basic wire [%s] region\n", getPrePostName(NULL, wirename, DEFAULT_REGIONSUFFIX));
  }
  else {
    if (debug) fprintf(DEFAULT_DEBUG_OUTPUT, "generateWire:mk_lcomb wirename[%s] FAILED (region)\n", getPrePostName(NULL, wirename, DEFAULT_REGIONSUFFIX));
    errors++;
  }


  return errors;
}

/*
 * createWire generates the basic "cell" of the fence which is physically a section of the fence
 * mesh that forms the "honey-comb"-like appearance for a standard chain-link fence (the triangular
 * subsections of the fence.  the actual cell is what forms two entire and intertwining wires.  the
 * wire segment consists of exactly 20 "pieces" (currently) which are the cylinders and spheres that
 * form the twists and straits of the wire.
 *
 ***************************************************************************************************************/
int createWire(struct rt_wdb *fp, char *segmentname, fastf_t *heightvector, fastf_t *widthvector, double radius, double angle, double segmentlength, double segmentdepthseparation)
{
  int count=0;
  int errors=0;
  point_t position=DEFAULT_FENCESTARTPOSITION;
  vect_t segmentvector=DEFAULT_FENCESTARTPOSITION;

  vect_t upvector=DEFAULT_FENCESTARTPOSITION;
  vect_t sidevector=DEFAULT_FENCESTARTPOSITION;

  vect_t moveupvector=DEFAULT_FENCESTARTPOSITION;
  vect_t movebackvector=DEFAULT_FENCESTARTPOSITION;
  vect_t movesidevector=DEFAULT_FENCESTARTPOSITION;

  double anglex=0.0;
  double angley=0.0;

  if (verbose) fprintf(DEFAULT_VERBOSE_OUTPUT, "\nBeginning basic wire-mesh component [%s] creation...\n", segmentname);

  if (debug) fprintf(DEFAULT_DEBUG_OUTPUT, "createWire:heightvector[%f][%f][%f], widthvector[%f][%f][%f]\n", heightvector[0], heightvector[1], heightvector[2], widthvector[0], widthvector[1], widthvector[2]);
  if (debug) fprintf(DEFAULT_DEBUG_OUTPUT, "createWire:radius[%f], angle[%f], segmentlength[%f], segmentdepthseparation[%f]\n", radius, angle, segmentlength, segmentdepthseparation);


  /*first segment going ne*/
  count=0;

  VMOVE(position, position);

  angley=(double) sin( (double) RADIAN(angle) ) * segmentlength;
  anglex=(double) cos( (double) RADIAN(angle) ) * segmentlength;

  VSCALE(upvector, heightvector, angley);
  VSCALE(sidevector, widthvector, anglex);
  VADD2(segmentvector, upvector, sidevector);


  if (debug) fprintf(DEFAULT_DEBUG_OUTPUT, "createWire:segmentvector[%f][%f][%f]\n", segmentvector[0], segmentvector[1], segmentvector[2]);
  if (debug) fprintf(DEFAULT_DEBUG_OUTPUT, "createWire:position[%f][%f][%f]\n", position[0], position[1], position[2]);

  if ((mk_rcc(fp, getName(segmentName, count, DEFAULT_WIREBASICPARAM), position, segmentvector, radius))==0) {
    if (verbose) fprintf(DEFAULT_VERBOSE_OUTPUT, "...Wire segment [%d] generated\n", count);
  }
  else {
    if (verbose) fprintf(DEFAULT_VERBOSE_OUTPUT, "...Wire segment [%d] generation FAILED\n", count);
    errors++;
  }


  /*second segment going ne*/
  count++;

  angley=(double) sin( (double) RADIAN(angle) ) * segmentlength;
  anglex=(double) cos( (double) RADIAN(angle) ) * segmentlength;

  VSCALE(upvector, heightvector, angley);
  VSCALE(sidevector, widthvector, anglex);
  VADD2(segmentvector, upvector, sidevector);

  VSCALE(movesidevector, widthvector, (anglex));
  VSCALE(moveupvector, heightvector, (angley));

  VSUB2(position, moveupvector, movesidevector);


  if (debug) fprintf(DEFAULT_DEBUG_OUTPUT, "createWire:segmentvector[%f][%f][%f]\n", segmentvector[0], segmentvector[1], segmentvector[2]);
  if (debug) fprintf(DEFAULT_DEBUG_OUTPUT, "createWire:position[%f][%f][%f]\n", position[0], position[1], position[2]);

  if ((mk_rcc(fp, getName(segmentName, count, DEFAULT_WIREBASICPARAM), position, segmentvector, radius))==0) {
    if (verbose) fprintf(DEFAULT_VERBOSE_OUTPUT, "...Wire segment [%d] generated\n", count);
  }
  else {
    if (verbose) fprintf(DEFAULT_VERBOSE_OUTPUT, "...Wire segment [%d] generation FAILED\n", count);
    errors++;
  }


  /*third segment going nw*/
  count++;

  VCROSS(movebackvector, heightvector, widthvector);
  VSCALE(movebackvector, movebackvector, (segmentdepthseparation+(2*radius)) );

  VMOVE(position, movebackvector);

  angley=(double) sin( (double) RADIAN((double)180.0-angle) )*segmentlength;
  anglex=(double) cos( (double) RADIAN((double)180.0-angle) )*segmentlength;

  VSCALE(upvector, heightvector, angley);
  VSCALE(sidevector, widthvector, anglex);
  VADD2(segmentvector, upvector, sidevector);


  if (debug) fprintf(DEFAULT_DEBUG_OUTPUT, "createWire:segmentvector[%f][%f][%f]\n", segmentvector[0], segmentvector[1], segmentvector[2]);
  if (debug) fprintf(DEFAULT_DEBUG_OUTPUT, "createWire:position[%f][%f][%f]\n", position[0], position[1], position[2]);

  if ((mk_rcc(fp, getName(segmentName, count, DEFAULT_WIREBASICPARAM), position, segmentvector, radius))==0) {
    if (verbose) fprintf(DEFAULT_VERBOSE_OUTPUT, "...Wire segment [%d] generated\n", count);
  }
  else {
    if (verbose) fprintf(DEFAULT_VERBOSE_OUTPUT, "...Wire segment [%d] generation FAILED\n", count);
    errors++;
  }


  /*fourth segment going nw*/
  count++;

  angley=(double) sin( (double) RADIAN(angle) ) * segmentlength;
  anglex=(double) cos( (double) RADIAN(angle) ) * segmentlength;

  VSCALE(movesidevector, widthvector, (anglex));
  VSCALE(moveupvector, heightvector, (angley));

  VADD2(position, moveupvector, movesidevector);

  VCROSS(movebackvector, heightvector, widthvector);
  VSCALE(movebackvector, movebackvector, (segmentdepthseparation+(2*radius)) );

  VADD2(position, position, movebackvector);

  angley=(double) sin( (double) RADIAN((double)180.0-angle) )*segmentlength;
  anglex=(double) cos( (double) RADIAN((double)180.0-angle) )*segmentlength;

  VSCALE(upvector, heightvector, angley);
  VSCALE(sidevector, widthvector, anglex);
  VADD2(segmentvector, upvector, sidevector);


  if (debug) fprintf(DEFAULT_DEBUG_OUTPUT, "createWire:segmentvector[%f][%f][%f]\n", segmentvector[0], segmentvector[1], segmentvector[2]);
  if (debug) fprintf(DEFAULT_DEBUG_OUTPUT, "createWire:position[%f][%f][%f]\n", position[0], position[1], position[2]);

  if ((mk_rcc(fp, getName(segmentName, count, DEFAULT_WIREBASICPARAM), position, segmentvector, radius))==0) {
    if (verbose) fprintf(DEFAULT_VERBOSE_OUTPUT, "...Wire segment [%d] generated\n", count);
  }
  else {
    if (verbose) fprintf(DEFAULT_VERBOSE_OUTPUT, "...Wire segment [%d] generation FAILED\n", count);
    errors++;
  }


  /*extension of first segment going ne*/
  count++;

  angley=(double) sin( (double) RADIAN(angle) ) * ((double)2.0 * radius);
  anglex=(double) cos( (double) RADIAN(angle) ) * ((double)2.0 * radius);

  VSCALE(upvector, heightvector, -angley);
  VSCALE(sidevector, widthvector, -anglex);

  VADD2(segmentvector, upvector, sidevector);

  VSCALE(position, heightvector, 0);


  if (debug) fprintf(DEFAULT_DEBUG_OUTPUT, "createWire:segmentvector[%f][%f][%f]\n", segmentvector[0], segmentvector[1], segmentvector[2]);
  if (debug) fprintf(DEFAULT_DEBUG_OUTPUT, "createWire:position[%f][%f][%f]\n", position[0], position[1], position[2]);

  if ((mk_rcc(fp, getName(segmentName, count, DEFAULT_WIREBASICPARAM), position, segmentvector, radius))==0) {
    if (verbose) fprintf(DEFAULT_VERBOSE_OUTPUT, "...Wire segment [%d] generated\n", count);
  }
  else {
    if (verbose) fprintf(DEFAULT_VERBOSE_OUTPUT, "...Wire segment [%d] generation FAILED\n", count);
    errors++;
  }


  /*extension of second segment going ne*/
  count++;

  angley=(double) sin( (double) RADIAN(angle) ) * ((double)2.0 * radius);
  anglex=(double) cos( (double) RADIAN(angle) ) * ((double)2.0 * radius);

  VSCALE(upvector, heightvector, angley);
  VSCALE(sidevector, widthvector, anglex);
  VADD2(segmentvector, upvector, sidevector);

  angley=(double) sin( (double) RADIAN(angle) ) * segmentlength;
  VSCALE(moveupvector, heightvector, ((double)2.0 * angley));

  VMOVE(position, moveupvector);


  if (debug) fprintf(DEFAULT_DEBUG_OUTPUT, "createWire:segmentvector[%f][%f][%f]\n", segmentvector[0], segmentvector[1], segmentvector[2]);
  if (debug) fprintf(DEFAULT_DEBUG_OUTPUT, "createWire:position[%f][%f][%f]\n", position[0], position[1], position[2]);

  if ((mk_rcc(fp, getName(segmentName, count, DEFAULT_WIREBASICPARAM), position, segmentvector, radius))==0) {
    if (verbose) fprintf(DEFAULT_VERBOSE_OUTPUT, "...Wire segment [%d] generated\n", count);
  }
  else {
    if (verbose) fprintf(DEFAULT_VERBOSE_OUTPUT, "...Wire segment [%d] generation FAILED\n", count);
    errors++;
  }


  /*extension of third segment going nw*/
  count++;

  VCROSS(movebackvector, heightvector, widthvector);
  VSCALE(movebackvector, movebackvector, (segmentdepthseparation+(2*radius)) );

  VMOVE(position, movebackvector);

  angley=(double) sin( (double) RADIAN((double)180.0-angle) ) * ((double)2.0 * radius);
  anglex=(double) cos( (double) RADIAN((double)180.0-angle) ) * ((double)2.0 * radius);

  VSCALE(upvector, heightvector, -angley);
  VSCALE(sidevector, widthvector, -anglex);
  VADD2(segmentvector, upvector, sidevector);


  if (debug) fprintf(DEFAULT_DEBUG_OUTPUT, "createWire:segmentvector[%f][%f][%f]\n", segmentvector[0], segmentvector[1], segmentvector[2]);
  if (debug) fprintf(DEFAULT_DEBUG_OUTPUT, "createWire:position[%f][%f][%f]\n", position[0], position[1], position[2]);

  if ((mk_rcc(fp, getName(segmentName, count, DEFAULT_WIREBASICPARAM), position, segmentvector, radius))==0) {
    if (verbose) fprintf(DEFAULT_VERBOSE_OUTPUT, "...Wire segment [%d] generated\n", count);
  }
  else {
    if (verbose) fprintf(DEFAULT_VERBOSE_OUTPUT, "...Wire segment [%d] generation FAILED\n", count);
    errors++;
  }


  /*extension of fourth segment going nw*/
  count++;

  angley=(double) sin( (double) RADIAN(angle) ) * segmentlength;

  VSCALE(moveupvector, heightvector, ((double)2.0 * angley));

  VMOVE(position, moveupvector);

  VCROSS(movebackvector, heightvector, widthvector);
  VSCALE(movebackvector, movebackvector, (segmentdepthseparation+(2*radius)) );

  VADD2(position, position, movebackvector);

  angley=(double) sin( (double) RADIAN((double)180.0-angle) ) * ((double)2.0 * radius);
  anglex=(double) cos( (double) RADIAN((double)180.0-angle) ) * ((double)2.0 * radius);

  VSCALE(upvector, heightvector, angley);
  VSCALE(sidevector, widthvector, anglex);
  VADD2(segmentvector, upvector, sidevector);


  if (debug) fprintf(DEFAULT_DEBUG_OUTPUT, "createWire:segmentvector[%f][%f][%f]\n", segmentvector[0], segmentvector[1], segmentvector[2]);
  if (debug) fprintf(DEFAULT_DEBUG_OUTPUT, "createWire:position[%f][%f][%f]\n", position[0], position[1], position[2]);

  if ((mk_rcc(fp, getName(segmentName, count, DEFAULT_WIREBASICPARAM), position, segmentvector, radius))==0) {
    if (verbose) fprintf(DEFAULT_VERBOSE_OUTPUT, "...Wire segment [%d] generated\n", count);
  }
  else {
    if (verbose) fprintf(DEFAULT_VERBOSE_OUTPUT, "...Wire segment [%d] generation FAILED\n", count);
    errors++;
  }


  /*first ball joint segment*/
  count++;

  angley=(double) sin( (double) RADIAN(angle) ) * segmentlength;
  anglex=(double) cos( (double) RADIAN(angle) ) * segmentlength;

  VSCALE(moveupvector, heightvector, angley);
  VSCALE(movesidevector, widthvector, anglex);
  VADD2(position, moveupvector, movesidevector);

  if (debug) fprintf(DEFAULT_DEBUG_OUTPUT, "createWire:segmentvector[%f][%f][%f]\n", segmentvector[0], segmentvector[1], segmentvector[2]);
  if (debug) fprintf(DEFAULT_DEBUG_OUTPUT, "createWire:position[%f][%f][%f]\n", position[0], position[1], position[2]);

  if ((mk_sph(fp, getName(segmentName, count, DEFAULT_WIREBASICPARAM), position, radius))==0) {
    if (verbose) fprintf(DEFAULT_VERBOSE_OUTPUT, "...Wire segment [%d] generated\n", count);
  }
  else {
    if (verbose) fprintf(DEFAULT_VERBOSE_OUTPUT, "...Wire segment [%d] generation FAILED\n", count);
    errors++;
  }


  /*second ball joint segment*/
  count++;

  angley=(double) sin( (double) RADIAN(angle) ) * segmentlength;
  anglex=(double) cos( (double) RADIAN(angle) ) * segmentlength;

  VSCALE(moveupvector, heightvector, angley);
  VSCALE(movesidevector, widthvector, anglex);
  VSUB2(position, moveupvector, movesidevector);


  if (debug) fprintf(DEFAULT_DEBUG_OUTPUT, "createWire:segmentvector[%f][%f][%f]\n", segmentvector[0], segmentvector[1], segmentvector[2]);
  if (debug) fprintf(DEFAULT_DEBUG_OUTPUT, "createWire:position[%f][%f][%f]\n", position[0], position[1], position[2]);

  if ((mk_sph(fp, getName(segmentName, count, DEFAULT_WIREBASICPARAM), position, radius))==0) {
    if (verbose) fprintf(DEFAULT_VERBOSE_OUTPUT, "...Wire segment [%d] generated\n", count);
  }
  else {
    if (verbose) fprintf(DEFAULT_VERBOSE_OUTPUT, "...Wire segment [%d] generation FAILED\n", count);
    errors++;
  }


  /*third ball joint segment*/
  count++;

  angley=(double) sin( (double) RADIAN(angle) ) * segmentlength;
  anglex=(double) cos( (double) RADIAN(angle) ) * segmentlength;

  VSCALE(moveupvector, heightvector, angley);
  VSCALE(movesidevector, widthvector, anglex);
  VADD2(position, moveupvector, movesidevector);

  VCROSS(movebackvector, heightvector, widthvector);
  VSCALE(movebackvector, movebackvector, (segmentdepthseparation+((double)2.0*radius)) );

  VADD2(position, position, movebackvector);


  if (debug) fprintf(DEFAULT_DEBUG_OUTPUT, "createWire:segmentvector[%f][%f][%f]\n", segmentvector[0], segmentvector[1], segmentvector[2]);
  if (debug) fprintf(DEFAULT_DEBUG_OUTPUT, "createWire:position[%f][%f][%f]\n", position[0], position[1], position[2]);

  if ((mk_sph(fp, getName(segmentName, count, DEFAULT_WIREBASICPARAM), position, radius))==0) {
    if (verbose) fprintf(DEFAULT_VERBOSE_OUTPUT, "...Wire segment [%d] generated\n", count);
  }
  else {
    if (verbose) fprintf(DEFAULT_VERBOSE_OUTPUT, "...Wire segment [%d] generation FAILED\n", count);
    errors++;
  }


  /*fourth ball joint segment */
  count++;

  angley=(double) sin( (double) RADIAN(angle) ) * segmentlength;
  anglex=(double) cos( (double) RADIAN(angle) ) * segmentlength;

  VSCALE(moveupvector, heightvector, angley);
  VSCALE(movesidevector, widthvector, anglex);
  VSUB2(position, moveupvector, movesidevector);

  VCROSS(movebackvector, heightvector, widthvector);
  VSCALE(movebackvector, movebackvector, (segmentdepthseparation+(2*radius)) );

  VADD2(position, position, movebackvector);


  if (debug) fprintf(DEFAULT_DEBUG_OUTPUT, "createWire:segmentvector[%f][%f][%f]\n", segmentvector[0], segmentvector[1], segmentvector[2]);
  if (debug) fprintf(DEFAULT_DEBUG_OUTPUT, "createWire:position[%f][%f][%f]\n", position[0], position[1], position[2]);

  if ((mk_sph(fp, getName(segmentName, count, DEFAULT_WIREBASICPARAM), position, radius))==0) {
    if (verbose) fprintf(DEFAULT_VERBOSE_OUTPUT, "...Wire segment [%d] generated\n", count);
  }
  else {
    if (verbose) fprintf(DEFAULT_VERBOSE_OUTPUT, "...Wire segment [%d] generation FAILED\n", count);
    errors++;
  }


  /*first segment going back*/
  count++;

  angley=(double) sin( (double) RADIAN(angle) ) * segmentlength;
  anglex=(double) cos( (double) RADIAN(angle) ) * segmentlength;

  VSCALE(movesidevector, widthvector, (anglex));
  VSCALE(moveupvector, heightvector, (angley));

  VADD2(position, moveupvector, movesidevector);

  VCROSS(movebackvector, heightvector, widthvector);
  VSCALE(movebackvector, movebackvector, (segmentdepthseparation+(2*radius)) );

  VMOVE(segmentvector, movebackvector);


  if (debug) fprintf(DEFAULT_DEBUG_OUTPUT, "createWire:segmentvector[%f][%f][%f]\n", segmentvector[0], segmentvector[1], segmentvector[2]);
  if (debug) fprintf(DEFAULT_DEBUG_OUTPUT, "createWire:position[%f][%f][%f]\n", position[0], position[1], position[2]);

  if ((mk_rcc(fp, getName(segmentName, count, DEFAULT_WIREBASICPARAM), position, segmentvector, radius))==0) {
    if (verbose) fprintf(DEFAULT_VERBOSE_OUTPUT, "...Wire segment [%d] generated\n", count);
  }
  else {
    if (verbose) fprintf(DEFAULT_VERBOSE_OUTPUT, "...Wire segment [%d] generation FAILED\n", count);
    errors++;
  }


  /*second segment going back*/
  count++;

  angley=(double) sin( (double) RADIAN((double)180.0-angle) )*segmentlength;
  anglex=(double) cos( (double) RADIAN((double)180.0-angle) )*segmentlength;

  VSCALE(moveupvector, heightvector, angley);
  VSCALE(movesidevector, widthvector, anglex);
  VADD2(position, moveupvector, movesidevector);

  VCROSS(movebackvector, heightvector, widthvector);
  VSCALE(movebackvector, movebackvector, (segmentdepthseparation+(2*radius)) );

  VMOVE(segmentvector, movebackvector);


  if (debug) fprintf(DEFAULT_DEBUG_OUTPUT, "createWire:segmentvector[%f][%f][%f]\n", segmentvector[0], segmentvector[1], segmentvector[2]);
  if (debug) fprintf(DEFAULT_DEBUG_OUTPUT, "createWire:position[%f][%f][%f]\n", position[0], position[1], position[2]);

  if ((mk_rcc(fp, getName(segmentName, count, DEFAULT_WIREBASICPARAM), position, segmentvector, radius))==0) {
    if (verbose) fprintf(DEFAULT_VERBOSE_OUTPUT, "...Wire segment [%d] generated\n", count);
  }
  else {
    if (verbose) fprintf(DEFAULT_VERBOSE_OUTPUT, "...Wire segment [%d] generation FAILED\n", count);
    errors++;
  }


  /*third segment going back*/
  count++;

  angley=(double) sin( (double) RADIAN(angle) ) * segmentlength;

  VSCALE(moveupvector, heightvector, ((double)2.0 * angley));
  VMOVE(position, moveupvector);

  angley=(double) sin( (double) RADIAN(angle) ) * ((double)2.0 * radius);
  anglex=(double) cos( (double) RADIAN(angle) ) * ((double)2.0 * radius);

  VSCALE(upvector, heightvector, angley);
  VSCALE(sidevector, widthvector, anglex);
  VADD2(segmentvector, upvector, sidevector);

  VADD2(position, position, segmentvector);

  VCROSS(movebackvector, heightvector, widthvector);
  VSCALE(movebackvector, movebackvector, (segmentdepthseparation+((double)2.0*radius)) );

  VMOVE(segmentvector, movebackvector);


  if (debug) fprintf(DEFAULT_DEBUG_OUTPUT, "createWire:segmentvector[%f][%f][%f]\n", segmentvector[0], segmentvector[1], segmentvector[2]);
  if (debug) fprintf(DEFAULT_DEBUG_OUTPUT, "createWire:position[%f][%f][%f]\n", position[0], position[1], position[2]);

  if ((mk_rcc(fp, getName(segmentName, count, DEFAULT_WIREBASICPARAM), position, segmentvector, radius))==0) {
    if (verbose) fprintf(DEFAULT_VERBOSE_OUTPUT, "...Wire segment [%d] generated\n", count);
  }
  else {
    if (verbose) fprintf(DEFAULT_VERBOSE_OUTPUT, "...Wire segment [%d] generation FAILED\n", count);
    errors++;
  }


  /*fourth segment going back*/
  count++;

  angley=(double) sin( (double) RADIAN((double)180.0-angle) ) * segmentlength;

  VSCALE(moveupvector, heightvector, ((double)2.0 * angley));
  VMOVE(position, moveupvector);

  angley=(double) sin( (double) RADIAN(angle) ) * ((double)2.0 * radius);
  anglex=(double) cos( (double) RADIAN(angle) ) * ((double)2.0 * radius);

  VSCALE(upvector, heightvector, angley);
  VSCALE(sidevector, widthvector, -anglex);
  VADD2(segmentvector, upvector, sidevector);

  VADD2(position, position, segmentvector);

  VCROSS(movebackvector, heightvector, widthvector);
  VSCALE(movebackvector, movebackvector, (segmentdepthseparation+(2*radius)) );

  VMOVE(segmentvector, movebackvector);


  if (debug) fprintf(DEFAULT_DEBUG_OUTPUT, "createWire:segmentvector[%f][%f][%f]\n", segmentvector[0], segmentvector[1], segmentvector[2]);
  if (debug) fprintf(DEFAULT_DEBUG_OUTPUT, "createWire:position[%f][%f][%f]\n", position[0], position[1], position[2]);

  if ((mk_rcc(fp, getName(segmentName, count, DEFAULT_WIREBASICPARAM), position, segmentvector, radius))==0) {
    if (verbose) fprintf(DEFAULT_VERBOSE_OUTPUT, "...Wire segment [%d] generated\n", count);
  }
  else {
    if (verbose) fprintf(DEFAULT_VERBOSE_OUTPUT, "...Wire segment [%d] generation FAILED\n", count);
    errors++;
  }


  /*first end ball joint segment*/
  count++;

  angley=(double) sin( (double) RADIAN(angle) ) * ((double)2.0 * radius);
  anglex=(double) cos( (double) RADIAN(angle) ) * ((double)2.0 * radius);

  VSCALE(moveupvector, heightvector, -angley);
  VSCALE(movesidevector, widthvector, -anglex);

  VADD2(position, moveupvector, movesidevector);


  if (debug) fprintf(DEFAULT_DEBUG_OUTPUT, "createWire:segmentvector[%f][%f][%f]\n", segmentvector[0], segmentvector[1], segmentvector[2]);
  if (debug) fprintf(DEFAULT_DEBUG_OUTPUT, "createWire:position[%f][%f][%f]\n", position[0], position[1], position[2]);

  if ((mk_sph(fp, getName(segmentName, count, DEFAULT_WIREBASICPARAM), position, radius))==0) {
    if (verbose) fprintf(DEFAULT_VERBOSE_OUTPUT, "...Wire segment [%d] generated\n", count);
  }
  else {
    if (verbose) fprintf(DEFAULT_VERBOSE_OUTPUT, "...Wire segment [%d] generation FAILED\n", count);
    errors++;
  }


  /*second end ball joint segment*/
  count++;

  angley=(double) sin( (double) RADIAN(angle) ) * ((double)2.0 * radius);
  anglex=(double) cos( (double) RADIAN(angle) ) * ((double)2.0 * radius);

  VSCALE(moveupvector, heightvector, -angley);
  VSCALE(movesidevector, widthvector, anglex);

  VADD2(position, moveupvector, movesidevector);

  VCROSS(movebackvector, heightvector, widthvector);
  VSCALE(movebackvector, movebackvector, (segmentdepthseparation+((double)2.0*radius)) );

  VADD2(position, position, movebackvector);


  if (debug) fprintf(DEFAULT_DEBUG_OUTPUT, "createWire:segmentvector[%f][%f][%f]\n", segmentvector[0], segmentvector[1], segmentvector[2]);
  if (debug) fprintf(DEFAULT_DEBUG_OUTPUT, "createWire:position[%f][%f][%f]\n", position[0], position[1], position[2]);

  if ((mk_sph(fp, getName(segmentName, count, DEFAULT_WIREBASICPARAM), position, radius))==0) {
    if (verbose) fprintf(DEFAULT_VERBOSE_OUTPUT, "...Wire segment [%d] generated\n", count);
  }
  else {
    if (verbose) fprintf(DEFAULT_VERBOSE_OUTPUT, "...Wire segment [%d] generation FAILED\n", count);
    errors++;
  }


  /*third end ball joint segment*/
  count++;

  angley=(double) sin( (double) RADIAN(angle) ) * ((double)2.0 * radius);
  anglex=(double) cos( (double) RADIAN(angle) ) * ((double)2.0 * radius);

  VSCALE(moveupvector, heightvector, angley);
  VSCALE(movesidevector, widthvector, anglex);

  VADD2(position, moveupvector, movesidevector);

  angley=(double) sin( (double) RADIAN(angle) ) * segmentlength;
  VSCALE(moveupvector, heightvector, ((double)2.0 * angley));
  VADD2(position, position, moveupvector);


  if (debug) fprintf(DEFAULT_DEBUG_OUTPUT, "createWire:segmentvector[%f][%f][%f]\n", segmentvector[0], segmentvector[1], segmentvector[2]);
  if (debug) fprintf(DEFAULT_DEBUG_OUTPUT, "createWire:position[%f][%f][%f]\n", position[0], position[1], position[2]);

  if ((mk_sph(fp, getName(segmentName, count, DEFAULT_WIREBASICPARAM), position, radius))==0) {
    if (verbose) fprintf(DEFAULT_VERBOSE_OUTPUT, "...Wire segment [%d] generated\n", count);
  }
  else {
    if (verbose) fprintf(DEFAULT_VERBOSE_OUTPUT, "...Wire segment [%d] generation FAILED\n", count);
    errors++;
  }


  /*fourth end ball joint segment*/
  count++;

  angley=(double) sin( (double) RADIAN(angle) ) * ((double)2.0 * radius);
  anglex=(double) cos( (double) RADIAN(angle) ) * ((double)2.0 * radius);

  VSCALE(moveupvector, heightvector, angley);
  VSCALE(movesidevector, widthvector, -anglex);

  VADD2(position, moveupvector, movesidevector);

  VCROSS(movebackvector, heightvector, widthvector);
  VSCALE(movebackvector, movebackvector, (segmentdepthseparation+((double)2.0*radius)) );

  VADD2(position, position, movebackvector);

  angley=(double) sin( (double) RADIAN(angle) ) * segmentlength;
  VSCALE(moveupvector, heightvector, ((double)2.0 * angley));
  VADD2(position, position, moveupvector);


  if (debug) fprintf(DEFAULT_DEBUG_OUTPUT, "createWire:segmentvector[%f][%f][%f]\n", segmentvector[0], segmentvector[1], segmentvector[2]);
  if (debug) fprintf(DEFAULT_DEBUG_OUTPUT, "createWire:position[%f][%f][%f]\n", position[0], position[1], position[2]);

  if ((mk_sph(fp, getName(segmentName, count, DEFAULT_WIREBASICPARAM), position, radius))==0) {
    if (verbose) fprintf(DEFAULT_VERBOSE_OUTPUT, "...Wire segment [%d] generated\n", count);
  }
  else {
    if (verbose) fprintf(DEFAULT_VERBOSE_OUTPUT, "...Wire segment [%d] generation FAILED\n", count);
    errors++;
  }


  return errors;
}


/* main() is basically just a simple interface to the program.  The interface changes the
 * defaults according to what the user has specified and offers the opportunity to enter
 * data in an interactive mode
 *******************/
int main(int argc, char **argv)
{
  struct rt_wdb *fp;
  int errors;

  int len = 0;
  char *verboseinput;
  int colorinput[3];

  verboseinput = (char *) calloc(DEFAULT_MAXNAMELENGTH * 3, sizeof(char));

  (void) parseArguments(argc, argv);

  if ((fp=wdb_fopen(outputFilename))==NULL) {
    perror(outputFilename);
    exit(2);
  }
  else {
    if (verbose) fprintf(DEFAULT_VERBOSE_OUTPUT, "\nUsing [%s] for output file\n\n", outputFilename);
  }

  if (verbose) fprintf(DEFAULT_VERBOSE_OUTPUT, "Verbose mode is on\n");
  if (debug) fprintf(DEFAULT_DEBUG_OUTPUT, "Debug mode is on\n");
  if (verbose && interactive) fprintf(DEFAULT_VERBOSE_OUTPUT, "Interactive mode is on\n");


  if (verbose) {
    fprintf(DEFAULT_VERBOSE_OUTPUT, "\nCurrent Fence Properties:\n");
    fprintf(DEFAULT_VERBOSE_OUTPUT, "\tHeight[%f] \n\tWidth[%f]\n\n", MAGNITUDE(fenceHeight), MAGNITUDE(fenceWidth));
    fprintf(DEFAULT_VERBOSE_OUTPUT, "\tPole:\n");
    fprintf(DEFAULT_VERBOSE_OUTPUT, "\t\tHeight[%f] \n\t\tRadius[%f] \n\t\tSpacing[%f] \n\n", poleHeight, poleRadius, fencePoleSpacing);
    fprintf(DEFAULT_VERBOSE_OUTPUT, "\tMesh:\n");
    fprintf(DEFAULT_VERBOSE_OUTPUT, "\t\tHeight[%f] \n\t\tWidth[%f] \n\n", meshHeight, meshWidth);
    fprintf(DEFAULT_VERBOSE_OUTPUT, "\t\tWire:\n");
    fprintf(DEFAULT_VERBOSE_OUTPUT, "\t\t\tWireRadius[%f] \n\t\t\tWireAngle[%f] \n\n", wireRadius, wireAngle);
    fprintf(DEFAULT_VERBOSE_OUTPUT, "Combination Names: \n");
    fprintf(DEFAULT_VERBOSE_OUTPUT, "\tFence: [%s] \n\tPoles: [%s] \n\tMesh: [%s] \n\tWires: [%s] \n\tSegments: [%s] \n\n", fenceName, poleName, meshName, wireName, segmentName);
    putc((int)'\n', DEFAULT_VERBOSE_OUTPUT);
  }


  if (interactive) {
    if (fenceFence) {
      if (verbose) fprintf(DEFAULT_VERBOSE_OUTPUT, "\nFence Name: [%s] ", fenceName);
      if ( ! fgets(verboseinput, DEFAULT_MAXNAMELENGTH, stdin) ) {
	if (verbose) fprintf(DEFAULT_VERBOSE_OUTPUT, "Error reading input for %s encountered.\nContinuing with default values.\n", "fenceName");
	if (debug) fprintf(DEFAULT_DEBUG_OUTPUT, "main:fgets error reading %s from stdin\n", "fenceName");
      }
      else {
	len = strlen(verboseinput);
	if ((len>0) && (verboseinput[len-1]=='\n')) verboseinput[len-1] = 0;
	if (debug) {
	  fprintf(DEFAULT_DEBUG_OUTPUT,"main:entered [%s] for fence name\n", verboseinput);
	}
	if (strcmp(verboseinput, "") != 0) {
	  strncpy(fenceName, verboseinput, DEFAULT_MAXNAMELENGTH);
	}
      }
      memset(verboseinput, 0, DEFAULT_MAXNAMELENGTH*3);

      if (verbose) fprintf(DEFAULT_VERBOSE_OUTPUT, "Start Position(x,y,z): [%f %f %f] ", fenceStartPosition[0], fenceStartPosition[1], fenceStartPosition[2]);
      if ( ! fgets(verboseinput, DEFAULT_MAXNAMELENGTH * 3, stdin) ) {
	if (verbose) fprintf(DEFAULT_VERBOSE_OUTPUT, "Error reading input for %s encountered.\nContinuing with default values.\n", "fenceStartPosition");
	if (debug) fprintf(DEFAULT_DEBUG_OUTPUT, "main:fgets error reading %s from stdin\n", "fenceStartPosition");
      }
      else {
	len = strlen(verboseinput);
	if ((len>0) && (verboseinput[len-1]=='\n')) verboseinput[len-1] = 0;
	if (debug) {
	  fprintf(DEFAULT_DEBUG_OUTPUT,"main:entered [%s] for start position\n", verboseinput);
	}
	if (strcmp(verboseinput, "") != 0) {
	  sscanf(verboseinput, "%lf%lf%lf", &fenceStartPosition[0], &fenceStartPosition[1], &fenceStartPosition[2]);
	}
      }
      memset(verboseinput, 0, DEFAULT_MAXNAMELENGTH*3);

      if (verbose) fprintf(DEFAULT_VERBOSE_OUTPUT, "End Position(x,y,z): [%f %f %f] ", fenceEndPosition[0], fenceEndPosition[1], fenceEndPosition[2]);
      if ( ! fgets(verboseinput, DEFAULT_MAXNAMELENGTH*3, stdin) ) {
	if (verbose) fprintf(DEFAULT_VERBOSE_OUTPUT, "Error reading input for %s encountered.\nContinuing with default values.\n", "fenceEndPosition");
	if (debug) fprintf(DEFAULT_DEBUG_OUTPUT, "main:fgets error reading %s from stdin\n", "fenceEndPosition");
      }
      else {
	len = strlen(verboseinput);
	if ((len>0) && (verboseinput[len-1]=='\n')) verboseinput[len-1] = 0;
	if (debug) {
	  fprintf(DEFAULT_DEBUG_OUTPUT,"main:entered [%s] for end position\n", verboseinput);
	}
	if (strcmp(verboseinput, "") != 0) {
	  sscanf(verboseinput, "%lf%lf%lf", &fenceEndPosition[0], &fenceEndPosition[1], &fenceEndPosition[2]);
	}
      }
      memset(verboseinput, 0, DEFAULT_MAXNAMELENGTH*3);

      if (verbose) fprintf(DEFAULT_VERBOSE_OUTPUT, "Fence Height(i,j,k): [%f %f %f] ", fenceHeight[0], fenceHeight[1], fenceHeight[2]);
      if ( ! fgets(verboseinput, DEFAULT_MAXNAMELENGTH*3, stdin) ) {
	if (verbose) fprintf(DEFAULT_VERBOSE_OUTPUT, "Error reading input for %s encountered.\nContinuing with default values.\n", "fenceHeight");
	if (debug) fprintf(DEFAULT_DEBUG_OUTPUT, "main:fgets error reading %s from stdin\n", "fenceHeight");
      }
      else {
	len = strlen(verboseinput);
	if ((len>0) && (verboseinput[len-1]=='\n')) verboseinput[len-1] = 0;
	if (debug) {
	  fprintf(DEFAULT_DEBUG_OUTPUT,"main:entered [%s] for fence height\n", verboseinput);
	}
	if (strcmp(verboseinput, "") != 0) {
	  sscanf(verboseinput, "%lf%lf%lf", &fenceHeight[0], &fenceHeight[1], &fenceHeight[2]);
	}
      }
      memset(verboseinput, 0, DEFAULT_MAXNAMELENGTH*3);

      VSUB2(fenceWidth, fenceEndPosition, fenceStartPosition);
      poleHeight=MAGNITUDE(fenceHeight);
      meshHeight=MAGNITUDE(fenceHeight);
      meshWidth=MAGNITUDE(fenceWidth);
      if (debug) {
	fprintf(DEFAULT_DEBUG_OUTPUT, "main:calculated fenceWidth[(%f)(%f)(%f)]\n", fenceWidth[0], fenceWidth[1], fenceWidth[2]);
	fprintf(DEFAULT_DEBUG_OUTPUT, "main:calculated poleHeight[(%f)]\n", poleHeight);
	fprintf(DEFAULT_DEBUG_OUTPUT, "main:calculated meshHeight[(%f)]\n", meshHeight);
	fprintf(DEFAULT_DEBUG_OUTPUT, "main:calculated meshWidth[(%f)]\n", meshWidth);
      }

      if (verbose) fprintf(DEFAULT_VERBOSE_OUTPUT, "Fence Material: [%s] ", fenceMaterial);
      if ( ! fgets(verboseinput, DEFAULT_MAXNAMELENGTH*3, stdin) ) {
	if (verbose) fprintf(DEFAULT_VERBOSE_OUTPUT, "Error reading input for %s encountered.\nContinuing with default values.\n", "fenceMaterial");
	if (debug) fprintf(DEFAULT_DEBUG_OUTPUT, "main:fgets error reading %s from stdin\n", "fenceMaterial");
      }
      else {
	len = strlen(verboseinput);
	if ((len>0) && (verboseinput[len-1]=='\n')) verboseinput[len-1] = 0;
	if (debug) {
	  fprintf(DEFAULT_DEBUG_OUTPUT,"main:entered [%s] for fence material\n", verboseinput);
	}
	if(strcmp(verboseinput, "") != 0) {
	  strncpy(fenceMaterial, verboseinput, DEFAULT_MAXNAMELENGTH*3);
	}
      }
      memset(verboseinput, 0, DEFAULT_MAXNAMELENGTH*3);

      if (verbose) fprintf(DEFAULT_VERBOSE_OUTPUT, "Fence Material Parameters: [%s] ", fenceMaterialParams);
      if ( ! fgets(verboseinput, DEFAULT_MAXNAMELENGTH*3, stdin) ) {
	if (verbose) fprintf(DEFAULT_VERBOSE_OUTPUT, "Error reading input for %s encountered.\nContinuing with default values.\n", "fenceMaterialParams");
	if (debug) fprintf(DEFAULT_DEBUG_OUTPUT, "main:fgets error reading %s from stdin\n", "fenceMaterialParams");
      }
      else {
	len = strlen(verboseinput);
	if ((len>0) && (verboseinput[len-1]=='\n')) verboseinput[len-1] = 0;
	if (debug) {
	  fprintf(DEFAULT_DEBUG_OUTPUT,"main:entered [%s] for fence material params\n", verboseinput);
	}
	if(strcmp(verboseinput, "") != 0) {
	  strncpy(fenceMaterialParams, verboseinput, DEFAULT_MAXNAMELENGTH*3);
	}
      }
      memset(verboseinput, 0, DEFAULT_MAXNAMELENGTH*3);

      if (verbose) fprintf(DEFAULT_VERBOSE_OUTPUT, "Fence Material Color: [%d %d %d] ", fenceMaterialColor[0], fenceMaterialColor[1], fenceMaterialColor[2]);
      if ( ! fgets(verboseinput, DEFAULT_MAXNAMELENGTH*3, stdin) ) {
	if (verbose) fprintf(DEFAULT_VERBOSE_OUTPUT, "Error reading input for %s encountered.\nContinuing with default values.\n", "fenceMaterialColor");
	if (debug) fprintf(DEFAULT_DEBUG_OUTPUT, "main:fgets error reading %s from stdin\n", "fenceMaterialColor");
      }
      else {
	len = strlen(verboseinput);
	if ((len>0) && (verboseinput[len-1]=='\n')) verboseinput[len-1] = 0;
	if (debug) {
	  fprintf(DEFAULT_DEBUG_OUTPUT,"main:entered [%s] for fence material color\n", verboseinput);
	}
	if(strcmp(verboseinput, "") != 0) {
	  sscanf(verboseinput, "%d%d%d", &colorinput[0], &colorinput[1], &colorinput[2]);

	  if ((colorinput[0]<0)|(colorinput[1]<0)|(colorinput[2]<0)) {
	    if (verbose) fprintf(DEFAULT_VERBOSE_OUTPUT, "Clamping color value to 0\nOnly numbers between 0 and 255 inclusive are valid color values\n");
	    if (colorinput[0]<0) colorinput[0] = 0;
	  if (colorinput[1]<0) colorinput[1] = 0;
	  if (colorinput[2]<0) colorinput[2] = 0;
	  }
	  if ((colorinput[0]>255)|(colorinput[1]>255)|(colorinput[2]>255)) {
	    if (verbose) fprintf(DEFAULT_VERBOSE_OUTPUT, "Clamping color value to 255\nOnly numbers between 0 and 255 inclusive are valid color values\n");
	    if (colorinput[0]>255) colorinput[0] = 255;
	    if (colorinput[1]>255) colorinput[1] = 255;
	    if (colorinput[2]>255) colorinput[2] = 255;
	  }
	  fenceMaterialColor[0] = (unsigned char)colorinput[0];
	  fenceMaterialColor[1] = (unsigned char)colorinput[1];
	  fenceMaterialColor[2] = (unsigned char)colorinput[2];
	}
      }
      memset(verboseinput, 0, DEFAULT_MAXNAMELENGTH*3);

    }

    if (fencePoles) {
      if (verbose) fprintf(DEFAULT_VERBOSE_OUTPUT, "\nPole Name: [%s] ", poleName);
      if ( ! fgets(verboseinput, DEFAULT_MAXNAMELENGTH, stdin) ) {
	if (verbose) fprintf(DEFAULT_VERBOSE_OUTPUT, "Error reading input for %s encountered.\nContinuing with default values.\n", "poleName");
	if (debug) fprintf(DEFAULT_DEBUG_OUTPUT, "main:fgets error reading %s from stdin\n", "poleName");
      }
      else {
	len = strlen(verboseinput);
	if ((len>0) && (verboseinput[len-1]=='\n')) verboseinput[len-1] = 0;
	if (debug) {
	  fprintf(DEFAULT_DEBUG_OUTPUT,"main:entered [%s] for pole name\n", verboseinput);
	}
	if (strcmp(verboseinput, "") != 0) {
	  strncpy(poleName, verboseinput, DEFAULT_MAXNAMELENGTH);
	}
      }
      memset(verboseinput, 0, DEFAULT_MAXNAMELENGTH*3);

      if (verbose) fprintf(DEFAULT_VERBOSE_OUTPUT, "Pole Height: [%f] ", poleHeight);
      if ( ! fgets(verboseinput, DEFAULT_MAXNAMELENGTH, stdin) ) {
	if (verbose) fprintf(DEFAULT_VERBOSE_OUTPUT, "Error reading input for %s encountered.\nContinuing with default values.\n", "poleHeight");
	if (debug) fprintf(DEFAULT_DEBUG_OUTPUT, "main:fgets error reading %s from stdin\n", "poleHeight");
      }
      else {
	len = strlen(verboseinput);
	if ((len>0) && (verboseinput[len-1]=='\n')) verboseinput[len-1] = 0;
	if (debug) {
	  fprintf(DEFAULT_DEBUG_OUTPUT,"main:entered [%s] for pole height\n", verboseinput);
	}
	if (strcmp(verboseinput, "") != 0) {
	  sscanf(verboseinput, "%lf", &poleHeight);
	}
      }
      memset(verboseinput, 0, DEFAULT_MAXNAMELENGTH*3);

      if (verbose) fprintf(DEFAULT_VERBOSE_OUTPUT, "Pole Radius: [%f] ", poleRadius);
      if ( ! fgets(verboseinput, DEFAULT_MAXNAMELENGTH, stdin) ) {
	if (verbose) fprintf(DEFAULT_VERBOSE_OUTPUT, "Error reading input for %s encountered.\nContinuing with default values.\n", "poleRadius");
	if (debug) fprintf(DEFAULT_DEBUG_OUTPUT, "main:fgets error reading %s from stdin\n", "poleRadius");
      }
      else {
	len = strlen(verboseinput);
	if ((len>0) && (verboseinput[len-1]=='\n')) verboseinput[len-1] = 0;
	if (debug) {
	  fprintf(DEFAULT_DEBUG_OUTPUT,"main:entered [%s] for pole radius\n", verboseinput);
	}
	if (strcmp(verboseinput, "") != 0) {
	  sscanf(verboseinput, "%lf", &poleRadius);
	}
      }
      memset(verboseinput, 0, DEFAULT_MAXNAMELENGTH*3);

      if (verbose) fprintf(DEFAULT_VERBOSE_OUTPUT, "Pole Spacing: [%f] ", fencePoleSpacing);
      if ( ! fgets(verboseinput, DEFAULT_MAXNAMELENGTH, stdin) ) {
	if (verbose) fprintf(DEFAULT_VERBOSE_OUTPUT, "Error reading input for %s encountered.\nContinuing with default values.\n", "fencePoleSpacing");
	if (debug) fprintf(DEFAULT_DEBUG_OUTPUT, "main:fgets error reading %s from stdin\n", "fencePoleSpacing");
      }
      else {
	len = strlen(verboseinput);
	if ((len>0) && (verboseinput[len-1]=='\n')) verboseinput[len-1] = 0;
	if (debug) {
	  fprintf(DEFAULT_DEBUG_OUTPUT,"main:entered [%s] for pole spacing\n", verboseinput);
	}
	if (strcmp(verboseinput, "") != 0) {
	  sscanf(verboseinput, "%lf", &fencePoleSpacing);
	}
      }
      memset(verboseinput, 0, DEFAULT_MAXNAMELENGTH*3);

      if (verbose) fprintf(DEFAULT_VERBOSE_OUTPUT, "Pole Material: [%s] ", poleMaterial);
      if ( ! fgets(verboseinput, DEFAULT_MAXNAMELENGTH*3, stdin) ) {
	if (verbose) fprintf(DEFAULT_VERBOSE_OUTPUT, "Error reading input for %s encountered.\nContinuing with default values.\n", "poleMaterial");
	if (debug) fprintf(DEFAULT_DEBUG_OUTPUT, "main:fgets error reading %s from stdin\n", "poleMaterial");
      }
      else {
	len = strlen(verboseinput);
	if ((len>0) && (verboseinput[len-1]=='\n')) verboseinput[len-1] = 0;
	if (debug) {
	  fprintf(DEFAULT_DEBUG_OUTPUT,"main:entered [%s] for pole material\n", verboseinput);
	}
	if(strcmp(verboseinput, "") != 0) {
	  strncpy(poleMaterial, verboseinput, DEFAULT_MAXNAMELENGTH*3);
	}
      }
      memset(verboseinput, 0, DEFAULT_MAXNAMELENGTH*3);

      if (verbose) fprintf(DEFAULT_VERBOSE_OUTPUT, "Pole Material Parameters: [%s] ", poleMaterialParams);
      if ( ! fgets(verboseinput, DEFAULT_MAXNAMELENGTH*3, stdin) ) {
	if (verbose) fprintf(DEFAULT_VERBOSE_OUTPUT, "Error reading input for %s encountered.\nContinuing with default values.\n", "poleMaterialParams");
	if (debug) fprintf(DEFAULT_DEBUG_OUTPUT, "main:fgets error reading %s from stdin\n", "poleMaterialParams");
      }
      else {
	len = strlen(verboseinput);
	if ((len>0) && (verboseinput[len-1]=='\n')) verboseinput[len-1] = 0;
	if (debug) {
	  fprintf(DEFAULT_DEBUG_OUTPUT,"main:entered [%s] for pole material params\n", verboseinput);
	}
	if(strcmp(verboseinput, "") != 0) {
	  strncpy(poleMaterialParams, verboseinput, DEFAULT_MAXNAMELENGTH*3);
	}
      }
      memset(verboseinput, 0, DEFAULT_MAXNAMELENGTH*3);

      if (verbose) fprintf(DEFAULT_VERBOSE_OUTPUT, "Pole Material Color: [%d %d %d] ", poleMaterialColor[0], poleMaterialColor[1], poleMaterialColor[2]);
      if ( ! fgets(verboseinput, DEFAULT_MAXNAMELENGTH*3, stdin) ) {
	if (verbose) fprintf(DEFAULT_VERBOSE_OUTPUT, "Error reading input for %s encountered.\nContinuing with default values.\n", "poleMaterialColor");
	if (debug) fprintf(DEFAULT_DEBUG_OUTPUT, "main:fgets error reading %s from stdin\n", "poleMaterialColor");
      }
      else {
	len = strlen(verboseinput);
	if ((len>0) && (verboseinput[len-1]=='\n')) verboseinput[len-1] = 0;
	if (debug) {
	  fprintf(DEFAULT_DEBUG_OUTPUT,"main:entered [%s] for pole material color\n", verboseinput);
	}
	if(strcmp(verboseinput, "") != 0) {
	  sscanf(verboseinput, "%d%d%d", &colorinput[0], &colorinput[1], &colorinput[2]);

	  if ((colorinput[0]<0)|(colorinput[1]<0)|(colorinput[2]<0)) {
	    if (verbose) fprintf(DEFAULT_VERBOSE_OUTPUT, "Clamping color value to 0\nOnly numbers between 0 and 255 inclusive are valid color values\n");
	    if (colorinput[0]<0) colorinput[0] = 0;
	    if (colorinput[1]<0) colorinput[1] = 0;
	    if (colorinput[2]<0) colorinput[2] = 0;
	  }
	  if ((colorinput[0]>255)|(colorinput[1]>255)|(colorinput[2]>255)) {
	    if (verbose) fprintf(DEFAULT_VERBOSE_OUTPUT, "Clamping color value to 255\nOnly numbers between 0 and 255 inclusive are valid color values\n");
	    if (colorinput[0]>255) colorinput[0] = 255;
	    if (colorinput[1]>255) colorinput[1] = 255;
	    if (colorinput[2]>255) colorinput[2] = 255;
	  }
	  poleMaterialColor[0] = (unsigned char)colorinput[0];
	  poleMaterialColor[1] = (unsigned char)colorinput[1];
	  poleMaterialColor[2] = (unsigned char)colorinput[2];
	}
      }
      memset(verboseinput, 0, DEFAULT_MAXNAMELENGTH*3);

    }

    if (fenceMesh) {
      if (verbose) fprintf(DEFAULT_VERBOSE_OUTPUT, "\nMesh Name: [%s] ", meshName);
      if ( ! fgets(verboseinput, DEFAULT_MAXNAMELENGTH, stdin) ) {
	if (verbose) fprintf(DEFAULT_VERBOSE_OUTPUT, "Error reading input for %s encountered.\nContinuing with default values.\n", "meshName");
	if (debug) fprintf(DEFAULT_DEBUG_OUTPUT, "main:fgets error reading %s from stdin\n", "meshName");
      }
      else {
	len = strlen(verboseinput);
	if ((len>0) && (verboseinput[len-1]=='\n')) verboseinput[len-1] = 0;
	if (debug) {
	  fprintf(DEFAULT_DEBUG_OUTPUT,"main:entered [%s] for mesh name\n", verboseinput);
	}
	if (strcmp(verboseinput, "") != 0) {
	  strncpy(meshName, verboseinput, DEFAULT_MAXNAMELENGTH);
	}
      }
      memset(verboseinput, 0, DEFAULT_MAXNAMELENGTH*3);

      if (verbose) fprintf(DEFAULT_VERBOSE_OUTPUT, "Mesh Height: [%f] ", meshHeight);
      if ( ! fgets(verboseinput, DEFAULT_MAXNAMELENGTH, stdin) ) {
	if (verbose) fprintf(DEFAULT_VERBOSE_OUTPUT, "Error reading input for %s encountered.\nContinuing with default values.\n", "meshHeight");
	if (debug) fprintf(DEFAULT_DEBUG_OUTPUT, "main:fgets error reading %s from stdin\n", "meshHeight");
      }
      else {
	len = strlen(verboseinput);
	if ((len>0) && (verboseinput[len-1]=='\n')) verboseinput[len-1] = 0;
	if (debug) {
	  fprintf(DEFAULT_DEBUG_OUTPUT,"main:entered [%s] for mesh height\n", verboseinput);
	}
	if (strcmp(verboseinput, "") != 0) {
	  sscanf(verboseinput, "%lf", &meshHeight);
	}
      }
      memset(verboseinput, 0, DEFAULT_MAXNAMELENGTH*3);

      if (verbose) fprintf(DEFAULT_VERBOSE_OUTPUT, "Mesh Width: [%f] ", meshWidth);
      if ( ! fgets(verboseinput, DEFAULT_MAXNAMELENGTH, stdin) ) {
	if (verbose) fprintf(DEFAULT_VERBOSE_OUTPUT, "Error reading input for %s encountered.\nContinuing with default values.\n", "meshWidth");
	if (debug) fprintf(DEFAULT_DEBUG_OUTPUT, "main:fgets error reading %s from stdin\n", "meshWidth");
      }
      else {
	len = strlen(verboseinput);
	if ((len>0) && (verboseinput[len-1]=='\n')) verboseinput[len-1] = 0;
	if (debug) {
	  fprintf(DEFAULT_DEBUG_OUTPUT,"main:entered [%s] for mesh width\n", verboseinput);
	}
	if (strcmp(verboseinput, "") != 0) {
	  sscanf(verboseinput, "%lf", &meshWidth);
	}
      }
      memset(verboseinput, 0, DEFAULT_MAXNAMELENGTH*3);

      if (verbose) fprintf(DEFAULT_VERBOSE_OUTPUT, "Mesh Material: [%s] ", meshMaterial);
      if ( ! fgets(verboseinput, DEFAULT_MAXNAMELENGTH*3, stdin) ) {
	if (verbose) fprintf(DEFAULT_VERBOSE_OUTPUT, "Error reading input for %s encountered.\nContinuing with default values.\n", "meshMaterial");
	if (debug) fprintf(DEFAULT_DEBUG_OUTPUT, "main:fgets error reading %s from stdin\n", "meshMaterial");
      }
      else {
	len = strlen(verboseinput);
	if ((len>0) && (verboseinput[len-1]=='\n')) verboseinput[len-1] = 0;
	if (debug) {
	  fprintf(DEFAULT_DEBUG_OUTPUT,"main:entered [%s] for mesh material\n", verboseinput);
	}
	if(strcmp(verboseinput, "") != 0) {
	  strncpy(meshMaterial, verboseinput, DEFAULT_MAXNAMELENGTH*3);
	}
      }
      memset(verboseinput, 0, DEFAULT_MAXNAMELENGTH*3);

      if (verbose) fprintf(DEFAULT_VERBOSE_OUTPUT, "Mesh Material Parameters: [%s] ", meshMaterialParams);
      if ( ! fgets(verboseinput, DEFAULT_MAXNAMELENGTH*3, stdin) ) {
	if (verbose) fprintf(DEFAULT_VERBOSE_OUTPUT, "Error reading input for %s encountered.\nContinuing with default values.\n", "meshMaterialParams");
	if (debug) fprintf(DEFAULT_DEBUG_OUTPUT, "main:fgets error reading %s from stdin\n", "meshMaterialParams");
      }
      else {
	len = strlen(verboseinput);
	if ((len>0) && (verboseinput[len-1]=='\n')) verboseinput[len-1] = 0;
	if (debug) {
	  fprintf(DEFAULT_DEBUG_OUTPUT,"main:entered [%s] for mesh material params\n", verboseinput);
	}
	if(strcmp(verboseinput, "") != 0) {
	  strncpy(meshMaterialParams, verboseinput, DEFAULT_MAXNAMELENGTH*3);
	}
      }
      memset(verboseinput, 0, DEFAULT_MAXNAMELENGTH*3);

      if (verbose) fprintf(DEFAULT_VERBOSE_OUTPUT, "Mesh Material Color: [%d %d %d] ", meshMaterialColor[0], meshMaterialColor[1], meshMaterialColor[2]);
      if ( ! fgets(verboseinput, DEFAULT_MAXNAMELENGTH*3, stdin) ) {
	if (verbose) fprintf(DEFAULT_VERBOSE_OUTPUT, "Error reading input for %s encountered.\nContinuing with default values.\n", "meshMaterialColor");
	if (debug) fprintf(DEFAULT_DEBUG_OUTPUT, "main:fgets error reading %s from stdin\n", "meshMaterialColor");
      }
      else {
	len = strlen(verboseinput);
	if ((len>0) && (verboseinput[len-1]=='\n')) verboseinput[len-1] = 0;
	if (debug) {
	  fprintf(DEFAULT_DEBUG_OUTPUT,"main:entered [%s] for mesh material color\n", verboseinput);
	}
	if(strcmp(verboseinput, "") != 0) {
	  sscanf(verboseinput, "%d%d%d", &colorinput[0], &colorinput[1], &colorinput[2]);

	  if ((colorinput[0]<0)|(colorinput[1]<0)|(colorinput[2]<0)) {
	    if (verbose) fprintf(DEFAULT_VERBOSE_OUTPUT, "Clamping color value to 0\nOnly numbers between 0 and 255 inclusive are valid color values\n");
	    if (colorinput[0]<0) colorinput[0] = 0;
	    if (colorinput[1]<0) colorinput[1] = 0;
	    if (colorinput[2]<0) colorinput[2] = 0;
	  }
	  if ((colorinput[0]>255)|(colorinput[1]>255)|(colorinput[2]>255)) {
	    if (verbose) fprintf(DEFAULT_VERBOSE_OUTPUT, "Clamping color value to 255\nOnly numbers between 0 and 255 inclusive are valid color values\n");
	    if (colorinput[0]>255) colorinput[0] = 255;
	    if (colorinput[1]>255) colorinput[1] = 255;
	    if (colorinput[2]>255) colorinput[2] = 255;
	  }
	  meshMaterialColor[0] = (unsigned char)colorinput[0];
	  meshMaterialColor[1] = (unsigned char)colorinput[1];
	  meshMaterialColor[2] = (unsigned char)colorinput[2];
	}
      }
      memset(verboseinput, 0, DEFAULT_MAXNAMELENGTH*3);

    }

    if (fenceWire) {
      if (verbose) fprintf(DEFAULT_VERBOSE_OUTPUT, "\nWire Name: [%s] ", wireName);
      if ( ! fgets(verboseinput, DEFAULT_MAXNAMELENGTH, stdin) ) {
	if (verbose) fprintf(DEFAULT_VERBOSE_OUTPUT, "Error reading input for %s encountered.\nContinuing with default values.\n", "wireName");
	if (debug) fprintf(DEFAULT_DEBUG_OUTPUT, "main:fgets error reading %s from stdin\n", "wireName");
      }
      else {
	len = strlen(verboseinput);
	if ((len>0) && (verboseinput[len-1]=='\n')) verboseinput[len-1] = 0;
	if (debug) {
	  fprintf(DEFAULT_DEBUG_OUTPUT,"main:entered [%s] for wire name\n", verboseinput);
	}
	if (strcmp(verboseinput, "") != 0) {
	  strncpy(wireName, verboseinput, DEFAULT_MAXNAMELENGTH);
	}
      }
      memset(verboseinput, 0, DEFAULT_MAXNAMELENGTH*3);

      if (verbose) fprintf(DEFAULT_VERBOSE_OUTPUT, "Wire Radius: [%f] ", wireRadius);
      if ( ! fgets(verboseinput, DEFAULT_MAXNAMELENGTH, stdin) ) {
	if (verbose) fprintf(DEFAULT_VERBOSE_OUTPUT, "Error reading input for %s encountered.\nContinuing with default values.\n", "wireRadius");
	if (debug) fprintf(DEFAULT_DEBUG_OUTPUT, "main:fgets error reading %s from stdin\n", "wireRadius");
      }
      else {
	len = strlen(verboseinput);
	if ((len>0) && (verboseinput[len-1]=='\n')) verboseinput[len-1] = 0;
	if (debug) {
	  fprintf(DEFAULT_DEBUG_OUTPUT,"main:entered [%s] wire radius\n", verboseinput);
	}
	if (strcmp(verboseinput, "") != 0) {
	  sscanf(verboseinput, "%lf", &wireRadius);
	}
      }
      memset(verboseinput, 0, DEFAULT_MAXNAMELENGTH*3);

      if (verbose) fprintf(DEFAULT_VERBOSE_OUTPUT, "Wire Angle: [%f] ", wireAngle);
      if ( ! fgets(verboseinput, DEFAULT_MAXNAMELENGTH, stdin) ) {
	if (verbose) fprintf(DEFAULT_VERBOSE_OUTPUT, "Error reading input for %s encountered.\nContinuing with default values.\n", "wireAngle");
	if (debug) fprintf(DEFAULT_DEBUG_OUTPUT, "main:fgets error reading %s from stdin\n", "wireAngle");
      }
      else {
	len = strlen(verboseinput);
	if ((len>0) && (verboseinput[len-1]=='\n')) verboseinput[len-1] = 0;
	if (debug) {
	  fprintf(DEFAULT_DEBUG_OUTPUT,"main:entered [%s] for wire angle\n", verboseinput);
	}
	if (strcmp(verboseinput, "") != 0) {
	  sscanf(verboseinput, "%lf", &wireAngle);
	}
      }
      memset(verboseinput, 0, DEFAULT_MAXNAMELENGTH*3);

      if (verbose) fprintf(DEFAULT_VERBOSE_OUTPUT, "Wire Material: [%s] ", wireMaterial);
      if ( ! fgets(verboseinput, DEFAULT_MAXNAMELENGTH*3, stdin) ) {
	if (verbose) fprintf(DEFAULT_VERBOSE_OUTPUT, "Error reading input for %s encountered.\nContinuing with default values.\n", "wireMaterial");
	if (debug) fprintf(DEFAULT_DEBUG_OUTPUT, "main:fgets error reading %s from stdin\n", "wireMaterial");
      }
      else {
	len = strlen(verboseinput);
	if ((len>0) && (verboseinput[len-1]=='\n')) verboseinput[len-1] = 0;
	if (debug) {
	  fprintf(DEFAULT_DEBUG_OUTPUT,"main:entered [%s] for wire material\n", verboseinput);
	}
	if(strcmp(verboseinput, "") != 0) {
	  strncpy(wireMaterial, verboseinput, DEFAULT_MAXNAMELENGTH*3);
	}
      }
      memset(verboseinput, 0, DEFAULT_MAXNAMELENGTH*3);

      if (verbose) fprintf(DEFAULT_VERBOSE_OUTPUT, "Wire Material Parameters: [%s] ", wireMaterialParams);
      if ( ! fgets(verboseinput, DEFAULT_MAXNAMELENGTH*3, stdin) ) {
	if (verbose) fprintf(DEFAULT_VERBOSE_OUTPUT, "Error reading input for %s encountered.\nContinuing with default values.\n", "wireMaterialParams");
	if (debug) fprintf(DEFAULT_DEBUG_OUTPUT, "main:fgets error reading %s from stdin\n", "wireMaterialParams");
      }
      else {
	len = strlen(verboseinput);
	if ((len>0) && (verboseinput[len-1]=='\n')) verboseinput[len-1] = 0;
	if (debug) {
	  fprintf(DEFAULT_DEBUG_OUTPUT,"main:entered [%s] for wire material params\n", verboseinput);
	}
	if(strcmp(verboseinput, "") != 0) {
	  strncpy(wireMaterialParams, verboseinput, DEFAULT_MAXNAMELENGTH*3);
	}
      }
      memset(verboseinput, 0, DEFAULT_MAXNAMELENGTH*3);

      if (verbose) fprintf(DEFAULT_VERBOSE_OUTPUT, "Wire Material Color: [%d %d %d] ", wireMaterialColor[0], wireMaterialColor[1], wireMaterialColor[2]);
      if ( ! fgets(verboseinput, DEFAULT_MAXNAMELENGTH*3, stdin) ) {
	if (verbose) fprintf(DEFAULT_VERBOSE_OUTPUT, "Error reading input for %s encountered.\nContinuing with default values.\n", "wireMaterialColor");
	if (debug) fprintf(DEFAULT_DEBUG_OUTPUT, "main:fgets error reading %s from stdin\n", "wireMaterialColor");
      }
      else {
	len = strlen(verboseinput);
	if ((len>0) && (verboseinput[len-1]=='\n')) verboseinput[len-1] = 0;
	if (debug) {
	  fprintf(DEFAULT_DEBUG_OUTPUT,"main:entered [%s] for wire material color\n", verboseinput);
	}
	if(strcmp(verboseinput, "") != 0) {
	  sscanf(verboseinput, "%d%d%d", &colorinput[0], &colorinput[1], &colorinput[2]);

	  if ((colorinput[0]<0)|(colorinput[1]<0)|(colorinput[2]<0)) {
	    if (verbose) fprintf(DEFAULT_VERBOSE_OUTPUT, "Clamping color value to 0\nOnly numbers between 0 and 255 inclusive are valid color values\n");
	    if (colorinput[0]<0) colorinput[0] = 0;
	    if (colorinput[1]<0) colorinput[1] = 0;
	    if (colorinput[2]<0) colorinput[2] = 0;
	  }
	  if ((colorinput[0]>255)|(colorinput[1]>255)|(colorinput[2]>255)) {
	    if (verbose) fprintf(DEFAULT_VERBOSE_OUTPUT, "Clamping color value to 255\nOnly numbers between 0 and 255 inclusive are valid color values\n");
	    if (colorinput[0]>255) colorinput[0] = 255;
	    if (colorinput[1]>255) colorinput[1] = 255;
	    if (colorinput[2]>255) colorinput[2] = 255;
	  }
	  wireMaterialColor[0] = (unsigned char)colorinput[0];
	  wireMaterialColor[1] = (unsigned char)colorinput[1];
	  wireMaterialColor[2] = (unsigned char)colorinput[2];
	}
      }
      memset(verboseinput, 0, DEFAULT_MAXNAMELENGTH*3);

      if (verbose) fprintf(DEFAULT_VERBOSE_OUTPUT, "Wire Segment Length: [%f] ", wireSegmentLength);
      if ( ! fgets(verboseinput, DEFAULT_MAXNAMELENGTH, stdin) ) {
	if (verbose) fprintf(DEFAULT_VERBOSE_OUTPUT, "Error reading input for %s encountered.\nContinuing with default values.\n", "wireSegmentLength");
	if (debug) fprintf(DEFAULT_DEBUG_OUTPUT, "main:fgets error reading %s from stdin\n", "wireSegmentLength");
      }
      else {
	len = strlen(verboseinput);
	if ((len>0) && (verboseinput[len-1]=='\n')) verboseinput[len-1] = 0;
	if (debug) {
	  fprintf(DEFAULT_DEBUG_OUTPUT,"main:entered [%s] for wire segment length\n", verboseinput);
	}
	if (strcmp(verboseinput, "") != 0) {
	  sscanf(verboseinput, "%lf", &wireSegmentLength);
	}
      }
      memset(verboseinput, 0, DEFAULT_MAXNAMELENGTH*3);

      if (verbose) fprintf(DEFAULT_VERBOSE_OUTPUT, "Wire Segment Separation: [%f] ", wireSegmentSeparation);
      if ( ! fgets(verboseinput, DEFAULT_MAXNAMELENGTH, stdin) ) {
	if (verbose) fprintf(DEFAULT_VERBOSE_OUTPUT, "Error reading input for %s encountered.\nContinuing with default values.\n", "wireSegmentSeparation");
	if (debug) fprintf(DEFAULT_DEBUG_OUTPUT, "main:fgets error reading %s from stdin\n", "wireSegmentSeparation");
      }
      else {
	len = strlen(verboseinput);
	if ((len>0) && (verboseinput[len-1]=='\n')) verboseinput[len-1] = 0;
	if (debug) {
	  fprintf(DEFAULT_DEBUG_OUTPUT,"main:entered [%s] for wire segment separation\n", verboseinput);
	}
	if (strcmp(verboseinput, "") != 0) {
	  sscanf(verboseinput, "%lf", &wireSegmentSeparation);
	}
      }
      memset(verboseinput, 0, DEFAULT_MAXNAMELENGTH*3);

      if (verbose) fprintf(DEFAULT_VERBOSE_OUTPUT, "Segment Name: [%s] ", segmentName);
      if ( ! fgets(verboseinput, DEFAULT_MAXNAMELENGTH, stdin) ) {
	if (verbose) fprintf(DEFAULT_VERBOSE_OUTPUT, "Error reading input for %s encountered.\nContinuing with default values.\n", "segmentName");
	if (debug) fprintf(DEFAULT_DEBUG_OUTPUT, "main:fgets error reading %s from stdin\n", "segmentName");
      }
      else {
	len = strlen(verboseinput);
	if ((len>0) && (verboseinput[len-1]=='\n')) verboseinput[len-1] = 0;
	if (debug) {
	  fprintf(DEFAULT_DEBUG_OUTPUT,"main:entered [%s] for segment name\n", verboseinput);
	}
	if (strcmp(verboseinput, "") != 0) {
	  strncpy(segmentName, verboseinput, DEFAULT_MAXNAMELENGTH);
	}
      }
      memset(verboseinput, 0, DEFAULT_MAXNAMELENGTH*3);

    }

    if ((errors=generateFence_s(fp, fenceName, fenceStartPosition, fenceEndPosition))!=0) {
      if (verbose) fprintf(DEFAULT_VERBOSE_OUTPUT, "\n...[%d] Errors generating fence [%s]\n", errors, fenceName);
    }
    else {
      if (verbose) fprintf(DEFAULT_VERBOSE_OUTPUT, "\n...Fence [%s] Generated.\n", fenceName);
    }
  }
  else {
    if ((errors=generateFence(fp, fenceName, fenceStartPosition, fenceHeight, fenceWidth))!=0) {
      if (verbose) fprintf(DEFAULT_VERBOSE_OUTPUT, "\n...[%d] Errors generating fence [%s]\n", errors, fenceName);
    }
    else {
      if (verbose) fprintf(DEFAULT_VERBOSE_OUTPUT, "\n...Fence [%s] Generated.\n", fenceName);
    }
  }

  wdb_close(fp);
  return errors;
}

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
