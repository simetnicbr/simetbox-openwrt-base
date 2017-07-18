#!/bin/sh
mac_address=`ifconfig |grep 'br-lan '|sed s/[\ ]/'\n'/g|sed -e s/://g|egrep '^[0-9A-Fa-f]{12}'|tr A-Z a-z` 
if [ "$mac_address" == "" ] ; then
  mac_address=`ifconfig |grep 'eth0 '|sed s/[\ ]/'\n'/g|sed -e s/://g|egrep '^[0-9A-Fa-f]{12}'|tr A-Z a-z`
fi
model=$(/usr/bin/get_model.sh)
#
# Acerta MAC Address para modelo TP-LINKTL-WR741NDv4
#
if [ "$model" = "TP-LINKTL-WR741NDv4" ] ; then
  mac_address=$(cat /sys/devices/platform/ar933x_wmac/ieee80211/phy0/macaddress)
fi
echo $mac_address|sed -e s/://g|tr A-Z a-z
