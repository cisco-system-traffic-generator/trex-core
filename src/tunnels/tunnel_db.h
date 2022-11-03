#ifndef TUNNEL_DB_H_
#define TUNNEL_DB_H_

#include "tunnels/tunnel_handler.h"


namespace Json {
    class Value;
}

class ClientCfgBase;


/************************************************* CTunnelsLatencyPerPort ****************************************************************************/
class CTunnelsLatencyPerPort {
public:

    /**
    * CTunnelsCtxGroup constructor
    * This class holds the latency context per port.
    *
    * @param client_port_id
    *   Ths client port id, which will generate the ICMP packets with this context.
    *
    * @param client_ip
    *   The source IP address of the ICMP packets.
    *
    * @param server_ip
    *   The destination IP address of the ICMP packets.
    */
    CTunnelsLatencyPerPort(uint8_t client_port_id, uint32_t client_ip, uint32_t server_ip) {
        m_client_port_id = client_port_id;
        m_client_ip = client_ip;
        m_server_ip = server_ip;
    }

    /**
    * Gets the the client_port_id.
    *
    * @return uint8_t
    *   The client_port_id.
    */
    uint8_t get_client_port_id() const {
        return m_client_port_id;
    }

    /**
    * Gets the the client_ip.
    *
    * @return uint32_t ip address
    *   The client_ip - the source IP address of the ICMP packets.
    */
    uint32_t get_client_ip() const {
        return m_client_ip;
    }

    /**
    * Gets the the server_ip.
    *
    * @return uint32_t ip address
    *   The server_ip - the destination IP address of the ICMP packets.
    */
    uint32_t get_server_ip() const {
        return m_server_ip;
    }

    /**
    * Initialize the data argument with the CTunnelsLatencyPerPort data.
    *
    * @param latency_json
    *   The json object to which we want to copy the
    *   CTunnelsLatencyPerPort data to.
    */
    void to_json(Json::Value& latency_json) const;

private:
    uint8_t  m_client_port_id;
    uint32_t m_client_ip;
    uint32_t m_server_ip;
};

/************************************************* CTunnelsCtxGroup ****************************************************************************/


class CTunnelsCtxGroup {
public:
    /**
    * CTunnelsCtxGroup constructor
    * This class holds the tunnel context of range of consecutive clients.
    * The range is being defined by start_ip and end_ip when:
    * start_ip - is the ip address of the first client in range.
    * end_ip - is the ip address of the last client in the range.
    *
    * @param start_ip
    *   The first ip address in range of consecutive clients.
    *
    * @param end_ip
    *   The last ip address in the range of consecutive clients.
    *
    * @param tunnel_data
    *   The tunnel_data holds the shared tunnel info of the group of clients.
    *   The teid field of the tunnel_data holds the teid of the first client in the group.
    *
    * @param teid_jump
    *   The difference between the teids of two consequtive clients in the range.
    *
    * @param activate
    *   bool that determines whether to activate or deactivate by deafult the clients in the group.
    * 
    * @param cvlan_id
    *   Customer VLAN ID. Identifies the VLAN to which the frame belongs
    * 
    * @param svlan_id
    *   Service VLAN ID. Identifies the VLAN to which the QinQ belongs
    */
    CTunnelsCtxGroup(uint32_t start_ip, uint32_t end_ip, uint32_t teid_jump, client_tunnel_data_t& tunnel_data, bool activate = true, uint16_t vid = 0, uint16_t cvlan_id = 0, uint16_t svlan_id = 0) {
        m_start_ip = start_ip;
        m_end_ip = end_ip;
        m_teid_jump = teid_jump;
        m_tunnel_data = tunnel_data;
        m_activate = activate;
        m_vid = vid;
        m_cvlan_id = cvlan_id;
        m_svlan_id = svlan_id;
    }

