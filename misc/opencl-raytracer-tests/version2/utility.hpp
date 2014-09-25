#ifndef __UTILITY_H__
#define __UTILITY_H__


#include <stdexcept>
#include <sstream>


#ifdef DEBUG_BUILD
#define DEBUG(x) do {x;} while (0)
#else
#define DEBUG(x) do {} while (0)
#endif


namespace utility
{


    // modified from Boost
    class bad_lexical_cast : public std::runtime_error
    {
        public:
            bad_lexical_cast(const std::string &vwhat) : std::runtime_error(vwhat) {}
    };


    template<typename Target, typename Source>
        Target lexical_cast(const Source arg)
        {
            std::stringstream interpreter;
            Target result;
            if(!(interpreter << arg) || !(interpreter >> result) || !(interpreter >> std::ws).eof())
                throw bad_lexical_cast("invalid conversion value");
            return result;
        }



}


#endif


