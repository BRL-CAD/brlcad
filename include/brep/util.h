/*                      U T I L . H
 * BRL-CAD
 *
 * Copyright (c) 2004-2022 United States Government as represented by
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
/** @{ */
/** @file brep/util.h */
/** @addtogroup brep_util
 *
 * @brief
 * These are utility routines for libbrep, used throughout the library.
 *
 */

#ifndef BREP_UTIL_H
#define BREP_UTIL_H

#include "common.h"

#include "bnetwork.h" /* Needed for ntohl and htonl */

#ifdef __cplusplus
// @cond SKIP_C++_INCLUDE
extern "C++" {
#  include <cstring>
}
// @endcond
#endif


#include "bu/cv.h"
#include "bu/endian.h"
#include "bu/log.h"
#include "bu/malloc.h"
#include "bu/parse.h"

#include "brep/defines.h"

__BEGIN_DECLS

#ifdef __cplusplus
extern "C++" {

extern BREP_EXPORT void ON_BoundingBox_Plot(FILE *pf, ON_BoundingBox &bb);
extern BREP_EXPORT ON_3dPoint ON_LinePlaneIntersect(ON_Line &line, ON_Plane &plane);
extern BREP_EXPORT void ON_Plane_Plot(FILE *pf, ON_Plane &plane);
extern BREP_EXPORT void ON_MinMaxInit(ON_3dPoint *min, ON_3dPoint *max);
extern BREP_EXPORT bool ON_NearZero(double x, double tolerance = ON_ZERO_TOLERANCE);

extern BREP_EXPORT bool face_GetBoundingBox(const ON_BrepFace &face,ON_BoundingBox& bbox,bool bGrowBox);
extern BREP_EXPORT bool surface_GetBoundingBox(const ON_Surface *surf,const ON_Interval &u_interval,const ON_Interval &v_interval,ON_BoundingBox& bbox,bool bGrowBox);
extern BREP_EXPORT bool surface_EvNormal(const ON_Surface *surf,double s,double t,ON_3dPoint& point,ON_3dVector& normal,int side=0,int* hint=0);

extern BREP_EXPORT ON_Curve *interpolateCurve(ON_2dPointArray &samples);
extern BREP_EXPORT ON_NurbsCurve *
interpolateLocalCubicCurve(const ON_3dPointArray &Q);

extern BREP_EXPORT int
ON_Curve_PolyLine_Approx(ON_Polyline *polyline, const ON_Curve *curve, double tol);

extern BREP_EXPORT ON_Curve*
ON_Surface_Pushup(
	const ON_Surface *s,
	const ON_Curve& curve_2d,
	const ON_Interval* curve_2d_subdomain
	);

/* Experimental function to generate Tikz plotting information
 * from B-Rep objects.  This may or may not be something we
 * expose as a feature long term - probably should be a more
 * generic API that supports multiple formats... */
extern BREP_EXPORT int
ON_BrepTikz(ON_String &s, const ON_Brep *brep, const char *color, const char *prefix);

/**
 * Get the curve segment between param a and param b
 *
 * @param in [in] the curve to split
 * @param a  [in] either a or b can be the larger one
 * @param b  [in] either a or b can be the larger one
 *
 * @return the result curve segment. NULL for error.
 */
extern BREP_EXPORT ON_Curve *
sub_curve(const ON_Curve *in, double a, double b);

/**
 * Get the sub-surface whose u in [a,b] or v in [a, b]
 *
 * @param in [in] the surface to split
 * @param dir [in] 0: u-split, 1: v-split
 * @param a [in] either a or b can be the larger one
 * @param b [in] either a or b can be the larger one
 *
 * @return the result sub-surface. NULL for error.
 */
extern BREP_EXPORT ON_Surface *
sub_surface(const ON_Surface *in, int dir, double a, double b);


BREP_EXPORT void set_key(struct bu_vls *key, int k, int *karray);


class Serializer
{
public:
Serializer() :
    m_capacity(1024 * 1024),
    m_external()
    {
	BU_EXTERNAL_INIT(&m_external);

	m_external.ext_buf = static_cast<uint8_t *>(bu_malloc(m_capacity, "m_external"));
    }


    ~Serializer()
    {
	bu_free_external(&m_external);
    }


    bu_external take()
    {
	const bu_external result = m_external;
	m_capacity = 1024 * 1024;
	BU_EXTERNAL_INIT(&m_external);
	m_external.ext_buf = static_cast<uint8_t *>(bu_malloc(m_capacity, "m_external"));
	return result;
    }


    void write_uint8(uint8_t value)
    {
	uint8_t * const dest = extend(sizeof(value));
	*dest = value;
    }


    void write_uint32(uint32_t value)
    {
	uint8_t * const dest = extend(SIZEOF_NETWORK_LONG);
	const uint32_t out_value = htonl(value);
	std::memcpy(dest, &out_value, SIZEOF_NETWORK_LONG);
    }


    void write_int32(int32_t value)
    {
	uint8_t * const dest = extend(SIZEOF_NETWORK_LONG);
	long out_value = value;

	if (UNLIKELY(1 != bu_cv_htonsl(dest, SIZEOF_NETWORK_LONG, &out_value, 1)))
	    bu_bomb("bu_cv_htonsl() failed");
    }


    void write_double(double value)
    {
	uint8_t * const dest = extend(SIZEOF_NETWORK_DOUBLE);
	bu_cv_htond(dest, reinterpret_cast<const unsigned char *>(&value), 1);
    }


    void write(const ON_3dPoint &value)
    {
	write_double(value.x);
	write_double(value.y);
	write_double(value.z);
    }


    void write(const ON_BoundingBox &value)
    {
	write(value.m_min);
	write(value.m_max);
    }


    void write(const ON_Interval &value)
    {
	write_double(value.m_t[0]);
	write_double(value.m_t[1]);
    }


private:
    Serializer(const Serializer &source);
    Serializer &operator=(const Serializer &source);


    uint8_t *extend(std::size_t size)
    {
	const std::size_t position = m_external.ext_nbytes;
	m_external.ext_nbytes += size;

	if (m_external.ext_nbytes > m_capacity) {
	    m_capacity = m_external.ext_nbytes * 2;
	    m_external.ext_buf = static_cast<uint8_t *>(bu_realloc(m_external.ext_buf, m_capacity, "m_external"));
	}

	return &m_external.ext_buf[position];
    }


    std::size_t m_capacity;
    bu_external m_external;
};


class Deserializer
{
public:
Deserializer(const bu_external &external) :
    m_position(0),
    m_external(external)
    {
	BU_CK_EXTERNAL(&m_external);
    }


    ~Deserializer()
    {
	if (m_position != m_external.ext_nbytes)
	    bu_bomb("did not deserialize entire stream");
    }


    uint8_t read_uint8()
    {
	return *get(1);
    }


    uint32_t read_uint32()
    {
	uint32_t result;
	std::memcpy(&result, get(SIZEOF_NETWORK_LONG), SIZEOF_NETWORK_LONG);
	return ntohl(result);
    }


    int32_t read_int32()
    {
	long result;

	if (UNLIKELY(1 != bu_cv_ntohsl(&result, sizeof(result), const_cast<void *>(reinterpret_cast<const void *>(get(SIZEOF_NETWORK_LONG))), 1)))
	    bu_bomb("bu_cv_ntohsl() failed");

	return result;
    }


    double read_double()
    {
	double result;
	bu_cv_ntohd(reinterpret_cast<unsigned char *>(&result), get(SIZEOF_NETWORK_DOUBLE), 1);
	return result;
    }


    void read(ON_3dPoint &value)
    {
	value.x = read_double();
	value.y = read_double();
	value.z = read_double();
    }


    void read(ON_3dVector &value)
    {
	value.x = read_double();
	value.y = read_double();
	value.z = read_double();
    }


    void read(ON_BoundingBox &value)
    {
	read(value.m_min);
	read(value.m_max);
    }


    void read(ON_Interval &value)
    {
	value.m_t[0] = read_double();
	value.m_t[1] = read_double();
    }


private:
    Deserializer(const Deserializer &source);
    Deserializer &operator=(const Deserializer &source);


    const uint8_t *get(std::size_t size)
    {
	const std::size_t result_position = m_position;
	m_position += size;

	if (UNLIKELY(m_position > m_external.ext_nbytes))
	    bu_bomb("invalid position");

	return &m_external.ext_buf[result_position];
    }


    std::size_t m_position;
    const bu_external &m_external;
};


template <typename T>
class PooledObject
{
public:
    static void *operator new(std::size_t size)
    {
	if (UNLIKELY(size != sizeof(T)))
	    throw std::bad_alloc();

	return bu_heap_get(size);
    }


    static void operator delete(void *pointer)
    {
	bu_heap_put(pointer, sizeof(T));
    }
};

// Moved from librt - placed here until a better location is found
BREP_EXPORT int
brep_translate_scv(
        ON_Brep *brep,
        int surface_index,
        int i,
        int j,
        fastf_t dx,
        fastf_t dy,
        fastf_t dz);

} /* extern C++ */

__END_DECLS

#endif /* __cplusplus */

/** @} */

#endif  /* BREP_UTIL_H */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
