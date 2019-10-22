#ifndef __TREX_TIMESYNC_H__
#define __TREX_TIMESYNC_H__

#include "stl/trex_stl_fs.h"

/**************************************
 * RXTimesync
 *************************************/
class RXTimesync {
  public:
    RXTimesync(uint8_t timesync_method) { m_timesync_method = timesync_method; };

    void handle_pkt(const rte_mbuf_t *m, int port);

    Json::Value to_json() const;

  private:
    uint8_t m_timesync_method;
};

#endif
