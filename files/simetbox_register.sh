#!/bin/sh

. /etc/simet/simet1.conf

hash_device=$(/usr/bin/get_mac_address.sh)
hash=$(cat /etc/simet/simet1_hash)
model=$(get_model.sh)
version=$(get_simet_box_version.sh)

[ -z "$hash" ] && {
	echo "simetbox_register.sh: simet1_hash missing. cannot proceed" >&2
	exit 1
}

parametros="{\"macAddress\": \"$hash_device\",\"model\": \"$model\",\"currentVersion\":\"$version\",\"hash\":\"$hash\"}"

force=
# detecta troca de MAC por qualquer motivo (por exemplo, reconfiguração de quem é br-lan)
# caso contrário, o sistema SIMET irá recusar serviço à caixa.
if [ -r /etc/simet/simet1_deviceid ] ; then
  read -r oldhashdevice oldversion < /etc/simet/simet1_deviceid
  if [ "x${hash_device}" != "x${oldhashdevice}" ]; then
     echo "simetbox_register.sh: WARNING: main device MAC address changed, registering again..." >&2
     rm -f /etc/simet/simet1_deviceid
  elif [ "x${version}" != "x${oldversion}" ]; then
     echo "simetbox_register.sh: version change detected, updating registration..." >&2
     force=1
  fi
fi

while [ ! -f /etc/simet/simet1_deviceid ] || [ -n "$force" ]
do 
  output_send_hash=$(simet_ws -p "https://$cf_host/$cf_simet_web_services_optional/resources/simetbox" "$parametros" | grep "\"success\":true")
  if [ "$output_send_hash" != "" ] ; then
    echo "${hash_device} ${version}" > /etc/simet/simet1_deviceid
    force=
  else
    sleep 60
  fi
done
