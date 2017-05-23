#!/bin/sh

. /etc/config/simet.conf

if [ $# -eq 0 ]
  then
    echo "No arguments supplied"
    echo "use simet_geolocation.sh <hash_measure>"
    exit
fi

hash_measure=$1

total_radio=$(uci show wireless | awk -F '.' '{print $2}' | grep "=wifi-device" | wc -l)
mac_address=""
cont=0
while [ $cont -lt $total_radio ]
do
        echo "cont: $cont"
        radio_state_disabled=$(uci get wireless.radio$cont.disabled)
        echo "radio_state_disabled: $radio_state_disabled"
        if [ $radio_state_disabled == 1 ] ; then
                uci set wireless.radio$cont.disabled=0
                wifi
                sleep 5
        fi

        mac_address="$mac_address
$(iw wlan$cont scan | grep "^BSS " | awk '{print $2}' | sed -e 's/(on//')"

        if [ $radio_state_disabled == 1 ] ; then
                uci set wireless.radio$cont.disabled=1
                wifi
        fi

        cont=$(expr $cont + 1)
done
echo "mac_address: $mac_address"
url_google="https://maps.googleapis.com/maps/api/browserlocation/json?browser=firefox&sensor=true"
for mac in $mac_address
do
  url_google="$url_google&wifi=mac:$mac"
done      
#echo $url_google
saida_simet_ws=$(simet_ws "$url_google" | grep "accuracy\|lat\|lng" | sed -e "s/\"//g" | sed -e "s/,//" | sed -e "s/^.*: //")
cont_line=0
for line in $saida_simet_ws
do
 if [ $cont_line -eq 0 ] ; then
    accuracy=$line
  fi
  if [ $cont_line -eq 1 ] ; then
    latitude=$line
  fi
  if [ $cont_line -eq 2 ] ; then
    longitude=$line
  fi
  cont_line=$((cont_line+1))
done
#echo "accuracy: $accuracy"
#echo "lat: $latitude"
#echo "long: $longitude"
echo "{
\"hashMeasure\":\"$hash_measure\",
\"geolocation\":[
{
\"longitude\": $longitude,
\"latitude\": $latitude,
\"precisionMeters\": $accuracy,
\"idGeolocationSource\": 2
}
]
}" > /tmp/geolocation.$$

echo "{
\"hashDevice\":\"$hash_device\",
\"geolocation\":
{
\"longitude\": $longitude,
\"latitude\": $latitude,
\"precisionMeters\": $accuracy,
\"idGeolocationSource\": 2
}
}" > /tmp/geolocation_d.$$


envia_geolocation=$(simet_ws -p "http://$cf_host/$cf_simet_web_persistence/geolocation" -f /tmp/geolocation.$$)
envia_geolocationi_d=$(simet_ws -p "http://$cf_host/$cf_simet_web_persistence/geolocation-device" -f /tmp/geolocation_d.$$)

rm /tmp/geolocation.$$
rm /tmp/geolocation_d.$$

#echo "saida envia_geolocation: $envia_geolocation"

