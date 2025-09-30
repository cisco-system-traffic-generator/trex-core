/*
 * Copyright (c) 2004-2021 Mellanox Technologies LTD. All rights reserved.
 * Copyright (c) 2021-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 *
 * This software is available to you under the terms of the
 * OpenIB.org BSD license included below:
 *
 *     Redistribution and use in source and binary forms, with or
 *     without modification, are permitted provided that the following
 *     conditions are met:
 *
 *      - Redistributions of source code must retain the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer.
 *
 *      - Redistributions in binary form must reproduce the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer in the documentation and/or other materials
 *        provided with the distribution.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 */


#ifndef IBIS_H_
#define IBIS_H_


#include "ext_umad.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <string>
#include <vector>
#include <map>
#include <list>
#include <fstream>      // std::ifstream
#include <stdexcept>
#include <queue>
using namespace std;

#include "ibis_types.h"
#include "ibis_mads_stat.h"
#include "mkey_mngr.h"
#include "key_mngr.h"
#include "csv_parser.h"
#include <infiniband/ibis/memory_pool.h>

class VerbsPort;

#define IB_SLT_UNASSIGNED 255

struct node_addr_t {
    direct_route_t m_direct_route;
    u_int16_t      m_lid;
    bool operator < (const node_addr_t& y) const
    {
        if (m_lid != y.m_lid)
            return m_lid < y.m_lid;
        if (m_direct_route.length != y.m_direct_route.length)
            return m_direct_route.length < y.m_direct_route.length;
        return memcmp(m_direct_route.path.BYTE, y.m_direct_route.path.BYTE,
                      m_direct_route.length) < 0;
    }
};


struct ibis_log
{
    static void ibis_log_msg_function(const char *file_name,
                                      unsigned line_num,
                                      const char *function_name,
                                      int level,
                                      const char *format, ...);

    static void ibis_log_mad_function(dump_data_func_t dump_func,
                                      void *mad_obj,
                                      bool msg_send_mad);
};

/*
struct less_node_addr {
  bool operator() (const node_addr_t* x, const node_addr_t* y) const
  {
      if (x->m_lid != y->m_lid)
          return x->m_lid < y->m_lid;
      if (x->m_direct_route.length != y->m_direct_route.length)
          return x->m_direct_route.length < y->m_direct_route.length;
      return memcmp(x->m_direct_route.path.BYTE, y->m_direct_route.path.BYTE,
                    x->m_direct_route.length) < 0;
  }
};
*/

struct transaction_data_t;

struct pending_mad_data_t {
    u_int8_t* m_umad;
    unsigned int m_umad_size;
    u_int8_t m_mgmt_class;
    transaction_data_t * m_transaction_data;

    pending_mad_data_t():m_umad(NULL),m_umad_size(0),
                         m_mgmt_class(0),
                         m_transaction_data(NULL) {}

    ~pending_mad_data_t() {
        delete[] m_umad;
    }

    int init();
};


struct ib_address_t {
    u_int16_t m_lid;
    u_int32_t m_qp;
    u_int32_t m_qkey;
    u_int8_t m_sl;

    ib_address_t(u_int16_t lid, u_int32_t qp, u_int32_t qkey, u_int8_t sl):
            m_lid(lid), m_qp(qp), m_qkey(qkey), m_sl(sl) {}
};


typedef void (*mad_handler_callback_func_t)
    (ib_address_t *p_ib_address, void *p_class_data, void *p_attribute_data,
     void *context);

struct mad_handler_t {
    unpack_data_func_t          m_unpack_class_data_func;
    dump_data_func_t            m_dump_class_data_func;
    unpack_data_func_t          m_unpack_attribute_data_func;
    dump_data_func_t            m_dump_attribute_data_func;
    mad_handler_callback_func_t m_callback_func;
    void                       *m_context;
    u_int8_t                     m_data_offset;
};

typedef std::map<pair<u_int16_t, u_int8_t>, mad_handler_t> attr_method_pair_to_handler_map_t;

typedef list<pending_mad_data_t *>  pending_mads_on_node_t;

struct transaction_data_t {
    u_int8_t           m_data_offset;
    unpack_data_func_t m_unpack_class_data_func;
    dump_data_func_t   m_dump_class_data_func;
    unpack_data_func_t m_unpack_attribute_data_func;
    dump_data_func_t   m_dump_attribute_data_func;
    bool               m_is_smp;
    clbck_data_t       m_clbck_data;
    pending_mads_on_node_t *m_pending_mads;

    transaction_data_t() :
            m_data_offset(0),
            m_unpack_class_data_func(NULL),
            m_dump_class_data_func(NULL),
            m_unpack_attribute_data_func(NULL),
            m_dump_attribute_data_func(NULL),
            m_is_smp(false),
            m_clbck_data(),
            m_pending_mads(NULL) {}

    int init(){return 0;} //for MemoryPool usage

    void Set(u_int8_t data_offset,
             const unpack_data_func_t unpack_class_data_func,
             const dump_data_func_t   dump_class_data_func,
             const unpack_data_func_t unpack_attribute_data_func,
             const dump_data_func_t   dump_attribute_data_func,
             bool is_smp,
             const clbck_data_t &clbck_data,
             pending_mads_on_node_t *pending_mads)
    {
        m_data_offset = data_offset;
        m_unpack_class_data_func = unpack_class_data_func;
        m_dump_class_data_func = dump_class_data_func;
        m_unpack_attribute_data_func = unpack_attribute_data_func;
        m_dump_attribute_data_func = dump_attribute_data_func;
        m_is_smp = is_smp;
        m_clbck_data = clbck_data;
        m_pending_mads = pending_mads;
        // reset timing
        memset(&(m_clbck_data.m_stat), 0xFF, sizeof(m_clbck_data.m_stat));
        // set smp in clbck
        m_clbck_data.m_stat.m_is_smp = m_is_smp;
    }



    /*
    transaction_data_t(
        u_int8_t data_offset,
        const unpack_data_func_t unpack_class_data_func,
        const dump_data_func_t   dump_class_data_func,
        const unpack_data_func_t unpack_attribute_data_func,
        const dump_data_func_t   dump_attribute_data_func,
        bool is_smp,
        const clbck_data_t &clbck_data,
        pending_mads_on_node_t *pending_mads) :
            m_data_offset(data_offset),
            m_unpack_class_data_func(unpack_class_data_func),
            m_dump_class_data_func(dump_class_data_func),
            m_unpack_attribute_data_func(unpack_attribute_data_func),
            m_dump_attribute_data_func(dump_attribute_data_func),
            m_is_smp(is_smp),
            m_clbck_data(clbck_data),
            m_pending_mads(pending_mads) {}
            */

    /*
    transaction_data_t& operator=(const transaction_data_t& transaction_data)
    {
        m_data_offset = transaction_data.m_data_offset;
        m_unpack_class_data_func = transaction_data.m_unpack_class_data_func;
        m_dump_class_data_func = transaction_data.m_dump_class_data_func;
        m_unpack_attribute_data_func = transaction_data.m_unpack_attribute_data_func;
        m_dump_attribute_data_func = transaction_data.m_dump_attribute_data_func;
        m_is_smp = transaction_data.m_is_smp;
        m_handle_data_func = transaction_data.m_handle_data_func;
        m_p_obj = transaction_data.m_p_obj;
        m_p_key = transaction_data.m_p_key;
        return *this;
    }
    */

};

struct port_properties_t {
    uint16_t base_lid;
    uint16_t sm_lid;
    uint64_t port_guid;
    uint64_t subnet_prefix;
    uint32_t state;

    port_properties_t(): base_lid(0), sm_lid(0), port_guid(0), subnet_prefix(0), state(0) {}
};

typedef struct umad_port_info_t {
    string dev_name;
    phys_port_t port_num;
    umad_port_t umad_port;
    bool umad_get_port_done;
    int umad_port_id;                                             /* file descriptor returned by umad_open() */
    int umad_agents_by_class[IBIS_IB_MAX_MAD_CLASSES]
                            [IBIS_IB_MAX_CLASS_VERSION_SUPP + 1]; /* array to map class --> agent */
    umad_port_info_t() : port_num(0), umad_port(), umad_get_port_done(false),
                         umad_port_id(-1)
    {
        for (int mgmt_class = 0; mgmt_class < IBIS_IB_MAX_MAD_CLASSES; ++mgmt_class) {
            for (uint8_t version = 0; version <= IBIS_IB_MAX_CLASS_VERSION_SUPP; ++version) {
                    this->umad_agents_by_class[mgmt_class][version] = -1;
            }
        }
    }
} umad_port_info_t;

typedef map < u_int32_t, transaction_data_t *>  transactions_map_t;
typedef map < node_addr_t, pending_mads_on_node_t >  mads_on_node_map_t;
typedef list < transaction_data_t *>  transactions_queue_t;
typedef list < u_int8_t > methods_list_t;
typedef vector < u_int8_t > class_versions_vec_t;

typedef list < u_int16_t > device_id_list_t;

typedef void(*ibis_log_msg_function_t)(const char *file_name, unsigned line_num,
                                       const char *function_name, int level,
                                       const char *format, ...);

typedef void(*ibis_log_mad_function_t)(dump_data_func_t dump_func,
                                       void *mad_obj, bool msg_send_mad);


/****************************************************/
const char * get_ibis_version();

/****************************************************/
class Ibis {
public:
    IbisMadsStat  m_mads_stat;

private:
    ////////////////////
    //members
    ////////////////////
    umad_port_info_t smp_port;
    umad_port_info_t gmp_port;

    enum {NOT_INITILIAZED, NOT_SET_PORT, READY} ibis_status;
    string last_error;

    void *p_umad_buffer_send;       /* buffer for send mad - sync buffer */
    void *p_umad_buffer_recv;       /* buffer for recv mad - sync buffer */
    u_int8_t *p_pkt_send;           /* mad pkt to send */
    u_int8_t *p_pkt_recv;           /* mad pkt was received */
    u_int64_t mads_counter;         /* number of mads we sent already */

    MKeyManager *p_mkeymngr;
    KeyManager   key_manager;

    bool allow_undefined_mkey;

    // class version to register
    class_versions_vec_t class_versions_by_class[IBIS_IB_MAX_MAD_CLASSES];

    // Methods to register as replier
    methods_list_t replier_methods_list_by_class[IBIS_IB_MAX_MAD_CLASSES];
    int timeout;
    int retries;

    // Unsolicited MADs handler
    attr_method_pair_to_handler_map_t m_mad_handlers_by_class[IBIS_IB_MAX_MAD_CLASSES];

    vector<uint8_t > PSL;          /* PSL table (dlid->SL mapping) of this node */
    bool usePSL;

    //asynchronouse invocation
    MemoryPool<transaction_data_t> m_transaction_data_pool;
    transactions_map_t transactions_map;
    u_int32_t m_pending_gmps;
    u_int32_t m_pending_smps;
    u_int32_t m_max_gmps_on_wire;
    u_int32_t m_max_smps_on_wire;

    MemoryPool<pending_mad_data_t> m_pending_mads_pool;
    mads_on_node_map_t      m_mads_on_node_map;
    transactions_queue_t m_pending_nodes_transactions;
    bool suppressMadSending;       /* Indicator for Ibis to void sending MADs */

    FILE *pcap_fp;

    ////////////////////
    //methods
    ////////////////////
    void SetLastError(const char *fmt, ...);

    // Returns: 0 - success / 1 - error
    int Bind();
    int Unbind();
    int UnbindPort(umad_port_info_t& port_info);
    void CalculateMethodMaskByClass(u_int8_t mgmt_class, long (&method_mask)[4]);
    int GetLocalPortProperties(OUT port_properties_t *p_port_properties,
                                const umad_port_info_t& port_info);

    /*
     * Add methods to class for IBIS to register as a replier.
     * This method should be called before calling SetPort.
     */
    int AddMethodToClass(uint8_t mgmt_class, u_int8_t methods);

    int RecvMad(u_int8_t mgmt_class, int umad_timeout);
    int RecvPollGMP_SMP(int timeout_ms);
    int SendMad(u_int8_t mgmt_class, int umad_timeout, int umad_retries);
    int RecvAsyncMad(int umad_timeout);

    int AsyncRec(bool &retry, pending_mad_data_t *&next_pending_mad_data);
    void MadRecTimeoutAll();
    void MadRecTimeoutAll(transaction_data_t *p_transaction_data);
    void TimeoutAllPendingMads();

    /*
     * Checks if method is of a request MAD
     * Returns true if the method is of a request, otherwise false
     */
    bool IsRequestMad(uint8_t method);

    /*
     * Returns: mad status[bits: 0-15] / IBIS_MAD_STATUS_SEND_FAILED / IBIS_MAD_STATUS_RECV_FAILED /
     *  IBIS_MAD_STATUS_TIMEOUT / IBIS_MAD_STATUS_GENERAL_ERR / IBIS_MAD_STATUS_SUCCESS
     */
    int DoRPC(u_int8_t mgmt_class);

    /*
     * Returns: mad status[bits: 0-15] / IBIS_MAD_STATUS_SEND_FAILED / IBIS_MAD_STATUS_GENERAL_ERR
     *  IBIS_MAD_STATUS_SUCCESS
     */
    int DoAsyncSend(u_int8_t mgmt_class);

    /*
     * Returns: mad status[bits: 0-15] / IBIS_MAD_STATUS_RECV_FAILED /
     *  IBIS_MAD_STATUS_TIMEOUT / IBIS_MAD_STATUS_GENERAL_ERR / IBIS_MAD_STATUS_SUCCESS
     */
    int DoAsyncRec();

    /*
     * remove the current pending mad data from the list and free it
     * set next_pending_mad_data to the next element in the list.
     *
     * Returns: IBIS_MAD_STATUS_GENERAL_ERR / IBIS_MAD_STATUS_SUCCESS
     */
    int GetNextPendingData(transaction_data_t * p_transaction_data,
                           pending_mad_data_t *&next_pending_mad_data);

