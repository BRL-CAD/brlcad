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

/// @ref ext_scalar_uint_sized
/// @file glm/ext/scalar_uint_sized.hpp
///
/// @defgroup ext_scalar_uint_sized GLM_EXT_scalar_uint_sized
/// @ingroup ext
///
/// Exposes sized unsigned integer scalar types.
///
/// Include <glm/ext/scalar_uint_sized.hpp> to use the features of this extension.
///
/// @see ext_scalar_int_sized

#pragma once

#include "../detail/setup.hpp"

#if GLM_MESSAGES == GLM_ENABLE && !defined(GLM_EXT_INCLUDED)
#	pragma message("GLM: GLM_EXT_scalar_uint_sized extension included")
#endif

namespace glm{
namespace detail
{
#	if GLM_HAS_EXTENDED_INTEGER_TYPE
		typedef std::uint8_t		uint8;
		typedef std::uint16_t		uint16;
		typedef std::uint32_t		uint32;
#	else
		typedef unsigned char		uint8;
		typedef unsigned short		uint16;
		typedef unsigned int		uint32;
#endif

	template<>
	struct is_int<uint8>
	{
		enum test {value = ~0};
	};

	template<>
	struct is_int<uint16>
	{
		enum test {value = ~0};
	};

	template<>
	struct is_int<uint64>
	{
		enum test {value = ~0};
	};
}//namespace detail


	/// @addtogroup ext_scalar_uint_sized
	/// @{

	/// 8 bit unsigned integer type.
	typedef detail::uint8		uint8;

	/// 16 bit unsigned integer type.
	typedef detail::uint16		uint16;

	/// 32 bit unsigned integer type.
	typedef detail::uint32		uint32;

	/// 64 bit unsigned integer type.
	typedef detail::uint64		uint64;

	/// @}
}//namespace glm
