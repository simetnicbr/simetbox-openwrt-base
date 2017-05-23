#!/bin/sh

if [ ! -f /usr/bin/ctorrent ] ; then
       exit;
fi


mac_address=`get_mac_address.sh`
peer_id=$mac_address"SIMETBOX"


source /etc/config/simet.conf

simet_ws http://$cf_TorrentFile_host/$cf_TorrentFile_name.torrent -f /tmp/$cf_TorrentFile_name.torrent



if [ "$1" = "" ]; then
	hash_measure=`simet_hash_measure -s $cf_host`
	if [ "$hash_measure" = "" ]; then
	        echo nao conseguiu obter hash_measure >&2
	        exit 1
	fi
else
	hash_measure=$1
fi

echo $hash_measure

ctorrent -P $peer_id -C 0 -f -s /tmp/$cf_TorrentFile_name -H $hash_measure -L 13 /tmp/$cf_TorrentFile_name.torrent &
sleep 120
killall -9 ctorrent
rm -f /tmp/simet_bt* /tmp/bt_measures.json
