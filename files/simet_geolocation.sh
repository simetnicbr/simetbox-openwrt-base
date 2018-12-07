#!/bin/sh

###
## simet_geolocation.sh - simet geolocation helper
## Copyright 2012-2018 NIC.br
##
## Parameters:
## @1 - measure id, optional. When unspecificed, does only
##      device geolocation webservice update calls.
##
## simet_geolocation.sh API level: 1

# Pode ser modificado por simet.conf ou simet-private.conf, mas
# isso não é oficialmente documentado.
GEOLOC_DIR=/tmp/simet_geo
GEOLOC_CACHE="${GEOLOC_DIR}/simet_geolocation_cache"

. /etc/config/simet.conf
[ -r /etc/config/simet-private.conf ] && . /etc/config/simet-private.conf

[ ! -d "${GEOLOC_DIR}" ] && mkdir -p "${GEOLOC_DIR}"

#
# Chave de API para geolocalização é requisito
# para que este script funcione.  Caso não tenha
# sido definida, sai sem fazer nada.  Defina em
# /etc/config/simet-private.conf.
#

if [ -z "$GOOGLE_MAP_GEOLOC_APIKEY" ] ; then
	echo "$0: please define GOOGLE_MAP_GEOLOC_APIKEY in /etc/config/simet-private.conf" >&2
	exit 26
fi

hash_device=${hash_device:-$(get_mac_address.sh)}

##
# persist_geoloc_RAM() - armazena em cache volátil local a geoloc
# $1: latitude
# $2: longitude
# $3: precisao
#
persist_geoloc_RAM() {
	# Presença ou ausência do arquivo de cache indica se alguma
	# geolocalização já foi feita neste boot.
	touch "${GEOLOC_CACHE}" && chmod 0600 "${GEOLOC_CACHE}"
	( date -u +%s && echo "$1" "$2" "$3" ) > "${GEOLOC_CACHE}"
	:
}

##
# cache_not_too_old - returns 0 if it is not too old
#
cache_not_too_old() {
	[ -r "${GEOLOC_CACHE}" ] || return 1;

	cache_time=$(head -n 1 "${GEOLOC_CACHE}")
	# Shell is limited to 32bits, not y2038-safe, but we don't have bc :(
	time_delta=$(( $(date +%s -u) - cache_time ))
	if [ $time_delta -ge 259200 ] ; then
		# more than 72h
		return 1;
	fi

	return 0;
}

##
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
		exit 1
	fi
	:
}

###
### MAIN
###

# Pode ser vazia!
hash_measure=$1

if [ -z "$hash_measure" ] ; then
	echo "hash_measure not specificed, will persist only device location" >&2
fi

if cache_not_too_old ; then
	# return from cache

	echo "Using cached geolocation..." >&2
	saida_geoloc=$(tail -n 1 "${GEOLOC_CACHE}")
	echo "geolocation result (cached): $saida_geoloc" >&2

	# result to stdout
	tail -n 2 "${GEOLOC_CACHE}"

	if [ -z "${saida_geoloc}" ] ; then
		echo "error: geolocation cache corrupted, removing..." >&2
		rm -f "${GEOLOC_CACHE}"
		exit 3
	fi
else
	# GEOLOCATE
	# FIXME: do it at a thruly low level, and firewall whatever we bring up

	# save radio disabled state, and force-enable them
	TMPRADIO=$(mktemp -t simetgeoloc.$$.XXXXXX) || exit 1
	[ -z "$TMPRADIO" ] && exit 1
	uci show wireless | grep -E "\.disabled='?(1|true|on|yes|enabled)" > "$TMPRADIO"
	sed -n '/[.]disabled=/ {s/=.*// p}' < "$TMPRADIO" | xargs -r -n1 uci delete && uci commit wireless
	grep -q 'disabled=' "$TMPRADIO" && {
		# "wifi" greatly disturbs DFS channels, don't call if we can avoid it
		wifi >&2
		sleep 10
	}

	mac_address=
	WDEVS=$(iw dev | sed -n -e '/nterface/ {s/.*nterface[[:blank:]]\+// p}')
	if [ -n "$WDEVS" ] ; then
		for w in $WDEVS ; do
			scan=
			for i in 1 2 3 4 5; do
				[ -z "$scan" ] && scan=$(iw "$w" scan | awk '/^BSS / {print $2}' | sed -e 's/(on//')
				[ -z "$scan" ] && sleep 5
			done
			mac_address="$mac_address
$scan"
		done
	else
		echo "Could not find any enabled wireless devices to use for geolocation" >&2
	fi

	# restore radio disabled state
	xargs -r -n1 uci set < "$TMPRADIO" && uci commit wireless
	grep -q 'disabled=' "$TMPRADIO" && wifi
	[ -n "$TMPRADIO" ] && rm -f "$TMPRADIO"

	echo "BSSes detected for geolocalization: $mac_address" >&2

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
		exit 24
	fi

	echo "geolocation result: " $saida_geoloc >&2
	persist_geoloc_RAM $saida_geoloc
	# result to stdout
	tail -n 2 "${GEOLOC_CACHE}"
fi

TMPGEO=$(mktemp -t simetgeoloc.$$.XXXXXX) && TMPGEOD=$(mktemp -t simetgeoloc_d.$$.XXXXXX)

if [ -z "$TMPGEO" ] || [ -z "$TMPGEOD" ] ; then
	[ -n "$TMPGEO" ] && rm -f "$TMPGEO"
	[ -n "$TMPGEOD" ] && rm -f "$TMPGEOD"
	exit 3
fi

generate_simetws_device  $saida_geoloc > "$TMPGEOD"
envia_geolocation_d=$(simet_ws -p "https://$cf_host/$cf_simet_web_persistence_optional/geolocation-device" -f "$TMPGEOD")

if [ -n "$hash_measure" ] ; then
	generate_simetws_measure $saida_geoloc > "$TMPGEO"
	envia_geolocation=$(simet_ws -p "https://$cf_host/$cf_simet_web_persistence/geolocation" -f "$TMPGEO")
else
	envia_geolocation="(skipped, hash_measure was not specificed)"
fi

rm -f "$TMPGEO"
rm -f "$TMPGEOD"

echo "SIMET geoapi: measurement location result: $envia_geolocation" >&2
echo "SIMET geoapi: device location result: $envia_geolocation_d" >&2

exit 0
