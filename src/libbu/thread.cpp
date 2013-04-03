/*                      T H R E A D . C P P
 * BRL-CAD
 *
 * Copyright (c) 2013 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */

#include "common.h"

#include <vector>

#ifdef HAVE_PTHREAD_H
#  include <pthread.h>
#endif


template<typename T>
class ThreadLocal
{
private:
#ifdef HAVE_PTHREAD_H
    pthread_key_t key;
#endif
    std::vector<T*> vals;
public:
    ThreadLocal() {
#ifdef HAVE_PTHREAD_H
	pthread_key_create(&key, NULL);
#endif
    }
    ~ThreadLocal() {
#ifdef HAVE_PTHREAD_H
	pthread_key_delete(key);
#endif
	while (!vals.empty()) {
	    delete vals.back();
	    vals.pop_back();
	}
	vals.clear();
    }
    ThreadLocal& operator=(T& val) {
	T* value = new T(val);
	vals.push_back(value);
#ifdef HAVE_PTHREAD_H
	pthread_setspecific(key, value);
#endif
	return *this;
    }
    bool operator!() {
#ifdef HAVE_PTHREAD_H
	return (pthread_getspecific(key) == NULL);
#endif
    }
    operator T() {
#ifdef HAVE_PTHREAD_H
	return *static_cast<T*>(pthread_getspecific(key));
#endif
    }
};


#if defined(__cplusplus) && __cplusplus > 199711L
// C++11 provides thread-local storage
thread_local int thread_cpu = 0;
#elif defined(HAVE___THREAD)
__thread int thread_cpu = 0;
#elif defined(_MSC_VER)
__declspec(thread) int thread_cpu;
#else
ThreadLocal<int> thread_cpu;
#endif

extern "C" {

void
thread_set_cpu(int cpu)
{
    thread_cpu = cpu;
}


int
thread_get_cpu(void)
{
    return thread_cpu;
}


} /* extern "C" */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
