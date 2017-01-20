KERNEL_MODULES="sch_sfq sch_prio sch_codel sch_fq_codel sch_hfsc cls_fw"

# sets up the qdisc structures on an interface
# Note: this only sets a default rate on the root class, not necessarily the
#	correct rate; qdiscman handles this when it starts up
#
# $1: dev
setup_iface() {
	# ####################################################################
	# configure the root prio
	# ####################################################################
	tc qdisc add dev $1 root \
		handle ${PRIO_HANDLE_MAJOR}: \
		prio bands 3 priomap 0 1 2 1 1 1 1 1 1 1 1 1 1 1 1 1
	[ $? = 0 ] || return $?
	# interactive for localhost OUTPUT
	add_interactive_qdisc $1 \
		"${PRIO_HANDLE_MAJOR}:2" \
		"${OUTPUT_HANDLE_MAJOR}:"
	[ $? = 0 ] || return $?
	# base hfsc under which all streamboost classes appear
	tc qdisc add dev $1 \
		parent ${PRIO_HANDLE_MAJOR}:1 \
		handle ${BF_HANDLE_MAJOR}: \
		hfsc default ${CLASSID_DEFAULT}
	[ $? = 0 ] || return $?

	# ###################################################################
	# configure the base hfsc
	# ###################################################################

	#
	# the main hfsc class is where adjusted global bandwidth is enforced
	#
	tc class add dev $1 \
		parent ${BF_HANDLE_MAJOR}:0 \
		classid ${BF_HANDLE_MAJOR}:${CLASSID_ROOT} \
		hfsc ls m1 0 d 0 m2 ${COMMITTED_WEIGHT} ul m1 0 d 0 m2 1875000
	[ $? = 0 ] || return $?

	#
	# default classifier for the main hfsc classifies on fwmark
	#
	tc filter add dev $1 parent ${BF_HANDLE_MAJOR}: fw
	[ $? = 0 ] || return $?

	#
	# classified
	#
	tc class add dev $1 \
		parent ${BF_HANDLE_MAJOR}:${CLASSID_ROOT} \
		classid ${BF_HANDLE_MAJOR}:${CLASSID_CLASSIFIED} \
		hfsc ls m1 0 d 0 m2 ${COMMITTED_WEIGHT}
	[ $? = 0 ] || return $?

	#
	# prioritized
	#
	tc class add dev $1 \
		parent ${BF_HANDLE_MAJOR}:${CLASSID_ROOT} \
		classid ${BF_HANDLE_MAJOR}:${CLASSID_PRIORITIZED} \
		hfsc ls m1 0 d 0 m2 ${PRIORITIZED_WEIGHT}
	[ $? = 0 ] || return $?

	#
	# background
	#
	tc class add dev $1 \
		parent ${BF_HANDLE_MAJOR}:${CLASSID_ROOT} \
		classid ${BF_HANDLE_MAJOR}:${CLASSID_BACKGROUND} \
		hfsc ls m1 0 d 0 m2 ${BACKGROUND_WEIGHT}
	[ $? = 0 ] || return $?
	# default for unclassified flows
	tc class add dev $1 \
		parent ${BF_HANDLE_MAJOR}:${CLASSID_BACKGROUND} \
		classid ${BF_HANDLE_MAJOR}:${CLASSID_DEFAULT} \
		hfsc ls m1 0 d 0 m2 ${INTERACTIVE_WEIGHT}
	[ $? = 0 ] || return $?
	add_interactive_qdisc $1 \
		"${BF_HANDLE_MAJOR}:${CLASSID_DEFAULT}" \
		"${CLASSID_DEFAULT}:"
	[ $? = 0 ] || return $?
	# localhost class for traffic originating from the router
	tc class add dev $1 \
		parent ${BF_HANDLE_MAJOR}:${CLASSID_BACKGROUND} \
		classid ${BF_HANDLE_MAJOR}:${CLASSID_LOCALHOST} \
		hfsc ls m1 0 d 0 m2 ${BACKGROUND_WEIGHT}
	[ $? = 0 ] || return $?
	add_interactive_qdisc $1 \
		"${BF_HANDLE_MAJOR}:${CLASSID_LOCALHOST}" \
		"${CLASSID_LOCALHOST}:"
	[ $? = 0 ] || return $?

	#
	# elevated
	#
	tc class add dev $1 \
		parent ${BF_HANDLE_MAJOR}:${CLASSID_ROOT} \
		classid ${BF_HANDLE_MAJOR}:${CLASSID_ELEVATED} \
		hfsc ls m1 0 d 0 m2 ${ELEVATED_WEIGHT}
	[ $? = 0 ] || return $?
	# "cheat" is for things like ICMP acceleration
	tc class add dev $1 \
		parent ${BF_HANDLE_MAJOR}:${CLASSID_ELEVATED} \
		classid ${BF_HANDLE_MAJOR}:${CLASSID_ELEVATED_CHEAT} \
		hfsc ls m1 0 d 0 m2 ${ELEVATED_WEIGHT}
	[ $? = 0 ] || return $?
	add_interactive_qdisc $1 \
		"${BF_HANDLE_MAJOR}:${CLASSID_ELEVATED_CHEAT}" \
		"${CLASSID_ELEVATED_CHEAT}:"
	[ $? = 0 ] || return $?
	# browser
	tc class add dev $1 \
		parent ${BF_HANDLE_MAJOR}:${CLASSID_ELEVATED} \
		classid ${BF_HANDLE_MAJOR}:${CLASSID_ELEVATED_BROWSER} \
		hfsc ls m1 0 d 0 m2 ${ELEVATED_WEIGHT}
	[ $? = 0 ] || return $?
	add_interactive_qdisc $1 \
		"${BF_HANDLE_MAJOR}:${CLASSID_ELEVATED_BROWSER}" \
		"${CLASSID_ELEVATED_BROWSER}:"
	[ $? = 0 ] || return $?
	# dns
	tc class add dev $1 \
		parent ${BF_HANDLE_MAJOR}:${CLASSID_ELEVATED} \
		classid ${BF_HANDLE_MAJOR}:${CLASSID_ELEVATED_DNS} \
		hfsc ls m1 0 d 0 m2 ${ELEVATED_WEIGHT}
	[ $? = 0 ] || return $?
	add_interactive_qdisc $1 \
		"${BF_HANDLE_MAJOR}:${CLASSID_ELEVATED_DNS}" \
		"${CLASSID_ELEVATED_DNS}:"
	[ $? = 0 ] || return $?
}

