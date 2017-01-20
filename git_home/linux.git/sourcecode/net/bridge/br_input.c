/*
 *	Handle incoming frames
 *	Linux ethernet bridge
 *
 *	Authors:
 *	Lennert Buytenhek		<buytenh@gnu.org>
 *
 *	This program is free software; you can redistribute it and/or
 *	modify it under the terms of the GNU General Public License
 *	as published by the Free Software Foundation; either version
 *	2 of the License, or (at your option) any later version.
 */

#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/netfilter_bridge.h>
#include <linux/export.h>
#include <linux/if_arp.h>
#include "br_private.h"

#define MAX_DEVICE 128

extern struct mac_traffic mac_updownload[MAX_DEVICE];

#ifdef CONFIG_DNI_MCAST_TO_UNICAST
#include <linux/ip.h>
#include <linux/igmp.h>

static inline void
add_mac_cache(struct sk_buff *skb)
{
      unsigned char i, num = 0xff;
      unsigned char *src, check = 1;
      struct iphdr *iph;
      struct ethhdr *ethernet=(struct ethhdr *)skb->mac_header;

      iph = (struct iphdr *)skb->network_header;
      src = ethernet->h_source;

      for (i = 0; i < MCAST_ENTRY_SIZE; i++)
      {
              if (mac_cache[i].valid)
                      if ((++mac_cache[i].count) == MAX_CLEAN_COUNT)
                              mac_cache[i].valid = 0;
      }

      for (i = 0; i < MCAST_ENTRY_SIZE; i++)
      {
              if (mac_cache[i].valid)
              {
                      if (mac_cache[i].sip==iph->saddr)
                      {
                              num = i;
                              break;
                      }
              }
              else if (check)
              {
                      num=i;
                      check = 0;
              }
      }

      if (num < MCAST_ENTRY_SIZE)
      {
              mac_cache[num].valid = mac_cache[num].count = 1;
              memcpy(mac_cache[num].mac, src, 6);
              mac_cache[num].sip = iph->saddr;
              mac_cache[num].dev = skb->dev;
      }
}

#endif

static int check_authorized_mac(unsigned char *dev_mac)
{
    int i;

    for (i = 0; i < MAX_DEVICE; i++) {
        if (mac_updownload[i].flag == 0)
            continue;
        if (compare_ether_addr(mac_updownload[i].mac, dev_mac) == 0)
            return i;
    }

    return -1;
}

/* Bridge group multicast address 802.1d (pg 51). */
const u8 br_group_address[ETH_ALEN] = { 0x01, 0x80, 0xc2, 0x00, 0x00, 0x00 };

/* Hook for brouter */
br_should_route_hook_t __rcu *br_should_route_hook __read_mostly;
EXPORT_SYMBOL(br_should_route_hook);

/* Hook for external Multicast handler */
br_multicast_handle_hook_t __rcu *br_multicast_handle_hook __read_mostly;
EXPORT_SYMBOL_GPL(br_multicast_handle_hook);

/* Hook for external forwarding logic */
br_get_dst_hook_t __rcu *br_get_dst_hook __read_mostly;
EXPORT_SYMBOL_GPL(br_get_dst_hook);

static int br_pass_frame_up(struct sk_buff *skb)
{
	struct net_device *indev, *brdev = BR_INPUT_SKB_CB(skb)->brdev;
	struct net_bridge *br = netdev_priv(brdev);
	struct br_cpu_netstats *brstats = this_cpu_ptr(br->stats);
	unsigned char *dev_mac;
	int ret;

#ifdef CONFIG_DNI_MCAST_TO_UNICAST
      unsigned char *dest;
      struct iphdr *iph;
      unsigned char proto=0;
      struct ethhdr *ethernet=(struct ethhdr *)skb->mac_header;

      // if skb come from wireless interface, ex. ath0, ath1, ath2...
      if (skb->dev->name[0] == 'a')
      {
              iph = (struct iphdr *)skb->network_header;
              proto =  iph->protocol;
              dest = ethernet->h_dest;
              if ( igmp_snoop_enable && MULTICAST_MAC(dest)
                       && (ethernet->h_proto == ETH_P_IP))
              {
                      if (proto == IPPROTO_IGMP)
                              add_mac_cache(skb);
              }
      }
#endif
#ifdef CONFIG_BRIDGE_NETGEAR_ACL
      if (!br_acl_should_pass(br, skb, ACL_CHECK_SRC)) {
              br->dev->stats.rx_dropped++;
              kfree_skb(skb);
              return;
      }
#endif
	u64_stats_update_begin(&brstats->syncp);
	brstats->rx_packets++;
	brstats->rx_bytes += skb->len;
	u64_stats_update_end(&brstats->syncp);

    if (brdev->name != NULL && brdev->name[0] == 'b' &&
        brdev->name[1] == 'r' && brdev->name[2] == '0') {
         dev_mac = eth_hdr(skb)->h_source;
         if (NULL != dev_mac) {
            ret = check_authorized_mac(dev_mac);
            if (ret >= 0) {
                mac_updownload[ret].upload += skb->len;
                //printk("upload package len = %d\n", mac_updownload[ret].upload);
            }
         }
    }

	indev = skb->dev;
	skb->dev = brdev;

	br_drop_fake_rtable(skb);

	return BR_HOOK(NFPROTO_BRIDGE, NF_BR_LOCAL_IN, skb, indev, NULL,
		       netif_receive_skb);
}

