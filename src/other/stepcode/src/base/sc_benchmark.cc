/// \file sc_benchmark.cc memory info, timers, etc for benchmarking

#include "sc_benchmark.h"
#include "sc_memmgr.h"

#ifdef __WIN32__
#include <Windows.h>
#include <psapi.h>
#else
#include <sys/time.h>
#include <ctime>
#include <unistd.h>
#endif

#include <iostream>
#include <fstream>
#include <iomanip>
#include <string>
#include <sstream>
#include <ios>

/// mem values in kb, times in ms (granularity may be higher than 1ms)
benchVals getMemAndTime( ) {
    benchVals vals;
#ifdef __linux__
    // adapted from http://stackoverflow.com/questions/669438/how-to-get-memory-usage-at-run-time-in-c
    std::ifstream stat_stream( "/proc/self/stat", std::ios_base::in );

    // dummy vars for leading entries in stat that we don't care about
    std::string pid, comm, state, ppid, pgrp, session, tty_nr;
    std::string tpgid, flags, minflt, cminflt, majflt, cmajflt;
    std::string /*utime, stime,*/ cutime, cstime, priority, nice;
    std::string O, itrealvalue, starttime;

    // the fields we want
    unsigned long utime, stime, vsize;
    long rss;

    stat_stream >> pid >> comm >> state >> ppid >> pgrp >> session >> tty_nr
                >> tpgid >> flags >> minflt >> cminflt >> majflt >> cmajflt
                >> utime >> stime >> cutime >> cstime >> priority >> nice
                >> O >> itrealvalue >> starttime >> vsize >> rss; // don't care about the rest

    long page_size_kb = sysconf( _SC_PAGE_SIZE ) / 1024; // in case x86-64 is configured to use 2MB pages
    vals.physMemKB  = rss * page_size_kb;
    vals.virtMemKB  = ( vsize / 1024 ) - vals.physMemKB;
    vals.userMilliseconds = ( utime * 1000 ) / sysconf( _SC_CLK_TCK );
    vals.sysMilliseconds  = ( stime * 1000 ) / sysconf( _SC_CLK_TCK );
#elif defined(__APPLE__)
    // http://stackoverflow.com/a/1911863/382458
#elif defined(__WIN32__)
    // http://stackoverflow.com/a/282220/382458 and http://stackoverflow.com/a/64166/382458
    PROCESS_MEMORY_COUNTERS MemoryCntrs;
    FILETIME CreationTime, ExitTime, KernelTime, UserTime;
    long page_size_kb = 1024;

    if( GetProcessMemoryInfo( GetCurrentProcess(), &MemoryCntrs, sizeof( MemoryCntrs ) ) ) {
        vals.physMemKB = MemoryCntrs.PeakWorkingSetSize / page_size_kb;
        vals.virtMemKB = MemoryCntrs.PeakPagefileUsage / page_size_kb;
    } else {
        vals.physMemKB = 0;
        vals.virtMemKB = 0;
    }

    if( GetProcessTimes( GetCurrentProcess(), &CreationTime, &ExitTime, &KernelTime, &UserTime ) ) {
        vals.userMilliseconds = ( long )( ( ( ULARGE_INTEGER * ) &UserTime )->QuadPart / 100000L );
        vals.sysMilliseconds = ( long )( ( ( ULARGE_INTEGER * ) &KernelTime )->QuadPart / 100000L );
    } else {
        vals.userMilliseconds = 0;
        vals.sysMilliseconds = 0;
    }
#else
#warning Unknown platform!
#endif // __linux__
    return vals;
}

// ---------------------   benchmark class   ---------------------

benchmark::benchmark( std::string description, bool debugMessages, std::ostream & o_stream ): ostr( o_stream ),
    descr( description ), debug( debugMessages ), stopped( false ) {
    initialVals = getMemAndTime( );
}

benchmark::~benchmark() {
    if( !stopped ) {
        stop( );
        if( debug ) {
            ostr << "benchmark::~benchmark(): stop was not called before destructor!" << std::endl;
        }
        out( );
    }
}

void benchmark::stop( ) {
    if( stopped ) {
        std::cerr << "benchmark::stop(): tried to stop a benchmark that was already stopped!" << std::endl;
    } else {
        laterVals = getMemAndTime( );
        stopped = true;
    }
}

benchVals benchmark::get( ) {
    if( !stopped ) {
        laterVals = getMemAndTime( );
    }
    benchVals delta;
    delta.physMemKB = laterVals.physMemKB - initialVals.physMemKB;
    delta.virtMemKB = laterVals.virtMemKB - initialVals.virtMemKB;
    delta.sysMilliseconds = laterVals.sysMilliseconds - initialVals.sysMilliseconds;
    delta.userMilliseconds = laterVals.userMilliseconds - initialVals.userMilliseconds;

    //If vm is negative, the memory had been requested before initialVals was set. Don't count it
    if( delta.virtMemKB < 0 ) {
        delta.physMemKB -= delta.virtMemKB;
        delta.virtMemKB = 0;
    }
    return delta;
}

void benchmark::reset( std::string description ) {
    descr = description;
    reset();
}
void benchmark::reset( ) {
    stopped = false;
    initialVals = getMemAndTime();
}

std::string benchmark::str( ) {
    return str( get( ) );
}

void benchmark::out() {
    ostr << str( ) << std::endl;
}

std::string benchmark::str( const benchVals & bv ) {
    std::stringstream ss;
    ss << descr << " Physical memory: " << bv.physMemKB << "kb; Virtual memory: " << bv.virtMemKB;
    ss << "kb; User CPU time: " << bv.userMilliseconds << "ms; System CPU time: " << bv.sysMilliseconds << "ms";
    return ss.str();
}

