/*
 *			P H O T O N M A P. H
 *
 *  Declarations related to Photon Mapping
 *
 *  Source -
 *	Bldg 238
 *	The U. S. Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066
 *  
 *  Copyright Notice -
 *	This software is Copyright (C) 2002 by the United States Army.
 *	All rights reserved.
 *
 *  @(#)$Header
 */

#include "conf.h"
#ifdef USE_STRING_H     /* OPTIONAL, for strcmp() etc. */
#  include <string.h>
#else
#  include <strings.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "machine.h"
#include "externs.h"
#include "bu.h"
#include "vmath.h"
#include "bn.h"
#include "db.h"
#include "nmg.h"
#include "raytrace.h"
#include "shadework.h"


#define	PM_GLOBAL	0
#define	PM_CAUSTIC	1
#define	PM_SHADOW	2

#define	PM_SEM		RT_SEM_LAST-1
#define	PM_SEM_INIT	RT_SEM_LAST

/*
 *  Photon Map Data Structure
 */
struct Photon {
  int			Axis;		/* Splitting Plane */
  point_t		Pos;		/* Photon Position */
  vect_t		Dir;		/* Photon Direction */
  vect_t		Normal;		/* Normal at Intersection */
  vect_t		Power;		/* Photon Power */
  vect_t		Irrad;		/* Irradiance */
};


struct PSN {
  struct	Photon	P;
  fastf_t		Dist;
};


/*
 *  Photon Search Structure
 */
struct PhotonSearch {
  int			Max;		/* Max Number of Photons */
  int			Found;		/* Number of Photons Found */
  vect_t		Normal;		/* Normal */
  fastf_t		RadSq;		/* Search Radius Sq */
  point_t		Pos;		/* Search Position */
  struct	PSN	*List;		/* Located Photon List */
};


struct NearestPhotons {
  int			Max;		/* Max Number of Photons */
  int			Found;		/* Number of Photons Found */
  vect_t		Normal;		/* Normal */
  fastf_t		RadSq;		/* Search Radius Sq */
  point_t		Pos;		/* Search Position */
  struct	Photon	*List;		/* Located Photon List */
};


struct PNode {
  struct	Photon	P;
  struct	PNode	*L;
  struct	PNode	*R;
  int			C;		/* For Threading Purposes to see if it's been computed yet */
};


struct PhotonMap {
  int			StoredPhotons;
  int			MaxPhotons;
  struct	PNode	*Root;
};


struct IrradNode {
  point_t		Pos;
  vect_t		RGB;
};


struct IrradCache {
  int				Num;
  struct	IrradNode	*List;
};


extern	int	PM_Activated;	/* Photon Mapping Activated, 0=off, 1=on */

BU_EXTERN(void BuildPhotonMap, (struct application *ap, int cpus, int width, int height, int Hypersample, int GlobalPhotons, double CausticsPercent, int Rays, double AngularTolerance, int RandomSeed, int IrradianceHypersampling));
BU_EXTERN(void IrradianceEstimate, (struct application *ap, vect_t irrad, point_t pos, vect_t normal, fastf_t rad, int np));
