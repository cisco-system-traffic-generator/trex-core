
#include <trex_stateless_dp_core.h>

class CFlowGenListPerThread;
class TrexStatelessCpToDpMsgBase;

TrexStatelessDpCore::TrexStatelessDpCore(unsigned char, CFlowGenListPerThread*) {}

void TrexStatelessDpCore::start(){}

void TrexStatelessDpCore::handle_cp_msg(TrexStatelessCpToDpMsgBase*) {}

void TrexStatelessDpCore::handle_pkt_event(CGenNode*) {}