generic_iptables() {
	# All packets from localhost to LAN are marked as prio 2
	iptables -t mangle -$1 OUTPUT -o $LAN_IFACE -j CLASSIFY --set-class 2:2

	# If this is from localhost AND is using the aperture source
	# ports, set the class to avoid BWC.
	iptables -t mangle -$1 OUTPUT ! -o $LAN_IFACE -p tcp -m multiport \
		--source-ports 321:353 -j CLASSIFY --set-class 2:2
	iptables -t mangle -$1 OUTPUT ! -o $LAN_IFACE -p tcp -m multiport \
		--source-ports 321:353 -j RETURN

	# All packets from localhost to WAN are marked
	# Note the !LAN_IFACE logic allows us to catch any potential
	# PPPoE interface as well
	iptables -t mangle -$1 OUTPUT ! -o $LAN_IFACE -j CLASSIFY \
		--set-class 1:FFF1

	# For the LAN side, we set the default to be the parent of the
	# HTB, so that when ct_mark is copied to nf_mark, by
	# CONNMARK --restore mark, priority will be unset, and filter fw
	# will read the mark and set the class correctly.  In the WAN
	# direction, the root is the HTB, so we do not need to set the
	# class; it will just work.
	iptables -t mangle -$1 FORWARD -o $LAN_IFACE -j CLASSIFY --set-class 2:1

	# Forwarded ICMP packets in their own queue
	iptables -t mangle -$1 FORWARD -p icmp -m limit --limit 2/second \
		-j CLASSIFY --set-class 1:FFEC

	# DNS Elevation
	iptables -t mangle -$1 POSTROUTING -p udp --dport 53 \
		-j CLASSIFY --set-class 1:ffed
	iptables -t mangle -$1 POSTROUTING -p udp --dport 53 \
		-j RETURN

	# Restore the CONNMARK to the packet
	iptables -t mangle -$1 POSTROUTING -j CONNMARK --restore-mark
}

setup_iptables () {
	# call iptables to add rules
	generic_iptables A
}

teardown_iptables () {
	# call iptables to delete rules
	generic_iptables D
}
