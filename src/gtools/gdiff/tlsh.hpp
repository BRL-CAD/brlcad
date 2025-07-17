// Original code from https://github.com/trendmicro/tlsh
//
// TLSH - Trend Micro Locality Sensitive Hash
//
// =====================
// LICENSE OPTION NOTICE
// =====================
// TLSH is provided for use under two licenses: Apache OR BSD.  Users may opt
// to use either license depending on the license restrictions of the systems
// with which they plan to integrate the TLSH code.
//
// BRL-CAD opts for the BSD license:
//
// Trend Locality Sensitive Hash (TLSH)
// Copyright 2010-2014 Trend Micro
//
// BSD License Version 3
//                    https://opensource.org/licenses/BSD-3-Clause
//
// TERMS AND CONDITIONS FOR USE, REPRODUCTION, AND DISTRIBUTION
//
// Redistribution and use in source and binary forms, with or without modification,
// are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice, this
//    list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
//
// 3. Neither the name of the copyright holder nor the names of its contributors
//    may be used to endorse or promote products derived from this software without
//    specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
// ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
// WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
// IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
// INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
// BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
// LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
// OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
// OF THE POSSIBILITY OF SUCH DAMAGE.
//
// This version was processed with Github Copilot GPT 4.1 to provide a
// header-only version of the core hashing methods.
// July 2025 Clifford Yapp

#pragma once
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <cassert>
#include <string>
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <stdexcept>

namespace tlsh {

constexpr std::size_t BUCKETS = 256; // increased to 256 for fast_b_mapping safety
constexpr std::size_t CODE_SIZE = 32;
constexpr std::size_t SLIDING_WND_SIZE = 5;
constexpr std::size_t SLIDING_WND_SIZE_M1 = SLIDING_WND_SIZE - 1;
constexpr std::size_t TLSH_CHECKSUM_LEN = 1;
constexpr std::size_t TLSH_STRING_LEN_REQ = 70;
constexpr std::size_t MIN_DATA_LENGTH = 50;
constexpr std::size_t RANGE_LVALUE = 256;
constexpr std::size_t RANGE_QRATIO = 16;

inline uint8_t v_table[256] = {
    1,87,49,12,176,178,102,166,121,193,6,84,249,230,44,163,
    14,197,213,181,161,85,218,80,64,239,24,226,236,142,38,200,
    110,177,104,103,141,253,255,50,77,101,81,18,45,96,31,222,
    25,107,190,70,86,237,240,34,72,242,20,214,244,227,149,235,
    97,234,57,22,60,250,82,175,208,5,127,199,111,62,135,248,
    174,169,211,58,66,154,106,195,245,171,17,187,182,179,0,243,
    132,56,148,75,128,133,158,100,130,126,91,13,153,246,216,219,
    119,68,223,78,83,88,201,99,122,11,92,32,136,114,52,10,
    138,30,48,183,156,35,61,26,143,74,251,94,129,162,63,152,
    170,7,115,167,241,206,3,150,55,59,151,220,90,53,23,131,
    125,173,15,238,79,95,89,16,105,137,225,224,217,160,37,123,
    118,73,2,157,46,116,9,145,134,228,207,212,202,215,69,229,
    27,188,67,124,168,252,42,4,29,108,21,247,19,205,39,203,
    233,40,186,147,198,192,155,33,164,191,98,204,165,180,117,76,
    140,36,210,172,41,54,159,8,185,232,113,196,231,47,146,120,
    51,65,28,144,254,221,93,189,194,139,112,43,71,109,184,209
};

inline uint8_t fast_b_mapping(uint8_t ms, uint8_t i, uint8_t j, uint8_t k) {
    return v_table[v_table[v_table[ms ^ i] ^ j] ^ k];
}

// Corrected mod_diff to accept std::size_t R, and all calls use RANGE_LVALUE without narrowing cast
inline int mod_diff(uint8_t x, uint8_t y, std::size_t R) {
    int d = static_cast<int>(x) - static_cast<int>(y);
    if (d < 0) d += static_cast<int>(R);
    if (d > static_cast<int>(R) / 2) d = static_cast<int>(R) - d;
    return d;
}

inline int h_distance(std::size_t len, const uint8_t x[], const uint8_t y[]) {
    int dist = 0;
    for (std::size_t i = 0; i < len; ++i) {
	uint8_t diff = x[i] ^ y[i];
	while (diff) {
	    dist += diff & 1;
	    diff >>= 1;
	}
    }
    return dist;
}

inline uint8_t swap_byte(uint8_t in) {
    return static_cast<uint8_t>(((in & 0x0F) << 4) | ((in & 0xF0) >> 4));
}

inline void to_hex(const uint8_t* psrc, std::size_t len, char* pdest) {
    static const char* hex = "0123456789ABCDEF";
    for (std::size_t i = 0; i < len; ++i) {
	pdest[2*i]   = hex[(psrc[i] >> 4) & 0xF];
	pdest[2*i+1] = hex[psrc[i] & 0xF];
    }
    pdest[2*len] = '\0';
}

inline void from_hex(const char* psrc, std::size_t len, uint8_t* pdest) {
    for (std::size_t i = 0; i < len; ++i) {
	char c1 = psrc[2*i], c2 = psrc[2*i+1];
	uint8_t v1 = (c1 >= '0' && c1 <= '9') ? (c1 - '0') : (c1 >= 'A' ? c1 - 'A' + 10 : c1 - 'a' + 10);
	uint8_t v2 = (c2 >= '0' && c2 <= '9') ? (c2 - '0') : (c2 >= 'A' ? c2 - 'A' + 10 : c2 - 'a' + 10);
	pdest[i] = static_cast<uint8_t>((v1 << 4) | v2);
    }
}

struct lsh_bin_struct {
    uint8_t checksum[TLSH_CHECKSUM_LEN];
    uint8_t Lvalue;
    union {
	struct {
	    uint8_t Q1ratio  : 4;
	    uint8_t Q2ratio  : 4;
	} QR;
	uint8_t QB;
    } Q;
    uint8_t tmp_code[CODE_SIZE];
};

class TlshImpl {
    public:
	uint32_t* a_bucket;
	std::size_t data_len;
	lsh_bin_struct lsh_bin;
	bool lsh_code_valid;
	uint8_t slide_window[SLIDING_WND_SIZE];

