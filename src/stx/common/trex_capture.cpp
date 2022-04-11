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
#include "trex_capture.h"
#include "trex_exception.h"

#ifndef TREX_SIM
#define ENDPOINT_PCAPNG
#endif

#include <zmq.h>
#ifdef ENDPOINT_PCAPNG
#include <pcapng_proto.h>
#endif

#include <linux/limits.h>   // PATH_MAX
#include <unistd.h>         // getcwd()
#include <sys/stat.h>
#include <thread>


class FileWriter: public EndpointWriter {
    FILE *m_file_handler;
    std::string m_filepath_tmp;

public:
    virtual ~FileWriter() {
        close();
        if (!m_filepath_tmp.empty()) {
            /* thread trick is to avoid watchdog during removing large size file */
            std::thread([](std::string file){ remove(file.c_str()); }, m_filepath_tmp).detach();
            m_filepath_tmp.clear();
        }
    }

    virtual bool Create(std::string &endpoint, std::string &err) {
        std::string prefix("file:///");
        if (endpoint.substr(0, prefix.length()) != prefix) {
            err = "file endpoint should start with " + prefix;
            return false;
        }

        std::string filepath = endpoint.substr(prefix.length());
        if (filepath.empty() || filepath[0] != '/') {
            /* convert to absolute path */
            char cpath[PATH_MAX];
            getcwd(cpath, PATH_MAX);
            filepath = std::string(cpath) + "/" + filepath;
        }
        if (filepath.substr(filepath.length()-1) == "/") {
            /* generate internal temporary filename when it is not specified */
            std::ostringstream filename;
            filename << "endpoint_" << reinterpret_cast<intptr_t>(this);

            filepath += filename.str();
            /* save path to remove later automatically */
            m_filepath_tmp = filepath;
        }

        struct stat buffer;
        if (stat(filepath.c_str(), &buffer) == 0) {
            err = "file '" + filepath + "' already exists.";
            return false;
        }

        m_file_handler = fopen(filepath.c_str(), "wb");
        if (!m_file_handler) {
            err = "create file '" + filepath + "' faild - " + std::to_string(errno);
            return false;
        }
        setbuf(m_file_handler, NULL);

        /* update by generated endpoint file path for the later response */
        endpoint = prefix + filepath;
        return true;
    }

    virtual bool is_open() const {
        return !!m_file_handler;
    }

    virtual void close() {
        if (m_file_handler) {
            /* this trick is to avoid watchdog during closing large size file */
            std::thread([](FILE* fp){ fclose(fp); }, m_file_handler).detach();
            m_file_handler = nullptr;
        }
    }

    virtual int write(const void *buf, size_t nbyte) {
        size_t size = fwrite(buf, nbyte, 1, m_file_handler);
        if (size != 1 && ferror(m_file_handler)) {
            clearerr(m_file_handler);
            close();
        }
        return size * nbyte;
    }
};


class ZmqWriter: public EndpointWriter {
    void *m_zeromq_ctx;
    void *m_zeromq_socket;

public:
    virtual ~ZmqWriter() {
        close();
    }

    virtual bool Create(std::string &endpoint, std::string &err) {
        std::string prefix("ipc://");
        if (endpoint.substr(0, prefix.length()) == prefix) {
            std::string path = endpoint.substr(prefix.length());
            struct stat buffer;
            if (stat(path.c_str(), &buffer) == -1) {
                err = "ipc socket '" + path + "' does not exists.";
                return false;
            }
        }

        m_zeromq_ctx = zmq_ctx_new();
        if ( !m_zeromq_ctx ) {
            err = "Could not create ZMQ context";
            return false;
        }
        m_zeromq_socket = zmq_socket(m_zeromq_ctx, ZMQ_PAIR);
        if ( !m_zeromq_socket ) {
            zmq_ctx_term(m_zeromq_ctx);
            m_zeromq_ctx = nullptr;
            err = "Could not create ZMQ socket";
            return false;
        }

        int linger = 0;
        zmq_setsockopt(m_zeromq_socket, ZMQ_LINGER, &linger, sizeof(linger));

        if ( zmq_connect(m_zeromq_socket, endpoint.c_str()) != 0 ) {
            zmq_close(m_zeromq_socket);
            zmq_ctx_term(m_zeromq_ctx);
            m_zeromq_socket = nullptr;
            m_zeromq_ctx = nullptr;
            err = "Could not connect to ZMQ socket";
            return false;
        }
        return true;
    }

