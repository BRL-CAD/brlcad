#ifndef OSL_RENDERER_H
#define OSL_RENDERER_H

#include "vmath.h"

#ifdef __cplusplus

#include "render_svc.h"

/* Class 'OSLRenderer' holds global information about OSL shader system. 
   These information are hidden from the calling C code */
class OSLRenderer {

    

};

#endif


#ifdef __cplusplus
extern "C" {
#endif

    /* Wrappers for OSLREnderer class methods */
    int Renderer2(point_t *p);

#ifdef __cplusplus
}
#endif




#endif