extern int brnf_filter_qtn __read_mostly;
/* note: already called with rcu_read_lock */
int br_handle_frame_finish(struct sk_buff *skb)
{
	const unsigned char *dest = eth_hdr(skb)->h_dest;
	struct net_bridge_port *p = br_port_get_rcu(skb->dev);
	struct net_bridge *br;
	struct net_bridge_fdb_entry *dst;
	struct net_bridge_mdb_entry *mdst;
	struct sk_buff *skb2;
	struct net_bridge_port *pdst = NULL;
	br_get_dst_hook_t *get_dst_hook = rcu_dereference(br_get_dst_hook);

	if (!p || p->state == BR_STATE_DISABLED)
		goto drop;

	/* insert into forwarding database after filtering to avoid spoofing */
	br = p->br;
	br_fdb_update(br, p, eth_hdr(skb)->h_source);

	struct iphdr *ip_h;
	struct ethhdr *eth_h;
	struct arp_payload *arp_p;

	eth_h = (struct ethhdr *)skb->mac_header;
	ip_h = (struct iphdr *)(skb->mac_header + sizeof(struct ethhdr));
	arp_p = (struct arp_payload *) (skb->mac_header + sizeof(struct ethhdr) + sizeof(struct arphdr));
	if (brnf_filter_qtn && skb->dev->name[0] == 'h') {
		if (is_multicast_ether_addr(dest) && eth_h->h_proto == __constant_htons(ETH_P_IP) && ip_h->saddr == QTN_CHIP_IP)
			goto drop;

		if ((eth_h->h_proto == __constant_htons(ETH_P_IP) && ip_h->saddr == QTN_CHIP_IP) ||
		    (eth_h->h_proto == __constant_htons(ETH_P_ARP) && arp_p->src_ip == QTN_CHIP_IP)) {
			if ((p->state == BR_STATE_LEARNING) && skb->protocol != htons(ETH_P_PAE))
				goto drop;
			BR_INPUT_SKB_CB(skb)->brdev = br->dev;
			return br_pass_frame_up(skb);
		}
	}

	if (!is_broadcast_ether_addr(dest) && is_multicast_ether_addr(dest) &&
	    br_multicast_rcv(br, p, skb))
		goto drop;

	if ((p->state == BR_STATE_LEARNING) && skb->protocol != htons(ETH_P_PAE))
		goto drop;

	BR_INPUT_SKB_CB(skb)->brdev = br->dev;

	/* The packet skb2 goes to the local host (NULL to skip). */
	skb2 = NULL;

	if (br->dev->flags & IFF_PROMISC)
		skb2 = skb;

	dst = NULL;

	if (skb->protocol == htons(ETH_P_PAE)) {
		skb2 = skb;
		/* Do not forward 802.1x/EAP frames */
		skb = NULL;
	} else if (is_broadcast_ether_addr(dest))
		skb2 = skb;
	else if (is_multicast_ether_addr(dest)
#ifdef CONFIG_DNI_IPV6_PASSTHROUGH
			|| (skb->protocol == __constant_htons(ETH_P_IPV6))
#endif
			) {
		br_multicast_handle_hook_t *multicast_handle_hook = rcu_dereference(br_multicast_handle_hook);
		if (!__br_get(multicast_handle_hook, true, p, skb))
			goto out;

		mdst = br_mdb_get(br, skb);
		if (mdst || BR_INPUT_SKB_CB_MROUTERS_ONLY(skb)) {
			if ((mdst && mdst->mglist) ||
			    br_multicast_is_router(br))
				skb2 = skb;
			br_multicast_forward(mdst, skb, skb2);
			skb = NULL;
			if (!skb2)
				goto out;
		} else
			skb2 = skb;

		br->dev->stats.multicast++;
	} else if ((pdst = __br_get(get_dst_hook, NULL, p, &skb))) {
		if (!skb) goto out;
	} else if ((p->flags & BR_ISOLATE_MODE) ||
		   ((dst = __br_fdb_get(br, dest)) && dst->is_local)) {
		skb2 = skb;
		/* Do not forward the packet since it's local. */
		skb = NULL;
	}

	if (skb) {
		if (dst) {
			dst->used = jiffies;
			pdst = dst->dst;
		}

		if (pdst)
			br_forward(pdst, skb, skb2);
		else
			br_flood_forward(br, skb, skb2);
	}

	if (skb2)
		return br_pass_frame_up(skb2);

out:
	return 0;
drop:
	kfree_skb(skb);
	goto out;
}