    virtual bool is_open() const {
        return m_zeromq_ctx && m_zeromq_socket;
    }

    virtual void close() {
        if ( m_zeromq_socket ) {
            zmq_close(m_zeromq_socket);
            m_zeromq_socket = nullptr;
        }
        if ( m_zeromq_ctx ) {
            zmq_ctx_term(m_zeromq_ctx);
            m_zeromq_ctx = nullptr;
        }
    }

    virtual int write(const void *buf, size_t nbyte) {
        int rc = zmq_send(m_zeromq_socket, buf, nbyte, ZMQ_DONTWAIT);
        if (rc == -1 && errno != EAGAIN) {
            close();
        }
        return rc;
    }
};


#define ENDPOINT_BUFFER_SIZE    0x100000

EndpointBuffer::EndpointBuffer() {
    m_base = new uint8_t[ENDPOINT_BUFFER_SIZE];
    m_size = 0;
    m_reserved = 0;
    m_count = 0;
}

EndpointBuffer::~EndpointBuffer() {
    if (m_base) {
        delete[] m_base;
        m_base = nullptr;
    }
}

uint32_t EndpointBuffer::available_size() const {
    return ENDPOINT_BUFFER_SIZE - m_reserved;
}

uint8_t* EndpointBuffer::reserve_block(uint32_t size) {
    uint8_t* block = nullptr;
    if (size <= available_size()) {
        block = m_base + m_reserved;
        m_reserved += size;
    }
    return block;
}

void EndpointBuffer::consume_block(uint32_t size) {
    m_size += size;
    ++m_count;
}


#ifndef NSEC_PER_SEC
#define NSEC_PER_SEC    1000000000L
#endif
#define USEC_PER_SEC    1000000L

/**************************************
 * Capture Endpoint
 *
 * An endpoint to save captured packets
 *************************************/
CaptureEndpoint::CaptureEndpoint(const std::string& endpoint, uint16_t snaplen): m_endpoint(endpoint) {
    m_snaplen = snaplen;
    m_writer = nullptr;
    m_format = PCAP;

    /* save start time as UTC nanoseconds */
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    m_timestamp_ns = ts.tv_sec * NSEC_PER_SEC + ts.tv_nsec;
    m_timestamp_cycles = os_get_hr_tick_64();
    m_timestamp_last = m_timestamp_ns;

    m_packet_limit = 0;
    m_packet_count = 0;
    m_packet_bytes = 0;
}

CaptureEndpoint::~CaptureEndpoint() {
    if (m_writer) {
        delete m_writer;
        m_writer = nullptr;
    }
    for (auto buffer: m_buffers) {
        delete buffer;
    }
}

double CaptureEndpoint::get_start_ts() const {
    return (m_timestamp_ns*1.0) / NSEC_PER_SEC;
}

uint64_t CaptureEndpoint::get_timestamp_ns() const {
    uint64_t cycles = os_get_hr_tick_64() - m_timestamp_cycles;
    return  m_timestamp_ns + (cycles*NSEC_PER_SEC)/os_get_hr_freq();
}

uint8_t *
CaptureEndpoint::allocate_block(uint32_t size) {
    std::unique_lock<std::mutex> ulock(m_lock);

    if (m_buffers.empty() || size > m_buffers.back()->available_size()) {
        m_buffers.push_back(new EndpointBuffer());
    }

    return m_buffers.back()->reserve_block(size);
}

void
CaptureEndpoint::finalize_block(uint8_t* block, uint32_t size, uint32_t bytes) {
    std::unique_lock<std::mutex> ulock(m_lock);

    for(auto it = m_buffers.rbegin(); it != m_buffers.rend(); ++it) {
        if ((*it)->has_reserved(block)) {
            (*it)->consume_block(size);
            break;
        }
    }
    m_packet_count += 1;
    m_packet_bytes += bytes;
}

