#ifndef OSL_RENDERER_H
#define OSL_RENDERER_H

#include <stdio.h>
#include "vmath.h"

/* Shared struct by which the C shader and the C++ render system may 
   exchange information
 */
struct RenderInfo {
    
    /* -- input -- */
    fastf_t screen_x;     /* Coordinates of the screen (if applicable) */
    fastf_t screen_y; 
    point_t P;            /* Query point */
    point_t N;            /* Object normal */
    point_t I;            /* Incident ray direction */
    fastf_t u, v;         /* uv coordinates */
    point_t dPdu, dPdv;   /* uv tangents */
    int depth;            /* How many times the ray hit an object */
    fastf_t surfacearea;  /* FIXME */
    int isbackfacing;     /* FIXME */
   
    /* -- output -- */
    point_t pc; /* Color of the point (or multiplier) */
};

#ifdef __cplusplus

#include "render_svc.h"
/* FIXME  -- Add my own ShaderSystem? */
#include "./oslexec_pvt.h"
#include "oslclosure.h"

using namespace OSL;

struct ThreadInfo {
    void *handle;
    ShadingContext *ctx;
    unsigned short Xi[3];
};

/* Class 'OSLRenderer' holds global information about OSL shader system. 
   These information are hidden from the calling C code */
class OSLRenderer {

    ErrorHandler errhandler;
    ShaderGlobals globals;
    ShadingSystem *shadingsys;
    ShadingSystemImpl *ssi;
    SimpleRenderer rend;
    ThreadInfo thread_info;
    ShadingAttribStateRef shaderstate;

    /* Load OSL shaders 
       FIXME: Add support for any osl shader */
    void InitShaders();
    /* Build the closure tree */
    //static const ClosureColor *ExecuteShaders(ShaderGlobals &globals, RenderInfo *info);

    static const ClosureColor 
        *ExecuteShaders(ThreadInfo &thread_info,
                        ShaderGlobals &globals, RenderInfo *info,
                        ShadingAttribStateRef shaderstate);

    /* Sample a primitive from the shaders group */
    const ClosurePrimitive* SamplePrimitive(Color3& weight, const ClosureColor *closure, float r);
    /* Helper function for SamplePrimitive */
    void SamplePrimitiveRecurse(const ClosurePrimitive*& r_prim, Color3& r_weight, const ClosureColor *closure,
				const Color3& weight, float& totw, float& r);
    
    
 public:
    
    OSLRenderer();
    ~OSLRenderer();

    /* Query a color */
    Color3 QueryColor(RenderInfo *info);

};

#else

typedef 
struct OSLRenderer
OSLRenderer;

typedef 
struct RenderInfo
RenderInfo;

#endif


#ifdef __cplusplus
extern "C" {
#endif

    /* Wrapper for OSLRenderer constructor */
    OSLRenderer * oslrenderer_init();

    /* Wrapper for OSLRenderer::QueryColor */
    void oslrenderer_query_color(OSLRenderer *oslr, RenderInfo *info);

    /* Wrapper for OSLRenderer destructor */
    void oslrenderer_free(OSLRenderer **osl);

#ifdef __cplusplus
}
#endif




#endif
