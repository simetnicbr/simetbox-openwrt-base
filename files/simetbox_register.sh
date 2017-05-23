#!/bin/sh

. /etc/config/simet.conf

hash_device=$(/usr/bin/get_mac_address.sh)

hash=$(cat /etc/config/hash)

model=$(get_model.sh)

version=$(get_simet_box_version.sh)

parametros="{\"macAddress\": \"$hash_device\",\"model\": \"$model\",\"currentVersion\":\"$version\",\"hash\":\"$hash\"}"

# echo "parametros: $parametros"

while [ ! -f /etc/config/simetbox_hash_configured ] 
do 
  output_send_hash=$(simet_ws -p "https://$cf_host/$cf_simet_web_services_optional/resources/simetbox" "$parametros" | grep "\"success\":true")
#  echo "output_send_hash: $output_send_hash"
  if [ "$output_send_hash" != "" ] ; then
    touch /etc/config/simetbox_hash_configured
  fi      
  sleep 10
done
