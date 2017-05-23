#!/bin/sh
mac_address=`ifconfig |grep 'br-lan '|sed s/[\ ]/'\n'/g|sed -e s/://g|egrep '^[0-9A-Fa-f]{12}'|tr A-Z a-z` 
if [ "$mac_address" == "" ] ; then
  mac_address=`ifconfig |grep 'eth0 '|sed s/[\ ]/'\n'/g|sed -e s/://g|egrep '^[0-9A-Fa-f]{12}'|tr A-Z a-z`
fi
echo $mac_address|sed -e s/://g|tr A-Z a-z
