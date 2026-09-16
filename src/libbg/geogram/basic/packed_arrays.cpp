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
#include <geogram/basic/packed_arrays.h>

namespace GEOBRL {

    PackedArrays::PackedArrays() {
        nb_arrays_ = 0;
        Z1_block_size_ = 0;
        Z1_stride_ = 0;
        Z1_ = nullptr;
        ZV_ = nullptr;
        thread_safe_ = false;
    }

    void PackedArrays::show_stats() {
    }

    PackedArrays::~PackedArrays() {
        clear();
    }

    void PackedArrays::clear() {
        if(ZV_ != nullptr) {
            for(index_t i = 0; i < nb_arrays_; i++) {
                free(ZV_[i]);
            }
            free(ZV_);
            ZV_ = nullptr;
        }
        nb_arrays_ = 0;
        Z1_block_size_ = 0;
        Z1_stride_ = 0;
        free(Z1_);
        Z1_ = nullptr;
    }

    void PackedArrays::set_thread_safe(bool x) {
        thread_safe_ = x;
        if(x) {
            Z1_spinlocks_.resize(nb_arrays_);
        } else {
            Z1_spinlocks_.clear();
        }
    }

    void PackedArrays::init(
        index_t nb_arrays,
        index_t Z1_block_size,
        bool static_mode
    ) {
        clear();
        nb_arrays_ = nb_arrays;
        Z1_block_size_ = Z1_block_size;
        Z1_stride_ = Z1_block_size_ + 1;  // +1 for storing array size.
        Z1_ = (index_t*) calloc(
            nb_arrays_, sizeof(index_t) * Z1_stride_
        );
        if(!static_mode) {
            ZV_ = (index_t**) calloc(
                nb_arrays_, sizeof(index_t*)
            );
        }
        if(thread_safe_) {
            Z1_spinlocks_.resize(nb_arrays_);
        }
    }

    void PackedArrays::get_array(
        index_t array_index, index_t* array, bool lock
    ) const {
        geo_debug_assert(array_index < nb_arrays_);
        if(lock) {
            lock_array(array_index);
        }
        const index_t* array_base = Z1_ + array_index * Z1_stride_;
        index_t array_size = *array_base;
        index_t nb = array_size;
        array_base++;
        index_t nb_in_block = std::min(nb, Z1_block_size_);
        Memory::copy(array, array_base, sizeof(index_t) * nb_in_block);
        if(nb > nb_in_block) {
            nb -= nb_in_block;
            array += nb_in_block;
            array_base = ZV_[array_index];
            Memory::copy(array, array_base, sizeof(index_t) * nb);
        }
        if(lock) {
            unlock_array(array_index);
        }
    }

    void PackedArrays::set_array(
        index_t array_index,
        index_t array_size, const index_t* array,
        bool lock
    ) {
        geo_debug_assert(array_index < nb_arrays_);
        if(lock) {
            lock_array(array_index);
        }
        index_t* array_base = Z1_ + array_index * Z1_stride_;
        index_t old_array_size = *array_base;
        array_base++;
        if(array_size != old_array_size) {
            resize_array(array_index, array_size, false);
        }
        index_t nb = array_size;
        index_t nb_in_block = std::min(nb, Z1_block_size_);
        Memory::copy(array_base, array, sizeof(index_t) * nb_in_block);
        if(nb > nb_in_block) {
            nb -= nb_in_block;
            array += nb_in_block;
            array_base = ZV_[array_index];
            Memory::copy(array_base, array, sizeof(index_t) * nb);
        }
        if(lock) {
            unlock_array(array_index);
        }
    }

    void PackedArrays::resize_array(
        index_t array_index, index_t array_size, bool lock
    ) {
        geo_debug_assert(array_index < nb_arrays_);
        if(lock) {
            lock_array(array_index);
        }
        index_t* array_base = Z1_ + array_index * Z1_stride_;
        index_t old_array_size = *array_base;
        if(old_array_size != array_size) {
            *array_base = array_size;
            if(static_mode()) {
                geo_assert(array_size <= Z1_block_size_);
            } else {
                index_t nb_in_ZV =
                    (array_size > Z1_block_size_) ?
                    array_size - Z1_block_size_ : 0;
                ZV_[array_index] = (index_t*) realloc(
                    ZV_[array_index], sizeof(index_t) * nb_in_ZV
                );
            }
        }
        if(lock) {
            unlock_array(array_index);
        }
    }
}
