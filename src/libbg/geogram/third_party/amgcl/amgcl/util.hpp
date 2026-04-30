#ifndef AMGCL_UTIL_HPP
#define AMGCL_UTIL_HPP

/*
The MIT License

Copyright (c) 2012-2022 Denis Demidov <dennis.demidov@gmail.com>

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/

/**
 * \file   amgcl/util.hpp
 * \author Denis Demidov <dennis.demidov@gmail.com>
 * \brief  Various utilities.
 */

#include <iostream>
#include <iomanip>
#include <iterator>
#include <vector>
#include <array>
#include <string>
#include <set>
#include <complex>
#include <limits>
#include <stdexcept>
#include <cstddef>

/* Performance measurement macros
 *
 * If AMGCL_PROFILING macro is defined at compilation, then AMGCL_TIC(name) and
 * AMGCL_TOC(name) macros correspond to prof.tic(name) and prof.toc(name).
 * amgcl::prof should be an instance of amgcl::profiler<> defined in a user
 * code similar to:
 * \code
 * namespace amgcl { profiler<> prof; }
 * \endcode
 * If AMGCL_PROFILING is undefined, then AMGCL_TIC and AMGCL_TOC are noop macros.
 */
#  ifndef AMGCL_TIC
#    define AMGCL_TIC(name)
#  endif
#  ifndef AMGCL_TOC
#    define AMGCL_TOC(name)
#  endif

#define AMGCL_DEBUG_SHOW(x)                                                    \
    std::cout << std::setw(20) << #x << ": "                                   \
              << std::setw(15) << std::setprecision(8) << std::scientific      \
              << (x) << std::endl

namespace amgcl {

/// Throws \p message if \p condition is not true.
template <class Condition, class Message>
void precondition(const Condition &condition, const Message &message) {
#ifdef _MSC_VER
#  pragma warning(push)
#  pragma warning(disable: 4800)
#endif
    if (!condition) throw std::runtime_error(message);
#ifdef _MSC_VER
#  pragma warning(pop)
#endif
}


namespace detail {


struct empty_params {
    empty_params() {}

};

} // namespace detail

// Iterator range
template <class Iterator>
class iterator_range {
    public:
        typedef Iterator iterator;
        typedef Iterator const_iterator;
        typedef typename std::iterator_traits<Iterator>::value_type value_type;
        typedef typename std::iterator_traits<Iterator>::reference reference;

        iterator_range(Iterator b, Iterator e)
            : b(b), e(e) {}

        ptrdiff_t size() const {
            return std::distance(b, e);
        }

        Iterator begin() const {
            return b;
        }

        Iterator end() const {
            return e;
        }

        reference operator[](size_t i) const {
            return b[i];
        }
    private:
        Iterator b, e;
};

template <class Iterator>
iterator_range<Iterator> make_iterator_range(Iterator b, Iterator e) {
    return iterator_range<Iterator>(b, e);
}

// N-dimensional dense matrix
template <class T, int N>
class multi_array {
    static_assert(N > 0, "Wrong number of dimensions");

    public:
        template <class... I>
        multi_array(I... n) {
            static_assert(sizeof...(I) == N, "Wrong number of dimensions");
            buf.resize(init(n...));
        }

        size_t size() const {
            return buf.size();
        }

        int stride(int i) const {
            return strides[i];
        }

        template <class... I>
        T operator()(I... i) const {
            static_assert(sizeof...(I) == N, "Wrong number of indices");
            return buf[index(i...)];
        }

        template <class... I>
        T& operator()(I... i) {
            static_assert(sizeof...(I) == N, "Wrong number of indices");
            return buf[index(i...)];
        }

        const T* data() const {
            return buf.data();
        }

        T* data() {
            return buf.data();
        }
    private:
        std::array<int, N> strides;
        std::vector<T>  buf;

        template <class... I>
        int index(int i, I... tail) const {
            return strides[N - sizeof...(I) - 1] * i + index(tail...);
        }

        int index(int i) const {
            return strides[N-1] * i;
        }

        template <class... I>
        int init(int i, I... tail) {
            int size = init(tail...);
            strides[N - sizeof...(I) - 1] = size;
            return i * size;
        }

        int init(int i) {
            strides[N-1] = 1;
            return i;
        }
};

template <class T>
class circular_buffer {
    public:
        circular_buffer(size_t n) : start(0) {
            buf.reserve(n);
        }

        size_t size() const {
            return buf.size();
        }

        void push_back(const T &v) {
            if (buf.size() < buf.capacity()) {
                buf.push_back(v);
            } else {
                buf[start] = v;
                start = (start + 1) % buf.capacity();
            }
        }

        const T& operator[](size_t i) const {
            return buf[(start + i) % buf.capacity()];
        }

        T& operator[](size_t i) {
            return buf[(start + i) % buf.capacity()];
        }

        void clear() {
            buf.clear();
            start = 0;
        }

    private:
        size_t start;
        std::vector<T> buf;
};


namespace detail {

template <class T>
T eps(size_t n) {
    return 2 * std::numeric_limits<T>::epsilon() * n;
}

} // namespace detail

template <class T> struct is_complex : std::false_type {};
template <class T> struct is_complex< std::complex<T> > : std::true_type {};

inline std::string human_readable_memory(size_t bytes) {
    static const char *suffix[] = {"B", "K", "M", "G", "T"};

    int i = 0;
    double m = static_cast<double>(bytes);
    for(; i < 4 && m >= 1024.0; ++i, m /= 1024.0);

    std::ostringstream s;
    s << std::fixed << std::setprecision(2) << m << " " << suffix[i];
    return s.str();
}

namespace detail {

class non_copyable {
    protected:
        non_copyable() = default;
        ~non_copyable() = default;

        non_copyable(non_copyable const &) = delete;
        void operator=(non_copyable const &x) = delete;
};

} // namespace detail

namespace error {

struct empty_level {};

} // namespace error
} // namespace amgcl

#endif
