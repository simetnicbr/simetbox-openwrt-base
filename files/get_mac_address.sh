#!/bin/sh
# get_mac_address.sh result should match the MAC address printed in the device label

# Modern OpenWRT
mac_address=$(. /lib/functions.sh && . /lib/functions/system.sh && get_mac_label) 2>/dev/null || mac_address=

# If that fails, try to find from UCI
[ -z "$mac_address" ] && {
	# Extension from an unnamed vendor
	label_ifname=$(uci get network.network.mac_label_ifname) 2>/dev/null || label_ifname=
	[ -n "$label_ifname" ] && {
		mac_address=$(cat /sys/class/net/"$label_ifname"/address 2>/dev/null) || mac_address=
	}
}

[ -z "$mac_address" ] && {
	model=$(/usr/bin/get_model.sh | tr a-z A-Z) 2>/dev/null || model=
	case "$model" in
		TP-LINK*TL-WR740N*V4)
			mac_address=$(cat /sys/devices/platform/ar933x_wmac/ieee80211/phy0/macaddress)
			;;
		UBIQUITI_LOCO_M_XW)
			mac_address=$(cat /sys/devices/platform/ar934x_wmac/ieee80211/phy0/macaddress)
			;;
		TP-LINK*ARCHER_C60_V2)
			mac_address=$(cat /sys/devices/platform/qca956x_wmac/ieee80211/phy1/macaddress)
			;;
		TP-LINK*TL-WR740ND|TP-LINK*TL-WR741N*V4|TP-LINK*TL-WR841N*V7|TP-LINK*TL-WR842N*V1)
			mac_address=$(cat /sys/devices/pci0000:00/0000:00:00.0/ieee80211/phy0/macaddress)
			;;
		TP-LINK*TL-WDR3600_V1|TP-LINK*TL-WDR4300_V1)
			mac_address=$(cat /sys/devices/pci0000:00/0000:00:00.0/ieee80211/phy1/macaddress)
			;;
		*)
			mac_address=$(cat /sys/class/net/br-lan/address 2>/dev/null) || mac_address=
			;;
	esac
}

[ -z "$mac_address" ] && \
	mac_address=$(cat /sys/class/net/*lan*/address /sys/class/net/*wan*/address /sys/class/net/eth*/address /sys/class/net/*/address 2>/dev/null | sed -nE -e '/[0:]+$/ d' -e 's/://g' -e '/^[0-9A-Fa-f]{12}$/ { p; q }' ) || true

echo "$mac_address" | tr -d : | tr A-Z a-z