bool
CaptureEndpoint::Init(const CaptureFilter& filter, std::string &err) {
    assert(!m_endpoint.empty());

    size_t optpos = m_endpoint.find("?");
    if (optpos != std::string::npos) {
        std::string option = m_endpoint.substr(optpos+1);

        if (option == "pcap") {
            m_format = PCAP;
        } else if (option == "pcapng") {
            m_format = PCAPNG;
        } else {
            err = "upsuported format: " + option;
            return false;
        }
        m_endpoint = m_endpoint.substr(0, optpos);
    }

    size_t pos = m_endpoint.find("://");
    if (pos == std::string::npos) {
        err = "no endpoint protocol specified: " + m_endpoint;
        return false;
    }
    std::string proto = m_endpoint.substr(0, pos);
    if (proto.compare("file") == 0) {
        /* start with "file://" */
        m_writer = new FileWriter();
    } else {
        /* start with "tcp://" or "ipc://" */
        m_writer = new ZmqWriter();
    }

    /* to fill IDB for PCAPNG */
    uint64_t ports_bit = filter.get_tx_active_map() | filter.get_rx_active_map();
    for (int port_id = 0; ports_bit; ports_bit >>= 1, port_id++) {
        if (ports_bit & 1) {
            uint8_t index = (uint8_t) m_ports_map.size();
            m_ports_map[port_id] = index;
        }
    }

    bool done = m_writer->Create(m_endpoint, err);
    if (done && !write_header()) {
        if (done) {
            err = "Failed writing header to " + m_endpoint;
        }
        done = false;
    }

    if (!done) {
        delete m_writer;
        m_writer = nullptr;
    }
    return writer_is_active();
}

#ifdef ENDPOINT_PCAPNG
/* length of option including padding */
static uint16_t pcapng_optlen(uint16_t len)
{
    return RTE_ALIGN(sizeof(struct pcapng_option) + len,
                     sizeof(uint32_t));
}

/* build TLV option and return location of next */
static struct pcapng_option *
pcapng_add_option(struct pcapng_option *popt, uint16_t code,
                  const void *data, uint16_t len)
{
    popt->code = code;
    popt->length = len;
    memcpy(popt->data, data, len);

    return (struct pcapng_option *)((uint8_t *)popt + pcapng_optlen(len));
}

/* section header block */
static uint32_t pcapng_shb_length() {
    return sizeof(struct pcapng_section_header) + pcapng_optlen(0) + sizeof(uint32_t);
}

static uint32_t
pcapng_shb(uint8_t *buf) {
    uint32_t len = pcapng_shb_length();

    struct pcapng_section_header* shb = (struct pcapng_section_header*) buf;
    *shb = (struct pcapng_section_header) {
            .block_type = PCAPNG_SECTION_BLOCK,
            .block_length = len,
            .byte_order_magic = PCAPNG_BYTE_ORDER_MAGIC,
            .major_version = PCAPNG_MAJOR_VERS,
            .minor_version = PCAPNG_MINOR_VERS,
            .section_length = UINT64_MAX,
    };
    struct pcapng_option* opt = (struct pcapng_option *)(shb + 1);

    /* The standard requires last option to be OPT_END */
    opt = pcapng_add_option(opt, PCAPNG_OPT_END, NULL, 0);

    /* clone block_length after option */
    memcpy(opt, &shb->block_length, sizeof(uint32_t));

    return len;
}

/* interface description block */
static uint32_t pcapng_idb_length() {
    return sizeof(struct pcapng_interface_block) + sizeof(uint32_t);
}

static uint32_t
pcapng_idb(uint8_t* buf, uint32_t snaplen) {
    uint32_t len = pcapng_idb_length();

    struct pcapng_interface_block* idb = (struct pcapng_interface_block*) buf;
    *idb = (struct pcapng_interface_block) {
            .block_type = PCAPNG_INTERFACE_BLOCK,
            .block_length = len,
            .link_type = 1,         /* DLT_EN10MB - Ethernet */
            .reserved = 0,
            .snap_len = (uint32_t) (snaplen ? snaplen: 2000),
    };

    /* clone block_length */
    memcpy(idb + 1, &idb->block_length, sizeof(uint32_t));

    return len;
}
#endif /* ENDPOINT_PCAPNG */

