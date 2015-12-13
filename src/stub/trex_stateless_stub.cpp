
#include <trex_stateless_dp_core.h>

class CFlowGenListPerThread;
class TrexStatelessCpToDpMsgBase;

void
TrexStatelessDpCore::create(unsigned char, CFlowGenListPerThread*) {
    m_thread_id = 0;
    m_core = NULL;

    m_state = STATE_IDLE;

    CMessagingManager * cp_dp = CMsgIns::Ins()->getCpDp();

    m_ring_from_cp = cp_dp->getRingCpToDp(0);
    m_ring_to_cp   = cp_dp->getRingDpToCp(0);
}

void TrexStatelessDpCore::start(){}

void TrexStatelessDpCore::handle_cp_msg(TrexStatelessCpToDpMsgBase*) {}

