/*
 *  Copyright (c) 2000-2022 Inria
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are met:
 *
 *  * Redistributions of source code must retain the above copyright notice,
 *  this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright notice,
 *  this list of conditions and the following disclaimer in the documentation
 *  and/or other materials provided with the distribution.
 *  * Neither the name of the ALICE Project-Team nor the names of its
 *  contributors may be used to endorse or promote products derived from this
 *  software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 *  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 *
 *  Contact: Bruno Levy
 *
 *     https://www.inria.fr/fr/bruno-levy
 *
 *     Inria,
 *     Domaine de Voluceau,
 *     78150 Le Chesnay - Rocquencourt
 *     FRANCE
 *
 */

#include "common.h"
#include <geogram/basic/thread_sync.h>
#include <thread>
#include <vector>
#include <algorithm>
#include <atomic>

namespace {
    using namespace GEOBRL;
    std::atomic<int> running_threads_invocations_{0};
}

namespace GEOBRL {

    namespace Process {

        index_t maximum_concurrent_threads() {
            index_t result = index_t(std::thread::hardware_concurrency());
            return result > 0 ? result : 1;
        }

        bool is_running_threads() {
            return running_threads_invocations_ > 0;
        }

    }

    void parallel_for(
        index_t from, index_t to, std::function<void(index_t)> func,
        index_t threads_per_core, bool interleaved
    ) {
        index_t nb_threads = std::min(
            to - from,
            Process::maximum_concurrent_threads() * threads_per_core
        );
        nb_threads = std::max(index_t(1), nb_threads);
        index_t batch_size = (to - from) / nb_threads;

        if(running_threads_invocations_ > 0 || nb_threads == 1) {
            for(index_t i = from; i < to; i++) {
                func(i);
            }
            return;
        }

        ++running_threads_invocations_;
        std::vector<std::thread> threads;
        threads.reserve(nb_threads);

        if(interleaved) {
            for(index_t i = 0; i < nb_threads; i++) {
                threads.emplace_back([func, from, to, i, nb_threads]() {
                    for(index_t j = from + i; j < to; j += nb_threads) {
                        func(j);
                    }
                });
            }
        } else {
            index_t cur = from;
            for(index_t i = 0; i < nb_threads; i++) {
                index_t end = (i == nb_threads - 1) ? to : cur + batch_size;
                threads.emplace_back([func, cur, end]() {
                    for(index_t j = cur; j < end; j++) {
                        func(j);
                    }
                });
                cur += batch_size;
            }
        }

        for(auto& t : threads) t.join();
        --running_threads_invocations_;
    }

    void parallel(
        std::function<void()> f1,
        std::function<void()> f2
    ) {
        if(running_threads_invocations_ > 0 || Process::maximum_concurrent_threads() <= 1) {
            f1(); f2();
            return;
        }
        ++running_threads_invocations_;
        std::thread t1(f1), t2(f2);
        t1.join(); t2.join();
        --running_threads_invocations_;
    }

    void parallel(
        std::function<void()> f1,
        std::function<void()> f2,
        std::function<void()> f3,
        std::function<void()> f4
    ) {
        if(running_threads_invocations_ > 0 || Process::maximum_concurrent_threads() <= 1) {
            f1(); f2(); f3(); f4();
            return;
        }
        ++running_threads_invocations_;
        std::thread t1(f1), t2(f2), t3(f3), t4(f4);
        t1.join(); t2.join(); t3.join(); t4.join();
        --running_threads_invocations_;
    }

    void parallel(
        std::function<void()> f1,
        std::function<void()> f2,
        std::function<void()> f3,
        std::function<void()> f4,
        std::function<void()> f5,
        std::function<void()> f6,
        std::function<void()> f7,
        std::function<void()> f8
    ) {
        if(running_threads_invocations_ > 0 || Process::maximum_concurrent_threads() <= 1) {
            f1(); f2(); f3(); f4(); f5(); f6(); f7(); f8();
            return;
        }
        ++running_threads_invocations_;
        std::thread t1(f1), t2(f2), t3(f3), t4(f4);
        std::thread t5(f5), t6(f6), t7(f7), t8(f8);
        t1.join(); t2.join(); t3.join(); t4.join();
        t5.join(); t6.join(); t7.join(); t8.join();
        --running_threads_invocations_;
    }

}
