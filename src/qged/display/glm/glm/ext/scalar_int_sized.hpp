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

/// @ref ext_scalar_int_sized
/// @file glm/ext/scalar_int_sized.hpp
///
/// @defgroup ext_scalar_int_sized GLM_EXT_scalar_int_sized
/// @ingroup ext
///
/// Exposes sized signed integer scalar types.
///
/// Include <glm/ext/scalar_int_sized.hpp> to use the features of this extension.
///
/// @see ext_scalar_uint_sized

#pragma once

#include "../detail/setup.hpp"

#if GLM_MESSAGES == GLM_ENABLE && !defined(GLM_EXT_INCLUDED)
#	pragma message("GLM: GLM_EXT_scalar_int_sized extension included")
#endif

namespace glm{
namespace detail
{
#	if GLM_HAS_EXTENDED_INTEGER_TYPE
		typedef std::int8_t			int8;
		typedef std::int16_t		int16;
		typedef std::int32_t		int32;
#	else
		typedef signed char			int8;
		typedef signed short		int16;
		typedef signed int			int32;
#endif//

	template<>
	struct is_int<int8>
	{
		enum test {value = ~0};
	};

	template<>
	struct is_int<int16>
	{
		enum test {value = ~0};
	};

	template<>
	struct is_int<int64>
	{
		enum test {value = ~0};
	};
}//namespace detail


	/// @addtogroup ext_scalar_int_sized
	/// @{

	/// 8 bit signed integer type.
	typedef detail::int8		int8;

	/// 16 bit signed integer type.
	typedef detail::int16		int16;

	/// 32 bit signed integer type.
	typedef detail::int32		int32;

	/// 64 bit signed integer type.
	typedef detail::int64		int64;

	/// @}
}//namespace glm
