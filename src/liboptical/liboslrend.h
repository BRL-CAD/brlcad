/*                  O S L - R E N D E R E R . H
 * BRL-CAD
 *
 * Copyright (c) 2011 United States Government as represented by
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
/** @file osl-renderer.h
 *
 * This code represents an interface to OSL system. Through it, one can
 * add OSL shaders and query colors, given some global parameters
 *
 */

#ifndef LIB_OSL_RENDERER_H
#define LIB_OSL_RENDERER_H

#include <stdio.h>

#include <string>
#include <vector>

#include "vmath.h"

#  include "oslclosure.h"
#  include "render_svc.h"
/* FIXME  -- Add my own ShaderSystem? */
#  include "./oslexec_pvt.h"

using namespace OSL;

typedef struct Ray {
    point_t dir;
    point_t origin;
} Ray;

/* Shared struct by which the C shader and the C++ render system may
   exchange information
*/
struct RenderInfo {

    /* -- input -- */
    void *thread_info;      /* Thread specific information */
    fastf_t screen_x;       /* Coordinates of the screen (if applicable) */
    fastf_t screen_y;
    point_t P;              /* Query point */
    point_t N;              /* Object normal */
    point_t I;              /* Incident ray direction */
    fastf_t u, v;           /* uv coordinates */
    point_t dPdu, dPdv;     /* uv tangents */
    int depth;              /* How many times the ray hit an object */
    fastf_t surfacearea;    /* FIXME */
    ShadingAttribStateRef shader_ref;   /* Reference for the shader we're querying */
 
    /* -- output -- */
    point_t pc;           /* Color of the point (or multiplier) */
    int doreflection;     /* 1 if there will be reflection 0, otherwise */
    Ray out_ray;          /* output ray (in case of reflection) */
};

/* Required structure to initialize an OSL shader */
struct ShaderInfo {
    std::string shadername; // Name of the shader (type of shader)
    std::string layername;  // Name of the layer  (name of this partilar instance)
    std::vector< std::pair<std::string, float> > fparam;
    std::vector< std::pair<std::string, Color3> > cparam;
    std::vector< std::pair<std::string, std::string> > sparam;
};
/* Represents a parameter a shader */
struct ShaderParam {
    std::string layername;
    std::string paramname;
};
/* Represents an edge from first to second  */
typedef std::pair < ShaderParam, ShaderParam > ShaderEdge;

/* Required structure to initialize an OSL shader group */
struct ShaderGroupInfo {
    std::vector< ShaderInfo > shader_layers;
    std::vector< ShaderEdge > shader_edges;
};


/* Class 'OSLRenderer' holds global information about OSL shader system.
   These information are hidden from the calling C code */
class OSLRenderer {

    ErrorHandler errhandler;
    
    ShadingSystem *shadingsys;
    ShadingSystemImpl *ssi;
    SimpleRenderer rend;
    void *handle;

    /* Information about each shader of the renderer */
#if 0
    struct OSLShader{
	std::string name;
	ShadingAttribStateRef state;
    };
    std::vector<OSLShader> shaders;
#endif

    const ClosureColor
	*ExecuteShaders(ShaderGlobals &globals, RenderInfo *info) const;

    /* Sample a primitive from the shaders group */
    const ClosurePrimitive* SamplePrimitive(Color3& weight,
					    const ClosureColor *closure,
					    float r) const;

    /* Helper function for SamplePrimitive */
    void SamplePrimitiveRecurse(const ClosurePrimitive*& r_prim,
				Color3& r_weight,
				const ClosureColor *closure,
				const Color3& weight, float& totw, float& r) const;

public:

    OSLRenderer();
    ~OSLRenderer();

    /* Add an OSL shader to the system */
    void AddShader(const char *shadername);

    ShadingAttribStateRef AddShader(ShaderGroupInfo &group_info);

    /* Query a color */
    Color3 QueryColor(RenderInfo *info) const;

    /* Return thread specific information */
    void * CreateThreadInfo();

    static void Vec3toPoint_t(Vec3 s, point_t t){
	t[0] = s[0];
	t[1] = s[1];
	t[2] = s[2];
    }

};

#endif

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
