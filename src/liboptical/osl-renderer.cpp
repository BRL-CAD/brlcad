#include "osl-renderer.h"

OSLRenderer::OSLRenderer(){

    shadingsys = ShadingSystem::create(&rend, NULL, &errhandler);
    
    InitShaders();

    // create thread info
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

Color3 OSLRenderer::QueryColor(){
    return Color3(1.0f, 0.0f, 0.0f);
}
/* -----------------------------------------------
 * Private methods
 * ----------------------------------------------- */
void OSLRenderer::InitShaders(){

    shadingsys->attribute("optimize", 2);
    shadingsys->attribute("lockgeom", 1);
    
    shadingsys->ShaderGroupBegin();
    shadingsys->Shader("surface", "glass", NULL);
    shadingsys->ShaderGroupEnd();
    
    shadingsys->clear_state();
}

/* -----------------------------------------------
 * Wrapper methods
 * ----------------------------------------------- */
OSLRenderer* oslrenderer_init(){
    OSLRenderer* oslr = new OSLRenderer();
    return oslr;
}
void oslrenderer_query_color(OSLRenderer *oslr, point_t *q){
    Color3 c = oslr->QueryColor();
    (*q)[0] = c[0];
    (*q)[1] = c[1];
    (*q)[2] = c[2];
}
void oslrenderer_free(OSLRenderer **osl){
    delete (*osl);
    *osl = NULL;
}

int Renderer2(point_t *p){
  (*p)[0] = 0.0;
  (*p)[1] = 1.0;
  (*p)[2] = 0.0;
  return 1;
}

