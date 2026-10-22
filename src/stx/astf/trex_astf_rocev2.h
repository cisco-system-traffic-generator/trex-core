/* Copyright (c) 2026 Cisco Systems, Inc. Licensed under the Apache License, Version 2.0. */
#ifndef TREX_ASTF_ROCEV2_H
#define TREX_ASTF_ROCEV2_H

#include "trex_messaging.h"
#include "json/json.h"
#include <atomic>
#include <memory>
#include <string>

class CFlowGenListPerThread;

/* Each command has one recipient.  The reply outlives a CP timeout and is
 * published with release/acquire ordering; the CP never reads live DP state. */
struct TrexAstfRoceV2Reply {
    std::atomic<bool> done;
    Json::Value result;
    std::string error;
    TrexAstfRoceV2Reply() : done(false) {}
};

class TrexAstfDpRoceV2 : public TrexCpToDpMsgBase {
public:
    TrexAstfDpRoceV2(const std::string &command, const Json::Value &config,
                    std::shared_ptr<TrexAstfRoceV2Reply> reply);
    TrexCpToDpMsgBase *clone() override;
    bool handle(TrexDpCore *core) override;
private:
    std::string m_command;
    Json::Value m_config;
    std::shared_ptr<TrexAstfRoceV2Reply> m_reply;
};

/* One endpoint engine per ASTF worker.  No engine, QP or frame-pool object is
 * shared by workers.  Only redirected DPDK mbuf ownership crosses a mailbox. */
class TrexAstfRoceV2Worker {
public:
    TrexAstfRoceV2Worker(uint8_t thread_id, CFlowGenListPerThread *core);
    ~TrexAstfRoceV2Worker();
    bool configured() const;
    bool active() const;
    bool poll();
    Json::Value command(const std::string &name, const Json::Value &config);
private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};
#endif
