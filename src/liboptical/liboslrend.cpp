/*                O S L - R E N D E R E R . C P P
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
/** @file osl-renderer.cpp
 *
 * This code represents an interface to OSL system. Through it, one can
 * add OSL shaders and query colors, given some global parameters
 */

#include "liboslrend.h"


OSLRenderer::OSLRenderer(){

    shadingsys = ShadingSystem::create(&rend, NULL, &errhandler);
    
    ssi = (ShadingSystemImpl *)shadingsys;
}

OSLRenderer::~OSLRenderer(){

    ssi->destroy_thread_info(handle);
    ShadingSystem::destroy(shadingsys);
}

void OSLRenderer::AddShader(const char *shadername){

    /*
    OSLShader osl_sh;
    osl_sh.name = std::string(shadername);

    shadingsys->ShaderGroupBegin();
    shadingsys->Shader("surface", osl_sh.name.c_str(), NULL);
    shadingsys->ShaderGroupEnd();

    osl_sh.state = shadingsys->state();
    shadingsys->clear_state();

    shaders.push_back(osl_sh);
    */
}
ShadingAttribStateRef OSLRenderer::AddShader(ShaderGroupInfo &group_info){

    shadingsys->ShaderGroupBegin();

    for(size_t i = 0; i < group_info.shader_layers.size(); i++){

	ShaderInfo &sh_info = group_info.shader_layers[i];

	/* Set parameters */
	for(size_t i = 0; i < sh_info.fparam.size(); i++)
	    shadingsys->Parameter(sh_info.fparam[i].first.c_str(), TypeDesc::TypeFloat, &(sh_info.fparam[i].second));
	for(size_t i = 0; i < sh_info.cparam.size(); i++)
	    shadingsys->Parameter(sh_info.cparam[i].first.c_str(), TypeDesc::TypeColor, &(sh_info.cparam[i].second));
	for(size_t i = 0; i < sh_info.sparam.size(); i++)
	    shadingsys->Parameter(sh_info.sparam[i].first.c_str(), TypeDesc::TypeString, &(sh_info.sparam[i].second));

	if(sh_info.layername == "")
	    shadingsys->Shader("surface", sh_info.shadername.c_str(), NULL);
	else
	    shadingsys->Shader("surface", sh_info.shadername.c_str(), sh_info.layername.c_str());
#if 0
	printf("Adding a shader: %s, with name: %s\n", sh_info.shadername.c_str(), sh_info.layername.c_str());
#endif
    }	

    /* Set the edges between shader layers */
    for(size_t i = 0; i < group_info.shader_edges.size(); i++){
	ShaderParam &sh_param1 = group_info.shader_edges[i].first;
	ShaderParam &sh_param2 = group_info.shader_edges[i].second;
	shadingsys->ConnectShaders(sh_param1.layername.c_str(), sh_param1.paramname.c_str(),
				  sh_param2.layername.c_str(), sh_param2.paramname.c_str());

#if 0
	printf("Adding an edge between %s,%s and %s,%s\n", sh_param1.layername.c_str(), sh_param1.paramname.c_str(),
	       sh_param2.layername.c_str(), sh_param2.paramname.c_str());
#endif
    }
    
    shadingsys->ShaderGroupEnd();
    
    ShadingAttribStateRef sh_ref = shadingsys->state();
    shadingsys->clear_state();
    
    return sh_ref;
}

