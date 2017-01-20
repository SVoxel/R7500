/*
 *	Forwarding decision
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

#include <linux/err.h>
#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/netdevice.h>
#include <linux/netpoll.h>
#include <linux/skbuff.h>
#include <linux/if_vlan.h>
#include <linux/netfilter_bridge.h>
#include <linux/if_arp.h>
#include "br_private.h"

#ifdef CONFIG_DNI_MCAST_TO_UNICAST
#include <linux/ip.h>
static inline struct __mac_cache *
find_mac_cache(unsigned long sip, unsigned long gip)
{
      int i;
      for (i = 0; i < MCAST_ENTRY_SIZE; i++)
      {
              if (mac_cache[i].valid)
              {
                      if (mac_cache[i].sip == sip)
                              return &mac_cache[i];
              }
      }
      return NULL;
}

static inline struct __mgroup_mbr_list *
mbr_find(struct __mgroup_list *mg, unsigned long sip)
{
      struct __mgroup_mbr_list *ptr = mg->member;

      while (ptr)
      {
              if (ptr->sip == sip)
                      break;
              ptr = ptr->next;
      }

      return ptr;
}

static inline struct __mgroup_mbr_list *
mbr_add(struct __mac_cache *cache)
{
      struct __mgroup_mbr_list *ptr = NULL;

      ptr = kmalloc( sizeof(struct __mgroup_mbr_list), GFP_ATOMIC);
      if (likely(ptr))
      {
              memset(ptr, 0, sizeof(struct __mgroup_mbr_list));
              memcpy(ptr->mac, cache->mac, 6);
              ptr->sip = cache->sip;
              ptr->dev = cache->dev;
              ptr->next = NULL;
      }
      return ptr;
}

static inline void
mbr_del(struct __mgroup_list *mg, unsigned long sip)
{
      struct __mgroup_mbr_list *ptr = mg->member, *preptr = NULL;

      while (ptr)
      {
              if (ptr->sip == sip)
                      break;
              preptr = ptr;
              ptr = ptr->next;
      }
      if (preptr)
              preptr->next = ptr->next;
      else if (ptr->next)
              mg->member = ptr->next;
      else
              mg->member = NULL;

      kfree(ptr);
      return;
}

static inline struct __mgroup_list *
mgroup_find(unsigned long gip)
{
      struct __mgroup_list *ptr = mhead;
      while (ptr)
      {
              if (ptr->gip == gip)
                      break;
              ptr = ptr->next;
      }

      return ptr;
}

static inline struct __mgroup_list *
mgroup_add(unsigned long sip, unsigned long gip)
{
      struct __mgroup_list *ptr = mhead, *tmp;
      struct __mac_cache *cache = NULL;
      struct __mgroup_mbr_list *msource = NULL;

      while (ptr)
              ptr = ptr->next;
      cache = find_mac_cache(sip, gip);
      if (cache)
              ptr = kmalloc( sizeof(struct __mgroup_list), GFP_ATOMIC);

      if (likely(ptr))
      {
              ptr->gip = gip;
              msource = mbr_add(cache);
              if (unlikely(!msource))
              {
                      kfree(ptr);
                      return NULL;
              }
              ptr->member = msource;
              ptr->next = NULL;

              if (mhead)
                      INSERT_TO_TAIL(mhead, ptr, tmp);
              else
                      mhead = ptr;
              cache->valid = 0;
      }
      else
              ptr = NULL;
      return ptr;
}

static inline void
mgroup_del(unsigned long gip)
{
      struct __mgroup_list *ptr = mhead, *preptr = NULL;

      while (ptr)
      {
              if (ptr->gip == gip)
                      break;
              preptr = ptr;
              ptr = ptr->next;
      }
      if (preptr)
              preptr->next = ptr->next;
      else if (ptr->next)
              mhead = ptr->next;
      else
              mhead = NULL;

      kfree(ptr);
      return ;
}

void
proc_mcast_entry(char cmd, unsigned long sip , unsigned long gip)
{
      struct __mgroup_list *ptr = NULL;
      struct __mgroup_mbr_list *mptr = NULL, *tmp;
      struct __mac_cache * cache = NULL;
      if (cmd == 'a')
      {
              ptr = mgroup_find(gip);
              if (ptr)
              {
                      mptr = mbr_find(ptr, sip);
                      if (!mptr)
                      {
                              cache = find_mac_cache(sip, gip);
                              if (cache)
                              {
                                      mptr = mbr_add(cache);
                                      if (mptr)
                                              INSERT_TO_TAIL(ptr->member, mptr, tmp);
                              }
                      }
              }
              else
                      ptr = mgroup_add(sip, gip);
      }
      else
      {
              ptr = mgroup_find(gip);
              if (ptr)
              {
                      mptr = mbr_find(ptr, sip);
                      if (mptr)
                      {
                              mbr_del(ptr, sip);
                              if (!ptr->member)
                                      mgroup_del(gip);
                      }
              }
      }
      return;
}

#define LOCAL_CONTROL_START 0xE0000000
#define LOCAL_CONTROL_END   0XE00000FF
#define SSDP    0XEFFFFFFA

static inline int
not_ctrl_group(unsigned long ip)
{
      if ( (ip >= LOCAL_CONTROL_START) &&( ip <= LOCAL_CONTROL_END))
              return 0;
      else if (ip == SSDP)
              return 0;
      return 1;
}

static inline struct sk_buff *
modifyMcast2Ucast(struct sk_buff *skb,unsigned char *mac)
{
      struct sk_buff *mSkb;

      mSkb=skb_copy(skb,GFP_ATOMIC);
      struct ethhdr *ethernet = (struct ethhdr *)mSkb->mac_header;
      //mSkb=skb_copy(skb,GFP_ATOMIC);
      memcpy(ethernet->h_dest, mac, 6);

      return mSkb;
}

int mcast_set_read( char *page, char **start, off_t off,
                                int count, int *eof, void *data )
{
      struct __mgroup_list *ptr = mhead;
      struct __mgroup_mbr_list *mptr;
      int i;
      while (ptr)
      {
              mptr = ptr->member;
              while (mptr)
              {
                      printk("client : [%8x] join group [%8x]\n", mptr->sip, ptr->gip);
                      printk("mode : %s\n", (mptr->set.mode)?"Include":"Exclude");
                      for (i = 0; i < mptr->set.num; i++)
                              printk("source%d : %8x\n", i, mptr->set.sip[i]);
                      mptr = mptr->next;
              }
              ptr = ptr->next;
      }

      return 0;
}

ssize_t mcast_set_write( struct file *filp, const char __user *buff,
                                         unsigned long len, void *data )
{
      char line[256], *p, *e, i;
      struct __mgroup_list *ptr = NULL;
      struct __mgroup_mbr_list *mptr = NULL;
      unsigned long sip, gip;
      struct source_set set;
      if (copy_from_user( line, buff, len ))
              return -EFAULT;
      line[len] = 0;
      // ip gip mode num source1 source2
      p = line;
      e = strsep(&p, " ");
      if (!e)
              return len;
      sip = a2n(e);

      e = strsep(&p, " ");
      if (!e)
              return len;
      gip = a2n(e);

      memset(&set, 0, sizeof(set));
      e = strsep(&p, " ");
      if (!e)
              return len;
      set.mode = *e - '0';

      e = strsep(&p, " ");
      if (!e)
              return len;
      set.num = *e - '0';

      if (set.num > MAX_SOURCE_SIZE)
                return -EINVAL;

      for (i = 0; i < set.num; i++)
      {
              e = strsep(&p, " ");
              if (!e)
                      return len;
              set.sip[i] = a2n(e);
      }

      ptr = mgroup_find(gip);
      if (ptr)
      {
              mptr = mbr_find(ptr, sip);
              if (mptr)
                      memcpy(&mptr->set, &set, sizeof(set));

      }
      return len;
}

static inline int
include_check(unsigned long sip, struct source_set *set)
{
      int i;
      for (i = 0; i < set->num; i++)
              if (sip == set->sip[i])
                      return 1;
      return 0;
}

static inline int
exclude_check(unsigned long sip, struct source_set *set)
{
      int i;
      for (i = 0; i < set->num; i++)
              if (sip == set->sip[i])
                      return 0;
      return 1;
}

static inline int
pass_check(unsigned long sip, struct __mgroup_mbr_list *mbr)
{
      if (mbr->set.mode)
              return include_check(sip, &mbr->set);
      else
              return exclude_check(sip, &mbr->set);
}

#ifdef CONFIG_DNI_IPTV_MCAST_TO_UNICAST

/* IGMPv1/IGMPv2 report format */
typedef struct _igmpr_t {
  u_char          igmpr_type;                  /* version & type */
  u_char          igmpr_code;                  /* unused */
  u_short         igmpr_cksum;                 /* IP-style checksum */
  struct in_addr  igmpr_group;                 /* group address being reported */
} igmpr_t;

