/*
Copyright (c) 2026 Cisco Systems, Inc.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/

#ifndef __TREX_ROCEV2_NATIVE_STORAGE_H__
#define __TREX_ROCEV2_NATIVE_STORAGE_H__

#include "trex_rocev2_native_types.h"

#include <algorithm>
#include <limits>
#include <new>
#include <stdexcept>
#include <string.h>
#include <utility>
#include <vector>

namespace TrexRoceV2 {
namespace Native {

struct NativeStorageSnapshot {
    uint32_t capacity;
    uint32_t in_use;
    uint32_t high_watermark;
    uint32_t stride;
    uint64_t growth_events;

    NativeStorageSnapshot()
        : capacity(0), in_use(0), high_watermark(0), stride(0),
          growth_events(0) {
    }
};

class NativeFramePool;

/*
 * A packet value is either an intrusive reference into an admitted native
 * slab, or (only for control-plane/tests) a conventional owned vector.  The
 * production engine and transport paths always use the former.  Pool-backed
 * copies only adjust a single-owner, non-atomic reference count; packet bytes
 * remain immutable after publication.
 */
class NativeFrameBuffer {
public:
    NativeFrameBuffer()
        : m_pool(NULL), m_slot(INVALID_SLOT), m_length(0) {
    }

    NativeFrameBuffer(const NativeFrameBuffer &other)
        : m_pool(other.m_pool), m_slot(other.m_slot), m_length(other.m_length),
          m_owned(other.m_owned) {
        retain();
    }

    NativeFrameBuffer(NativeFrameBuffer &&other)
        : m_pool(other.m_pool), m_slot(other.m_slot), m_length(other.m_length),
          m_owned(std::move(other.m_owned)) {
        other.m_pool = NULL;
        other.m_slot = INVALID_SLOT;
        other.m_length = 0;
    }

    ~NativeFrameBuffer() {
        release();
    }

    NativeFrameBuffer &operator=(const NativeFrameBuffer &other) {
        if (this != &other) {
            release();
            m_pool = other.m_pool;
            m_slot = other.m_slot;
            m_length = other.m_length;
            m_owned = other.m_owned;
            retain();
        }
        return *this;
    }

    NativeFrameBuffer &operator=(NativeFrameBuffer &&other) {
        if (this != &other) {
            release();
            m_pool = other.m_pool;
            m_slot = other.m_slot;
            m_length = other.m_length;
            m_owned = std::move(other.m_owned);
            other.m_pool = NULL;
            other.m_slot = INVALID_SLOT;
            other.m_length = 0;
        }
        return *this;
    }

    const uint8_t *data() const;
    uint8_t *mutable_data();
    size_t size() const {
        return m_pool == NULL ? m_owned.size() : m_length;
    }
    bool empty() const {
        return size() == 0;
    }
    bool pooled() const {
        return m_pool != NULL;
    }
    uint32_t slot_id() const {
        return pooled() ? m_slot : INVALID_SLOT;
    }
    uint8_t operator[](size_t index) const {
        return data()[index];
    }

    /* Compatibility storage for test/control-plane constructed frames. */
    void assign(size_t count, uint8_t value) {
        release();
        m_owned.assign(count, value);
    }
    void assign(const uint8_t *first, const uint8_t *last) {
        release();
        m_owned.assign(first, last);
    }
    void clear() {
        release();
        m_owned.clear();
    }

private:
    friend class NativeFramePool;
    static const uint32_t INVALID_SLOT = std::numeric_limits<uint32_t>::max();

    void retain();
    void release();

    NativeFramePool *m_pool;
    uint32_t m_slot;
    size_t m_length;
    std::vector<uint8_t> m_owned;
};

class NativeFramePool {
public:
    NativeFramePool()
        : m_stride(0), m_free_count(0), m_in_use(0), m_high_watermark(0) {
    }

    NativeFramePool(const NativeFramePool &) = delete;
    NativeFramePool &operator=(const NativeFramePool &) = delete;

    ~NativeFramePool() {
        reset();
    }

    static bool estimate_storage_bytes(uint32_t capacity,
                                       uint32_t stride,
                                       uint64_t &bytes,
                                       Status *status = NULL);

    bool configure(uint32_t capacity, uint32_t stride, Status *status = NULL) {
        if (capacity == 0 || stride == 0) {
            return status_error(status,
                                ErrorCode::INVALID_ARGUMENT,
                                "native frame pool bounds must be non-zero");
        }
        if (capacity > std::numeric_limits<size_t>::max() / stride) {
            return status_error(status,
                                ErrorCode::OUT_OF_RANGE,
                                "native frame pool byte reservation overflowed");
        }
        try {
            std::vector<uint8_t> bytes(static_cast<size_t>(capacity) * stride);
            std::vector<Slot> slots(capacity);
            std::vector<uint32_t> free_slots(capacity);
            for (uint32_t index = 0; index < capacity; ++index) {
                free_slots[index] = capacity - index - 1U;
            }
            reset();
            m_bytes.swap(bytes);
            m_slots.swap(slots);
            m_free_slots.swap(free_slots);
            m_stride = stride;
            m_free_count = capacity;
            m_in_use = 0;
            m_high_watermark = 0;
        } catch (const std::bad_alloc &) {
            return status_error(status,
                                ErrorCode::RESOURCE_EXHAUSTED,
                                "native frame pool allocation failed during admission");
        } catch (const std::length_error &) {
            return status_error(status,
                                ErrorCode::OUT_OF_RANGE,
                                "native frame pool reservation exceeds container capacity");
        }
        status_ok(status);
        return true;
    }