    CTunnelsCtxGroup() {
        m_start_ip = 0;
        m_end_ip = 0;
        m_teid_jump = 0;
        memset(&m_tunnel_data, 0, sizeof(client_tunnel_data_t));
        m_activate = false;
        m_cvlan_id = 0;
        m_svlan_id = 0;
        m_vid = 0;
    }

    /**
    * Gets the first ip of the group
    *
    * @return uint32_t ip address
    *   The first ip address of the group.
    */
    uint32_t get_start_ip() const {
        return m_start_ip;
    }

    /**
    * Gets the last ip of the group
    *
    * @return uint32_t ip address
    *   The last ip address of the group.
    */
    uint32_t get_end_ip() const {
        return m_end_ip;
    }

    /**
    * Gets the teid_jump
    *
    * @return uint32_t
    *   The difference between the teids of two consequtive clients in the range.
    */
    uint32_t get_teid_jump() const {
        return m_teid_jump;
    }

    /**
    * Gets the activate field.
    *
    * @return bool
    *   bool that determines whether to activate or deactivate by deafult the clients in the group.
    */
    bool is_activate() const {
        return m_activate;
    }

    /**
    * Initialize the data argument with the group tunnel data
    *
    * @param data
    *   The client_tunnel_data_t object to which we want to copy the
    *   group tunnel data to.
    */
    void get_tunnel_data(client_tunnel_data_t& data) const {
        data = m_tunnel_data;
    }

    /**
    * Initialize the data argument with the CTunnelsCtxGroup data.
    *
    * @param group_json
    *   The json object to which we want to copy the
    *   CTunnelsCtxGroup data to.
    */
    void to_json(Json::Value& group_json) const;

private:
    client_tunnel_data_t m_tunnel_data;
    uint32_t             m_start_ip;
    uint32_t             m_end_ip;
    /*The difference between the teids of two consequtive clients in the range.*/
    uint32_t             m_teid_jump;
    bool                 m_activate;
    /* QinQ Tunnel related parameters */
    uint16_t             m_cvlan_id;
    uint16_t             m_svlan_id;

    /* VLAN Tunnel related parameters */
    uint16_t             m_vid;
};


/************************************************* CClientCfgEntryTunnel ****************************************************************************/


class CClientCfgEntryTunnel {
public:
    CClientCfgEntryTunnel(){}

    /**
    * Check whether the given ip address is in the range [ip_start, ip_end]
    * of the CClientCfgEntryTunnel object
    *
    * @param ip
    *   an ip address
    *
    * @return bool:
    *   true in case the ip address is indeed in the range, false otherwise.
    */
    bool contains(uint32_t ip) const {
        return ( (ip >= m_tunnel_group.get_start_ip()) && (ip <= m_tunnel_group.get_end_ip()) );
    }

    /**
    * Assign the a tunnel contexts, depending on the tunnel type, to the ClientCfgBase object.
    *
    * @param info
    *   a ClientCfgBase that will hold the tunnel context of a specific client.
    *
    * @param ip
    *  the ip address of the client that the tunnel context will be assign to.
    */
    void assign(ClientCfgBase &info, uint32_t ip) const;

    /**
    * Whether the clients needs to be activated by deafult.
    *
    * @return bool
    *   bool that determines whether to activate deafult the clients in the group.
    */
    bool is_activate() const {
        return m_tunnel_group.is_activate();
    }

public:
    CTunnelsCtxGroup m_tunnel_group;
};


/************************************************* CTunnelsTopo ****************************************************************************/

typedef std::map<uint8_t, CTunnelsLatencyPerPort> client_per_port_t;

class CTunnelsTopo {
    friend class Topo_Test;

public:
    CTunnelsTopo() {}

    /**
    * Initialize the topo object from a json string.
    *
    * @param json_str
    *   Json string that holds list of CTunnelsCtxGroup objects and list of latency clients with their port id.
    */
    void from_json_str(const std::string &json_str);

    /**
    * Clears the tunnel topo.
    */
    void clear();

