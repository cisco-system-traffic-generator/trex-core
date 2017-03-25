/*-
 * GPL LICENSE SUMMARY
 *
 *   Copyright(c) 2010-2014 Intel Corporation. All rights reserved.
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of version 2 of the GNU General Public License as
 *   published by the Free Software Foundation.
 *
 *   This program is distributed in the hope that it will be useful, but
 *   WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *   General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 *   The full GNU General Public License is included in this distribution
 *   in the file called LICENSE.GPL.
 *
 *   Contact Information:
 *   Intel Corporation
 */

#ifndef _KNI_DEV_H_
#define _KNI_DEV_H_

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/if.h>
#include <linux/wait.h>
#include <linux/sched.h>
#include <linux/netdevice.h>
#include <linux/spinlock.h>
#include <linux/list.h>

#ifdef RTE_KNI_VHOST
#include <net/sock.h>
#endif

#include <exec-env/rte_kni_common.h>
#define KNI_KTHREAD_RESCHEDULE_INTERVAL 5 /* us */

#define MBUF_BURST_SZ 32

/**
 * A structure describing the private information for a kni device.
 */
struct kni_dev {
	/* kni list */
	struct list_head list;

	struct net_device_stats stats;
	int status;
	uint16_t group_id;           /* Group ID of a group of KNI devices */
	uint32_t core_id;            /* Core ID to bind */
	char name[RTE_KNI_NAMESIZE]; /* Network device name */
	struct task_struct *pthread;

	/* wait queue for req/resp */
	wait_queue_head_t wq;
	struct mutex sync_lock;

	/* PCI device id */
	uint16_t device_id;

	/* kni device */
	struct net_device *net_dev;
	struct net_device *lad_dev;
	struct pci_dev *pci_dev;

	/* queue for packets to be sent out */
	void *tx_q;

	/* queue for the packets received */
	void *rx_q;

	/* queue for the allocated mbufs those can be used to save sk buffs */
	void *alloc_q;

	/* free queue for the mbufs to be freed */
	void *free_q;

	/* request queue */
	void *req_q;

	/* response queue */
	void *resp_q;

	void *sync_kva;
	void *sync_va;

	void *mbuf_kva;
	void *mbuf_va;

	/* mbuf size */
	uint32_t mbuf_size;

	/* synchro for request processing */
	unsigned long synchro;

#ifdef RTE_KNI_VHOST
	struct kni_vhost_queue *vhost_queue;

	volatile enum {
		BE_STOP = 0x1,
		BE_START = 0x2,
		BE_FINISH = 0x4,
	} vq_status;
#endif
	/* buffers */
	void *pa[MBUF_BURST_SZ];
	void *va[MBUF_BURST_SZ];
	void *alloc_pa[MBUF_BURST_SZ];
	void *alloc_va[MBUF_BURST_SZ];
};

#ifdef RTE_KNI_VHOST
uint32_t
kni_poll(struct file *file, struct socket *sock, poll_table * wait);
int kni_chk_vhost_rx(struct kni_dev *kni);
int kni_vhost_init(struct kni_dev *kni);
int kni_vhost_backend_release(struct kni_dev *kni);

struct kni_vhost_queue {
	struct sock sk;
	struct socket *sock;
	int vnet_hdr_sz;
	struct kni_dev *kni;
	int sockfd;
	uint32_t flags;
	struct sk_buff *cache;
	struct rte_kni_fifo *fifo;
};

#endif

void kni_net_rx(struct kni_dev *kni);
void kni_net_init(struct net_device *dev);
void kni_net_config_lo_mode(char *lo_str);
void kni_net_poll_resp(struct kni_dev *kni);
void kni_set_ethtool_ops(struct net_device *netdev);

int ixgbe_kni_probe(struct pci_dev *pdev, struct net_device **lad_dev);
void ixgbe_kni_remove(struct pci_dev *pdev);
int igb_kni_probe(struct pci_dev *pdev, struct net_device **lad_dev);
void igb_kni_remove(struct pci_dev *pdev);

#endif
