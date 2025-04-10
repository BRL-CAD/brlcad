/*                         F E N C E . H
 * BRL-CAD
 *
 * Copyright (c) 2004-2025 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 *
 */
/** @file shapes/fence.h
 *
 * This is the header file to the program that generates a chain-link
 * fence.  Every parameter of the fence may be adjusted by changing
 * the DEFAULT_* macros defined below.
 *
 */

#ifndef SHAPES_FENCE_H
#define SHAPES_FENCE_H

#include "common.h"

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <string.h>

#include "vmath.h"
#include "bn.h"
#include "raytrace.h"
#include "wdb.h"

__BEGIN_DECLS

/**
 * these variables control the "behavior" of this program's output:
 *
 *   if debug is set, debug information (extra "useful" output is
 *     given) is displayed on stdout.
 *
 *   if verbose is set, then all prompting and output that may be
 *     shown (such as results and errors) will be sent to stderr.
 *     this should probably be switched with debug (maybe) but
 *     convention was taken from another db program.
 *
 *   if interactive is set, then the user will be prompted for values
 *     from stdin.
 *
 *   the default output files for debug and verbose modes may be
 *     adjusted by providing standard file pointers (like stdout or
 *     stderr)
 */
#define DEFAULT_DEBUG 0
#define DEFAULT_VERBOSE 0
#define DEFAULT_INTERACTIVE 0

#define DEFAULT_DEBUG_OUTPUT stdout
#define DEFAULT_VERBOSE_OUTPUT stderr

/**
 * this is the name of the default output file name
 */
#define DEFAULT_OUTPUTFILENAME "fence.g"

/**
 * this is the default measuring units for the database file
 */
#define DEFAULT_UNITS "mm"

/**
 * this is the identifying title or name for the database file
 */
#define DEFAULT_ID "Untitled"

/**
 * for all of the default names that are specified, a `.s' or `.r'
 * suffix will automatically get appended where needed for the solid
 * and region objects that are generated, respectively, and do not
 * need to be added.
 */

/**
 * the fence generation is broken into four main parts, consisting of
 * one of the wires of the fence mesh, the mesh as a whole, the poles
 * that may attach to the mesh, and the fence as a whole (poles and
 * mesh).
 */

/**
 * these are the parameters for the main fence object that gets
 * generated.
 *
 *   the name of the entire resulting fence object may be set as a
 *   string.
 *
 *   the point at which the fence starts is where the first pole, and
 *     the mesh will begin at.  the fence is generated bottom-up
 *     (first quadrant).
 *
 *   the width and height settings here may be overridden by the pole
 *     and/or mesh width and height settings if they are given.
 *     default fence generation, however, generates the poles based on
 *     the default fence height and width.
 *
 *   material and material params are the two defaults that set the
 *     material properties of the fence poles and wires (not separate
 *     at this time)
 *
 *   fence, poles, mesh, and wire are all truth values (1 == TRUE, 0
 *     == FALSE) that indicate whether or not the user will be
 *     prompted for respective values when the program is running in
 *     interactive mode.  that is to say that if the values are (1, 1,
 *     1, 0) respectively, then the user will be prompted (if in
 *     interactive mode) for the height and width of the fence, poles,
 *     and mesh, but will not be prompted for values for the wire
 *     (such as wire radius).  default values or command-line values
 *     are used if not in interactive mode.
 *
 *   the generate fence, poles, and mesh defaults are truth values
 *     that determine whether or not the fence pole and mesh object
 *     collections get generated.  the default is to generate all
 *     objects, but this may be changed through a command-line value
 */
#define DEFAULT_FENCENAME "fence"
#define DEFAULT_FENCESTARTPOSITION {0.0, 0.0, 0.0}
#define DEFAULT_FENCEENDPOSITION {3000.0, 0.0, 0.0}
#define DEFAULT_FENCEWIDTH {3000.0, 0.0, 0.0}
#define DEFAULT_FENCEHEIGHT {0.0, 0.0, 2000.0}
#define DEFAULT_FENCEPOLESPACING 3000.0
#define DEFAULT_FENCEMATERIAL "plastic"
#define DEFAULT_FENCEMATERIALPARAMS "sh=16 sp=.5 di=.5"
#define DEFAULT_FENCEMATERIALCOLOR {128, 128, 128}

#define DEFAULT_FENCEFENCE 1
#define DEFAULT_FENCEPOLES 0
#define DEFAULT_FENCEMESH 0
#define DEFAULT_FENCEWIRE 0
#define DEFAULT_GENERATEFENCE 1
#define DEFAULT_GENERATEPOLES 1
#define DEFAULT_GENERATEMESH 1

/**
 * these are the parameters for the pole objects that get generated.
 * every pole object is a separate object and may be individually
 * addressed.
 *
 *   all of the poles are named with the name given plus a numerical
 *     identifier (i.e. "poles4.r").  the format of the name may be
 *     adjusted below under the polebasicparam setting.
 *
 *   the poles may have their material properties set through the
 *     settings of the type, parameters, and color.
 *
 *   the pole's height and radius may also be respectively set
 */