    /*
     * Send the current mad
     * May recive mad and send pending mad on the received mad address
     * Returns: IBIS_MAD_STATUS_GENERAL_ERR / IBIS_MAD_STATUS_SUCCESS
     */
    int AsyncSendAndRec(u_int8_t mgmt_class,
                        transaction_data_t *p_transaction_data,
                        pending_mad_data_t *pending_mad_data);

    // Send-only version without internal polling (for aggregator support)
    int AsyncSendOnly(u_int8_t mgmt_class,
                      transaction_data_t *p_transaction_data,
                      pending_mad_data_t *pending_mad_data);

    /*
     * Invoke call back function
     */
    void InvokeCallbackFunction(const clbck_data_t &clbck_data, int rec_status,
                                void *p_attribute_data);

    ////////////////////
    //methods mads
    ////////////////////
    void MADToString(const u_int8_t *buffer, string &mad_in_hex);
    void DumpReceivedMAD();
    void DumpMadData(dump_data_func_t dump_func, void *mad_obj,
                     bool msg_send_mad);
    void PcapDumpMAD(bool msg_send_mad);

    void CommonMadHeaderBuild(struct MAD_Header_Common *mad_header,
            u_int8_t mgmt_class,
            u_int8_t method,
            u_int16_t attribute_id,
            u_int32_t attribute_modifier,
            u_int8_t class_version = 0);

    u_int8_t GetDefaultMgmtClassVersion(u_int8_t mgmt_class);

    ////////////////////
    //smp class methods
    ////////////////////
    void SMPHeaderDirectRoutedBuild(struct MAD_Header_SMP_Direct_Routed *smp_direct_route_header,
            u_int8_t method,
            u_int16_t attribute_id,
            u_int32_t attribute_modifier,
            u_int8_t direct_path_len);

    ////////////////////
    //mellanox methods
    ////////////////////
    static bool IsSupportIB(void *type);
    static bool IsIBDevice(void *arr, unsigned arr_size, u_int16_t dev_id);

    // Pointer to logging function
    static ibis_log_msg_function_t m_log_msg_function;
    static ibis_log_mad_function_t m_log_mad_function;

    void InitClassVersionsDB();
    int RegisterClassVersionToUmad(u_int8_t mgmt_class, umad_port_info_t& port_info);

    inline int GetAgentId(u_int8_t mgmt_class, uint8_t class_version);
    int CheckValidAgentIdForClass(int recv_agent_id, u_int8_t mgmt_class, uint8_t class_version);

    static bool IsSMP(u_int8_t mgmt_class);

    VerbsPort*  m_verbs_port;
    bool        m_verbs_enabled;

    int VerbsOpenPort(const char *dev_name, int port_num);
    void VerbsClosePort();
    int VerbsSendMad();
    int VerbsRecvMad(int timeout_ms);
    int VerbsUmadRecvMad(int timeout_ms);

    uint8_t *VerbsGetSendMad(uint64_t i);
    uint8_t *VerbsGetRecvBuff(uint64_t i);
    uint8_t *VerbsGetRecvMad(uint64_t i);
    int VerbsPostReceive(uint64_t i);
    int VerbsEmptySendQueue();

public:

    ////////////////////
    //methods
    ////////////////////
    Ibis();
    ~Ibis();

    inline const string &GetDevName() { return this->GetSMIDevName(); }
    inline const string &GetSMIDevName() { return this->smp_port.dev_name; }
    inline const string &GetGSIDevName() { return this->gmp_port.dev_name; }
    inline int GetPortNum() { return this->GetSMIPortNum(); }
    inline int GetSMIPortNum() { return this->smp_port.port_num; }
    inline int GetGSIPortNum() { return this->gmp_port.port_num; }
    inline bool IsInit() { return (this->ibis_status != NOT_INITILIAZED); };
    inline bool IsReady() { return (this->ibis_status == READY); };
    inline void SetTimeout(int timeout_value) { this->timeout = timeout_value; };
    inline int GetTimeout() { return this->timeout; };
    inline void SetNumOfRetries(int retries_value) { this->retries = retries_value; };
    inline int GetNumOfRetries() { return this->retries; };
    inline u_int64_t GetNewTID() { return ++this->mads_counter;	}
    //this function is for ibdiagnet's plugins
    inline void SetMADsCounter(u_int64_t mads_cnt) { this->mads_counter = mads_cnt; }
    inline u_int8_t *GetSendMadPktPtr() { return this->p_pkt_send; };
    inline u_int8_t *GetRecvMadPktPtr() { return this->p_pkt_recv; };
    inline KeyManager& GetKeyManager() { return this->key_manager; }

    const char* GetLastError();
    void ClearLastError() {last_error.clear();}
    bool IsFailed() {return !last_error.empty();}

    static string ConvertDirPathToStr(const direct_route_t *p_curr_direct_route);
    static string ConvertMadStatusToStr(u_int16_t status);

    inline static ibis_log_msg_function_t GetLogMsgFunction() {
        return m_log_msg_function;
    }

    inline static void SetLogMsgFunction(ibis_log_msg_function_t log_msg_function) {
        m_log_msg_function = log_msg_function;
    }

    inline static ibis_log_mad_function_t GetLogMadFunction() {
        return m_log_mad_function;
    }

    inline static void SetLogMadFunction(ibis_log_mad_function_t log_mad_function) {
        m_log_mad_function = log_mad_function;
    }

    inline static void GetClockTime(struct timespec &value) {
        clock_gettime(CLOCK_REALTIME, &value);
    }

    // Returns: 0 - success / 1 - error
    int Init();
    int SetPort(const char* device_name, phys_port_t port_num);
    int SetPort(const char* smi_dev_name, phys_port_t smi_port_num,
                const char* gsi_dev_name, phys_port_t gsi_port_num);
    int SetPort(u_int64_t port_guid);   //guid is BE
    int CheckSMPDevicePort(const char *device_name, phys_port_t port_num);
    int CheckGMPDevicePort(const char *device_name, phys_port_t port_num);
    inline int CheckDevicePort(const char *device_name, phys_port_t port_num) {
        return CheckSMPDevicePort(device_name, port_num);
    };
    int CheckCAType(const char *device_name);  // validate that node is an IB node type
    int AutoSelectPortsForDevice(const ext_umad_ca_t *ca);
    int AutoSelectDeviceAndPort();

    int SetSendMadAddr(int d_lid, int d_qp, int sl, int qkey);
    int GetAllLocalPortGUIDs(OUT local_port_t local_ports_array[IBIS_MAX_LOCAL_PORTS],
            OUT u_int32_t *p_local_ports_num);

    // Returns: 0 - success / 1 - error
    int GetLocalSMIPortProperties(OUT port_properties_t *p_port_properties);
    int GetLocalGSIPortProperties(OUT port_properties_t *p_port_properties);

    int SetPSLTable(const vector<uint8_t > &PSLTable);
    uint8_t getPSLForLid(u_int16_t lid);
    void setPSLForLid(u_int16_t lid, u_int16_t maxLid, uint8_t sl);

    void SetMaxMadsOnWire(u_int16_t max_gmps_on_wire, u_int8_t max_smps_on_wire)
    {
        m_max_gmps_on_wire = max_gmps_on_wire;
        m_max_smps_on_wire = max_smps_on_wire;
    }

    inline void SetMKeyManager(MKeyManager *ptr_mkeymngr){p_mkeymngr = ptr_mkeymngr;};
    inline MKeyManager * GetMKeyManager() {return p_mkeymngr;};

    inline void SetAllowUndefinedMkey(bool value) { allow_undefined_mkey = value; };
    inline bool GetAllowUndefinedMkey() const { return allow_undefined_mkey; };

    void SetPcapFilePath(const char *pcap_path);

    void SetVerbsEnabled(bool verbs_enabled);

    ////////////////////
    //methods mads
    ////////////////////

    /*
     * Returns: mad status[bits: 0-15] / IBIS_MAD_STATUS_SEND_FAILED / IBIS_MAD_STATUS_RECV_FAILED /
     *  IBIS_MAD_STATUS_TIMEOUT / IBIS_MAD_STATUS_GENERAL_ERR
     */
    int MadGetSet(ib_address_t *p_ib_address,
        u_int8_t mgmt_class,
        u_int8_t method,
        u_int8_t data_offset,
        const data_func_set_t & class_data,
        const data_func_set_t & attribute_data,
        const clbck_data_t *p_clbck_data);

    /*
     * Returns: mad status[bits: 0-15] / IBIS_MAD_STATUS_SEND_FAILED / IBIS_MAD_STATUS_RECV_FAILED /
     *  IBIS_MAD_STATUS_TIMEOUT / IBIS_MAD_STATUS_GENERAL_ERR
     */
    int MadGetSet(u_int16_t lid,
            u_int32_t d_qp,
            u_int8_t sl,
            u_int32_t qkey,
            u_int8_t mgmt_class,
            u_int8_t method,
            u_int16_t attribute_id,
            u_int32_t attribute_modifier,
            u_int8_t data_offset,
            const data_func_set_t & class_data,
            const data_func_set_t & attribute_data,
            const clbck_data_t *p_clbck_data = NULL);

    void MadCancelAll();
    void MadRecAll();

    //////////////////////
    // IbisAggregator support methods for unified polling
    //////////////////////

    // Get file descriptors for external polling
    std::pair<int, int> GetFds();

    // Check if instance has pending transactions
    bool HasPendingTransactions();

    void MadRecOnce();

    //////////////////////
    // IBIS Server methods
    //////////////////////

    int ReceiveUnsolicitedMad(int timeout_ms);

    // Register MAD handler
    // Returns: 0 - success / 1 - error
    int RegisterMadHandler(u_int8_t mgmt_class, u_int16_t attribute_id,
                           u_int8_t method,
                           u_int8_t data_offset,
                           unpack_data_func_t unpack_class_data_func,
                           dump_data_func_t   dump_class_data_func,
                           unpack_data_func_t unpack_attribute_data_func,
                           dump_data_func_t dump_attribute_data_func,
                           mad_handler_callback_func_t callback_func,
                           void *context);

    ////////////////////
    //smp class methods
    ////////////////////
    /*
     * Returns: mad status[bits: 0-15] / IBIS_MAD_STATUS_SEND_FAILED / IBIS_MAD_STATUS_RECV_FAILED /
     *  IBIS_MAD_STATUS_TIMEOUT / IBIS_MAD_STATUS_GENERAL_ERR
     */
    int SMPMadGetSetByLid(u_int16_t lid,
            u_int8_t method,
            u_int16_t attribute_id,
            u_int32_t attribute_modifier,
            const data_func_set_t & attribute_data,
            const clbck_data_t *p_clbck_data = NULL);
    int SMPMadGetSetByDirect(const direct_route_t *p_direct_route,
            u_int8_t method,
            u_int16_t attribute_id,
            u_int32_t attribute_modifier,
            const data_func_set_t & attribute_data,
            const clbck_data_t *p_clbck_data = NULL);

    /*
     * Returns: mad status[bits: 0-7] / IBIS_MAD_STATUS_SEND_FAILED / IBIS_MAD_STATUS_RECV_FAILED /
     *  IBIS_MAD_STATUS_TIMEOUT / IBIS_MAD_STATUS_GENERAL_ERR
     */
    int SMPPortInfoMadGetByLid(u_int16_t lid,
            phys_port_t port_number,
            struct SMP_PortInfo *p_port_info,
            const clbck_data_t *p_clbck_data = NULL);
    int SMPHierarchyInfoMadGetByLid(u_int16_t lid,
            phys_port_t port_number, uint8_t hierarchy_index,
            struct SMP_HierarchyInfo *p_hierarchy_info,
            const clbck_data_t *p_clbck_data);
    int SMPPortInfoExtMadGetByLid(u_int16_t lid,
                                  phys_port_t port_number,
                                  struct SMP_PortInfoExtended *p_port_info_ext,
                                  const clbck_data_t *p_clbck_data = NULL);
    int SMPMlnxExtPortInfoMadGetByLid(u_int16_t lid,
            phys_port_t port_number,
            struct SMP_MlnxExtPortInfo *p_mlnx_ext_port_info,
            const clbck_data_t *p_clbck_data = NULL);
    int SMPSwitchInfoMadGetByLid(u_int16_t lid,
            struct SMP_SwitchInfo *p_switch_info,
            const clbck_data_t *p_clbck_data = NULL);
    int SMPNodeInfoMadGetByLid(u_int16_t lid,
            struct SMP_NodeInfo *p_node_info,
            const clbck_data_t *p_clbck_data = NULL);
    int SMPNodeDescMadGetByLid(u_int16_t lid,
            struct SMP_NodeDesc *p_node_desc,
            const clbck_data_t *p_clbck_data = NULL);
    int SMPSMInfoMadGetByLid(u_int16_t lid,
            struct SMP_SMInfo *p_sm_info,
            const clbck_data_t *p_clbck_data = NULL);
    int SMPLinearForwardingTableGetByLid(u_int16_t lid,
            u_int32_t lid_to_port_block_num,
            struct SMP_LinearForwardingTable *p_linear_forwarding_table,
            const clbck_data_t *p_clbck_data = NULL);
    int SMPMulticastForwardingTableGetByLid(u_int16_t lid,
            u_int8_t port_group,
            u_int32_t lid_to_port_block_num,
            struct SMP_MulticastForwardingTable *p_multicast_forwarding_table,
            const clbck_data_t *p_clbck_data = NULL);
    int SMPPkeyTableGetByLid(u_int16_t lid,
            u_int16_t port_num,
            u_int16_t block_num,
            struct SMP_PKeyTable *p_pkey_table,
            const clbck_data_t *p_clbck_data = NULL);
    int SMPGUIDInfoTableGetByLid(u_int16_t lid,
            u_int32_t block_num,
            struct SMP_GUIDInfo *p_guid_info,
            const clbck_data_t *p_clbck_data = NULL);
    int SMPSLToVLMappingTableGetByLid(u_int16_t lid,
            phys_port_t out_port_number,
            phys_port_t in_port_number,
            struct SMP_SLToVLMappingTable *p_slvl_mapping,
            const clbck_data_t *p_clbck_data = NULL);
    int SMPPLFTInfoMadGetSetByLid(u_int16_t lid,
            u_int8_t method,
            ib_private_lft_info *p_plft_info,
            const clbck_data_t *p_clbck_data = NULL);
    int SMPPLFTDefMadGetSetByLid(u_int16_t lid,
            u_int8_t method,
            u_int8_t plft_block,
            struct ib_private_lft_def *p_plft_def,
            const clbck_data_t *p_clbck_data = NULL);
    int SMPPLFTMapMadGetSetByLid(u_int16_t lid,
            u_int8_t method,
            u_int8_t pLFTID,
            struct ib_private_lft_map *p_plft_map,
            const clbck_data_t *p_clbck_data = NULL);
    int SMPPortSLToPrivateLFTMapGetSetByLid(u_int16_t lid,
            u_int8_t method,
            u_int8_t port_block,
            struct ib_port_sl_to_private_lft_map *p_plft_map,
            const clbck_data_t *p_clbck_data = NULL);
    int SMPWHBFConfigGetSetByLid(u_int16_t lid,
            u_int8_t method,
            bool global_config,
            struct whbf_config *p_whbf_config,
            const clbck_data_t *p_clbck_data = NULL);
    int SMPARInfoGetSetByLid(u_int16_t lid,
            u_int8_t method,
            bool get_cap,
            struct adaptive_routing_info *p_ar_info,
            const clbck_data_t *p_clbck_data = NULL);
    int SMPHBFConfigGetSetByLid(u_int16_t lid,
            u_int8_t method,
            bool global_config,
            u_int8_t port,
            struct hbf_config *p_hbf_config,
            const clbck_data_t *p_clbck_data = NULL);
    int SMPARGroupTableGetSetByLid(u_int16_t lid,
            u_int8_t  method,
            u_int16_t group_block,
            u_int8_t  group_table,
            u_int8_t  pLFTID,
            struct ib_ar_grp_table *p_ar_group_table,
            const clbck_data_t *p_clbck_data = NULL);
    int SMPARLinearForwardingTableGetSetByLid(u_int16_t lid,
            u_int8_t method,
            u_int32_t lid_block,
            u_int8_t pLFTID,
            struct ib_ar_linear_forwarding_table_sx *p_ar_linear_forwarding_table,
            const clbck_data_t *p_clbck_data = NULL);
    int SMPARLinearForwardingTableNoSXGetSetByLid(u_int16_t lid,
            u_int8_t method,
            u_int32_t lid_block,
            u_int8_t pLFTID,
            struct ib_ar_linear_forwarding_table *p_ar_linear_forwarding_table,
            const clbck_data_t *p_clbck_data = NULL);

