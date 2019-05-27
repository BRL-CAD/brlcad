#include <iostream>
#include <string>
#include <sstream>
#include <stdint.h>

/* This is an extraction in stand-alone form of a small set of the Apache
 * Portable Runtime logic for date parsing.  It's sole purpose is to convert
 * internal SVN timestamps into proper Git timestamp strings.
 *
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The ASF licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#define TIMESTAMP_FORMAT "%04d-%02d-%02dT%02d:%02d:%02d.%06dZ"
#define APR_USEC_PER_SEC 1000000LL

struct svn_time {
    int tm_year;
    int tm_mon;
    int tm_mday;
    int tm_hour;
    int tm_min;
    int tm_sec;
    int tm_usec;
};

uint64_t
svn_time_from_cstring(struct svn_time *xt, const char *tstamp)
{
    uint64_t tfinal;
    uint64_t year;
    uint64_t days;
    static const int dayoffset[12] = {306, 337, 0, 31, 61, 92, 122, 153, 184, 214, 245, 275};

    sscanf(tstamp, TIMESTAMP_FORMAT,  &xt->tm_year, &xt->tm_mon,
	    &xt->tm_mday, &xt->tm_hour, &xt->tm_min,
	    &xt->tm_sec, &xt->tm_usec);

    xt->tm_year  -= 1900;
    xt->tm_mon   -= 1;
    year = xt->tm_year;

    if (xt->tm_mon < 0 || xt->tm_mon >= 12)
	return 0;

    /* shift new year to 1st March in order to make leap year calc easy */
    if (xt->tm_mon < 2)
	year--;

    /* Find number of days since 1st March 1900 (in the Gregorian calendar). */
    days = year * 365 + year / 4 - year / 100 + (year / 100 + 3) / 4;
    days += dayoffset[xt->tm_mon] + xt->tm_mday - 1;
    days -= 25508;              /* 1 jan 1970 is 25508 days since 1 mar 1900 */
    days = ((days * 24 + xt->tm_hour) * 60 + xt->tm_min) * 60 + xt->tm_sec;

    if (days < 0) {
	return 0;
    }

    tfinal = days * APR_USEC_PER_SEC + xt->tm_usec;

    return tfinal;
}

std::string
svn_time_to_git_time(const char *tstamp)
{
    struct svn_time svntime;
    uint64_t stime = svn_time_from_cstring(&svntime, tstamp);
    uint64_t ssec = (stime) / APR_USEC_PER_SEC;
    return std::to_string(ssec) + std::string(" +0000");
}

#if 1
int
main()
{
    const std::string tstamp("1989-10-31T08:48:58.000000Z");
    std::cout << tstamp << "\n";
    std::cout << "converted time: " << svn_time_to_git_time(tstamp.c_str()) << "\n";
}
#endif

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8