/* IGMPv3 group record format */
typedef struct _igmp_grouprec_t {
  u_char          igmpg_type;          /* record type */
  u_char          igmpg_datalen;       /* amount of aux data */
  u_short         igmpg_numsrc;                /* number of sources */
  struct in_addr  igmpg_group;         /* the group being reported */
  struct in_addr  igmpg_sources[0];    /* source addresses */
} igmp_grouprec_t;

/* IGMPv3 report format */
typedef struct _igmp_report_t {
  u_char          igmpr_type;          /* version & type of IGMP message */
  u_char          igmpr_code;          /* subtype for routing msgs */
  u_short         igmpr_cksum;                 /* IP-style checksum */
  u_short         igmpr_rsv;           /* reserved */
  u_short         igmpr_numgrps;        /* number of groups*/
  igmp_grouprec_t igmpr_group[0];      /* group records */
} igmp_report_t;

#define IGMP_V1_MEMBERSHIP_REPORT   0x12
#define IGMP_V2_MEMBERSHIP_REPORT   0x16
#define IGMP_V2_MEMBERSHIP_LEAVE    0x17
#define IGMP_V3_MEMBERSHIP_REPORT   0x22
static void iptv_port_update_mgroup(const struct sk_buff *skb)
{
       struct ethhdr *ehdr;
       struct iphdr *igmp_iph;
       unsigned char *igmp_type;
       igmpr_t *igmpv12_report;
       igmp_report_t *igmpv3_report;
       unsigned short igmpv3_numsrc, igmpv3_numgrps;
       unsigned char igmpv3_type;
       uint32_t sip, gip;
       int i;

       ehdr = (struct ethhdr *)skb->mac_header;
       igmp_iph = (struct iphdr *)skb->network_header;

       if (MULTICAST_MAC(ehdr->h_dest) && igmp_iph->protocol == IPPROTO_IGMP && igmp_iph->daddr != SSDP)
       {
               igmp_type = (unsigned char *)igmp_iph + (igmp_iph->ihl << 2);
               sip = igmp_iph->saddr;

               switch(*igmp_type)
               {
                       case IGMP_V1_MEMBERSHIP_REPORT:
                       case IGMP_V2_MEMBERSHIP_REPORT:
                               igmpv12_report = (igmpr_t *)igmp_type;
                               gip = *((uint32_t *)&igmpv12_report->igmpr_group);
                               proc_mcast_entry('a', sip, gip);
                               printk("IPTV client : [%8x] join group [%8x]\n",sip,gip);
                               break;
                       case IGMP_V2_MEMBERSHIP_LEAVE:
                               igmpv12_report = (igmpr_t *)igmp_type;
                               gip = *((uint32_t *)&igmpv12_report->igmpr_group);
                               printk("IPTV client : [%8x] leave group [%8x]\n",sip,gip);
                               proc_mcast_entry('d', sip, gip);
                               break;
                       case IGMP_V3_MEMBERSHIP_REPORT:
                               igmpv3_report = (igmp_report_t *)igmp_type;
                               igmpv3_numgrps = ntohs(igmpv3_report->igmpr_numgrps);
                               for(i=0; i<igmpv3_numgrps; i++)
                               {
                                       igmpv3_type   = (unsigned char)igmpv3_report->igmpr_group[i].igmpg_type;
                                       switch(igmpv3_type)
                                       {
                                               case 1:
                                               case 2:
                                               case 4:
                                               case 5:
                                               case 6:
                                                       gip = *((uint32_t *)&igmpv3_report->igmpr_group[i].igmpg_group);
                                                       proc_mcast_entry('a', sip, gip);
                                                       printk("IPTV client : [%8x] join group [%8x]\n",sip,gip);
                                                       break;
                                               case 3:
                                                       igmpv3_numsrc = ntohs(igmpv3_report->igmpr_group[i].igmpg_numsrc);
                                                       gip = *((uint32_t *)&igmpv3_report->igmpr_group[i].igmpg_group);
                                                       if (igmpv3_numsrc == 0) {
                                                           proc_mcast_entry('d', sip, gip);
                                                           printk("IPTV client : [%8x] leave group [%8x]\n",sip,gip);
                                                       } else {
                                                           proc_mcast_entry('a', sip, gip);
                                                           printk("IPTV client : [%8x] join group [%8x]\n",sip,gip);
                                                       }
                                                       break;
                                               default:
                                                       break;

                                       }
                               }
                       default:
                               break;
               }
       }

}
#endif