    bool acquire(NativeFrameBuffer &buffer, Status *status = NULL) {
        if (m_free_count == 0) {
            return status_error(status,
                                ErrorCode::RESOURCE_EXHAUSTED,
                                "native frame pool is exhausted");
        }
        buffer.release();
        const uint32_t slot = m_free_slots[--m_free_count];
        m_slots[slot].references = 1;
        buffer.m_pool = this;
        buffer.m_slot = slot;
        buffer.m_length = 0;
        ++m_in_use;
        m_high_watermark = std::max(m_high_watermark, m_in_use);
        status_ok(status);
        return true;
    }

    bool copy(const uint8_t *data,
              size_t length,
              NativeFrameBuffer &buffer,
              Status *status = NULL) {
        if (length > m_stride || (length != 0 && data == NULL)) {
            return status_error(status,
                                ErrorCode::OUT_OF_RANGE,
                                "native frame does not fit its admitted pool stride");
        }
        NativeFrameBuffer candidate;
        if (!acquire(candidate, status)) {
            return false;
        }
        if (length != 0) {
            memcpy(candidate.mutable_data(), data, length);
        }
        candidate.m_length = length;
        buffer = std::move(candidate);
        status_ok(status);
        return true;
    }

    bool set_length(NativeFrameBuffer &buffer,
                    size_t length,
                    Status *status = NULL) {
        if (buffer.m_pool != this || length > m_stride) {
            return status_error(status,
                                ErrorCode::OUT_OF_RANGE,
                                "native frame length does not fit its owning pool");
        }
        buffer.m_length = length;
        status_ok(status);
        return true;
    }

    uint32_t capacity() const {
        return static_cast<uint32_t>(m_slots.size());
    }
    uint32_t available() const {
        return m_free_count;
    }
    uint32_t stride() const {
        return m_stride;
    }
    NativeStorageSnapshot snapshot() const {
        NativeStorageSnapshot result;
        result.capacity = capacity();
        result.in_use = m_in_use;
        result.high_watermark = m_high_watermark;
        result.stride = m_stride;
        return result;
    }

    void reset() {
        m_bytes.clear();
        m_slots.clear();
        m_free_slots.clear();
        m_stride = 0;
        m_free_count = 0;
        m_in_use = 0;
        m_high_watermark = 0;
    }
    void release_storage() {
        reset();
        std::vector<uint8_t>().swap(m_bytes);
        std::vector<Slot>().swap(m_slots);
        std::vector<uint32_t>().swap(m_free_slots);
    }

private:
    friend class NativeFrameBuffer;
    struct Slot {
        Slot() : references(0) {}
        uint32_t references;
    };

    static bool add_storage_reservation(uint64_t count,
                                        uint64_t bytes_per_item,
                                        uint64_t &total,
                                        Status *status,
                                        const char *description) {
        if ((count != 0 &&
             bytes_per_item > std::numeric_limits<uint64_t>::max() / count) ||
            count * bytes_per_item >
                std::numeric_limits<uint64_t>::max() - total) {
            return status_error(status,
                                ErrorCode::OUT_OF_RANGE,
                                std::string(description) +
                                    " storage reservation overflowed");
        }
        total += count * bytes_per_item;
        return true;
    }

    uint8_t *slot_data(uint32_t slot) {
        return m_bytes.data() + static_cast<size_t>(slot) * m_stride;
    }
    const uint8_t *slot_data(uint32_t slot) const {
        return m_bytes.data() + static_cast<size_t>(slot) * m_stride;
    }
    void retain(uint32_t slot) {
        ++m_slots[slot].references;
    }
    void release(uint32_t slot) {
        Slot &entry = m_slots[slot];
        if (--entry.references == 0) {
            m_free_slots[m_free_count++] = slot;
            --m_in_use;
        }
    }

    std::vector<uint8_t> m_bytes;
    std::vector<Slot> m_slots;
    std::vector<uint32_t> m_free_slots;
    uint32_t m_stride;
    uint32_t m_free_count;
    uint32_t m_in_use;
    uint32_t m_high_watermark;
};

