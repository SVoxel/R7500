EXTRA_CMD_ARGS="--nss"
KERNEL_MODULES=qca-nss-qdisc

# sets up the qdisc structures on an interface
# Note: this only sets a default rate on the root class, not necessarily the
#	correct rate; qdiscman handles this when it starts up
#
# $1: dev
setup_iface() {
	tc qdisc add dev $1 root \
		handle ${PRIO_HANDLE_MAJOR}: \
		nssprio bands 3
	[ $? = 0 ] || return $?
	# interactive for localhost OUTPUT
	add_interactive_qdisc $1 \
		"${PRIO_HANDLE_MAJOR}:2" \
		"${OUTPUT_HANDLE_MAJOR}:" "nsscodel"
	[ $? = 0 ] || return $?
	# base nsstbl under which all streamboost classes appear
	tc qdisc add dev $1 \
		parent ${PRIO_HANDLE_MAJOR}:1 \
		handle ${TBF_HANDLE_MAJOR}: \
		nsstbl rate 1875000 burst 10000 mtu 1514
	[ $? = 0 ] || return $?

	# schroot
	tc qdisc add dev $1 \
		parent ${TBF_HANDLE_MAJOR}: \
		handle ${SCHROOT_HANDLE_MAJOR}: \
		nssbf
	[ $? = 0 ] || return $?

	# classified
	tc class add dev $1 \
		parent ${SCHROOT_HANDLE_MAJOR}: \
		classid ${SCHROOT_HANDLE_MAJOR}:${CLASSID_CLASSIFIED} \
		nssbf rate 125000000 burst 125000000 mtu 1514
	[ $? = 0 ] || return $?
	tc qdisc add dev $1 \
		parent ${SCHROOT_HANDLE_MAJOR}:${CLASSID_CLASSIFIED} \
		handle ${BF_HANDLE_MAJOR}: \
		nssbf
	[ $? = 0 ] || return $?

	# background
	tc class add dev $1 \
		parent ${SCHROOT_HANDLE_MAJOR}: \
		classid ${SCHROOT_HANDLE_MAJOR}:${CLASSID_BACKGROUND} \
		nssbf rate 125000000 burst 125000000 mtu 1514
	[ $? = 0 ] || return $?
	tc qdisc add dev $1 \
		parent ${SCHROOT_HANDLE_MAJOR}:${CLASSID_BACKGROUND} \
		handle ${BGROOT_HANDLE_MAJOR}: \
		nssbf
	[ $? = 0 ] || return $?

	# default for unclassified flows
	tc class add dev $1 \
		parent ${SCHROOT_HANDLE_MAJOR}:${CLASSID_DEFAULT} \
		classid ${CLASSID_DEFAULT} \
		nssbf rate 125000000 burst 125000000 mtu 1514
	[ $? = 0 ] || return $?
	add_interactive_qdisc $1 \
		"${SCHROOT_HANDLE_MAJOR}:${CLASSID_DEFAULT}" \
		"${CLASSID_DEFAULT}:" "nsscodel" "set_default" # TODO: we should not need this
	[ $? = 0 ] || return $?
	# localhost class for traffic originating from the router
	tc class add dev $1 \
		parent ${SCHROOT_HANDLE_MAJOR}: \
		classid ${SCHROOT_HANDLE_MAJOR}:${CLASSID_LOCALHOST} \
		nssbf rate 125000000 burst 125000000 mtu 1514
	[ $? = 0 ] || return $?
	add_interactive_qdisc $1 \
		"${SCHROOT_HANDLE_MAJOR}:${CLASSID_LOCALHOST}" \
		"${CLASSID_LOCALHOST}:" "nsscodel"
	[ $? = 0 ] || return $?

	# "cheat" is for things like ICMP acceleration
	tc class add dev $1 \
		parent ${SCHROOT_HANDLE_MAJOR}: \
		classid ${SCHROOT_HANDLE_MAJOR}:${CLASSID_ELEVATED_CHEAT} \
		nssbf rate 125000000 burst 125000000 mtu 1514
	[ $? = 0 ] || return $?
	add_interactive_qdisc $1 \
		"${SCHROOT_HANDLE_MAJOR}:${CLASSID_ELEVATED_CHEAT}" \
		"${CLASSID_ELEVATED_CHEAT}:" "nsscodel"
	[ $? = 0 ] || return $?
	# browser
	tc class add dev $1 \
		parent ${SCHROOT_HANDLE_MAJOR}: \
		classid ${SCHROOT_HANDLE_MAJOR}:${CLASSID_ELEVATED_BROWSER} \
		nssbf rate 125000000 burst 125000000 mtu 1514
	[ $? = 0 ] || return $?
	add_interactive_qdisc $1 \
		"${SCHROOT_HANDLE_MAJOR}:${CLASSID_ELEVATED_BROWSER}" \
		"${CLASSID_ELEVATED_BROWSER}:" "nsscodel"
	[ $? = 0 ] || return $?
	# dns
	tc class add dev $1 \
		parent ${SCHROOT_HANDLE_MAJOR}: \
		classid ${SCHROOT_HANDLE_MAJOR}:${CLASSID_ELEVATED_DNS} \
		nssbf rate 125000000 burst 125000000
	[ $? = 0 ] || return $?
	add_interactive_qdisc $1 \
		"${SCHROOT_HANDLE_MAJOR}:${CLASSID_ELEVATED_DNS}" \
		"${CLASSID_ELEVATED_DNS}:" "nsscodel"
	[ $? = 0 ] || return $?
}