#endif

static int deliver_clone(const struct net_bridge_port *prev,
			 struct sk_buff *skb,
			 void (*__packet_hook)(const struct net_bridge_port *p,
					       struct sk_buff *skb));

/* Don't forward packets to originating port or forwarding diasabled */
static inline int should_deliver(const struct net_bridge_port *p,
				 const struct sk_buff *skb)
{
	const unsigned char *dest = eth_hdr(skb)->h_dest;
	return (((skb->dev != p->dev) || ((p->flags & BR_HAIRPIN_MODE) && (!is_multicast_ether_addr(dest)))) &&
		p->state == BR_STATE_FORWARDING);
}

static inline unsigned packet_length(const struct sk_buff *skb)
{
	return skb->len - (skb->protocol == htons(ETH_P_8021Q) ? VLAN_HLEN : 0);
}

int br_dev_queue_push_xmit(struct sk_buff *skb)
{
	/* ip_fragment doesn't copy the MAC header */
	if (nf_bridge_maybe_copy_header(skb) ||
	    (packet_length(skb) > skb->dev->mtu && !skb_is_gso(skb))) {
		kfree_skb(skb);
	} else {
		skb_push(skb, ETH_HLEN);
		br_drop_fake_rtable(skb);
		dev_queue_xmit(skb);
	}

	return 0;
}

