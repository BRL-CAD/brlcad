#ifndef __UTILITY_H__
#define __UTILITY_H__


#ifdef DEBUG_BUILD
#include <iostream>
#define D(x) do {std::cerr << x << std::endl;} while(0)
#else
#define D(x) do {} while(0)
#endif


#endif


