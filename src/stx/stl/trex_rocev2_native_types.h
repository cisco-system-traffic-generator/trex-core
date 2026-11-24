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

#ifndef __TREX_ROCEV2_NATIVE_TYPES_H__
#define __TREX_ROCEV2_NATIVE_TYPES_H__

#include <stdint.h>

#include <string>

namespace TrexRoceV2 {
namespace Native {

static const uint32_t PSN24_MASK = 0x00ffffffU;
static const uint32_t PSN24_SPACE = 0x01000000U;
static const uint32_t PSN24_HALF_SPACE = 0x00800000U;
static const uint32_t PSN24_MAX_SERIAL_SPAN = PSN24_HALF_SPACE - 1U;
static const uint32_t INVALID_QP_INDEX = 0xffffffffU;

/*
 * Native dataplane methods return Status instead of throwing.  Exceptions may
 * still be translated at a control-plane boundary, but packet processing and
 * timer paths must remain deterministic under allocation or protocol errors.
 */
enum class ErrorCode {
    OK = 0,
    INVALID_ARGUMENT,
    INVALID_STATE,
    STALE_GENERATION,
    RESOURCE_EXHAUSTED,
    NOT_FOUND,
    DUPLICATE,
    OUT_OF_RANGE,
    ACCESS_DENIED,
    PROTOCOL_ERROR,
    INVALID_REQUEST,
    REMOTE_ACCESS_ERROR,
    REMOTE_OPERATION_ERROR,
    RETRY_EXHAUSTED,
    RNR_RETRY_EXHAUSTED,
    TIMEOUT,
    ABORTED,
    INTEGRITY_ERROR,
    UNSUPPORTED,
    INTERNAL_ERROR
};

class Status {
public:
    Status();
    Status(ErrorCode code, const std::string &message);

    bool ok() const;
    ErrorCode code() const;
    const std::string &message() const;

    void clear();
    void set(ErrorCode code, const std::string &message);

private:
    ErrorCode m_code;
    std::string m_message;
};

void status_ok(Status *status);
bool status_error(Status *status, ErrorCode code, const std::string &message);
const char *error_code_to_string(ErrorCode code);

/*
 * A zero generation is deliberately invalid.  Session owners must allocate a
 * new non-zero value before publishing QP handles, preventing stale work from
 * an earlier session from being accepted after resource reuse.
 */
class SessionGeneration {
public:
    SessionGeneration();
    explicit SessionGeneration(uint64_t value);

    bool valid() const;
    uint64_t value() const;

    bool operator==(const SessionGeneration &other) const;
    bool operator!=(const SessionGeneration &other) const;
    bool operator<(const SessionGeneration &other) const;

private:
    uint64_t m_value;
};

/* A generation-qualified logical QP identity.  It is not a wire QPN. */
class QpId {
public:
    QpId();
    QpId(const SessionGeneration &generation, uint32_t qp_index);

    bool valid() const;
    const SessionGeneration &generation() const;
    uint32_t qp_index() const;

    bool operator==(const QpId &other) const;
    bool operator!=(const QpId &other) const;
    bool operator<(const QpId &other) const;

private:
    SessionGeneration m_generation;
    uint32_t m_qp_index;
};

/*
 * Work IDs are never inferred from a PSN: PSNs wrap after 2^24 packets.  The
 * monotonically increasing sequence disambiguates reuse while QpId rejects a
 * completion or timer event belonging to an older session generation.
 */
class WorkId {
public:
    WorkId();
    WorkId(const QpId &qp, uint64_t sequence);

    bool valid() const;
    const QpId &qp() const;
    uint64_t sequence() const;
    bool belongs_to(const QpId &qp) const;
    std::string to_string() const;

    bool operator==(const WorkId &other) const;
    bool operator!=(const WorkId &other) const;
    bool operator<(const WorkId &other) const;

private:
    QpId m_qp;
    uint64_t m_sequence;
};

enum class PsnOrder {
    BEFORE = -1,
    EQUAL = 0,
    AFTER = 1,
    AMBIGUOUS = 2,
    INVALID = 3
};

bool psn24_valid(uint32_t psn);

/* Add a signed delta modulo 2^24. */
bool psn24_add(uint32_t base, int64_t delta, uint32_t &result, Status *status = NULL);

/* Forward modular distance in the range 0..2^24-1. */
bool psn24_forward_distance(uint32_t from,
                            uint32_t to,
                            uint32_t &distance,
                            Status *status = NULL);

/*
 * RFC-1982-style serial comparison.  Values exactly half the serial space
 * apart are AMBIGUOUS; callers must not invent an ordering for that case.
 */
PsnOrder psn24_compare(uint32_t left, uint32_t right);

/*
 * Return whether value lies in the closed forward interval [first, last].
 * The interval must span less than half of the PSN space.
 */
bool psn24_in_closed_interval(uint32_t value,
                              uint32_t first,
                              uint32_t last,
                              bool &contains,
                              Status *status = NULL);

const char *psn_order_to_string(PsnOrder order);

} // namespace Native
} // namespace TrexRoceV2

#endif /* __TREX_ROCEV2_NATIVE_TYPES_H__ */