    int SMPExtendedNodeInfoMadGetSetByLid(u_int16_t lid,
            u_int8_t method,
            struct ib_extended_node_info *p_ext_node_info,
            const clbck_data_t *p_clbck_data);

    int SMPRNSubGroupDirectionTableGetSetByLid(u_int16_t lid,
            u_int8_t method,
            u_int16_t block_num,
            struct rn_sub_group_direction_tbl *p_sub_group_direction_table,
            const clbck_data_t *p_clbck_data);
    int SMPRNGenStringTableGetSetByLid(u_int16_t lid,
            u_int8_t method,
            u_int8_t direction_block,
            u_int8_t pLFTID,
            struct rn_gen_string_tbl *p_gen_string_table,
            const clbck_data_t *p_clbck_data);
    int SMPRNGenBySubGroupPriorityMadGetSetByLid(u_int16_t lid,
            u_int8_t method,
            struct rn_gen_by_sub_group_prio *p_gen_by_sg_priority,
            const clbck_data_t *p_clbck_data);
    int SMPRNRcvStringGetSetByLid(u_int16_t lid,
            u_int8_t method,
            u_int8_t string_block,
            struct rn_rcv_string *p_rcv_string,
            const clbck_data_t *p_clbck_data);
    int SMPRNXmitPortMaskGetSetByLid(u_int16_t lid,
            u_int8_t method,
            u_int8_t ports_block,
            struct rn_xmit_port_mask *p_xmit_port_mask,
            const clbck_data_t *p_clbck_data);
    int SMPARGroupTableCopySetByLid(u_int16_t lid,
            u_int16_t group_to_copy,
            bool copy_direction,
            struct adaptive_routing_group_table_copy *p_group_table_copy,
            const clbck_data_t *p_clbck_data);
    int SMPSwitchPortStateTableMadGetByDirect(const direct_route_t *p_direct_route,
            u_int8_t block_number,
            struct SMP_SwitchPortStateTable *p_switch_port_state_table,
            const clbck_data_t *p_clbck_data = NULL);
    int SMPPortInfoMadGetByDirect(const direct_route_t *p_direct_route,
            phys_port_t port_number,
            struct SMP_PortInfo *p_port_info,
            const clbck_data_t *p_clbck_data = NULL);

    int SMP_VLArbitrationMadGetByDirect(const direct_route_t *p_direct_route,
            u_int32_t attribute_modifier,
            struct SMP_VLArbitrationTable *p_vl_arbitration,
            const clbck_data_t *p_clbck_data = NULL);

    int SMPHierarchyInfoMadGetByDirect(const direct_route_t *p_direct_route,
            phys_port_t port_number, uint8_t hierarchy_index,
            struct SMP_HierarchyInfo *p_hierarchy_info,
            const clbck_data_t *p_clbck_data);
    int SMPPortInfoExtMadGetByDirect(const direct_route_t *p_direct_route,
            phys_port_t port_number,
            struct SMP_PortInfoExtended *p_port_info_ext,
            const clbck_data_t *p_clbck_data = NULL);
    int SMPMlnxExtPortInfoMadGetByDirect(const direct_route_t *p_direct_route,
            phys_port_t port_number,
            struct SMP_MlnxExtPortInfo *p_mlnx_ext_port_info,
            const clbck_data_t *p_clbck_data = NULL);
    int SMPVSGeneralInfoFwInfoMadGetByDirect(const direct_route_t *p_direct_route,
                                             struct FWInfo_Block_Element *p_general_info,
                                             const clbck_data_t *p_clbck_data = NULL);
    int SMPVSGeneralInfoCapabilityMaskMadGetByDirect(const direct_route_t *p_direct_route,
                                                     struct GeneralInfoCapabilityMask *p_general_info,
                                                     const clbck_data_t *p_clbck_data = NULL);
    int SMPSwitchInfoMadGetByDirect(const direct_route_t *p_direct_route,
            struct SMP_SwitchInfo *p_switch_info,
            const clbck_data_t *p_clbck_data = NULL);
    int SMPNodeInfoMadGetByDirect(const direct_route_t *p_direct_route,
            struct SMP_NodeInfo *p_node_info,
            const clbck_data_t *p_clbck_data = NULL);
    int SMPNodeDescMadGetByDirect(const direct_route_t *p_direct_route,
            struct SMP_NodeDesc *p_node_desc,
            const clbck_data_t *p_clbck_data = NULL);
    int SMPSMInfoMadGetByDirect(const direct_route_t *p_direct_route,
            struct SMP_SMInfo *p_sm_info,
            const clbck_data_t *p_clbck_data = NULL);
    int SMPLinearForwardingTableGetByDirect(const direct_route_t *p_direct_route,
            u_int32_t lid_to_port_block_num,
            struct SMP_LinearForwardingTable *p_linear_forwarding_table,
            const clbck_data_t *p_clbck_data = NULL);
    int SMPMulticastForwardingTableGetByDirect(const direct_route_t *p_direct_route,
            u_int8_t port_group,
            u_int32_t lid_to_port_block_num,
            struct SMP_MulticastForwardingTable *p_multicast_forwarding_table,
            const clbck_data_t *p_clbck_data = NULL);
    int SMPPKeyTableGetByDirect(const direct_route_t *p_direct_route,
            u_int16_t port_num,
            u_int16_t block_num,
            struct SMP_PKeyTable *p_pkey_table,
            const clbck_data_t *p_clbck_data = NULL);
    int SMPGUIDInfoTableGetByDirect(const direct_route_t *p_direct_route,
            u_int32_t block_num,
            struct SMP_GUIDInfo *p_guid_info,
            const clbck_data_t *p_clbck_data = NULL);
    int SMPSLToVLMappingTableGetByDirect(const direct_route_t *p_direct_route,
            phys_port_t out_port_number,
            phys_port_t in_port_number,
            struct SMP_SLToVLMappingTable *p_slvl_mapping,
            const clbck_data_t *p_clbck_data = NULL);
    int SMPPLFTInfoMadGetSetByDirect(const direct_route_t *p_direct_route,
            u_int8_t method,
            ib_private_lft_info *p_plft_info,
            const clbck_data_t *p_clbck_data = NULL);
    int SMPPLFTDefMadGetSetByDirect(const direct_route_t *p_direct_route,
            u_int8_t method,
            u_int8_t plft_block,
            struct ib_private_lft_def *p_plft_def,
            const clbck_data_t *p_clbck_data = NULL);
    int SMPPLFTMapMadGetSetByDirect(const direct_route_t *p_direct_route,
            u_int8_t method,
            u_int8_t pLFTID,
            struct ib_private_lft_map *p_plft_map,
            const clbck_data_t *p_clbck_data = NULL);
    int SMPPortSLToPrivateLFTMapGetSetByDirect(const direct_route_t *p_direct_route,
            u_int8_t method,
            u_int8_t port_block,
            struct ib_port_sl_to_private_lft_map *p_plft_map,
            const clbck_data_t *p_clbck_data = NULL);
    int SMPWHBFConfigGetSetByDirect(const direct_route_t *p_direct_route,
            u_int8_t method,
            bool global_config,
            u_int8_t block_index,
            struct whbf_config *p_whbf_config,
            const clbck_data_t *p_clbck_data = NULL);
    int SMPHBFConfigGetSetByDirect(const direct_route_t *p_direct_route,
            u_int8_t method,
            bool global_config,
            u_int8_t port,
            struct hbf_config *p_hbf_config,
            const clbck_data_t *p_clbck_data = NULL);

    int SMP_pFRNConfigGetSetByDirect(const direct_route_t *p_direct_route,
            u_int8_t method,
            struct SMP_pFRNConfig *p_pfrn,
            const clbck_data_t *p_clbck_data = NULL);

    int SMPARInfoGetSetByDirect(const direct_route_t *p_direct_route,
            u_int8_t method,
            bool get_cap,
            struct adaptive_routing_info *p_ar_info,
            const clbck_data_t *p_clbck_data = NULL);
    int SMPARGroupTableGetSetByDirect(const direct_route_t *p_direct_route,
            u_int8_t  method,
            u_int16_t group_block,
            u_int8_t  group_table,
            u_int8_t  pLFTID,
            struct ib_ar_grp_table *p_ar_group_table,
            const clbck_data_t *p_clbck_data = NULL);
    int SMPARLinearForwardingTableGetSetByDirect(const direct_route_t *p_direct_route,
            u_int8_t method,
            u_int32_t lid_block,
            u_int8_t pLFTID,
            struct ib_ar_linear_forwarding_table_sx *p_ar_linear_forwarding_table,
            const clbck_data_t *p_clbck_data = NULL);
    int SMPARLinearForwardingTableNoSXGetSetByDirect(const direct_route_t *p_direct_route,
            u_int8_t method,
            u_int32_t lid_block,
            u_int8_t pLFTID,
            struct ib_ar_linear_forwarding_table *p_ar_linear_forwarding_table,
            const clbck_data_t *p_clbck_data = NULL);

    int SMPExtendedNodeInfoMadGetSetByDirect(const direct_route_t *p_direct_route,
            u_int8_t method,
            struct ib_extended_node_info *p_ext_node_info,
            const clbck_data_t *p_clbck_data);

    int SMPExtendedSwitchInfoMadGetSetByDirect(const direct_route_t *p_direct_route,
            u_int8_t method,
            struct SMP_ExtendedSwitchInfo *p_ext_switch_info,
            const clbck_data_t *p_clbck_data);

    int SMPNVLinkFecModeMadGetSetByDirect(const direct_route_t *p_direct_route,
            u_int32_t port,
            u_int8_t method,
            struct SMP_NVLinkFecMode *p_data,
            const clbck_data_t *p_clbck_data);

    int SMPChassisInfoMadGetByDirect(const direct_route_t *p_direct_route,
            struct SMP_ChassisInfo* p_chassis_info,
            const clbck_data_t *p_clbck_data);

