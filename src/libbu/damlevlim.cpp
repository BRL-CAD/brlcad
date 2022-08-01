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
    if (!s1 || !s2) {
	return 0;
    }

    // If we have equal strings, trivial equality
    if (BU_STR_EQUAL(s1, s2)) {
	return 0;
    }

    // If one of either of the strings doesn't exist, the length of the other
    // string is our edit distance
    if (!s1 || !s2) {
	if (!s1)
	    return strlen(s2);
	if (!s2)
	    return strlen(s1);
    }

    // We need std::mismatch to handle any length string in either argument -
    // prior to C++14, it did not. That case fails hard on some platforms
    // (Windows) at runtime.
#if defined(CXX_STANDARD) && CXX_STANDARD >= 14

    // Set up buffer
    char *ptr = (char *)new(std::nothrow) std::vector<size_t>(DAMLEVLIM_MAX_EDIT_DIST);
    if (!ptr)
	return DAMLEVLIM_MAX_EDIT_DIST;
    std::vector<size_t> &buffer = *(std::vector<size_t> *)ptr;

    // Let's make some string views so we can use the STL.
    std::string subject(s1);
    std::string query(s2);

    // Skip any common prefix.
    auto mb = std::mismatch(subject.begin(), subject.end(), query.begin());
    auto start_offset = (size_t)std::distance(subject.begin(), mb.first);

    // If one of the strings is a prefix of the other, done.
    if (subject.length() == start_offset) {
	delete ptr;
	return (size_t)(query.length() - start_offset);
    } else if (query.length() == start_offset) {
	delete ptr;
	return (size_t)(subject.length() - start_offset);
    }

    // Skip any common suffix.
    auto me = std::mismatch(
		  subject.rbegin(), static_cast<decltype(subject.rend())>(mb.first - 1),
		  query.rbegin());
    auto end_offset = std::min((size_t)std::distance(subject.rbegin(), me.first),
			       (size_t)(subject.size() - start_offset));

    // Take the different part.
    subject = subject.substr(start_offset, subject.size() - end_offset - start_offset + 1);
    query = query.substr(start_offset, query.size() - end_offset - start_offset + 1);

    // Make "subject" the smaller one.
    if (query.length() < subject.length()) {
	std::swap(subject, query);
    }
    // If one of the strings is a suffix of the other.
    if (subject.length() == 0) {
	delete ptr;
	return query.length();
    }

    // Init buffer.
    std::iota(buffer.begin(), buffer.begin() + query.length() + 1, 0);

    size_t end_j = 0;
    for (size_t i = 1; i < subject.length() + 1; ++i) {
	// temp = i - 1;
	size_t temp = buffer[0]++;
	size_t prior_temp = 0;

	// Setup for max distance, which only needs to look in the window
	// between i-max <= j <= i+max.
	// The result of the max is positive, but we need the second argument
	// to be allowed to be negative.
	const size_t start_j = static_cast<size_t>(std::max(1ll, static_cast<long long>(i) -
			       DAMLEVLIM_MAX_EDIT_DIST/2));
	end_j = std::min(static_cast<size_t>(query.length() + 1),
			 static_cast<size_t >(i + DAMLEVLIM_MAX_EDIT_DIST/2));
	size_t column_min = DAMLEVLIM_MAX_EDIT_DIST;     // Sentinels
	for (size_t j = start_j; j < end_j; ++j) {
	    const size_t p = temp; // p = buffer[j - 1];
	    const size_t r = buffer[j];
	    /*
	       auto min = r;
	       if (p < min) min = p;
	       if (prior_temp < min) min = prior_temp;
	       min++;
	       temp = temp + (subject[i - 1] == query[j - 1] ? 0 : 1);
	       if (min < temp) temp = min;
	       */
	    temp = std::min(std::min(r,  // Insertion.
				     p   // Deletion.
				    ) + 1,

			    std::min(
				// Transposition.
				prior_temp + 1,
				// Substitution.
				temp + (subject[i - 1] == query[j - 1] ? 0 : 1)
			    )
			   );
	    // Keep track of column minimum.
	    if (temp < column_min) {
		column_min = temp;
	    }
	    // Record matrix value mat[i-2][j-2].
	    prior_temp = temp;
	    std::swap(buffer[j], temp);
	}
	if (column_min >= DAMLEVLIM_MAX_EDIT_DIST) {
	    // There is no way to get an edit distance > column_min.
	    // We can bail out early.
	    delete ptr;
	    return DAMLEVLIM_MAX_EDIT_DIST;
	}
    }

    size_t ret = buffer[end_j-1];
    delete ptr;
    return ret;

#else

    return DAMLEVLIM_MAX_EDIT_DIST;

#endif
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
