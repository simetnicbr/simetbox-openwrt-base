#!/bin/sh

# get_mac_address.sh result should match the MAC address printed in the device label
model=$(/usr/bin/get_model.sh | tr a-z A-Z)
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
		mac_address=$(ifconfig | grep 'br-lan ' | sed s/[\ ]/'\n'/g | sed -e s/://g | grep -E '^[0-9A-Fa-f]{12}')
		if [ -z "$mac_address" ] ; then
			mac_address=$(ifconfig | grep 'eth0 ' | sed s/[\ ]/'\n'/g | sed -e s/://g | grep -E '^[0-9A-Fa-f]{12}')
		fi
esac

echo "$mac_address" | tr -d : | tr A-Z a-z
