/*
 * OpenGL Mathematics (GLM)
 * --------------------------------------------------------------------------------
 * GLM is licensed under The Happy Bunny License or MIT License
 *
 * BRL-CAD will use glm under the standard MIT license
 *
 * ================================================================================
 * The MIT License
 * --------------------------------------------------------------------------------
 * Copyright (c) 2005 - G-Truc Creation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#pragma once

#include "setup.hpp"

#if GLM_COMPILER == GLM_COMPILER_VC12
#	pragma warning(push)
#	pragma warning(disable: 4512) // assignment operator could not be generated
#endif

namespace glm{
namespace detail
{
	template <typename T>
	union float_t
	{};

	// https://randomascii.wordpress.com/2012/02/25/comparing-floating-point-numbers-2012-edition/
	template <>
	union float_t<float>
	{
		typedef int int_type;
		typedef float float_type;

		GLM_CONSTEXPR float_t(float_type Num = 0.0f) : f(Num) {}

		GLM_CONSTEXPR float_t& operator=(float_t const& x)
		{
			f = x.f;
			return *this;
		}

		// Portable extraction of components.
		GLM_CONSTEXPR bool negative() const { return i < 0; }
		GLM_CONSTEXPR int_type mantissa() const { return i & ((1 << 23) - 1); }
		GLM_CONSTEXPR int_type exponent() const { return (i >> 23) & ((1 << 8) - 1); }

		int_type i;
		float_type f;
	};

	template <>
	union float_t<double>
	{
		typedef detail::int64 int_type;
		typedef double float_type;

		GLM_CONSTEXPR float_t(float_type Num = static_cast<float_type>(0)) : f(Num) {}

		GLM_CONSTEXPR float_t& operator=(float_t const& x)
		{
			f = x.f;
			return *this;
		}

		// Portable extraction of components.
		GLM_CONSTEXPR bool negative() const { return i < 0; }
		GLM_CONSTEXPR int_type mantissa() const { return i & ((int_type(1) << 52) - 1); }
		GLM_CONSTEXPR int_type exponent() const { return (i >> 52) & ((int_type(1) << 11) - 1); }

		int_type i;
		float_type f;
	};
}//namespace detail
}//namespace glm

#if GLM_COMPILER == GLM_COMPILER_VC12
#	pragma warning(pop)
#endif