    int SMPRNSubGroupDirectionTableGetSetByDirect(const direct_route_t *p_direct_route,
            u_int8_t method,
            u_int16_t block_num,
            struct rn_sub_group_direction_tbl *p_sub_group_direction_table,
            const clbck_data_t *p_clbck_data);
    int SMPRNGenStringTableGetSetByDirect(const direct_route_t *p_direct_route,
            u_int8_t method,
            u_int8_t direction_block,
            u_int8_t pLFTID,
            struct rn_gen_string_tbl *p_gen_string_table,
            const clbck_data_t *p_clbck_data);
    int SMPRNGenBySubGroupPriorityMadGetSetByDirect(
            const direct_route_t *p_direct_route,
            u_int8_t method,
            struct rn_gen_by_sub_group_prio *p_gen_by_sg_priority,
            const clbck_data_t *p_clbck_data);
    int SMPRNRcvStringGetSetByDirect(const direct_route_t *p_direct_route,
            u_int8_t method,
            u_int16_t string_block,
            struct rn_rcv_string *p_rcv_string,
            const clbck_data_t *p_clbck_data);
    int SMPRNXmitPortMaskGetSetByDirect(const direct_route_t *p_direct_route,
            u_int8_t method,
            u_int8_t ports_block,
            struct rn_xmit_port_mask *p_xmit_port_mask,
            const clbck_data_t *p_clbck_data);
    int SMPARGroupTableCopySetByDirect(const direct_route_t *p_direct_route,
            u_int16_t group_to_copy,
            bool copy_direction,
            struct adaptive_routing_group_table_copy *p_group_table_copy,
            const clbck_data_t *p_clbck_data);
    int SMPVPortGUIDInfoMadGetByDirect(const direct_route_t *p_direct_route,
            virtual_port_t vport_num,
            u_int16_t block_num,
            struct SMP_VPortGUIDInfo *p_vport_guid_info,
            const clbck_data_t *p_clbck_data);
    int SMPVNodeInfoMadGetByDirect(const direct_route_t *p_direct_route,
            virtual_port_t vport_num,
            struct SMP_VNodeInfo *p_vnode_info,
            const clbck_data_t *p_clbck_data);
    int SMPVirtualizationInfoMadGetByDirect(const direct_route_t *p_direct_route,
            struct SMP_VirtualizationInfo *p_virtual_info,
            const clbck_data_t *p_clbck_data);
    int SMPVNodeDescriptionMadGetByDirect(const direct_route_t *p_direct_route,
            virtual_port_t vport_num,
            struct SMP_NodeDesc *p_vnode_description,
            const clbck_data_t *p_clbck_data);

    int SMPVirtualizationInfoMadGetByLid(u_int16_t lid,
            struct SMP_VirtualizationInfo *p_virtual_info,
            const clbck_data_t *p_clbck_data);
    int SMPVPortInfoMadGetByLid(u_int16_t lid,
            virtual_port_t vport_num,
            struct SMP_VPortInfo *p_vport_info,
            const clbck_data_t *p_clbck_data);
    int SMPVPortStateMadGetByLid(u_int16_t lid,
            u_int16_t block_num,
            struct SMP_VPortState *p_vport_state,
            const clbck_data_t *p_clbck_data);
    int SMPVPortGUIDInfoMadGetByLid(u_int16_t lid,
            virtual_port_t vport_num,
            u_int16_t block_num,
            struct SMP_VPortGUIDInfo *p_vport_guid_info,
            const clbck_data_t *p_clbck_data);
    int SMPVNodeInfoMadGetByLid(u_int16_t lid,
            virtual_port_t vport_num,
            struct SMP_VNodeInfo *p_vnode_info,
            const clbck_data_t *p_clbck_data);
    int SMPVNodeDescriptionMadGetByLid(u_int16_t lid,
            virtual_port_t vport_num,
            struct SMP_NodeDesc *p_vnode_description,
            const clbck_data_t *p_clbck_data);
    int SMPVPortPKeyTblMadGetByLid(u_int16_t lid,
            virtual_port_t vport_num,
            u_int16_t block_num,
            struct SMP_PKeyTable *p_vport_pkey_tbl,
            const clbck_data_t *p_clbck_data);
    int SMPVPortStateMadGetByDirect(const direct_route_t *p_direct_route,
            u_int16_t block_num,
            struct SMP_VPortState *p_vport_state,
            const clbck_data_t *p_clbck_data);
    int SMPVPortPKeyTblMadGetByDirect(const direct_route_t *p_direct_route,
            virtual_port_t vport_num,
            u_int16_t block_num,
            struct SMP_PKeyTable *p_vport_pkey_tbl,
            const clbck_data_t *p_clbck_data);
    int SMPTempSensingDataGetByDirect(const direct_route_t *p_direct_route,
            struct SMP_TempSensing *p_tempsens,
            const clbck_data_t *p_clbck_data);
    int SMPQosConfigSLGetByDirect(const direct_route_t *p_direct_route,
            struct SMP_QosConfigSL *p_qos_config_sl,
            const clbck_data_t *p_clbck_data,
            phys_port_t port_num);
    int SMPVPortQoSConfigSLGetByDirect(const direct_route_t *p_direct_route,
            struct SMP_QosConfigSL *p_qos_config_sl,
            const clbck_data_t *p_clbck_data,
            virtual_port_t vport_num);
    int SMPQosConfigVLGetByDirect(const direct_route_t *p_direct_route,
            struct SMP_QosConfigVL *p_qos_config_vl,
            const clbck_data_t *p_clbck_data,
            phys_port_t port_num, bool profile_number_enable);
    int SMPRouterInfoGetByDirect(const direct_route_t *p_direct_route,
            struct SMP_RouterInfo *p_router_info,
            const clbck_data_t *p_clbck_data);
    int SMPAdjRouterTableGetByDirect(const direct_route_t *p_direct_route,
            u_int8_t block_num,
            struct SMP_AdjSiteLocalSubnTbl *p_router_tbl,
            const clbck_data_t *p_clbck_data);
    int SMPNextHopRouterTableGetByDirect(const direct_route_t *p_direct_route,
            u_int32_t block_num,
            struct SMP_NextHopTbl *p_router_tbl,
            const clbck_data_t *p_clbck_data);
     int SMPAdjSubnetRouterLIDInfoTableGetByDirect(const direct_route_t *p_direct_route,
            u_int8_t block_num,
            struct SMP_AdjSubnetsRouterLIDInfoTable *p_router_lid_tbl,
            const clbck_data_t *p_clbck_data);
    int SMPRouterLIDTableGetByDirect(const direct_route_t *p_direct_route,
            u_int8_t block_num,
            struct SMP_RouterLIDTable *p_router_lid_tbl,
            const clbck_data_t *p_clbck_data);
    int SMPARGroupToRouterLIDTableGetByDirect(const direct_route_t *p_direct_route,
            u_int8_t block_num,
            struct SMP_ARGroupToRouterLIDTable *p_data,
            const clbck_data_t *p_clbck_data);
    int SMPVPortInfoMadGetByDirect(const direct_route_t *p_direct_route,
            virtual_port_t vport_num,
            struct SMP_VPortInfo *p_vport_info,
            const clbck_data_t *p_clbck_data);

    int SMPEntryPlaneFilterConfigMadGetByDirect(const direct_route_t *p_direct_route,
            u_int16_t in_port,
            u_int8_t plane,
            u_int8_t egress_block,
            struct SMP_EntryPlaneFilterConfig *p_entry_plane_filter,
            const clbck_data_t *p_clbck_data);

    int SMPEntryPlaneFilterConfigMadSetByDirect(const direct_route_t *p_direct_route,
            u_int16_t in_port,
            u_int8_t plane,
            u_int8_t ingress_block,
            u_int8_t egress_block,
            struct SMP_EntryPlaneFilterConfig *p_entry_plane_filter,
            const clbck_data_t *p_clbck_data);

    int SMPEndPortPlaneFilterConfigMadGetByDirect(const direct_route_t *p_direct_route,
            struct SMP_EndPortPlaneFilterConfig *p_end_port_plane_filter,
            const clbck_data_t *p_clbck_data);

    int SMPEndPortPlaneFilterConfigMadSetByDirect(const direct_route_t *p_direct_route,
            struct SMP_EndPortPlaneFilterConfig *p_end_port_plane_filter,
            const clbck_data_t *p_clbck_data);

    int SMPAnycastLIDInfoGetByDirect(const direct_route_t *p_direct_route, u_int32_t block,
            struct SMP_AnycastLIDInfo *p_data,
            const clbck_data_t *p_clbck_data);

    int SMPRailFilterConfigGetByDirect(const direct_route_t *p_direct_route, u_int32_t port,
            u_int32_t ingress_block, u_int32_t egress_block,
            struct SMP_RailFilterConfig *p_data,
            const clbck_data_t *p_clbck_data);

    int SMPLinearForwardingTableSplitGetByDirect(const direct_route_t *p_direct_route,
            struct SMP_LinearForwardingTableSplit *p_data,
            const clbck_data_t *p_clbck_data);

    int SMPContainAndDrainInfoGetByDirect(const direct_route_t *p_direct_route,
            u_int8_t block,
            struct SMP_ContainAndDrainInfo *p_data,
            const clbck_data_t *p_clbck_data);

    int SMPContainAndDrainPortStateGetByDirect(const direct_route_t *p_direct_route,
            u_int8_t block,
            struct SMP_ContainAndDrainPortState *p_data,
            const clbck_data_t *p_clbck_data);

    int SMPNVLHBFConfigGetByDirect(const direct_route_t *p_direct_route,
            u_int32_t port, u_int32_t profile_number_en,
            struct SMP_NVLHBFConfig *p_data,
            const clbck_data_t *p_clbck_data);

    int SMPPortRecoveryPolicyConfigGetByDirect(const direct_route_t *p_direct_route,
            phys_port_t port,
            u_int8_t port_recovery_policy_profile,
            struct SMP_PortRecoveryPolicyConfig *p_data,
            const clbck_data_t *p_clbck_data);

    // ////////////////////
    // Multicst Private LFT methods
    ////////////////////
    int SMPMulticastPrivateLFTInfoGetByDirect(const direct_route_t *p_direct_route,
            struct SMP_MulticastPrivateLFTInfo *p_data,
            const clbck_data_t *p_clbck_data);

    int SMPMulticastPrivateLFTMapGetByDirect(const direct_route_t *p_direct_route,
            u_int8_t position, u_int8_t mplft_id,
            struct SMP_MulticastPrivateLFTMap *p_data,
            const clbck_data_t *p_clbck_data);

    int SMPMulticastPrivateLFTDefByDirect(const direct_route_t *p_direct_route,
            u_int8_t block_id,
            struct SMP_MulticastPrivateLFTDef *p_data,
            const clbck_data_t *p_clbck_data);

    int SMPPortSLToMulticastPrivateLFTMapGetByDirect(const direct_route_t *p_direct_route,
            u_int16_t block_index,
            struct SMP_PortSLToMulticastPrivateLFTMap *p_data,
            const clbck_data_t *p_clbck_data);


    ////////////////////
    // fast recovery methods
    ////////////////////
    int SMPCreditWatchdogConfigGetByDirect(const direct_route_t *p_direct_route,
            u_int32_t fast_recovery_profile_idx,
            struct SMP_CreditWatchdogConfig *p_credit_wd_config,
            const clbck_data_t *p_clbck_data);

    int SMPCreditWatchdogConfigSetByDirect(const direct_route_t *p_direct_route,
            u_int32_t fast_recovery_profile_idx,
            struct SMP_CreditWatchdogConfig *p_credit_wd_config,
            const clbck_data_t *p_clbck_data);

    int SMPBERConfigGetByDirect(const direct_route_t *p_direct_route,
            u_int32_t fast_recovery_profile_idx,
            u_int32_t ber_type,
            u_int32_t default_thr,
            struct SMP_BERConfig *p_ber_config,
            const clbck_data_t *p_clbck_data);

    int SMPBERConfigSetByDirect(const direct_route_t *p_direct_route,
            u_int32_t fast_recovery_profile_idx,
            u_int32_t ber_type,
            u_int32_t default_thr,
            struct SMP_BERConfig *p_ber_config,
            const clbck_data_t *p_clbck_data);

    int SMPProfilesConfigGetByDirect(const direct_route_t *p_direct_route,
            u_int32_t block_num,
            u_int32_t feature,
            struct SMP_ProfilesConfig *p_profiles_config,
            const clbck_data_t *p_clbck_data);

    int SMPProfilesConfigSetByDirect(const direct_route_t *p_direct_route,
            u_int32_t block_num,
            u_int32_t feature,
            struct SMP_ProfilesConfig *p_profiles_config,
            const clbck_data_t *p_clbck_data);

    int VSCreditWatchdogTimeoutCountersGet(u_int16_t lid,
            phys_port_t port_number,
            struct VS_CreditWatchdogTimeoutCounters *p_credit_wd_counters,
            const clbck_data_t *p_clbck_data = NULL);

    int VSCreditWatchdogTimeoutCountersClear(u_int16_t lid,
            phys_port_t port_number,
            struct VS_CreditWatchdogTimeoutCounters *p_credit_wd_counters,
            const clbck_data_t *p_clbck_data = NULL);

    int VSFastRecoveryCountersGet(u_int16_t lid,
            phys_port_t port_number,
            u_int8_t trigger,
            struct VS_FastRecoveryCounters *p_fast_recovery_counters,
            const clbck_data_t *p_clbck_data = NULL);

    int VSFastRecoveryCountersClear(u_int16_t lid,
            phys_port_t port_number,
            u_int8_t trigger,
            struct VS_FastRecoveryCounters *p_fast_recovery_counters,
            const clbck_data_t *p_clbck_data = NULL);

    int VSPortGeneralCountersGet(u_int16_t lid, 
            phys_port_t port_number,
            struct VS_PortGeneralCounters* p_port_general_counters,
            const clbck_data_t *p_clbck_data = NULL);

    int VSPortGeneralCountersClear(u_int16_t lid, 
            phys_port_t port_number,
            struct VS_PortGeneralCounters* p_port_general_counters,
            const clbck_data_t *p_clbck_data = NULL);

    int VSPortRecoveryPolicyCountersGet(u_int16_t lid,
            phys_port_t port_number,
            struct VS_PortRecoveryPolicyCounters* p_port_recovery_policy_counters,
            const clbck_data_t *p_clbck_data = NULL);

    int VSPortRecoveryPolicyCountersClear(u_int16_t lid,
            phys_port_t port_number,
            struct VS_PortRecoveryPolicyCounters* p_port_recovery_policy_counters,
            const clbck_data_t *p_clbck_data = NULL);

    int VSPerformanceHistogramInfoGet(u_int16_t lid,
            struct VS_PerformanceHistogramInfo *p_data,
            const clbck_data_t *p_clbck_data);

    int VSPerformanceHistogramBufferControlGet(u_int16_t lid,
            phys_port_t port_number, uint8_t vl, u_int8_t dir, 
            bool port_global, bool vl_global,
            struct VS_PerformanceHistogramBufferControl *p_data,
            const clbck_data_t *p_clbck_data);