int br_forward_finish(struct sk_buff *skb)
{
	return BR_HOOK(NFPROTO_BRIDGE, NF_BR_POST_ROUTING, skb, NULL, skb->dev,
		       br_dev_queue_push_xmit);

}

extern int brnf_filter_qtn __read_mostly;
static void __br_deliver(const struct net_bridge_port *to, struct sk_buff *skb)
{
	skb->dev = to->dev;

#ifdef CONFIG_BRIDGE_NETGEAR_ACL
       struct net_bridge *br = to->br;
       if (!br_acl_should_pass(br, skb, ACL_CHECK_DST)) {
               br->dev->stats.tx_dropped++;
               kfree_skb(skb);
               return;
       }
#endif
	struct iphdr *ip_h;
	struct ethhdr *eth_h;
	struct arp_payload *arp_p;

	eth_h = (struct ethhdr *)skb->mac_header;
	ip_h = (struct iphdr *)(skb->mac_header + sizeof(struct ethhdr));
	arp_p = (struct arp_payload *) (skb->mac_header + sizeof(struct ethhdr) + sizeof(struct arphdr));
	if (brnf_filter_qtn && eth_h->h_proto == __constant_htons(ETH_P_IP) && (ip_h->saddr == QTN_HOST_IP || ip_h->saddr == QTN_HOST_BRIDGE_IP) && to->dev->name[0] != 'h') {
		kfree_skb(skb);
		return;
	}
	if (brnf_filter_qtn && eth_h->h_proto == __constant_htons(ETH_P_ARP) && (arp_p->src_ip == QTN_HOST_IP || arp_p->src_ip == QTN_HOST_BRIDGE_IP) && to->dev->name[0] != 'h') {
		kfree_skb(skb);
		return;
	}

	if (unlikely(netpoll_tx_running(to->dev))) {
		if (packet_length(skb) > skb->dev->mtu && !skb_is_gso(skb))
			kfree_skb(skb);
		else {
			skb_push(skb, ETH_HLEN);
			br_netpoll_send_skb(to, skb);
		}
		return;
	}

	BR_HOOK(NFPROTO_BRIDGE, NF_BR_LOCAL_OUT, skb, NULL, skb->dev,
		br_forward_finish);
}

static void __br_forward(const struct net_bridge_port *to, struct sk_buff *skb)
{
	struct net_device *indev;

	if (skb_warn_if_lro(skb)) {
		kfree_skb(skb);
		return;
	}

	indev = skb->dev;
	skb->dev = to->dev;
	skb_forward_csum(skb);
#ifdef CONFIG_BRIDGE_NETGEAR_ACL
       struct net_bridge *br = to->br;
       if (!br_acl_should_pass(br, skb, (ACL_CHECK_SRC | ACL_CHECK_DST))) {
               br->dev->stats.tx_dropped++;
               kfree_skb(skb);
               return;
       }
#endif

	BR_HOOK(NFPROTO_BRIDGE, NF_BR_FORWARD, skb, indev, skb->dev,
		br_forward_finish);
}

/* called with rcu_read_lock */
void br_deliver(const struct net_bridge_port *to, struct sk_buff *skb)
{
	if (to && should_deliver(to, skb)) {
		__br_deliver(to, skb);
		return;
	}

	kfree_skb(skb);
}

/* called with rcu_read_lock */
void br_forward(const struct net_bridge_port *to, struct sk_buff *skb, struct sk_buff *skb0)
{
	if (should_deliver(to, skb) && !(to->flags & BR_ISOLATE_MODE)) {
		if (skb0)
			deliver_clone(to, skb, __br_forward);
		else
			__br_forward(to, skb);
		return;
	}

	if (!skb0)
		kfree_skb(skb);
}

