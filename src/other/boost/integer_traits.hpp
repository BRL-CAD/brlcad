/* boost integer_traits.hpp header file
 *
 * Copyright Jens Maurer 2000
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE_1_0.txt or copy at
 * http://www.boost.org/LICENSE_1_0.txt)
 *
 * $Id$
 *
 * Idea by Beman Dawes, Ed Brey, Steve Cleary, and Nathan Myers
 */

//  See http://www.boost.org/libs/integer for documentation.


#ifndef BOOST_INTEGER_TRAITS_HPP
#define BOOST_INTEGER_TRAITS_HPP

#include <boost/config.hpp>
#include <boost/limits.hpp>

// These are an implementation detail and not part of the interface
#include <limits.h>
// we need wchar.h for WCHAR_MAX/MIN but not all platforms provide it,
// and some may have <wchar.h> but not <cwchar> ...
#if !defined(BOOST_NO_INTRINSIC_WCHAR_T) && (!defined(BOOST_NO_CWCHAR) || defined(sun) || defined(__sun) || defined(__QNX__))
#include <wchar.h>
#endif

#include <boost/detail/extended_integer.hpp>  // for BOOST_HAS_XINT, etc.


namespace boost {
template<class T>
class integer_traits : public std::numeric_limits<T>
{
public:
  BOOST_STATIC_CONSTANT(bool, is_integral = false);
};

namespace detail {
template<class T, T min_val, T max_val>
class integer_traits_base
{
public:
  BOOST_STATIC_CONSTANT(bool, is_integral = true);
  BOOST_STATIC_CONSTANT(T, const_min = min_val);
  BOOST_STATIC_CONSTANT(T, const_max = max_val);
};

#ifndef BOOST_NO_INCLASS_MEMBER_INITIALIZATION
//  A definition is required even for integral static constants
template<class T, T min_val, T max_val>
const bool integer_traits_base<T, min_val, max_val>::is_integral;

template<class T, T min_val, T max_val>
const T integer_traits_base<T, min_val, max_val>::const_min;

template<class T, T min_val, T max_val>
const T integer_traits_base<T, min_val, max_val>::const_max;
#endif

} // namespace detail

template<>
class integer_traits<bool>
  : public std::numeric_limits<bool>,
    public detail::integer_traits_base<bool, false, true>
{ };

template<>
class integer_traits<char>
  : public std::numeric_limits<char>,
    public detail::integer_traits_base<char, CHAR_MIN, CHAR_MAX>
{ };

template<>
class integer_traits<signed char>
  : public std::numeric_limits<signed char>,
    public detail::integer_traits_base<signed char, SCHAR_MIN, SCHAR_MAX>
{ };

template<>
class integer_traits<unsigned char>
  : public std::numeric_limits<unsigned char>,
    public detail::integer_traits_base<unsigned char, 0, UCHAR_MAX>
{ };

#ifndef BOOST_NO_INTRINSIC_WCHAR_T
template<>
class integer_traits<wchar_t>
  : public std::numeric_limits<wchar_t>,
    // Don't trust WCHAR_MIN and WCHAR_MAX with Mac OS X's native
    // library: they are wrong!
#if defined(WCHAR_MIN) && defined(WCHAR_MAX) && !defined(__APPLE__)
    public detail::integer_traits_base<wchar_t, WCHAR_MIN, WCHAR_MAX>
#elif defined(__BORLANDC__) || defined(__CYGWIN__) || defined(__MINGW32__) || (defined(__BEOS__) && defined(__GNUC__))
    // No WCHAR_MIN and WCHAR_MAX, whar_t is short and unsigned:
    public detail::integer_traits_base<wchar_t, 0, 0xffff>
#elif (defined(__sgi) && (!defined(__SGI_STL_PORT) || __SGI_STL_PORT < 0x400))\
    || (defined __APPLE__)\
    || (defined(__OpenBSD__) && defined(__GNUC__))\
    || (defined(__NetBSD__) && defined(__GNUC__))\
    || (defined(__FreeBSD__) && defined(__GNUC__))\
    || (defined(__DragonFly__) && defined(__GNUC__))\
    || (defined(__hpux) && defined(__GNUC__) && (__GNUC__ == 3) && !defined(__SGI_STL_PORT))
    // No WCHAR_MIN and WCHAR_MAX, wchar_t has the same range as int.
    //  - SGI MIPSpro with native library
    //  - gcc 3.x on HP-UX
    //  - Mac OS X with native library
    //  - gcc on FreeBSD, OpenBSD and NetBSD
    public detail::integer_traits_base<wchar_t, INT_MIN, INT_MAX>
#elif defined(__hpux) && defined(__GNUC__) && (__GNUC__ == 2) && !defined(__SGI_STL_PORT)
    // No WCHAR_MIN and WCHAR_MAX, wchar_t has the same range as unsigned int.
    //  - gcc 2.95.x on HP-UX
    // (also, std::numeric_limits<wchar_t> appears to return the wrong values).
    public detail::integer_traits_base<wchar_t, 0, UINT_MAX>
#else
#error No WCHAR_MIN and WCHAR_MAX present, please adjust integer_traits<> for your compiler.
#endif
{ };
#endif // BOOST_NO_INTRINSIC_WCHAR_T

template<>
class integer_traits<short>
  : public std::numeric_limits<short>,
    public detail::integer_traits_base<short, SHRT_MIN, SHRT_MAX>
{ };

template<>
class integer_traits<unsigned short>
  : public std::numeric_limits<unsigned short>,
    public detail::integer_traits_base<unsigned short, 0, USHRT_MAX>
{ };

template<>
class integer_traits<int>
  : public std::numeric_limits<int>,
    public detail::integer_traits_base<int, INT_MIN, INT_MAX>
{ };

template<>
class integer_traits<unsigned int>
  : public std::numeric_limits<unsigned int>,
    public detail::integer_traits_base<unsigned int, 0, UINT_MAX>
{ };

template<>
class integer_traits<long>
  : public std::numeric_limits<long>,
    public detail::integer_traits_base<long, LONG_MIN, LONG_MAX>
{ };

template<>
class integer_traits<unsigned long>
  : public std::numeric_limits<unsigned long>,
    public detail::integer_traits_base<unsigned long, 0, ULONG_MAX>
{ };

#if !defined(BOOST_NO_INTEGRAL_INT64_T) && !defined(BOOST_NO_INT64_T) && BOOST_HAS_XINT
template<>
class integer_traits< detail::xint_t >
  : public std::numeric_limits< detail::xint_t >,
    public detail::integer_traits_base< detail::xint_t, BOOST_XINT_MIN, BOOST_XINT_MAX >
{ };

template<>
class integer_traits< detail::uxint_t >
  : public std::numeric_limits< detail::uxint_t >,
    public detail::integer_traits_base< detail::uxint_t, 0u, BOOST_UXINT_MAX >
{ };
#endif

} // namespace boost

#endif /* BOOST_INTEGER_TRAITS_HPP */