#define DEFAULT_POLENAME "poles"
#define DEFAULT_POLEHEIGHT 2000.0
#define DEFAULT_POLERADIUS 30.0
#define DEFAULT_POLEMATERIAL "plastic"
#define DEFAULT_POLEMATERIALPARAMS "sh=16 sp=.5 di=.5"
#define DEFAULT_POLEMATERIALCOLOR {128, 128, 128}

/**
 * these are the parameters for the mesh object that gets generated.
 * there is only one mesh object, consisting of two wires translated
 * across a region to form a mesh.
 *
 *   the name, height and width of the mesh may be respectively set.
 *   the material properties of material type, parameters, and color
 *     may also be set.
 */
#define DEFAULT_MESHNAME "mesh"
#define DEFAULT_MESHHEIGHT DEFAULT_POLEHEIGHT
#define DEFAULT_MESHWIDTH DEFAULT_FENCEPOLESPACING
#define DEFAULT_MESHMATERIAL "plastic"
#define DEFAULT_MESHMATERIALPARAMS "sh=16 sp=.5 di=.5"
#define DEFAULT_MESHMATERIALCOLOR {128, 128, 128}

/**
 *   the mesh piece count is a count of the number of actual wire
 *     segments that form a single mesh cell.  this value should not
 *     be changed (20)
 */
#define DEFAULT_MESHPIECECOUNT 20

/**
 * these are the parameters for the basic wires (2) that form the mesh
 * object.  there are only two wires, and it is translated across the
 * span of the mesh when the mesh is generated.
 *
 *   the name and radius of the wire may be set.
 *   the wire is composed of twisting segments that twist based on some
 *     given angle.
 *   this angle may be respectively set, as may the name of the wire
 *     segments.
 *   the material properties of the wire be specified by material,
 *     parameters, and color
 *   the length of the wire segments and the amount of distance that
 *     that the wire twists back may also be adjusted through segment
 *     length and segment separation.
 */
#define DEFAULT_WIRENAME "wire"
#define DEFAULT_SEGMENTNAME "seg"
#define DEFAULT_WIRERADIUS 2.0
#define DEFAULT_WIREANGLE 45.0
#define DEFAULT_WIREMATERIAL "plastic"
#define DEFAULT_WIREMATERIALPARAMS "sh=16 sp=.5 di=.5"
#define DEFAULT_WIREMATERIALCOLOR {128, 128, 128}
#define DEFAULT_WIRESEGMENTLENGTH 50.0
#define DEFAULT_WIRESEGMENTSEPARATION DEFAULT_WIRERADIUS
/**
 * the max wire segments should not be changed as it is merely a max
 * upper bound on the maximum number of segments that may be generated
 * for any wire pair.  it is provided as a safeguard against having an
 * "out-of-control" program if invalid wire values are entered.
 */
#define DEFAULT_MAXWIRESEGMENTS 1234567890L

/**
 * these are the format specifiers that get set for the pole and wire
 * names that get generated.  the format of the parameter specifier is
 * the same as "C" stdio specifications and should not usually need to
 * be changed.
 */
#define DEFAULT_POLEBASICPARAM "%s%d"
#define DEFAULT_WIREBASICPARAM "%s%d"
#define DEFAULT_REGIONSUFFIX ".r"
/**
 * this is an upper bounds restriction placed on the length of any and
 * all names in the database.
 */
#define DEFAULT_MAXNAMELENGTH 64


/**
 * this macro does the standard conversion of an angle to a radian
 * value.
 */
#define RADIAN(x) ((x) * DEG2RAD)


extern void argumentHelp(FILE *fp, char *progname, char *message);
extern void argumentExamples(FILE *fp, char *progname);
extern void defaultSettings(FILE *fp);
extern int parseArguments(int argc, char *argv[]);
extern void printMatrix(FILE *fp, char *n, mat_t m);
extern char *getName(const char *base, int id, const char *suffix);
extern char *getPrePostName(char *prefix, char *base, char *suffix);

extern int generateFence_s(struct rt_wdb *fp, char *fencename, point_t startposition, point_t endposition);
extern int generateFence(struct rt_wdb *fp, char *fencename, point_t startposition, vect_t heightvector, vect_t widthvector);

extern int generatePoles_s(struct rt_wdb *fp, char *polename);
extern int generatePoles(struct rt_wdb *fp, char *polename, point_t startposition, vect_t heightvector, vect_t widthvector, double radius);

extern int generateMesh_s(struct rt_wdb *fp, char *meshname);
extern int generateMesh(struct rt_wdb *fp, char *meshname, point_t startposition, vect_t heightvector, vect_t widthvector);

extern int generateWire_s(struct rt_wdb *fp, char *wirename, point_t position);
extern int generateWire(struct rt_wdb *fp, char *wirename, point_t position, vect_t heightvector, vect_t widthvector, double radius, double angle, double wiresegmentlength);

extern int createWire(struct rt_wdb *fp, char *segmentname, vect_t heightvector, vect_t widthvector, double radius, double angle, double segmentlength, double segmentdepthseparation);

__END_DECLS

#endif /* SHAPES_FENCE_H */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