    int VSPerformanceHistogramBufferControlSet(u_int16_t lid,
            phys_port_t port_number, uint8_t vl, u_int8_t dir,
            bool port_global, bool vl_global,
            struct VS_PerformanceHistogramBufferControl *p_data,
            const clbck_data_t *p_clbck_data);

    int VSPerformanceHistogramBufferDataGet(u_int16_t lid,
            phys_port_t port_number, uint8_t vl, u_int8_t dir, bool clr,
            struct VS_PerformanceHistogramBufferData *p_data,
            const clbck_data_t *p_clbck_data);

   int VSPerformanceHistogramPortsControlGet(u_int16_t lid,
            phys_port_t port_number, u_int8_t hist_id,
            struct VS_PerformanceHistogramPortsControl *p_data,
            const clbck_data_t *p_clbck_data);

   int VSPerformanceHistogramPortsDataGet(u_int16_t lid,
            phys_port_t port_number, u_int8_t hist_id, bool clr,
            struct VS_PerformanceHistogramPortsData *p_data,
            const clbck_data_t *p_clbck_data);

    ////////////////////
    //pm class methods
    ////////////////////
    /*
     * Returns: mad status[bits: 0-15] / IBIS_MAD_STATUS_SEND_FAILED / IBIS_MAD_STATUS_RECV_FAILED /
     *  IBIS_MAD_STATUS_TIMEOUT / IBIS_MAD_STATUS_GENERAL_ERR
     */
    int PMMadGetSet(u_int16_t lid,
            u_int8_t method,
            u_int16_t attribute_id,
            u_int32_t attribute_modifier,
            const data_func_set_t & attribute_data,
            const clbck_data_t *p_clbck_data = NULL);

    /*
     * Returns: mad status[bits: 0-7] / IBIS_MAD_STATUS_SEND_FAILED / IBIS_MAD_STATUS_RECV_FAILED /
     *  IBIS_MAD_STATUS_TIMEOUT / IBIS_MAD_STATUS_GENERAL_ERR
     */
    int PMClassPortInfoGet(u_int16_t lid,
            struct IB_ClassPortInfo *p_ib_class_port_info,
            const clbck_data_t *p_clbck_data = NULL);
    int PMPortSampleControlGet(u_int16_t lid,
        phys_port_t port_number,
        struct PM_PortSamplesControl *p_sample_control,
        const clbck_data_t *p_clbck_data);
    int PMPortCountersGet(u_int16_t lid,
            phys_port_t port_number,
            struct PM_PortCounters *p_port_counters,
            const clbck_data_t *p_clbck_data = NULL);
    int PMPortCountersSet(u_int16_t lid,
            struct PM_PortCounters *p_port_counters,
            const clbck_data_t *p_clbck_data = NULL);
    int PMPortCountersClear(u_int16_t lid,
            phys_port_t port_number,
            const clbck_data_t *p_clbck_data = NULL);
    int PMPortCountersExtendedGet(u_int16_t lid,
            phys_port_t port_number,
            struct PM_PortCountersExtended *p_port_counters,
            const clbck_data_t *p_clbck_data = NULL);
    int PMPortCountersExtendedSet(u_int16_t lid,
            struct PM_PortCountersExtended *p_port_counters,
            const clbck_data_t *p_clbck_data = NULL);
    int PMPortCountersExtendedClear(u_int16_t lid,
            phys_port_t port_number,
            const clbck_data_t *p_clbck_data = NULL);
    int PMPortExtendedSpeedsCountersGet(u_int16_t lid,
            phys_port_t port_number,
            struct PM_PortExtendedSpeedsCounters *p_port_counters,
            const clbck_data_t *p_clbck_data = NULL);
    //undefined behavior
    //int PMPortExtendedSpeedsCountersSet(u_int16_t lid,
    //    struct PM_PortExtendedSpeedsCounters *p_port_counters);
    int PMPortExtendedSpeedsCountersClear(u_int16_t lid,
            phys_port_t port_number,
            const clbck_data_t *p_clbck_data = NULL);
    int PMPortExtendedSpeedsRSFECCountersGet(u_int16_t lid,
            phys_port_t port_number,
            struct PM_PortExtendedSpeedsRSFECCounters *p_port_counters,
            const clbck_data_t *p_clbck_data = NULL);
    int PMPortExtendedSpeedsRSFECCountersClear(u_int16_t lid,
            phys_port_t port_number,
            const clbck_data_t *p_clbck_data = NULL);
   /*
    * Returns: mad status[bits: 0-7] / IBIS_MAD_STATUS_SEND_FAILED / IBIS_MAD_STATUS_RECV_FAILED /
    *  IBIS_MAD_STATUS_TIMEOUT / IBIS_MAD_STATUS_GENERAL_ERR
    */
    int PMPerSLVLCounters(bool is_reset_cntr,
            u_int16_t lid,
            phys_port_t port_number,
            u_int32_t attr_id,
            struct PM_PortRcvXmitCntrsSlVl *p_pm_port_rcvxmit_data_slvl,
            const clbck_data_t *p_clbck_data);

    // PortVLXmitFlowCtlUpdateErrors
    int PMPortVLXmitFlowCtlUpdateErrorsGet(u_int16_t lid,
                                           phys_port_t port_number,
                                           struct PM_PortVLXmitFlowCtlUpdateErrors *p_port_vl_xmit_flow_ctl,
                                           const clbck_data_t *p_clbck_data = NULL);

    int PMPortVLXmitFlowCtlUpdateErrorsClear(u_int16_t lid,
                                             phys_port_t port_number,
                                             const clbck_data_t *p_clbck_data = NULL);

    //PortRcvErrorDetails and PortXmitDiscardDetails
    int PMPortRcvErrorDetailsGet(u_int16_t lid,
                                 phys_port_t port_number,
                                 struct PM_PortRcvErrorDetails *p_pm_port_rcv_error_details,
                                 const clbck_data_t *p_clbck_data = NULL);

    int PMPortRcvErrorDetailsClear(u_int16_t lid,
                                   phys_port_t port_number,
                                   const clbck_data_t *p_clbck_data = NULL);

    int PMPortXmitDiscardDetailsGet(u_int16_t lid,
                                    phys_port_t port_number,
                                    struct PM_PortXmitDiscardDetails *p_pm_port_xmit_discard_details,
                                    const clbck_data_t *p_clbck_data = NULL);

    int PMPortXmitDiscardDetailsClear(u_int16_t lid,
                                      phys_port_t port_number,
                                      const clbck_data_t *p_clbck_data = NULL);
    int PMPortSamplesResultGet(u_int16_t lid,
                               phys_port_t port_number,
                               struct PM_PortSamplesResult *p_pm_port_samples_result,
                               const clbck_data_t *p_clbck_data = NULL);

    ////////////////////
    //vs class methods
    ////////////////////
    /*
     * Returns: mad status[bits: 0-15] / IBIS_MAD_STATUS_SEND_FAILED / IBIS_MAD_STATUS_RECV_FAILED /
     *  IBIS_MAD_STATUS_TIMEOUT / IBIS_MAD_STATUS_GENERAL_ERR
     */
    int VSMadGetSet(u_int16_t lid,
            u_int8_t method,
            u_int16_t attribute_id,
            u_int32_t attribute_modifier,
            const data_func_set_t & attribute_data,
            const clbck_data_t *p_clbck_data = NULL);

    /*
     * Returns: mad status[bits: 0-7] / IBIS_MAD_STATUS_SEND_FAILED / IBIS_MAD_STATUS_RECV_FAILED /
     *  IBIS_MAD_STATUS_TIMEOUT / IBIS_MAD_STATUS_GENERAL_ERR
     */
    int VSGeneralInfoGet(u_int16_t lid, struct VendorSpec_GeneralInfo *p_general_info,
                         const clbck_data_t *p_clbck_data = NULL);

    /*
     * Returns: mad status[bits: 0-7] / IBIS_MAD_STATUS_SEND_FAILED / IBIS_MAD_STATUS_RECV_FAILED /
     *  IBIS_MAD_STATUS_TIMEOUT / IBIS_MAD_STATUS_GENERAL_ERR
     */
    int VSSwitchNetworkInfoGet(u_int16_t lid, struct  VS_SwitchNetworkInfo *p_switch_network_info,
                               const clbck_data_t *p_clbck_data = NULL);

    /*
     * Returns: mad status[bits: 0-7] / IBIS_MAD_STATUS_SEND_FAILED / IBIS_MAD_STATUS_RECV_FAILED /
     *  IBIS_MAD_STATUS_TIMEOUT / IBIS_MAD_STATUS_GENERAL_ERR
     */
    int VSPortLLRStatisticsGet(u_int16_t lid,
            phys_port_t port_number,
            struct VendorSpec_PortLLRStatistics *p_port_llr_statistics,
            bool get_symbol_errors,
            const clbck_data_t *p_clbck_data = NULL);

    /*
     * Returns: mad status[bits: 0-7] / IBIS_MAD_STATUS_SEND_FAILED / IBIS_MAD_STATUS_RECV_FAILED /
     *  IBIS_MAD_STATUS_TIMEOUT / IBIS_MAD_STATUS_GENERAL_ERR
     */
    int VSPortLLRStatisticsClear(u_int16_t lid,
        phys_port_t port_number,
        bool clear_symbol_errors,
        const clbck_data_t *p_clbck_data = NULL);

    /*
     * Returns: mad status[bits: 0-7] / IBIS_MAD_STATUS_SEND_FAILED / IBIS_MAD_STATUS_RECV_FAILED /
     *  IBIS_MAD_STATUS_TIMEOUT / IBIS_MAD_STATUS_GENERAL_ERR
     */
    int VSPortLLRStatisticsSet(u_int16_t lid,
        phys_port_t port_number,
        struct VendorSpec_PortLLRStatistics *p_port_llr_statistics,
        bool set_symbol_errors,
        const clbck_data_t *p_clbck_data = NULL);

    int VSDiagnosticDataGet(u_int16_t lid,
        phys_port_t port_number,
        u_int8_t page_number,
        struct VS_DiagnosticData *p_dc,
        const clbck_data_t *p_clbck_data);

    int VSDiagnosticDataGet_AM(u_int16_t lid,
        u_int32_t attribute_modifier,
        struct VS_DiagnosticData *p_dc,
        const clbck_data_t *p_clbck_data);

    int VSDiagnosticDataPageClear(u_int16_t lid,
        phys_port_t port_number,
        u_int8_t page_number,
        struct VS_DiagnosticData *p_dc,
        const clbck_data_t *p_clbck_data);

    int VSDiagnosticDataPageClear_AM(u_int16_t lid,
        u_int32_t attribute_modifier,
        struct VS_DiagnosticData *p_dc,
        const clbck_data_t *p_clbck_data);

    int VSPortRNCountersClear(u_int16_t lid,
            phys_port_t port_number,
            const clbck_data_t *p_clbck_data);

    int VSPortRNCountersGet(u_int16_t lid,
            phys_port_t port_number,
            struct port_rn_counters *p_rn_counters,
            const clbck_data_t *p_clbck_data);

    int VSPortRoutingDecisionCountersClear(u_int16_t lid,
            phys_port_t port_number,
            const clbck_data_t *p_clbck_data);

    int VSPortRoutingDecisionCountersGet(u_int16_t lid,
            phys_port_t port_number,
            struct port_routing_decision_counters *p_routing_decision_counters,
            const clbck_data_t *p_clbck_data);


   /*
    * Returns: mad status[bits: 0-7] / IBIS_MAD_STATUS_SEND_FAILED / IBIS_MAD_STATUS_RECV_FAILED /
    *  IBIS_MAD_STATUS_TIMEOUT / IBIS_MAD_STATUS_GENERAL_ERR
    */
    int VSPerVLCounters(bool is_reset_cntr,
            u_int16_t lid,
            phys_port_t port_number,
            u_int32_t attr_id,
            struct PM_PortRcvXmitCntrsSlVl *p_pm_port_rcvxmit_data_slvl,
            const clbck_data_t *p_clbck_data);

    /*
     * Returns: mad status[bits: 0-7] / IBIS_MAD_STATUS_SEND_FAILED / IBIS_MAD_STATUS_RECV_FAILED /
     *  IBIS_MAD_STATUS_TIMEOUT / IBIS_MAD_STATUS_GENERAL_ERR
     */
    int VSMirroringInfoGet(u_int16_t lid,
                struct VS_MirroringInfo *p_mirroring_info,
                const clbck_data_t *p_clbck_data = NULL);

    /*
     * Returns: mad status[bits: 0-7] / IBIS_MAD_STATUS_SEND_FAILED / IBIS_MAD_STATUS_RECV_FAILED /
     *  IBIS_MAD_STATUS_TIMEOUT / IBIS_MAD_STATUS_GENERAL_ERR
     */
    int VSMirroringAgentGet(u_int16_t lid,
                u_int8_t mirror_agent_index,
                struct VS_MirroringAgent *p_mirroring_agent,
                const clbck_data_t *p_clbck_data = NULL);

    /*
     * Returns: mad status[bits: 0-7] / IBIS_MAD_STATUS_SEND_FAILED / IBIS_MAD_STATUS_RECV_FAILED /
     *  IBIS_MAD_STATUS_TIMEOUT / IBIS_MAD_STATUS_GENERAL_ERR
     */
    int VSMirroringAgentSet(u_int16_t lid,
                u_int8_t mirror_agent_index,
                struct VS_MirroringAgent *p_mirroring_agent,
                const clbck_data_t *p_clbck_data = NULL);

    /*
     * Returns: mad status[bits: 0-7] / IBIS_MAD_STATUS_SEND_FAILED / IBIS_MAD_STATUS_RECV_FAILED /
     *  IBIS_MAD_STATUS_TIMEOUT / IBIS_MAD_STATUS_GENERAL_ERR
     */
    int VSMirroringGlobalTriggerGet(u_int16_t lid,
                u_int8_t trigger,
                struct VS_MirroringGlobalTrigger *p_mirroring_global_trigger,
                const clbck_data_t *p_clbck_data = NULL);

