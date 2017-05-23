#!/bin/sh
total_radio=$(uci show wireless | awk -F '.' '{print $2}' | grep "=wifi-device" | wc -l)
mac_address=""
cont=0
while [ $cont -lt $total_radio ]
do
#        echo "cont: $cont"
        radio_state_disabled=$(uci -q get wireless.radio$cont.disabled)
#        echo "radio_state_disabled: $radio_state_disabled"
        if [ $radio_state_disabled == 1 ] ; then
                uci -q set wireless.radio$cont.disabled=0
                wifi 1>/dev/null 2>/dev/null
                sleep 5
        fi

        mac_address="$mac_address
$(iw wlan$cont scan | grep "^BSS " | awk '{print $2}' | sed -e 's/(on//')"

        if [ $radio_state_disabled == 1 ] ; then
                uci -q set wireless.radio$cont.disabled=1
                wifi 1>/dev/null 2>/dev/null
        fi

        cont=$(expr $cont + 1)
done
#echo "mac_address: $mac_address"
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
echo "$latitude,$longitude,$accuracy"

