// MIT License
//
// Copyright (c) 2021 Samuel Bear Powell
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
//
// https://github.com/s-bear/image-hash

#include "PImgHash.h"

#include <algorithm>
#include <bitset>

#ifdef max
#undef max
#endif
#ifdef min
#undef min
#endif

namespace imghash
{

template<> uint8_t convert_pix<uint8_t>(uint8_t p)
{
    return p;
}
template<> float convert_pix<float>(uint8_t p)
{
    return static_cast<float>(p) / 255.0f;
}
template<> float convert_pix<float>(float p)
{
    return p;
}
template<> uint8_t convert_pix<uint8_t>(float p)
{
    return static_cast<uint8_t>(p * 255.9999f); //we could use nextafter(256, 0) but it might not be optimized away
}

Hasher::Hasher() : bytes(), bi(8) {}

void Hasher::clear()
{
    bytes.clear();
    bi = 8;
}

void Hasher::append_bit(bool b)
{
    if (bi > 7) {
	bytes.push_back(0);
	bi = 0;
    }
    if (b) {
	bytes.back() |= (uint8_t(1) << bi);
    }
    ++bi;
}

uint32_t Hasher::hamming_distance(const hash_type& h1, const hash_type& h2)
{
    //NB we only look at bytes in common
    size_t n = std::min(h1.size(), h2.size());
    size_t d = 0;
    for (size_t i = 0; i < n; ++i) {
	d += std::bitset<8*sizeof(hash_type::value_type)>(h1[i] ^ h2[i]).count();
    }
    return static_cast<uint32_t>(d);
}

DCTHasher::DCTHasher(unsigned M, bool even)
    : N_(128), M_(M), even_(even), m_(mat(N_, M_, even_))
{
    //nothing to do
}

std::vector<float> DCTHasher::mat(unsigned N, unsigned M)
{
    if (M > N) M = N;
    std::vector<float> m;
    if (M <= 1) return m;
    //column-major order!
    m.reserve(static_cast<size_t>(N) * M);
    for (unsigned j = 0; j < N; ++j) {
	for (unsigned i = 0; i < M; ++i) {
	    m.push_back(coef(N, i+1, j));
	}
    }
    return m;
}

std::vector<float> DCTHasher::mat_even(unsigned N, unsigned M)
{
    if (M > N/2) M = N/2;
    std::vector<float> m;
    if (M <= 1) return m;
    //column-major order!
    m.reserve(static_cast<size_t>(N) * M);
    for (unsigned j = 0; j < N; ++j) {
	for (unsigned i = 0; i < M; ++i) {
	    m.push_back(coef(N, 2*(i+1), j));
	}
    }
    return m;
}

std::vector<float> DCTHasher::mat(unsigned N, unsigned M, bool even)
{
    if (even) return mat_even(N, M);
    else return mat(N, M);
}

std::vector<uint8_t> DCTHasher::apply(const Image<float>& image)
{
    if (image.width != image.height || image.channels != 1) {
	throw std::runtime_error("DCT: image must be square and single-channel");
    }
    if (N_ != image.width) {
	N_ = static_cast<unsigned>(image.width);
	m_ = mat(N_, M_, even_);
    }
    unsigned M = M_;
    if (even_) {
	if (M > N_ / 2) M = N_ / 2;
    } else if (M > N_) {
	M = N_;
    }
    if (M <= 1) {
	clear();
	return bytes;
    }

    /* Phase 1: Apply DCT across rows */
    Image<float> dct_1(image.height, M);

    //iterate over image rows
    for (size_t y = 0, ti = 0, di = 0;
	 y < image.height;
	 ++y, ti += image.row_size, di += dct_1.row_size) {
	//init dct
	for (size_t u = 0, dj = di; u < dct_1.width; ++u, ++dj) {
	    dct_1[dj] = 0.0f;
	}

	//iterate over image columns (reduction)
	for (size_t x = 0, tj = ti, k = 0;
	     x < image.width;
	     ++x, ++tj) {
	    //iterate over horizontal spatial frequencies
	    float p = image[tj];
	    for (size_t u = 0, dj = di; u < dct_1.width; ++u, ++k, ++dj) {
		dct_1[dj] += m_[k] * p;
	    }
	}
    }

    /* Phase 2: Apply DCT along columns */
    Image<float> dct(M, M);
    //iterate over vertical spatial frequencies
    for (size_t v = 0, i = 0; v < M; ++v, i += dct.row_size) {
	//iterate over horizontal spatial frequencies
	for (size_t u = 0, j = i; u < M; ++u, ++j) {
	    //reduce over image rows
	    float dct_uv = 0.0f;
	    for (size_t y = 0, k = v, di = u; y < N_; ++y, k += M, di += M) {
		dct_uv += m_[k] * dct_1[di];
	    }
	    dct[j] = dct_uv;
	}
    }

    /* Phase 3: Compute hash */
    clear();
    bytes.reserve((size_t(M) * M + 7) / 8);
    //iterate over the DCT so that we always output the bits in the same order, no matter the size
    // we will start in the corner, and then build up in square shells:
    // 0 1 4
    // 2 3 5
    // 6 7 8

    //iterate across the first row
    for (size_t u = 0; u < M; ++u) {
	//iterate down the column at u, to the (u-1) row
	size_t i = 0;
	for (size_t v = 0; v < u; ++v, i += dct.row_size) {
	    append_bit(dct[i + u] > 0);
	}
	//iterate across row v, to column u
	for (size_t uu = 0, j = i; uu < u + 1; ++uu, ++j) {
	    append_bit(dct[j] > 0);
	}
    }

    return bytes;
}

std::vector<size_t> tile_size(size_t a, size_t b)
{
    // a > b
    //Use modified Bresenham's algorithm to distribute b groups over a items

    intptr_t D = intptr_t(b) - intptr_t(a); //the usual algorithm uses b - 2*a, but that reduces the size of the first and last bins by half
    std::vector<size_t> sizes(b, 0);
    for (size_t i = 0, j = 0; i < a; ++i) {
	sizes[j]++;
	if (D > 0) {
	    ++j;
	    D += intptr_t(b) - intptr_t(a);
	} else {
	    D += intptr_t(b);
	}
    }
    return sizes;
}

Preprocess::Preprocess(size_t w, size_t h)
    : img(h,w,3), hist(), y(0), i(0), ty(0), in_w(0), in_h(0), in_c(0)
{
    //nothing else to do
}

void Preprocess::start(size_t input_height, size_t input_width, size_t input_channels)
{
    in_w = input_width;
    in_h = input_height;
    in_c = input_channels;

    if (img.height > in_h) tile_h = tile_size(img.height, in_h);
    else if (in_h> img.height) tile_h = tile_size(in_h, img.height);

    if (img.width > in_w) tile_w = tile_size(img.width, in_w);
    else if (in_w> img.width) tile_w = tile_size(in_w, img.width);

    if (hist.size() != in_c * 256) {
	hist.resize(in_c * 256);
    }
    std::fill(hist.begin(), hist.end(), 0);

    if (img.channels != in_c) {
	img = Image<float>(img.height, img.width, in_c);
    }
    y = 0;
    i = 0;
    ty = 0;
}

Image<float> Preprocess::stop()
{
    //equalization lookup table
    // cumulative sum of the normalized histogram
    std::vector<float> lut;
    lut.reserve(hist.size());
    size_t in_count = in_c * in_w * in_h;
    for (size_t c = 0, j = 0; c < in_c; ++c) {
	size_t sum = 0;
	for (size_t ip = 0; ip < hist_bins; ++ip, ++j) {
	    sum += hist[j];
	    lut.push_back(float(sum) / in_count);
	}
    }

    Image<float> out(img.height, img.width, 1);
    //apply the equalization, storing the result in out
    for (size_t out_y = 0, out_i = 0, img_i = 0;
	 out_y < out.height;
	 ++out_y, out_i += out.row_size, img_i += img.row_size) {
	for (size_t out_x = 0, out_j = out_i, img_j = img_i; out_x < out.width; ++out_x, ++out_j) {
	    float sum = 0.0f;
	    for (size_t c = 0; c < img.channels; ++c, ++img_j) {
		auto p = img[img_j];
		sum += lut[c * hist_bins + convert_pix<uint8_t>(p)];
	    }
	    out[out_j] = sum;
	}
    }
    return out;
}

}




// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
