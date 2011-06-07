#ifndef OSL_RENDERER_H
#define OSL_RENDERER_H

#include <stdio.h>
#include "vmath.h"

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

    SimpleRenderer rend;
    ErrorHandler errhandler;
    ShadingSystem *shadingsys;
    ShadingSystemImpl *ssi;
    ThreadInfo thread_info;

    /* Load OSL shaders */
    /* FIXME: Add support for any osl shader */
    void InitShaders();

 public:
    
    OSLRenderer();
    ~OSLRenderer();

    /* Query a color */
    Color3 QueryColor();

};

#else

typedef 
struct OSLRenderer
OSLRenderer;

#endif


#ifdef __cplusplus
extern "C" {
#endif

    /* Wrapper for OSLRenderer constructor */
    OSLRenderer * oslrenderer_init();

    /* Wrapper for OSLRenderer::QueryColor */
    void oslrenderer_query_color(OSLRenderer *oslr, point_t *q);

    /* Wrapper for OSLRenderer destructor */
    void oslrenderer_free(OSLRenderer **osl);

#ifdef __cplusplus
}
#endif




#endif