bool
CaptureEndpoint::write_header() {
    if (!writer_is_active()) {
        return false;
    }

    if (m_format == PCAP) {
        packet_file_header_t header;

#define MAGIC_NUM_DONT_FLIP 0xa1b2c3d4  /* from common/pcap.cpp */
        header.magic   = MAGIC_NUM_DONT_FLIP;
        header.version_major  = 0x0002;
        header.version_minor  = 0x0004;
        header.thiszone       = 0;
        header.sigfigs        = 0;
        header.snaplen        = 2000;
        header.linktype       = 1;

        int rc = m_writer->write(&header, sizeof(header));
        if (rc != sizeof(header)) {
            return false;
        }
    }
#ifdef ENDPOINT_PCAPNG
    else if (m_format == PCAPNG) {
        int idbcnt = m_ports_map.size();
        uint32_t size = pcapng_shb_length() + idbcnt * pcapng_idb_length();
        uint8_t buf[size];

        uint32_t len = pcapng_shb(buf);

        for (int i = 0; i < idbcnt; i++) {
            len += pcapng_idb(buf + len, m_snaplen);
        }
        assert(size == len);

        int rc = m_writer->write(buf, len);
        if (rc != len) {
            return false;
        }
    }
#endif /* ENDPOINT_PCAPNG */

    return true;
}

static int
copy_packet_data(uint8_t *dest, const rte_mbuf_t *m, uint32_t caplen) {
    int index = 0;

    for (const rte_mbuf_t *it = m; it != NULL && (index < caplen); it = it->next) {
        int copy_len = caplen - index;
        copy_len = (copy_len > it->data_len) ? it->data_len: copy_len;

        const uint8_t *src = rte_pktmbuf_mtod(it, const uint8_t *);
        memcpy(dest + index, src, copy_len);

        index += copy_len;
    }

    return index;
}

#ifdef ENDPOINT_PCAPNG
static uint32_t
pcapng_option_flags(TrexPkt::origin_e origin) {
    uint32_t flags = 0;
    if (origin == TrexPkt::ORIGIN_TX) {
        flags = PCAPNG_IFB_OUTBOUND;
    } else if (origin == TrexPkt::ORIGIN_RX) {
        flags = PCAPNG_IFB_INBOUND;
    }
    return flags;
}

static uint32_t pcapng_packet_block_length(uint32_t datalen, uint16_t optlen) {
    return RTE_ALIGN(sizeof(struct pcapng_enhance_packet_block) + datalen + optlen + sizeof(uint32_t),
                     sizeof(uint32_t));
}
#endif /* ENDPOINT_PCAPNG */

void
CaptureEndpoint::write_packet(const rte_mbuf_t *m, int port, TrexPkt::origin_e origin, uint64_t index) {
    if (!writer_is_active() || (m_packet_limit && index > m_packet_limit)) {
        return;
    }

    uint64_t timestamp_ns = get_timestamp_ns();
    uint64_t timestamp_us = timestamp_ns/1000;

    uint32_t pktlen = m->pkt_len;
    uint32_t caplen = (m_snaplen && m_snaplen < pktlen) ? m_snaplen: pktlen;

    if (m_format == PCAP) {
        uint8_t* block = allocate_block(sizeof(sf_pkthdr_t) + caplen);

        sf_pkthdr_t* pkthdr = (sf_pkthdr_t*) block;
        pkthdr->len = pktlen;
        pkthdr->caplen = caplen;
        pkthdr->ts.msec = timestamp_us % USEC_PER_SEC;
        pkthdr->ts.sec  = timestamp_us / USEC_PER_SEC;

        uint32_t len = sizeof(*pkthdr);

        /* add packet data */
        len += copy_packet_data(block + len, m, caplen);

        finalize_block(block, len, pktlen);
    }
#ifdef ENDPOINT_PCAPNG
    else if (m_format == PCAPNG) {
        uint32_t flags = pcapng_option_flags(origin);
        uint32_t optlen = flags ? pcapng_optlen(sizeof(flags)) : 0;
        uint32_t size = pcapng_packet_block_length(caplen, optlen);

        uint8_t* block = allocate_block(size);

        /* fill header */
        struct pcapng_enhance_packet_block* epb = (struct pcapng_enhance_packet_block*) block;
        *epb = (struct pcapng_enhance_packet_block) {
                .block_type = PCAPNG_ENHANCED_PACKET_BLOCK,
                .block_length = size,
                .interface_id = m_ports_map[port],
                .timestamp_hi = (uint32_t)(timestamp_us >> 32),
                .timestamp_lo = (uint32_t)timestamp_us,
                .capture_length = caplen,
                .original_length = pktlen,
        };
        uint32_t len = sizeof(*epb);

        /* add packet data */
        caplen = copy_packet_data(block + len, m, caplen);
        len += RTE_ALIGN(caplen, sizeof(uint32_t));

        /* add options */
        if (flags) {
            pcapng_option* opt = (struct pcapng_option*) (block + len);
            pcapng_add_option(opt, PCAPNG_EPB_FLAGS, &flags, sizeof(flags));
            len += optlen;
        }

        /* clone block_length */
        memcpy(block + len, &epb->block_length, sizeof(uint32_t));
        len += sizeof(uint32_t);

        assert(len == size);
        finalize_block(block, len, pktlen);
    }
#endif /* ENDPOINT_PCAPNG */

    m_timestamp_last = timestamp_ns;
}