	TlshImpl()
	    : a_bucket(nullptr), data_len(0), lsh_code_valid(false)
	{
	    std::memset(this->slide_window, 0, sizeof(this->slide_window));
	    std::memset(&this->lsh_bin, 0, sizeof(this->lsh_bin));
	}

	~TlshImpl() {
	    delete[] this->a_bucket;
	}

	void reset() {
	    delete[] this->a_bucket; this->a_bucket = nullptr;
	    std::memset(this->slide_window, 0, sizeof(this->slide_window));
	    std::memset(&this->lsh_bin, 0, sizeof(this->lsh_bin));
	    this->data_len = 0;
	    this->lsh_code_valid = false;
	}

	void update(const uint8_t* data, std::size_t len) {
	    if (len == 0) return;
	    if (!data) throw std::invalid_argument("tlsh::TlshImpl::update() - data pointer is null and len > 0");

	    if (this->lsh_code_valid) {
		throw std::logic_error("tlsh::TlshImpl::update() - cannot update after finalization");
	    }

	    std::size_t fed_len = this->data_len;
	    if (this->a_bucket == nullptr) {
		this->a_bucket = new (std::nothrow) uint32_t[BUCKETS];
		if (!this->a_bucket) throw std::bad_alloc();
		std::memset(this->a_bucket, 0, sizeof(uint32_t) * BUCKETS);
	    }
	    std::size_t j = this->data_len % SLIDING_WND_SIZE;
	    for (std::size_t i = 0; i < len; i++, fed_len++, j = (j + 1) % SLIDING_WND_SIZE) {
		this->slide_window[j] = data[i];
		if (fed_len >= SLIDING_WND_SIZE_M1) {
		    std::size_t j_1 = (j + SLIDING_WND_SIZE - 1) % SLIDING_WND_SIZE;
		    std::size_t j_2 = (j + SLIDING_WND_SIZE - 2) % SLIDING_WND_SIZE;
		    std::size_t j_3 = (j + SLIDING_WND_SIZE - 3) % SLIDING_WND_SIZE;
		    std::size_t j_4 = (j + SLIDING_WND_SIZE - 4) % SLIDING_WND_SIZE;
		    this->lsh_bin.checksum[0] = fast_b_mapping(1, this->slide_window[j], this->slide_window[j_1], this->lsh_bin.checksum[0]);
		    this->a_bucket[fast_b_mapping(49, this->slide_window[j], this->slide_window[j_1], this->slide_window[j_2])]++;
		    this->a_bucket[fast_b_mapping(12, this->slide_window[j], this->slide_window[j_1], this->slide_window[j_3])]++;
		    this->a_bucket[fast_b_mapping(178, this->slide_window[j], this->slide_window[j_2], this->slide_window[j_3])]++;
		    this->a_bucket[fast_b_mapping(166, this->slide_window[j], this->slide_window[j_2], this->slide_window[j_4])]++;
		    this->a_bucket[fast_b_mapping(84, this->slide_window[j], this->slide_window[j_1], this->slide_window[j_4])]++;
		    this->a_bucket[fast_b_mapping(230, this->slide_window[j], this->slide_window[j_3], this->slide_window[j_4])]++;
		}
	    }
	    if (data_len > static_cast<std::size_t>(-1) - len)
		throw std::overflow_error("tlsh::TlshImpl::update() - data_len overflow");
	    this->data_len += len;
	}

