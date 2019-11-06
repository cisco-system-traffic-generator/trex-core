#include "trex_timesync.h"

#include "trex_global.h"

/**************************************
 * RXTimesync
 *************************************/
// RXTimesync::RXTimesync() {}

void RXTimesync::handle_pkt(const rte_mbuf_t *m, int port) {
    if (m_timesync_method == CParserOption::TIMESYNC_PTP) {
        printf("Syncing time with PTP method (slave side).\n");
#ifdef _DEBUG
        printf("PTP time synchronisation is currently not supported (but we are working on that).\n");
#endif
    }
}

Json::Value RXTimesync::to_json() const { return Json::objectValue; }