/* note: already called with rcu_read_lock */
static int br_handle_local_finish(struct sk_buff *skb)
{
	struct net_bridge_port *p = br_port_get_rcu(skb->dev);

	br_fdb_update(p->br, p, eth_hdr(skb)->h_source);
	return 0;	 /* process further */
}

/* Does address match the link local multicast address.
 * 01:80:c2:00:00:0X
 */
static inline int is_link_local(const unsigned char *dest)
{
	__be16 *a = (__be16 *)dest;
	static const __be16 *b = (const __be16 *)br_group_address;
	static const __be16 m = cpu_to_be16(0xfff0);

	return ((a[0] ^ b[0]) | (a[1] ^ b[1]) | ((a[2] ^ b[2]) & m)) == 0;
}

/*
 * Return NULL if skb is handled
 * note: already called with rcu_read_lock
 */
rx_handler_result_t br_handle_frame(struct sk_buff **pskb)
{
	struct net_bridge_port *p;
	struct sk_buff *skb = *pskb;
	const unsigned char *dest = eth_hdr(skb)->h_dest;
	br_should_route_hook_t *rhook;

	if (unlikely(skb->pkt_type == PACKET_LOOPBACK))
		return RX_HANDLER_PASS;

	if (!is_valid_ether_addr(eth_hdr(skb)->h_source))
		goto drop;

	skb = skb_share_check(skb, GFP_ATOMIC);
	if (!skb)
		return RX_HANDLER_CONSUMED;

	p = br_port_get_rcu(skb->dev);

	if (unlikely(is_link_local(dest))) {
		/*
		 * See IEEE 802.1D Table 7-10 Reserved addresses
		 *
		 * Assignment		 		Value
		 * Bridge Group Address		01-80-C2-00-00-00
		 * (MAC Control) 802.3		01-80-C2-00-00-01
		 * (Link Aggregation) 802.3	01-80-C2-00-00-02
		 * 802.1X PAE address		01-80-C2-00-00-03
		 *
		 * 802.1AB LLDP 		01-80-C2-00-00-0E
		 *
		 * Others reserved for future standardization
		 */
		switch (dest[5]) {
		case 0x00:	/* Bridge Group Address */
			/* If STP is turned off,
			   then must forward to keep loop detection */
			if (p->br->stp_enabled == BR_NO_STP)
				goto forward;
			break;

		case 0x01:	/* IEEE MAC (Pause) */
			goto drop;

		default:
			/* Allow selective forwarding for most other protocols */
			if (p->br->group_fwd_mask & (1u << dest[5]))
				goto forward;
		}

		/* Deliver packet to local host only */
		if (BR_HOOK(NFPROTO_BRIDGE, NF_BR_LOCAL_IN, skb, skb->dev,
			    NULL, br_handle_local_finish)) {
			return RX_HANDLER_CONSUMED; /* consumed by filter */
		} else {
			*pskb = skb;
			return RX_HANDLER_PASS;	/* continue processing */
		}
	}

forward:
	switch (p->state) {
	case BR_STATE_FORWARDING:
		rhook = rcu_dereference(br_should_route_hook);
		if (rhook) {
			if ((*rhook)(skb)) {
				*pskb = skb;
				return RX_HANDLER_PASS;
			}
			dest = eth_hdr(skb)->h_dest;
		}
		/* fall through */
	case BR_STATE_LEARNING:
		if (!compare_ether_addr(p->br->dev->dev_addr, dest))
			skb->pkt_type = PACKET_HOST;

		BR_HOOK(NFPROTO_BRIDGE, NF_BR_PRE_ROUTING, skb, skb->dev, NULL,
			br_handle_frame_finish);
		break;
	default:
drop:
		kfree_skb(skb);
	}
	return RX_HANDLER_CONSUMED;
}
