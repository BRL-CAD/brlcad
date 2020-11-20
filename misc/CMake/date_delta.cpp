/*                  D A T E _ D E L T A . C P P
 * BRL-CAD
 *
 * Published in 2020 by the United States Government.
 * This work is in the public domain.
 *
 */
/** @file date_delta.cpp
 *
 * Delta calculations using https://github.com/HowardHinnant/date, which
 * is close to what is being considered for C++20.
 *
 */

#include "date.h"
#include <iostream>

int
main()
{
    long t1 = 1602012807;
    long t2 = 1602114000;
    std::time_t s1 = std::time_t(t1);
    std::time_t s2 = std::time_t(t2);
    std::chrono::system_clock::time_point tp1 = std::chrono::system_clock::from_time_t(s1);
    std::chrono::system_clock::time_point tp2 = std::chrono::system_clock::from_time_t(s2);

    auto dd = date::floor<date::days>(tp2) - date::floor<date::days>(tp1);
    if (dd.count())
	std::cout << "delta(dys): " << dd.count() << "\n";

    auto dtime = date::make_time(std::chrono::duration_cast<std::chrono::milliseconds>(tp2-tp1));
    if (dtime.hours().count())
	std::cout << "delta(hrs): " << dtime.hours().count() << "\n";
    if (dtime.minutes().count())
	std::cout << "delta(min): " << dtime.minutes().count() << "\n";
    if (dtime.seconds().count())
	std::cout << "delta(sec): " << dtime.seconds().count() << "\n";
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
