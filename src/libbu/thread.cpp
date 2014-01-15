/*                      T H R E A D . C P P
 * BRL-CAD
 *
 * Copyright (c) 2013-2014 United States Government as represented by
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

#include "bu.h"


#if defined(HAVE_THREAD_LOCAL)

static thread_local int thread_cpu = 0;

#elif defined(HAVE___THREAD)

static __thread int thread_cpu = 0;

#elif defined(HAVE___DECLSPEC_THREAD)

static __declspec(thread) int thread_cpu;

#elif defined(HAVE_PTHREAD_H)

template<typename T>
class ThreadLocal
{
private:
    pthread_key_t key;
    std::vector<T*> vals;
protected:
    void set(T& val) {
	T* value = new T(val);

	bu_semaphore_acquire(BU_SEM_THREAD);
	vals.push_back(value);
	bu_semaphore_release(BU_SEM_THREAD);

	pthread_setspecific(key, value);
    }
    T* get() {
	T* val = static_cast<T*>(pthread_getspecific(key));
	return val;
    }
public:
    ThreadLocal() {
	T init = 0;
	pthread_key_create(&key, NULL);
	set(init);
    }
    ~ThreadLocal() {
	pthread_key_delete(key);

	bu_semaphore_acquire(BU_SEM_THREAD);
	while (!vals.empty()) {
	    delete vals.back();
	    vals.pop_back();
	}
	vals.clear();
	bu_semaphore_release(BU_SEM_THREAD);
    }
    ThreadLocal& operator=(T& val) {
	set(val);
	return *this;
    }
    bool operator!() {
	return (get() == NULL);
    }
    operator T() {
	T* val = get();
	if (!val)
	    return 0;
	return *val;
    }
};
static ThreadLocal<int> thread_cpu;

#else
#  error "Unrecognized thread local storage method for this platform"
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