inline bool NativeFramePool::estimate_storage_bytes(uint32_t capacity,
                                                    uint32_t stride,
                                                    uint64_t &bytes,
                                                    Status *status) {
    bytes = 0;
    if (capacity == 0 || stride == 0) {
        return status_error(status,
                            ErrorCode::INVALID_ARGUMENT,
                            "native frame pool bounds must be non-zero");
    }
    if (capacity > std::numeric_limits<size_t>::max() / stride ||
        capacity > std::numeric_limits<size_t>::max() / sizeof(Slot) ||
        capacity >
            std::numeric_limits<size_t>::max() / sizeof(uint32_t)) {
        return status_error(status,
                            ErrorCode::OUT_OF_RANGE,
                            "native frame pool reservation cannot be represented on this platform");
    }
    if (!add_storage_reservation(capacity,
                                 stride,
                                 bytes,
                                 status,
                                 "native frame pool byte slab") ||
        !add_storage_reservation(capacity,
                                 sizeof(Slot),
                                 bytes,
                                 status,
                                 "native frame pool slot table") ||
        !add_storage_reservation(capacity,
                                 sizeof(uint32_t),
                                 bytes,
                                 status,
                                 "native frame pool free-slot table")) {
        return false;
    }
    status_ok(status);
    return true;
}

inline const uint8_t *NativeFrameBuffer::data() const {
    return m_pool == NULL ? (m_owned.empty() ? NULL : m_owned.data())
                          : m_pool->slot_data(m_slot);
}

inline uint8_t *NativeFrameBuffer::mutable_data() {
    return m_pool == NULL ? (m_owned.empty() ? NULL : m_owned.data())
                          : m_pool->slot_data(m_slot);
}

inline void NativeFrameBuffer::retain() {
    if (m_pool != NULL) {
        m_pool->retain(m_slot);
    }
}

inline void NativeFrameBuffer::release() {
    if (m_pool != NULL) {
        m_pool->release(m_slot);
        m_pool = NULL;
        m_slot = INVALID_SLOT;
        m_length = 0;
    }
    m_owned.clear();
}

/* Fixed-capacity, allocation-free-after-configure FIFO. */
template <typename T>
class NativeBoundedRing {
public:
    NativeBoundedRing()
        : m_head(0), m_tail(0), m_count(0), m_high_watermark(0) {
    }

    NativeBoundedRing(const NativeBoundedRing &) = delete;
    NativeBoundedRing &operator=(const NativeBoundedRing &) = delete;

    bool configure(uint32_t capacity, Status *status = NULL) {
        if (capacity == 0) {
            return status_error(status,
                                ErrorCode::INVALID_ARGUMENT,
                                "native bounded ring capacity must be non-zero");
        }
        try {
            std::vector<T> slots(capacity);
            clear();
            m_slots.swap(slots);
            m_head = 0;
            m_tail = 0;
            m_count = 0;
            m_high_watermark = 0;
        } catch (const std::bad_alloc &) {
            return status_error(status,
                                ErrorCode::RESOURCE_EXHAUSTED,
                                "native bounded ring allocation failed during admission");
        } catch (const std::length_error &) {
            return status_error(status,
                                ErrorCode::OUT_OF_RANGE,
                                "native bounded ring reservation exceeds container capacity");
        }
        status_ok(status);
        return true;
    }

    bool push(T value, Status *status = NULL) {
        if (full()) {
            return status_error(status,
                                ErrorCode::RESOURCE_EXHAUSTED,
                                "native bounded ring is full");
        }
        m_slots[m_tail] = std::move(value);
        m_tail = (m_tail + 1U) % m_slots.size();
        ++m_count;
        m_high_watermark = std::max(
            m_high_watermark, static_cast<uint32_t>(m_count));
        status_ok(status);
        return true;
    }

    bool push_copy(const T &value, Status *status = NULL) {
        return push(T(value), status);
    }

    T &front() {
        return m_slots[m_head];
    }
    const T &front() const {
        return m_slots[m_head];
    }
    /* Peek without changing ownership; useful for bounded device TX bursts. */
    const T *at(size_t index) const {
        return index < m_count ? &m_slots[(m_head + index) % m_slots.size()]
                               : NULL;
    }
    void pop() {
        if (m_count == 0) {
            return;
        }
        m_slots[m_head] = T();
        m_head = (m_head + 1U) % m_slots.size();
        --m_count;
    }
    void clear() {
        while (m_count != 0) {
            pop();
        }
        m_head = 0;
        m_tail = 0;
    }
    void reset() {
        clear();
        std::vector<T>().swap(m_slots);
        m_head = 0;
        m_tail = 0;
        m_count = 0;
        m_high_watermark = 0;
    }
    bool empty() const {
        return m_count == 0;
    }
    bool full() const {
        return !m_slots.empty() && m_count == m_slots.size();
    }
    size_t size() const {
        return m_count;
    }
    size_t capacity() const {
        return m_slots.size();
    }
    uint32_t high_watermark() const {
        return m_high_watermark;
    }

private:
    std::vector<T> m_slots;
    size_t m_head;
    size_t m_tail;
    size_t m_count;
    uint32_t m_high_watermark;
};

} // namespace Native
} // namespace TrexRoceV2

#endif /* __TREX_ROCEV2_NATIVE_STORAGE_H__ */
