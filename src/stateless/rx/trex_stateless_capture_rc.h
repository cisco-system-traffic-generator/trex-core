/*
 Itay Marom
 Cisco Systems, Inc.
*/

/*
Copyright (c) 2015-2016 Cisco Systems, Inc.

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
#ifndef __TREX_STATELESS_CAPTURE_RC_H__
#define __TREX_STATELESS_CAPTURE_RC_H__

typedef int32_t capture_id_t;

/**
 * a base class for a capture command RC 
 * not to be used directly 
 */
class TrexCaptureRC {
    
protected:
    /* cannot instantiate this object from outside */
    TrexCaptureRC() {
        m_rc = RC_INVALID;
    }

public:

    /**
     * error types for commands
     */
    enum rc_e {
        RC_INVALID = 0,
        RC_OK = 1,
        RC_CAPTURE_NOT_FOUND,
        RC_CAPTURE_LIMIT_REACHED,
        RC_CAPTURE_FETCH_UNDER_ACTIVE
    };

    bool operator !() const {
        return (m_rc != RC_OK);
    }
    
    std::string get_err() const {
        assert(m_rc != RC_INVALID);
        
        switch (m_rc) {
        case RC_OK:
            return "";
        case RC_CAPTURE_LIMIT_REACHED:
            return "capture limit has reached";
        case RC_CAPTURE_NOT_FOUND:
            return "capture ID not found";
        case RC_CAPTURE_FETCH_UNDER_ACTIVE:
            return "fetch command cannot be executed on an active capture";
        case RC_INVALID:
            /* should never be called under invalid */
            assert(0);
            
        default:
            assert(0);
        }
    }
    
    void set_err(rc_e rc) {
        m_rc = rc;
    }
    
    
protected:
    rc_e m_rc;
};

/**
 * return code for executing capture start
 */
class TrexCaptureRCStart : public TrexCaptureRC {
public:

    void set_rc(capture_id_t new_id, dsec_t start_ts) {
        m_capture_id  = new_id;
        m_start_ts    = start_ts;
        m_rc          = RC_OK;
    }
    
    capture_id_t get_new_id() const {
        assert(m_rc == RC_OK);
        return m_capture_id;
    }
    
    dsec_t get_start_ts() const {
        assert(m_rc == RC_OK);
        return m_start_ts;
    }
    
private:
    capture_id_t  m_capture_id;
    dsec_t        m_start_ts;
};

/**
 * return code for exectuing capture stop
 */
class TrexCaptureRCStop : public TrexCaptureRC {
public:
    
    void set_rc(uint32_t pkt_count) {
        m_pkt_count = pkt_count;
        m_rc        = RC_OK;
    }
    
    uint32_t get_pkt_count() const {
        assert(m_rc == RC_OK);
        return m_pkt_count;
    }
    
private:
    uint32_t m_pkt_count;
};

/**
 * return code for executing capture fetch
 */
class TrexCaptureRCFetch : public TrexCaptureRC {
public:

    void set_rc(const TrexPktBuffer *pkt_buffer, uint32_t pending, dsec_t start_ts) {
        m_pkt_buffer  = pkt_buffer;
        m_pending     = pending;
        m_start_ts    = start_ts;
        m_rc          = RC_OK;
    }
    
    const TrexPktBuffer *get_pkt_buffer() const {
        assert(m_rc == RC_OK);
        return m_pkt_buffer;
    }
    
    uint32_t get_pending() const {
        assert(m_rc == RC_OK);
        return m_pending;
    }
    
    dsec_t get_start_ts() const {
        assert(m_rc == RC_OK);
        return m_start_ts;
    }
    
private:
    const TrexPktBuffer *m_pkt_buffer;
    uint32_t             m_pending;
    dsec_t               m_start_ts;
};



class TrexCaptureRCRemove : public TrexCaptureRC {
public:
    void set_rc() {
        m_rc = RC_OK;
    }
};


class TrexCaptureRCStatus : public TrexCaptureRC {
public:
    
    void set_rc(const Json::Value &json) {
        m_json = json;
        m_rc   = RC_OK;
    }
    
    const Json::Value & get_status() const {
        assert(m_rc == RC_OK);
        return m_json;
    }
    
private:
    Json::Value m_json;
};


#endif /* __TREX_STATELESS_CAPTURE_RC_H__ */

