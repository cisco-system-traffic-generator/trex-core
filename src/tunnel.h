#ifndef _TUNNEL_H_
#define _TUNNEL_H_

class Tunnel {
public:
    virtual ~Tunnel() {}
    
	virtual int Encap(struct rte_mbuf *pkt) { return 0; }
	virtual int Decap(struct rte_mbuf *pkt) { return 0; }

	static Tunnel* Parse(YAML::Node const&);

	static void InstallRxCallback(uint16_t port_id, uint16_t queue);

	static void InstallTxCallback(uint16_t port_id, uint16_t queue);

private:
	static uint16_t RxCallback(uint16_t port_id, uint16_t queue,
			struct rte_mbuf *pkts[], uint16_t nb_pkts, uint16_t max_pkts,
			void *user_param);

	static uint16_t TxCallback(uint16_t port_id, uint16_t queue,
			struct rte_mbuf *pkts[], uint16_t nb_pkts, void *user_param);
};

#endif
