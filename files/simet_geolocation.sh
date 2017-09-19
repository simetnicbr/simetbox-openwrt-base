#!/bin/sh

. /etc/config/simet.conf
[ -r /etc/config/simet-private.conf ] && . /etc/config/simet-private.conf

#
# Chave de API para geolocalização é requisito
# para que este script funcione.  Caso não tenha
# sido definida, sai sem fazer nada.  Defina em
# /etc/config/simet-private.conf.
#

if [ -z "$GOOGLE_MAP_GEOLOC_APIKEY" ] ; then
	echo "$0: please define GOOGLE_MAP_GEOLOC_APIKEY in /etc/config/simet-private.conf" >&2
	exit 0
fi

hash_device=${hash_device:-$(get_mac_address.sh)}

##
# generate_geopost_body() - gera POST body para Google geolocation API
# $@ : endereços BSS nas redondezas
#
# Gera POST body segundo https://developers.google.com/maps/documentation/geolocation/intro
#
# Retorna: <stdout> - POST body
#          $?       - 0 se ok, errno em outro caso.
generate_geopost_body() {
cat <<EOF
{ "considerIp": "false", "wifiAccessPoints": [
EOF

while [ $# != 0 ] ; do
	mac=$1
	shift

	if [ $# != 0 ] ; then
		echo "{ \"macAddress\": \"$mac\" },"
	else
		echo "{ \"macAddress\": \"$mac\" }"
	fi
done

cat <<EOF2
] }
EOF2
}

##
# send_geoquery() - envia query de geolocation
# $1      : chave de API
# <stdin> : body da requisição
#
# Retorna:
# <stdout> : resultado da query
# $?       : 0 se ok, errno em outro caso
send_geoquery() {
	curl -s -d @- -H "Content-type: application/json" -i "https://www.googleapis.com/geolocation/v1/geolocate?key=$1" || {
		echo "CURL error, could not contact geoapi server" >&2
	}
	:
}

##
# generate_simetws_measure() - cria body georequest measure
# $1     : latitude
# $2     : longitude
# $3     : accuracy
generate_simetws_measure() {
echo "{
\"hashMeasure\":\"$hash_measure\",
\"geolocation\":[
{
\"longitude\": $2,
\"latitude\": $1,
\"precisionMeters\": $3,
\"idGeolocationSource\": 2
}
]
}"
}

##
# generate_simetws_device() - cria body georequest dispositivo
# $1     : latitude
# $2     : longitude
# $3     : accuracy
generate_simetws_device() {
echo "{
\"hashDevice\":\"$hash_device\",
\"geolocation\":
{
\"longitude\": $2,
\"latitude\": $1,
\"precisionMeters\": $3,
\"idGeolocationSource\": 2
}
}"
}

##
# geoapi_precheck() - testes de prelaunch dos BSS
# $1..$n : enderecos BSS
#
# Testes de prelaunch, exit se nao adianta fazer a
# query da geoapi
geoapi_precheck() {
	# no minimo 2 BSS sao necessarios
	if [ $# -le 2 ] ; then
		echo "Not enough BSS addresses to geo-locate" >&2
		exit 0
	fi
	:
}

if [ $# -eq 0 ]
  then
    echo "No arguments supplied"
    echo "use simet_geolocation.sh <hash_measure>"
    exit
fi

hash_measure=$1

#FIXME: no lugar de ligar "oficialmente" o radio de forma temporária,
#ligar direto em modo passivo, fazer o scan, e derrubar, assim não
#corre o risco dele associar, receber IP, etc.
total_radio=$(uci show wireless | awk -F '.' '/=wifi-device/ {print $2}' | wc -l)
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
$(iw wlan$cont scan | awk '/^BSS / {print $2}' | sed -e 's/(on//')"

        if [ $radio_state_disabled == 1 ] ; then
                uci set wireless.radio$cont.disabled=1
                wifi
        fi

        cont=$(expr $cont + 1)
done

echo "BSSes detected for geolocalization: $mac_address"

geoapi_precheck $mac_address

saida_geoloc=$(generate_geopost_body $mac_address | send_geoquery "$GOOGLE_MAP_GEOLOC_APIKEY" | \
	awk '
		BEGIN { FS=":|," ; accuracy=0 ; code=200 ; n=0 }
		/"code"/     { code=$2 }
		/"accuracy"/ { accuracy=$2 }
		/"lat"/      { lat=$2 ; n++ }
		/"lng"/	     { lng=$2 ; n++ }
		END { if (n == 2 && code == 200) print lat lng accuracy }
	')

if [ "x$saida_geoloc" = "x" ] ; then
	# Problema ao contactar o google, aborta sem sinalizar erro
	echo "geolocation API call failed" >&2
	exit 0
fi

echo "geolocation result: " $saida_geoloc

TMPGEO=$(mktemp -t simetgeoloc.$$.XXXXXX) && TMPGEOD=$(mktemp -t simetgeoloc_d.$$.XXXXXX)

if [ -z "$TMPGEO" ] || [ -z "$TMPGEOD" ] ; then
	[ -n "$TMPGEO" ] && rm -f "$TMPGEO"
	[ -n "$TMPGEOD" ] && rm -f "$TMPGEOD"
	exit 1
fi

generate_simetws_measure $saida_geoloc > "$TMPGEO"
generate_simetws_device  $saida_geoloc > "$TMPGEOD"

envia_geolocation=$(simet_ws -p "https://$cf_host/$cf_simet_web_persistence/geolocation" -f "$TMPGEO")
envia_geolocation_d=$(simet_ws -p "https://$cf_host/$cf_simet_web_persistence_optional/geolocation-device" -f "$TMPGEOD")

rm -f "$TMPGEO"
rm -f "$TMPGEOD"

echo "SIMET geoapi: measurement location result: " $envia_geolocation
echo "SIMET geoapi: device location result: " $envia_geolocation_d

exit 0
