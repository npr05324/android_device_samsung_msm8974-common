/*
 * Copyright (C) 2026 The LineageOS Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <stdint.h>

int32_t android_atomic_acquire_load(volatile const int32_t* addr) {
    int32_t value = *addr;
    __atomic_thread_fence(__ATOMIC_ACQUIRE);
    return value;
}

int32_t android_atomic_release_load(volatile const int32_t* addr) {
    __atomic_thread_fence(__ATOMIC_RELEASE);
    return *addr;
}

void android_atomic_acquire_store(int32_t value, volatile int32_t* addr) {
    *addr = value;
    __atomic_thread_fence(__ATOMIC_ACQUIRE);
}

void android_atomic_release_store(int32_t value, volatile int32_t* addr) {
    __atomic_thread_fence(__ATOMIC_RELEASE);
    *addr = value;
}

int32_t android_atomic_inc(volatile int32_t* addr) {
    return __atomic_fetch_add(addr, 1, __ATOMIC_SEQ_CST);
}

int32_t android_atomic_dec(volatile int32_t* addr) {
    return __atomic_fetch_sub(addr, 1, __ATOMIC_SEQ_CST);
}

int32_t android_atomic_add(int32_t value, volatile int32_t* addr) {
    return __atomic_fetch_add(addr, value, __ATOMIC_SEQ_CST);
}

int32_t android_atomic_and(int32_t value, volatile int32_t* addr) {
    return __atomic_fetch_and(addr, value, __ATOMIC_SEQ_CST);
}

int32_t android_atomic_or(int32_t value, volatile int32_t* addr) {
    return __atomic_fetch_or(addr, value, __ATOMIC_SEQ_CST);
}

int32_t android_atomic_cmpxchg(int32_t oldvalue, int32_t newvalue, volatile int32_t* addr) {
    int32_t expected = oldvalue;
    return __atomic_compare_exchange_n(addr, &expected, newvalue, 0, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST) ? 0 : 1;
}