void
CaptureEndpoint::stop() {
    if (writer_is_active()) {
        m_writer->close();
    }
}

void
CaptureEndpoint::to_json_status(Json::Value &output) const {
    output["endpoint"] = get_name();

    output["count"] = get_packet_count();   // total packets in endpoint and buffers
    output["bytes"] = get_packet_bytes();   // total original bytes
    output["limit"] = get_packet_limit();
    output["snaplen"] = get_snaplen();

    output["format"] = (m_format == PCAPNG) ? "pcapng": "pcap";
}

int
CaptureEndpoint::flush_buffer(bool all) {
    if (m_buffers.empty()) {
        return 0;
    }
    /* last one is current working buffer */
    if (m_buffers.size() == 1 && all == false &&
        (get_timestamp_ns() - m_timestamp_last < NSEC_PER_SEC/2)) {
        return 0;
    }

    std::unique_lock<std::mutex> ulock(m_lock);

    EndpointBuffer* buffer = m_buffers[0];
    if (buffer->in_use()) { /* TX core is filling data in the buffer */
        return 0;
    }

    if (!writer_is_active() || buffer->get_count() == 0) {
        /* to empty m_buffers gracefully */
        delete buffer;
        m_buffers.erase(m_buffers.begin());
        return 0;
    }

    /* to prevent being used from allocate_block() */
    buffer->reserve_block(buffer->available_size());

    ulock.unlock();

    int block_count = 0;
    int rc = m_writer->write(buffer->get_base(), buffer->get_size());
    if (rc == buffer->get_size()) {
        block_count = buffer->get_count();
        delete buffer;

        ulock.lock();
        m_buffers.erase(m_buffers.begin());
    }

    return block_count;
}

uint32_t
CaptureEndpoint::get_element_count() const {
    std::unique_lock<std::mutex> ulock(m_lock);

    uint32_t count = 0;
    for (auto buffer: m_buffers) {
        count += buffer->get_count();
    }
    return count;
}


/**************************************
 * Capture
 *  
 * A single instance of a capture
 *************************************/
TrexCapture::TrexCapture(capture_id_t id,
                         uint64_t limit,
                         const CaptureFilter &filter,
                         TrexPktBuffer::mode_e mode,
                         CaptureEndpoint *endpoint) {
    m_id         = id;
    m_pkt_buffer = endpoint ? nullptr: new TrexPktBuffer(limit, mode);
    m_filter     = filter;
    m_state      = STATE_ACTIVE;
    m_start_ts   = endpoint ? endpoint->get_start_ts(): now_sec();
    m_pkt_index  = 0;
    m_pkt_count  = 0;
    m_endpoint   = endpoint;

    m_filter.compile();

    if (m_endpoint) {
        m_endpoint->set_packet_limit(limit);
    }
}

TrexCapture::~TrexCapture() {
    if (m_pkt_buffer) {
        delete m_pkt_buffer;
        m_pkt_buffer = nullptr;
    }
    if (m_endpoint) {
        delete m_endpoint;
        m_endpoint = nullptr;
    }
}

void
TrexCapture::handle_pkt(const rte_mbuf_t *m, int port, TrexPkt::origin_e origin) {
    
    if (m_state != STATE_ACTIVE) {
        return;
    }
    
    /* if not in filter - back off */
    if (!m_filter.match(port, origin, m)) {
        return;
    }

    ++m_pkt_index;

    if (has_endpoint()) {
        m_endpoint->write_packet(m, port, origin, m_pkt_index);
    } else {
        m_pkt_buffer->push(m, port, origin, m_pkt_index);
    }
}


