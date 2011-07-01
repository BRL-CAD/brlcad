/*                     P H O T O N M A P . H
 * BRL-CAD
 *
 * Copyright (c) 2002-2011 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @addtogroup librt */
/** @{ */
/** @file photonmap.h
 *			P H O T O N M A P. H
 * @brief
 *  Declarations related to Photon Mapping
 *
 */

#include "common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "bu.h"
#include "vmath.h"
#include "bn.h"
#include "db.h"
#include "nmg.h"
#include "raytrace.h"
#include "optical.h"
#include "plastic.h"
#include "light.h"


#define	PM_MAPS		4

#define	PM_GLOBAL	0
#define	PM_CAUSTIC	1
#define	PM_SHADOW	2	/* Current not used */
#define	PM_IMPORTANCE	3

#define	PM_SEM		RT_SEM_LAST-1
#define	PM_SEM_INIT	RT_SEM_LAST

/**
 *  Photon Map Data Structure
 */
struct Photon {
    int			Axis;		/**< @brief Splitting Plane */
    point_t		Pos;		/**< @brief Photon Position */
    vect_t		Dir;		/**< @brief Photon Direction */
    vect_t		Normal;		/**< @brief Normal at Intersection */
    vect_t		Power;		/**< @brief Photon Power */
    vect_t		Irrad;		/**< @brief Irradiance */
};


struct PSN {
    struct	Photon	P;
    fastf_t		Dist;
};


/**
 *  Photon Search Structure
 */
struct PhotonSearch {
    int			Max;		/**< @brief Max Number of Photons */
    int			Found;		/**< @brief Number of Photons Found */
    vect_t		Normal;		/**< @brief Normal */
    fastf_t		RadSq;		/**< @brief Search Radius Sq */
    point_t		Pos;		/**< @brief Search Position */
    struct	PSN	*List;		/**< @brief Located Photon List */
};


struct NearestPhotons {
    int			Max;		/**< @brief Max Number of Photons */
    int			Found;		/**< @brief Number of Photons Found */
    vect_t		Normal;		/**< @brief Normal */
    fastf_t		RadSq;		/**< @brief Search Radius Sq */
    point_t		Pos;		/**< @brief Search Position */
    struct	Photon	*List;		/**< @brief Located Photon List */
};


struct PNode {
    struct	Photon	P;
    struct	PNode	*L;
    struct	PNode	*R;
    int			C;		/**< @brief For Threading Purposes to see if it's been computed yet */
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


OPTICAL_EXPORT extern int PM_Activated;	/**< @brief Photon Mapping Activated, 0=off, 1=on */
OPTICAL_EXPORT extern int PM_Visualize;	/**< @brief Photon Mapping Visualization of Irradiance Cache */

OPTICAL_EXPORT extern void BuildPhotonMap(struct application *ap,
					  point_t eye_pos,
					  int cpus,
					  int width,
					  int height,
					  int Hypersample,
					  int GlobalPhotons,
					  double CausticsPercent,
					  int Rays,
					  double AngularTolerance,
					  int RandomSeed,
					  int ImportanceMapping,
					  int IrradianceHypersampling,
					  int VisualizeIrradiance,
					  double LightIntensity,
					  char pmfile[255]);
OPTICAL_EXPORT extern void IrradianceEstimate(struct application *ap,
					      vect_t irrad,
					      point_t pos,
					      vect_t normal);
/** @} */
/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
