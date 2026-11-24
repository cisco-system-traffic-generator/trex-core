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

#include "trex_rocev2_native_types.h"

#include <sstream>

namespace TrexRoceV2 {
namespace Native {

Status::Status() : m_code(ErrorCode::OK) {
}

Status::Status(ErrorCode code, const std::string &message) :
    m_code(code),
    m_message(message) {
}

bool Status::ok() const {
    return m_code == ErrorCode::OK;
}

ErrorCode Status::code() const {
    return m_code;
}

const std::string &Status::message() const {
    return m_message;
}

void Status::clear() {
    m_code = ErrorCode::OK;
    m_message.clear();
}

void Status::set(ErrorCode code, const std::string &message) {
    m_code = code;
    m_message = message;
}

void status_ok(Status *status) {
    if (status != NULL) {
        status->clear();
    }
}

bool status_error(Status *status, ErrorCode code, const std::string &message) {
    if (status != NULL) {
        status->set(code, message);
    }
    return false;
}

const char *error_code_to_string(ErrorCode code) {
    switch (code) {
    case ErrorCode::OK:
        return "ok";
    case ErrorCode::INVALID_ARGUMENT:
        return "invalid_argument";
    case ErrorCode::INVALID_STATE:
        return "invalid_state";
    case ErrorCode::STALE_GENERATION:
        return "stale_generation";
    case ErrorCode::RESOURCE_EXHAUSTED:
        return "resource_exhausted";
    case ErrorCode::NOT_FOUND:
        return "not_found";
    case ErrorCode::DUPLICATE:
        return "duplicate";
    case ErrorCode::OUT_OF_RANGE:
        return "out_of_range";
    case ErrorCode::ACCESS_DENIED:
        return "access_denied";
    case ErrorCode::PROTOCOL_ERROR:
        return "protocol_error";
    case ErrorCode::INVALID_REQUEST:
        return "invalid_request";
    case ErrorCode::REMOTE_ACCESS_ERROR:
        return "remote_access_error";
    case ErrorCode::REMOTE_OPERATION_ERROR:
        return "remote_operation_error";
    case ErrorCode::RETRY_EXHAUSTED:
        return "retry_exhausted";
    case ErrorCode::RNR_RETRY_EXHAUSTED:
        return "rnr_retry_exhausted";
    case ErrorCode::TIMEOUT:
        return "timeout";
    case ErrorCode::ABORTED:
        return "aborted";
    case ErrorCode::INTEGRITY_ERROR:
        return "integrity_error";
    case ErrorCode::UNSUPPORTED:
        return "unsupported";
    case ErrorCode::INTERNAL_ERROR:
        return "internal_error";
    }
    return "unknown";
}

SessionGeneration::SessionGeneration() : m_value(0) {
}

SessionGeneration::SessionGeneration(uint64_t value) : m_value(value) {
}

bool SessionGeneration::valid() const {
    return m_value != 0;
}

uint64_t SessionGeneration::value() const {
    return m_value;
}

bool SessionGeneration::operator==(const SessionGeneration &other) const {
    return m_value == other.m_value;
}

bool SessionGeneration::operator!=(const SessionGeneration &other) const {
    return !(*this == other);
}

bool SessionGeneration::operator<(const SessionGeneration &other) const {
    return m_value < other.m_value;
}

QpId::QpId() : m_qp_index(INVALID_QP_INDEX) {
}

QpId::QpId(const SessionGeneration &generation, uint32_t qp_index) :
    m_generation(generation),
    m_qp_index(qp_index) {
}

bool QpId::valid() const {
    return m_generation.valid() && m_qp_index != INVALID_QP_INDEX;
}

const SessionGeneration &QpId::generation() const {
    return m_generation;
}

uint32_t QpId::qp_index() const {
    return m_qp_index;
}

bool QpId::operator==(const QpId &other) const {
    return m_generation == other.m_generation && m_qp_index == other.m_qp_index;
}

bool QpId::operator!=(const QpId &other) const {
    return !(*this == other);
}

bool QpId::operator<(const QpId &other) const {
    if (m_generation != other.m_generation) {
        return m_generation < other.m_generation;
    }
    return m_qp_index < other.m_qp_index;
}

WorkId::WorkId() : m_sequence(0) {
}

WorkId::WorkId(const QpId &qp, uint64_t sequence) :
    m_qp(qp),
    m_sequence(sequence) {
}

bool WorkId::valid() const {
    return m_qp.valid() && m_sequence != 0;
}

const QpId &WorkId::qp() const {
    return m_qp;
}

uint64_t WorkId::sequence() const {
    return m_sequence;
}

bool WorkId::belongs_to(const QpId &qp) const {
    return valid() && m_qp == qp;
}

std::string WorkId::to_string() const {
    std::ostringstream out;
    out << m_qp.generation().value() << ':' << m_qp.qp_index() << ':' << m_sequence;
    return out.str();
}

bool WorkId::operator==(const WorkId &other) const {
    return m_qp == other.m_qp && m_sequence == other.m_sequence;
}

bool WorkId::operator!=(const WorkId &other) const {
    return !(*this == other);
}

bool WorkId::operator<(const WorkId &other) const {
    if (m_qp != other.m_qp) {
        return m_qp < other.m_qp;
    }
    return m_sequence < other.m_sequence;
}

bool psn24_valid(uint32_t psn) {
    return (psn & ~PSN24_MASK) == 0;
}

bool psn24_add(uint32_t base, int64_t delta, uint32_t &result, Status *status) {
    if (!psn24_valid(base)) {
        return status_error(status, ErrorCode::INVALID_ARGUMENT, "base PSN exceeds 24 bits");
    }

    const int64_t space = static_cast<int64_t>(PSN24_SPACE);
    int64_t reduced = delta % space;
    int64_t sum = static_cast<int64_t>(base) + reduced;
    if (sum < 0) {
        sum += space;
    } else if (sum >= space) {
        sum -= space;
    }
    result = static_cast<uint32_t>(sum);
    status_ok(status);
    return true;
}

bool psn24_forward_distance(uint32_t from,
                            uint32_t to,
                            uint32_t &distance,
                            Status *status) {
    if (!psn24_valid(from) || !psn24_valid(to)) {
        return status_error(status, ErrorCode::INVALID_ARGUMENT, "PSN exceeds 24 bits");
    }
    distance = (to - from) & PSN24_MASK;
    status_ok(status);
    return true;
}

PsnOrder psn24_compare(uint32_t left, uint32_t right) {
    if (!psn24_valid(left) || !psn24_valid(right)) {
        return PsnOrder::INVALID;
    }

    const uint32_t delta = (left - right) & PSN24_MASK;
    if (delta == 0) {
        return PsnOrder::EQUAL;
    }
    if (delta == PSN24_HALF_SPACE) {
        return PsnOrder::AMBIGUOUS;
    }
    return (delta < PSN24_HALF_SPACE) ? PsnOrder::AFTER : PsnOrder::BEFORE;
}

bool psn24_in_closed_interval(uint32_t value,
                              uint32_t first,
                              uint32_t last,
                              bool &contains,
                              Status *status) {
    uint32_t span = 0;
    uint32_t offset = 0;
    if (!psn24_forward_distance(first, last, span, status) ||
        !psn24_forward_distance(first, value, offset, status)) {
        contains = false;
        return false;
    }
    if (span > PSN24_MAX_SERIAL_SPAN) {
        contains = false;
        return status_error(status,
                            ErrorCode::INVALID_ARGUMENT,
                            "PSN interval is ambiguous or exceeds half the serial space");
    }
    contains = offset <= span;
    status_ok(status);
    return true;
}

const char *psn_order_to_string(PsnOrder order) {
    switch (order) {
    case PsnOrder::BEFORE:
        return "before";
    case PsnOrder::EQUAL:
        return "equal";
    case PsnOrder::AFTER:
        return "after";
    case PsnOrder::AMBIGUOUS:
        return "ambiguous";
    case PsnOrder::INVALID:
        return "invalid";
    }
    return "unknown";
}

} // namespace Native
} // namespace TrexRoceV2