	void final() {
	    if (this->lsh_code_valid) return;
	    if (this->data_len < MIN_DATA_LENGTH) {
		delete[] this->a_bucket; this->a_bucket = nullptr;
		return;
	    }
	    uint32_t bucket_copy[BUCKETS];
	    for (std::size_t i = 0; i < BUCKETS; ++i) bucket_copy[i] = this->a_bucket[i];
	    std::nth_element(bucket_copy, bucket_copy + BUCKETS / 4, bucket_copy + BUCKETS);
	    uint32_t q1 = bucket_copy[BUCKETS / 4];
	    std::nth_element(bucket_copy, bucket_copy + BUCKETS / 2, bucket_copy + BUCKETS);
	    uint32_t q2 = bucket_copy[BUCKETS / 2];
	    std::nth_element(bucket_copy, bucket_copy + 3 * BUCKETS / 4, bucket_copy + BUCKETS);
	    uint32_t q3 = bucket_copy[3 * BUCKETS / 4];
	    for (std::size_t i = 0; i < CODE_SIZE; ++i) {
		uint8_t h = 0;
		for (std::size_t j = 0; j < 4; ++j) {
		    uint32_t k = this->a_bucket[4 * i + j];
		    if (q3 < k) {
			h += 3 << (j * 2);
		    } else if (q2 < k) {
			h += 2 << (j * 2);
		    } else if (q1 < k) {
			h += 1 << (j * 2);
		    }
		}
		this->lsh_bin.tmp_code[i] = h;
	    }
	    delete[] this->a_bucket; this->a_bucket = nullptr;
	    this->lsh_bin.Lvalue = (this->data_len > 656) ? 255 : static_cast<uint8_t>(this->data_len / 2);
	    this->lsh_bin.Q.QR.Q1ratio = static_cast<uint8_t>(((static_cast<uint64_t>(q1) * 100 / q3) % 16));
	    this->lsh_bin.Q.QR.Q2ratio = static_cast<uint8_t>(((static_cast<uint64_t>(q2) * 100 / q3) % 16));
	    this->lsh_code_valid = true;
	}

