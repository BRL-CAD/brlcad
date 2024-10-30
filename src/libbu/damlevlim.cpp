// Copyright (C) 2019 Robert Jacobson. Released under the MIT license.
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

    int s1len = strlen(s1);
    int s2len = strlen(s2);

    int max_string_length = int(std::max(s1len, s2len));
    auto max = std::min(s2len, DAMLEVLIM_MAX_EDIT_DIST);


    // Set up buffer
    std::vector<size_t> buffer(DAMLEVLIM_MAX_EDIT_DIST);

    // Let's make some string views so we can use the STL.
    std::string_view subject(s1);
    std::string_view query(s2);

    // Skip any common prefix.
    auto prefix_mismatch = std::mismatch(subject.begin(), subject.end(), query.begin(), query.end());
    auto start_offset = std::distance(subject.begin(), prefix_mismatch.first);

    // If one of the strings is a prefix of the other, done.
    if (static_cast<int>(subject.length()) == start_offset) {
	return  static_cast<int>(query.length()) - start_offset;
    } else if (static_cast<int>(query.length()) == start_offset) {
	return  static_cast<int>(subject.length()) - start_offset;
    }

    // Skip any common suffix.
    auto suffix_mismatch = std::mismatch(subject.rbegin(), std::next(subject.rend(), -start_offset),
	    query.rbegin(), std::next(query.rend(), -start_offset));
    auto end_offset = std::distance(subject.rbegin(), suffix_mismatch.first);

    // Extract the different part if significant.
    if (start_offset + end_offset <  static_cast<int>(subject.length())) {
	subject = subject.substr(start_offset, subject.length() - start_offset - end_offset);
	query = query.substr(start_offset, query.length() - start_offset - end_offset);
    }

    // Ensure 'subject' is the smaller string for efficiency
    if (query.length() < subject.length()) {
	std::swap(subject, query);
    }

    int n = static_cast<int>(subject.size()); // Length of the smaller string,Cast size_type to int
    int m = static_cast<int>(query.size()); // Length of the larger string, Cast size_type to int

    // Calculate trimmed_max based on the lengths of the trimmed strings
    auto trimmed_max = std::max(n, m);

    // Determine the effective maximum edit distance
    // Casting max to int (ensure that max is within the range of int)
    int effective_max = std::min(static_cast<int>(max), static_cast<int>(trimmed_max));

    // Resize the buffer to simulate a 2D matrix with dimensions (n+1) x (m+1)
    buffer.resize((n + 1) * (m + 1));

    // Lambda function for 2D matrix indexing in the 1D buffer
    auto idx = [m](int i, int j) { return i * (m + 1) + j; };


    // Initialize the first row and column of the matrix
    for (int i = 0; i <= n; ++i) {
	buffer[idx(i, 0)] = i;
    }
    for (int j = 0; j <= m; ++j) {
	buffer[idx(0, j)] = j;
    }

    // Main loop to calculate the Damerau-Levenshtein distance
    for (int i = 1; i <= n; ++i) {
	size_t column_min = std::numeric_limits<size_t>::max();

	for (int j = 1; j <= m; ++j) {
	    int cost = (subject[i - 1] == query[j - 1]) ? 0 : 1;

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
	if (column_min > static_cast<size_t>(effective_max)) {
	    return max_string_length;
	}
    }
    buffer.resize(DAMLEVLIM_MAX_EDIT_DIST);
    return (static_cast<int>(buffer[idx(n, m)]));
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