    /*
     * Returns: mad status[bits: 0-7] / IBIS_MAD_STATUS_SEND_FAILED / IBIS_MAD_STATUS_RECV_FAILED /
     *  IBIS_MAD_STATUS_TIMEOUT / IBIS_MAD_STATUS_GENERAL_ERR
     */
    int VSMirroringGlobalTriggerSet(u_int16_t lid,
                u_int8_t trigger,
                struct VS_MirroringGlobalTrigger *p_mirroring_global_trigger,
                const clbck_data_t *p_clbck_data = NULL);

    /*
     * Returns: mad status[bits: 0-7] / IBIS_MAD_STATUS_SEND_FAILED / IBIS_MAD_STATUS_RECV_FAILED /
     *  IBIS_MAD_STATUS_TIMEOUT / IBIS_MAD_STATUS_GENERAL_ERR
     */
    int VSCongestionMirroringGet(u_int16_t lid,
                bool global,   // if true, configure all mission ports
                u_int8_t port, // reserved if global is set to true
                struct VS_CongestionMirroring *p_mirroring_vl,
                const clbck_data_t *p_clbck_data = NULL);

    /*
     * Returns: mad status[bits: 0-7] / IBIS_MAD_STATUS_SEND_FAILED / IBIS_MAD_STATUS_RECV_FAILED /
     *  IBIS_MAD_STATUS_TIMEOUT / IBIS_MAD_STATUS_GENERAL_ERR
     */
    int VSCongestionMirroringSet(u_int16_t lid,
                bool global,   // if true, configure all mission ports
                u_int8_t port, // reserved if global is set to true
                struct VS_CongestionMirroring *p_mirroring_vl,
                const clbck_data_t *p_clbck_data = NULL);

    ////////////////////
    //cc class methods
    ////////////////////
    /*
     * Returns: mad status[bits: 0-15] / IBIS_MAD_STATUS_SEND_FAILED / IBIS_MAD_STATUS_RECV_FAILED /
     *  IBIS_MAD_STATUS_TIMEOUT / IBIS_MAD_STATUS_GENERAL_ERR
     */
    int CCMadGetSet(u_int16_t lid,
            u_int8_t sl,
            u_int8_t method,
            u_int16_t attribute_id,
            u_int32_t attribute_modifier,
            const data_func_set_t *p_cc_log_attribute_data,
            const data_func_set_t *p_cc_mgt_attribute_data,
            const clbck_data_t *p_clbck_data = NULL);

    /*
     * Returns: mad status[bits: 0-7] / IBIS_MAD_STATUS_SEND_FAILED / IBIS_MAD_STATUS_RECV_FAILED /
     *  IBIS_MAD_STATUS_TIMEOUT / IBIS_MAD_STATUS_GENERAL_ERR
     */
    int CCClassPortInfoGet(u_int16_t lid,
            u_int8_t sl,
            struct IB_ClassPortInfo *p_ib_class_port_info,
            const clbck_data_t *p_clbck_data = NULL);
    int CCClassPortInfoSet(u_int16_t lid,
            u_int8_t sl,
            struct IB_ClassPortInfo *p_ib_class_port_info,
            const clbck_data_t *p_clbck_data = NULL);
    int CCCongestionInfoGet(u_int16_t lid,
            u_int8_t sl,
            struct CC_CongestionInfo *p_cc_congestion_info,
            const clbck_data_t *p_clbck_data = NULL);
    int CCCongestionKeyInfoGet(u_int16_t lid,
            u_int8_t sl,
            struct CC_CongestionKeyInfo *p_cc_congestion_key_info,
            const clbck_data_t *p_clbck_data = NULL);
    int CCCongestionKeyInfoSet(u_int16_t lid,
            u_int8_t sl,
            struct CC_CongestionKeyInfo *p_cc_congestion_key_info,
            const clbck_data_t *p_clbck_data = NULL);
    int CCCongestionLogSwitchGet(u_int16_t lid,
            u_int8_t sl,
            struct CC_CongestionLogSwitch *p_cc_congestion_log_sw,
            const clbck_data_t *p_clbck_data = NULL);
    int CCCongestionLogCAGet(u_int16_t lid,
            u_int8_t sl,
            struct CC_CongestionLogCA *p_cc_congestion_log_ca,
            const clbck_data_t *p_clbck_data = NULL);
    int CCSwitchCongestionSettingGet(u_int16_t lid,
            u_int8_t sl,
            struct CC_SwitchCongestionSetting *p_cc_sw_congestion_setting,
            const clbck_data_t *p_clbck_data = NULL);
    int CCSwitchCongestionSettingSet(u_int16_t lid,
            u_int8_t sl,
            struct CC_SwitchCongestionSetting *p_cc_sw_congestion_setting,
            const clbck_data_t *p_clbck_data = NULL);
    int CCSwitchPortCongestionSettingGet(u_int16_t lid,
            u_int8_t sl,
            u_int8_t block_idx,
            struct CC_SwitchPortCongestionSetting *p_cc_sw_port_congestion_setting,
            const clbck_data_t *p_clbck_data = NULL);
    int CCSwitchPortCongestionSettingSet(u_int16_t lid,
            u_int8_t sl,
            u_int8_t block_idx,
            struct CC_SwitchPortCongestionSetting *p_cc_sw_port_congestion_setting,
            const clbck_data_t *p_clbck_data = NULL);
    int CCCACongestionSettingGet(u_int16_t lid,
            u_int8_t sl,
            struct CC_CACongestionSetting *p_cc_ca_congestion_setting,
            const clbck_data_t *p_clbck_data = NULL);
    int CCCACongestionSettingSet(u_int16_t lid,
            u_int8_t sl,
            struct CC_CACongestionSetting *p_cc_ca_congestion_setting,
            const clbck_data_t *p_clbck_data = NULL);
    int CCCongestionControlTableGet(u_int16_t lid,
            u_int8_t sl,
            u_int8_t block_idx,
            struct CC_CongestionControlTable *p_cc_congestion_control_table,
            const clbck_data_t *p_clbck_data = NULL);
    int CCCongestionControlTableSet(u_int16_t lid,
            u_int8_t sl,
            u_int8_t block_idx,
            struct CC_CongestionControlTable *p_cc_congestion_control_table,
            const clbck_data_t *p_clbck_data = NULL);
    int CCTimeStampGet(u_int16_t lid,
            u_int8_t sl,
            struct CC_TimeStamp *p_cc_time_stamp,
            const clbck_data_t *p_clbck_data = NULL);

    // CC configuration

    int CCEnhancedInfoGet(u_int16_t lid,
            u_int8_t sl,
            struct CC_EnhancedCongestionInfo *p_cc_enhanced_info,
            const clbck_data_t *p_clbck_data = NULL);
    // switch MADs
    int CCSwitchGeneralSettingsGet(u_int16_t lid,
               u_int8_t sl,
               struct CC_CongestionSwitchGeneralSettings *p_cc_switch_general_settings,
               const clbck_data_t *p_clbck_data = NULL);

    int CCPortProfileSettingsGet(u_int16_t lid,
               u_int8_t port,
               u_int8_t sl,
               struct CC_CongestionPortProfileSettings *p_cc_port_profile_settings,
               const clbck_data_t *p_clbck_data = NULL);

    int CCSLMappingSettingsGet(u_int16_t lid,
               u_int8_t port,
               u_int8_t sl,
               struct CC_CongestionSLMappingSettings *p_cc_sl_mapping_settings,
               const clbck_data_t *p_clbck_data = NULL);
    // HCA MADs
    int CCHCAGeneralSettingsGet(u_int16_t lid,
               u_int8_t sl,
               struct CC_CongestionHCAGeneralSettings *p_cc_hca_general_settings,
               const clbck_data_t *p_clbck_data = NULL);

    int CCHCARPParametersGet(u_int16_t lid,
               u_int8_t sl,
               struct CC_CongestionHCARPParameters *p_cc_hca_rp_parameters,
               const clbck_data_t *p_clbck_data = NULL);

    int CCHCANPParametersGet(u_int16_t lid,
               u_int8_t sl,
               struct CC_CongestionHCANPParameters *p_cc_hca_np_parameters,
               const clbck_data_t *p_clbck_data = NULL);

    int CCHCAStatisticsQueryGet(u_int16_t lid,
               u_int8_t sl,
               struct CC_CongestionHCAStatisticsQuery *p_cc_hca_statistics_query,
               const clbck_data_t *p_clbck_data = NULL,
               bool to_clear_congestion_counters = false);

    int CCHCAAlgoConfigGet(u_int16_t lid,
               u_int8_t algo_slot,
               u_int8_t encap_type,
               struct CC_CongestionHCAAlgoConfig *p_cc_hca_algo_config,
               const clbck_data_t *p_clbck_data);

    int CCHCAAlgoConfigParamGet(u_int16_t lid,
               u_int8_t algo_slot,
               u_int8_t encap_type,
               struct CC_CongestionHCAAlgoConfigParams *p_cc_hca_algo_config_param,
               const clbck_data_t *p_clbck_data);

    int CCHCAAlgoCountersGet(u_int16_t lid,
               u_int8_t algo_slot,
               u_int8_t encap_type,
               struct CC_CongestionHCAAlgoCounters *p_cc_hca_algo_counters,
               const clbck_data_t *p_clbck_data,
               bool to_clear_congestion_counters = false);

    int CCHCAAlgoCountersSet(u_int16_t lid,
               u_int8_t algo_slot,
               u_int8_t encap_type,
               struct CC_CongestionHCAAlgoCounters *p_cc_hca_algo_counters,
               const clbck_data_t *p_clbck_data,
               bool to_clear_congestion_counters = false);

    ////////////////////
    //AM class methods
    ////////////////////
    /*
     * Returns: mad status[bits: 0-15] / IBIS_MAD_STATUS_SEND_FAILED / IBIS_MAD_STATUS_RECV_FAILED /
     *  IBIS_MAD_STATUS_TIMEOUT / IBIS_MAD_STATUS_GENERAL_ERR
     */
    int AMMadGetSet(u_int16_t lid,
            u_int8_t sl,
            u_int8_t method,
            u_int16_t attribute_id,
            u_int32_t attribute_modifier,
            u_int64_t am_key,
            u_int8_t class_version,
            const data_func_set_t & attribute_data,
            const clbck_data_t *p_clbck_data = NULL);

    /*
     * Returns: mad status[bits: 0-7] / IBIS_MAD_STATUS_SEND_FAILED / IBIS_MAD_STATUS_RECV_FAILED /
     *  IBIS_MAD_STATUS_TIMEOUT / IBIS_MAD_STATUS_GENERAL_ERR
     */
    int AMClassPortInfoGet(u_int16_t lid,
            u_int8_t sl,
            uint64_t am_key,
            struct IB_ClassPortInfo *p_ib_class_port_info,
            const clbck_data_t *p_clbck_data = NULL);

    /*
     * Returns: mad status[bits: 0-7] / IBIS_MAD_STATUS_SEND_FAILED / IBIS_MAD_STATUS_RECV_FAILED /
     *  IBIS_MAD_STATUS_TIMEOUT / IBIS_MAD_STATUS_GENERAL_ERR
     */
    int AMClassPortInfoSet(u_int16_t lid,
            u_int8_t sl,
            uint64_t am_key,
            struct IB_ClassPortInfo *p_ib_class_port_info,
            const clbck_data_t *p_clbck_data = NULL);

    /*
     * Returns: mad status[bits: 0-7] / IBIS_MAD_STATUS_SEND_FAILED / IBIS_MAD_STATUS_RECV_FAILED /
     *  IBIS_MAD_STATUS_TIMEOUT / IBIS_MAD_STATUS_GENERAL_ERR
     */
    int AMKeyInfoGet(u_int16_t lid,
        u_int8_t sl,
        uint64_t am_key,
        uint8_t class_version,
        struct AM_AMKeyInfo *p_am_key_info,
        const clbck_data_t *p_clbck_data = NULL);

    /*
     * Returns: mad status[bits: 0-7] / IBIS_MAD_STATUS_SEND_FAILED / IBIS_MAD_STATUS_RECV_FAILED /
     *  IBIS_MAD_STATUS_TIMEOUT / IBIS_MAD_STATUS_GENERAL_ERR
     */
    int AMKeyInfoSet(u_int16_t lid,
        u_int8_t sl,
        uint64_t am_key,
        uint8_t class_version,
        struct AM_AMKeyInfo *p_am_key_info,
        const clbck_data_t *p_clbck_data = NULL);

    /*
     * Returns: mad status[bits: 0-7] / IBIS_MAD_STATUS_SEND_FAILED / IBIS_MAD_STATUS_RECV_FAILED /
     *  IBIS_MAD_STATUS_TIMEOUT / IBIS_MAD_STATUS_GENERAL_ERR
     */
    int AMANInfoGet(u_int16_t lid,
            uint8_t sl,
            uint64_t am_key,
            uint8_t class_version,
            struct AM_ANInfo *p_an_info,
            const clbck_data_t *p_clbck_data = NULL);

    /*
     * Returns: mad status[bits: 0-7] / IBIS_MAD_STATUS_SEND_FAILED / IBIS_MAD_STATUS_RECV_FAILED /
     *  IBIS_MAD_STATUS_TIMEOUT / IBIS_MAD_STATUS_GENERAL_ERR
     */
    int AMANActiveJobsGet(u_int16_t lid,
            uint8_t sl,
            uint64_t am_key,
            uint8_t class_version,
            struct AM_ANActiveJobs *p_an_jobs,
            const clbck_data_t *p_clbck_data = NULL);

