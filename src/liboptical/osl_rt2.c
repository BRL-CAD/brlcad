#include "osl-renderer.h"
#include <math.h>

int w = 1024, h = 768;
static unsigned short Xi[3];

int main (){

    OSLRenderer *oslr;
    oslr = oslrenderer_init("yellow");

    for(int y=0; y<h; y++) {
        
        fprintf(stderr, "\rRendering %5.2f%%", 100.*y/(h-1));

        Xi[0] = 0;
        Xi[1] = 0;
        Xi[2] = y*y*y;

        for(int x=0; x<w; x++) {
            int i = (h - y - 1)*w + x;

            double r1 = 2*erand48(Xi);
            double r2 = 2*erand48(Xi);
                        
            double dx = (r1 < 1)? sqrt(r1)-1: 1-sqrt(2-r1);
            double dy = (r2 < 1)? sqrt(r2)-1: 1-sqrt(2-r2);

            float d[3] = {1.0, 2.0, 1.3};
            
            // -----------------------------------------
            // -           Fill in RenderInfo          -
            // -----------------------------------------
            RenderInfo info;
            
            info.screen_x = x;
            info.screen_y = y;
            
            float P[3] = {0.0, 1.0, 0.0};
            info.P[0] = P[0];
            info.P[1] = P[1];
            info.P[2] = P[2];
            
            float N[3] = {1.0, 0, 0.0};
            info.N[0] = N[0];
            info.N[1] = N[1];
            info.N[2] = N[2];
            
            info.u = 0;
            info.v = 0;
            
            float dPdu[3] = {0.0, 0.0, 0.0};
            float dPdv[3] = {0.0, 0.0, 0.0};
                                    
            info.dPdu[0] = dPdu[0];
            info.dPdu[1] = dPdu[1];
            info.dPdu[2] = dPdu[2];
            
            info.dPdv[0] = dPdv[0];
            info.dPdv[1] = dPdv[1];
            info.dPdv[2] = dPdv[2];
            
            info.depth = 0;
            info.isbackfacing = 0;
            info.surfacearea = 1.0;
            
            oslrenderer_query_color(oslr, &info);
        }
    }
    
    return 0;
}