Json::Value
TrexCapture::to_json() const {
    Json::Value output = Json::objectValue;

    output["id"]       = Json::UInt64(m_id);
    output["matched"]  = Json::UInt64(m_pkt_index);
    output["fetched"]  = Json::UInt64(m_pkt_count);
    output["filter"]   = m_filter.to_json();
    
    switch (m_state) {
    case STATE_ACTIVE:
        output["state"]  = "ACTIVE";
        break;
        
    case STATE_STOPPED:
        output["state"]  = "STOPPED";
        break;
        
    default:
        assert(0);
    }

    /* write the pkt buffer status */
    if (has_endpoint()) {
        m_endpoint->to_json_status(output);
    } else {
        m_pkt_buffer->to_json_status(output);
    }
    
    return output;
}

/**
 * flush pkt buffer in endpoint
 */
int
TrexCapture::flush_endpoint() {

    int flush_cnt = m_endpoint->flush_buffer(!is_active());

    m_pkt_count += flush_cnt;

    if (!is_active() && m_endpoint->is_empty()) {
        m_endpoint->stop();
    }

    return flush_cnt;
}

/**
 * fetch up to 'pkt_limit' from the capture
 * 
 */
TrexPktBuffer *
TrexCapture::fetch(uint32_t pkt_limit, uint32_t &pending) {

    /* if the total sum of packets is within the limit range - take it */
    if (m_pkt_buffer->get_element_count() <= pkt_limit) {
        TrexPktBuffer *current = m_pkt_buffer;
        m_pkt_buffer = new TrexPktBuffer(m_pkt_buffer->get_capacity(), m_pkt_buffer->get_mode());
        pending = 0;
        m_pkt_count += current->get_element_count();
        return current;
    }
    
    /* partial fetch - take a partial list */
    TrexPktBuffer *partial = m_pkt_buffer->pop_n(pkt_limit);
    pending  = m_pkt_buffer->get_element_count();
    m_pkt_count += partial->get_element_count();

    return partial;
}


/**************************************
 * Capture Manager 
 * handles all the captures 
 * in the system 
 *************************************/

/**
 * holds the global filter in the capture manager 
 * which ports in the entire system are monitored 
 */
void
TrexCaptureMngr::update_global_filter() {
    
    /* if no captures - clear global filter */
    if (m_captures.size() == 0) {
        m_global_filter = CaptureFilter();
        return;
    }
    
    /* copy the first one */
    CaptureFilter new_filter = CaptureFilter();

    /* add the rest */
    for (int i = 0; i < m_captures.size(); i++) {
        if (m_captures[i]->is_active()) {
            new_filter += m_captures[i]->get_filter();
        }
    }
  
    /* copy and compile */
    m_global_filter = new_filter;
    m_global_filter.compile();
}


/**
 * lookup a specific capture by ID
 */
TrexCapture *
TrexCaptureMngr::lookup(capture_id_t capture_id) const {
    
    for (int i = 0; i < m_captures.size(); i++) {
        if (m_captures[i]->get_id() == capture_id) {
            return m_captures[i];
        }
    }
    
    /* does not exist */
    return nullptr;
}


int
TrexCaptureMngr::lookup_index(capture_id_t capture_id) const {
    for (int i = 0; i < m_captures.size(); i++) {
        if (m_captures[i]->get_id() == capture_id) {
            return i;
        }
    }
    return -1;
}


/**
 * starts a new capture
 * 
 */
void
TrexCaptureMngr::start(const CaptureFilter &filter,
                                uint64_t limit,
                                TrexPktBuffer::mode_e mode,
                                uint16_t snaplen,
                                const std::string& endpoint,
                                TrexCaptureRCStart &rc) {

    /* check for maximum active captures */
    if (m_captures.size() >= MAX_CAPTURE_SIZE) {
        rc.set_err(TrexCaptureRC::RC_CAPTURE_LIMIT_REACHED);
        return;
    }

    /* create a new endpoint */
    CaptureEndpoint *new_endpoint = nullptr;
    if (!endpoint.empty()) {
        new_endpoint = new CaptureEndpoint(endpoint, snaplen);
        std::string err;
        if (!new_endpoint->Init(filter, err)) {
            delete new_endpoint;
            rc.set_err(TrexCaptureRC::RC_CAPTURE_INVALID_ENDPOINT, err);
            return;
        }
    }

    /* create a new capture*/
    int new_id = m_id_counter++;
    TrexCapture *new_capture = new TrexCapture(new_id, limit, filter, mode, new_endpoint);
    /**
     * add the new capture in a safe mode 
     * (TX might be active) 
     */
    std::unique_lock<std::mutex> ulock(m_lock);
    m_captures.push_back(new_capture);

    /* update global filter */
    update_global_filter();
    
    /* done with critical section */
    ulock.unlock();
    
    /* result */
    rc.set_rc(new_id, new_capture->get_start_ts());
}

