/*              A R T _ B E C K M A N N _ R E F E R E N C E . H
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
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
#ifndef ART_BECKMANN_REFERENCE_H
#define ART_BECKMANN_REFERENCE_H

// OSL differentiates this scalar visible-normal sampler independently of
// the renderer's sampling Jacobian. Its azimuth requires correct atan2
// partials, as checked by the OSL dependency's numerical regressions.
float inverse_error_reference(float x)
{
    // Differentiate the clamped Giles polynomial used by the sampler,
    // rather than the mathematically exact inverse error function.
    float y = clamp(x, -0.99999, 0.99999);
    float w = -log((1 - y) * (1 + y));
    float p;
    if (w < 5) {
        w -= 2.5;
        p = 2.81022636e-8;
        p = 3.43273939e-7 + p * w;
        p = -3.5233877e-6 + p * w;
        p = -4.39150654e-6 + p * w;
        p = 0.00021858087 + p * w;
        p = -0.00125372503 + p * w;
        p = -0.00417768164 + p * w;
        p = 0.246640727 + p * w;
        p = 1.50140941 + p * w;
    } else {
        w = sqrt(w) - 3;
        p = -0.000200214257;
        p = 0.000100950558 + p * w;
        p = 0.00134934322 + p * w;
        p = -0.00367342844 + p * w;
        p = 0.00573950773 + p * w;
        p = -0.0076224613 + p * w;
        p = 0.00943887047 + p * w;
        p = 1.00167406 + p * w;
        p = 2.83297682 + p * w;
    }
    return p * y;
}

vector beckmann_sample(vector outgoing, float rx, float ry, float ax, float ay)
{
    float side = outgoing[1] < 0 ? -1 : 1;
    vector view = normalize(side * vector(ax * outgoing[0], outgoing[1], ay * outgoing[2]));
    float ct = view[1];
    float phi = ct < 0.99999 ? atan2(view[2], view[0]) : 0;
    float slope_y = inverse_error_reference(2 * ry - 1);
    float slope_x;
    float threshold = 1e-6;
    if (ct > 1 - threshold)
        slope_x = inverse_error_reference(2 * rx - 1);
    else {
        ct = max(ct, threshold);
        float t = sqrt(1 - ct * ct) / ct;
        float cot = 1 / t;
        float c = erf(cot);
        float k = t / sqrt(M_PI);
        float approx = rx * (1 + c + k * (1 - c * c));
        float exact = rx * (1 + c + k * exp(-cot * cot));
        float b = (0.5 - sqrt(k * (k - approx + 1) + 0.25)) / k;
        float inv = inverse_error_reference(b);
        float error = 1 + b + k * exp(-inv * inv) - exact;
        if (abs(error) > threshold) {
            // Keep both Newton updates: differentiating an ideal inverse
            // CDF would miss the actual sampler's approximation error.
            b -= error / (1 - inv * t);
            inv = inverse_error_reference(b);
            error = 1 + b + k * exp(-inv * inv) - exact;
            b -= error / (1 - inv * t);
            inv = inverse_error_reference(b);
        }
        slope_x = inv;
    }
    float x = cos(phi) * slope_x - sin(phi) * slope_y;
    float z = sin(phi) * slope_x + cos(phi) * slope_y;
    return normalize(vector(-ax * x, 1, -ay * z));
}

float beckmann_lambda(vector w, float ax, float ay)
{
    float projected = ax * ax * w[0] * w[0] + ay * ay * w[2] * w[2];
    if (projected == 0)
        return 0;
    float a = abs(w[1]) / sqrt(projected);
    if (a >= 1.6)
        return 0;
    return (1 - 1.259 * a + 0.396 * a * a) / (3.535 * a + 2.181 * a * a);
}

#endif
