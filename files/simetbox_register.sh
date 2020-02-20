#!/bin/sh

. /etc/config/simet.conf

hash_device=$(/usr/bin/get_mac_address.sh)

hash=$(cat /etc/config/hash)

model=$(get_model.sh)

version=$(get_simet_box_version.sh)

parametros="{\"macAddress\": \"$hash_device\",\"model\": \"$model\",\"currentVersion\":\"$version\",\"hash\":\"$hash\"}"

force=
# detecta troca de MAC por qualquer motivo (por exemplo, reconfiguração de quem é br-lan)
# caso contrário, o sistema SIMET irá recusar serviço à caixa.
if [ -r /etc/config/simetbox_hash_configured ] ; then
  read -r oldhashdevice oldversion < /etc/config/simetbox_hash_configured
  if [ "x${hash_device}" != "x${oldhashdevice}" ]; then
     echo "simetbox_register.sh: WARNING: main device MAC address changed, registering again..." >&2
     rm -f /etc/config/simetbox_hash_configured
  elif [ "x${version}" != "x${oldversion}" ]; then
     echo "simetbox_register.sh: version change detected, updating registration..." >&2
     force=1
  fi
fi

while [ ! -f /etc/config/simetbox_hash_configured ] || [ -n "$force" ]
do 
  output_send_hash=$(simet_ws -p "https://$cf_host/$cf_simet_web_services_optional/resources/simetbox" "$parametros" | grep "\"success\":true")
  if [ "$output_send_hash" != "" ] ; then
    echo "${hash_device} ${version}" > /etc/config/simetbox_hash_configured
    force=
  else
    sleep 60
  fi
done
