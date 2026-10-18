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

/*
 * Runtime fixups for the Samsung sensors.msm8974.so blob.
 *
 * GeomagneticSensor::readEvents() writes a calibrated *and* an uncalibrated
 * event per input sample (or two "pending" events right after enable) but
 * only decrements the caller's count once.  When both magnetic sensors are
 * enabled it returns more events than it was asked for: the caller's buffer
 * overflows, the blob's pollEvents() count goes negative, every following
 * readEvents() fails with -EINVAL ("count is invalid(-1)") and pollEvents()
 * spins forever. All sensors (and auto-rotation) stop until reboot.
 *
 * Every sensor class's readEvents vtable slot is redirected to a wrapper that
 * never returns more than `count` events; any surplus is kept and delivered
 * on the next call (hasPendingEvents() reports it so it is not delayed).
 */

#define LOG_TAG "MultiHal"

#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#include <dlfcn.h>
#include <log/log.h>
#include <hardware/sensors.h>

#include "BlobFixups.h"

namespace {

typedef int (*read_events_t)(void* self, sensors_event_t* data, int count);
typedef bool (*has_pending_t)(const void* self);

// vptr points at _ZTV + 8: [0] D1, [1] D0, [2] readEvents, [3] hasPendingEvents
constexpr int kReadEventsSlot = 2;
constexpr int kHasPendingSlot = 3;

// Read at most this many samples from the blob per call; the blob may emit
// two events per sample plus two pending events.
constexpr int kMaxRequest = 32;
constexpr int kScratchSize = 2 * kMaxRequest + 2;

// One instance of each class exists in the blob, so state is per class.
struct ClassFixup {
    const char* vtable_symbol;
    read_events_t orig_read;
    has_pending_t orig_pending;
    sensors_event_t surplus[kScratchSize];
    int surplus_start;
    int surplus_count;
};

ClassFixup gFixups[] = {
    {"_ZTV9MetaEvent", nullptr, nullptr, {}, 0, 0},
    {"_ZTV19AccelerometerSensor", nullptr, nullptr, {}, 0, 0},
    {"_ZTV15GyroscopeSensor", nullptr, nullptr, {}, 0, 0},
    {"_ZTV17GeomagneticSensor", nullptr, nullptr, {}, 0, 0},
    {"_ZTV11LightSensor", nullptr, nullptr, {}, 0, 0},
    {"_ZTV10GripSensor", nullptr, nullptr, {}, 0, 0},
};

constexpr int kNumFixups = sizeof(gFixups) / sizeof(gFixups[0]);

// Called only from the blob's pollEvents() on the single poll thread.
template <int I>
int readEventsWrapper(void* self, sensors_event_t* data, int count) {
    ClassFixup& f = gFixups[I];
    if (count < 1) return -EINVAL;

    int n = 0;
    while (f.surplus_count > 0 && n < count) {
        data[n++] = f.surplus[f.surplus_start++];
        f.surplus_count--;
    }
    if (n == count) return n;
    f.surplus_start = 0;

    sensors_event_t scratch[kScratchSize];
    int want = count - n;
    if (want > kMaxRequest) want = kMaxRequest;
    int got = f.orig_read(self, scratch, want);
    if (got < 0) return n > 0 ? n : got;
    if (got > kScratchSize) {
        ALOGE("%s: readEvents returned %d events, dropping overflow", f.vtable_symbol, got);
        got = kScratchSize;
    }

    int take = got < count - n ? got : count - n;
    memcpy(&data[n], scratch, take * sizeof(sensors_event_t));
    n += take;
    if (got > take) {
        f.surplus_count = got - take;
        memcpy(f.surplus, &scratch[take], f.surplus_count * sizeof(sensors_event_t));
    }
    return n;
}

template <int I>
bool hasPendingWrapper(const void* self) {
    return gFixups[I].surplus_count > 0 || gFixups[I].orig_pending(self);
}

read_events_t kReadWrappers[] = {
    readEventsWrapper<0>, readEventsWrapper<1>, readEventsWrapper<2>,
    readEventsWrapper<3>, readEventsWrapper<4>, readEventsWrapper<5>,
};
has_pending_t kPendingWrappers[] = {
    hasPendingWrapper<0>, hasPendingWrapper<1>, hasPendingWrapper<2>,
    hasPendingWrapper<3>, hasPendingWrapper<4>, hasPendingWrapper<5>,
};

bool patchSlot(void** slot, void* value) {
    uintptr_t page = sysconf(_SC_PAGESIZE);
    uintptr_t start = reinterpret_cast<uintptr_t>(slot) & ~(page - 1);
    uintptr_t end = (reinterpret_cast<uintptr_t>(slot + 1) + page - 1) & ~(page - 1);
    // The vtables live in RELRO, which is read-only after relocation.
    if (mprotect(reinterpret_cast<void*>(start), end - start, PROT_READ | PROT_WRITE)) {
        ALOGE("mprotect(rw) failed: %s", strerror(errno));
        return false;
    }
    *slot = value;
    mprotect(reinterpret_cast<void*>(start), end - start, PROT_READ);
    return true;
}

}  // namespace

void apply_sensor_blob_fixups(void* lib_handle) {
    for (int i = 0; i < kNumFixups; i++) {
        ClassFixup& f = gFixups[i];
        if (f.orig_read != nullptr) continue;
        void** vtable = static_cast<void**>(dlsym(lib_handle, f.vtable_symbol));
        if (vtable == nullptr) continue;
        void** vptr = vtable + 2;
        read_events_t orig_read = reinterpret_cast<read_events_t>(vptr[kReadEventsSlot]);
        has_pending_t orig_pending = reinterpret_cast<has_pending_t>(vptr[kHasPendingSlot]);
        if (orig_read == nullptr || orig_pending == nullptr) continue;
        f.orig_read = orig_read;
        f.orig_pending = orig_pending;
        if (!patchSlot(&vptr[kReadEventsSlot], reinterpret_cast<void*>(kReadWrappers[i])) ||
            !patchSlot(&vptr[kHasPendingSlot], reinterpret_cast<void*>(kPendingWrappers[i]))) {
            f.orig_read = nullptr;
            continue;
        }
        ALOGI("Patched %s readEvents", f.vtable_symbol);
    }
}
