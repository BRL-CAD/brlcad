#include "osl-renderer.h"

OSLRenderer::OSLRenderer(){

    shadingsys = ShadingSystem::create(&rend, NULL, &errhandler);
    
    InitShaders();


    ssi = (ShadingSystemImpl *)shadingsys;
    thread_info.handle = ssi->create_thread_info();
    thread_info.ctx = ssi->get_context(thread_info.handle);

}
OSLRenderer::~OSLRenderer(){
    
    // free OSL data
    ssi->release_context(thread_info.ctx, thread_info.handle);
    ssi->destroy_thread_info(thread_info.handle);
    
    ShadingSystem::destroy(shadingsys);
}

Color3 OSLRenderer::QueryColor(RenderInfo *info){

    fastf_t y = info->screen_y;

    thread_info.Xi[0] = 0;
    thread_info.Xi[1] = 0;
    thread_info.Xi[2] = y*y*y;
    
    // execute shader
    ShaderGlobals globals;
    ClosureColor *closure = ExecuteShaders(globals, info);

    // sample primitive from closure tree
    Color3 weight;
    const ClosurePrimitive *prim = SamplePrimitive(weight, closure, erand48(thread_info.Xi));

    if(prim) {
        if(prim->category() == OSL::ClosurePrimitive::BSDF) {
            // sample BSDF closure
            BSDFClosure *bsdf = (BSDFClosure*)prim;
            Vec3 omega_in, zero(0.0f);
            Color3 eval;
            float pdf = 0.0;

            bsdf->sample(globals.Ng, globals.I, zero, zero, erand48(thread_info.Xi), erand48(thread_info.Xi),
			 omega_in, zero, zero, pdf, eval);
	    
	    printf("Weight: %.2lf %.2lf %.2lf -- pdf: %.2lf\n", weight[0], weight[1], weight[2], pdf);
	    if (pdf <= 1e-4){
		printf("Error\n");
		return Color3(0.0f);
	    }
	    return weight*eval/pdf;
	    /* Recursion FIXME
            if(pdf != 0.0f) {
                Ray new_ray(globals.P, omega_in);
                Color3 r = (weight*eval/pdf)*radiance(thread_info, new_ray, depth+1);

                return r;
            }
	    */
        }
        else if(prim->category() == OSL::ClosurePrimitive::Emissive) {
            // evaluate emissive closure
            EmissiveClosure *emissive = (EmissiveClosure*)prim;
            return weight*emissive->eval(globals.Ng, globals.I);
        }
        else if(prim->category() == OSL::ClosurePrimitive::Background) {
            // background closure just returns weight
            return weight;
        }
    }
    return Color3(0.0f);
}
/* -----------------------------------------------
 * Private methods
 * ----------------------------------------------- */

void OSLRenderer::InitShaders(){

    shadingsys->attribute("optimize", 2);
    shadingsys->attribute("lockgeom", 1);
    
    shadingsys->ShaderGroupBegin();
    shadingsys->Shader("surface", "yellow", NULL);
    shadingsys->ShaderGroupEnd();

    shaderstate = shadingsys->state();
    
    shadingsys->clear_state();
}
ClosureColor * OSLRenderer::ExecuteShaders(ShaderGlobals globals, RenderInfo *info){
    
    PointTtoVec3(info->P, globals.P);
    PointTtoVec3(info->I, globals.I);
    PointTtoVec3(info->N, globals.Ng);
    globals.N = globals.Ng;    
    
    // u-v coordinates
    globals.u = info->u;
    globals.v = info->v;

    // tangents
    PointTtoVec3(info->dPdu, globals.dPdu);
    PointTtoVec3(info->dPdv, globals.dPdv);

    // other
    globals.raytype |= ((info->depth == 0) & (1 << 1));
    globals.surfacearea = info->surfacearea;
    globals.backfacing = (globals.Ng.dot(globals.I) < 0.0f);
    globals.Ci = NULL;

    // execute shader
    thread_info.ctx->execute(ShadUseSurface, *shaderstate, globals);

    return globals.Ci;
}

const ClosurePrimitive * OSLRenderer::SamplePrimitive(Color3& weight, const ClosureColor *closure, float r){

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
                                     const Color3& weight, float& totw, float& r){
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

/* -----------------------------------------------
 * Wrapper methods
 * ----------------------------------------------- */
OSLRenderer* oslrenderer_init(){
    OSLRenderer* oslr = new OSLRenderer();
    return oslr;
}
void oslrenderer_query_color(OSLRenderer *oslr, RenderInfo *info){
    Color3 pc = oslr->QueryColor(info);
    info->pc[0] = pc[0];
    info->pc[1] = pc[1];
    info->pc[2] = pc[2];
}
void oslrenderer_free(OSLRenderer **osl){
    delete (*osl);
    *osl = NULL;
}


