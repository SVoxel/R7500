#ifndef _PPP_CHANNEL_H_
#define _PPP_CHANNEL_H_
/*
 * Definitions for the interface between the generic PPP code
 * and a PPP channel.
 *
 * A PPP channel provides a way for the generic PPP code to send
 * and receive packets over some sort of communications medium.
 * Packets are stored in sk_buffs and have the 2-byte PPP protocol
 * number at the start, but not the address and control bytes.
 *
 * Copyright 1999 Paul Mackerras.
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; either version
 *  2 of the License, or (at your option) any later version.
 *
 * ==FILEVERSION 20000322==
 */

#include <linux/list.h>
#include <linux/skbuff.h>
#include <linux/poll.h>
#include <net/net_namespace.h>

typedef void (*ppp_channel_destroy_method_t)(void *, uint16_t, uint8_t *);

struct ppp_channel;

struct ppp_channel_ops {
	/* Send a packet (or multilink fragment) on this channel.
	   Returns 1 if it was accepted, 0 if not. */
	int	(*start_xmit)(struct ppp_channel *, struct sk_buff *);
	/* Handle an ioctl call that has come in via /dev/ppp. */
	int	(*ioctl)(struct ppp_channel *, unsigned int, unsigned long);
	/* Return the session ID of the connection which is cretaed on the channel. */
	__be16	(*get_session_id)(struct ppp_channel *);
	/* Return the net_device of the given channel. */
	struct net_device* (*get_netdev)(struct ppp_channel *);
	/* Return the remote MAC address of the connection which is created on the channel. */
	unsigned char* (*get_remote_mac)(struct ppp_channel *);
	/* Register destroy function into PPP channels */
	bool (*reg_destroy_method)(struct ppp_channel *, ppp_channel_destroy_method_t method, void *);
	/* Unregister destroy function from PPP channels */
	void (*unreg_destroy_method)(struct ppp_channel *);
};

struct ppp_channel {
	void		*private;	/* channel private data */
	const struct ppp_channel_ops *ops; /* operations for this channel */
	int		mtu;		/* max transmit packet size */
	int		hdrlen;		/* amount of headroom channel needs */
	void		*ppp;		/* opaque to channel */
	int		speed;		/* transfer rate (bytes/second) */
	/* the following is not used at present */
	int		latency;	/* overhead time in milliseconds */
};

#ifdef __KERNEL__
/* Called by upper layers to get the Ethernet (channel) net_device corresponding to
   the given PPP net_device */
extern struct net_device *ppp_get_eth_netdev(struct net_device *);

/* Called by upper layers to get the channel session ID corresponding to
   the given PPP net_device */
extern __be16 ppp_get_session_id(struct net_device *);

/* Called by upper layers to get the channel remote MAC corresponding to
   the given PPP net_device */
extern unsigned char *ppp_get_remote_mac(struct net_device *);

/* Called by upper layers to get the PPP net_device corresponding to
   the given Ethernet net_device */
extern struct net_device *ppp_get_ppp_netdev(struct net_device *);

/* Get the ppp net_device associated with a particular session ID (sid) */
extern struct net_device *ppp_session_to_netdev(uint16_t session_id, uint8_t *remote_mac);

/* Register destroy function into PPP channels */
extern bool ppp_register_destroy_method(struct net_device *dev, ppp_channel_destroy_method_t method, void *arg);

/* Unregister destroy function from PPP channels */
extern bool ppp_unregister_destroy_method(struct net_device *dev);

/* Update statistics of the PPP net_device by incrementing related
   statistics field value with corresponding parameter */
extern void ppp_update_stats(struct net_device *dev, unsigned long rx_packets,
		unsigned long rx_bytes, unsigned long tx_packets, unsigned long tx_bytes);

/* Called by the channel when it can send some more data. */
extern void ppp_output_wakeup(struct ppp_channel *);

/* Called by the channel to process a received PPP packet.
   The packet should have just the 2-byte PPP protocol header. */
extern void ppp_input(struct ppp_channel *, struct sk_buff *);

/* Called by the channel when an input error occurs, indicating
   that we may have missed a packet. */
extern void ppp_input_error(struct ppp_channel *, int code);

/* Attach a channel to a given PPP unit in specified net. */
extern int ppp_register_net_channel(struct net *, struct ppp_channel *);

/* Attach a channel to a given PPP unit. */
extern int ppp_register_channel(struct ppp_channel *);

/* Detach a channel from its PPP unit (e.g. on hangup). */
extern void ppp_unregister_channel(struct ppp_channel *);

/* Get the channel number for a channel */
extern int ppp_channel_index(struct ppp_channel *);

/* Get the unit number associated with a channel, or -1 if none */
extern int ppp_unit_number(struct ppp_channel *);

/* Get the device name associated with a channel, or NULL if none */
extern char *ppp_dev_name(struct ppp_channel *);

/*
 * SMP locking notes:
 * The channel code must ensure that when it calls ppp_unregister_channel,
 * nothing is executing in any of the procedures above, for that
 * channel.  The generic layer will ensure that nothing is executing
 * in the start_xmit and ioctl routines for the channel by the time
 * that ppp_unregister_channel returns.
 */

#endif /* __KERNEL__ */
#endif