void
TrexCaptureMngr::stop(capture_id_t capture_id, TrexCaptureRCStop &rc) {
    TrexCapture *capture = lookup(capture_id);
    if (!capture) {
        rc.set_err(TrexCaptureRC::RC_CAPTURE_NOT_FOUND);
        return;
    }
    
    std::unique_lock<std::mutex> ulock(m_lock);
    capture->stop();
    /* update global filter under lock (for barrier) */
    update_global_filter();

    /* done with critical section */
    ulock.unlock();
    
    rc.set_rc(capture->get_pkt_count(), capture->get_endpoint_name());
}


void
TrexCaptureMngr::fetch(capture_id_t capture_id, uint32_t pkt_limit, TrexCaptureRCFetch &rc) {
    TrexCapture *capture = lookup(capture_id);
    if (!capture) {
        rc.set_err(TrexCaptureRC::RC_CAPTURE_NOT_FOUND);
        return;
    }
    
    uint32_t pending = 0;
    TrexPktBuffer *pkt_buffer;

    if (capture->has_endpoint()) {
        /* while endpoint is active, fetching from client would be stalled */
        pkt_buffer = new TrexPktBuffer(0);
        pending = capture->get_pkt_count();
    } else {
        /* take a lock before fetching all the packets */
        std::unique_lock<std::mutex> ulock(m_lock);

        pkt_buffer = capture->fetch(pkt_limit, pending);

        ulock.unlock();
    }
    
    rc.set_rc(pkt_buffer, pending, capture->get_start_ts());
}

void
TrexCaptureMngr::remove(capture_id_t capture_id, TrexCaptureRCRemove &rc) {
    
    /* lookup index */
    int index = lookup_index(capture_id);
    if (index == -1) {
        rc.set_err(TrexCaptureRC::RC_CAPTURE_NOT_FOUND);
        return;
    }
    
    TrexCapture *capture =  m_captures[index];
    
    /* remove from list under lock */
    std::unique_lock<std::mutex> ulock(m_lock);
    
    m_captures.erase(m_captures.begin() + index);
    
    /* update global filter under lock (for barrier) */
    update_global_filter();
    
    /* done with critical section */
    ulock.unlock();
    
    /* free memory */
    delete capture;

    rc.set_rc();
}

void
TrexCaptureMngr::reset() {
    TrexCaptureRCRemove dummy;
    
    while (m_captures.size() > 0) {
        remove(m_captures[0]->get_id(), dummy);
        assert(!!dummy);
    }
}

/* define this macro to stress test the critical section */
//#define STRESS_TEST

void 
TrexCaptureMngr::handle_pkt_slow_path(const rte_mbuf_t *m, int port, TrexPkt::origin_e origin) {
    
    #ifdef STRESS_TEST
        static int sanity = 0;
        assert(__sync_fetch_and_add(&sanity, 1) == 0);
    #endif
    
    for (TrexCapture *capture : m_captures) {
        capture->handle_pkt(m, port, origin);
    }
    
    #ifdef STRESS_TEST
        assert(__sync_fetch_and_sub(&sanity, 1) == 1);
    #endif
}

bool
TrexCaptureMngr::tick() {
    dsec_t now = now_sec();
    if (m_tick_time_sec && now < m_tick_time_sec) {
        return false;
    }
    m_tick_time_sec = now + 0.001;  /* on every 1 msec */

    if (m_captures.empty()) {
        return false;
    }

    /* Frequent locks will reduce TX core performance.
     * Since TX cores are only reading m_captures,
     * the locks is not required for reading m_captures in RX core.
     * But each capture (endpoint) should have own locks for critical section.
     */

    uint64_t pkt_count = 0;
    for (TrexCapture *capture : m_captures) {
        if (capture->has_active_endpoint()) {
            pkt_count += capture->flush_endpoint();
        }
    }

    return !!pkt_count;
}

Json::Value
TrexCaptureMngr::to_json() {
    Json::Value lst = Json::arrayValue;
    
    std::unique_lock<std::mutex> ulock(m_lock);
    
    for (TrexCapture *capture : m_captures) {
        lst.append(capture->to_json());
    }

    ulock.unlock();
    
    return lst;
}

TrexCaptureMngr TrexCaptureMngr::g_instance;