    /*
     * Returns: mad status[bits: 0-7] / IBIS_MAD_STATUS_SEND_FAILED / IBIS_MAD_STATUS_RECV_FAILED /
     *  IBIS_MAD_STATUS_TIMEOUT / IBIS_MAD_STATUS_GENERAL_ERR
     */
    int AMANInfoSet(u_int16_t lid,
            uint8_t sl,
            uint64_t am_key,
            uint8_t class_version,
            struct AM_ANInfo *p_an_info,
            const clbck_data_t *p_clbck_data = NULL);
    /*
     * Returns: mad status[bits: 0-7] / IBIS_MAD_STATUS_SEND_FAILED / IBIS_MAD_STATUS_RECV_FAILED /
     *  IBIS_MAD_STATUS_TIMEOUT / IBIS_MAD_STATUS_GENERAL_ERR
     */
    int AMResourceCleanupSet(u_int16_t lid,
            uint8_t sl,
            uint64_t am_key,
            uint8_t class_version,
            struct AM_ResourceCleanup_V2 *p_resource_cleanup,
            const clbck_data_t *p_clbck_data = NULL);

    /*
     * Returns: mad status[bits: 0-7] / IBIS_MAD_STATUS_SEND_FAILED / IBIS_MAD_STATUS_RECV_FAILED /
     *  IBIS_MAD_STATUS_TIMEOUT / IBIS_MAD_STATUS_GENERAL_ERR
     */
    int AMResourceCleanupSet(u_int16_t lid,
            uint8_t sl,
            uint64_t am_key,
            uint8_t class_version,
            struct AM_ResourceCleanup *p_resource_cleanup,
            const clbck_data_t *p_clbck_data = NULL);

    /*
     * Returns: mad status[bits: 0-7] / IBIS_MAD_STATUS_SEND_FAILED / IBIS_MAD_STATUS_RECV_FAILED /
     *  IBIS_MAD_STATUS_TIMEOUT / IBIS_MAD_STATUS_GENERAL_ERR
     */
    int AMQPAllocationSet(u_int16_t lid,
            uint8_t sl,
            uint64_t am_key,
            uint8_t class_version,
            struct AM_QPAllocation *p_qp_allocation,
            const clbck_data_t *p_clbck_data = NULL);

    /*
     * Returns: mad status[bits: 0-7] / IBIS_MAD_STATUS_SEND_FAILED / IBIS_MAD_STATUS_RECV_FAILED /
     *  IBIS_MAD_STATUS_TIMEOUT / IBIS_MAD_STATUS_GENERAL_ERR
     */
    int AMQPCConfigSet(u_int16_t lid,
            uint8_t sl,
            uint64_t am_key,
            uint8_t class_version,
            struct AM_QPCConfig *p_qpc_config,
            const clbck_data_t *p_clbck_data = NULL);

    /*
     * Returns: mad status[bits: 0-7] / IBIS_MAD_STATUS_SEND_FAILED / IBIS_MAD_STATUS_RECV_FAILED /
     *  IBIS_MAD_STATUS_TIMEOUT / IBIS_MAD_STATUS_GENERAL_ERR
     */
    int AMQPCConfigGet(u_int16_t lid,
            uint8_t sl,
            uint64_t am_key,
            uint8_t class_version,
            struct AM_QPCConfig *p_qpc_config,
            const clbck_data_t *p_clbck_data = NULL);

    /*
     * Returns: mad status[bits: 0-7] / IBIS_MAD_STATUS_SEND_FAILED / IBIS_MAD_STATUS_RECV_FAILED /
     *  IBIS_MAD_STATUS_TIMEOUT / IBIS_MAD_STATUS_GENERAL_ERR
     */
    int AMTreeConfigSet(u_int16_t lid,
            uint8_t sl,
            uint64_t am_key,
            uint8_t class_version,
            struct AM_TreeConfig *p_tree_config,
            const clbck_data_t *p_clbck_data = NULL);

    /*
     * Returns: mad status[bits: 0-7] / IBIS_MAD_STATUS_SEND_FAILED / IBIS_MAD_STATUS_RECV_FAILED /
     *  IBIS_MAD_STATUS_TIMEOUT / IBIS_MAD_STATUS_GENERAL_ERR
     */
    int AMPerformanceCountersGet(u_int16_t lid,
        uint8_t sl,
        uint64_t am_key,
        uint8_t class_version,
        struct AM_PerformanceCounters *p_perf_cntr,
        const clbck_data_t *p_clbck_data);

    /*
     * Returns: mad status[bits: 0-7] / IBIS_MAD_STATUS_SEND_FAILED / IBIS_MAD_STATUS_RECV_FAILED /
     *  IBIS_MAD_STATUS_TIMEOUT / IBIS_MAD_STATUS_GENERAL_ERR
     */
    int AMPerformanceCountersGet(u_int16_t lid,
        uint8_t sl,
        uint64_t am_key,
        uint8_t class_version,
        u_int8_t mode,
        struct AM_PerformanceCounters *p_perf_cntr,
        const clbck_data_t *p_clbck_data,
        phys_port_t port_number = IBIS_ALL_PORTS_SELECT);

    /*
     * Returns: mad status[bits: 0-7] / IBIS_MAD_STATUS_SEND_FAILED / IBIS_MAD_STATUS_RECV_FAILED /
     *  IBIS_MAD_STATUS_TIMEOUT / IBIS_MAD_STATUS_GENERAL_ERR
     */
    int AMPerformanceCountersSet(u_int16_t lid,
        uint8_t sl,
        uint64_t am_key,
        uint8_t class_version,
        struct AM_PerformanceCounters *p_perf_cntr,
        const clbck_data_t *p_clbck_data);

    /*
     * Returns: mad status[bits: 0-7] / IBIS_MAD_STATUS_SEND_FAILED / IBIS_MAD_STATUS_RECV_FAILED /
     *  IBIS_MAD_STATUS_TIMEOUT / IBIS_MAD_STATUS_GENERAL_ERR
     */
    int AMPerformanceCountersSet(u_int16_t lid,
        uint8_t sl,
        uint64_t am_key,
        uint8_t class_version,
        u_int8_t mode,
        struct AM_PerformanceCounters *p_perf_cntr,
        const clbck_data_t *p_clbck_data,
        phys_port_t port_number = IBIS_ALL_PORTS_SELECT);

    /*
     * Returns: mad status[bits: 0-7] / IBIS_MAD_STATUS_SEND_FAILED / IBIS_MAD_STATUS_RECV_FAILED /
     *  IBIS_MAD_STATUS_TIMEOUT / IBIS_MAD_STATUS_GENERAL_ERR
     */
    int AMTreeConfigGet(u_int16_t lid,
        uint8_t sl,
        uint64_t am_key,
        uint8_t class_version,
        struct AM_TreeConfig *p_tree_config,
        const clbck_data_t *p_clbck_data);

    /*
     * Returns: mad status[bits: 0-7] / IBIS_MAD_STATUS_SEND_FAILED / IBIS_MAD_STATUS_RECV_FAILED /
     *  IBIS_MAD_STATUS_TIMEOUT / IBIS_MAD_STATUS_GENERAL_ERR
     */
    int AM_TreeToJobBindGet(u_int16_t lid,
        uint8_t sl,
        uint64_t am_key,
        uint8_t class_version,
        struct AM_TreeToJobBind *p_tree_job_bind,
        const clbck_data_t *p_clbck_data = NULL);

    /*
     * Returns: mad status[bits: 0-7] / IBIS_MAD_STATUS_SEND_FAILED / IBIS_MAD_STATUS_RECV_FAILED /
     *  IBIS_MAD_STATUS_TIMEOUT / IBIS_MAD_STATUS_GENERAL_ERR
     */
    int AM_TreeToJobBindSet(u_int16_t lid,
        uint8_t sl,
        uint64_t am_key,
        uint8_t class_version,
        struct AM_TreeToJobBind *p_tree_job_bind,
        const clbck_data_t *p_clbck_data = NULL);

    /*
     * Returns: mad status[bits: 0-7] / IBIS_MAD_STATUS_SEND_FAILED / IBIS_MAD_STATUS_RECV_FAILED /
     *  IBIS_MAD_STATUS_TIMEOUT / IBIS_MAD_STATUS_GENERAL_ERR
     */
    int AM_ANSemaphoreInfoGet(u_int16_t lid,
        uint8_t sl,
        uint64_t am_key,
        uint8_t class_version,
        struct AM_ANSemaphoreInfo *p_an_semaphore_info,
        const clbck_data_t *p_clbck_data = NULL);

    /*
     * Returns: mad status[bits: 0-7] / IBIS_MAD_STATUS_SEND_FAILED / IBIS_MAD_STATUS_RECV_FAILED /
     *  IBIS_MAD_STATUS_TIMEOUT / IBIS_MAD_STATUS_GENERAL_ERR
     */
    int AMANSATQPInfoGet(u_int16_t lid,
        uint8_t sl,
        uint64_t am_key,
        uint8_t class_version,
        struct AM_ANSATQPInfo *p_an_sat_qp_info,
        const clbck_data_t *p_clbck_data = NULL);

    /*
     * Returns: mad status[bits: 0-7] / IBIS_MAD_STATUS_SEND_FAILED / IBIS_MAD_STATUS_RECV_FAILED /
     *  IBIS_MAD_STATUS_TIMEOUT / IBIS_MAD_STATUS_GENERAL_ERR
     */
    int AMQPDatabaseGet(u_int16_t lid,
        uint8_t sl,
        uint64_t am_key,
        uint8_t class_version,
        struct AM_QPDatabase *p_an_qp_database,
        const clbck_data_t *p_clbck_data = NULL);

    /*
     * Returns: mad status[bits: 0-7] / IBIS_MAD_STATUS_SEND_FAILED / IBIS_MAD_STATUS_RECV_FAILED /
     *  IBIS_MAD_STATUS_TIMEOUT / IBIS_MAD_STATUS_GENERAL_ERR
     */
    int AMQuotaConfigSet(u_int16_t lid,
            uint8_t sl,
            uint64_t am_key,
            uint8_t class_version,
            struct AM_QuotaConfig *p_tree_config,
            const clbck_data_t *p_clbck_data = NULL);

    /*
     * Returns: mad status[bits: 0-7] / IBIS_MAD_STATUS_SEND_FAILED / IBIS_MAD_STATUS_RECV_FAILED /
     *  IBIS_MAD_STATUS_TIMEOUT / IBIS_MAD_STATUS_GENERAL_ERR
     */
    int AMQuotaConfigGet(u_int16_t lid,
            uint8_t sl,
            uint64_t am_key,
            uint8_t class_version,
            struct AM_QuotaConfig *p_tree_config,
            const clbck_data_t *p_clbck_data = NULL);

    /*
     * Returns: mad status[bits: 0-7] / IBIS_MAD_STATUS_SEND_FAILED / IBIS_MAD_STATUS_RECV_FAILED /
     *  IBIS_MAD_STATUS_TIMEOUT / IBIS_MAD_STATUS_GENERAL_ERR
     */
    int AMPortCreditResourcesAllocationSet(u_int16_t lid,
            uint8_t sl,
            uint64_t am_key,
            uint8_t class_version,
            uint8_t port_number,
            struct AM_PortCreditResourcesAllocation *p_port_credits_res_alloc,
            const clbck_data_t *p_clbck_data = NULL);

    /*
     * Returns: mad status[bits: 0-7] / IBIS_MAD_STATUS_SEND_FAILED / IBIS_MAD_STATUS_RECV_FAILED /
     *  IBIS_MAD_STATUS_TIMEOUT / IBIS_MAD_STATUS_GENERAL_ERR
     */
    int AMPortCreditResourcesAllocationGet(u_int16_t lid,
            uint8_t sl,
            uint64_t am_key,
            uint8_t class_version,
            uint8_t port_number,
            struct AM_PortCreditResourcesAllocation *p_port_credits_res_alloc,
            const clbck_data_t *p_clbck_data = NULL);

    /*
     * Returns: mad status[bits: 0-7] / IBIS_MAD_STATUS_SEND_FAILED / IBIS_MAD_STATUS_RECV_FAILED /
     *  IBIS_MAD_STATUS_TIMEOUT / IBIS_MAD_STATUS_GENERAL_ERR
     */
    int AMMulticastPrivateLFTSet(u_int16_t lid,
            uint8_t sl,
            uint64_t am_key,
            uint8_t class_version,
            uint16_t mlid_block,
            uint8_t port_group,
            uint8_t plft,
            struct AM_MulticastPrivateLFT *p_multicast_table,
            const clbck_data_t *p_clbck_data = NULL);

    /*
     * Returns: mad status[bits: 0-7] / IBIS_MAD_STATUS_SEND_FAILED / IBIS_MAD_STATUS_RECV_FAILED /
     *  IBIS_MAD_STATUS_TIMEOUT / IBIS_MAD_STATUS_GENERAL_ERR
     */
    int AMMulticastPrivateLFTGet(u_int16_t lid,
            uint8_t sl,
            uint64_t am_key,
            uint8_t class_version,
            uint16_t mlid_block,
            uint8_t port_group,
            uint8_t plft,
            struct AM_MulticastPrivateLFT *p_multicast_table,
            const clbck_data_t *p_clbck_data = NULL);

    /*
     * Register AM traps handler
     * Returns: 0 - success / 1 - error
     */
    int RegisterAmTrapsHandler(mad_handler_callback_func_t callback_func,
                               void *context);

    /*
     * Send TrapRepress for AM trap
     */
    int RepressAmTrap(ib_address_t *p_ib_address,
                      MAD_AggregationManagement *p_am_mad,
                      Notice *p_notice);


    ////////////////////
    // Class C methods
    ////////////////////
    /*
     * Returns: mad status[bits: 0-15] / IBIS_MAD_STATUS_SEND_FAILED / IBIS_MAD_STATUS_RECV_FAILED /
     *  IBIS_MAD_STATUS_TIMEOUT / IBIS_MAD_STATUS_GENERAL_ERR
     */
    int ClassCMadGetSet(u_int16_t lid,
            u_int8_t sl,
            u_int8_t method,
            u_int16_t attribute_id,
            u_int32_t attribute_modifier,
            const data_func_set_t & attribute_data,
            const clbck_data_t *p_clbck_data);