Color3 OSLRenderer::QueryColor(RenderInfo *info) const {

    if(info->depth >= 5){
	return Color3(0.0f);
    }

    fastf_t y = info->screen_y;

    unsigned short Xi[3];                /* seed for RNG */
    Xi[0] = rand();
    Xi[1] = rand();
    Xi[2] = rand();

    // execute shader
    ShaderGlobals globals;
    const ClosureColor *closure = ExecuteShaders(globals, info);

    Color3 weight = Color3(0.0f);
    // sample primitive from closure tree
    const ClosurePrimitive *prim = SamplePrimitive(weight, closure, 0.5);

    if(prim) {
	if(prim->category() == OSL::ClosurePrimitive::BSDF) {
	    // sample BSDF closure
	    BSDFClosure *bsdf = (BSDFClosure*)prim;
	    Vec3 omega_in, zero(0.0f);
	    Color3 eval;
	    float pdf = 0.0;

	    bsdf->sample(globals.Ng, globals.I, zero, zero,
			 erand48(Xi), erand48(Xi),
			 omega_in, zero, zero, pdf, eval);

	    if(pdf != 0.0f) {
		OSLRenderer::Vec3toPoint_t(globals.P, info->out_ray.origin);

		OSLRenderer::Vec3toPoint_t(omega_in, info->out_ray.dir);
		info->doreflection = 1;
		return weight*eval/pdf;
	    }
	}
	else if(prim->category() == OSL::ClosurePrimitive::Emissive) {
	    // evaluate emissive closure

	    EmissiveClosure *emissive = (EmissiveClosure*)prim;
	    Color3 l = weight*emissive->eval(globals.Ng, globals.I);
	    return l;
	}
	else if(prim->category() == OSL::ClosurePrimitive::Background) {
	    // background closure just returns weight
	    printf("[Background]\n");
	    return weight;
	}
    }
    return Color3(0.0f);
}
/* Return thread specific information */
void* OSLRenderer::CreateThreadInfo(){
    return ssi->create_thread_info();
}


/* -----------------------------------------------
 * Private methods
 * ----------------------------------------------- */

const ClosureColor * OSLRenderer::
ExecuteShaders(ShaderGlobals &globals, RenderInfo *info) const {

    memset(&globals, 0, sizeof(globals));

    VMOVE(globals.P, info->P);
    VMOVE(globals.I, info->I);
    VREVERSE(globals.I, globals.I);
    VMOVE(globals.Ng, info->N);

    globals.N = globals.Ng;

    // u-v coordinates
    globals.u = info->u;
    globals.v = info->v;

    // u-v tangents
    VMOVE(globals.dPdu, info->dPdu);
    VMOVE(globals.dPdv, info->dPdv);

    // other
    globals.raytype |= ((info->depth == 0) & (1 << 1));
    globals.surfacearea = info->surfacearea;
    globals.backfacing = (globals.Ng.dot(globals.I) < 0.0f);
    globals.Ci = NULL;

    // execute shader
    ShadingContext* ctx = ssi->get_context(info->thread_info);
    ctx->execute(ShadUseSurface, *(info->shader_ref),
		 globals);
    ssi->release_context(ctx, info->thread_info);
    
    return globals.Ci;
}

const ClosurePrimitive * OSLRenderer::SamplePrimitive(Color3& weight, const ClosureColor *closure, float r) const {

    if(closure) {
	const ClosurePrimitive *prim = NULL;
	float totw = 0.0f;

	SamplePrimitiveRecurse(prim, weight, closure, Color3(1.0f), totw, r);
	weight *= totw;

	return prim;
    }
    return NULL;
}
void OSLRenderer::SamplePrimitiveRecurse(const ClosurePrimitive*& r_prim, Color3& r_weight, const ClosureColor *closure,
					 const Color3& weight, float& totw, float& r) const {

    if(closure->type == ClosureColor::COMPONENT) {

	ClosureComponent *comp = (ClosureComponent*)closure;
	ClosurePrimitive *prim = (ClosurePrimitive*)comp->data();
	float p, w = fabsf(weight[0]) + fabsf(weight[1]) + fabsf(weight[2]);

	if(w == 0.0f)
	    return;

	totw += w;

	if(!r_prim) {
	    // no primitive was found yet, so use this
	    r_prim = prim;
	    r_weight = weight/w;
	}
	else {

	    p = w/totw;

	    if(r < p) {
		// pick other primitive
		r_prim = prim;
		r_weight = weight/w;

		r = r/p;
	    }
	    else {
		// keep existing primitive
		r = (r + p)/(1.0f - p);
	    }
	}
    }
    else if(closure->type == ClosureColor::MUL) {
	ClosureMul *mul = (ClosureMul*)closure;

	SamplePrimitiveRecurse(r_prim, r_weight, mul->closure, mul->weight * weight, totw, r);
    }
    else if(closure->type == ClosureColor::ADD) {
	ClosureAdd *add = (ClosureAdd*)closure;
	
	SamplePrimitiveRecurse(r_prim, r_weight, add->closureA, weight, totw, r);
	SamplePrimitiveRecurse(r_prim, r_weight, add->closureB, weight, totw, r);
    }
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
