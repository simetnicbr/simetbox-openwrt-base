#!/bin/sh
if_gw=`netstat -nr | grep -e '^0.0.0.0' | awk '{print $8}'`
mac_address=`/usr/bin/get_mac_address.sh`;
for f in `ls /sys/class/net/`
do
  if [ ! -d /tmp/ifbytes/$f ]; then
    mkdir -p /tmp/ifbytes/$f
    echo "0"> /tmp/ifbytes/$f/rxbytes
    echo "0"> /tmp/ifbytes/$f/txbytes
  fi
  rx_bytes_old=`cat /tmp/ifbytes/$f/rxbytes`
  tx_bytes_old=`cat /tmp/ifbytes/$f/txbytes`
  rx_bytes=`cat /sys/class/net/$f/statistics/rx_bytes`;
  tx_bytes=`cat /sys/class/net/$f/statistics/tx_bytes`;
  if [ "$if_gw" == "$f" ]; then
    is_gw=1
  else
    is_gw=0
  fi
#  echo "Mac Address: $mac_address - Interface: $f -> RX: $rx_bytes -> TX: $tx_bytes -> is_gw -> $is_gw"
  if [ "$rx_bytes" -gt 0 ] && [ "$rx_bytes" -ne "$rx_bytes_old" ]; then
#    echo "url_rx -> http://200.160.6.34/SimetServicesOptional/InterfaceTraffic?hashDevice=$mac_address&interfaceName=$f&totalBytes=$rx_bytes&isGw=$is_gw&idDirection=1"	
    simet_ws_output_rx=`simet_ws "https://simet-publico.ceptro.br/SimetServicesOptional/InterfaceTraffic?hashDevice=$mac_address&interfaceName=$f&totalBytes=$rx_bytes&isGw=$is_gw&idDirection=1"`
#    echo "simet_ws_output_rx -> $simet_ws_output_rx"
    echo "$rx_bytes"> /tmp/ifbytes/$f/rxbytes
  fi
  if [ "$tx_bytes" -gt 0 ] && [ "$tx_bytes" -ne "$tx_bytes_old" ]; then
#    echo "url_tx -> http://200.160.6.34/SimetServicesOptional/InterfaceTraffic?hashDevice=$mac_address&interfaceName=$f&totalBytes=$tx_bytes&isGw=$is_gw&idDirection=2"
    simet_ws_output_tx=`simet_ws "https://simet-publico.ceptro.br/SimetServicesOptional/InterfaceTraffic?hashDevice=$mac_address&interfaceName=$f&totalBytes=$tx_bytes&isGw=$is_gw&idDirection=2"`
#    echo "simet_ws_output_tx -> $simet_ws_output_tx"
    echo "$tx_bytes"> /tmp/ifbytes/$f/txbytes
  fi
done

