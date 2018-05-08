#!/bin/sh
mac_address=$(ifconfig | grep 'br-lan ' | sed s/[\ ]/'\n'/g | sed -e s/://g | grep -E '^[0-9A-Fa-f]{12}'|tr A-Z a-z )
if [ -z "$mac_address" ] ; then
	mac_address=$(ifconfig | grep 'eth0 ' | sed s/[\ ]/'\n'/g | sed -e s/://g | grep -E '^[0-9A-Fa-f]{12}'|tr A-Z a-z)
fi

#
# Acerta MAC Address para certos modelos
#
model=$(/usr/bin/get_model.sh)
case "$model" in
	TP-LINK*TL-WR741NDv4)
		mac_address=$(cat /sys/devices/platform/ar933x_wmac/ieee80211/phy0/macaddress)
		;;
	TP-LINK*Archer_C60_v2*)
		mac_address=$(cat /sys/devices/platform/qca956x_wmac/ieee80211/phy1/macaddress)
		;;
esac

echo "$mac_address" | sed -e 's/://g' | tr 'A-Z' 'a-z'
