
#include <rte_flow.h>
#include <rte_flow_driver.h>
#include <rte_ethdev.h>
#include <rte_log.h>
#include <rte_pci.h>
#include <rte_malloc.h>
#include <nt.h>

#include "rte_eth_ntacc.h"
#include "filter_ntacc.h"
#include "nt_compat.h"

void *ntacc_add_rules(uint8_t port_id, uint16_t type,
                      uint8_t l4_proto, int queue, char *ntpl_str)
 {
  struct rte_flow *rte_flow = NULL;
  // Simple L4PROTO and TOS/TC bit 1 filter.
  // The most annoying part is that a id <-> struct rte_flow* must be maintained
  struct rte_flow_attr attr = { .ingress=1};
  struct rte_flow_action_queue rte_flow_action_queue = { .index = queue, };
  struct rte_flow_action actions[2] = {
      {
          .type = RTE_FLOW_ACTION_TYPE_QUEUE,
          .conf = &rte_flow_action_queue,
      },
      {
          .type = RTE_FLOW_ACTION_TYPE_END,
          .conf = NULL,
      }
  };
  struct rte_flow_item pattern[3] =
  {
      {
          .type = 0,
          .spec = NULL,
          .last = NULL,
          .mask = NULL,
      },
      {
          .type = RTE_FLOW_ITEM_TYPE_END,
          .spec = NULL,
          .last = NULL,
          .mask = NULL,
      },
      {
          .type = RTE_FLOW_ITEM_TYPE_END,
          .spec = NULL,
          .last = NULL,
          .mask = NULL,
      },
  };
  struct rte_flow_error rte_flow_error;
  switch (type) {
    case RTE_ETH_FLOW_NTPL:
    {
      struct rte_flow_item_ntpl ntpl = {
        .ntpl_str = ntpl_str,
        .tunnel = RTE_FLOW_NTPL_NO_TUNNEL,
      };
      pattern[0].type = RTE_FLOW_ITEM_TYPE_NTPL;
      pattern[0].spec = &ntpl;
      pattern[0].last = NULL;
      pattern[0].mask = NULL;
      rte_flow = rte_flow_create(port_id, &attr, pattern, actions, &rte_flow_error);
    }
    break;
    case RTE_ETH_FLOW_RAW:
    pattern[0].type = RTE_FLOW_ITEM_TYPE_ETH;
    pattern[0].spec = NULL;
    pattern[0].last = NULL;
    pattern[0].mask = NULL;
    rte_flow = rte_flow_create(port_id, &attr, pattern, actions, &rte_flow_error);
    break;
    case RTE_ETH_FLOW_IPV4:
      pattern[0].type = RTE_FLOW_ITEM_TYPE_IPV4;
      pattern[0].spec = NULL;
      pattern[0].last = NULL;
      pattern[0].mask = NULL;
      rte_flow = rte_flow_create(port_id, &attr, pattern, actions, &rte_flow_error);
      break;
    case RTE_ETH_FLOW_NONFRAG_IPV4_UDP:
      {
        pattern[0].type = RTE_FLOW_ITEM_TYPE_IPV4;
        pattern[1].type = RTE_FLOW_ITEM_TYPE_UDP;
        rte_flow = rte_flow_create(port_id, &attr, pattern, actions, &rte_flow_error);
        break;
      }
    case RTE_ETH_FLOW_NONFRAG_IPV4_TCP:
      {
        pattern[0].type = RTE_FLOW_ITEM_TYPE_IPV4;
        pattern[1].type = RTE_FLOW_ITEM_TYPE_TCP;
        rte_flow = rte_flow_create(port_id, &attr, pattern, actions, &rte_flow_error);
        break;
      }
    case RTE_ETH_FLOW_NONFRAG_IPV4_SCTP:
      {
        pattern[0].type = RTE_FLOW_ITEM_TYPE_IPV4;
        pattern[1].type = RTE_FLOW_ITEM_TYPE_SCTP;
        rte_flow = rte_flow_create(port_id, &attr, pattern, actions, &rte_flow_error);
        break;
      }
    case RTE_ETH_FLOW_NONFRAG_IPV4_OTHER:
      {
        pattern[0].type = RTE_FLOW_ITEM_TYPE_IPV4;
        pattern[1].type = RTE_FLOW_ITEM_TYPE_ICMP;
        rte_flow = rte_flow_create(port_id, &attr, pattern, actions, &rte_flow_error);
        break;
      }
    case RTE_ETH_FLOW_IPV6:
      pattern[0].type = RTE_FLOW_ITEM_TYPE_IPV6;
      pattern[0].spec = NULL;
      pattern[0].last = NULL;
      pattern[0].mask = NULL;
      rte_flow = rte_flow_create(port_id, &attr, pattern, actions, &rte_flow_error);
      break;
  case RTE_ETH_FLOW_NONFRAG_IPV6_UDP:
    {
      pattern[0].type = RTE_FLOW_ITEM_TYPE_IPV6;
      pattern[1].type = RTE_FLOW_ITEM_TYPE_UDP;
      rte_flow = rte_flow_create(port_id, &attr, pattern, actions, &rte_flow_error);
      break;
    }
  case RTE_ETH_FLOW_NONFRAG_IPV6_TCP:
    {
      pattern[0].type = RTE_FLOW_ITEM_TYPE_IPV6;
      pattern[1].type = RTE_FLOW_ITEM_TYPE_TCP;
      rte_flow = rte_flow_create(port_id, &attr, pattern, actions, &rte_flow_error);
      break;
    }
  case RTE_ETH_FLOW_NONFRAG_IPV6_SCTP:
    {
      pattern[0].type = RTE_FLOW_ITEM_TYPE_IPV6;
      pattern[1].type = RTE_FLOW_ITEM_TYPE_SCTP;
      rte_flow = rte_flow_create(port_id, &attr, pattern, actions, &rte_flow_error);
      break;
    }
  case RTE_ETH_FLOW_NONFRAG_IPV6_OTHER:
    {
      pattern[0].type = RTE_FLOW_ITEM_TYPE_IPV6;
      pattern[1].type = RTE_FLOW_ITEM_TYPE_ICMP;
      rte_flow = rte_flow_create(port_id, &attr, pattern, actions, &rte_flow_error);
      break;
    }
  }
  if (rte_flow == NULL) {
    printf("Programming port %d failed: %d %s\n",
            port_id, rte_flow_error.type, rte_flow_error.message);
  }
  return rte_flow;
}

void ntacc_del_rules(uint8_t port_id, void *rte_flow)
{
  rte_flow_destroy(port_id, (struct rte_flow*)rte_flow, NULL);
}
