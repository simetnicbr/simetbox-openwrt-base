#!/bin/sh
source /etc/config/simet.conf
hash_measure_v4=$(/usr/bin/simet_hash_measure -s $cf_host)
hash_measure_v6=$(/usr/bin/simet_hash_measure -s $cf_host)
ntpq_enable=$(uci get simet_cron.simet_ntpq.enable)
if [ "$ntpq_enable" == "true" ] ; then
  /usr/bin/simet_ntpq
fi
if test -n "$hash_measure_v4" ; then
	/usr/bin/simet_geolocation.sh $hash_measure_v4
	/usr/bin/simet_client -4 -m $hash_measure_v4
	sleep 5
fi
if test -n "$hash_measure_v6" ; then
	/usr/bin/simet_geolocation.sh $hash_measure_v6
	/usr/bin/simet_client -6 -m $hash_measure_v6
fi
