// Copyright (C) 2023 Robert Jacobson. Released under the MIT license.
//
// Based on "Iosifovich", Copyright (C) 2019 Frederik Hertzum, which is
// licensed under the MIT license: https://bitbucket.org/clearer/iosifovich.
//
// The MIT License
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to
// deal in the Software without restriction, including without limitation the
// rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
// sell copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
// IN THE SOFTWARE.

/*
 * Compute the Damarau Levenshtein edit distance between two strings when the
 * edit distance is less than a given number.
 *
 * https://github.com/rljacobson/Levenshtein
 */

#include "common.h"

#include <string.h>

#include <algorithm>
#include <limits>
#include <vector>
#include <string>
#include <numeric>

#include "bu/str.h"

// Limits
#define DAMLEVLIM_MAX_EDIT_DIST 16384

size_t
bu_editdist(const char *s1, const char *s2)
{
    // If we don't have both strings, trivial equality
    if (!s1 && !s2)
	return 0;

    // If one of either of the strings doesn't exist, the length of the other
    // string is our edit distance
    if (!s1 || !s2) {
	if (!s1)
	    return strlen(s2);
	if (!s2)
	    return strlen(s1);
    }

    // If we have equal strings, trivial equality
    if (BU_STR_EQUAL(s1, s2))
	return 0;

    size_t s1len = strlen(s1);
    size_t s2len = strlen(s2);
    size_t max_string_length = std::max(s1len, s2len);
    size_t len_diff = (s1len > s2len) ? (s1len - s2len) : (s2len - s1len);

    if (len_diff >= DAMLEVLIM_MAX_EDIT_DIST) {
	return max_string_length;
    }

    size_t max = std::min(s2len, static_cast<size_t>(DAMLEVLIM_MAX_EDIT_DIST));

    try {
	// Let's make some string views so we can use the STL.
	std::string_view subject(s1);
	std::string_view query(s2);

	// Skip any common prefix.
	auto prefix_mismatch = std::mismatch(subject.begin(), subject.end(), query.begin(), query.end());
	auto start_offset = std::distance(subject.begin(), prefix_mismatch.first);

	// If one of the strings is a prefix of the other, done.
	if (static_cast<size_t>(subject.length()) == static_cast<size_t>(start_offset)) {
	    return query.length() - start_offset;
	} else if (static_cast<size_t>(query.length()) == static_cast<size_t>(start_offset)) {
	    return subject.length() - start_offset;
	}

	// Skip any common suffix.
	auto suffix_mismatch = std::mismatch(subject.rbegin(), std::next(subject.rend(), -start_offset),
		query.rbegin(), std::next(query.rend(), -start_offset));
	auto end_offset = std::distance(subject.rbegin(), suffix_mismatch.first);

	// Extract the different part if significant.
	if (start_offset + end_offset < static_cast<ptrdiff_t>(subject.length())) {
	    subject = subject.substr(start_offset, subject.length() - start_offset - end_offset);
	    query = query.substr(start_offset, query.length() - start_offset - end_offset);
	}

	// Ensure 'subject' is the smaller string for efficiency
	if (query.length() < subject.length()) {
	    std::swap(subject, query);
	}

	size_t n = subject.size(); // Length of the smaller string
	size_t m = query.size(); // Length of the larger string

	if (m - n >= DAMLEVLIM_MAX_EDIT_DIST) {
	    return max_string_length;
	}

	// Calculate trimmed_max based on the lengths of the trimmed strings
	size_t trimmed_max = std::max(n, m);

	// Determine the effective maximum edit distance
	size_t effective_max = std::min(max, trimmed_max);

	// Check for integer overflow on (n + 1) * (m + 1) and cap reasonable allocation
	if ((n + 1) > SIZE_MAX / (m + 1) || (n + 1) * (m + 1) > (size_t)DAMLEVLIM_MAX_EDIT_DIST * 1024) {
	    return max_string_length;
	}

	// Resize the buffer to simulate a 2D matrix with dimensions (n+1) x (m+1)
	std::vector<size_t> buffer((n + 1) * (m + 1));

	// Lambda function for 2D matrix indexing in the 1D buffer
	auto idx = [m](size_t i, size_t j) -> size_t { return i * (m + 1) + j; };

	// Initialize the first row and column of the matrix
	for (size_t i = 0; i <= n; ++i) {
	    buffer[idx(i, 0)] = i;
	}
	for (size_t j = 0; j <= m; ++j) {
	    buffer[idx(0, j)] = j;
	}

	// Main loop to calculate the Damerau-Levenshtein distance
	for (size_t i = 1; i <= n; ++i) {
	    size_t column_min = std::numeric_limits<size_t>::max();

	    for (size_t j = 1; j <= m; ++j) {
		size_t cost = (subject[i - 1] == query[j - 1]) ? 0 : 1;

		buffer[idx(i, j)] = std::min({buffer[idx(i - 1, j)] + 1,
			buffer[idx(i, j - 1)] + 1,
			buffer[idx(i - 1, j - 1)] + cost});

		// Check for transpositions
		if (i > 1 && j > 1 && subject[i - 1] == query[j - 2] && subject[i - 2] == query[j - 1]) {
		    buffer[idx(i, j)] = std::min(buffer[idx(i, j)], buffer[idx(i - 2, j - 2)] + cost);
		}

		column_min = std::min(column_min, buffer[idx(i, j)]);
	    }

	    // Early exit if the minimum edit distance exceeds the effective maximum
	    if (column_min > effective_max) {
		return max_string_length;
	    }
	}
	return buffer[idx(n, m)];
    } catch (...) {
	return max_string_length;
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