    /**
    * Initialize the a json object with the tunnel topo data.
    *
    * @param val
    *   The json object to which we want to copy the
    *   CTunnelsTopo data to.
    */
    void to_json(Json::Value& val);

    /**
    * Gets a vector of CTunnelsCtxGroup that holds the tunnel_topo's context data.
    *
    * @return const std::vector<CTunnelsCtxGroup>&
    *   A vector of CTunnelsCtxGroup that holds the tunnel topo data.
    */
    const std::vector<CTunnelsCtxGroup>& get_tunnel_topo() const;

    /**
    * Gets a map from port id to its client
    *
    * @return const std::map<uint8_t, CTunnelsLatencyPerPort>&
    *   map from port id to latency tunnel context per port
    */
    const client_per_port_t& get_latency_clients() const;

    ~CTunnelsTopo() {}

private:
    /**
    * Verifies there is no intersections between the CTunnelsCtxGroup objects
    * inside the groups vector.
    *
    * @param groups
    *   A vector that holds CTunnelsCtxGroup objects.
    */
    void validate_tunnel_topo(std::vector<CTunnelsCtxGroup>& groups);

    /**
    * Initialize the tunnel groups vector of CTunnelsTopo
    *
    * @param tunnels_group_json
    *   Json object that holds list of CTunnelsCtxGroup objects.
    */
    void from_json_obj_tunnel_groups(Json::Value& tunnels_group_json);

    /**
    * Initialize the latency port_id->client_ip map of CTunnelsTopo
    *
    * @param latency_json
    *   Json object that holds list of latency tunnel context per port.
    */
    void from_json_obj_tunnel_latency(Json::Value& latency_json);

private:
    std::vector<CTunnelsCtxGroup> m_tunnels_groups;
    client_per_port_t             m_latency_clients;
    std::recursive_mutex          m_lock;
};


class TunnelTopoError : public std::runtime_error {
public:
    TunnelTopoError(const std::string &what);
};



/************************************************* CTunnelsDB ****************************************************************************/


class CTunnelsDB {
public:
    CTunnelsDB() {m_cache_group = nullptr;}
    ~CTunnelsDB() {}

    /**
    * Initialize the db from the tunnel topo object
    *
    * @param topo
    *   pointer to a CTunnelsTopo object from which we want to
    *   to initialize the tunnel topo object from.
    */
    void load_from_tunnel_topo(const CTunnelsTopo *topo);

    /**
    * Gets the CClientCfgEntryTunnel that holds
    * the tunnel info of the ip address.
    *
    * @param ip
    *  an IP address in uint32_t.
    *
    * @return CClientCfgEntryTunnel*
    *   in case of succsess: pointer to the CClientCfgEntryTunnel that holds the tunnel info of the ip address.
    *   nullptr in case there are no CClientCfgEntryTunnel object to this ip address.
    */
    CClientCfgEntryTunnel* lookup(uint32_t ip);

    /**
    * Gets the CClientCfgEntryTunnel that holds
    * the tunnel info of the ip address.
    *
    * @param ip
    *  a string IP address.
    *
    * @return CClientCfgEntryTunnel*
    *   in case of succsess: pointer to the CClientCfgEntryTunnel that holds the tunnel info of the ip address.
    *   nullptr in case there are no CClientCfgEntryTunnel object to this ip address.
    */
    CClientCfgEntryTunnel* lookup(const std::string &ip);

    /**
    * Clear the CTunnelsDB object from its tunnel entries.
    *
    **/
    void clear() {
        m_cache_group = nullptr;
        m_groups.clear();
    }

    /**
    * Checks if the CTunnelsDB object has tunnel entries.
    *
    * @return bool
    *   true in case there are no tunnel entries, otherwise false.
    **/
    bool is_empty() {
        return (m_groups.size() == 0);
    }

private:
    /* maps the IP start value to client groups tunnels */
    std::map<uint32_t, CClientCfgEntryTunnel>  m_groups;
    CClientCfgEntryTunnel*                     m_cache_group;
};

#endif