generic_iptables() {
	# All packets from localhost to LAN are marked as prio 2
	iptables -t mangle -$1 OUTPUT -o $LAN_IFACE -j CLASSIFY \
		--set-class ${PRIO_HANDLE_MAJOR}:2

	# If this is from localhost AND is using the aperture source
	# ports, set the class to avoid BWC.
	iptables -t mangle -$1 OUTPUT ! -o $LAN_IFACE -p tcp -m multiport \
		--source-ports 321:353 -j CLASSIFY \
		--set-class ${PRIO_HANDLE_MAJOR}:2
	iptables -t mangle -$1 OUTPUT ! -o $LAN_IFACE -p tcp -m multiport \
		--source-ports 321:353 -j RETURN

	# All packets from localhost to WAN are marked
	# Note the !LAN_IFACE logic allows us to catch any potential
	# PPPoE interface as well
	iptables -t mangle -$1 OUTPUT ! -o $LAN_IFACE -j CLASSIFY \
		--set-class ${CLASSID_LOCALHOST}:0

	# For the LAN side, we set the default to be the parent of the
	# HTB, so that when ct_mark is copied to nf_mark, by
	# CONNMARK --restore mark, priority will be unset, and filter fw
	# will read the mark and set the class correctly.  In the WAN
	# direction, the root is the HTB, so we do not need to set the
	# class; it will just work.
	iptables -t mangle -$1 FORWARD ! -o $WAN_IFACE -j CLASSIFY \
		--set-class ${CLASSID_DEFAULT}:0
	iptables -t mangle -$1 FORWARD ! -o $LAN_IFACE -j CLASSIFY \
		--set-class ${CLASSID_DEFAULT}:0

	# Forwarded ICMP packets in their own queue
	iptables -t mangle -$1 FORWARD -p icmp -m limit --limit 2/second \
		-j CLASSIFY \
		--set-class ${CLASSID_ELEVATED_CHEAT}:0

	# DNS Elevation
	iptables -t mangle -$1 POSTROUTING -p udp --dport 53 \
		-j CLASSIFY --set-class ${CLASSID_ELEVATED_DNS}:0
	iptables -t mangle -$1 POSTROUTING -p udp --dport 53 \
		-j RETURN

	# Restore the CONNMARK to the packet
	iptables -t mangle -$1 POSTROUTING -j CONNMARK --restore-mark
	# Further restore the mark to priority since filters don't work
	iptables -t mangle -$1 POSTROUTING -j mark2prio
}

setup_iptables () {
	# call iptables to add rules
	generic_iptables A
}

teardown_iptables () {
	# call iptables to delete rules
	generic_iptables D
}