	int fromTlshStr(const char* str) {
	    if (!str) return 1;
	    std::size_t start = 0;
	    if (str[0] == 'T' && (str[1] == '1' || str[1] == '0')) {
		start = 2;
	    }
	    std::size_t expectedLen = 2 * (1 + 1 + 1 + CODE_SIZE);
	    std::size_t strLen = std::strlen(str);
	    if (strLen < start + expectedLen) return 1;
	    this->reset();
	    uint8_t tmpbuf[1 + 1 + 1 + CODE_SIZE];
	    from_hex(str + start, 1 + 1 + 1 + CODE_SIZE, tmpbuf);
	    this->lsh_bin.checksum[0] = swap_byte(tmpbuf[0]);
	    this->lsh_bin.Lvalue = swap_byte(tmpbuf[1]);
	    this->lsh_bin.Q.QB = swap_byte(tmpbuf[2]);
	    for (std::size_t i = 0; i < CODE_SIZE; ++i) {
		this->lsh_bin.tmp_code[i] = tmpbuf[3 + CODE_SIZE - 1 - i];
	    }
	    this->lsh_code_valid = true;
	    return 0;
	}

	std::string hash(int showvers = 1) const {
	    char buf[TLSH_STRING_LEN_REQ + 1] = {0};
	    lsh_bin_struct tmp;
	    tmp.checksum[0] = swap_byte(this->lsh_bin.checksum[0]);
	    tmp.Lvalue = swap_byte(this->lsh_bin.Lvalue);
	    tmp.Q.QB = swap_byte(this->lsh_bin.Q.QB);
	    for (std::size_t i = 0; i < CODE_SIZE; ++i) {
		tmp.tmp_code[i] = this->lsh_bin.tmp_code[CODE_SIZE - 1 - i];
	    }
	    if (showvers) {
		buf[0] = 'T';
		buf[1] = '1';
		to_hex(reinterpret_cast<const uint8_t*>(&tmp), sizeof(tmp), &buf[2]);
	    } else {
		to_hex(reinterpret_cast<const uint8_t*>(&tmp), sizeof(tmp), buf);
	    }
	    return std::string(buf);
	}

	int compare(const TlshImpl& other) const {
	    return std::memcmp(&(this->lsh_bin), &(other.lsh_bin), sizeof(this->lsh_bin));
	}

	int diff(const TlshImpl& other) const {
	    int diff_score = 0;
	    diff_score += mod_diff(this->lsh_bin.Lvalue, other.lsh_bin.Lvalue, RANGE_LVALUE);
	    diff_score += mod_diff(this->lsh_bin.Q.QR.Q1ratio, other.lsh_bin.Q.QR.Q1ratio, RANGE_QRATIO);
	    diff_score += mod_diff(this->lsh_bin.Q.QR.Q2ratio, other.lsh_bin.Q.QR.Q2ratio, RANGE_QRATIO);
	    if (this->lsh_bin.checksum[0] != other.lsh_bin.checksum[0]) diff_score++;
	    diff_score += h_distance(CODE_SIZE, this->lsh_bin.tmp_code, other.lsh_bin.tmp_code);
	    return diff_score;
	}
};

class Tlsh {
    TlshImpl impl_;
    public:
    Tlsh() = default;

    void update(const uint8_t* data, std::size_t len) {
	impl_.update(data, len);
    }
    void final() { impl_.final(); }
    std::string getHash() const { return impl_.hash(); }
    int compare(const Tlsh& other) const { return impl_.compare(other.impl_); }
    int diff(const Tlsh& other) const { return impl_.diff(other.impl_); }
    void reset() { impl_.reset(); }
    int fromTlshStr(const char* str) { return impl_.fromTlshStr(str); }
};

} // namespace tlsh

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8 cino=N-s