static int deliver_clone(const struct net_bridge_port *prev,
			 struct sk_buff *skb,
			 void (*__packet_hook)(const struct net_bridge_port *p,
					       struct sk_buff *skb))
{
	struct net_device *dev = BR_INPUT_SKB_CB(skb)->brdev;

	skb = skb_clone(skb, GFP_ATOMIC);
	if (!skb) {
		dev->stats.tx_dropped++;
		return -ENOMEM;
	}

	__packet_hook(prev, skb);
	return 0;
}

static struct net_bridge_port *maybe_deliver(
	struct net_bridge_port *prev, struct net_bridge_port *p,
	struct sk_buff *skb,
	void (*__packet_hook)(const struct net_bridge_port *p,
			      struct sk_buff *skb))
{
	int err;

	if (!should_deliver(p, skb))
		return prev;

	if (!prev)
		goto out;

	err = deliver_clone(prev, skb, __packet_hook);
	if (err)
		return ERR_PTR(err);

out:
	return p;
}

/* called under bridge lock */
static void br_flood(struct net_bridge *br, struct sk_buff *skb,
		     struct sk_buff *skb0,
		     void (*__packet_hook)(const struct net_bridge_port *p,
					   struct sk_buff *skb),
		     bool forward)
{
	struct net_bridge_port *p;
	struct net_bridge_port *prev;

	prev = NULL;

	list_for_each_entry_rcu(p, &br->port_list, list) {
		if (forward && (p->flags & BR_ISOLATE_MODE))
			continue;

		prev = maybe_deliver(prev, p, skb, __packet_hook);
		if (IS_ERR(prev))
			goto out;
	}

	if (!prev)
		goto out;

	if (skb0)
		deliver_clone(prev, skb, __packet_hook);
	else
		__packet_hook(prev, skb);
	return;

out:
	if (!skb0)
		kfree_skb(skb);
}


/* called with rcu_read_lock */
void br_flood_deliver(struct net_bridge *br, struct sk_buff *skb)
{
	br_flood(br, skb, NULL, __br_deliver, false);
}

/* called under bridge lock */
void br_flood_forward(struct net_bridge *br, struct sk_buff *skb,
		      struct sk_buff *skb2)
{
	br_flood(br, skb, skb2, __br_forward, true);
}

#ifdef CONFIG_BRIDGE_IGMP_SNOOPING
/* called with rcu_read_lock */
static void br_multicast_flood(struct net_bridge_mdb_entry *mdst,
			       struct sk_buff *skb, struct sk_buff *skb0,
			       void (*__packet_hook)(
					const struct net_bridge_port *p,
					struct sk_buff *skb))
{
	struct net_device *dev = BR_INPUT_SKB_CB(skb)->brdev;
	struct net_bridge *br = netdev_priv(dev);
	struct net_bridge_port *prev = NULL;
	struct net_bridge_port_group *p;
	struct hlist_node *rp;

	rp = rcu_dereference(hlist_first_rcu(&br->router_list));
	p = mdst ? rcu_dereference(mdst->ports) : NULL;
	while (p || rp) {
		struct net_bridge_port *port, *lport, *rport;

		lport = p ? p->port : NULL;
		rport = rp ? hlist_entry(rp, struct net_bridge_port, rlist) :
			     NULL;

		port = (unsigned long)lport > (unsigned long)rport ?
		       lport : rport;

		prev = maybe_deliver(prev, port, skb, __packet_hook);
		if (IS_ERR(prev))
			goto out;

		if ((unsigned long)lport >= (unsigned long)port)
			p = rcu_dereference(p->next);
		if ((unsigned long)rport >= (unsigned long)port)
			rp = rcu_dereference(hlist_next_rcu(rp));
	}

	if (!prev)
		goto out;

	if (skb0)
		deliver_clone(prev, skb, __packet_hook);
	else
		__packet_hook(prev, skb);
	return;

out:
	if (!skb0)
		kfree_skb(skb);
}

/* called with rcu_read_lock */
void br_multicast_deliver(struct net_bridge_mdb_entry *mdst,
			  struct sk_buff *skb)
{
	br_multicast_flood(mdst, skb, NULL, __br_deliver);
}

/* called with rcu_read_lock */
void br_multicast_forward(struct net_bridge_mdb_entry *mdst,
			  struct sk_buff *skb, struct sk_buff *skb2)
{
	br_multicast_flood(mdst, skb, skb2, __br_forward);
}
#endif

EXPORT_SYMBOL_GPL(br_deliver);
EXPORT_SYMBOL_GPL(br_forward);