    int ClassCPortInfoGet(u_int16_t lid,
            u_int8_t sl,
            struct IB_ClassPortInfo *p_ib_class_port_info,
            const clbck_data_t *p_clbck_data);

    int ClassCNeighborsInfoGet(u_int16_t lid,
            u_int8_t sl,
            u_int32_t block,
            struct NeighborsInfo *p_neighbors_info,
            const clbck_data_t *p_clbck_data);

    int ClassCKeyInfoGet(u_int16_t lid,
            u_int8_t sl,
            struct Class_C_KeyInfo *p_key_info,
            const clbck_data_t *p_clbck_data);

    ////////////////////
    // Class RDM methods
    ////////////////////

    int ClassRDMMadGetSet(u_int16_t lid,
            u_int8_t sl,
            u_int8_t method,
            u_int16_t attribute_id,
            u_int32_t attribute_modifier,
            const data_func_set_t & attribute_data,
            const clbck_data_t *p_clbck_data);

    /*
     * Returns: mad status[bits: 0-7] / IBIS_MAD_STATUS_SEND_FAILED / IBIS_MAD_STATUS_RECV_FAILED /
     *  IBIS_MAD_STATUS_TIMEOUT / IBIS_MAD_STATUS_GENERAL_ERR
     */
    int NVLClassPortInfoGet(u_int16_t lid,
            uint8_t sl,
            struct IB_ClassPortInfo *p_class_port_info,
            const clbck_data_t *p_clbck_data);

    /*
     * Returns: mad status[bits: 0-7] / IBIS_MAD_STATUS_SEND_FAILED / IBIS_MAD_STATUS_RECV_FAILED /
     *  IBIS_MAD_STATUS_TIMEOUT / IBIS_MAD_STATUS_GENERAL_ERR
     */
    int NVLClassPortInfoSet(u_int16_t lid,
            uint8_t sl,
            struct IB_ClassPortInfo *p_class_port_info,
            const clbck_data_t *p_clbck_data);

    /*
     * Returns: mad status[bits: 0-7] / IBIS_MAD_STATUS_SEND_FAILED / IBIS_MAD_STATUS_RECV_FAILED /
     *  IBIS_MAD_STATUS_TIMEOUT / IBIS_MAD_STATUS_GENERAL_ERR
     */
    int NVLReductionInfoSet(u_int16_t lid,
            uint8_t sl,
            struct NVLReductionInfo *p_nvl_reduction_info,
            const clbck_data_t *p_clbck_data = NULL);

    /*
     * Returns: mad status[bits: 0-7] / IBIS_MAD_STATUS_SEND_FAILED / IBIS_MAD_STATUS_RECV_FAILED /
     *  IBIS_MAD_STATUS_TIMEOUT / IBIS_MAD_STATUS_GENERAL_ERR
     */
    int NVLReductionInfoGet(u_int16_t lid,
            uint8_t sl,
            struct NVLReductionInfo *p_nvl_reduction_info,
            const clbck_data_t *p_clbck_data = NULL);

    /*
     * Returns: mad status[bits: 0-7] / IBIS_MAD_STATUS_SEND_FAILED / IBIS_MAD_STATUS_RECV_FAILED /
     *  IBIS_MAD_STATUS_TIMEOUT / IBIS_MAD_STATUS_GENERAL_ERR
     */
    int NVLReductionPortInfoSet(u_int16_t lid,
            uint8_t sl,
            uint16_t port_number,
            uint8_t profile_select,
            struct NVLReductionPortInfo *p_nvl_reduction_port_info,
            const clbck_data_t *p_clbck_data = NULL);

    /*
     * Returns: mad status[bits: 0-7] / IBIS_MAD_STATUS_SEND_FAILED / IBIS_MAD_STATUS_RECV_FAILED /
     *  IBIS_MAD_STATUS_TIMEOUT / IBIS_MAD_STATUS_GENERAL_ERR
     */
    int NVLReductionPortInfoGet(u_int16_t lid,
            uint8_t sl,
            uint16_t port_number,
            uint8_t profile_select,
            struct NVLReductionPortInfo *p_nvl_reduction_port_info,
            const clbck_data_t *p_clbck_data = NULL);
    /*
     * Returns: mad status[bits: 0-7] / IBIS_MAD_STATUS_SEND_FAILED / IBIS_MAD_STATUS_RECV_FAILED /
     *  IBIS_MAD_STATUS_TIMEOUT / IBIS_MAD_STATUS_GENERAL_ERR
     */
    int NVLReductionProfilesConfigSet(u_int16_t lid,
            uint8_t sl,
            uint8_t block_id,
            uint8_t feature_id,
            struct NVLReductionProfilesConfig *p_nvl_reduction_profiles_config,
            const clbck_data_t *p_clbck_data = NULL);

    /*
     * Returns: mad status[bits: 0-7] / IBIS_MAD_STATUS_SEND_FAILED / IBIS_MAD_STATUS_RECV_FAILED /
     *  IBIS_MAD_STATUS_TIMEOUT / IBIS_MAD_STATUS_GENERAL_ERR
     */
    int NVLReductionProfilesConfigGet(u_int16_t lid,
            uint8_t sl,
            uint8_t block_id,
            uint8_t feature_id,
            struct NVLReductionProfilesConfig *p_nvl_reduction_profiles_config,
            const clbck_data_t *p_clbck_data = NULL);
    /*
     * Returns: mad status[bits: 0-7] / IBIS_MAD_STATUS_SEND_FAILED / IBIS_MAD_STATUS_RECV_FAILED /
     *  IBIS_MAD_STATUS_TIMEOUT / IBIS_MAD_STATUS_GENERAL_ERR
     */
    int NVLReductionForwardingTableSet(u_int16_t lid,
            uint8_t sl,
            uint16_t block_index,
            struct NVLReductionForwardingTable *p_reduction_forwarding_table,
            const clbck_data_t *p_clbck_data = NULL);

    /*
     * Returns: mad status[bits: 0-7] / IBIS_MAD_STATUS_SEND_FAILED / IBIS_MAD_STATUS_RECV_FAILED /
     *  IBIS_MAD_STATUS_TIMEOUT / IBIS_MAD_STATUS_GENERAL_ERR
     */
    int NVLReductionForwardingTableGet(u_int16_t lid,
            uint8_t sl,
            uint16_t block_index,
            struct NVLReductionForwardingTable *p_reduction_forwarding_table,
            const clbck_data_t *p_clbck_data = NULL);
    /*
     * Returns: mad status[bits: 0-7] / IBIS_MAD_STATUS_SEND_FAILED / IBIS_MAD_STATUS_RECV_FAILED /
     *  IBIS_MAD_STATUS_TIMEOUT / IBIS_MAD_STATUS_GENERAL_ERR
     */
    int NVLPenaltyBoxConfigSet(u_int16_t lid,
            uint8_t sl,
            uint8_t block_index,
            struct NVLPenaltyBoxConfig *p_penalty_box_config,
            const clbck_data_t *p_clbck_data = NULL);

    /*
     * Returns: mad status[bits: 0-7] / IBIS_MAD_STATUS_SEND_FAILED / IBIS_MAD_STATUS_RECV_FAILED /
     *  IBIS_MAD_STATUS_TIMEOUT / IBIS_MAD_STATUS_GENERAL_ERR
     */
    int NVLPenaltyBoxConfigGet(u_int16_t lid,
            uint8_t sl,
            uint8_t block_index,
            struct NVLPenaltyBoxConfig *p_penalty_box_config,
            const clbck_data_t *p_clbck_data = NULL);

    /*
     * Returns: mad status[bits: 0-7] / IBIS_MAD_STATUS_SEND_FAILED / IBIS_MAD_STATUS_RECV_FAILED /
     *  IBIS_MAD_STATUS_TIMEOUT / IBIS_MAD_STATUS_GENERAL_ERR
     */
    int NVLReductionConfigureMLIDMonitorsSet(u_int16_t lid,
            uint8_t sl,
            uint16_t port_select,
            uint8_t profile_number_enable,
            struct NVLReductionConfigureMLIDMonitors *p_reduction_configure_mlid_monitors,
            const clbck_data_t *p_clbck_data = NULL);

    /*
     * Returns: mad status[bits: 0-7] / IBIS_MAD_STATUS_SEND_FAILED / IBIS_MAD_STATUS_RECV_FAILED /
     *  IBIS_MAD_STATUS_TIMEOUT / IBIS_MAD_STATUS_GENERAL_ERR
     */

    int NVLReductionConfigureMLIDMonitorsGet(u_int16_t lid,
            uint8_t sl,
            uint16_t port_select,
            uint8_t profile_number_enable,
            struct NVLReductionConfigureMLIDMonitors *p_reduction_configure_mlid_monitors,
            const clbck_data_t *p_clbck_data = NULL);

    /*
     * Returns: mad status[bits: 0-7] / IBIS_MAD_STATUS_SEND_FAILED / IBIS_MAD_STATUS_RECV_FAILED /
     *  IBIS_MAD_STATUS_TIMEOUT / IBIS_MAD_STATUS_GENERAL_ERR
     */

    int NVLReductionCountersSet(u_int16_t lid,
            uint8_t sl,
            struct NVLReductionCounters *p_reduction_counters,
            const clbck_data_t *p_clbck_data = NULL);
    /*
     * Returns: mad status[bits: 0-7] / IBIS_MAD_STATUS_SEND_FAILED / IBIS_MAD_STATUS_RECV_FAILED /
     *  IBIS_MAD_STATUS_TIMEOUT / IBIS_MAD_STATUS_GENERAL_ERR
     */

    int NVLReductionCountersGet(u_int16_t lid,
            uint8_t sl,
            struct NVLReductionCounters *p_reduction_counters,
            const clbck_data_t *p_clbck_data = NULL);

     /*
     * Returns: mad status[bits: 0-7] / IBIS_MAD_STATUS_SEND_FAILED / IBIS_MAD_STATUS_RECV_FAILED /
     *  IBIS_MAD_STATUS_TIMEOUT / IBIS_MAD_STATUS_GENERAL_ERR
     */

     int NVLReductionRoundingModeSet(u_int16_t lid,
            uint8_t sl,
            uint16_t port_select,
            uint8_t profile_number_enable,
            struct NVLReductionRoundingMode *p_reduction_rounding_mode,
            const clbck_data_t *p_clbck_data = NULL);

     /*
     * Returns: mad status[bits: 0-7] / IBIS_MAD_STATUS_SEND_FAILED / IBIS_MAD_STATUS_RECV_FAILED /
     *  IBIS_MAD_STATUS_TIMEOUT / IBIS_MAD_STATUS_GENERAL_ERR
     */

    int NVLReductionRoundingModeGet(u_int16_t lid,
            uint8_t sl,
            uint16_t port_select,
            uint8_t profile_number_enable,
            struct NVLReductionRoundingMode *p_reduction_rounding_mode,
            const clbck_data_t *p_clbck_data = NULL);

    /*
     * Register AM traps handler
     * Returns: 0 - success / 1 - error
     */
    int RegisterClassRDMTrapsHandler(mad_handler_callback_func_t callback_func,
                                   void *context);

    /*
     * Send TrapRepress for class RDM trap
     */
    int RepressClassRDMTrap(ib_address_t *p_ib_address,
                          MAD_Class_RDM *p_mad_class_rdm,
                          RDMNotice *p_notice);



    ////////////////////
    //mellanox methods
    ////////////////////
    static bool IsVenMellanox(u_int32_t vendor_id);
    //switch
    static bool IsDevAnafa(u_int16_t dev_id);
    static bool IsDevPelican(u_int16_t dev_id);
    static bool IsDevShaldag(u_int16_t dev_id);
    static bool IsDevSwitchXIB(u_int16_t dev_id);
    static void GetSwitchXIBDevIds(device_id_list_t& mlnx_dev_ids_list,
                                   device_id_list_t& bull_dev_ids_list);
    static void GetAnafaDevIds(device_id_list_t& dev_ids_list);
    static void GetShaldagDevIds(device_id_list_t& mlnx_dev_ids_list,
                                 device_id_list_t& volt_dev_ids_list);
    //bridge
    static bool IsDevBridgeXIB(u_int16_t dev_id);
    static void GetBridgeXIBDevIds(device_id_list_t& dev_ids_list);
    //hca
    static bool IsDevTavor(u_int16_t dev_id);
    static bool IsDevSinai(u_int16_t dev_id);
    static bool IsDevArbel(u_int16_t dev_id);
    static bool IsDevConnectX_1IB(u_int16_t dev_id);
    static bool IsDevConnectX_2IB(u_int16_t dev_id);
    static bool IsDevConnectX_3IB(u_int16_t dev_id);
    static bool IsDevConnectXIB(u_int16_t dev_id);
    static bool IsDevGolan(u_int16_t dev_id);
    static void GetConnectX_3IBDevIds(device_id_list_t& mlnx_dev_ids_list,
                                      device_id_list_t& bull_dev_ids_list);
    static void GetGolanDevIds(device_id_list_t& dev_ids_list);
    static void GetTavorDevIds(device_id_list_t& dev_ids_list);
    static void GetSinaiDevIds(device_id_list_t& dev_ids_list);
    static void GetArbelDevIds(device_id_list_t& dev_ids_list);
    static void GetConnectXDevIds(device_id_list_t& dev_ids_list);
    static void GetConnectX_2DevIds(device_id_list_t& dev_ids_list);
    static void GetConnectX_2_ENtDevIds(device_id_list_t& dev_ids_list);
    static void GetConnectX_2_LxDevIds(device_id_list_t& dev_ids_list);
};

#endif	/* IBIS_H_ */

