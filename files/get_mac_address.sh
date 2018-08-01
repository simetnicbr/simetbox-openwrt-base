#!/bin/sh

# get_mac_address.sh result should match the MAC address printed in the device label
model=$(/usr/bin/get_model.sh)
case "$model" in
	TP-LINK*TL-WR740N*v4)
		mac_address=$(cat /sys/devices/platform/ar933x_wmac/ieee80211/phy0/macaddress)
		;;
	TP-LINK*Archer_C60_v2)
		mac_address=$(cat /sys/devices/platform/qca956x_wmac/ieee80211/phy1/macaddress)
		;;
	TP-LINK*TL-WR740ND|TP-LINK*TL-WR741N*v4|TP-LINK*TL-WR841N*v7|TP-LINK*TL-WR842N*v1)
		mac_address=$(cat /sys/devices/pci0000:00/0000:00:00.0/ieee80211/phy0/macaddress)
		;;
	TP-LINK*TL-WDR3600/4300/4310)
		mac_address=$(cat /sys/devices/pci0000:00/0000:00:00.0/ieee80211/phy1/macaddress)
		;;
	*)
		mac_address=$(ifconfig | grep 'br-lan ' | sed s/[\ ]/'\n'/g | sed -e s/://g | grep -E '^[0-9A-Fa-f]{12}'|tr A-Z a-z )
		if [ -z "$mac_address" ] ; then
			mac_address=$(ifconfig | grep 'eth0 ' | sed s/[\ ]/'\n'/g | sed -e s/://g | grep -E '^[0-9A-Fa-f]{12}'|tr A-Z a-z)
		fi
esac

echo "$mac_address" | tr -d : | tr 'A-Z' 'a-z'
