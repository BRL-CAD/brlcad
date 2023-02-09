#ifndef _PM_GAMMA_H_
#define _PM_GAMMA_H_

#include <netpbm/pm_config.h>

#include <math.h>

#ifdef __cplusplus
extern "C" {
#endif
#if 0
} /* to fake out automatic code indenters */
#endif

static __inline__ float
pm_gamma709(float const intensity) {

    /* Here are parameters of the gamma transfer function
       for the Netpbm formats.  This is CIE Rec 709.
       
       This transfer function is linear for sample values 0 .. .018 
       and an exponential for larger sample values.
       The exponential is slightly stretched and translated, though,
       unlike the popular pure exponential gamma transfer function.
    */
    float const gamma = 2.2;
    float const oneOverGamma = 1.0 / gamma;
    float const linearCutoff = 0.018;
    float const linearExpansion = 
        (1.099 * pow(linearCutoff, oneOverGamma) - 0.099) / linearCutoff;

    float brightness;

    if (intensity < linearCutoff)
        brightness = intensity * linearExpansion;
    else
        brightness = 1.099 * pow(intensity, oneOverGamma) - 0.099;

    return brightness;
}



static __inline__ float
pm_ungamma709(float const brightness) {

    /* These are the same parameters as in pm_gamma, above */

    float const gamma = 2.2;
    float const oneOverGamma = 1.0 / gamma;
    float const linearCutoff = 0.018;
    float const linearExpansion = 
        (1.099 * pow(linearCutoff, oneOverGamma) - 0.099) / linearCutoff;
    
    float intensity;

    if (brightness < linearCutoff * linearExpansion)
        intensity = brightness / linearExpansion;
    else
        intensity = pow((brightness + 0.099) / 1.099, gamma);

    return intensity;
}

#ifdef __cplusplus
}
#endif
#endif
